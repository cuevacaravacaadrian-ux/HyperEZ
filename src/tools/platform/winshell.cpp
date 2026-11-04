// Integración con Windows: asociación de archivos .hz/.hez, PATH del usuario y accesos directos.
// Todo se escribe en HKEY_CURRENT_USER, así que no hace falta ser administrador.
#ifdef _WIN32
#include "tools/platform/winshell.h"

#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>

#include <cstdio>
#include <vector>

namespace hz::win {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring backslashes(std::wstring p) {
    for (auto& c : p) if (c == L'/') c = L'\\';
    return p;
}

bool setValue(HKEY root, const std::wstring& key, const std::wstring& name, const std::wstring& value, DWORD type) {
    HKEY h;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) != ERROR_SUCCESS) return false;
    LONG r = RegSetValueExW(h, name.empty() ? nullptr : name.c_str(), 0, type, (const BYTE*)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    return r == ERROR_SUCCESS;
}

bool setDword(HKEY root, const std::wstring& key, const std::wstring& name, DWORD value) {
    HKEY h;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) != ERROR_SUCCESS) return false;
    LONG r = RegSetValueExW(h, name.c_str(), 0, REG_DWORD, (const BYTE*)&value, sizeof value);
    RegCloseKey(h);
    return r == ERROR_SUCCESS;
}

bool getValue(HKEY root, const std::wstring& key, const std::wstring& name, std::wstring& out, DWORD* typeOut) {
    HKEY h;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS) return false;
    DWORD type = 0, size = 0;
    LONG r = RegQueryValueExW(h, name.empty() ? nullptr : name.c_str(), nullptr, &type, nullptr, &size);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) { RegCloseKey(h); return false; }
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
    r = RegQueryValueExW(h, name.empty() ? nullptr : name.c_str(), nullptr, &type, (BYTE*)buf.data(), &size);
    RegCloseKey(h);
    if (r != ERROR_SUCCESS) return false;
    out.assign(buf.data());
    if (typeOut) *typeOut = type;
    return true;
}

bool deleteTree(HKEY root, const std::wstring& key) {
    LONG r = RegDeleteTreeW(root, key.c_str());
    if (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND) {
        RegDeleteKeyW(root, key.c_str());
        return true;
    }
    return false;
}

static bool exists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) r = p;
    if (p) CoTaskMemFree(p);
    return r;
}

std::wstring findVSCode() {
    std::vector<std::wstring> c;
    c.push_back(knownFolder(FOLDERID_LocalAppData) + L"\\Programs\\Microsoft VS Code\\Code.exe");
    c.push_back(knownFolder(FOLDERID_ProgramFiles) + L"\\Microsoft VS Code\\Code.exe");
    c.push_back(knownFolder(FOLDERID_ProgramFilesX86) + L"\\Microsoft VS Code\\Code.exe");
    for (auto& p : c) if (exists(p)) return p;
    // instalación registrada en "Abrir con"
    std::wstring cmd;
    if (getValue(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\Code.exe\\shell\\open\\command", L"", cmd) ||
        getValue(HKEY_LOCAL_MACHINE, L"Software\\Classes\\Applications\\Code.exe\\shell\\open\\command", L"", cmd)) {
        if (!cmd.empty() && cmd[0] == L'"') {
            size_t e = cmd.find(L'"', 1);
            if (e != std::wstring::npos && exists(cmd.substr(1, e - 1))) return cmd.substr(1, e - 1);
        }
    }
    return L"";
}

std::wstring findVSCodeCli() {
    std::wstring code = findVSCode();
    if (code.empty()) return L"";
    std::wstring dir = code.substr(0, code.find_last_of(L'\\'));
    std::wstring cli = dir + L"\\bin\\code.cmd";
    return exists(cli) ? cli : L"";
}

void notifyAssociationsChanged() { SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr); }

void broadcastEnvironmentChange() {
    DWORD_PTR res;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 3000, &res);
}

