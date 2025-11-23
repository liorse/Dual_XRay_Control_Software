// Dual_XRay_Control_Software.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Dual_XRay_Control_Software.h"
#include <fstream>
#include <string>


// Protect against macro collisions coming from Windows headers.
// Push and undef common macros that commonly conflict with ROOT headers.
#ifdef _MSC_VER
	// push/undef min/max
	#ifdef min
		#pragma push_macro("min")
		#undef min
		#define _ROOT_PUSHED_min
	#endif
	#ifdef max
		#pragma push_macro("max")
		#undef max
		#define _ROOT_PUSHED_max
	#endif
	// push/undef CreateWindow (sometimes defined by Win headers as macro)
	#ifdef CreateWindow
		#pragma push_macro("CreateWindow")
		#undef CreateWindow
		#define _ROOT_PUSHED_CreateWindow
	#endif
	// push/undef SendMessage (WinAPI macro colides with TGFrame::SendMessage)
	#ifdef SendMessage
		#pragma push_macro("SendMessage")
		#undef SendMessage
		#define _ROOT_PUSHED_SendMessage
	#endif
#endif

// Minimal ROOT GUI includes (only bring in what we use here)
#include "TApplication.h"
#include "TSystem.h"
#include "TGClient.h"
#include "TGFrame.h"
#include "TGLabel.h"
#include "TGButton.h"
#include "TGNumberEntry.h"
#include "TGLayout.h"
#include "TGStatusBar.h"
#include "TTimer.h"
#include <stdio.h>
#include "hwdrivers/XRay.h"
#include "TSystem.h"
#include "Vparams.h"
#include "ipc/NamedPipeServer.h"
#include "TThread.h"
#include <vector>

// Global XRay instance (hardware driver). Created in WinMain.
static XRay* gXRay = NULL;
static FILE* gLogFile = NULL;
static Bool_t gServerRunning = kFALSE;
static TMutex gServerMutex;

// Read XRaySerialNumber "XXXXXXXX" from a simple key-value config file.
static std::string ReadXRaySerialFromConfig(const char* path)
{
	std::ifstream in(path);
	if (!in.is_open()) {
		if (gLogFile) fprintf(gLogFile, "Warning: Could not open config file: %s\n", path);
		return std::string();
	}
	if (gLogFile) fprintf(gLogFile, "Reading config file: %s\n", path);
	std::string line;
	const char* paramName = "XRaySerialNumber";
	size_t paramLen = strlen(paramName);
	while (std::getline(in, line)) {
		// Trim leading spaces
		size_t start = line.find_first_not_of(" \t");
		if (start == std::string::npos) continue;
		if (line[start] == '#') continue; // comment
		if (line.compare(start, paramLen, paramName) == 0) {
			// Find first quote after parameter name
			size_t q1 = line.find('"', start + paramLen);
			if (q1 == std::string::npos) break;
			size_t q2 = line.find('"', q1+1);
			if (q2 == std::string::npos) break;
			std::string serial = line.substr(q1+1, q2 - (q1+1));
			if (gLogFile) fprintf(gLogFile, "Found XRaySerialNumber: %s\n", serial.c_str());
			return serial;
		}
	}
	if (gLogFile) fprintf(gLogFile, "XRaySerialNumber not found in config file\n");
	return std::string();
}

// Read numeric parameter from config file
static double ReadNumericFromConfig(const char* path, const char* paramName, double defaultVal)
{
	std::ifstream in(path);
	if (!in.is_open()) return defaultVal;
	std::string line;
	size_t paramLen = strlen(paramName);
	while (std::getline(in, line)) {
		size_t start = line.find_first_not_of(" \t");
		if (start == std::string::npos) continue;
		if (line[start] == '#') continue;
		if (line.compare(start, paramLen, paramName) == 0) {
			size_t valStart = start + paramLen;
			while (valStart < line.length() && (line[valStart] == ' ' || line[valStart] == '\t'))
				valStart++;
			if (valStart < line.length()) {
				try {
					return std::stod(line.substr(valStart));
				} catch (...) {
					return defaultVal;
				}
			}
		}
	}
	return defaultVal;
}

