#include "stdafx.h"
#include "XRay.h"

#include <stdio.h>
#include <TSystem.h>
#include <TRandom.h>
#include <string.h>
#ifdef _WIN32
#include "ipc/NamedPipeClient.h"
#include <vector>
#include <cstdlib>
#endif

static Vparams &params = *Vparams::getParams();

#ifdef _WIN32
// Save original printf before redefining
#undef printf
extern "C" int printf(const char *format, ...);
static int (*original_printf)(const char *, ...) = printf;
#define printf(fmt, ...)                    \
  do                                        \
  {                                         \
    original_printf(fmt, __VA_ARGS__);      \
    fprintf(params.fLog, fmt, __VA_ARGS__); \
    fflush(params.fLog);                    \
  } while (0)
#endif

// --- IPC helpers (Windows only) ---
#ifdef _WIN32
bool XRay::IPC_Send(const string& req, string* resp)
{
  if (!fPipeClient) return false;
  std::string reply;
  if (!fPipeClient->call(req, reply)) return false;
  if (resp) *resp = reply;
  return true;
}

void XRay::SplitTokens(const string& s, char delim, vector<string>& out)
{
  out.clear();
  size_t start = 0;
  while (start <= s.size())
  {
    size_t pos = s.find(delim, start);
    if (pos == string::npos) { out.push_back(s.substr(start)); break; }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
}
#endif

//---------------------------------------------------------------------------
XRay::XRay(const char *serialNumber)
{
  debug = params.verbose;
#ifdef _WIN32
  fUseRemote = false;
  fPipeClient = 0;
#endif
  if (serialNumber)
  {
    fSerialNumber = string(serialNumber);
    if (debug > 0)
      printf("In XRay constructor for serial number: %s\n", fSerialNumber.c_str());
  }
  else
  {
    if (debug > 0)
      printf("In XRay constructor\n");
  }

  // Get configuration parameters
  XRReadConfig();

  fXRayMutex = new TMutex;

  SetXRayState(kFALSE);
  fXRayState.ActualVoltage = 0.0;
  fXRayState.ActualCurrent = 0.0;
  fXRayState.ActualPower = 0.0;
  fXRayState.Temperature = 0.0;
  fXRayMode = REAL_TIME;
  fDeviceIndex = -1;
#ifndef _WIN32
  fXRReady = 0;
#endif

#ifdef _WIN32
  // Optional remote mode via named pipe. Enable by setting XRAY_REMOTE=1
  const char *remoteEnv = getenv("XRAY_REMOTE");
  if (remoteEnv && remoteEnv[0] == '1')
  {
    const char *pipeEnv = getenv("XRAY_PIPE_NAME");
    std::string pipeName = pipeEnv && pipeEnv[0] ? std::string(pipeEnv) : std::string("\\\\.\\pipe\\XRayService");
    fPipeClient = new NamedPipeClient(pipeName);
    if (fPipeClient->connect(5000))
    {
      // Handshake: INIT|<serialNumber or empty>
      std::string req = std::string("INIT|") + fSerialNumber;
      std::string resp;
      if (IPC_Send(req, &resp) && resp.rfind("OK", 0) == 0)
      {
        fUseRemote = true;
        if (debug > 0) printf("XRay running in REMOTE mode via %s\n", pipeName.c_str());
      }
      else
      {
        if (debug > 0) printf("Failed to INIT remote XRay service, falling back to local. Resp='%s'\n", resp.c_str());
        delete fPipeClient; fPipeClient = 0; fUseRemote = false;
      }
    }
    else
    {
      if (debug > 0) printf("Could not connect to XRayService pipe. Falling back to local.\n");
      delete fPipeClient; fPipeClient = 0; fUseRemote = false;
    }
  }
#endif

#ifndef VIEWER_ONLY
  if (!
#ifdef _WIN32
      fUseRemote
#else
      false
#endif
  )
  {
  // Define operation mode (Real or Simulated)
  printf("\n\t***** Start MiniX X-Ray tube controller application *****\n");
  OpenMiniX();
  gSystem->Sleep(100);

  if (isMiniXDlg())
  {
    // Enumerate and list available MiniX devices
	  ClearDeviceList();
    GetDeviceList();
    long deviceCount = GetDeviceCount();
    printf("Found %ld MiniX device(s)\n", deviceCount);

    if (deviceCount > 0)
    {
   
      // Try to connect to first device (device 0) and retrieve its serial number
      printf("Attempting to connect to device 0...\n");
	  char serialNumber[256];
	  GetDeviceSerialNumberByIndex(0, serialNumber);
      fSerialNumber = string(serialNumber);
      SetDevice(0);
      printf("Successfully connected to device 0 with serial number: %s\n", fSerialNumber.c_str());
    
    }
    else
    {
      // No devices found - run in simulation mode
      printf("No X-ray devices found. Running in SIMULATION mode\n");
      fXRayMode = SIMULATION;
      fDeviceIndex = -1;
      fSerialNumber = "SIM_MODE";
    }

    if (fXRayMode != SIMULATION)
    {
      SetDevice(fDeviceIndex);
      ReadMiniXMonitor(&fXRayMonitor);
      ReadMiniXSettings(&fXRaySettings);
      if (fXRayMonitor.mxmStatusInd == mxstMiniXApplicationReady)
      {

        SendMiniXCommand((byte)mxcStartMiniX);
        gSystem->Sleep(100);
        SetDevice(fDeviceIndex);
        ReadMiniXMonitor(&fXRayMonitor);

        long mxserial = ReadMiniXSerialNumber();
        printf("MiniX Serial Number %ld connected\n", mxserial);
        printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
        if (fXRayMonitor.mxmStatusInd == mxstMiniXControllerReady || fXRayMonitor.mxmStatusInd == mxstConnectingToMiniX)
        {
          fXRayMode = REAL_TIME;
          printf(" MiniX X-Ray tube is in REAL_TIME operation mode!\n");
        }
        else
        {
          fXRayMode = SIMULATION;
          printf("WARNING: MiniX X-Ray tube is in SIMULATION operation mode!\n");
        }
      }
      else
      {
        fXRayMode = SIMULATION;
        printf("WARNING: MiniX  X-Ray tube is in SIMULATION operation mode!\n");
      } // endif mxstMiniXApplicationReady
    } // endif fXRayMode != SIMULATION
  } // endif isMiniXDlg
  //  }

#else // VIEWER_ONLY
  fXRayMode = SIMULATION;
#endif
  }
}
//-----------------------------------------------------------------------------
void XRay::XRReadConfig()
{
  LevelParameter *p1 = params.conf->GetParameter<LevelParameter>("XRVoltageToSet");
  fXRayState.VoltageToSet = p1 ? (Float_t)p1->Level : XRVDEFAULT;

  LevelParameter *p2 = params.conf->GetParameter<LevelParameter>("XRCurrentToSet");
  fXRayState.CurrentToSet = p2 ? (Float_t)p2->Level : XRCURRENTDEFAULT;
}
//-----------------------------------------------------------------------------
void XRay::SetXRayState(Bool_t power)
{
  if (debug > 0)
    printf("In XRay::SetXRayState\n");
  fXRayMutex->Lock();

#ifdef _WIN32
  if (fUseRemote)
  {
    fXRayState.Power = power;
    char buf[128];
    sprintf(buf, "SET_POWER|%d|%.6f|%.6f", power ? 1 : 0, (double)fXRayState.VoltageToSet, (double)fXRayState.CurrentToSet);
    std::string resp; IPC_Send(buf, &resp);
    fXRayMutex->UnLock();
    return;
  }
#endif

  if (fXRayMode == REAL_TIME)
  {
    if (debug > 0)
      printf("XRay real mode\n");
    if (isMiniXDlg())
    {
      if (power)
      { // turn ON XRay
        if (debug > 0)
          printf("\n       ********* X-Ray tube will be powered ON *********\n");
        SetMiniXHV((double)fXRayState.VoltageToSet);
        //	gSystem->Sleep(100);
        SetMiniXCurrent((double)fXRayState.CurrentToSet);
        //	gSystem->Sleep(100);

        SetDevice(fDeviceIndex);
        ReadMiniXMonitor(&fXRayMonitor);
        if (debug > 0)
          printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
        if (fXRayMonitor.mxmEnabledCmds & mxcSetHVandCurrent)
        {
          SendMiniXCommand((byte)mxcSetHVandCurrent);
        }
        else
        {
          SetDevice(fDeviceIndex);
          ReadMiniXMonitor(&fXRayMonitor);
          printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
        }

        //	gSystem->Sleep(100);
        ReadMiniXSettings(&fXRaySettings);
        printf(" Corrected MiniX settings:\n XRayVMon=%.0f, XRayImon=%.0f\n", fXRaySettings.HighVoltage_kV, fXRaySettings.Current_uA);

        SetDevice(fDeviceIndex);
        ReadMiniXMonitor(&fXRayMonitor);
        if (fXRayMonitor.mxmEnabledCmds & mxcHVOn)
        {
          SendMiniXCommand((byte)mxcHVOn);
        }
        else
        {
          SetDevice(fDeviceIndex);
          ReadMiniXMonitor(&fXRayMonitor);
          printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
        }
      }
      else
      { // turn OFF XRay
        SetDevice(fDeviceIndex);
        ReadMiniXMonitor(&fXRayMonitor);
        if (fXRayMonitor.mxmEnabledCmds & mxcHVOff)
        {
          SendMiniXCommand((byte)mxcHVOff);
        }
        else
        {
          SetDevice(fDeviceIndex);
          ReadMiniXMonitor(&fXRayMonitor);
          printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
        }
        fXRayState.ActualVoltage = 0.0;
        fXRayState.ActualCurrent = 0.0;
      } // end turn OFF XRay
    }
    else
    {
      printf(" MiniX status: no MiniXDlg\n");
    }
  }

  if (fXRayMode == SIMULATION)
  {
    if (debug > 0)
      printf("XRay in simulation mode!\n");
    if (power)
    {
      fXRayState.ActualVoltage = 0.99f * fXRayState.VoltageToSet + 0.02f * fXRayState.VoltageToSet * (Float_t)gRandom->Rndm();
      fXRayState.ActualCurrent = 0.99f * fXRayState.CurrentToSet + 0.02f * fXRayState.CurrentToSet * (Float_t)gRandom->Rndm();
    }
    else
    {
      fXRayState.ActualVoltage = 0.0;
      fXRayState.ActualCurrent = 0.0;
    }
  }

  fXRayState.Power = power;
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
XRay::XRayState XRay::GetXRayState()
{
  if (debug > 1)
    printf("In XRay::GetXRayState\n");
  XRayState myXRayState;
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp;
    if (IPC_Send("GET_STATE", &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 8)
      {
        fXRayState.Power = atoi(tok[1].c_str()) != 0;
        fXRayState.VoltageToSet = (Float_t)atof(tok[2].c_str());
        fXRayState.ActualVoltage = (Float_t)atof(tok[3].c_str());
        fXRayState.CurrentToSet = (Float_t)atof(tok[4].c_str());
        fXRayState.ActualCurrent = (Float_t)atof(tok[5].c_str());
        fXRayState.ActualPower = (Float_t)atof(tok[6].c_str());
        fXRayState.Temperature = (Float_t)atof(tok[7].c_str());
      }
    }
    myXRayState = fXRayState;
    fXRayMutex->UnLock();
    return myXRayState;
  }
#endif
  myXRayState = fXRayState;
  fXRayMutex->UnLock();
  return myXRayState;
}
//---------------------------------------------------------------------------
void XRay::PrintStatus()
{
  if (debug > 0)
    printf("In XRay::PrintStatus\n");
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp; IPC_Send("PRINT_STATUS", &resp);
  }
#endif
  printf("********* X-ray status: *********\n");
  fXRayMutex->Lock();
  if (fXRayState.Power)
    printf("Power: On\n");
  else
    printf("Power: Off\n");
  printf("VoltageToSet: %f\n", fXRayState.VoltageToSet);
  printf("ActualVoltage: %f\n", fXRayState.ActualVoltage);
  printf("CurrentToSet: %f\n", fXRayState.CurrentToSet);
  printf("ActualCurrent: %f\n", fXRayState.ActualCurrent);
  printf("ActualPower: %f\n", fXRayState.ActualPower);
  printf("ActualTemperature: %f, C\n", fXRayState.Temperature);
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::SetXRayVoltage(Float_t xVoltage)
{
  if (debug > 0)
    printf("In XRay::SetXRayVoltage\n");
  fXRayMutex->Lock();
  fXRayState.VoltageToSet = xVoltage;
#ifdef _WIN32
  if (fUseRemote)
  {
    char buf[64]; sprintf(buf, "SET_VOLTAGE|%.6f", (double)xVoltage);
    std::string resp; IPC_Send(buf, &resp);
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      // Inline the hardware calls to avoid deadlock
      SetMiniXHV((double)fXRayState.VoltageToSet);
      SetMiniXCurrent((double)fXRayState.CurrentToSet);
      SetDevice(fDeviceIndex);
      ReadMiniXMonitor(&fXRayMonitor);
      if (fXRayMonitor.mxmEnabledCmds & mxcSetHVandCurrent)
      {
        SendMiniXCommand((byte)mxcSetHVandCurrent);
      }
      else
      {
        SetDevice(fDeviceIndex);
        ReadMiniXMonitor(&fXRayMonitor);
        printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
      }
    }
  }
  else if (fXRayMode == SIMULATION)
    fXRayState.ActualVoltage = 0.95f * fXRayState.VoltageToSet + 0.1f * fXRayState.VoltageToSet * (Float_t)gRandom->Rndm();
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::SetXRayCurrent(Float_t xCurrent)
{
  if (debug > 0)
    printf("In XRay::SetXRayCurrent\n");
  fXRayMutex->Lock();
  fXRayState.CurrentToSet = xCurrent;
#ifdef _WIN32
  if (fUseRemote)
  {
    char buf[64]; sprintf(buf, "SET_CURRENT|%.6f", (double)xCurrent);
    std::string resp; IPC_Send(buf, &resp);
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      // Inline the hardware calls to avoid deadlock
      SetMiniXHV((double)fXRayState.VoltageToSet);
      SetMiniXCurrent((double)fXRayState.CurrentToSet);
      SetDevice(fDeviceIndex);
      ReadMiniXMonitor(&fXRayMonitor);
      if (fXRayMonitor.mxmEnabledCmds & mxcSetHVandCurrent)
      {
        SendMiniXCommand((byte)mxcSetHVandCurrent);
      }
      else
      {
        SetDevice(fDeviceIndex);
        ReadMiniXMonitor(&fXRayMonitor);
        printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
      }
    }
  }
  else if (fXRayMode == SIMULATION)
    fXRayState.ActualCurrent = 0.95f * fXRayState.CurrentToSet + 0.1f * fXRayState.CurrentToSet * (Float_t)gRandom->Rndm();
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::SetXRayHVAndCurrent()
{
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp; IPC_Send("SET_HV_I", &resp);
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      SetMiniXHV((double)fXRayState.VoltageToSet);
      SetMiniXCurrent((double)fXRayState.CurrentToSet);
      SetDevice(fDeviceIndex);
      ReadMiniXMonitor(&fXRayMonitor);
      if (fXRayMonitor.mxmEnabledCmds & mxcSetHVandCurrent)
      {
        SendMiniXCommand((byte)mxcSetHVandCurrent);
      }
      else
      {
        SetDevice(fDeviceIndex);
        ReadMiniXMonitor(&fXRayMonitor);
        printf(" MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
      }
    }
  }
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::ReadXRayVoltage()
{
  if (debug > 0)
    printf("In XRay::ReadXRayVoltage\n");
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp;
    if (IPC_Send("READ_DATA", &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 8)
      {
        fXRayState.Power = atoi(tok[1].c_str()) != 0;
        fXRayState.VoltageToSet = (Float_t)atof(tok[2].c_str());
        fXRayState.ActualVoltage = (Float_t)atof(tok[3].c_str());
        fXRayState.CurrentToSet = (Float_t)atof(tok[4].c_str());
        fXRayState.ActualCurrent = (Float_t)atof(tok[5].c_str());
        fXRayState.ActualPower = (Float_t)atof(tok[6].c_str());
        fXRayState.Temperature = (Float_t)atof(tok[7].c_str());
      }
    }
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      SetDevice(fDeviceIndex);
      ReadMiniXMonitor(&fXRayMonitor);
      // if(fXRayMonitor.mxmRefreshed){
      // ReadMiniXMonitor(&fXRayMonitor);
      fXRayState.ActualVoltage = (Float_t)fXRayMonitor.mxmHighVoltage_kV;
      //}
    }
  }
  else if (fXRayMode == SIMULATION)
  {
    if (fXRayState.Power)
      fXRayState.ActualVoltage = 0.95f * fXRayState.VoltageToSet + 0.1f * fXRayState.VoltageToSet * (Float_t)gRandom->Rndm();
    else
      fXRayState.ActualVoltage = 0;
  }
  if (debug > 1)
    printf("fXRayState.ActualVoltage=%f\n", fXRayState.ActualVoltage);
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::ReadXRayCurrent()
{
  if (debug > 0)
    printf("In XRay::ReadXRayCurrent\n");
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp;
    if (IPC_Send("READ_DATA", &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 8)
      {
        fXRayState.Power = atoi(tok[1].c_str()) != 0;
        fXRayState.VoltageToSet = (Float_t)atof(tok[2].c_str());
        fXRayState.ActualVoltage = (Float_t)atof(tok[3].c_str());
        fXRayState.CurrentToSet = (Float_t)atof(tok[4].c_str());
        fXRayState.ActualCurrent = (Float_t)atof(tok[5].c_str());
        fXRayState.ActualPower = (Float_t)atof(tok[6].c_str());
        fXRayState.Temperature = (Float_t)atof(tok[7].c_str());
      }
    }
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      SetDevice(fDeviceIndex);
      ReadMiniXMonitor(&fXRayMonitor);
      // if(fXRayMonitor.mxmRefreshed){
      // ReadMiniXMonitor(&fXRayMonitor);
      fXRayState.ActualCurrent = (Float_t)fXRayMonitor.mxmCurrent_uA;
      //}
    }
  }
  else if (fXRayMode == SIMULATION)
  {
    if (fXRayState.Power)
      fXRayState.ActualCurrent = 0.95f * fXRayState.CurrentToSet + 0.1f * fXRayState.CurrentToSet * (Float_t)gRandom->Rndm();
    else
      fXRayState.ActualCurrent = 0;
  }
  if (debug > 1)
    printf("fXRayState.ActualCurrent=%f\n", fXRayState.ActualCurrent);
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::ReadXRayPowerDraw()
{
  if (debug > 0)
    printf("In XRay::ReadXRayPowerDraw\n");
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp;
    if (IPC_Send("READ_DATA", &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 8)
      {
        fXRayState.Power = atoi(tok[1].c_str()) != 0;
        fXRayState.VoltageToSet = (Float_t)atof(tok[2].c_str());
        fXRayState.ActualVoltage = (Float_t)atof(tok[3].c_str());
        fXRayState.CurrentToSet = (Float_t)atof(tok[4].c_str());
        fXRayState.ActualCurrent = (Float_t)atof(tok[5].c_str());
        fXRayState.ActualPower = (Float_t)atof(tok[6].c_str());
        fXRayState.Temperature = (Float_t)atof(tok[7].c_str());
      }
    }
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      // ReadMiniXMonitor(&fXRayMonitor);
      // if(fXRayMonitor.mxmRefreshed){
      fXRayState.ActualPower = (Float_t)fXRayMonitor.mxmPower_mW;
      //}
    }
  }
  else if (fXRayMode == SIMULATION)
  {
    if (fXRayState.Power)
      fXRayState.ActualPower = 0.95f * fXRayState.VoltageToSet * fXRayState.CurrentToSet + 0.1f * fXRayState.VoltageToSet * fXRayState.CurrentToSet * (Float_t)gRandom->Rndm();
    else
      fXRayState.ActualPower = 0;
  }
  if (debug > 1)
    printf("fXRayState.ActualPower=%f\n", fXRayState.ActualPower);
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::ReadXRayTemperature()
{
  if (debug > 0)
    printf("In XRay::ReadXRayTemperature\n");
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp;
    if (IPC_Send("READ_DATA", &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 8)
      {
        fXRayState.Power = atoi(tok[1].c_str()) != 0;
        fXRayState.VoltageToSet = (Float_t)atof(tok[2].c_str());
        fXRayState.ActualVoltage = (Float_t)atof(tok[3].c_str());
        fXRayState.CurrentToSet = (Float_t)atof(tok[4].c_str());
        fXRayState.ActualCurrent = (Float_t)atof(tok[5].c_str());
        fXRayState.ActualPower = (Float_t)atof(tok[6].c_str());
        fXRayState.Temperature = (Float_t)atof(tok[7].c_str());
      }
    }
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      // ReadMiniXMonitor(&fXRayMonitor);
      // if(fXRayMonitor.mxmRefreshed){
      fXRayState.Temperature = (Float_t)fXRayMonitor.mxmTemperatureC;
      //}
    }
  }
  else if (fXRayMode == SIMULATION)
  {
    if (fXRayState.Power)
      fXRayState.Temperature = 30 + 2 * (Float_t)gRandom->Rndm();
    else
      fXRayState.Temperature = 0;
  }
  if (debug > 1)
    printf("fXRayState.Temperature=%f\n", fXRayState.Temperature);
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
void XRay::ReadXRayData()
{
  if (debug > 1)
    printf("In XRay::ReadXRayData\n");
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp;
    if (IPC_Send("READ_DATA", &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 8)
      {
        fXRayState.Power = atoi(tok[1].c_str()) != 0;
        fXRayState.VoltageToSet = (Float_t)atof(tok[2].c_str());
        fXRayState.ActualVoltage = (Float_t)atof(tok[3].c_str());
        fXRayState.CurrentToSet = (Float_t)atof(tok[4].c_str());
        fXRayState.ActualCurrent = (Float_t)atof(tok[5].c_str());
        fXRayState.ActualPower = (Float_t)atof(tok[6].c_str());
        fXRayState.Temperature = (Float_t)atof(tok[7].c_str());
      }
    }
    if (debug > 1)
    {
      printf("(remote) VoltageToSet: %f\n", fXRayState.VoltageToSet);
      printf("(remote) CurrentToSet: %f\n", fXRayState.CurrentToSet);
      printf("(remote) fXRayState.ActualVoltage=%f\n", fXRayState.ActualVoltage);
      printf("(remote) fXRayState.ActualCurrent=%f\n", fXRayState.ActualCurrent);
      printf("(remote) fXRayState.ActualPower=%f\n", fXRayState.ActualPower);
      printf("(remote) fXRayState.Temperature=%f\n", fXRayState.Temperature);
    }
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      SetDevice(fDeviceIndex);
      ReadMiniXMonitor(&fXRayMonitor);
      if (fXRayMonitor.mxmRefreshed)
      {
        fXRayState.ActualVoltage = (Float_t)fXRayMonitor.mxmHighVoltage_kV;
        fXRayState.ActualCurrent = (Float_t)fXRayMonitor.mxmCurrent_uA;
        fXRayState.ActualPower = (Float_t)fXRayMonitor.mxmPower_mW;
        fXRayState.Temperature = (Float_t)fXRayMonitor.mxmTemperatureC;
      }
    }
  }
  else if (fXRayMode == SIMULATION)
  {
    if (fXRayState.Power)
    {
      fXRayState.ActualVoltage = 0.95f * fXRayState.VoltageToSet + 0.1f * fXRayState.VoltageToSet * (Float_t)gRandom->Rndm();
      fXRayState.ActualCurrent = 0.95f * fXRayState.CurrentToSet + 0.1f * fXRayState.CurrentToSet * (Float_t)gRandom->Rndm();
      fXRayState.ActualPower = 0.95f * fXRayState.VoltageToSet * fXRayState.CurrentToSet + 0.1f * fXRayState.VoltageToSet * fXRayState.CurrentToSet * (Float_t)gRandom->Rndm();
      fXRayState.Temperature = 30 + 2 * (Float_t)gRandom->Rndm();
    }
    else
    {
      fXRayState.ActualVoltage = 0;
      fXRayState.ActualCurrent = 0;
      fXRayState.ActualPower = 0;
      fXRayState.Temperature = 0;
    }
  }

  if (debug > 1)
  {
    printf("MiniX status: %s\n", GetMiniXStatusString(fXRayMonitor.mxmStatusInd));
    printf("VoltageToSet: %f\n", fXRayState.VoltageToSet);
    printf("CurrentToSet: %f\n", fXRayState.CurrentToSet);
    printf("fXRayState.ActualVoltage=%f\n", fXRayState.ActualVoltage);
    printf("fXRayState.ActualCurrent=%f\n", fXRayState.ActualCurrent);
    printf("fXRayState.ActualPower=%f\n", fXRayState.ActualPower);
    printf("fXRayState.Temperature=%f\n", fXRayState.Temperature);
    if (fXRayMonitor.mxmOutOfRange)
      printf("fXRayMonitor.mxmOutOfRange=%d\n", 1);
    else
      printf("fXRayMonitor.mxmOutOfRange=%d\n", 0);
    if (fXRayMonitor.mxmInterLock)
      printf("fXRayMonitor.mxmInterLock=%d\n", 1);
    else
      printf("fXRayMonitor.mxmInterLock=%d\n", 0);
    if (fXRayMonitor.mxmRefreshed)
      printf("fXRayMonitor.mxmRefreshed=%d\n", 1);
    else
      printf("fXRayMonitor.mxmRefreshed=%d\n", 0);
  }
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
XRay::~XRay()
{
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp;
    IPC_Send("SHUTDOWN", &resp); // optional
    if (fPipeClient) { fPipeClient->disconnect(); delete fPipeClient; fPipeClient = 0; }
  }
  else
