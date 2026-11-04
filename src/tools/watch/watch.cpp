// hyper watch — ejecuta un programa y lo vuelve a ejecutar cada vez que se guarda un archivo .hz
// de su carpeta (o de sus subcarpetas).
#include "tools/tools.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <map>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace hz {

namespace fs = std::filesystem;
namespace {

using Snapshot = std::map<fs::path::string_type, fs::file_time_type>;

// Fecha de modificación de cada archivo .hz/.hez (sin entrar en carpetas ocultas, de paquetes o de compilación).
Snapshot scan(const fs::path& dir) {
    Snapshot out;
    std::error_code ec;
    int visited = 0;
    for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (++visited > 50000) break; // carpeta enorme: no seguir
        std::string name = it->path().filename().string();
        if (it->is_directory(ec)) {
            if (!name.empty() && (name[0] == '.' || name == "node_modules" || name == "hz_packages" || name.rfind("build", 0) == 0))
                it.disable_recursion_pending();
            continue;
        }
        std::string ext = it->path().extension().string();
        if (ext == ".hz" || ext == ".hez") out[it->path().native()] = it->last_write_time(ec);
    }
    return out;
}

std::string timeOfDay() {
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%H:%M:%S", std::localtime(&t));
    return buf;
}

// Espera hasta que cambie un archivo o termine el proceso (lo que antes ocurra, como mucho ms).
struct Watcher {
#ifdef _WIN32
    HANDLE change = INVALID_HANDLE_VALUE;
    explicit Watcher(const fs::path& dir) {
        change = FindFirstChangeNotificationW(dir.c_str(), TRUE,
                                              FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
    }
    ~Watcher() { if (change != INVALID_HANDLE_VALUE) FindCloseChangeNotification(change); }
    // true si puede haber cambios en los archivos
    bool wait(void* child, bool& childEnded, int& code) {
        childEnded = false;
        if (change == INVALID_HANDLE_VALUE) { // p. ej. unidades de red: sondeo
            if (child && processFinished(child, 400, code)) childEnded = true;
            else if (!child) Sleep(400);
            return true;
        }
        HANDLE hs[2] = {change, (HANDLE)child};
        DWORD w = WaitForMultipleObjects(child ? 2 : 1, hs, FALSE, INFINITE);
        if (w == WAIT_OBJECT_0 + 1) { childEnded = processFinished(child, 0, code); return false; }
        FindNextChangeNotification(change);
        Sleep(100); // deja que el editor termine de guardar
        return true;
    }
#else
    explicit Watcher(const fs::path&) {}
    bool wait(void* child, bool& childEnded, int& code) {
        childEnded = child && processFinished(child, 300, code);
        if (!child) std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return true;
    }
#endif
};

} // namespace

int runWatch(const std::string& file, const std::vector<std::string>& args) {
    std::string abs = pathAbsolute(file);
    fs::path dir = fs::path((const char8_t*)pathDir(abs).c_str());
    std::vector<std::string> argv{exePath(), "run", abs};
    if (!args.empty()) {
        argv.push_back("--");
        argv.insert(argv.end(), args.begin(), args.end());
    }
    Watcher watcher(dir);
    Snapshot snap = scan(dir);
    for (;;) {
        std::printf("\x1b[2J\x1b[3J\x1b[H%s▶ %s%s %s· %s · guarda cualquier .hz para volver a ejecutar · Ctrl+C para salir%s\n\n",
                    color::cyan(), pathFilename(abs).c_str(), color::reset(), color::gray(), timeOfDay().c_str(), color::reset());
        std::fflush(stdout);
        double t0 = nowSeconds();
        void* child = spawnProcess(argv);
        if (!child) {
            std::fprintf(stderr, "%serror:%s no se pudo ejecutar hyper\n", color::red(), color::reset());
            return 1;
        }
        bool changed = false;
        while (!changed) {
            bool ended = false;
            int code = 0;
            bool maybeChanged = watcher.wait(child, ended, code);
            if (ended) {
                endProcess(child, false);
                child = nullptr;
                std::fflush(stdout);
                if (code == 0)
                    std::printf("\n%s── ✓ terminó en %.2f s · esperando cambios...%s\n", color::gray(), nowSeconds() - t0, color::reset());
                else
                    std::printf("\n%s── ✗ terminó con errores · corrige y guarda para volver a ejecutar...%s\n", color::red(), color::reset());
                std::fflush(stdout);
            }
            if (!maybeChanged) continue;
            Snapshot now = scan(dir);
            if (now != snap) {
                snap = std::move(now);
                changed = true;
            }
        }
        endProcess(child, true);
    }
}

} // namespace hz
