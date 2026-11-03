// hyper watch — ejecuta un programa y lo vuelve a ejecutar cada vez que se guarda un archivo .hz
// de su carpeta (o de sus subcarpetas).
#include "tools.h"

#include <ctime>
#include <filesystem>
#include <map>

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

#ifdef _WIN32
namespace fs = std::filesystem;
namespace {

using Snapshot = std::map<std::wstring, fs::file_time_type>;

// Fecha de modificación de cada archivo .hz/.hez (sin entrar en carpetas ocultas, de paquetes o de compilación).
Snapshot scan(const fs::path& dir) {
    Snapshot out;
    std::error_code ec;
    int visited = 0;
    for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (++visited > 50000) break; // carpeta enorme: no seguir
        std::wstring name = it->path().filename().wstring();
        if (it->is_directory(ec)) {
            if (!name.empty() && (name[0] == L'.' || name == L"node_modules" || name == L"hz_packages" || name == L"build"))
                it.disable_recursion_pending();
            continue;
        }
        std::wstring ext = it->path().extension().wstring();
        if (ext == L".hz" || ext == L".hez") out[it->path().wstring()] = it->last_write_time(ec);
    }
    return out;
}

std::string timeOfDay() {
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%H:%M:%S", std::localtime(&t));
    return buf;
}

} // namespace

int runWatch(const std::string& file, const std::vector<std::string>& args) {
    std::string abs = pathAbsolute(file);
    fs::path dir = fs::path((const char8_t*)pathDir(abs).c_str());
    std::vector<std::string> argv{exePath(), "run", abs};
    if (!args.empty()) {
        argv.push_back("--");
        argv.insert(argv.end(), args.begin(), args.end());
    }
    HANDLE change = FindFirstChangeNotificationW(dir.c_str(), TRUE,
                                                 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
    bool polling = change == INVALID_HANDLE_VALUE; // p. ej. en unidades de red
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
            HANDLE hs[2] = {child ? (HANDLE)child : nullptr, change};
            DWORD w;
            if (polling) {
                if (child) w = WaitForSingleObject(child, 400) == WAIT_OBJECT_0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
                else { Sleep(400); w = WAIT_TIMEOUT; }
            } else if (child) w = WaitForMultipleObjects(2, hs, FALSE, INFINITE);
            else w = WaitForSingleObject(change, INFINITE) == WAIT_OBJECT_0 ? WAIT_OBJECT_0 + 1 : WAIT_FAILED;
            if (w == WAIT_FAILED) { endProcess(child, true); return 1; }
            if (child && w == WAIT_OBJECT_0) {
                int code = 0;
                processFinished(child, 0, code);
                endProcess(child, false);
                child = nullptr;
                std::fflush(stdout);
                if (code == 0)
                    std::printf("\n%s── ✓ terminó en %.2f s · esperando cambios...%s\n", color::gray(), nowSeconds() - t0, color::reset());
                else
                    std::printf("\n%s── ✗ terminó con errores · corrige y guarda para volver a ejecutar...%s\n", color::red(), color::reset());
                std::fflush(stdout);
                continue;
            }
            if (!polling) {
                FindNextChangeNotification(change);
                Sleep(100); // deja que el editor termine de guardar
            }
            Snapshot now = scan(dir);
            if (now != snap) {
                snap = std::move(now);
                changed = true;
            }
        }
        endProcess(child, true);
    }
}
#else
int runWatch(const std::string&, const std::vector<std::string>&) {
    std::fprintf(stderr, "hyper watch solo está disponible en Windows por ahora\n");
    return 2;
}
#endif

} // namespace hz