bool registerFileTypes(const std::wstring& exe0, bool install, const std::wstring& classes, const std::wstring& iconFile0) {
    HKEY root = HKEY_CURRENT_USER;
    std::wstring exe = backslashes(exe0);
    std::wstring iconFile = iconFile0.empty() ? exe : backslashes(iconFile0);
    const std::wstring prog = classes + L"\\HyperEZ.Source";
    const std::wstring app = classes + L"\\Applications\\hyper.exe";
    const std::wstring here = classes + L"\\Directory\\Background\\shell\\HyperEZ"; // "Abrir HyperEZ aquí"
    if (!install) {
        for (const wchar_t* ext : {L".hz", L".hez"}) {
            std::wstring key = classes + L"\\" + ext, cur;
            if (getValue(root, key, L"", cur) && cur == L"HyperEZ.Source") deleteTree(root, key);
            else {
                HKEY h;
                if (RegOpenKeyExW(root, (key + L"\\OpenWithProgids").c_str(), 0, KEY_SET_VALUE, &h) == ERROR_SUCCESS) {
                    RegDeleteValueW(h, L"HyperEZ.Source");
                    RegCloseKey(h);
                }
            }
        }
        deleteTree(root, prog);
        deleteTree(root, app);
        deleteTree(root, here);
        notifyAssociationsChanged();
        return true;
    }
    bool ok = true;
    std::wstring q = L"\"" + exe + L"\"";
    ok &= setValue(root, prog, L"", L"Programa HyperEZ");
    ok &= setValue(root, prog, L"FriendlyTypeName", L"Programa HyperEZ");
    ok &= setValue(root, prog + L"\\DefaultIcon", L"", L"\"" + iconFile + L"\"" + (iconFile0.empty() ? L",1" : L",0"));
    ok &= setValue(root, prog + L"\\shell", L"", L"open");
    ok &= setValue(root, prog + L"\\shell\\open", L"", L"Ejecutar con HyperEZ");
    ok &= setValue(root, prog + L"\\shell\\open", L"Icon", q);
    ok &= setValue(root, prog + L"\\shell\\open\\command", L"", q + L" run --pause \"%1\"");
    std::wstring editor = findVSCode();
    std::wstring editCmd = editor.empty() ? L"notepad.exe \"%1\"" : L"\"" + editor + L"\" \"%1\"";
    ok &= setValue(root, prog + L"\\shell\\edit", L"", editor.empty() ? L"Editar" : L"Editar con Visual Studio Code");
    if (!editor.empty()) ok &= setValue(root, prog + L"\\shell\\edit", L"Icon", L"\"" + editor + L"\"");
    ok &= setValue(root, prog + L"\\shell\\edit\\command", L"", editCmd);
    ok &= setValue(root, prog + L"\\shell\\check", L"", L"Comprobar errores (HyperEZ)");
    ok &= setValue(root, prog + L"\\shell\\check", L"Icon", q);
    ok &= setValue(root, prog + L"\\shell\\check\\command", L"", q + L" check --pause \"%1\"");
    ok &= setValue(root, prog + L"\\shell\\watch", L"", L"Ejecutar y repetir al guardar (HyperEZ)");
    ok &= setValue(root, prog + L"\\shell\\watch", L"Icon", q);
    ok &= setValue(root, prog + L"\\shell\\watch\\command", L"", q + L" watch \"%1\"");
    // Clic derecho en el fondo de una carpeta: consola interactiva en esa carpeta
    ok &= setValue(root, here, L"", L"Abrir HyperEZ aquí");
    ok &= setValue(root, here, L"Icon", q);
    ok &= setValue(root, here + L"\\command", L"", q + L" repl");
    for (const wchar_t* ext : {L".hz", L".hez"}) {
        std::wstring key = classes + L"\\" + ext;
        ok &= setValue(root, key, L"", L"HyperEZ.Source");
        ok &= setValue(root, key, L"Content Type", L"text/x-hyperez");
        ok &= setValue(root, key, L"PerceivedType", L"text");
        ok &= setValue(root, key + L"\\OpenWithProgids", L"HyperEZ.Source", L"");
        ok &= setValue(root, key + L"\\ShellNew", L"NullFile", L"");
    }
    ok &= setValue(root, app, L"FriendlyAppName", L"HyperEZ");
    ok &= setValue(root, app + L"\\DefaultIcon", L"", q + L",0");
    ok &= setValue(root, app + L"\\shell\\open\\command", L"", q + L" run --pause \"%1\"");
    ok &= setValue(root, app + L"\\SupportedTypes", L".hz", L"");
    ok &= setValue(root, app + L"\\SupportedTypes", L".hez", L"");
    // Si antes se eligió otro programa para .hz/.hez ("Abrir siempre con..."), Windows guarda esa
    // elección en UserChoice y tiene prioridad sobre todo lo anterior: se borra para que HyperEZ
    // sea el programa predeterminado. (Solo en el registro real, no en el de pruebas.)
    if (_wcsicmp(classes.c_str(), L"Software\\Classes") == 0) {
        for (const wchar_t* ext : {L".hz", L".hez"})
            deleteTree(root, std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + ext + L"\\UserChoice");
    }
    notifyAssociationsChanged();
    return ok;
}

