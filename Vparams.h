#ifndef VPARAMS_H
#define VPARAMS_H

#include <TROOT.h>

#include "config/AnalysisConfig.h"

// ******************************* Global parameters *************************

// Main program version: hard-coded for Windows and automatically extracted
// from SVN repository and $ROOTSYS environment for Linux
#ifdef _WIN32
#define MYREVISION "115"
#define MYROOT "5.34.14"
#endif
// Version of output data file format
#define FFVERSION 2

// Define VIEWER_ONLY to compile program version with viewer capabilities only,
// without scanner - just to process already saved data files
//#define VIEWER_ONLY
#ifdef VIEWER_ONLY
#define PROGNAME "QSViewer"
#else
#define PROGNAME "QScanner"
#endif

// Choice of production cite: select just one from the list
#define WEIZMANN
//#define CHILE
//#define PNPI
//#define CANADA
//#define CHINA
//#define MEPHI

#ifdef WEIZMANN
#define CITE "WEIZMANN"
#elif defined(CHILE)
#define CITE "CHILE"
#elif defined(PNPI)
#define CITE "PNPI"
#elif defined(CANADA)
#define CITE "CANADA"
#elif defined(CHINA)
#define CITE "CHINA"
#elif defined(MEPHI)
#define CITE "MEPHI"
#else
#define CITE "UNKNOWN"
#endif

// Define BLACK_SCANNER or WHITE_SCANNER to compile proper program version
// for corresponding scanner construction
#ifdef CANADA
#define WHITE_SCANNER
#else
#define BLACK_SCANNER
#endif

// ******************************* Tested chambers *************************
// Number of tested chambers
#define NCHAMBERS 4

// ******************************* GUI parameters *************************

// Main ROOT canvas initial size, pixels
#define XDSIZE 600
#define YDSIZE 600
// Zoom Window initial size, pixels
#define XZOOMSIZE 800
#define YZOOMSIZE 600
// Projection canvas initial size, pixels
#define XPROJSIZE 450
#define YPROJSIZE 600
// Magnification in zoom window
#define ZOOMFACTOR 3
// Maximum current in chambers (nA) for sliders 
#define MAX_CURRENT_SLIDER 3000
// Number of canvas and tabs
#define NCANVAS 8

// ******************************* HV supply *************************

// Define high voltage supply module to compile corresponding program version
// CAEN_NDT147X, CAEN_N1471 & ISEG are exclusive switches in code
// HVSTITLE is just a text title in GUI panel
// HVMARK is a magic string to identify HV supply connected to virtual COM port

#ifdef WEIZMANN
#define CAEN_NDT147X
#define HVSTITLE "CAEN NDT1471 HV supply"
//#define CAEN_N1471
//#define HVSTITLE "CAEN N1471 HV supply"
#elif defined(CHILE)
#define CAEN_N1471
#define HVSTITLE "CAEN N1470 HV supply"
#elif defined(MEPHI)
#define CAEN_N1471
#define HVSTITLE "CAEN N1471 HV supply"
#elif defined(CANADA)
#define ISEG
#define HVSTITLE "ISEG HV supply"
#elif defined(CHINA)
#define CAEN_N1471
#define HVSTITLE "CAEN N1470 HV supply"
#else
#define CAEN_N1471
#define HVSTITLE "CAEN N1471 HV supply"
#endif

#if defined(CAEN_N1471)
#define HVMARK "FTDIBUS"
#elif defined(CAEN_NDT147X)
#define HVMARK "VID_21E1"
#elif defined(ISEG)
#define HVMARK "VID_0403"
#endif

// Number of channels in HV supply
#define NHVCHAN 4
// Default and maximum HV to set, Volt
#define HVDEFAULT 2900
#define MAXHVSUPPLY 3100

// ******************************* XR tube *************************

// Default XRay voltage and current to set, kiloVolts, microAmps
#define XRVDEFAULT 50
#define XRCURRENTDEFAULT 75
// Maximum XRay voltage and current to set, kiloVolts, microAmps
#define MAXXRAYVOLTAGE 50
#define MAXXRAYCURRENT 200
// Minimum XRay voltage and current to set, kiloVolts, microAmps
#define MINXRAYVOLTAGE 10
#define MINXRAYCURRENT 5

