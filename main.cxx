#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "ipc/NamedPipeServer.h"
#include "hwdrivers/XRay.h"

static void split(const std::string& s, char delim, std::vector<std::string>& out) {
  out.clear();
  size_t start = 0;
  while (start <= s.size()) {
    size_t pos = s.find(delim, start);
    if (pos == std::string::npos) { out.push_back(s.substr(start)); break; }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
}

int main() {
#ifndef _WIN32
  std::fprintf(stderr, "XRayService supported only on Windows.\n");
  return 1;
#else
  const char* pipeEnv = std::getenv("XRAY_PIPE_NAME");
  std::string pipeName = pipeEnv && pipeEnv[0] ? std::string(pipeEnv) : std::string("\\\\.\\pipe\\XRayService");
  NamedPipeServer server(pipeName);
  if (!server.listen()) {
    std::fprintf(stderr, "Failed to create named pipe: %s\n", pipeName.c_str());
    return 2;
  }
  std::printf("XRayService listening on %s\n", pipeName.c_str());
  if (!server.accept()) {
    std::fprintf(stderr, "Failed to accept named pipe client.\n");
    return 3;
  }

  XRay* xr = nullptr;
  bool running = true;
  while (running) {
    std::string line;
    if (!server.readLine(line)) break;
    std::vector<std::string> tok; split(line, '|', tok);
    if (tok.empty()) { server.writeLine("ERR|empty"); continue; }
    const std::string& cmd = tok[0];

    if (cmd == "INIT") {
      if (xr) { delete xr; xr = nullptr; }
      const char* sn = (tok.size() >= 2 && !tok[1].empty()) ? tok[1].c_str() : NULL;
      xr = new XRay(sn);
      server.writeLine("OK");
    } else if (cmd == "SET_POWER") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      int p = (tok.size() >= 2) ? std::atoi(tok[1].c_str()) : 0;
      if (tok.size() >= 3) xr->SetXRayVoltage((Float_t)std::atof(tok[2].c_str()));
      if (tok.size() >= 4) xr->SetXRayCurrent((Float_t)std::atof(tok[3].c_str()));
      xr->SetXRayState(p ? kTRUE : kFALSE);
      server.writeLine("OK");
    } else if (cmd == "SET_VOLTAGE") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      if (tok.size() >= 2) xr->SetXRayVoltage((Float_t)std::atof(tok[1].c_str()));
      server.writeLine("OK");
    } else if (cmd == "SET_CURRENT") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      if (tok.size() >= 2) xr->SetXRayCurrent((Float_t)std::atof(tok[1].c_str()));
      server.writeLine("OK");
    } else if (cmd == "SET_HV_I") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      xr->SetXRayHVAndCurrent();
      server.writeLine("OK");
    } else if (cmd == "READ_DATA") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      xr->ReadXRayData();
      XRay::XRayState st = xr->GetXRayState();
      char buf[256];
      std::snprintf(buf, sizeof(buf), "OK|%d|%f|%f|%f|%f|%f|%f", st.Power ? 1 : 0, st.VoltageToSet, st.ActualVoltage, st.CurrentToSet, st.ActualCurrent, st.ActualPower, st.Temperature);
      server.writeLine(buf);
    } else if (cmd == "GET_STATE") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      XRay::XRayState st = xr->GetXRayState();
      char buf[256];
      std::snprintf(buf, sizeof(buf), "OK|%d|%f|%f|%f|%f|%f|%f", st.Power ? 1 : 0, st.VoltageToSet, st.ActualVoltage, st.CurrentToSet, st.ActualCurrent, st.ActualPower, st.Temperature);
      server.writeLine(buf);
    } else if (cmd == "PRINT_STATUS") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      xr->PrintStatus();
      server.writeLine("OK");
    } else if (cmd == "GET_DEVICE_LIST") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      xr->GetDeviceList();
      server.writeLine("OK");
    } else if (cmd == "GET_DEVICE_COUNT") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      long c = xr->GetDeviceCount();
      char buf[64]; std::snprintf(buf, sizeof(buf), "OK|%ld", c);
      server.writeLine(buf);
    } else if (cmd == "GET_DEVICE_SERIAL") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      long idx = (tok.size() >= 2) ? std::atol(tok[1].c_str()) : 0;
      char serial[256] = {0};
      long ret = xr->GetDeviceSerialNumberByIndex(idx, serial);
      char buf[320]; std::snprintf(buf, sizeof(buf), "OK|%ld|%s", ret, serial);
      server.writeLine(buf);
    } else if (cmd == "SET_DEVICE") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      long idx = (tok.size() >= 2) ? std::atol(tok[1].c_str()) : 0;
      xr->SetDevice(idx);
      server.writeLine("OK");
    } else if (cmd == "EXEC") {
      if (!xr) { server.writeLine("ERR|noinst"); continue; }
      std::string arg = (tok.size() >= 2) ? tok[1] : std::string();
      std::string res;
      xr->ExecCommand(arg, &res);
      server.writeLine(std::string("OK|") + res);
    } else if (cmd == "SHUTDOWN") {
      server.writeLine("OK");
      running = false;
    } else {
      server.writeLine("ERR|unknown");
    }
  }

  if (xr) delete xr;
  server.close();
  return 0;
#endif
}