// Helper to split strings for pipe server
static void SplitPipeTokens(const std::string& s, char delim, std::vector<std::string>& out) {
	out.clear();
	size_t start = 0;
	while (start <= s.size()) {
		size_t pos = s.find(delim, start);
		if (pos == std::string::npos) { out.push_back(s.substr(start)); break; }
		out.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}
}

// Named pipe server thread function
static void* ServerThreadFunc(void* arg) {
	XRay* xray = (XRay*)arg;
	const char* pipeEnv = getenv("XRAY_PIPE_NAME");
	std::string pipeName = pipeEnv && pipeEnv[0] ? std::string(pipeEnv) : std::string("\\\\.\\pipe\\XRayService");
	
	if (gLogFile) fprintf(gLogFile, "Starting named pipe server on %s\n", pipeName.c_str());
	
	NamedPipeServer server(pipeName);
	if (!server.listen()) {
		if (gLogFile) fprintf(gLogFile, "Failed to create named pipe: %s\n", pipeName.c_str());
		return NULL;
	}
	
	if (gLogFile) fprintf(gLogFile, "Named pipe server listening, waiting for client...\n");
	
	Bool_t running;
	gServerMutex.Lock();
	running = gServerRunning;
	gServerMutex.UnLock();
	
	while (running) {
		if (!server.accept()) {
			if (gLogFile) fprintf(gLogFile, "Failed to accept client connection\n");
			break;
		}
		
		if (gLogFile) fprintf(gLogFile, "Client connected to pipe server\n");
		
		// Handle client requests
		gServerMutex.Lock();
		running = gServerRunning;
		gServerMutex.UnLock();
		
		while (running) {
			std::string line;
			if (!server.readLine(line)) break;
			
			std::vector<std::string> tok;
			SplitPipeTokens(line, '|', tok);
			if (tok.empty()) {
				server.writeLine("ERR|empty");
				continue;
			}
			
			const std::string& cmd = tok[0];
			
			if (cmd == "GET_STATE") {
				if (!xray) {
					server.writeLine("ERR|noinst");
					continue;
				}
				XRay::XRayState st = xray->GetXRayState();
			char buf[256];
			sprintf(buf, "OK|%d|%f|%f|%f|%f|%f|%f",
				st.Power ? 1 : 0, st.VoltageToSet, st.ActualVoltage,
					st.CurrentToSet, st.ActualCurrent, st.ActualPower, st.Temperature);
				server.writeLine(buf);
			}
			else if (cmd == "SET_POWER") {
				if (!xray) {
					server.writeLine("ERR|noinst");
					continue;
				}
				int p = (tok.size() >= 2) ? std::atoi(tok[1].c_str()) : 0;
				if (tok.size() >= 3) xray->SetXRayVoltage((Float_t)std::atof(tok[2].c_str()));
				if (tok.size() >= 4) xray->SetXRayCurrent((Float_t)std::atof(tok[3].c_str()));
				xray->SetXRayState(p ? kTRUE : kFALSE);
				server.writeLine("OK");
			}
			else if (cmd == "SET_VOLTAGE") {
				if (!xray) {
					server.writeLine("ERR|noinst");
					continue;
				}
				if (tok.size() >= 2) xray->SetXRayVoltage((Float_t)std::atof(tok[1].c_str()));
				server.writeLine("OK");
			}
			else if (cmd == "SET_CURRENT") {
				if (!xray) {
					server.writeLine("ERR|noinst");
					continue;
				}
				if (tok.size() >= 2) xray->SetXRayCurrent((Float_t)std::atof(tok[1].c_str()));
				server.writeLine("OK");
			}
			else if (cmd == "READ_DATA") {
				if (!xray) {
					server.writeLine("ERR|noinst");
					continue;
				}
			xray->ReadXRayData();
			XRay::XRayState st = xray->GetXRayState();
			char buf[256];
			sprintf(buf, "OK|%d|%f|%f|%f|%f|%f|%f",
				st.Power ? 1 : 0, st.VoltageToSet, st.ActualVoltage,
				st.CurrentToSet, st.ActualCurrent, st.ActualPower, st.Temperature);
			server.writeLine(buf);
			}
			else if (cmd == "GET_SERIAL") {
				if (!xray) {
					server.writeLine("ERR|noinst");
					continue;
				}
			const char* serial = xray->GetSerialNumber();
			char buf[128];
			sprintf(buf, "OK|%s", serial ? serial : "");
			server.writeLine(buf);
			}
			else if (cmd == "SHUTDOWN") {
				server.writeLine("OK");
				gServerMutex.Lock();
				gServerRunning = kFALSE;
				gServerMutex.UnLock();
				break;
			}
			else {
				server.writeLine("ERR|unknown_cmd");
			}
			
			gServerMutex.Lock();
			running = gServerRunning;
			gServerMutex.UnLock();
		}
		
		if (gLogFile) fprintf(gLogFile, "Client disconnected from pipe server\n");
		
		gServerMutex.Lock();
		running = gServerRunning;
		gServerMutex.UnLock();
	}
	
	if (gLogFile) fprintf(gLogFile, "Named pipe server stopped\n");
	return NULL;
}