// ******************************* Scanner *************************

// Speed of scanner moving without measurements, mm/sec
#define MOVESPEED 25
// Maximum moving speed, mm/sec
#define MAXMOVESPEED 50
// Stand by time in by-step scanning mode, sec
#define STANDBYTIME 5
// Maximum stand by time in by-step scanning mode, sec
#define MAXSTANDBYTIME 30

// Default size of collimator, mm
#define XCOLLIMATORSIZE 30
#define YCOLLIMATORSIZE 30

// Default area to scan
#define XMAXTOSCAN 200
#define YMAXTOSCAN 100

// Default calibration coefficients to convert internal coordinates to mm
// For black scanner, mm/step
#ifdef WEIZMANN
#define XCALIBRATION 0.03124
#define YCALIBRATION 0.0281
#elif defined(PNPI)
#define XCALIBRATION 0.0625
#define YCALIBRATION 0.056283
#elif defined(MEPHI)
#define XCALIBRATION 0.09380247
#define YCALIBRATION 0.09380247
#elif defined(CHILE)
#define XCALIBRATION 0.06256
#define YCALIBRATION 0.05625
#elif defined(CANADA)
// For white scanner, mm/count
#define XCALIBRATION 0.00244215
#define YCALIBRATION 0.00244215 
// Speed calibration, rpm/mm/sec
#define XSPEEDCALIB 2.9991
#define YSPEEDCALIB 2.9991
#elif defined(CHINA)
#define XCALIBRATION 0.12492
#define YCALIBRATION 0.11122
#else
#define XCALIBRATION 0.09380247
#define YCALIBRATION 0.09380247
#endif

// Permissible time (sec) to hold scanner in one position, if it have to move
#define MAXTIMEHOLD 30

// Information concerning to hercon contacts in BLACK scanners.
// Signals read from fLPTaddress+1
/*
  WEIZMANN
  NO_GERCON=199, GERCON_LEFT=231, GERCON_RIGHT=71, GERCON_UP=207, GERCON_DOWN=215

  PNPI
  NO_GERCON=127, GERCON_LEFT=95, GERCON_RIGHT=255, GERCON_UP=119, GERCON_DOWN=111

  MEPHI
  NO_GERCON=39, GERCON_LEFT=NaN, GERCON_RIGHT=7, GERCON_UP=167, GERCON_DOWN=NaN

CHILE - as for WEIZMANN

CHINA - as for PNPI
*/

// Masks for use in code
enum {
  NO_GERCON=127, 
  GERCON_LEFT=32, GERCON_RIGHT=128, GERCON_UP=8, GERCON_DOWN=16
};

// ******************************* Miscellaneous *************************

// Frequency to read information from hardware devices, msec
#define HVFREQ 1000

// Frequency to update status bar and drawing of histograms, events
#define HISTDRAWFREQ 1

// Maximum time to hold X-ray tube on in the same position, sec
#define MAXTIMEXRAYON 60

// ******************************* Enumeration *************************
enum ScanMode_t {
  CONTINUOUS=0,
  BYSTEP
};

enum BackMoveMode_t {
  SNAKE=0,
  ZIGZAG
};

enum ScanDirection_t {
  HORIZONTAL=0,
  VERTICAL
};

enum HWmode_t {
  REAL_TIME=0,
  SIMULATION
};

enum MoveDirection_t {
  MOVE_LEFT=0, MOVE_RIGHT, MOVE_UP, MOVE_DOWN
};
const string MoveDirectionName[4]={"left","right","up","down"};

enum smode {
  AUTO,
  MANUAL };

// ******************************* Units and convertions **********************
#define SEC2MSEC 1000
#define MSEC2SEC 0.001

class Vparams {
 private:
  Vparams();
  ~Vparams();
  static Vparams* pParams;

 public:
  static Vparams* getParams();
  std::string inputdataname;
  std::string confname;
  std::string inname;
  std::string outname;
  unsigned int nevents;
  unsigned int skipevents;
  bool nogui;
  int verbose;
  std::string version;
  AnalysisConfig *conf;
  std::string fRefFileName;
  int fMapCurrent;
  Float_t ScaleHighLimit;
  Float_t ScaleLowLimit;
  smode ScalingMode;
  FILE* fLog;
};
#endif //VPARAMS_H
