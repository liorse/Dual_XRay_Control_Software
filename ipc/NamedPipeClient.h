#pragma once

#ifdef _WIN32
#include <windows.h>
#include <string>

class NamedPipeClient {
public:
    explicit NamedPipeClient(const std::string& pipeName)
        : m_pipeName(pipeName), m_hPipe(INVALID_HANDLE_VALUE) {}

    ~NamedPipeClient() {
        disconnect();
    }

    bool connect(unsigned long timeoutMs = 5000) {
        std::string name = m_pipeName.empty() ? std::string("\\\\.\\pipe\\XRayService") : m_pipeName;
        DWORD waited = 0;
        const DWORD step = 100;
        while (waited < timeoutMs) {
            m_hPipe = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (m_hPipe != INVALID_HANDLE_VALUE) {
                DWORD mode = PIPE_READMODE_MESSAGE;
                SetNamedPipeHandleState(m_hPipe, &mode, NULL, NULL);
                return true;
            }
            if (GetLastError() != ERROR_PIPE_BUSY) break;
            if (!WaitNamedPipeA(name.c_str(), step)) break;
            waited += step;
        }
        return false;
    }

    void disconnect() {
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
    }

    bool call(const std::string& requestLine, std::string& responseLine) {
        if (m_hPipe == INVALID_HANDLE_VALUE) return false;
        std::string line = requestLine;
        if (line.empty() || line.back() != '\n') line.push_back('\n');
        DWORD written = 0;
        if (!WriteFile(m_hPipe, line.data(), (DWORD)line.size(), &written, NULL)) return false;
        // Read a single message (server replies per-request)
        char buffer[4096];
        DWORD read = 0;
        if (!ReadFile(m_hPipe, buffer, sizeof(buffer)-1, &read, NULL)) return false;
        buffer[read] = '\0';
        responseLine.assign(buffer, buffer + read);
        // Trim trailing newlines\r\n
        while (!responseLine.empty() && (responseLine.back() == '\n' || responseLine.back() == '\r')) responseLine.pop_back();
        return true;
    }

private:
    std::string m_pipeName;
    HANDLE m_hPipe;
};

#endif // _WIN32