// A small self-contained ROOT GUI that replicates the X-ray panel from EventDisplay.cxx
namespace XR {
enum {
	XR_SWITCH = 1001,
	XR_SETV   = 1002,
	XR_SETI   = 1003,
	XR_TIMER  = 1004
};

class XRayGui : public TGMainFrame {
public:
	XRayGui(const TGWindow* p, XRay* xr, UInt_t w=400, UInt_t h=300)
		: TGMainFrame(p, w, h, kVerticalFrame)
		, xray_(xr)
		, powerOn_(false)
		, setV_kV_(ReadNumericFromConfig("qsv.conf", "XRVoltageToSet", 10.0))
		, setI_uA_(ReadNumericFromConfig("qsv.conf", "XRCurrentToSet", 5.0))
		, monV_kV_(0.0)
		, monI_uA_(0.0)
		, power_mW_(0.0)
		, temp_C_(25.0)
	{
		SetCleanup(kDeepCleanup);

		gClient->GetColorByName("white", white_);
		gClient->GetColorByName("black", black_);
		gClient->GetColorByName("green", green_);
		gClient->GetColorByName("red", red_);

		// Root vertical layout - set title to include serial number from connected device
		// or indicate simulation mode
		std::string groupTitle = "X-ray";
		if (xray_) {
			if (xray_->GetXRayMode() == SIMULATION) {
				groupTitle = "X-ray - SIMULATION MODE";
			} else {
				const char* deviceSerial = xray_->GetSerialNumber();
				if (deviceSerial && deviceSerial[0]) {
					groupTitle = std::string("X-ray - ") + deviceSerial;
				}
			}
		}
		auto* xrGroup = new TGGroupFrame(this, groupTitle.c_str());
		xrGroup->SetTitlePos(TGGroupFrame::kCenter);

		// Row: Power
		auto* row1 = new TGHorizontalFrame(xrGroup);
		xrGroup->AddFrame(row1, new TGLayoutHints(kLHintsTop, 5,5,5,2));
		auto* lbPower = new TGLabel(row1, "Power:");
		row1->AddFrame(lbPower, new TGLayoutHints(kLHintsLeft, 1,10,1,1));

		lblPowerState_ = new TGLabel(row1, "Is off");
		lblPowerState_->SetBackgroundColor(green_);
		lblPowerState_->SetForegroundColor(red_);
		lblPowerState_->SetWidth(50);
		lblPowerState_->ChangeOptions(lblPowerState_->GetOptions() | kFixedWidth);
		row1->AddFrame(lblPowerState_, new TGLayoutHints(kLHintsLeft, 5,5,1,1));

		btnPower_ = new TGTextButton(row1, "Switch on", XR_SWITCH);
		btnPower_->Associate(this);
		row1->AddFrame(btnPower_, new TGLayoutHints(kLHintsLeft, 10,1,1,1));

		// Row: header V
		auto* rowVh = new TGHorizontalFrame(xrGroup);
		xrGroup->AddFrame(rowVh, new TGLayoutHints(kLHintsTop, 5,5,2,2));
		rowVh->AddFrame(new TGLabel(rowVh, " Vset, kV            Vmon, kV"), new TGLayoutHints(kLHintsLeft, 1,1,1,1));

		// Row: set V, mon V
		auto* rowV = new TGHorizontalFrame(xrGroup);
		xrGroup->AddFrame(rowV, new TGLayoutHints(kLHintsTop, 5,5,2,2));
		numSetV_ = new TGNumberEntry(rowV, setV_kV_);
		numSetV_->SetLimits(TGNumberFormat::kNELLimitMinMax, 0.0, 160.0);
		rowV->AddFrame(numSetV_, new TGLayoutHints(kLHintsLeft, 1,5,1,1));
		btnSetV_ = new TGTextButton(rowV, "Set", XR_SETV);
		btnSetV_->Associate(this);
		rowV->AddFrame(btnSetV_, new TGLayoutHints(kLHintsLeft, 5,10,1,1));
		lblMonV_ = new TGLabel(rowV, "0");
		lblMonV_->SetWidth(40);
		lblMonV_->SetBackgroundColor(green_);
		lblMonV_->SetForegroundColor(red_);
		lblMonV_->ChangeOptions(lblMonV_->GetOptions() | kFixedWidth);
		rowV->AddFrame(lblMonV_, new TGLayoutHints(kLHintsRight, 20,1,1,1));

		// Row: header I
		auto* rowIh = new TGHorizontalFrame(xrGroup);
		xrGroup->AddFrame(rowIh, new TGLayoutHints(kLHintsTop, 5,5,2,2));
		rowIh->AddFrame(new TGLabel(rowIh, "  Iset, uA             Imon, uA"), new TGLayoutHints(kLHintsLeft, 1,1,1,1));

		// Row: set I, mon I
		auto* rowI = new TGHorizontalFrame(xrGroup);
		xrGroup->AddFrame(rowI, new TGLayoutHints(kLHintsTop, 5,5,2,2));
		numSetI_ = new TGNumberEntry(rowI, setI_uA_);
		numSetI_->SetLimits(TGNumberFormat::kNELLimitMinMax, 0.0, 500.0);
		rowI->AddFrame(numSetI_, new TGLayoutHints(kLHintsLeft, 1,5,1,1));
		btnSetI_ = new TGTextButton(rowI, "Set", XR_SETI);
		btnSetI_->Associate(this);
		rowI->AddFrame(btnSetI_, new TGLayoutHints(kLHintsLeft, 5,10,1,1));
		lblMonI_ = new TGLabel(rowI, "0");
		lblMonI_->SetWidth(40);
		lblMonI_->SetBackgroundColor(green_);
		lblMonI_->SetForegroundColor(red_);
		lblMonI_->ChangeOptions(lblMonI_->GetOptions() | kFixedWidth);
		rowI->AddFrame(lblMonI_, new TGLayoutHints(kLHintsRight, 20,1,1,1));

		// Row: Power (mW)
		auto* rowP = new TGHorizontalFrame(xrGroup);
		xrGroup->AddFrame(rowP, new TGLayoutHints(kLHintsTop, 5,5,2,2));
		rowP->AddFrame(new TGLabel(rowP, "Draw power, mW"), new TGLayoutHints(kLHintsLeft, 1,1,1,1));
		lblPower_mW_ = new TGLabel(rowP, "0");
		lblPower_mW_->SetWidth(40);
		lblPower_mW_->SetBackgroundColor(green_);
		lblPower_mW_->SetForegroundColor(red_);
		lblPower_mW_->ChangeOptions(lblPower_mW_->GetOptions() | kFixedWidth);
		rowP->AddFrame(lblPower_mW_, new TGLayoutHints(kLHintsRight, 20,1,1,1));

		// Row: Temperature (C)
		auto* rowT = new TGHorizontalFrame(xrGroup);
		xrGroup->AddFrame(rowT, new TGLayoutHints(kLHintsTop, 5,5,2,5));
		rowT->AddFrame(new TGLabel(rowT, "Temperature, C"), new TGLayoutHints(kLHintsLeft, 1,1,1,1));
		lblTempC_ = new TGLabel(rowT, "25.0");
		lblTempC_->SetWidth(40);
		lblTempC_->SetBackgroundColor(green_);
		lblTempC_->SetForegroundColor(red_);
		lblTempC_->ChangeOptions(lblTempC_->GetOptions() | kFixedWidth);
		rowT->AddFrame(lblTempC_, new TGLayoutHints(kLHintsRight, 20,1,1,1));

		AddFrame(xrGroup, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 10,10,10,10));

