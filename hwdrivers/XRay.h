#ifndef XRAY_H
#define XRAY_H

#include <stdio.h>
#include "viewer/Vparams.h"
#ifdef _WIN32
#include "hwdrivers/miniX/MiniXAPI.h"
#else
#include "hwdrivers/miniX/MiniXAPI_Linux.h"
#endif

#include <TMutex.h>

using namespace std;

//===========================================
class XRay
{
 public:
  XRay(const char* serialNumber = NULL);
  ~XRay();

  typedef struct {
    Bool_t Power;
    Float_t VoltageToSet, ActualVoltage;
    Float_t CurrentToSet, ActualCurrent;
    Float_t ActualPower, Temperature;
  } XRayState;

  void SetXRayState(Bool_t);
  void PrintStatus();
  XRayState GetXRayState();
  void SetXRayVoltage(Float_t);
  void SetXRayCurrent(Float_t);
  void SetXRayHVAndCurrent();
  void ReadXRayVoltage();
  void ReadXRayCurrent();
  void ReadXRayPowerDraw();
  void ReadXRayTemperature();
  void ReadXRayData();
  HWmode_t GetXRayMode() {return fXRayMode;};
  void SetDebugLevel(Int_t newdebug){debug = newdebug;};
  void GetDeviceList();
  long GetDeviceCount();
  long GetDeviceSerialNumberByIndex(long lDeviceIndex, char *strSerialNumber);
  void SetDevice(long lDeviceIndex);
  Bool_t ExecCommand(string, string*);

  // Simulation mode for Linux
#ifndef _WIN32
  void OpenMiniX(){printf("In OpenMiniX\n"); fXRReady=1;};
  byte isMiniXDlg(){return fXRReady;};
  void CloseMiniX(){printf("In CloseMiniX\n");};
  void SendMiniXCommand(byte MiniXCommand){printf("In SendMiniXCommand\n");};
  void ReadMiniXMonitor(MiniX_Monitor *MiniXMonitor){printf("In ReadMiniXMonitor\n");};
  void SetMiniXHV(double HighVoltage_kV){printf("In SetMiniXHV\n");};
  void SetMiniXCurrent(double Current_uA){printf("In SetMiniXCurrent\n");};
  void ReadMiniXSettings(MiniX_Settings *MiniXSettings){printf("In ReadMiniXSettings\n");};
  long ReadMiniXSerialNumber(){printf("In ReadMiniXSerialNumber\n"); return 123456789;};
  void GetDeviceList(){printf("In GetDeviceList\n");};
  long GetDeviceCount(){printf("In GetDeviceCount\n"); return 1;};
  long GetDeviceSerialNumberByIndex(long lDeviceIndex, char *strSerialNumber){printf("In GetDeviceSerialNumberByIndex\n"); sprintf(strSerialNumber, "SIM123456789"); return 0;};
  void SetDevice(long lDeviceIndex){printf("In SetDevice\n");};
#endif

//---------------------------------
 private:
  void XRReadConfig();
  Int_t debug;
  HWmode_t fXRayMode;
  XRayState fXRayState;
  MiniX_Monitor fXRayMonitor;
  MiniX_Settings fXRaySettings;
  TMutex *fXRayMutex;
  string fSerialNumber;  // Serial number of this X-ray device
  long fDeviceIndex;     // Device index of this X-ray device
#ifndef _WIN32
  byte fXRReady;
#endif
#ifdef _WIN32
  // When enabled, XRay methods forward to a remote service via named pipe
  bool fUseRemote; 
  class NamedPipeClient* fPipeClient; // forward decl; implemented in ipc/NamedPipeClient.h
  bool IPC_Send(const string& req, string* resp);
  static void SplitTokens(const string& s, char delim, vector<string>& out);
#endif
};
#endif //XRAY_H
