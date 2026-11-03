// Instalador de HyperEZ para Windows (por usuario: no necesita administrador).
//
//   HyperEZ-Setup.exe                 Asistente gráfico
//   HyperEZ-Setup.exe /S              Instalación silenciosa
//   Opciones: /D=<carpeta> /NOASSOC /NOPATH /NOSHORTCUTS /NOVSCODE
//             /VSCODE-EXTDIR=<carpeta>   (instala la extensión en otra carpeta de extensiones)
//             /REGROOT=<clave>           (pruebas: escribe el registro bajo HKCU\<clave>)
//   uninstall.exe /uninstall [/S]     Desinstalación
//
// Qué hace: copia bin\hyper.exe, ejemplos, documentación e iconos en %LOCALAPPDATA%\Programs\HyperEZ,
// asocia .hz/.hez (icono, doble clic = ejecutar, Editar, Comprobar), añade bin al PATH del usuario,
// crea accesos directos, instala la extensión de VS Code y se registra en "Aplicaciones instaladas".
#include "../../src/tools/winshell.h"
#include "../../src/core/version.h"

#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace hz::win;

// ---------------------------------------------------------------------------
// Opciones
// ---------------------------------------------------------------------------

struct Options {
    bool silent = false, uninstall = false;
    bool assoc = true, path = true, shortcuts = true, vscode = true;
    std::wstring dir;
    std::wstring regRoot;      // vacío = ubicaciones reales
    std::wstring vscodeExtDir; // vacío = carpeta normal de extensiones
    std::wstring fromTemp;     // desinstalador relanzado desde %TEMP%: ruta original
    DWORD parentPid = 0;
};

static Options g_opt;
static HINSTANCE g_inst;

static std::wstring classesKey() { return g_opt.regRoot.empty() ? L"Software\\Classes" : g_opt.regRoot + L"\\Classes"; }
static std::wstring envKey() { return g_opt.regRoot.empty() ? L"Environment" : g_opt.regRoot + L"\\Environment"; }
static std::wstring appKey() { return g_opt.regRoot.empty() ? L"Software\\HyperEZ" : g_opt.regRoot + L"\\HyperEZ"; }
static std::wstring uninstallKey() {
    return g_opt.regRoot.empty() ? L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\HyperEZ" : g_opt.regRoot + L"\\Uninstall\\HyperEZ";
}

static std::wstring defaultDir() {
    std::wstring prev;
    if (getValue(HKEY_CURRENT_USER, appKey(), L"InstallDir", prev) && !prev.empty()) return prev;
    return knownFolder(FOLDERID_LocalAppData) + L"\\Programs\\HyperEZ";
}

static std::wstring startMenuDir() {
    if (!g_opt.regRoot.empty()) return g_opt.dir + L"\\_menu_inicio_prueba";
    return knownFolder(FOLDERID_Programs) + L"\\HyperEZ";
}

static std::wstring selfPath() {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof buf / sizeof buf[0]));
    return std::wstring(buf, n);
}

static bool startsWithI(const std::wstring& s, const std::wstring& p) {
    return s.size() >= p.size() && _wcsnicmp(s.c_str(), p.c_str(), p.size()) == 0;
}