		status_ = new TGStatusBar(this);
		AddFrame(status_, new TGLayoutHints(kLHintsBottom|kLHintsExpandX));

		SetWindowName("X-ray Control (Hardware)");

		// Initialize from hardware state if available
		if (xray_) {
			// Read initial data
			xray_->ReadXRayData();
			XRay::XRayState st = xray_->GetXRayState();
			setV_kV_ = st.VoltageToSet;
			setI_uA_ = st.CurrentToSet;
			monV_kV_ = st.ActualVoltage;
			monI_uA_ = st.ActualCurrent;
			power_mW_ = st.ActualPower;
			temp_C_ = st.Temperature;
			powerOn_ = st.Power;
			// Adjust widgets to reflect hardware
			if (powerOn_) {
				lblPowerState_->SetText("Is on");
				lblPowerState_->SetBackgroundColor(red_);
				lblPowerState_->SetForegroundColor(white_);
				btnPower_->SetText("Switch off");
			}
			RefreshXRLabels();
		} else {
			// No hardware - use config defaults for display
			numSetV_->SetNumber(setV_kV_);
			numSetI_->SetNumber(setI_uA_);
		}

		// Start timer to poll hardware every 500ms
		timer_ = new TTimer(this, 500);
		timer_->TurnOn();

		MapSubwindows();
		Resize(GetDefaultSize());
		MapWindow();
		UpdateStatus();
	}

