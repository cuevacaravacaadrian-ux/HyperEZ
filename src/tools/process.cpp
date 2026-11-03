// Ejecución de subprocesos con salida capturada (usada por 'hyper test').
#include "tools.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <thread>
#else
#include <cstdio>
#include <sys/wait.h>
#endif

namespace hz {

#ifdef _WIN32
static std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// Reglas de comillas de CommandLineToArgvW.
static std::wstring quoteArg(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring r = L"\"";
    int slashes = 0;
    for (wchar_t c : a) {
        if (c == L'\\') { slashes++; continue; }
        if (c == L'"') r.append(slashes * 2 + 1, L'\\');
        else r.append(slashes, L'\\');
        slashes = 0;
        r += c;
    }
    r.append(slashes * 2, L'\\');
    return r + L"\"";
}

int runProcess(const std::vector<std::string>& argv, const std::string& cwd, std::string& output, int timeoutMs) {
    output.clear();
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -2;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = nul;
    std::wstring cmd;
    for (auto& a : argv) { if (!cmd.empty()) cmd += L' '; cmd += quoteArg(widen(a)); }
    // Sin colores en la salida capturada
    std::wstring env;
    {
        LPWCH block = GetEnvironmentStringsW();
        for (LPWCH p = block; *p; p += wcslen(p) + 1) {
            if (_wcsnicmp(p, L"HYPEREZ_COLOR=", 14) == 0) continue;
            env += p;
            env += L'\0';
        }
        FreeEnvironmentStringsW(block);
        env += L"NO_COLOR=1";
        env += L'\0';
        env += L'\0';
    }
    PROCESS_INFORMATION pi{};
    std::wstring wcwd = widen(cwd);
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, env.data(),
                             wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(rd); return -2; }
    std::thread reader([&] {
        char buf[4096];
        DWORD n;
        while (ReadFile(rd, buf, sizeof buf, &n, nullptr) && n > 0) output.append(buf, n);
    });
    DWORD w = WaitForSingleObject(pi.hProcess, timeoutMs > 0 ? (DWORD)timeoutMs : INFINITE);
    int code;
    if (w == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        WaitForSingleObject(pi.hProcess, 2000);
        code = -1;
    } else {
        DWORD ec = 0;
        GetExitCodeProcess(pi.hProcess, &ec);
        code = (int)ec;
    }
    reader.join();
    CloseHandle(rd);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

void* spawnProcess(const std::vector<std::string>& argv) {
    std::wstring cmd;
    for (auto& a : argv) { if (!cmd.empty()) cmd += L' '; cmd += quoteArg(widen(a)); }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return nullptr;
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

bool processFinished(void* p, int waitMs, int& exitCode) {
    if (WaitForSingleObject((HANDLE)p, waitMs < 0 ? INFINITE : (DWORD)waitMs) != WAIT_OBJECT_0) return false;
    DWORD ec = 0;
    GetExitCodeProcess((HANDLE)p, &ec);
    exitCode = (int)ec;
    return true;
}

void endProcess(void* p, bool kill) {
    if (!p) return;
    if (kill) {
        TerminateProcess((HANDLE)p, 130);
        WaitForSingleObject((HANDLE)p, 2000);
    }
    CloseHandle((HANDLE)p);
}
#else
int runProcess(const std::vector<std::string>& argv, const std::string& cwd, std::string& output, int timeoutMs) {
    (void)timeoutMs;
    std::string cmd = "cd '" + cwd + "' && NO_COLOR=1";
    for (auto& a : argv) cmd += " '" + a + "'";
    cmd += " 2>&1 </dev/null";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return -2;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) output.append(buf, n);
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
#endif

} // namespace hz
