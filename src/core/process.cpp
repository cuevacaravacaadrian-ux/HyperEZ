#include "core/process.h"

#include <algorithm>
#include <chrono>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
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

static std::wstring commandLine(const std::vector<std::string>& argv) {
    std::wstring cmd;
    for (auto& a : argv) {
        if (!cmd.empty()) cmd += L' ';
        cmd += quoteArg(widen(a));
    }
    return cmd;
}

// Bloque de entorno: el actual menos las variables quitadas o redefinidas, más las nuevas.
static std::wstring environmentBlock(const ProcessOptions& opt) {
    std::vector<std::wstring> drop;
    for (auto& n : opt.unsetEnv) drop.push_back(widen(n) + L"=");
    for (auto& kv : opt.env) {
        size_t eq = kv.find('=');
        if (eq != std::string::npos) drop.push_back(widen(kv.substr(0, eq + 1)));
    }
    std::wstring env;
    LPWCH block = GetEnvironmentStringsW();
    for (LPWCH p = block; *p; p += wcslen(p) + 1) {
        bool skip = false;
        for (auto& d : drop) if (_wcsnicmp(p, d.c_str(), d.size()) == 0) skip = true;
        if (skip) continue;
        env += p;
        env += L'\0';
    }
    FreeEnvironmentStringsW(block);
    for (auto& kv : opt.env) { env += widen(kv); env += L'\0'; }
    env += L'\0';
    return env;
}

static void readAll(HANDLE h, std::string& out) {
    char buf[8192];
    DWORD n;
    while (ReadFile(h, buf, sizeof buf, &n, nullptr) && n > 0) out.append(buf, n);
}

static ProcessResult runCommandLine(std::wstring cmd, const ProcessOptions& opt) {
    ProcessResult r;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE outR = nullptr, outW = nullptr, errR = nullptr, errW = nullptr, inR = nullptr, inW = nullptr;
    if (!CreatePipe(&outR, &outW, &sa, 0)) return r;
    SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
    if (!opt.mergeStderr) {
        CreatePipe(&errR, &errW, &sa, 0);
        SetHandleInformation(errR, HANDLE_FLAG_INHERIT, 0);
    }
    CreatePipe(&inR, &inW, &sa, 0);
    SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = outW;
    si.hStdError = opt.mergeStderr ? outW : errW;
    si.hStdInput = inR;
    std::wstring env;
    bool customEnv = !opt.env.empty() || !opt.unsetEnv.empty();
    if (customEnv) env = environmentBlock(opt);
    std::wstring wcwd = widen(opt.cwd);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                             customEnv ? env.data() : nullptr, wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
    CloseHandle(outW);
    if (errW) CloseHandle(errW);
    CloseHandle(inR);
    if (!ok) {
        CloseHandle(outR);
        if (errR) CloseHandle(errR);
        CloseHandle(inW);
        return r;
    }
    r.started = true;
    std::thread to([&] { readAll(outR, r.out); });
    std::thread te([&] { if (errR) readAll(errR, r.err); });
    std::thread ti([&] {
        size_t off = 0;
        DWORD w;
        while (off < opt.input.size() &&
               WriteFile(inW, opt.input.data() + off, (DWORD)std::min<size_t>(opt.input.size() - off, 1 << 16), &w, nullptr))
            off += w;
        CloseHandle(inW);
    });
    DWORD wr = WaitForSingleObject(pi.hProcess, opt.timeoutMs >= 0 ? (DWORD)opt.timeoutMs : INFINITE);
    if (wr == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        WaitForSingleObject(pi.hProcess, 2000);
        r.timedOut = true;
    }
    DWORD ec = 0;
    GetExitCodeProcess(pi.hProcess, &ec);
    r.code = r.timedOut ? -1 : (int)ec;
    ti.join();
    to.join();
    te.join();
    CloseHandle(outR);
    if (errR) CloseHandle(errR);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return r;
}

ProcessResult runCapture(const std::vector<std::string>& argv, const ProcessOptions& opt) {
    if (argv.empty()) return {};
    return runCommandLine(commandLine(argv), opt);
}

ProcessResult runShell(const std::string& command, const ProcessOptions& opt) {
    // /s: cmd quita las comillas exteriores y respeta el resto de la línea tal cual
    return runCommandLine(L"cmd.exe /d /s /c \"" + widen(command) + L"\"", opt);
}