	virtual ~XRayGui() {
		if (timer_) {
			timer_->TurnOff();
			delete timer_;
		}
		// Turn off X-ray before closing
		if (xray_ && powerOn_) {
			xray_->SetXRayState(kFALSE);
		}
	}

	// Ensure clicking the window X quits the ROOT event loop
	virtual void CloseWindow() {
		// Turn off X-ray before exiting
		if (xray_ && powerOn_) {
			xray_->SetXRayState(kFALSE);
		}
		if (gApplication) {
			gApplication->Terminate(0);
		} else {
			gSystem->Exit(0);
		}
	}

	// Handle timer events to poll hardware
	virtual Bool_t HandleTimer(TTimer* /*timer*/) {
		if (xray_) {
			PullHardwareState();
			RefreshXRLabels();
			UpdateStatus();
		}
		return kTRUE;
	}

	Bool_t ProcessMessage(Long_t msg, Long_t parm1, Long_t /*parm2*/) {
		switch (GET_MSG(msg)) {
		case kC_COMMAND:
			switch (GET_SUBMSG(msg)) {
			case kCM_BUTTON:
				if (parm1 == XR_SWITCH) {
					TogglePower();
					return kTRUE;
				} else if (parm1 == XR_SETV) {
					setV_kV_ = numSetV_->GetNumber();
					if (xray_) {
						xray_->SetXRayVoltage((Float_t)setV_kV_);
						if (powerOn_) xray_->SetXRayHVAndCurrent();
						PullHardwareState();
					}
					else { if (powerOn_) monV_kV_ = setV_kV_; RecalcPower(); }
					RefreshXRLabels(); UpdateStatus();
					return kTRUE;
				} else if (parm1 == XR_SETI) {
					setI_uA_ = numSetI_->GetNumber();
					if (xray_) {
						xray_->SetXRayCurrent((Float_t)setI_uA_);
						if (powerOn_) xray_->SetXRayHVAndCurrent();
						PullHardwareState();
					}
					else { if (powerOn_) monI_uA_ = setI_uA_; RecalcPower(); }
					RefreshXRLabels(); UpdateStatus();
					return kTRUE;
				}
				break;
			default: break;
			}
			break;
		default: break;
		}
		return TGMainFrame::ProcessMessage(msg, parm1, 0);
	}

private:
	void TogglePower() {
		powerOn_ = !powerOn_;
		if (xray_) {
			// Apply desired settings before switching on
			if (powerOn_) {
				xray_->SetXRayVoltage((Float_t)setV_kV_);
				xray_->SetXRayCurrent((Float_t)setI_uA_);
				xray_->SetXRayHVAndCurrent();
				xray_->SetXRayState(kTRUE);
			} else {
				xray_->SetXRayState(kFALSE);
			}
			PullHardwareState();
		} else {
			// Fallback to local simulation if hardware pointer absent
			if (powerOn_) {
				monV_kV_ = setV_kV_;
				monI_uA_ = setI_uA_;
				temp_C_ = 25.0 + 2.0;
			} else {
				monV_kV_ = 0.0; monI_uA_ = 0.0; temp_C_ = 25.0;
			}
		}
		// Update widget visuals
		if (powerOn_) {
			lblPowerState_->SetText("Is on");
			lblPowerState_->SetBackgroundColor(red_);
			lblPowerState_->SetForegroundColor(white_);
			btnPower_->SetText("Switch off");
		} else {
			lblPowerState_->SetText("Is off");
			lblPowerState_->SetBackgroundColor(green_);
			lblPowerState_->SetForegroundColor(red_);
			btnPower_->SetText("Switch on");
		}
		RecalcPower(); RefreshXRLabels(); UpdateStatus();
	}

