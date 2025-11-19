#include "stdafx.h"
#include "Vparams.h"
#include <TTimeStamp.h>

Vparams* Vparams::pParams(0);
//-----------------------------------------------------------------------------
Vparams::Vparams():
  inputdataname(""),confname("qsv.conf"),inname("qsv.root"),
  outname("qsv.root"),nevents(0),skipevents(0),
  nogui(false),verbose(0),
  conf(new AnalysisConfig()),fMapCurrent(1),ScaleHighLimit(MAX_CURRENT_SLIDER),
  ScaleLowLimit(0),ScalingMode(AUTO)
{
#ifdef _WIN32
  TTimeStamp myTime;
  string dts=myTime.AsString("s"); // Format is: "YYYY-MM-DD HH:MM:SS"
  dts.replace(10,1,"_");  dts.replace(13,1,"-");
  string LogFileName="qscanner_log."+dts.substr(0,16)+".txt";
  printf("Open log file %s\n",LogFileName.c_str());
  fLog = fopen(LogFileName.c_str(), "w");
#endif
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Vparams::~Vparams()
{
#ifdef _WIN32
  if(fLog) {
    fclose(fLog);
    fLog = 0;
  }
#endif
  if(conf) {
    delete conf;
    conf = 0;
  }
}
//-----------------------------------------------------------------------------
Vparams* Vparams::getParams()
{
  if(pParams == 0) pParams = new Vparams;
  return pParams;
}