static void parseArgs() {
    int n = 0;
    LPWSTR* a = CommandLineToArgvW(GetCommandLineW(), &n);
    for (int i = 1; i < n; i++) {
        std::wstring s = a[i];
        if (s == L"/S" || s == L"/s" || s == L"/silent") g_opt.silent = true;
        else if (s == L"/uninstall") g_opt.uninstall = true;
        else if (s == L"/NOASSOC") g_opt.assoc = false;
        else if (s == L"/NOPATH") g_opt.path = false;
        else if (s == L"/NOSHORTCUTS") g_opt.shortcuts = false;
        else if (s == L"/NOVSCODE") g_opt.vscode = false;
        else if (startsWithI(s, L"/D=")) g_opt.dir = s.substr(3);
        else if (startsWithI(s, L"/REGROOT=")) g_opt.regRoot = s.substr(9);
        else if (startsWithI(s, L"/VSCODE-EXTDIR=")) g_opt.vscodeExtDir = s.substr(15);
        else if (startsWithI(s, L"/FROMTEMP=")) g_opt.fromTemp = s.substr(10);
        else if (startsWithI(s, L"/PARENT=")) g_opt.parentPid = (DWORD)_wtoi(s.c_str() + 8);
    }
    LocalFree(a);
    if (g_opt.dir.empty()) g_opt.dir = defaultDir();
    g_opt.dir = backslashes(g_opt.dir);
    while (g_opt.dir.size() > 3 && g_opt.dir.back() == L'\\') g_opt.dir.pop_back();
}

// ---------------------------------------------------------------------------
// Utilidades de archivos
// ---------------------------------------------------------------------------

static bool makeDirs(const std::wstring& d) {
    int r = SHCreateDirectoryExW(nullptr, d.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS || r == ERROR_FILE_EXISTS;
}

static bool writeAll(const std::wstring& path, const uint8_t* data, size_t size) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    size_t off = 0;
    bool ok = true;
    while (off < size) {
        DWORD w = 0, chunk = (DWORD)std::min<size_t>(size - off, 1 << 20);
        if (!WriteFile(h, data + off, chunk, &w, nullptr) || w == 0) { ok = false; break; }
        off += w;
    }
    CloseHandle(h);
    return ok;
}

static bool fileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring parentDir(const std::wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? L"" : p.substr(0, s);
}

static std::string readText(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    std::string s;
    char buf[65536];
    DWORD n;
    while (ReadFile(h, buf, sizeof buf, &n, nullptr) && n > 0) s.append(buf, n);
    CloseHandle(h);
    return s;
}

