// Dual_XRay_Control_Software.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Dual_XRay_Control_Software.h"


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
#include <stdio.h>

// A small self-contained ROOT GUI that replicates the X-ray panel from EventDisplay.cxx
// NOTE: This is GUI-only (no hardware I/O). Values update locally when you click Set.
namespace XR {
enum {
	XR_SWITCH = 1001,
	XR_SETV   = 1002,
	XR_SETI   = 1003
};

class XRayGui : public TGMainFrame {
public:
	XRayGui(const TGWindow* p, UInt_t w=400, UInt_t h=280)
		: TGMainFrame(p, w, h, kVerticalFrame)
		, powerOn_(false)
		, setV_kV_(40.0)
		, setI_uA_(50.0)
		, monV_kV_(0.0)
		, monI_uA_(0.0)
		, power_mW_(0.0)
		, temp_C_(25.0)
	{
		SetCleanup(kDeepCleanup);

		// Colors
		gClient->GetColorByName("white", white_);
		gClient->GetColorByName("black", black_);
		gClient->GetColorByName("green", green_);
		gClient->GetColorByName("red", red_);

		// Root vertical layout
		auto* xrGroup = new TGGroupFrame(this, "X-ray");
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

		SetWindowName("X-ray Control (GUI only)");
		MapSubwindows();
		Resize(GetDefaultSize());
		MapWindow();
		UpdateStatus();
	}

	virtual ~XRayGui() {}

	// Ensure clicking the window X quits the ROOT event loop
	virtual void CloseWindow() {
		if (gApplication) {
			gApplication->Terminate(0);
		} else {
			gSystem->Exit(0);
		}
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
					if (powerOn_) monV_kV_ = setV_kV_;
					RecalcPower();
					RefreshXRLabels();
					UpdateStatus();
					return kTRUE;
				} else if (parm1 == XR_SETI) {
					setI_uA_ = numSetI_->GetNumber();
					if (powerOn_) monI_uA_ = setI_uA_;
					RecalcPower();
					RefreshXRLabels();
					UpdateStatus();
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
		if (powerOn_) {
			lblPowerState_->SetText("Is on");
			lblPowerState_->SetBackgroundColor(red_);
			lblPowerState_->SetForegroundColor(white_);
			btnPower_->SetText("Switch off");
			// Assume immediate stabilization to set values for demo
			monV_kV_ = setV_kV_;
			monI_uA_ = setI_uA_;
			// Heat up a little on power on
			temp_C_ = 25.0 + 2.0;
		} else {
			lblPowerState_->SetText("Is off");
			lblPowerState_->SetBackgroundColor(green_);
			lblPowerState_->SetForegroundColor(red_);
			btnPower_->SetText("Switch on");
			monV_kV_ = 0.0;
			monI_uA_ = 0.0;
			temp_C_ = 25.0;
		}
		RecalcPower();
		RefreshXRLabels();
		UpdateStatus();
	}

	void RecalcPower() {
		// mW = kV * uA
		power_mW_ = monV_kV_ * monI_uA_;
	}

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

private:
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

	// Create and show GUI
	new XR::XRayGui(gClient->GetRoot(), 420, 320);

	app.Run();
	return 0;

}



// (Win32 message loop and window procedures removed; this build keeps only USE_ROOT path.)
