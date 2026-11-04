// Ampliaciones de la biblioteca estándar: pruebas, utilidades para juegos,
// archivos, sistema, consola y métodos extra de listas, textos y mapas.
#include "lang/stdlib/native.h"
#include "lang/stdlib/mathtypes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <random>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <conio.h>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace hz {

namespace fs = std::filesystem;

static ObjList* L(Value* args) { return static_cast<ObjList*>(args[-1].o); }
static ObjMap* M(Value* args) { return static_cast<ObjMap*>(args[-1].o); }
static const std::string& S(Value* args) { return asString(args[-1])->str; }


// Mantiene vivos (para el GC) los objetos creados mientras se llama a código del script.

// Llama a una función del script pasándole solo los argumentos que declara:
// m.mapValues(v => v * 2) y m.mapValues((v, k) => ...) funcionan las dos.
static Value callFlexible(VM& vm, Value fn, int argc, Value* args) {
    Value target = isObjType(fn, OT::Bound) ? static_cast<ObjBound*>(fn.o)->method : fn;
    int n = argc;
    if (isObjType(target, OT::Closure)) {
        ObjFunction* f = static_cast<ObjClosure*>(target.o)->fn;
        if (!f->hasRest) n = std::min(argc, std::max(f->arity, f->minArity));
    } else if (isObjType(target, OT::Native)) {
        auto* nf = static_cast<ObjNative*>(target.o);
        if (nf->maxArgs >= 0) n = std::min(argc, nf->maxArgs);
    }
    return vm.call(fn, n, args);
}

static bool lessForSort(VM& vm, const Value& a, const Value& b, const char* fname) {
    if (a.isNum() && b.isNum()) return a.n < b.n;
    if (isString(a) && isString(b)) return asString(a)->str < asString(b)->str;
    vm.raise(std::string(fname) + ": solo se pueden comparar números o textos, no " + vm.typeName(a) + " y " + vm.typeName(b));
}

// ---------------------------------------------------------------------------
// Pruebas
// ---------------------------------------------------------------------------

NATIVE(n_test) {
    std::string name = argStr(vm, args, argc, 0, "test");
    Value body = args[1];
    if (!(isObjType(body, OT::Closure) || isObjType(body, OT::Native) || isObjType(body, OT::Bound)))
        vm.raise("test: el segundo argumento debe ser una función, no " + vm.typeName(body));
    Value r;
    bool ok = vm.tryCall(body, 0, nullptr, &r);
    if (ok && isObjType(r, OT::Task)) {
        // prueba async: se espera a que termine
        auto* t = static_cast<ObjTask*>(r.o);
        vm.tempRoots.push_back(r);
        while (t->state == TaskState::Pending && vm.hasPendingTasks()) vm.pumpScheduler();
        vm.tempRoots.pop_back();
        if (t->state == TaskState::Failed) { ok = false; vm.lastError = t->error; t->errorObserved = true; }
        else if (t->state == TaskState::Pending) { ok = false; vm.lastError = vm.makeError("la tarea no terminó"); }
    }
    if (ok) {
        vm.testsPassed++;
        vm.print(std::string("  ") + color::green() + "✓" + color::reset() + " " + name + "\n");
    } else {
        vm.testsFailed++;
        vm.print(std::string("  ") + color::red() + "✗ " + name + color::reset() + "\n      " + vm.errorMessage(vm.lastError) + "\n");
    }
    return Value::boolean(ok);
}

NATIVE(n_assertEqual) {
    Value a = argc > 0 ? args[0] : Value(), b = argc > 1 ? args[1] : Value();
    if (a.isUndef()) a = Value();
    if (b.isUndef()) b = Value();
    bool eq = valuesEqual(a, b);
    if (!eq && a.isNum() && b.isNum()) eq = std::abs(a.n - b.n) <= 1e-9 * std::max(1.0, std::max(std::abs(a.n), std::abs(b.n)));
    if (!eq) {
        std::string msg = "se esperaba " + vm.toString(b, true) + " pero se obtuvo " + vm.toString(a, true);
        if (argc > 2 && isString(args[2])) msg = asString(args[2])->str + ": " + msg;
        vm.raise(msg);
    }
    return Value();
}

NATIVE(n_assertThrows) {
    Value fn = args[0];
    Value r;
    if (vm.tryCall(fn, 0, nullptr, &r)) vm.raise(argc > 1 && isString(args[1]) ? asString(args[1])->str : "se esperaba que la función lanzara un error");
    return vm.lastError;
}

// ---------------------------------------------------------------------------
// Utilidades numéricas para juegos
// ---------------------------------------------------------------------------

