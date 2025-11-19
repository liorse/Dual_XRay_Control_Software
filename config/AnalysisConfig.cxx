#include "stdafx.h"
#include "AnalysisConfig.h"
/**
 *  This code of from_string() is shamelessly copied from www.codeguru.com
 *
 */ 

template <class T>
bool from_string(T& t, 
                 const std::string& s, 
                 std::ios_base& (*f)(std::ios_base&))
{
  std::istringstream iss(s);
  return !(iss >> f >> t).fail();
}
//-----------------------------------------------------------------------------
AnalysisConfig::AnalysisConfig() {
  ftolo = tolower;
}
//-----------------------------------------------------------------------------
AnalysisConfig::~AnalysisConfig(){
  for(ConfigType::iterator it=fConfig.begin();it != fConfig.end();++it) {
    if((*it).second) delete (*it).second;
 //    std::cout<<"DEBUG: Removing parameter '"<<(*it).first<<"'"<<std::endl;
  }
}
//-----------------------------------------------------------------------------
void AnalysisConfig::DumpConfig(std::ostream&os) {
  os<<"### Online Physics Analysis Configuration BEGINS ###"<<std::endl
    <<"###"<<std::endl;
  for(ConfigType::iterator it = fConfig.begin(); it != fConfig.end();++it){
    os<<"# Parameter with name "
      <<((*it).second?(*it).second->ParName:(*it).first)
      <<std::endl;
    //    if((*it).second) os<<(*it).first<<(*it).second->Dump()<<std::endl;
    if((*it).second) os<<(*it).second->ParName<<(*it).second->Dump()
		       <<std::endl;
    else os<<" ###!!!! Bad parameter in Configuration !!!!"<<std::endl;

  }
  os<<"### Online Physics Analysis Configuration  ENDS  ###"<<std::endl;
}
//-----------------------------------------------------------------------------
int AnalysisConfig::SaveConfig(std::string& fname) {
  std::ofstream of(fname.c_str());
  if(!of.is_open()) return -1;
  DumpConfig(of);
  return 0;
}
//-----------------------------------------------------------------------------
int AnalysisConfig::ReadConfig(std::string& fname) {

  fConfig.clear();
  std::ifstream ff(fname.c_str());
  if(!ff.is_open()) {
    std::cerr<<"WARNING: Cannot open config file: "<<fname<<std::endl;
    return -1;
  }
  static const int MAXCHAR_PER_LINE=200;
  char confline[MAXCHAR_PER_LINE];

  while(ff.good()){
    ff.getline(confline,MAXCHAR_PER_LINE);
    std::string line(confline);
    int pos = line.find("#");
    if(pos>-1) line.erase(pos,line.size());
    if(line.size()<=0) continue;

    std::istringstream si(line);
    std::string parname,boolval;

    si>>parname;
    if(si.fail()) continue;
    //    std::cout<<"DEBUG: got parname='"<<parname<<"'"<<std::endl;

    // Transform to lower case
    //    std::transform(parname.begin(),parname.end(),parname.begin(),ftolo);
    int pp = si.str().find('"');
    if(pp>-1) {
      std::string strparam = "";

      int strlen = line.length();
      pp++;
      while(pp<strlen && (line[pp] != '"' || line[pp-1] == '\\')){
	strparam += line[pp];
	pp++;
      }
      SetParameter(parname,new StringParameter(strparam));
    }
    si>>boolval;
    if(si.fail()) continue;

    bool bv=false;
    std::vector<double> nums;

    // Transform to lower case
    std::transform(boolval.begin(),boolval.end(),boolval.begin(),ftolo);

    if(boolval == "on" || boolval == "off") {
      bv = false;
      if(boolval == "on") bv = true;
      SetParameter(parname,new SwitchParameter(bv));
    } else {
      double firstVal;
      bool isVector = false;
      int pos = boolval.find("<");
      if(pos>-1) {
	boolval[pos] = ' ';
	isVector = true;
	pos = si.str().find(">");
	if(pos>-1) si.str()[pos] = ' ';
      }
      if(from_string<double>(firstVal,boolval,std::dec))
	nums.push_back(firstVal);
      /*else 
	continue; */
      while(si.good()){
	double d;
	si>>d;
	if(si.fail()) break;
	nums.push_back(d);
      }
      if(nums.size() == 0) continue;
      
      if(isVector || nums.size()>2)
	SetParameter(parname,new VectorParameter(nums));
      else if(nums.size()==1)
	SetParameter(parname,new LevelParameter(nums[0]));
      else if(nums.size()==2)
	SetParameter(parname,new WindowParameter(nums[0],nums[1]));
    }
  }
  ff.close();
  return 0;
}
//-----------------------------------------------------------------------------
void AnalysisConfig::setBoolSwitch(bool&val,const char*name,bool def,bool readv)
{
  SwitchParameter* sw = GetParameter<SwitchParameter>(name);
  if(!sw)
    sw = SetParameter(name,new SwitchParameter(def));
  if(readv)
    val= sw->IsOn;
  else
    sw->IsOn = val;
}
//-----------------------------------------------------------------------------
void AnalysisConfig::setString(std::string&val,const char*name,const char*def,bool readv)
{
  StringParameter* sw = GetParameter<StringParameter>(name);
  if(!sw)
    sw = SetParameter(name,new StringParameter(def));
  if(readv)
    val= sw->StrVal;
  else
    sw->StrVal = val;  
}

