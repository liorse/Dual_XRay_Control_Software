#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdio>
#include <string>
#include "ipc/NamedPipeClient.h"

// Simple standalone sample demonstrating how another process (GUI) can
// connect to the XRayService (implemented in main.cxx) and activate the MiniX.
// Build separately or integrate into your GUI project.
// Steps:
// 1. Ensure XRayService process is running (launch compiled main.exe)
// 2. Run this sample; it will INIT, enumerate devices, select first, set HV & current, power ON, then read status.
// 3. Power OFF before exit.

static bool send(NamedPipeClient &cli, const std::string &req, std::string &resp) {
    if (!cli.call(req, resp)) { std::printf("Request failed: %s\n", req.c_str()); return false; }
    std::printf("REQ: %s\nRESP: %s\n", req.c_str(), resp.c_str());
    return true;
}

int main() {
#ifndef _WIN32
    std::fprintf(stderr, "Windows only sample.\n");
    return 1;
#else
    const char *pipeEnv = std::getenv("XRAY_PIPE_NAME");
    std::string pipeName = pipeEnv && pipeEnv[0] ? pipeEnv : "\\\\.\\pipe\\XRayService";

    NamedPipeClient client(pipeName);
    if (!client.connect(3000)) {
        std::fprintf(stderr, "Could not connect to pipe %s\n", pipeName.c_str());
        return 2;
    }

    std::string resp;

    // 1. INIT (optionally pass a serial number you want to bind to)
    send(client, "INIT|", resp); // empty serial => auto-select first device

    // 2. Enumerate devices
    send(client, "GET_DEVICE_LIST", resp);
    send(client, "GET_DEVICE_COUNT", resp);

    // 3. Optionally query first device serial number
    send(client, "GET_DEVICE_SERIAL|0", resp);

    // 4. Select device index 0
    send(client, "SET_DEVICE|0", resp);

    // 5. Set desired HV (kV) and current (uA) and apply together
    // Option A: single combined command path
    send(client, "SET_VOLTAGE|40.0", resp);   // 40 kV example
    send(client, "SET_CURRENT|200.0", resp);  // 200 uA example
    send(client, "SET_HV_I", resp);           // commits HV+Current (if enabled)

    // 6. Power ON (this triggers SetXRayState on server side which also performs HVOn logic)
    // Format: SET_POWER|<on/off>|<VoltageToSet>|<CurrentToSet>
    send(client, "SET_POWER|1|40.0|200.0", resp);

    // 7. Read current state metrics
    send(client, "GET_STATE", resp);
    send(client, "READ_DATA", resp);

    // 8. Print status (server side will log details)
    send(client, "PRINT_STATUS", resp);

    // 9. Power OFF before exit
    send(client, "SET_POWER|0|40.0|200.0", resp);

    // 10. Shutdown service (optional - normally service keeps running)
    // send(client, "SHUTDOWN", resp);

    client.disconnect();
    return 0;
#endif
}