void* spawnProcess(const std::vector<std::string>& argv) {
    std::wstring cmd = commandLine(argv);
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

bool openWithSystem(const std::string& target) {
    return (INT_PTR)ShellExecuteW(nullptr, L"open", widen(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}

int currentProcessId() { return (int)GetCurrentProcessId(); }

#else // POSIX

static void ignoreSigpipe() {
    static bool done = false;
    if (!done) { std::signal(SIGPIPE, SIG_IGN); done = true; }
}

static void applyChildOptions(const ProcessOptions& opt) {
    if (!opt.cwd.empty() && chdir(opt.cwd.c_str()) != 0) _exit(126);
    for (auto& n : opt.unsetEnv) unsetenv(n.c_str());
    for (auto& kv : opt.env) {
        size_t eq = kv.find('=');
        if (eq != std::string::npos) setenv(kv.substr(0, eq).c_str(), kv.c_str() + eq + 1, 1);
    }
}

static std::vector<char*> cArgs(const std::vector<std::string>& argv) {
    std::vector<char*> a;
    for (auto& s : argv) a.push_back(const_cast<char*>(s.c_str()));
    a.push_back(nullptr);
    return a;
}

ProcessResult runCapture(const std::vector<std::string>& argv, const ProcessOptions& opt) {
    ProcessResult r;
    if (argv.empty()) return r;
    ignoreSigpipe();
    int outp[2], errp[2], inp[2], failp[2];
    if (pipe(outp) || pipe(errp) || pipe(inp) || pipe(failp)) return r;
    fcntl(failp[1], F_SETFD, FD_CLOEXEC); // se cierra solo si exec funciona
    auto args = cArgs(argv);
    pid_t pid = fork();
    if (pid < 0) return r;
    if (pid == 0) {
        dup2(inp[0], 0);
        dup2(outp[1], 1);
        dup2(opt.mergeStderr ? outp[1] : errp[1], 2);
        for (int fd : {outp[0], outp[1], errp[0], errp[1], inp[0], inp[1], failp[0]}) close(fd);
        applyChildOptions(opt);
        execvp(args[0], args.data());
        int e = errno;
        ssize_t ignored = write(failp[1], &e, sizeof e);
        (void)ignored;
        _exit(127);
    }
    for (int fd : {outp[1], errp[1], inp[0], failp[1]}) close(fd);
    int execErr = 0;
    bool execFailed = read(failp[0], &execErr, sizeof execErr) == (ssize_t)sizeof execErr;
    close(failp[0]);
    r.started = !execFailed;

    size_t inOff = 0;
    int inFd = inp[1];
    if (opt.input.empty()) { close(inFd); inFd = -1; }
    else fcntl(inFd, F_SETFL, fcntl(inFd, F_GETFL) | O_NONBLOCK);
    int outFd = outp[0], errFd = errp[0];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(opt.timeoutMs);
    char buf[8192];
    while (outFd >= 0 || errFd >= 0) {
        pollfd fds[3];
        int n = 0;
        if (outFd >= 0) fds[n++] = {outFd, POLLIN, 0};
        if (errFd >= 0) fds[n++] = {errFd, POLLIN, 0};
        if (inFd >= 0) fds[n++] = {inFd, POLLOUT, 0};
        int wait = -1;
        if (opt.timeoutMs >= 0) {
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) { kill(pid, SIGKILL); r.timedOut = true; break; }
            wait = (int)left;
        }
        if (poll(fds, n, wait) < 0 && errno != EINTR) break;
        for (int i = 0; i < n; i++) {
            if (!fds[i].revents) continue;
            if (fds[i].fd == inFd) {
                ssize_t w = write(inFd, opt.input.data() + inOff, opt.input.size() - inOff);
                if (w > 0) inOff += (size_t)w;
                if (w < 0 || inOff >= opt.input.size()) { close(inFd); inFd = -1; }
                continue;
            }
            ssize_t got = read(fds[i].fd, buf, sizeof buf);
            if (got > 0) {
                (fds[i].fd == outFd ? r.out : r.err).append(buf, (size_t)got);
            } else {
                close(fds[i].fd);
                if (fds[i].fd == outFd) outFd = -1; else errFd = -1;
            }
        }
    }
    for (int fd : {outFd, errFd, inFd}) if (fd >= 0) close(fd);
    int st = 0;
    waitpid(pid, &st, 0);
    if (r.timedOut) r.code = -1;
    else r.code = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
    return r;
}

ProcessResult runShell(const std::string& command, const ProcessOptions& opt) {
    return runCapture({"/bin/sh", "-c", command}, opt);
}

void* spawnProcess(const std::vector<std::string>& argv) {
    if (argv.empty()) return nullptr;
    auto args = cArgs(argv);
    pid_t pid = fork();
    if (pid < 0) return nullptr;
    if (pid == 0) {
        execvp(args[0], args.data());
        _exit(127);
    }
    return reinterpret_cast<void*>(static_cast<intptr_t>(pid));
}

bool processFinished(void* p, int waitMs, int& exitCode) {
    pid_t pid = (pid_t) reinterpret_cast<intptr_t>(p);
    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs < 0 ? 0 : waitMs);
    for (;;) {
        int st = 0;
        pid_t w = waitpid(pid, &st, waitMs < 0 ? 0 : WNOHANG);
        if (w == pid) {
            exitCode = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
            return true;
        }
        if (w < 0 || std::chrono::steady_clock::now() >= until) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void endProcess(void* p, bool killIt) {
    if (!p) return;
    pid_t pid = (pid_t) reinterpret_cast<intptr_t>(p);
    if (killIt) {
        kill(pid, SIGTERM);
        int code;
        if (!processFinished(p, 2000, code)) { kill(pid, SIGKILL); waitpid(pid, nullptr, 0); }
    }
}

bool openWithSystem(const std::string& target) {
#ifdef __APPLE__
    return runCapture({"open", target}).code == 0;
#else
    return runCapture({"xdg-open", target}).code == 0;
#endif
}

int currentProcessId() { return (int)getpid(); }

#endif

} // namespace hz