NATIVE(n_remap) {
    double x = argNum(vm, args, argc, 0, "remap"), a1 = argNum(vm, args, argc, 1, "remap"), b1 = argNum(vm, args, argc, 2, "remap");
    double a2 = argNum(vm, args, argc, 3, "remap"), b2 = argNum(vm, args, argc, 4, "remap");
    if (b1 == a1) return Value(a2);
    return Value(a2 + (x - a1) * (b2 - a2) / (b1 - a1));
}
NATIVE(n_wrap) {
    double x = argNum(vm, args, argc, 0, "wrap"), lo = argNum(vm, args, argc, 1, "wrap"), hi = argNum(vm, args, argc, 2, "wrap");
    double span = hi - lo;
    if (span <= 0) return Value(lo);
    double r = std::fmod(x - lo, span);
    if (r < 0) r += span;
    return Value(lo + r);
}
NATIVE(n_approach) {
    double x = argNum(vm, args, argc, 0, "approach"), target = argNum(vm, args, argc, 1, "approach"), step = std::abs(argNum(vm, args, argc, 2, "approach"));
    if (x < target) return Value(std::min(x + step, target));
    return Value(std::max(x - step, target));
}
NATIVE(n_smoothstep) {
    double a = argNum(vm, args, argc, 0, "smoothstep"), b = argNum(vm, args, argc, 1, "smoothstep"), x = argNum(vm, args, argc, 2, "smoothstep");
    double t = b == a ? 0 : std::clamp((x - a) / (b - a), 0.0, 1.0);
    return Value(t * t * (3 - 2 * t));
}
NATIVE(n_chance) {
    double p = argNum(vm, args, argc, 0, "chance");
    return Value::boolean(randomUnit() < p);
}
NATIVE(n_distance) {
    double x1 = argNum(vm, args, argc, 0, "distance"), y1 = argNum(vm, args, argc, 1, "distance");
    double x2 = argNum(vm, args, argc, 2, "distance"), y2 = argNum(vm, args, argc, 3, "distance");
    return Value(std::hypot(x2 - x1, y2 - y1));
}
NATIVE(n_isNaN) { UNUSED; return Value::boolean(args[0].isNum() && std::isnan(args[0].n)); }

// ---------------------------------------------------------------------------
// File
// ---------------------------------------------------------------------------

std::string resolvePath(VM& vm, const std::string& p) {
    fs::path path((const char8_t*)p.c_str());
    if (path.is_absolute() || vm.projectRoot.empty()) return p;
    return pathJoin(vm.projectRoot, p);
}

std::vector<std::string> argStrList(VM& vm, Value* args, int argc, int i, const char* fname) {
    std::vector<std::string> out;
    Value v = optArg(args, argc, i);
    if (v.isNull()) return out;
    if (isString(v)) { out.push_back(asString(v)->str); return out; }
    if (!isObjType(v, OT::List)) vm.raise(std::string(fname) + ": se esperaba una lista de textos, no " + vm.typeName(v));
    for (auto& x : static_cast<ObjList*>(v.o)->items) out.push_back(isString(x) ? asString(x)->str : vm.toString(x));
    return out;
}

NATIVE(file_read) {
    std::string p = argStr(vm, args, argc, 0, "File.read");
    std::string out;
    if (!readFile(resolvePath(vm, p), out)) vm.raise("File.read: no se puede leer el archivo '" + p + "'");
    return vm.str(out);
}
NATIVE(file_readLines) {
    std::string p = argStr(vm, args, argc, 0, "File.readLines");
    std::string out;
    if (!readFile(resolvePath(vm, p), out)) vm.raise("File.readLines: no se puede leer el archivo '" + p + "'");
    ObjList* l = vm.newList();
    TempRoot keep(vm, l);
    for (auto& line : splitLines(out)) l->items.push_back(vm.str(line));
    if (!l->items.empty() && asString(l->items.back())->str.empty()) l->items.pop_back();
    return Value::object(l);
}
NATIVE(file_write) {
    std::string p = argStr(vm, args, argc, 0, "File.write");
    std::string data = isString(args[1]) ? asString(args[1])->str : vm.toString(args[1]);
    std::string full = resolvePath(vm, p);
    std::string dir = pathDir(full);
    if (!dir.empty() && dir != ".") makeDirs(dir);
    if (!writeFile(full, data)) vm.raise("File.write: no se puede escribir el archivo '" + p + "'");
    return Value();
}
NATIVE(file_append) {
    std::string p = argStr(vm, args, argc, 0, "File.append");
    std::string data = isString(args[1]) ? asString(args[1])->str : vm.toString(args[1]);
    std::string full = resolvePath(vm, p), prev;
    readFile(full, prev);
    if (!writeFile(full, prev + data)) vm.raise("File.append: no se puede escribir el archivo '" + p + "'");
    return Value();
}
NATIVE(file_exists) { std::string p = resolvePath(vm, argStr(vm, args, argc, 0, "File.exists")); return Value::boolean(fileExists(p) || dirExists(p)); }
NATIVE(file_isDir) { return Value::boolean(dirExists(resolvePath(vm, argStr(vm, args, argc, 0, "File.isDir")))); }
NATIVE(file_delete) {
    std::error_code ec;
    return Value::boolean(fs::remove(FP(resolvePath(vm, argStr(vm, args, argc, 0, "File.delete"))), ec));
}
NATIVE(file_makeDir) { return Value::boolean(makeDirs(resolvePath(vm, argStr(vm, args, argc, 0, "File.makeDir")))); }
NATIVE(file_list) {
    std::string p = resolvePath(vm, argStr(vm, args, argc, 0, "File.list", "."));
    ObjList* l = vm.newList();
    TempRoot keep(vm, l);
    std::vector<std::string> names;
    std::error_code ec;
    for (auto it = fs::directory_iterator(FP(p), ec); !ec && it != fs::directory_iterator(); it.increment(ec))
        names.push_back(U8(it->path().filename()));
    std::sort(names.begin(), names.end());
    for (auto& n : names) l->items.push_back(vm.str(n));
    return Value::object(l);
}

