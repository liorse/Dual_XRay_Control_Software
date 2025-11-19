#ifndef ANALYSISCONFIG_H
#define ANALYSISCONFIG_H
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <cctype>
#include <iostream>
#include <algorithm>

using namespace std;

/**
 *  @brief Abstract class to be saved in hash-map
 *
 */
struct Parameter { 
  int SectorNum;
  int ssize;
  string ParName;
  Parameter(int sn=0):SectorNum(sn) { ssize = sizeof(Parameter);};
  virtual string Dump() { 
    ostringstream os;
    os<<"\t"<<SectorNum;
    return os.str();
  };
  virtual ~Parameter() {};
};

struct StringParameter: public Parameter {
  string StrVal;
  StringParameter(const string& str):Parameter(),StrVal(str){}
  virtual string Dump(){
    ostringstream os;
    os<<"\t\""<<StrVal<<"\""; 
    return os.str();
  }
  virtual ~StringParameter(){};
};
struct WindowParameter: public Parameter {
  double LowLimit;
  double HighLimit;
  WindowParameter(double ll=0,double hl=0,int sn=0):LowLimit(ll),
       HighLimit(hl){ SectorNum = sn; ssize = sizeof(WindowParameter);}
  virtual string Dump() { 
    ostringstream os;
    os<<"\t"<<LowLimit<<"\t"<<HighLimit;
    return os.str();
  };
};

struct LevelParameter: public Parameter {
  double Level;
  LevelParameter(double l=0,int sn=0):Level(l){SectorNum=sn;
					   ssize = sizeof(LevelParameter);}
  virtual string Dump() { 
    ostringstream os;
    os<<"\t"<<Level;
    return os.str();
  };
};

struct SwitchParameter: public Parameter {
  bool IsOn;
  SwitchParameter(bool b=true,int sn=0):IsOn(b){SectorNum=sn;
					ssize = sizeof(SwitchParameter);}
  virtual string Dump() { 
    ostringstream os;
    os<<"\t"<<(IsOn?"On":"Off");
    return os.str();
  };
};

struct VectorParameter: public Parameter{
  vector<double> Vector;

  VectorParameter(int sn=0):Parameter(sn) {
    ssize = sizeof(VectorParameter);}
  VectorParameter(vector<double>&v,int sn=0):Vector(v){SectorNum=sn;
      ssize = sizeof(VectorParameter);}
  virtual string Dump() { 
    ostringstream os;
    if(Vector.size() == 0) return os.str();
    os<<"\t<"<<Vector[0];
    for(unsigned int n=1;n<Vector.size();n++ )
      os<<" "<<Vector[n];
    os<<">";
    return os.str();
  };
  virtual ~VectorParameter() {}
};

class AnalysisConfig {
  typedef map<string,Parameter*> ConfigType;
 public:

  AnalysisConfig();
  ~AnalysisConfig();
  int ReadConfig(string&);
  void DumpConfig(ostream& os=cout);
  int SaveConfig(string&);
  void setBoolSwitch(bool&,const char*,bool,bool);
  void setString(string&,const char*,const char*,bool);
  template <typename T> T* GetParameter(const char*); 
  template <typename T> T* GetParameter(string&);

  template <typename T> T* SetParameter(string&,T*);
  template <typename T> T* SetParameter(const char*,T*);
 private:
  ConfigType fConfig;
  int (*ftolo)(int);
};

template <typename T> 
T* AnalysisConfig::SetParameter(const char*parname,T* par) {
  string str(parname);
  return SetParameter(str,par);
}

template <typename T> 
T* AnalysisConfig::SetParameter(string&parname,T* par) {
  string parn(parname);
  par->ParName = parn;
  std::transform(parn.begin(),parn.end(),parn.begin(),ftolo);
  fConfig[parn] = par;
  return par;
}

template <typename T> 
T* AnalysisConfig::GetParameter(const char*par) {
  string st(par);
  return GetParameter<T>(st);
}

template <typename T> 
T* AnalysisConfig::GetParameter(string& par){
  string parn(par);
  transform(parn.begin(),parn.end(),parn.begin(),ftolo);
  ConfigType::iterator it = fConfig.find(parn);
  if(fConfig.end() == it || (*it).second == 0) return 0;
  return dynamic_cast<T*>((*it).second);
}

#endif // ANALYSISCONFIG_H