// Ejecuta un programa oculto y espera (devuelve el código de salida o -1).
static int runHidden(const std::wstring& cmdline, DWORD timeoutMs) {
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = cmdline;
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return -1;
    DWORD w = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD code = (DWORD)-1;
    if (w == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

static std::wstring cmdExe() {
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    return std::wstring(sys) + L"\\cmd.exe";
}

// ---------------------------------------------------------------------------
// Paquete incrustado
// ---------------------------------------------------------------------------

struct PackFile { std::wstring path; const uint8_t* data; uint64_t size; };

static bool loadPayload(std::vector<PackFile>& out) {
    HRSRC r = FindResourceW(nullptr, L"PAYLOAD", RT_RCDATA);
    if (!r) return false;
    HGLOBAL g = LoadResource(nullptr, r);
    const uint8_t* p = (const uint8_t*)LockResource(g);
    DWORD size = SizeofResource(nullptr, r);
    if (!p || size < 12 || memcmp(p, "HZPACK01", 8) != 0) return false;
    const uint8_t* end = p + size;
    const uint8_t* q = p + 8;
    uint32_t n;
    memcpy(&n, q, 4);
    q += 4;
    for (uint32_t i = 0; i < n; i++) {
        if (q + 4 > end) return false;
        uint32_t len;
        memcpy(&len, q, 4);
        q += 4;
        if (q + len + 8 > end) return false;
        std::string rel((const char*)q, len);
        q += len;
        uint64_t sz;
        memcpy(&sz, q, 8);
        q += 8;
        if (q + sz > end) return false;
        out.push_back({backslashes(widen(rel)), q, sz});
        q += sz;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Instalación
// ---------------------------------------------------------------------------

struct Progress {
    std::atomic<int> percent{0};
    std::wstring status;
    CRITICAL_SECTION cs;
    HWND notify = nullptr;
    Progress() { InitializeCriticalSection(&cs); }
    void set(int pct, const std::wstring& s) {
        EnterCriticalSection(&cs);
        status = s;
        LeaveCriticalSection(&cs);
        percent = pct;
        if (notify) PostMessageW(notify, WM_APP + 1, 0, 0);
    }
    std::wstring get() {
        EnterCriticalSection(&cs);
        std::wstring s = status;
        LeaveCriticalSection(&cs);
        return s;
    }
};

static Progress g_progress;
static std::wstring g_error;
static std::vector<std::wstring> g_warnings;

static std::wstring vsixPath(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\vscode\\*.vsix").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return L"";
    std::wstring r = dir + L"\\vscode\\" + fd.cFileName;
    FindClose(h);
    return r;
}

static bool installVSCodeExtension(const std::wstring& dir) {
    std::wstring cli = findVSCodeCli();
    std::wstring vsix = vsixPath(dir);
    if (cli.empty() || vsix.empty()) return false;
    std::wstring cmd = cmdExe() + L" /d /c \"\"" + cli + L"\" --install-extension \"" + vsix + L"\" --force";
    if (!g_opt.vscodeExtDir.empty()) cmd += L" --extensions-dir \"" + g_opt.vscodeExtDir + L"\"";
    cmd += L"\"";
    return runHidden(cmd, 180000) == 0;
}

static void uninstallVSCodeExtension() {
    std::wstring cli = findVSCodeCli();
    if (cli.empty()) return;
    std::wstring cmd = cmdExe() + L" /d /c \"\"" + cli + L"\" --uninstall-extension hyperez.hyperez";
    if (!g_opt.vscodeExtDir.empty()) cmd += L" --extensions-dir \"" + g_opt.vscodeExtDir + L"\"";
    cmd += L"\"";
    runHidden(cmd, 120000);
}

static void createShortcuts(const std::wstring& dir) {
    std::wstring sm = startMenuDir();
    makeDirs(sm);
    std::wstring hyperExe = dir + L"\\bin\\hyper.exe";
    std::wstring home = knownFolder(FOLDERID_Profile);
    createShortcut(sm + L"\\HyperEZ (terminal).lnk", cmdExe(), L"/k \"\"" + hyperExe + L"\" help\"", L"Terminal con HyperEZ listo para usar", hyperExe, 0, home);
    createShortcut(sm + L"\\HyperEZ (consola interactiva).lnk", hyperExe, L"repl", L"Escribe código HyperEZ y ve el resultado al momento",
                   hyperExe, 0, home);
    createShortcut(sm + L"\\Ejemplos de HyperEZ.lnk", dir + L"\\examples", L"", L"Programas de ejemplo de HyperEZ", hyperExe, 0, dir + L"\\examples");
    if (fileExists(dir + L"\\docs\\GUIA.md"))
        createShortcut(sm + L"\\Guía de HyperEZ.lnk", dir + L"\\docs\\GUIA.md", L"", L"Guía del lenguaje HyperEZ", dir + L"\\icons\\hz-file.ico", 0, dir + L"\\docs");
    createShortcut(sm + L"\\Referencia de la API.lnk", dir + L"\\docs\\API.md", L"", L"Todas las funciones de HyperEZ", dir + L"\\icons\\hz-file.ico", 0, dir + L"\\docs");
    createShortcut(sm + L"\\Desinstalar HyperEZ.lnk", dir + L"\\uninstall.exe", L"/uninstall", L"Desinstalar HyperEZ", dir + L"\\uninstall.exe", 0, dir);
}

static uint64_t dirSizeKB(const std::vector<PackFile>& files) {
    uint64_t s = 0;
    for (auto& f : files) s += f.size;
    return s / 1024;
}

static bool doInstall() {
    std::vector<PackFile> files;
    if (!loadPayload(files)) { g_error = L"El instalador está dañado (no se encontró el paquete de archivos)."; return false; }
    const std::wstring& dir = g_opt.dir;
    g_progress.set(2, L"Creando " + dir);
    if (!makeDirs(dir)) { g_error = L"No se puede crear la carpeta:\n" + dir; return false; }
    // Restos de actualizaciones anteriores
    WIN32_FIND_DATAW fd;
    HANDLE fh = FindFirstFileW((dir + L"\\bin\\*.old*").c_str(), &fd);
    if (fh != INVALID_HANDLE_VALUE) {
        do DeleteFileW((dir + L"\\bin\\" + fd.cFileName).c_str()); while (FindNextFileW(fh, &fd));
        FindClose(fh);
    }
    std::wstring manifest;
    size_t i = 0;
    for (auto& f : files) {
        std::wstring dest = dir + L"\\" + f.path;
        g_progress.set(5 + (int)(70 * i++ / files.size()), L"Copiando " + f.path);
        makeDirs(parentDir(dest));
        if (!writeAll(dest, f.data, (size_t)f.size)) {
            // Un ejecutable en uso (p. ej. el servidor de lenguaje de VS Code) se puede renombrar pero no sobrescribir.
            std::wstring old = dest + L".old" + std::to_wstring(GetTickCount());
            if (!MoveFileExW(dest.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING) || !writeAll(dest, f.data, (size_t)f.size)) {
                g_error = L"No se puede escribir el archivo:\n" + dest + L"\n\nCierra los programas que estén usando HyperEZ y vuelve a intentarlo.";
                return false;
            }
        }
        manifest += f.path + L"\n";
    }
    // Desinstalador: copia de este mismo ejecutable
    g_progress.set(78, L"Creando el desinstalador");
    std::wstring self = selfPath(), uninst = dir + L"\\uninstall.exe";
    if (_wcsicmp(self.c_str(), uninst.c_str()) != 0) CopyFileW(self.c_str(), uninst.c_str(), FALSE);
    manifest += L"uninstall.exe\n";
    std::string m8 = narrow(manifest);
    writeAll(dir + L"\\uninstall.lst", (const uint8_t*)m8.data(), m8.size());

    std::wstring hyperExe = dir + L"\\bin\\hyper.exe";
    if (g_opt.assoc) {
        g_progress.set(82, L"Asociando los archivos .hz y .hez");
        if (!registerFileTypes(hyperExe, true, classesKey(), dir + L"\\icons\\hz-file.ico")) g_warnings.push_back(L"No se pudo asociar .hz/.hez.");
    }
    if (g_opt.path) {
        g_progress.set(86, L"Añadiendo HyperEZ al PATH");
        if (!updateUserPath(dir + L"\\bin", true, envKey())) g_warnings.push_back(L"No se pudo modificar el PATH.");
    }
    if (g_opt.shortcuts) {
        g_progress.set(88, L"Creando accesos directos");
        createShortcuts(dir);
    }
    // Registro de la aplicación
    g_progress.set(90, L"Registrando HyperEZ");
    setValue(HKEY_CURRENT_USER, appKey(), L"InstallDir", dir);
    setValue(HKEY_CURRENT_USER, appKey(), L"Version", widen(HYPEREZ_VERSION));
    std::wstring uk = uninstallKey();
    setValue(HKEY_CURRENT_USER, uk, L"DisplayName", L"HyperEZ " + widen(HYPEREZ_VERSION));
    setValue(HKEY_CURRENT_USER, uk, L"DisplayVersion", widen(HYPEREZ_VERSION));
    setValue(HKEY_CURRENT_USER, uk, L"Publisher", L"HyperEZ");
    setValue(HKEY_CURRENT_USER, uk, L"DisplayIcon", hyperExe + L",0");
    setValue(HKEY_CURRENT_USER, uk, L"InstallLocation", dir);
    setValue(HKEY_CURRENT_USER, uk, L"UninstallString", L"\"" + uninst + L"\" /uninstall");
    setValue(HKEY_CURRENT_USER, uk, L"QuietUninstallString", L"\"" + uninst + L"\" /uninstall /S");
    setValue(HKEY_CURRENT_USER, uk, L"Comments", L"Lenguaje de programación sencillo para aprender y crear juegos");
    setDword(HKEY_CURRENT_USER, uk, L"EstimatedSize", (DWORD)dirSizeKB(files));
    setDword(HKEY_CURRENT_USER, uk, L"NoModify", 1);
    setDword(HKEY_CURRENT_USER, uk, L"NoRepair", 1);
    if (g_opt.vscode) {
        g_progress.set(92, L"Instalando la extensión de Visual Studio Code...");
        if (!findVSCodeCli().empty() && !installVSCodeExtension(dir)) g_warnings.push_back(L"No se pudo instalar la extensión de VS Code (puedes instalarla después desde la carpeta vscode).");
    }
    g_progress.set(100, L"¡Listo! HyperEZ " + widen(HYPEREZ_VERSION) + L" está instalado.");
    return true;
}

static void removeEmptyDirs(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) removeEmptyDirs(dir + L"\\" + n);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

static bool doUninstall() {
    const std::wstring& dir = g_opt.dir;
    g_progress.set(10, L"Quitando la asociación de archivos");
    registerFileTypes(dir + L"\\bin\\hyper.exe", false, classesKey());
    g_progress.set(25, L"Quitando HyperEZ del PATH");
    updateUserPath(dir + L"\\bin", false, envKey());
    g_progress.set(35, L"Quitando la extensión de VS Code");
    if (g_opt.vscode) uninstallVSCodeExtension();
    g_progress.set(55, L"Borrando accesos directos");
    std::wstring sm = startMenuDir();
    for (const wchar_t* n : {L"HyperEZ (terminal).lnk", L"HyperEZ (consola interactiva).lnk", L"Ejemplos de HyperEZ.lnk", L"Guía de HyperEZ.lnk", L"Referencia de la API.lnk", L"Desinstalar HyperEZ.lnk"})
        DeleteFileW((sm + L"\\" + n).c_str());
    RemoveDirectoryW(sm.c_str());
    g_progress.set(70, L"Borrando archivos");
    // Solo se borra lo que instaló el instalador (uninstall.lst)
    std::string lst = readText(dir + L"\\uninstall.lst");
    std::wstring all = widen(lst);
    size_t s = 0;
    while (s < all.size()) {
        size_t e = all.find(L'\n', s);
        if (e == std::wstring::npos) e = all.size();
        std::wstring rel = all.substr(s, e - s);
        if (!rel.empty() && rel.find(L"..") == std::wstring::npos) {
            std::wstring p = dir + L"\\" + rel;
            for (int tries = 0; tries < 20 && fileExists(p) && !DeleteFileW(p.c_str()); tries++) Sleep(150);
        }
        s = e + 1;
    }
    WIN32_FIND_DATAW fd;
    HANDLE fh = FindFirstFileW((dir + L"\\bin\\*.old*").c_str(), &fd);
    if (fh != INVALID_HANDLE_VALUE) {
        do DeleteFileW((dir + L"\\bin\\" + fd.cFileName).c_str()); while (FindNextFileW(fh, &fd));
        FindClose(fh);
    }
    DeleteFileW((dir + L"\\uninstall.lst").c_str());
    removeEmptyDirs(dir);
    g_progress.set(90, L"Borrando el registro");
    deleteTree(HKEY_CURRENT_USER, uninstallKey());
    deleteTree(HKEY_CURRENT_USER, appKey());
    g_progress.set(100, L"HyperEZ se ha desinstalado.");
    return true;
}

// El desinstalador no puede borrarse a sí mismo: se copia a %TEMP% y se relanza desde allí.
static bool relaunchFromTempIfNeeded() {
    if (!g_opt.fromTemp.empty()) return false;
    std::wstring self = selfPath();
    if (!startsWithI(self, g_opt.dir + L"\\")) return false;
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring copy = std::wstring(tmp) + L"hyperez-uninstall-" + std::to_wstring(GetCurrentProcessId()) + L".exe";
    if (!CopyFileW(self.c_str(), copy.c_str(), FALSE)) return false;
    std::wstring cmd = L"\"" + copy + L"\" /uninstall /D=\"" + g_opt.dir + L"\" /FROMTEMP=\"" + self + L"\" /PARENT=" + std::to_wstring(GetCurrentProcessId());
    if (g_opt.silent) cmd += L" /S";
    if (!g_opt.vscode) cmd += L" /NOVSCODE";
    if (!g_opt.regRoot.empty()) cmd += L" /REGROOT=\"" + g_opt.regRoot + L"\"";
    if (!g_opt.vscodeExtDir.empty()) cmd += L" /VSCODE-EXTDIR=\"" + g_opt.vscodeExtDir + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    if (g_opt.silent) WaitForSingleObject(pi.hProcess, 300000); // el que llamó espera el resultado real
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ExitProcess(g_opt.silent ? code : 0);
}

// Cuando terminan este proceso y el que lo lanzó, se borran uninstall.exe, la carpeta (si quedó vacía)
// y la copia temporal del desinstalador. rmdir sin /s nunca borra archivos del usuario.
static void scheduleSelfDelete() {
    if (g_opt.fromTemp.empty()) return;
    std::wstring cmd = cmdExe() + L" /d /c ping -n 4 127.0.0.1 >nul & del /f /q \"" + g_opt.fromTemp + L"\" & rmdir \"" + g_opt.dir +
                       L"\" & del /f /q \"" + selfPath() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

// ---------------------------------------------------------------------------
// Interfaz gráfica
// ---------------------------------------------------------------------------

enum { ID_DIR = 100, ID_BROWSE, ID_ASSOC, ID_PATH, ID_SHORTCUTS, ID_VSCODE, ID_PROGRESS, ID_STATUS, ID_INSTALL, ID_CANCEL };

static HWND g_wnd, g_dirEdit, g_progressBar, g_statusText, g_installBtn, g_cancelBtn;
static HWND g_checks[4];
static HFONT g_font, g_titleFont;
static HICON g_logo;
static int g_dpi = 96;
static bool g_busy = false, g_done = false;

static int S(int v) { return MulDiv(v, g_dpi, 96); }

static HWND control(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), g_wnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

static std::wstring windowText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, L'\0');
    GetWindowTextW(h, s.data(), n + 1);
    s.resize(n);
    return s;
}

static void browseFolder() {
    IFileDialog* fd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fd)))) return;
    DWORD o;
    fd->GetOptions(&o);
    fd->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    fd->SetTitle(L"Carpeta de instalación de HyperEZ");
    if (SUCCEEDED(fd->Show(g_wnd))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(fd->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                std::wstring d = p;
                if (d.size() < 8 || _wcsicmp(d.substr(d.size() - 7).c_str(), L"HyperEZ") != 0) d += L"\\HyperEZ";
                SetWindowTextW(g_dirEdit, d.c_str());
                CoTaskMemFree(p);
            }
            it->Release();
        }
    }
    fd->Release();
}