// ---------------------------------------------------------------------------
// System
// ---------------------------------------------------------------------------

NATIVE(sys_args) {
    UNUSED;
    ObjList* l = vm.newList();
    TempRoot keep(vm, l);
    for (auto& a : vm.scriptArgs) l->items.push_back(vm.str(a));
    return Value::object(l);
}
NATIVE(sys_platform) {
    UNUSED;
#ifdef _WIN32
    return vm.str("windows");
#elif defined(__APPLE__)
    return vm.str("macos");
#else
    return vm.str("linux");
#endif
}
NATIVE(sys_version) { UNUSED; return vm.str(HYPEREZ_VERSION); }
NATIVE(sys_cwd) { UNUSED; return vm.str(currentDir()); }
NATIVE(sys_scriptDir) { UNUSED; return vm.str(vm.projectRoot); }
NATIVE(sys_env) {
    std::string n = argStr(vm, args, argc, 0, "System.env");
#ifdef _WIN32
    std::wstring wn(n.begin(), n.end());
    wchar_t buf[32768];
    DWORD len = GetEnvironmentVariableW(wn.c_str(), buf, 32768);
    if (len == 0 || len >= 32768) return Value();
    int sz = WideCharToMultiByte(CP_UTF8, 0, buf, (int)len, nullptr, 0, nullptr, nullptr);
    std::string out(sz, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf, (int)len, out.data(), sz, nullptr, nullptr);
    return vm.str(out);
#else
    const char* v = std::getenv(n.c_str());
    return v ? vm.str(v) : Value();
#endif
}
NATIVE(sys_date) {
    UNUSED;
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    ObjMap* m = vm.newMap();
    TempRoot keep(vm, m);
    m->set(vm.str("year"), Value(tm.tm_year + 1900));
    m->set(vm.str("month"), Value(tm.tm_mon + 1));
    m->set(vm.str("day"), Value(tm.tm_mday));
    m->set(vm.str("hour"), Value(tm.tm_hour));
    m->set(vm.str("minute"), Value(tm.tm_min));
    m->set(vm.str("second"), Value(tm.tm_sec));
    m->set(vm.str("weekday"), Value(tm.tm_wday == 0 ? 7 : tm.tm_wday));
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    m->set(vm.str("text"), vm.str(buf));
    return Value::object(m);
}
NATIVE(sys_sleep) {
    double s = argNum(vm, args, argc, 0, "System.sleep");
    vm.flushOutput();
    if (s > 0) {
#ifdef _WIN32
        Sleep((DWORD)(s * 1000));
#else
        usleep((useconds_t)(s * 1e6));
#endif
    }
    return Value();
}

// ---------------------------------------------------------------------------
// Console (juegos de texto)
// ---------------------------------------------------------------------------

// Secuencias de escape solo si la salida es una terminal (no al redirigir a un archivo o en pruebas).
static bool ansiEnabled() {
    static int cached = -1;
    if (cached < 0) {
        if (std::getenv("HYPEREZ_COLOR")) cached = 1;
        else if (std::getenv("NO_COLOR")) cached = 0;
        else {
#ifdef _WIN32
            DWORD mode;
            cached = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode) ? 1 : 0;
#else
            cached = isatty(1) ? 1 : 0;
#endif
        }
    }
    return cached == 1;
}

static std::string ansiColor(VM& vm, Value c, bool background) {
    if (c.isNull() || c.isUndef()) return "";
    if (!ansiEnabled()) { toColor(vm, c, "Console"); return ""; } // valida el color igualmente
    RGBA rgba = toColor(vm, c, "Console");
    char buf[48];
    std::snprintf(buf, sizeof buf, "\x1b[%d;2;%d;%d;%dm", background ? 48 : 38, (int)std::lround(rgba.r * 255), (int)std::lround(rgba.g * 255), (int)std::lround(rgba.b * 255));
    return buf;
}