	void RecalcPower() { power_mW_ = monV_kV_ * monI_uA_; }

	void RefreshXRLabels() {
		char buf[64];
		sprintf_s(buf, sizeof(buf), "%.0f", monV_kV_);
		lblMonV_->SetText(buf);
		sprintf_s(buf, sizeof(buf), "%.0f", monI_uA_);
		lblMonI_->SetText(buf);
		sprintf_s(buf, sizeof(buf), "%.0f", power_mW_);
		lblPower_mW_->SetText(buf);
		sprintf_s(buf, sizeof(buf), "%.1f", temp_C_);
		lblTempC_->SetText(buf);
		Layout();
	}

	void UpdateStatus() {
		char s[128];
		sprintf_s(s, sizeof(s), "XR: %s  V=%.0f kV  I=%.0f uA  P=%.0f mW  T=%.1f C",
				 powerOn_ ? "ON" : "OFF", monV_kV_, monI_uA_, power_mW_, temp_C_);
		status_->SetText(s);
	}

	void PullHardwareState() {
		if (!xray_) return;
		xray_->ReadXRayData();
		XRay::XRayState st = xray_->GetXRayState();
		powerOn_ = st.Power;
		setV_kV_ = st.VoltageToSet; monV_kV_ = st.ActualVoltage;
		setI_uA_ = st.CurrentToSet; monI_uA_ = st.ActualCurrent;
		power_mW_ = st.ActualPower; temp_C_ = st.Temperature;
	}

private:
	// Hardware pointer
	XRay* xray_;
	// State
	bool   powerOn_;
	double setV_kV_, setI_uA_;
	double monV_kV_, monI_uA_;
	double power_mW_;
	double temp_C_;

