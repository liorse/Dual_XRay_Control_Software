
#if !defined(MINIXAPI_H__INCLUDED_)
#define MINIXAPI_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "windows.h"

//#define WINAPI __stdcall
typedef const char* CString; 
typedef unsigned char byte;

//holds monitored values from ReadMiniXMonitor
typedef struct _MiniX_Monitor {
    double mxmHighVoltage_kV;   //high voltage monitor
    double mxmCurrent_uA;       //current monitor
    double mxmPower_mW;         //power in milliwatts
    double mxmTemperatureC;     //temperature in degrees C
    byte mxmRefreshed;          //monitor data refresh ok
    byte mxmInterLock;          //0=Open,1=Restored(Closed)
    byte mxmEnabledCmds;        //command button enables
    byte mxmStatusInd;          //minix status indicator
    byte mxmOutOfRange;         //wattage value out of range
    byte mxmHVOn;               //high voltage on indicator
    double mxmReserved;         //reserved, should be 123.456
} MiniX_Monitor, *LP_MiniX_Monitor;

//holds corrected settings from ReadMiniXSettings
typedef struct _MiniX_Settings {
    double HighVoltage_kV;              //high voltage setting
    double Current_uA;                  //current setting
} MiniX_Settings, *LP_MiniX_Settings;

//commands that can be sent with SendMiniXCommand
//ALSO
//command enable masks for mxmEnabledCmds
enum MiniX_Commands {
    mxcDisabled = 0,        //null command
    mxcStartMiniX = 1,      //start minix controller
    mxcHVOn = 2,            //turn high voltage on
    mxcHVOff = 4,           //turn high voltage off
    mxcSetHVandCurrent = 8, //set high voltage and current
    mxcExit = 16            //exit controller
};

//minix controller status codes
enum MiniX_Status {
    mxstNoStatus,                       //no status available
    mxstDriversNotLoaded,               //drivers were not found, install drivers
    mxstMiniXApplicationReady,          //application is ready to connect to minix
    mxstPortCLOSED,                     //minix detected, port closed, will attempt connect
    mxstNoDevicesAttached,              //minix is not connected or is not powered
    mxstMiniXControllerSelected,        //minix has been found
    mxstMiniXControllerReady,           //minix connected and ready for first command
    mxstMiniXControllerFailedToOpen,    //minix detected, but failed to open
    mxstNoDeviceSelected,               //could not select minix device
    mxstRequestedVoltageOutOfRange,     //hv was selected out of range,api will set in range
    mxstRequestedCurrentOutOfRange,     //uA was selected out of range,api will set in range
    mxstConnectingToMiniX,              //api busy attempting to connect to minix
    mxstUpdatingSettings,               //api busy updating settings
    mxstMiniXReady                      //ready for next operation
};

//OpenMiniX - Opens an Instance of a MiniX Controller Application
void WINAPI OpenMiniX();

//isMiniXDlg - Test for an Instance of a MiniX Controller Application
byte WINAPI isMiniXDlg();

//CloseMiniX - Closes an Instance of a MiniX Controller Application
void WINAPI CloseMiniX();

//SendMiniXCommand - Sends Commands to MiniX
void WINAPI SendMiniXCommand(byte MiniXCommand);

//ReadMiniXMonitor - Reads Monitored Values
void WINAPI ReadMiniXMonitor(MiniX_Monitor *MiniXMonitor);

//SetMiniXHV - Sets Requested HighVoltage (kV)
void WINAPI SetMiniXHV(double HighVoltage_kV);

//SetMiniXCurrent - Sets Requested Current (uA)
void WINAPI SetMiniXCurrent(double Current_uA);

//ReadMiniXSettings - Reads Actual Values Set
void WINAPI ReadMiniXSettings(MiniX_Settings *MiniXSettings);

//ReadMiniXSerialNumber - Read MiniX Serial Number
long WINAPI ReadMiniXSerialNumber();

//Clear Device List
void WINAPI ClearDeviceList();

//Get Device List
void WINAPI GetDeviceList();

//Get Device Count
long WINAPI GetDeviceCount();

//Get Device Serial Number String
long WINAPI GetDeviceSerialNumberByIndex(long lDeviceIndex, char *strSerialNumber);

//Set MiniX Device by Device Index
void WINAPI SetDevice(long lDeviceIndex);

//ReadMiniXDeviceType - Read MiniX Device Type
long WINAPI ReadMinixOemMxDeviceType();

//converts minix controller status codes (mxmStatusInd) to text strings
static CString GetMiniXStatusString(byte monitorMiniXStatus)
{
    CString cstrMiniXStatus;
    switch (monitorMiniXStatus) {
        case mxstNoStatus:
            cstrMiniXStatus = "No Status";
            break;
        case mxstDriversNotLoaded:
            cstrMiniXStatus = "Drivers not loaded, please install MiniX drivers.";
            break;
        case mxstMiniXApplicationReady:
            cstrMiniXStatus = "MiniX application ready.";
            break;
        case mxstPortCLOSED:
            cstrMiniXStatus = "Port CLOSED";
            break;
        case mxstNoDevicesAttached:
            cstrMiniXStatus = "No devices attached";
            break;
        case mxstMiniXControllerSelected:
            cstrMiniXStatus = "MiniX Controller selected";
            break;
        case mxstMiniXControllerReady:
            cstrMiniXStatus = "MiniX Controller ready";
            break;
        case mxstMiniXControllerFailedToOpen:
            cstrMiniXStatus = "MiniX Controller failed to open";
            break;
        case mxstNoDeviceSelected:
            cstrMiniXStatus = "No device selected";
            break;
        case mxstRequestedVoltageOutOfRange:
            cstrMiniXStatus = "Requested voltage out of range.  Please enter voltage in range.";
            break;
        case mxstRequestedCurrentOutOfRange:
            cstrMiniXStatus = "Requested current out of range.  Please enter current in range.";
            break;
        case mxstConnectingToMiniX:
            cstrMiniXStatus = "Connecting to MiniX. Please wait...";
            break;
        case mxstUpdatingSettings:
            cstrMiniXStatus = "Updating settings. Please wait...";
            break;
        case mxstMiniXReady:
            cstrMiniXStatus = "Mini-X Ready";
            break;
        default:
            cstrMiniXStatus = "";
            break;
    }
    return(cstrMiniXStatus);
}

#endif // !defined(MINIXAPI_H__INCLUDED_)