static std::vector<std::wstring> splitPath(const std::wstring& p) {
    std::vector<std::wstring> r;
    size_t s = 0;
    while (s <= p.size()) {
        size_t e = p.find(L';', s);
        if (e == std::wstring::npos) e = p.size();
        std::wstring part = p.substr(s, e - s);
        if (!part.empty()) r.push_back(part);
        s = e + 1;
    }
    return r;
}

static std::wstring normDir(std::wstring d) {
    d = backslashes(d);
    while (d.size() > 3 && d.back() == L'\\') d.pop_back();
    CharLowerBuffW(d.data(), (DWORD)d.size());
    return d;
}

bool updateUserPath(const std::wstring& dir, bool add, const std::wstring& envKey) {
    std::wstring cur;
    DWORD type = REG_EXPAND_SZ;
    getValue(HKEY_CURRENT_USER, envKey, L"Path", cur, &type);
    auto parts = splitPath(cur);
    std::wstring target = normDir(dir);
    std::vector<std::wstring> kept;
    bool present = false;
    for (auto& p : parts) {
        if (normDir(p) == target) { present = true; if (!add) continue; }
        kept.push_back(p);
    }
    if (add && !present) kept.push_back(backslashes(dir));
    if (add == present && add) return true; // ya estaba
    std::wstring joined;
    for (size_t i = 0; i < kept.size(); i++) { if (i) joined += L';'; joined += kept[i]; }
    bool ok = setValue(HKEY_CURRENT_USER, envKey, L"Path", joined, type == REG_SZ ? REG_SZ : REG_EXPAND_SZ);
    if (ok && envKey == L"Environment") broadcastEnvironmentChange();
    return ok;
}

bool createShortcut(const std::wstring& lnk, const std::wstring& target, const std::wstring& args, const std::wstring& desc,
                    const std::wstring& icon, int iconIndex, const std::wstring& workDir) {
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW* sl = nullptr;
    bool ok = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl))) {
        sl->SetPath(target.c_str());
        if (!args.empty()) sl->SetArguments(args.c_str());
        if (!desc.empty()) sl->SetDescription(desc.c_str());
        if (!icon.empty()) sl->SetIconLocation(icon.c_str(), iconIndex);
        if (!workDir.empty()) sl->SetWorkingDirectory(workDir.c_str());
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf))) {
            ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
            pf->Release();
        }
        sl->Release();
    }
    if (SUCCEEDED(init)) CoUninitialize();
    return ok;
}

} // namespace hz::win

// Puente para la CLI (tools.h)
#include "tools/tools.h"
namespace hz {
bool registerFileTypes(const std::string& hyperExe, bool install, const std::wstring& classesRoot, bool quiet) {
    bool ok = win::registerFileTypes(win::widen(hyperExe), install, classesRoot);
    if (!quiet) {
        if (ok) std::printf(install ? "✓ Archivos .hz y .hez asociados a %s\n" : "✓ Asociación de .hz y .hez eliminada\n", hyperExe.c_str());
        else std::fprintf(stderr, "No se pudo modificar el registro de Windows\n");
    }
    return ok;
}
} // namespace hz
#endif