	// Widgets
	TGTextButton *btnPower_, *btnSetV_, *btnSetI_;
	TGNumberEntry *numSetV_, *numSetI_;
	TGLabel *lblPowerState_, *lblMonV_, *lblMonI_, *lblPower_mW_, *lblTempC_;
	TGStatusBar *status_;
	TTimer *timer_;

	// Colors
	Pixel_t white_, black_, green_, red_;
};
} // namespace XR

#ifdef _MSC_VER
	// restore/create macros if they were pushed
	#ifdef _ROOT_PUSHED_CreateWindow
		#pragma pop_macro("CreateWindow")
		#undef _ROOT_PUSHED_CreateWindow
	#endif
	#ifdef _ROOT_PUSHED_max
		#pragma pop_macro("max")
		#undef _ROOT_PUSHED_max
	#endif
	#ifdef _ROOT_PUSHED_min
		#pragma pop_macro("min")
		#undef _ROOT_PUSHED_min
	#endif
	#ifdef _ROOT_PUSHED_SendMessage
		#pragma pop_macro("SendMessage")
		#undef _ROOT_PUSHED_SendMessage
	#endif
#endif


int APIENTRY _tWinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPTSTR    lpCmdLine,
					 int       nCmdShow)
{

	// Run a ROOT-based X-ray control GUI closely modeled after EventDisplay's XR panel.
	int argc = 1;
	char* argv[] = { (char*)"DualXRApp", nullptr };
	TApplication app("DualXRApp", &argc, argv);

	// Open log file
	gLogFile = fopen("xray_debug.log", "w");
	if (gLogFile) fprintf(gLogFile, "=== X-ray Control Software Started ===\n");

	// Create XRay hardware instance before GUI
	// Connect to first available device (device 0) regardless of serial number
	if (gLogFile) fprintf(gLogFile, "Creating XRay connection to first available device\n");
	gXRay = new XRay(nullptr);
	
	// Log the connected device serial number
	if (gXRay && gLogFile) {
		const char* serial = gXRay->GetSerialNumber();
		fprintf(gLogFile, "Connected to device with serial: %s\n", serial ? serial : "(none)");
	}
	
	// Set default values from config immediately after connecting
	if (gXRay) {
		double defaultV = ReadNumericFromConfig("qsv.conf", "XRVoltageToSet", 10.0);
		double defaultI = ReadNumericFromConfig("qsv.conf", "XRCurrentToSet", 5.0);
		if (gLogFile) fprintf(gLogFile, "Setting defaults: V=%.1f kV, I=%.1f uA\n", defaultV, defaultI);
		gXRay->SetXRayVoltage((Float_t)defaultV);
		gXRay->SetXRayCurrent((Float_t)defaultI);
		gXRay->SetXRayHVAndCurrent();
	}
	
	// Start named pipe server in background thread
	gServerMutex.Lock();
	gServerRunning = kTRUE;
	gServerMutex.UnLock();
	
	TThread* serverThread = new TThread("XRayServerThread", ServerThreadFunc, (void*)gXRay);
	serverThread->Run();
	if (gLogFile) fprintf(gLogFile, "Named pipe server thread started\n");
	
	// Create and show GUI, passing hardware driver pointer
	new XR::XRayGui(gClient->GetRoot(), gXRay, 420, 340);

	// Run GUI event loop
	app.Run();

	// Cleanup
	if (gLogFile) fprintf(gLogFile, "GUI closed, shutting down server...\n");
	gServerMutex.Lock();
	gServerRunning = kFALSE;
	gServerMutex.UnLock();
	
	if (serverThread) {
		serverThread->Join();
		delete serverThread;
	}
	
	if (gXRay) { delete gXRay; gXRay = NULL; }
	if (gLogFile) { 
		fprintf(gLogFile, "=== Application Shutdown ===\n");
		fclose(gLogFile); 
		gLogFile = NULL; 
	}
	return 0;

}



// (Win32 message loop and window procedures removed; this build keeps only USE_ROOT path.)