NATIVE(con_clear) { UNUSED; vm.flushOutput(); std::fputs("\x1b[2J\x1b[3J\x1b[H", stdout); std::fflush(stdout); return Value(); }
NATIVE(con_write) {
    std::string text = isString(args[0]) ? asString(args[0])->str : vm.toString(args[0]);
    std::string pre = ansiColor(vm, optArg(args, argc, 1), false) + ansiColor(vm, optArg(args, argc, 2), true);
    vm.print(pre.empty() ? text : pre + text + "\x1b[0m");
    return Value();
}
NATIVE(con_print) {
    std::string text = argc > 0 ? (isString(args[0]) ? asString(args[0])->str : vm.toString(args[0])) : "";
    std::string pre = ansiColor(vm, optArg(args, argc, 1), false) + ansiColor(vm, optArg(args, argc, 2), true);
    vm.print((pre.empty() ? text : pre + text + "\x1b[0m") + "\n");
    return Value();
}
NATIVE(con_moveTo) {
    int x = (int)argNum(vm, args, argc, 0, "Console.moveTo"), y = (int)argNum(vm, args, argc, 1, "Console.moveTo");
    if (!ansiEnabled()) return Value();
    vm.print("\x1b[" + std::to_string(std::max(0, y) + 1) + ";" + std::to_string(std::max(0, x) + 1) + "H");
    return Value();
}
NATIVE(con_cursor) {
    if (!ansiEnabled()) return Value();
    vm.print(argBool(args, argc, 0, true) ? "\x1b[?25h" : "\x1b[?25l");
    return Value();
}
NATIVE(con_title) {
    std::string t = argStr(vm, args, argc, 0, "Console.title");
    if (!ansiEnabled()) return Value();
    vm.print("\x1b]0;" + t + "\x07");
    return Value();
}
NATIVE(con_width) {
    UNUSED;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) return Value(info.srWindow.Right - info.srWindow.Left + 1);
#else
    winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return Value((double)w.ws_col);
#endif
    return Value(80);
}
NATIVE(con_height) {
    UNUSED;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) return Value(info.srWindow.Bottom - info.srWindow.Top + 1);
#else
    winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) return Value((double)w.ws_row);
#endif
    return Value(25);
}

// Nombre legible de una tecla leída de la consola.
static std::string keyFromCode(uint32_t c, bool extended) {
    if (extended) {
        switch (c) {
            case 72: return "up";
            case 80: return "down";
            case 75: return "left";
            case 77: return "right";
            case 71: return "home";
            case 79: return "end";
            case 73: return "pageup";
            case 81: return "pagedown";
            case 82: return "insert";
            case 83: return "delete";
            default:
                if (c >= 59 && c <= 68) return "f" + std::to_string(c - 58);
                return "?";
        }
    }
    switch (c) {
        case 13: case 10: return "enter";
        case 27: return "escape";
        case 32: return "space";
        case 8: case 127: return "backspace";
        case 9: return "tab";
        default: break;
    }
    std::string s;
    utf8Append(s, c);
    return s;
}

#ifdef _WIN32
static Value readKeyImpl(VM& vm) {
    if (!GetConsoleWindow()) vm.raise("Console.readKey: no hay una consola interactiva (ejecuta el programa desde una terminal)");
    wint_t c = _getwch();
    if (c == WEOF) vm.raise("Console.readKey: no se pudo leer del teclado");
    if (c == 0 || c == 0xE0) return vm.str(keyFromCode((uint32_t)_getwch(), true));
    return vm.str(keyFromCode((uint32_t)c, false));
}
#else
// Lee un byte de la entrada esperando como mucho timeoutMs (-1: sin límite). -1 si no hay.
static int rawByte(int timeoutMs) {
    pollfd p{STDIN_FILENO, POLLIN, 0};
    if (poll(&p, 1, timeoutMs) <= 0) return -1;
    unsigned char c;
    return read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
}
// Terminal en modo "una tecla cada vez" mientras exista el objeto.
struct RawTerminal {
    termios old{};
    bool ok = false;
    RawTerminal() {
        if (tcgetattr(STDIN_FILENO, &old) != 0) return;
        termios raw = old;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        ok = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
    }
    ~RawTerminal() { if (ok) tcsetattr(STDIN_FILENO, TCSANOW, &old); }
};
static Value readKeyPosix(VM& vm, bool wait) {
    RawTerminal term;
    int c = rawByte(wait ? -1 : 0);
    if (c < 0) {
        if (wait) vm.raise("Console.readKey: no se pudo leer del teclado");
        return Value();
    }
    std::string r;
    if (c == 27) {
        int c2 = rawByte(30);
        if (c2 == '[' || c2 == 'O') {
            int c3 = rawByte(30);
            switch (c3) {
                case 'A': r = "up"; break;
                case 'B': r = "down"; break;
                case 'C': r = "right"; break;
                case 'D': r = "left"; break;
                case 'H': r = "home"; break;
                case 'F': r = "end"; break;
                case 'P': r = "f1"; break;
                case 'Q': r = "f2"; break;
                case 'R': r = "f3"; break;
                case 'S': r = "f4"; break;
                default:
                    if (c3 >= '1' && c3 <= '6') { // ESC [ n ~
                        int c4 = rawByte(30);
                        static const char* names[] = {"", "home", "insert", "delete", "end", "pageup", "pagedown"};
                        r = c4 == '~' ? names[c3 - '0'] : "?";
                    } else {
                        r = "?";
                    }
            }
        } else {
            r = "escape";
        }
    } else if (c >= 0xC0) { // carácter UTF-8 de varios bytes (ñ, á...)
        std::string bytes(1, (char)c);
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
        for (int i = 0; i < extra; i++) { int x = rawByte(30); if (x >= 0) bytes += (char)x; }
        size_t i = 0;
        r = keyFromCode(utf8Decode(bytes, i), false);
    } else {
        r = keyFromCode((uint32_t)c, false);
    }
    return vm.str(r);
}
static Value readKeyImpl(VM& vm) { return readKeyPosix(vm, true); }
#endif