static void startInstall() {
    g_opt.dir = backslashes(windowText(g_dirEdit));
    while (g_opt.dir.size() > 3 && g_opt.dir.back() == L'\\') g_opt.dir.pop_back();
    g_opt.assoc = SendMessageW(g_checks[0], BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_opt.path = SendMessageW(g_checks[1], BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_opt.shortcuts = SendMessageW(g_checks[2], BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_opt.vscode = SendMessageW(g_checks[3], BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_busy = true;
    for (HWND h : {g_dirEdit, g_installBtn, g_cancelBtn, GetDlgItem(g_wnd, ID_BROWSE)}) EnableWindow(h, FALSE);
    for (HWND h : g_checks) EnableWindow(h, FALSE);
    ShowWindow(g_progressBar, SW_SHOW);
    g_progress.notify = g_wnd;
    std::thread([] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool ok = doInstall();
        CoUninitialize();
        PostMessageW(g_wnd, WM_APP + 2, ok ? 1 : 0, 0);
    }).detach();
}

static void openExamples() {
    std::wstring ex = g_opt.dir + L"\\examples";
    std::wstring code = findVSCode();
    if (!code.empty()) ShellExecuteW(nullptr, L"open", code.c_str(), (L"\"" + ex + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    else ShellExecuteW(nullptr, L"explore", ex.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_wnd = h;
            control(L"STATIC", L"Carpeta de instalación:", 0, 24, 104, 300, 20, 0);
            g_dirEdit = control(L"EDIT", g_opt.dir.c_str(), WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 24, 126, 416, 25, ID_DIR);
            control(L"BUTTON", L"Examinar...", WS_TABSTOP | BS_PUSHBUTTON, 448, 125, 90, 27, ID_BROWSE);
            bool hasCode = !findVSCodeCli().empty();
            const wchar_t* labels[4] = {L"Asociar los archivos .hz y .hez (icono, doble clic para ejecutar)",
                                        L"Añadir HyperEZ al PATH (usar \"hyper\" desde cualquier terminal)",
                                        L"Crear accesos directos en el menú Inicio",
                                        hasCode ? L"Instalar la extensión para Visual Studio Code" : L"Instalar la extensión para Visual Studio Code (no se encontró VS Code)"};
            for (int i = 0; i < 4; i++) {
                g_checks[i] = control(L"BUTTON", labels[i], WS_TABSTOP | BS_AUTOCHECKBOX, 24, 166 + 27 * i, 514, 24, ID_ASSOC + i);
                SendMessageW(g_checks[i], BM_SETCHECK, BST_CHECKED, 0);
            }
            if (!hasCode) { SendMessageW(g_checks[3], BM_SETCHECK, BST_UNCHECKED, 0); EnableWindow(g_checks[3], FALSE); }
            g_progressBar = control(PROGRESS_CLASSW, L"", 0, 24, 284, 514, 16, ID_PROGRESS);
            SendMessageW(g_progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            ShowWindow(g_progressBar, SW_HIDE);
            g_statusText = control(L"STATIC", L"", SS_ENDELLIPSIS, 24, 306, 514, 20, ID_STATUS);
            g_installBtn = control(L"BUTTON", L"Instalar", WS_TABSTOP | BS_DEFPUSHBUTTON, 346, 340, 96, 30, ID_INSTALL);
            g_cancelBtn = control(L"BUTTON", L"Cancelar", WS_TABSTOP | BS_PUSHBUTTON, 448, 340, 90, 30, ID_CANCEL);
            SetFocus(g_installBtn);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            RECT head = {0, 0, rc.right, S(88)};
            FillRect(dc, &head, (HBRUSH)GetStockObject(WHITE_BRUSH));
            RECT line = {0, S(88), rc.right, S(89)};
            HBRUSH gray = CreateSolidBrush(RGB(220, 222, 226));
            FillRect(dc, &line, gray);
            DeleteObject(gray);
            if (g_logo) DrawIconEx(dc, S(24), S(16), g_logo, S(56), S(56), 0, nullptr, DI_NORMAL);
            SetBkMode(dc, TRANSPARENT);
            HGDIOBJ old = SelectObject(dc, g_titleFont);
            std::wstring title = L"HyperEZ " + widen(HYPEREZ_VERSION);
            TextOutW(dc, S(96), S(18), title.c_str(), (int)title.size());
            SelectObject(dc, g_font);
            SetTextColor(dc, RGB(90, 96, 106));
            const wchar_t* sub = L"Lenguaje de programación sencillo para aprender y crear juegos";
            TextOutW(dc, S(96), S(52), sub, (int)wcslen(sub));
            SelectObject(dc, old);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wp;
            SetBkMode(dc, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_BROWSE: browseFolder(); return 0;
                case ID_INSTALL:
                    if (g_done) { openExamples(); DestroyWindow(h); }
                    else if (!g_busy) startInstall();
                    return 0;
                case ID_CANCEL:
                    if (!g_busy) DestroyWindow(h);
                    return 0;
            }
            break;
        case WM_APP + 1:
            SendMessageW(g_progressBar, PBM_SETPOS, g_progress.percent, 0);
            SetWindowTextW(g_statusText, g_progress.get().c_str());
            return 0;
        case WM_APP + 2:
            g_busy = false;
            SendMessageW(g_progressBar, PBM_SETPOS, g_progress.percent, 0);
            SetWindowTextW(g_statusText, g_progress.get().c_str());
            if (wp) {
                g_done = true;
                SetWindowTextW(g_installBtn, L"Abrir ejemplos");
                SetWindowTextW(g_cancelBtn, L"Cerrar");
                EnableWindow(g_installBtn, TRUE);
                EnableWindow(g_cancelBtn, TRUE);
                std::wstring msg = L"HyperEZ se ha instalado en:\n" + g_opt.dir +
                                   L"\n\n• Haz doble clic en un archivo .hz para ejecutarlo.\n• Abre una terminal nueva y escribe:  hyper help\n• En VS Code, abre un .hz y pulsa F5.";
                for (auto& w : g_warnings) msg += L"\n\nAviso: " + w;
                MessageBoxW(h, msg.c_str(), L"Instalación completada", MB_ICONINFORMATION);
            } else {
                MessageBoxW(h, g_error.c_str(), L"Error en la instalación", MB_ICONERROR);
                EnableWindow(g_cancelBtn, TRUE);
                EnableWindow(g_installBtn, TRUE);
                EnableWindow(g_dirEdit, TRUE);
                for (HWND c : g_checks) EnableWindow(c, TRUE);
            }
            return 0;
        case WM_CLOSE:
            if (g_busy) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static int runGui() {
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    HDC screen = GetDC(nullptr);
    g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    NONCLIENTMETRICSW ncm{sizeof ncm};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    ncm.lfMessageFont.lfHeight = -MulDiv(9, g_dpi, 72);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW tf = ncm.lfMessageFont;
    tf.lfHeight = -MulDiv(17, g_dpi, 72);
    tf.lfWeight = FW_BOLD;
    g_titleFont = CreateFontIndirectW(&tf);
    g_logo = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(2), IMAGE_ICON, S(56), S(56), 0);
    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = g_inst;
    wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = L"HyperEZSetup";
    RegisterClassExW(&wc);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, S(562), S(388)};
    AdjustWindowRect(&r, style, FALSE);
    int w = r.right - r.left, hgt = r.bottom - r.top;
    HWND wnd = CreateWindowExW(0, wc.lpszClassName, (L"Instalar HyperEZ " + widen(HYPEREZ_VERSION)).c_str(), style,
                               (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - hgt) / 2, w, hgt, nullptr, nullptr, g_inst, nullptr);
    ShowWindow(wnd, SW_SHOW);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (!IsDialogMessageW(wnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return g_done ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    g_inst = inst;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    parseArgs();
    if (g_opt.uninstall) {
        if (!g_opt.silent && g_opt.fromTemp.empty() &&
            MessageBoxW(nullptr, (L"¿Desinstalar HyperEZ de\n" + g_opt.dir + L"?").c_str(), L"Desinstalar HyperEZ", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return 1;
        relaunchFromTempIfNeeded();
        if (g_opt.parentPid && !g_opt.silent) { // en modo silencioso el padre espera a que terminemos
            HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, g_opt.parentPid);
            if (p) { WaitForSingleObject(p, 10000); CloseHandle(p); }
        }
        bool ok = doUninstall();
        if (!g_opt.silent) MessageBoxW(nullptr, L"HyperEZ se ha desinstalado.", L"Desinstalar HyperEZ", MB_ICONINFORMATION);
        scheduleSelfDelete();
        CoUninitialize();
        return ok ? 0 : 1;
    }
    if (g_opt.silent) {
        bool ok = doInstall();
        CoUninitialize();
        return ok ? 0 : 1;
    }
    int r = runGui();
    CoUninitialize();
    return r;
}