#endif
  {
    if (isMiniXDlg())
    {
      SendMiniXCommand((byte)mxcExit);
      gSystem->Sleep(100);
    }
    if (isMiniXDlg())
    {
      CloseMiniX();
      gSystem->Sleep(100);
    }
  }
  if (fXRayMutex)
  {
    delete fXRayMutex;
    fXRayMutex = 0;
  }
}
//---------------------------------------------------------------------------
void XRay::GetDeviceList()
{
  if (debug > 0)
    printf("In XRay::GetDeviceList\n");
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp; IPC_Send("GET_DEVICE_LIST", &resp);
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      ::GetDeviceList();
    }
  }
}
//---------------------------------------------------------------------------
long XRay::GetDeviceCount()
{
  if (debug > 0)
    printf("In XRay::GetDeviceCount\n");
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string resp; if (IPC_Send("GET_DEVICE_COUNT", &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 2) return atol(tok[1].c_str());
    }
    return 0;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      return ::GetDeviceCount();
    }
  }
  return 0;
}
//---------------------------------------------------------------------------
long XRay::GetDeviceSerialNumberByIndex(long lDeviceIndex, char *strSerialNumber)
{
  if (debug > 0)
    printf("In XRay::GetDeviceSerialNumberByIndex\n");
#ifdef _WIN32
  if (fUseRemote)
  {
    char buf[64]; sprintf(buf, "GET_DEVICE_SERIAL|%ld", lDeviceIndex);
    std::string resp; if (IPC_Send(buf, &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 3)
      {
        if (strSerialNumber) strncpy(strSerialNumber, tok[2].c_str(), 255);
        return atol(tok[1].c_str());
      }
    }
    return -1;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      return ::GetDeviceSerialNumberByIndex(lDeviceIndex, strSerialNumber);
    }
  }
  return -1;
}
//---------------------------------------------------------------------------
void XRay::SetDevice(long lDeviceIndex)
{
  if (debug > 0)
    printf("In XRay::SetDevice\n");
  fXRayMutex->Lock();
#ifdef _WIN32
  if (fUseRemote)
  {
    char buf[64]; sprintf(buf, "SET_DEVICE|%ld", lDeviceIndex);
    std::string resp; IPC_Send(buf, &resp);
    fXRayMutex->UnLock();
    return;
  }
#endif
  if (fXRayMode == REAL_TIME)
  {
    if (isMiniXDlg())
    {
      ::SetDevice(lDeviceIndex);
      if (debug > 0)
        printf("Set MiniX device to index %ld\n", lDeviceIndex);
    }
    else
    {
      printf("Cannot set device - MiniX dialog not available\n");
    }
  }
  else if (fXRayMode == SIMULATION)
  {
    if (debug > 0)
      printf("SetDevice in simulation mode - no action taken\n");
  }
  fXRayMutex->UnLock();
}
//---------------------------------------------------------------------------
Bool_t XRay::ExecCommand(string cmdstr, string *result)
{
  *result = "OK";
#ifdef _WIN32
  if (fUseRemote)
  {
    std::string req = std::string("EXEC|") + cmdstr;
    std::string resp; if (IPC_Send(req, &resp) && resp.rfind("OK|", 0) == 0)
    {
      std::vector<std::string> tok; SplitTokens(resp, '|', tok);
      if (tok.size() >= 2) *result = tok[1];
      return kTRUE;
    }
    *result = "ERR";
    return kFALSE;
  }
#endif
  return kTRUE;
}