NATIVE(con_interactive) {
    UNUSED;
#ifdef _WIN32
    DWORD mode;
    return Value::boolean(GetConsoleWindow() != nullptr && GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) &&
                          GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode));
#else
    return Value::boolean(isatty(0) && isatty(1));
#endif
}

NATIVE(con_readKey) { UNUSED; vm.flushOutput(); return readKeyImpl(vm); }
NATIVE(con_keyAvailable) {
    UNUSED;
    vm.flushOutput();
#ifdef _WIN32
    return Value::boolean(_kbhit() != 0);
#else
    RawTerminal term;
    pollfd p{STDIN_FILENO, POLLIN, 0};
    return Value::boolean(poll(&p, 1, 0) > 0);
#endif
}
NATIVE(con_pollKey) {
    UNUSED;
    vm.flushOutput();
#ifdef _WIN32
    if (!_kbhit()) return Value();
    return readKeyImpl(vm);
#else
    return readKeyPosix(vm, false);
#endif
}

// ---------------------------------------------------------------------------
// Métodos extra de list
// ---------------------------------------------------------------------------

NATIVE(l_zip) {
    ObjList* other = argList(vm, args, argc, 0, "zip");
    std::vector<Value> a = L(args)->items, b = other->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        ObjList* pair = vm.newList();
        pair->items = {a[i], b[i]};
        out->items.push_back(Value::object(pair));
    }
    return Value::object(out);
}
static void flatten(ObjList* src, ObjList* out, int depth) {
    for (auto& v : src->items) {
        if (depth > 0 && isObjType(v, OT::List)) flatten(static_cast<ObjList*>(v.o), out, depth - 1);
        else out->items.push_back(v);
    }
}
NATIVE(l_flat) {
    int depth = (int)argNum(vm, args, argc, 0, "flat", 1);
    ObjList* out = vm.newList();
    flatten(L(args), out, depth);
    return Value::object(out);
}
NATIVE(l_flatMap) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    for (size_t i = 0; i < items.size(); i++) {
        Value a[2] = {items[i], Value((double)i)};
        Value r = callFlexible(vm, fn, 2, a);
        if (isObjType(r, OT::List)) for (auto& x : static_cast<ObjList*>(r.o)->items) out->items.push_back(x);
        else out->items.push_back(r);
    }
    return Value::object(out);
}
NATIVE(l_chunk) {
    double n = argNum(vm, args, argc, 0, "chunk");
    if (n < 1) vm.raise("chunk: el tamaño debe ser al menos 1");
    std::vector<Value> items = L(args)->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    for (size_t i = 0; i < items.size(); i += (size_t)n) {
        ObjList* part = vm.newList();
        out->items.push_back(Value::object(part));
        for (size_t j = i; j < items.size() && j < i + (size_t)n; j++) part->items.push_back(items[j]);
    }
    return Value::object(out);
}
NATIVE(l_take) {
    double n = std::max(0.0, argNum(vm, args, argc, 0, "take"));
    auto& items = L(args)->items;
    ObjList* out = vm.newList();
    out->items.assign(items.begin(), items.begin() + (long)std::min<double>(n, (double)items.size()));
    return Value::object(out);
}
NATIVE(l_skip) {
    double n = std::max(0.0, argNum(vm, args, argc, 0, "skip"));
    auto& items = L(args)->items;
    ObjList* out = vm.newList();
    out->items.assign(items.begin() + (long)std::min<double>(n, (double)items.size()), items.end());
    return Value::object(out);
}
NATIVE(l_groupBy) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    ObjMap* out = vm.newMap();
    TempRoot keep(vm, out);
    for (auto& v : items) {
        Value k = vm.call(fn, 1, &v);
        if (k.isNull() || k.isUndef()) k = vm.str("null");
        Value* slot = out->find(k);
        if (!slot) {
            ObjList* g = vm.newList();
            out->set(k, Value::object(g));
            slot = out->find(k);
        }
        static_cast<ObjList*>(slot->o)->items.push_back(v);
    }
    return Value::object(out);
}
static Value byKey(VM& vm, Value* args, bool wantMax, const char* fname) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    if (items.empty()) return Value();
    ObjList* keys = vm.newList();
    TempRoot keep(vm, keys);
    size_t best = 0;
    Value bestKey;
    for (size_t i = 0; i < items.size(); i++) {
        Value k = vm.call(fn, 1, &items[i]);
        keys->items.push_back(k);
        if (i == 0 || (wantMax ? lessForSort(vm, bestKey, k, fname) : lessForSort(vm, k, bestKey, fname))) { best = i; bestKey = k; }
    }
    return items[best];
}
NATIVE(l_minBy) { (void)argc; return byKey(vm, args, false, "minBy"); }
NATIVE(l_maxBy) { (void)argc; return byKey(vm, args, true, "maxBy"); }
NATIVE(l_average) {
    UNUSED;
    auto& items = L(args)->items;
    if (items.empty()) return Value(0);
    double s = 0;
    for (auto& v : items) { if (!v.isNum()) vm.raise("average: la lista contiene " + vm.typeName(v)); s += v.n; }
    return Value(s / (double)items.size());
}
NATIVE(l_partition) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    ObjList* yes = vm.newList();
    out->items.push_back(Value::object(yes));
    ObjList* no = vm.newList();
    out->items.push_back(Value::object(no));
    for (auto& v : items) (isFalsy(vm.call(fn, 1, &v)) ? no : yes)->items.push_back(v);
    return Value::object(out);
}
NATIVE(l_swap) {
    auto& items = L(args)->items;
    auto idx = [&](int i) {
        double d = argNum(vm, args, argc, i, "swap");
        long k = (long)d;
        if (k < 0) k += (long)items.size();
        if (d != std::floor(d) || k < 0 || k >= (long)items.size()) vm.raise("swap: índice fuera de rango: " + formatNumber(d));
        return (size_t)k;
    };
    size_t a = idx(0), b = idx(1);
    std::swap(items[a], items[b]);
    return args[-1];
}
NATIVE(l_fill) {
    Value v = args[0];
    for (auto& x : L(args)->items) x = v;
    return args[-1];
}

