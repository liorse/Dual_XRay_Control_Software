#pragma once
#ifdef _WIN32
#include <windows.h>
#include <string>

class NamedPipeServer {
public:
    explicit NamedPipeServer(const std::string& pipeName)
        : m_pipeName(pipeName), m_hPipe(INVALID_HANDLE_VALUE) {}
    ~NamedPipeServer() { close(); }

    bool listen() {
        std::string name = m_pipeName.empty() ? std::string("\\\\.\\pipe\\XRayService") : m_pipeName;
        m_hPipe = CreateNamedPipeA(
            name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,             // max instances
            4096, 4096,    // out/in buffer sizes
            0,             // default timeout
            NULL);
        return m_hPipe != INVALID_HANDLE_VALUE;
    }

    bool accept() {
        if (m_hPipe == INVALID_HANDLE_VALUE) return false;
        BOOL connected = ConnectNamedPipe(m_hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        return connected == TRUE;
    }

    bool readLine(std::string& line) {
        if (m_hPipe == INVALID_HANDLE_VALUE) return false;
        char buffer[4096];
        DWORD read = 0;
        if (!ReadFile(m_hPipe, buffer, sizeof(buffer)-1, &read, NULL)) return false;
        buffer[read] = '\0';
        line.assign(buffer, buffer + read);
        // trim CRLF
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        return true;
    }

    bool writeLine(const std::string& line) {
        if (m_hPipe == INVALID_HANDLE_VALUE) return false;
        std::string out = line;
        if (out.empty() || out.back() != '\n') out.push_back('\n');
        DWORD written = 0;
        return WriteFile(m_hPipe, out.data(), (DWORD)out.size(), &written, NULL) == TRUE;
    }

    void close() {
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(m_hPipe);
            DisconnectNamedPipe(m_hPipe);
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
    }

private:
    std::string m_pipeName;
    HANDLE m_hPipe;
};
#endif // _WIN32