// ---------------------------------------------------------------------------
// Métodos extra de text
// ---------------------------------------------------------------------------

NATIVE(t_capitalize) {
    UNUSED;
    const std::string& s = S(args);
    if (s.empty()) return args[-1];
    size_t i = 0;
    uint32_t cp = utf8Decode(s, i);
    std::string first;
    utf8Append(first, cp);
    // reutiliza upper() del propio tipo text
    Value fv = vm.str(first);
    Value up = vm.callMethod(fv, "upper", 0, nullptr);
    return vm.str(asString(up)->str + s.substr(i));
}
NATIVE(t_words) {
    UNUSED;
    const std::string& s = S(args);
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    std::string cur;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { if (!cur.empty()) { out->items.push_back(vm.str(cur)); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) out->items.push_back(vm.str(cur));
    return Value::object(out);
}
NATIVE(t_isNumeric) {
    UNUSED;
    std::string s = trim(S(args));
    if (s.empty()) return Value::boolean(false);
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return Value::boolean(end && *end == '\0');
}
NATIVE(t_center) {
    int width = (int)argNum(vm, args, argc, 0, "center");
    std::string fill = argStr(vm, args, argc, 1, "center", " ");
    if (fill.empty()) fill = " ";
    const std::string& s = S(args);
    int len = (int)utf8Length(s);
    if (len >= width) return args[-1];
    int total = width - len, left = total / 2, right = total - left;
    std::string out;
    for (int i = 0; i < left; i++) out += fill;
    out += s;
    for (int i = 0; i < right; i++) out += fill;
    return vm.str(out);
}
NATIVE(t_format) {
    // "{} tiene {} puntos".format("Ana", 10)  ·  "{0} y {0}".format("x")
    const std::string& s = S(args);
    std::string out;
    int next = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '{' && i + 1 < s.size() && s[i + 1] == '{') { out += '{'; i++; continue; }
        if (s[i] == '}' && i + 1 < s.size() && s[i + 1] == '}') { out += '}'; i++; continue; }
        if (s[i] == '{') {
            size_t close = s.find('}', i);
            if (close == std::string::npos) { out += s.substr(i); break; }
            std::string inner = s.substr(i + 1, close - i - 1);
            int idx = inner.empty() ? next++ : std::atoi(inner.c_str());
            if (idx < argc) out += isString(args[idx]) ? asString(args[idx])->str : vm.toString(args[idx]);
            else vm.raise("format: falta el valor número " + std::to_string(idx + 1));
            i = close;
            continue;
        }
        out += s[i];
    }
    return vm.str(out);
}

// ---------------------------------------------------------------------------
// Métodos extra de map
// ---------------------------------------------------------------------------

NATIVE(m_filter) {
    Value fn = args[0];
    std::vector<ObjMap::Entry> entries = M(args)->entries;
    ObjMap* out = vm.newMap();
    TempRoot keep(vm, out);
    for (auto& e : entries) {
        if (!e.alive) continue;
        Value a[2] = {e.key, e.value};
        if (!isFalsy(callFlexible(vm, fn, 2, a))) out->set(e.key, e.value);
    }
    return Value::object(out);
}
NATIVE(m_mapValues) {
    Value fn = args[0];
    std::vector<ObjMap::Entry> entries = M(args)->entries;
    ObjMap* out = vm.newMap();
    TempRoot keep(vm, out);
    for (auto& e : entries) {
        if (!e.alive) continue;
        Value a[2] = {e.value, e.key};
        out->set(e.key, callFlexible(vm, fn, 2, a));
    }
    return Value::object(out);
}
NATIVE(m_keyOf) {
    UNUSED;
    for (auto& e : M(args)->entries) if (e.alive && valuesEqual(e.value, args[0])) return e.key;
    return Value();
}

// ---------------------------------------------------------------------------
// Registro
// ---------------------------------------------------------------------------

void registerStdlibExtra(VM& vm) {
    // Pruebas
    vm.defineGlobalFn("test", "(nombre: text, cuerpo: function): bool",
                      "Ejecuta una prueba con nombre. Muestra ✓ o ✗ sin detener el programa; 'hyper test' falla si alguna prueba falla.", n_test,
                      "test(\"sumar\", () => assertEqual(2 + 2, 4))");
    vm.defineGlobalFn("assertEqual", "(actual: any, esperado: any, mensaje?: text): void",
                      "Lanza un error si los valores no son iguales (los números se comparan con una pequeña tolerancia).", n_assertEqual);
    vm.defineGlobalFn("assertThrows", "(funcion: function, mensaje?: text): Error",
                      "Comprueba que la función lanza un error y lo devuelve.", n_assertThrows);
    // Utilidades numéricas
    vm.defineGlobalFn("remap", "(x: number, desdeMin: number, desdeMax: number, hastaMin: number, hastaMax: number): number",
                      "Convierte x de un intervalo a otro (p. ej. de 0..100 a 0..1).", n_remap);
    vm.defineGlobalFn("wrap", "(x: number, minimo: number, maximo: number): number",
                      "Da la vuelta: si x se sale por un lado, entra por el otro (útil para pantallas que se repiten).", n_wrap);
    vm.defineGlobalFn("approach", "(actual: number, objetivo: number, paso: number): number",
                      "Acerca 'actual' a 'objetivo' como mucho 'paso' sin pasarse.", n_approach);
    vm.defineGlobalFn("smoothstep", "(desde: number, hasta: number, x: number): number", "Transición suave de 0 a 1 entre desde y hasta.", n_smoothstep);
    vm.defineGlobalFn("chance", "(probabilidad: number): bool", "true con la probabilidad indicada (0 a 1). chance(0.25) es true 1 de cada 4 veces.", n_chance);
    vm.defineGlobalFn("distance", "(x1: number, y1: number, x2: number, y2: number): number", "Distancia entre dos puntos.", n_distance);
    vm.defineGlobalFn("isNaN", "(x: any): bool", "true si x no es un número válido (NaN).", n_isNaN);

    // File
    ObjModule* file = vm.nativeModule("File", "Leer y escribir archivos. Las rutas relativas parten de la carpeta del programa.");
    vm.defineFn(file, "read", "(ruta: text): text", "Contenido completo de un archivo de texto.", file_read, "var datos = File.read(\"nivel1.txt\")");
    vm.defineFn(file, "readLines", "(ruta: text): list<text>", "Líneas de un archivo de texto.", file_readLines);
    vm.defineFn(file, "write", "(ruta: text, contenido: any): void", "Escribe (o reemplaza) un archivo. Crea las carpetas que falten.", file_write);
    vm.defineFn(file, "append", "(ruta: text, contenido: any): void", "Añade texto al final de un archivo.", file_append);
    vm.defineFn(file, "exists", "(ruta: text): bool", "¿Existe el archivo o la carpeta?", file_exists);
    vm.defineFn(file, "isDir", "(ruta: text): bool", "¿Es una carpeta?", file_isDir);
    vm.defineFn(file, "delete", "(ruta: text): bool", "Borra un archivo (o una carpeta vacía).", file_delete);
    vm.defineFn(file, "makeDir", "(ruta: text): bool", "Crea una carpeta (y las intermedias).", file_makeDir);
    vm.defineFn(file, "list", "(carpeta?: text = \".\"): list<text>", "Nombres de los archivos y carpetas que contiene una carpeta.", file_list);

    // System
    ObjModule* sys = vm.nativeModule("System", "Información del sistema y del programa.");
    vm.defineProp(sys, "args", "list<text>", "Argumentos pasados al programa: hyper juego.hz uno dos", sys_args);
    vm.defineProp(sys, "platform", "text", "\"windows\", \"linux\" o \"macos\".", sys_platform);
    vm.defineProp(sys, "version", "text", "Versión de HyperEZ.", sys_version);
    vm.defineProp(sys, "cwd", "text", "Carpeta de trabajo actual.", sys_cwd);
    vm.defineProp(sys, "scriptDir", "text", "Carpeta del programa que se está ejecutando.", sys_scriptDir);
    vm.defineFn(sys, "env", "(nombre: text): text?", "Valor de una variable de entorno (o null).", sys_env);
    vm.defineFn(sys, "date", "(): map", "Fecha y hora actuales: {year, month, day, hour, minute, second, weekday, text}.", sys_date);
    vm.defineFn(sys, "sleep", "(segundos: number): void", "Detiene el programa unos segundos (bloquea; dentro de async usa await wait()).", sys_sleep);

    // Console
    ObjModule* con = vm.nativeModule("Console", "Control de la consola para juegos de texto: colores, cursor y teclas sin pulsar Intro.");
    vm.defineFn(con, "clear", "(): void", "Borra la pantalla de la consola.", con_clear);
    vm.defineFn(con, "write", "(texto: any, color?: Color, fondo?: Color): void", "Escribe sin salto de línea, opcionalmente en color.", con_write,
                "Console.write(\"¡Peligro!\", \"red\")");
    vm.defineFn(con, "print", "(texto?: any, color?: Color, fondo?: Color): void", "Escribe una línea, opcionalmente en color.", con_print);
    vm.defineFn(con, "moveTo", "(columna: number, fila: number): void", "Mueve el cursor (0, 0 es la esquina superior izquierda).", con_moveTo);
    vm.defineFn(con, "showCursor", "(visible?: bool = true): void", "Muestra u oculta el cursor.", con_cursor);
    vm.defineFn(con, "title", "(texto: text): void", "Cambia el título de la ventana de la consola.", con_title);
    vm.defineFn(con, "readKey", "(): text", "Espera a que se pulse una tecla y devuelve su nombre: \"a\", \"up\", \"enter\", \"space\"...", con_readKey,
                "var tecla = Console.readKey()");
    vm.defineFn(con, "keyAvailable", "(): bool", "¿Hay una tecla pulsada esperando? (no bloquea)", con_keyAvailable);
    vm.defineFn(con, "pollKey", "(): text?", "Tecla pulsada o null si no hay ninguna (no bloquea). Ideal para bucles de juego.", con_pollKey);
    vm.defineProp(con, "interactive", "bool",
                  "true si el programa corre en una consola con teclado (false en pruebas o con la salida redirigida).", con_interactive);
    vm.defineProp(con, "width", "number", "Columnas visibles de la consola.", con_width);
    vm.defineProp(con, "height", "number", "Filas visibles de la consola.", con_height);

    // list
    ObjClass* l = vm.listClass;
    vm.defineMethod(l, "zip", "(otra: list): list", "Pares [a, b] con los elementos de ambas listas.", l_zip);
    vm.defineMethod(l, "flat", "(profundidad?: number = 1): list", "Aplana listas anidadas.", l_flat);
    vm.defineMethod(l, "flatMap", "(f: function): list", "map + flat: f puede devolver una lista.", l_flatMap);
    vm.defineMethod(l, "chunk", "(tamano: number): list", "Divide en grupos de 'tamano' elementos.", l_chunk);
    vm.defineMethod(l, "take", "(n: number): list", "Los n primeros elementos.", l_take);
    vm.defineMethod(l, "skip", "(n: number): list", "Todos menos los n primeros.", l_skip);
    vm.defineMethod(l, "groupBy", "(f: function): map", "Agrupa en un mapa según la clave que devuelve f.", l_groupBy);
    vm.defineMethod(l, "minBy", "(f: function): any", "Elemento con el menor valor de f(elemento).", l_minBy);
    vm.defineMethod(l, "maxBy", "(f: function): any", "Elemento con el mayor valor de f(elemento).", l_maxBy);
    vm.defineMethod(l, "average", "(): number", "Media de los números de la lista.", l_average);
    vm.defineMethod(l, "partition", "(f: function): list", "[los que cumplen f, los que no].", l_partition);
    vm.defineMethod(l, "swap", "(i: number, j: number): list", "Intercambia dos elementos (en el sitio).", l_swap);
    vm.defineMethod(l, "fill", "(valor: any): list", "Pone el mismo valor en todas las posiciones (en el sitio).", l_fill);

    // text
    ObjClass* t = vm.textClass;
    vm.defineMethod(t, "capitalize", "(): text", "Primera letra en mayúscula.", t_capitalize);
    vm.defineMethod(t, "words", "(): list<text>", "Palabras separadas por espacios.", t_words);
    vm.defineMethod(t, "isNumeric", "(): bool", "¿El texto es un número válido?", t_isNumeric);
    vm.defineMethod(t, "center", "(ancho: number, relleno?: text = \" \"): text", "Centra el texto en 'ancho' caracteres.", t_center);
    vm.defineMethod(t, "format", "(...valores: any): text", "Sustituye {} (o {0}, {1}...) por los valores: \"{} tiene {} puntos\".format(nombre, 10)", t_format);

    // map
    ObjClass* m = vm.mapClass;
    vm.defineMethod(m, "filter", "(f: function): map", "Nuevo mapa con las entradas para las que f(clave, valor) es verdadero.", m_filter);
    vm.defineMethod(m, "mapValues", "(f: function): map", "Nuevo mapa con f(valor, clave) como valores.", m_mapValues);
    vm.defineMethod(m, "keyOf", "(valor: any): any", "Primera clave que tiene ese valor (o null).", m_keyOf);

    // Áreas de la biblioteca en sus propios archivos
    registerSystemLibs(vm);
    registerBinaryLibs(vm);
    registerRegex(vm);
    registerNet(vm);
    registerMatrix(vm);
    registerImage(vm);
    registerServer(vm);
    registerMore(vm);
}

} // namespace hz
