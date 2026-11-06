// Biblioteca estándar del núcleo: funciones globales y métodos de los tipos integrados.
#include "lang/vm/vm.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>

namespace hz {

// ---------------------------------------------------------------------------
// Ayudas para argumentos
// ---------------------------------------------------------------------------

static std::string argWhere(const char* fname, int i) { return std::string(fname) + ": el argumento " + std::to_string(i + 1); }

double argNum(VM& vm, Value* args, int argc, int i, const char* fname, double def) {
    if (i >= argc || args[i].isUndef() || (args[i].isNull() && !std::isnan(def))) {
        if (std::isnan(def)) vm.raise(argWhere(fname, i) + " es obligatorio");
        return def;
    }
    if (!args[i].isNum()) vm.raise(argWhere(fname, i) + " debe ser un número, no " + vm.typeName(args[i]));
    return args[i].n;
}

std::string argStr(VM& vm, Value* args, int argc, int i, const char* fname, const char* def) {
    if (i >= argc || args[i].isUndef() || (args[i].isNull() && def)) {
        if (!def) vm.raise(argWhere(fname, i) + " es obligatorio");
        return def;
    }
    if (!isString(args[i])) vm.raise(argWhere(fname, i) + " debe ser texto, no " + vm.typeName(args[i]));
    return asString(args[i])->str;
}

bool argBool(Value* args, int argc, int i, bool def) {
    if (i >= argc || args[i].isUndef() || args[i].isNull()) return def;
    return !isFalsy(args[i]);
}

ObjList* argList(VM& vm, Value* args, int argc, int i, const char* fname) {
    if (i >= argc || !isObjType(args[i], OT::List)) vm.raise(argWhere(fname, i) + " debe ser una lista" + (i < argc ? ", no " + vm.typeName(args[i]) : ""));
    return static_cast<ObjList*>(args[i].o);
}

ObjMap* argMap(VM& vm, Value* args, int argc, int i, const char* fname) {
    if (i >= argc || !isObjType(args[i], OT::Map)) vm.raise(argWhere(fname, i) + " debe ser un mapa" + (i < argc ? ", no " + vm.typeName(args[i]) : ""));
    return static_cast<ObjMap*>(args[i].o);
}

ObjVec* argVec(VM& vm, Value* args, int argc, int i, const char* fname, int kind) {
    static const char* names[] = {"Vec2", "Vec3", "Vec4", "Color", "Quat"};
    if (i >= argc || !isObjType(args[i], OT::Vec) || (kind >= 0 && static_cast<ObjVec*>(args[i].o)->kind != kind))
        vm.raise(argWhere(fname, i) + " debe ser " + (kind >= 0 ? names[kind] : "un vector") + (i < argc ? ", no " + vm.typeName(args[i]) : ""));
    return static_cast<ObjVec*>(args[i].o);
}

Value optArg(Value* args, int argc, int i) { return (i < argc && !args[i].isUndef()) ? args[i] : Value(); }

Value optField(VM& vm, ObjMap* opts, const char* key) {
    if (!opts) return Value();
    Value* v = opts->find(vm.str(key));
    return v ? *v : Value();
}

std::mt19937_64& randomEngine() { static std::mt19937_64 r(std::random_device{}()); return r; }
static std::mt19937_64& rng() { return randomEngine(); }
double randomUnit() { return (double)(randomEngine()() >> 11) * (1.0 / 9007199254740992.0); } // 53 bits
long long randomBetween(long long lo, long long hi) {
    if (hi <= lo) return lo;
    uint64_t span = (uint64_t)(hi - lo) + 1, limit = UINT64_MAX - UINT64_MAX % span; // sin sesgo
    uint64_t x;
    do x = randomEngine()(); while (x >= limit);
    return lo + (long long)(x % span);
}
static double rand01() { return randomUnit(); }

#define NATIVE(name) static Value name(VM& vm, int argc, Value* args)
#define UNUSED (void)vm; (void)argc; (void)args

// ---------------------------------------------------------------------------
// Globales
// ---------------------------------------------------------------------------

NATIVE(n_print) {
    std::string s;
    for (int i = 0; i < argc; i++) { if (i) s += ' '; s += vm.toString(args[i]); }
    s += '\n';
    vm.print(s);
    return Value();
}
NATIVE(n_write) {
    std::string s;
    for (int i = 0; i < argc; i++) { if (i) s += ' '; s += vm.toString(args[i]); }
    vm.print(s);
    return Value();
}
NATIVE(n_input) {
    if (argc > 0) vm.print(vm.toString(args[0]));
    vm.flushOutput();
    if (vm.inputHook) return vm.str(vm.inputHook(argc > 0 ? vm.toString(args[0]) : ""));
    std::string line;
    if (!readLine(line)) return Value();
    return vm.str(line);
}
NATIVE(n_len) {
    UNUSED;
    Value v = args[0];
    if (isString(v)) return Value((double)utf8Length(asString(v)->str));
    if (isObjType(v, OT::List)) return Value((double)static_cast<ObjList*>(v.o)->items.size());
    if (isObjType(v, OT::Map)) return Value((double)static_cast<ObjMap*>(v.o)->live);
    if (isObjType(v, OT::Range)) return Value(static_cast<ObjRange*>(v.o)->count());
    if (isObjType(v, OT::Enum)) return Value((double)static_cast<ObjEnum*>(v.o)->values.size());
    if (isObjType(v, OT::Instance) || isObjType(v, OT::Foreign)) {
        Value n = vm.getProperty(v, vm.s_length, true); // clases con propiedad length (Bytes, propias...)
        if (n.isNum()) return n;
    }
    vm.raise("len: no se puede medir un valor de tipo " + vm.typeName(v));
}
NATIVE(n_typeof) { UNUSED; return vm.str(vm.typeName(args[0])); }
NATIVE(n_str) { UNUSED; return isString(args[0]) ? args[0] : vm.str(vm.toString(args[0])); }
NATIVE(n_toNumber) {
    Value v = args[0];
    if (v.isNum()) return v;
    if (v.isBool()) return Value(v.b ? 1.0 : 0.0);
    if (isString(v)) {
        std::string s = trim(asString(v)->str);
        if (s.empty()) return argc > 1 ? args[1] : Value();
        char* end = nullptr;
        double d = std::strtod(s.c_str(), &end);
        if (end && *end == '\0') return Value(d);
        return argc > 1 ? args[1] : Value();
    }
    if (argc > 1) return args[1];
    vm.raise("toNumber: no se puede convertir " + vm.typeName(v) + " en número");
}
NATIVE(n_toInt) {
    double d = argNum(vm, args, argc, 0, "toInt");
    return Value(std::trunc(d));
}
NATIVE(n_assert) {
    if (isFalsy(args[0])) vm.raise(argc > 1 ? vm.toString(args[1]) : "La aserción falló");
    return Value();
}
NATIVE(n_exit) {
    vm.flushOutput();
#ifdef __EMSCRIPTEN__
    throw ProgramExit{argc > 0 && args[0].isNum() ? (int)args[0].n : 0};
#else
    std::exit(argc > 0 && args[0].isNum() ? (int)args[0].n : 0);
#endif
}
NATIVE(n_range) {
    double a = argNum(vm, args, argc, 0, "range");
    bool hasEnd = argc > 1 && !args[1].isUndef() && !args[1].isNull();
    double b = hasEnd ? argNum(vm, args, argc, 1, "range") : a;
    double step = argNum(vm, args, argc, 2, "range", 1);
    if (!hasEnd) a = 0;
    if (step == 0) vm.raise("range: el paso no puede ser 0");
    return Value::object(vm.newRange(a, b, step, false));
}
NATIVE(n_time) { UNUSED; return Value(vm.currentTime()); }
NATIVE(n_now) {
    UNUSED;
    using namespace std::chrono;
    return Value((double)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() / 1000.0);
}
NATIVE(n_wait) {
    double s = argNum(vm, args, argc, 0, "wait", 0);
    return Value::object(vm.waitTask(s));
}
NATIVE(n_nextFrame) {
    UNUSED;
    ObjTask* t = vm.newTask();
    vm.nextFrameTasks.push_back(t);
    return Value::object(t);
}
NATIVE(n_spawn) {
    Value fn = args[0];
    std::vector<Value> rest(args + 1, args + argc);
    Value r = vm.call(fn, (int)rest.size(), rest.data());
    if (isObjType(r, OT::Task)) return r;
    ObjTask* t = vm.newTask();
    vm.completeTask(t, r);
    return Value::object(t);
}
NATIVE(n_off) {
    UNUSED;
    if (isObjType(args[0], OT::Subscription)) { static_cast<ObjSubscription*>(args[0].o)->active = false; static_cast<ObjSubscription*>(args[0].o)->event->needsCleanup = true; return Value(); }
    if (isObjType(args[0], OT::Event)) {
        auto* e = static_cast<ObjEvent*>(args[0].o);
        for (auto* s : e->subs) s->active = false;
        e->needsCleanup = true;
        if (e->dispatching == 0) e->subs.clear();
        return Value();
    }
    vm.raise("off: necesita una suscripción o un evento");
}

// Matemáticas
#define MATH1(fname, expr)                                     \
    NATIVE(n_##fname) {                                        \
        double x = argNum(vm, args, argc, 0, #fname);          \
        return Value(expr);                                    \
    }
MATH1(abs, std::fabs(x))
MATH1(sqrt, std::sqrt(x))
MATH1(floor, std::floor(x))
MATH1(ceil, std::ceil(x))
MATH1(sin, std::sin(x))
MATH1(cos, std::cos(x))
MATH1(tan, std::tan(x))
MATH1(asin, std::asin(x))
MATH1(acos, std::acos(x))
MATH1(atan, std::atan(x))
MATH1(exp, std::exp(x))
MATH1(log10, std::log10(x))
MATH1(sign, (double)((x > 0) - (x < 0)))
MATH1(radians, x * M_PI / 180.0)
MATH1(degrees, x * 180.0 / M_PI)
NATIVE(n_log) {
    double x = argNum(vm, args, argc, 0, "log");
    if (argc > 1) return Value(std::log(x) / std::log(argNum(vm, args, argc, 1, "log")));
    return Value(std::log(x));
}
NATIVE(n_round) {
    double x = argNum(vm, args, argc, 0, "round");
    double d = argNum(vm, args, argc, 1, "round", 0);
    double m = std::pow(10.0, d);
    return Value(std::round(x * m) / m);
}
// format(valor, "formato"): el mismo miniformato que "${valor:formato}"
//   [[relleno]alineación][+][0][ancho][, o _][.precisión][tipo]
//   alineación < > ^ · tipos: f (decimales), e (científica), % (porcentaje), d (entero),
//   x X (hexadecimal), b (binario), o (octal), s (texto)
static size_t utf8CharLen(unsigned char c) { return c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1; }
static std::string groupDigits(const std::string& digits, char sep) {
    std::string out;
    int n = 0;
    for (size_t i = digits.size(); i-- > 0;) {
        if (n > 0 && n % 3 == 0) out += sep;
        out += digits[i];
        n++;
    }
    return std::string(out.rbegin(), out.rend());
}
NATIVE(n_format) {
    Value v = argc > 0 ? args[0] : Value();
    std::string spec = argStr(vm, args, argc, 1, "format", "");
    auto bad = [&](const std::string& why) {
        vm.raise("formato '" + spec + "' no válido: " + why + " (ejemplos: \".2f\", \">8\", \",\", \"08.3f\", \"%\", \"x\")");
    };
    size_t i = 0;
    std::string fill = " ";
    char align = 0;
    auto isAlign = [](char c) { return c == '<' || c == '>' || c == '^'; };
    if (!spec.empty()) {
        size_t cl = utf8CharLen((unsigned char)spec[0]);
        if (spec.size() > cl && isAlign(spec[cl])) { fill = spec.substr(0, cl); align = spec[cl]; i = cl + 1; }
        else if (isAlign(spec[0])) { align = spec[0]; i = 1; }
    }
    bool plus = false, zero = false;
    if (i < spec.size() && spec[i] == '+') { plus = true; i++; }
    if (i < spec.size() && spec[i] == '0') { zero = true; i++; }
    int width = 0;
    while (i < spec.size() && std::isdigit((unsigned char)spec[i])) { width = width * 10 + (spec[i] - '0'); i++; if (width > 1000) bad("ancho demasiado grande"); }
    char group = 0;
    if (i < spec.size() && (spec[i] == ',' || spec[i] == '_')) group = spec[i++];
    int prec = -1;
    if (i < spec.size() && spec[i] == '.') {
        i++;
        if (i >= spec.size() || !std::isdigit((unsigned char)spec[i])) bad("falta el número de decimales después del punto");
        prec = 0;
        while (i < spec.size() && std::isdigit((unsigned char)spec[i])) { prec = prec * 10 + (spec[i] - '0'); i++; if (prec > 60) bad("demasiados decimales"); }
    }
    char type = 0;
    if (i < spec.size()) type = spec[i++];
    if (i != spec.size()) bad("sobra '" + spec.substr(i) + "'");
    if (type && !std::strchr("feE%dxXbos", type)) bad(std::string("el tipo '") + type + "' no existe");

    std::string body, sign;
    bool numeric = v.isNum() && type != 's';
    if (type && type != 's' && !v.isNum()) vm.raise("el formato '" + spec + "' necesita un número, no " + vm.typeName(v));
    if (numeric) {
        double x = v.n;
        if (!std::isfinite(x)) body = formatNumber(x);
        else {
            bool neg = std::signbit(x) && x != 0;
            double a = std::fabs(x);
            char buf[512];
            switch (type) {
                case 'f': std::snprintf(buf, sizeof buf, "%.*f", prec < 0 ? 6 : prec, a); body = buf; break;
                case 'e': case 'E': std::snprintf(buf, sizeof buf, type == 'e' ? "%.*e" : "%.*E", prec < 0 ? 6 : prec, a); body = buf; break;
                case '%': std::snprintf(buf, sizeof buf, "%.*f", prec < 0 ? 0 : prec, a * 100); body = std::string(buf) + "%"; break;
                case 'd': std::snprintf(buf, sizeof buf, "%.0f", std::round(a)); body = buf; break;
                case 'x': case 'X': case 'b': case 'o': {
                    if (a >= 9.007199254740992e15) vm.raise("el número es demasiado grande para el formato '" + spec + "'");
                    unsigned long long u = (unsigned long long)std::llround(a);
                    if (type == 'b') { do { body.insert(body.begin(), char('0' + (u & 1))); u >>= 1; } while (u); }
                    else { std::snprintf(buf, sizeof buf, type == 'x' ? "%llx" : type == 'X' ? "%llX" : "%llo", u); body = buf; }
                    break;
                }
                default:
                    if (prec >= 0) { std::snprintf(buf, sizeof buf, "%.*f", prec, a); body = buf; }
                    else body = formatNumber(a);
            }
            if (neg && body.find_first_not_of("0.%") != std::string::npos) sign = "-";
            else if (plus) sign = "+";
            if (group && type != 'e' && type != 'E' && type != 'x' && type != 'X' && type != 'b' && type != 'o') {
                size_t end = body.find_first_not_of("0123456789");
                body = groupDigits(body.substr(0, end == std::string::npos ? body.size() : end), group) +
                       (end == std::string::npos ? "" : body.substr(end));
            }
        }
    } else {
        body = isString(v) ? vm.toString(v) : vm.toString(v);
        if (prec >= 0) { // ".5" en un texto: como mucho 5 caracteres
            size_t pos = 0;
            for (int k = 0; k < prec && pos < body.size(); k++) pos += utf8CharLen((unsigned char)body[pos]);
            body = body.substr(0, pos);
        }
    }
    std::string out = sign + body;
    int len = (int)utf8Length(out);
    if (width > len) {
        int pad = width - len;
        if (zero && numeric && !align) out = sign + std::string(pad, '0') + body;
        else {
            char al = align ? align : (numeric ? '>' : '<');
            auto rep = [&](int n) { std::string r; for (int k = 0; k < n; k++) r += fill; return r; };
            if (al == '<') out += rep(pad);
            else if (al == '>') out = rep(pad) + out;
            else out = rep(pad / 2) + out + rep(pad - pad / 2);
        }
    }
    return vm.str(out);
}
NATIVE(n_pow) { return Value(std::pow(argNum(vm, args, argc, 0, "pow"), argNum(vm, args, argc, 1, "pow"))); }
NATIVE(n_atan2) { return Value(std::atan2(argNum(vm, args, argc, 0, "atan2"), argNum(vm, args, argc, 1, "atan2"))); }
NATIVE(n_hypot) { return Value(std::hypot(argNum(vm, args, argc, 0, "hypot"), argNum(vm, args, argc, 1, "hypot"))); }
static Value minmax(VM& vm, int argc, Value* args, bool isMax, const char* name) {
    std::vector<Value> vals;
    if (argc == 1 && isObjType(args[0], OT::List)) vals = static_cast<ObjList*>(args[0].o)->items;
    else vals.assign(args, args + argc);
    if (vals.empty()) vm.raise(std::string(name) + ": necesita al menos un valor");
    Value best = vals[0];
    for (auto& v : vals) {
        if (!v.isNum()) vm.raise(std::string(name) + ": todos los valores deben ser números, no " + vm.typeName(v));
        if (isMax ? v.n > best.n : v.n < best.n) best = v;
    }
    return best;
}
NATIVE(n_min) { return minmax(vm, argc, args, false, "min"); }
NATIVE(n_max) { return minmax(vm, argc, args, true, "max"); }
NATIVE(n_clamp) {
    double x = argNum(vm, args, argc, 0, "clamp"), a = argNum(vm, args, argc, 1, "clamp"), b = argNum(vm, args, argc, 2, "clamp");
    if (a > b) std::swap(a, b);
    return Value(std::clamp(x, a, b));
}
NATIVE(n_lerp) {
    double a = argNum(vm, args, argc, 0, "lerp"), b = argNum(vm, args, argc, 1, "lerp"), t = argNum(vm, args, argc, 2, "lerp");
    return Value(a + (b - a) * t);
}
NATIVE(n_random) {
    if (argc == 0) return Value(rand01());
    double a = argNum(vm, args, argc, 0, "random"), b = argNum(vm, args, argc, 1, "random", NAN);
    if (argc < 2) { b = a; a = 0; }
    return Value(a + (b - a) * rand01());
}
NATIVE(n_randomInt) {
    double a = argNum(vm, args, argc, 0, "randomInt"), b = argNum(vm, args, argc, 1, "randomInt");
    long long lo = (long long)std::ceil(std::min(a, b)), hi = (long long)std::floor(std::max(a, b));
    return Value((double)randomBetween(lo, hi));
}
NATIVE(n_randomChoice) {
    ObjList* l = argList(vm, args, argc, 0, "randomChoice");
    if (l->items.empty()) return Value();
    return l->items[(size_t)randomBetween(0, (long long)l->items.size() - 1)];
}
NATIVE(n_randomSeed) { rng().seed((uint64_t)argNum(vm, args, argc, 0, "randomSeed")); return Value(); }

// Error
NATIVE(n_Error) {
    ObjInstance* e = vm.newInstance(vm.errorClass);
    vm.tempRoots.push_back(Value::object(e));
    vm.errorInit(e, argc > 0 ? args[0] : Value());
    if (argc > 1 && isString(args[1])) e->fields[1] = args[1];
    vm.tempRoots.pop_back();
    return Value::object(e);
}

// ---------------------------------------------------------------------------
// text
// ---------------------------------------------------------------------------

static const std::string& S(Value* args) { return asString(args[-1])->str; }

NATIVE(t_length) { UNUSED; return Value((double)utf8Length(S(args))); }
NATIVE(t_upper) {
    UNUSED;
    std::string r = S(args);
    for (auto& c : r) c = (char)std::toupper((unsigned char)c);
    // Vocales acentuadas y ñ comunes (UTF-8 de 2 bytes, latin-1 suplementario)
    for (size_t i = 0; i + 1 < r.size(); i++)
        if ((unsigned char)r[i] == 0xC3 && (unsigned char)r[i + 1] >= 0xA0 && (unsigned char)r[i + 1] <= 0xBE && (unsigned char)r[i + 1] != 0xB7) r[i + 1] = (char)((unsigned char)r[i + 1] - 0x20);
    return vm.str(r);
}
NATIVE(t_lower) {
    UNUSED;
    std::string r = S(args);
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    for (size_t i = 0; i + 1 < r.size(); i++)
        if ((unsigned char)r[i] == 0xC3 && (unsigned char)r[i + 1] >= 0x80 && (unsigned char)r[i + 1] <= 0x9E && (unsigned char)r[i + 1] != 0x97) r[i + 1] = (char)((unsigned char)r[i + 1] + 0x20);
    return vm.str(r);
}
NATIVE(t_trim) { UNUSED; return vm.str(trim(S(args))); }
NATIVE(t_trimStart) { UNUSED; const auto& s = S(args); size_t a = 0; while (a < s.size() && std::isspace((unsigned char)s[a])) a++; return vm.str(s.substr(a)); }
NATIVE(t_trimEnd) { UNUSED; const auto& s = S(args); size_t b = s.size(); while (b > 0 && std::isspace((unsigned char)s[b - 1])) b--; return vm.str(s.substr(0, b)); }
NATIVE(t_split) {
    const std::string& s = S(args);
    ObjList* out = vm.newList();
    vm.tempRoots.push_back(Value::object(out));
    if (argc == 0 || args[0].isNull()) {
        // por espacios
        size_t i = 0;
        while (i < s.size()) {
            while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
            size_t j = i;
            while (j < s.size() && !std::isspace((unsigned char)s[j])) j++;
            if (j > i) out->items.push_back(vm.str(s.substr(i, j - i)));
            i = j;
        }
    } else {
        std::string sep = argStr(vm, args, argc, 0, "split");
        if (sep.empty()) {
            for (size_t i = 0; i < s.size();) { size_t a = i; utf8Decode(s, i); out->items.push_back(vm.str(s.substr(a, i - a))); }
        } else {
            size_t start = 0, pos;
            while ((pos = s.find(sep, start)) != std::string::npos) { out->items.push_back(vm.str(s.substr(start, pos - start))); start = pos + sep.size(); }
            out->items.push_back(vm.str(s.substr(start)));
        }
    }
    vm.tempRoots.pop_back();
    return Value::object(out);
}
NATIVE(t_contains) { return Value::boolean(S(args).find(argStr(vm, args, argc, 0, "contains")) != std::string::npos); }
NATIVE(t_startsWith) { return Value::boolean(startsWith(S(args), argStr(vm, args, argc, 0, "startsWith"))); }
NATIVE(t_endsWith) { return Value::boolean(endsWith(S(args), argStr(vm, args, argc, 0, "endsWith"))); }
NATIVE(t_replace) {
    std::string s = S(args), a = argStr(vm, args, argc, 0, "replace"), b = argStr(vm, args, argc, 1, "replace");
    if (a.empty()) return args[-1];
    std::string r;
    size_t start = 0, pos;
    while ((pos = s.find(a, start)) != std::string::npos) { r += s.substr(start, pos - start); r += b; start = pos + a.size(); }
    r += s.substr(start);
    return vm.str(r);
}
static double byteToChar(const std::string& s, size_t byte) { return byte == std::string::npos ? -1 : (double)utf8Length(std::string_view(s).substr(0, byte)); }
NATIVE(t_indexOf) {
    const auto& s = S(args);
    std::string sub = argStr(vm, args, argc, 0, "indexOf");
    double from = argNum(vm, args, argc, 1, "indexOf", 0);
    size_t start = from <= 0 ? 0 : utf8Offset(s, (size_t)from); // posición en caracteres, no en bytes
    if (start > s.size()) return Value(-1);
    return Value(byteToChar(s, s.find(sub, start)));
}
NATIVE(t_lastIndexOf) { const auto& s = S(args); return Value(byteToChar(s, s.rfind(argStr(vm, args, argc, 0, "lastIndexOf")))); }
NATIVE(t_substring) {
    const auto& s = S(args);
    long len = (long)utf8Length(s);
    long a = (long)argNum(vm, args, argc, 0, "substring"), b = (long)argNum(vm, args, argc, 1, "substring", (double)len);
    if (a < 0) a += len;
    if (b < 0) b += len;
    a = std::clamp(a, 0L, len);
    b = std::clamp(b, a, len);
    size_t sa = utf8Offset(s, a), sb = utf8Offset(s, b);
    return vm.str(s.substr(sa, sb - sa));
}
NATIVE(t_repeat) {
    std::string r;
    int n = (int)argNum(vm, args, argc, 0, "repeat");
    for (int i = 0; i < n; i++) r += S(args);
    return vm.str(r);
}
static Value padImpl(VM& vm, int argc, Value* args, bool start) {
    const auto& s = S(args);
    long n = (long)argNum(vm, args, argc, 0, "pad");
    std::string ch = argStr(vm, args, argc, 1, "pad", " ");
    if (ch.empty()) ch = " ";
    long len = (long)utf8Length(s);
    std::string pad;
    while ((long)utf8Length(pad) < n - len) pad += ch;
    return vm.str(start ? pad + s : s + pad);
}
NATIVE(t_padStart) { return padImpl(vm, argc, args, true); }
NATIVE(t_padEnd) { return padImpl(vm, argc, args, false); }
NATIVE(t_chars) {
    UNUSED;
    const auto& s = S(args);
    ObjList* out = vm.newList();
    vm.tempRoots.push_back(Value::object(out));
    for (size_t i = 0; i < s.size();) { size_t a = i; utf8Decode(s, i); out->items.push_back(vm.str(s.substr(a, i - a))); }
    vm.tempRoots.pop_back();
    return Value::object(out);
}
NATIVE(t_lines) {
    UNUSED;
    ObjList* out = vm.newList();
    vm.tempRoots.push_back(Value::object(out));
    for (auto& l : splitLines(S(args))) out->items.push_back(vm.str(l));
    vm.tempRoots.pop_back();
    return Value::object(out);
}
NATIVE(t_isEmpty) { UNUSED; return Value::boolean(S(args).empty()); }
NATIVE(t_toNumber) { UNUSED; Value a[2] = {args[-1], argc > 0 ? args[0] : Value()}; return n_toNumber(vm, argc > 0 ? 2 : 1, a); }
NATIVE(t_reverse) {
    UNUSED;
    const auto& s = S(args);
    std::vector<std::string> cs;
    for (size_t i = 0; i < s.size();) { size_t a = i; utf8Decode(s, i); cs.push_back(s.substr(a, i - a)); }
    std::string r;
    for (auto it = cs.rbegin(); it != cs.rend(); ++it) r += *it;
    return vm.str(r);
}
NATIVE(t_count) {
    const auto& s = S(args);
    std::string sub = argStr(vm, args, argc, 0, "count");
    if (sub.empty()) return Value(0.0);
    double n = 0;
    for (size_t p = s.find(sub); p != std::string::npos; p = s.find(sub, p + sub.size())) n++;
    return Value(n);
}

// ---------------------------------------------------------------------------
// list
// ---------------------------------------------------------------------------

static ObjList* L(Value* args) { return static_cast<ObjList*>(args[-1].o); }

NATIVE(l_length) { UNUSED; return Value((double)L(args)->items.size()); }
NATIVE(l_push) { UNUSED; for (int i = 0; i < argc; i++) L(args)->items.push_back(args[i]); return args[-1]; }
NATIVE(l_pop) {
    UNUSED;
    auto& it = L(args)->items;
    if (it.empty()) return Value();
    Value v = it.back();
    it.pop_back();
    return v;
}
NATIVE(l_shift) {
    UNUSED;
    auto& it = L(args)->items;
    if (it.empty()) return Value();
    Value v = it.front();
    it.erase(it.begin());
    return v;
}
NATIVE(l_insert) {
    auto& it = L(args)->items;
    long i = (long)argNum(vm, args, argc, 0, "insert");
    if (i < 0) i += (long)it.size() + 1;
    i = std::clamp(i, 0L, (long)it.size());
    it.insert(it.begin() + i, argc > 1 ? args[1] : Value());
    return args[-1];
}
NATIVE(l_removeAt) {
    auto& it = L(args)->items;
    long i = (long)argNum(vm, args, argc, 0, "removeAt");
    if (i < 0) i += (long)it.size();
    if (i < 0 || i >= (long)it.size()) vm.raise("removeAt: índice fuera de rango (" + std::to_string(i) + ")");
    Value v = it[i];
    it.erase(it.begin() + i);
    return v;
}
NATIVE(l_remove) {
    UNUSED;
    auto& it = L(args)->items;
    for (size_t i = 0; i < it.size(); i++) if (valuesEqual(it[i], args[0])) { it.erase(it.begin() + i); return Value::boolean(true); }
    return Value::boolean(false);
}
NATIVE(l_contains) { UNUSED; for (auto& v : L(args)->items) if (valuesEqual(v, args[0])) return Value::boolean(true); return Value::boolean(false); }
NATIVE(l_indexOf) { UNUSED; auto& it = L(args)->items; for (size_t i = 0; i < it.size(); i++) if (valuesEqual(it[i], args[0])) return Value((double)i); return Value(-1.0); }
NATIVE(l_clear) { UNUSED; L(args)->items.clear(); return args[-1]; }
NATIVE(l_isEmpty) { UNUSED; return Value::boolean(L(args)->items.empty()); }
NATIVE(l_first) { UNUSED; auto& it = L(args)->items; return it.empty() ? Value() : it.front(); }
NATIVE(l_last) { UNUSED; auto& it = L(args)->items; return it.empty() ? Value() : it.back(); }
NATIVE(l_copy) { UNUSED; ObjList* r = vm.newList(); r->items = L(args)->items; return Value::object(r); }
NATIVE(l_reverse) { UNUSED; std::reverse(L(args)->items.begin(), L(args)->items.end()); return args[-1]; }
NATIVE(l_reversed) { UNUSED; ObjList* r = vm.newList(); r->items.assign(L(args)->items.rbegin(), L(args)->items.rend()); return Value::object(r); }
NATIVE(l_slice) {
    auto& it = L(args)->items;
    long n = (long)it.size();
    long a = (long)argNum(vm, args, argc, 0, "slice", 0), b = (long)argNum(vm, args, argc, 1, "slice", (double)n);
    if (a < 0) a += n;
    if (b < 0) b += n;
    a = std::clamp(a, 0L, n);
    b = std::clamp(b, a, n);
    ObjList* r = vm.newList();
    r->items.assign(it.begin() + a, it.begin() + b);
    return Value::object(r);
}
NATIVE(l_join) {
    std::string sep = argStr(vm, args, argc, 0, "join", ", ");
    std::string s;
    auto items = L(args)->items;
    for (size_t i = 0; i < items.size(); i++) { if (i) s += sep; s += vm.toString(items[i]); }
    return vm.str(s);
}
NATIVE(l_shuffle) {
    UNUSED;
    auto& it = L(args)->items; // Fisher-Yates con randomBetween: mismo orden en todos los sistemas
    for (size_t i = it.size(); i > 1; i--) std::swap(it[i - 1], it[(size_t)randomBetween(0, (long long)i - 1)]);
    return args[-1];
}
NATIVE(l_random) {
    UNUSED;
    auto& it = L(args)->items;
    if (it.empty()) return Value();
    return it[(size_t)randomBetween(0, (long long)it.size() - 1)];
}
NATIVE(l_sum) {
    UNUSED;
    double s = 0;
    for (auto& v : L(args)->items) { if (!v.isNum()) vm.raise("sum: la lista contiene " + vm.typeName(v)); s += v.n; }
    return Value(s);
}
NATIVE(l_min) { UNUSED; auto it = L(args)->items; if (it.empty()) return Value(); ObjList tmp; tmp.items = it; Value a = Value::object(&tmp); return minmax(vm, 1, &a, false, "min"); }
NATIVE(l_max) { UNUSED; auto it = L(args)->items; if (it.empty()) return Value(); ObjList tmp; tmp.items = it; Value a = Value::object(&tmp); return minmax(vm, 1, &a, true, "max"); }

// Métodos con funciones de orden superior: copiamos los datos antes de llamar al script.
NATIVE(l_map) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    ObjList* out = vm.newList();
    vm.tempRoots.push_back(Value::object(out));
    out->items.reserve(items.size());
    for (size_t i = 0; i < items.size(); i++) {
        Value a[2] = {items[i], Value((double)i)};
        out->items.push_back(vm.call(fn, 1, a));
    }
    vm.tempRoots.pop_back();
    return Value::object(out);
}
NATIVE(l_filter) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    ObjList* out = vm.newList();
    vm.tempRoots.push_back(Value::object(out));
    for (auto& v : items) if (!isFalsy(vm.call(fn, 1, &v))) out->items.push_back(v);
    vm.tempRoots.pop_back();
    return Value::object(out);
}
NATIVE(l_reduce) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    size_t start = 0;
    Value acc;
    if (argc > 1) acc = args[1];
    else { if (items.empty()) vm.raise("reduce: la lista está vacía y no hay valor inicial"); acc = items[0]; start = 1; }
    vm.tempRoots.push_back(acc);
    for (size_t i = start; i < items.size(); i++) {
        Value a[2] = {acc, items[i]};
        acc = vm.call(fn, 2, a);
        vm.tempRoots.back() = acc;
    }
    vm.tempRoots.pop_back();
    return acc;
}
NATIVE(l_find) { UNUSED; Value fn = args[0]; std::vector<Value> items = L(args)->items; for (auto& v : items) if (!isFalsy(vm.call(fn, 1, &v))) return v; return Value(); }
NATIVE(l_findIndex) { UNUSED; Value fn = args[0]; std::vector<Value> items = L(args)->items; for (size_t i = 0; i < items.size(); i++) if (!isFalsy(vm.call(fn, 1, &items[i]))) return Value((double)i); return Value(-1.0); }
NATIVE(l_any) { UNUSED; Value fn = args[0]; std::vector<Value> items = L(args)->items; for (auto& v : items) if (!isFalsy(vm.call(fn, 1, &v))) return Value::boolean(true); return Value::boolean(false); }
NATIVE(l_all) { UNUSED; Value fn = args[0]; std::vector<Value> items = L(args)->items; for (auto& v : items) if (isFalsy(vm.call(fn, 1, &v))) return Value::boolean(false); return Value::boolean(true); }
NATIVE(l_count) {
    UNUSED;
    std::vector<Value> items = L(args)->items;
    if (argc == 0) return Value((double)items.size());
    double n = 0;
    for (auto& v : items) if (!isFalsy(vm.call(args[0], 1, &v))) n++;
    return Value(n);
}
NATIVE(l_forEach) {
    UNUSED;
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    for (size_t i = 0; i < items.size(); i++) { Value a[2] = {items[i], Value((double)i)}; vm.call(fn, 1, a); }
    return Value();
}
static bool defaultLess(VM& vm, const Value& a, const Value& b) {
    if (a.isNum() && b.isNum()) return a.n < b.n;
    if (isString(a) && isString(b)) return asString(a)->str < asString(b)->str;
    vm.raise("sort: no se pueden comparar " + vm.typeName(a) + " y " + vm.typeName(b) + " (pasa una función de comparación)");
}
NATIVE(l_sort) {
    std::vector<Value> items = L(args)->items;
    if (argc > 0 && !args[0].isNull()) {
        Value fn = args[0];
        std::stable_sort(items.begin(), items.end(), [&](const Value& a, const Value& b) {
            Value p[2] = {a, b};
            Value r = vm.call(fn, 2, p);
            if (r.isBool()) return r.b;
            if (!r.isNum()) vm.raise("sort: la función de comparación debe devolver un número o un bool");
            return r.n < 0;
        });
    } else {
        std::stable_sort(items.begin(), items.end(), [&](const Value& a, const Value& b) { return defaultLess(vm, a, b); });
    }
    L(args)->items = items;
    return args[-1];
}
NATIVE(l_sortBy) {
    UNUSED;
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    std::vector<std::pair<Value, Value>> keyed;
    ObjList* keep = vm.newList();
    vm.tempRoots.push_back(Value::object(keep));
    for (auto& v : items) { Value k = vm.call(fn, 1, &v); keep->items.push_back(k); keyed.push_back({k, v}); }
    std::stable_sort(keyed.begin(), keyed.end(), [&](auto& a, auto& b) { return defaultLess(vm, a.first, b.first); });
    vm.tempRoots.pop_back();
    for (size_t i = 0; i < items.size(); i++) items[i] = keyed[i].second;
    L(args)->items = items;
    return args[-1];
}
NATIVE(l_unique) {
    UNUSED;
    ObjList* r = vm.newList();
    for (auto& v : L(args)->items) {
        bool seen = false;
        for (auto& w : r->items) if (valuesEqual(v, w)) { seen = true; break; }
        if (!seen) r->items.push_back(v);
    }
    return Value::object(r);
}

// ---------------------------------------------------------------------------
// map / set
// ---------------------------------------------------------------------------

static ObjMap* M(Value* args) { return static_cast<ObjMap*>(args[-1].o); }

NATIVE(m_length) { UNUSED; return Value((double)M(args)->live); }
NATIVE(m_keys) { UNUSED; ObjList* r = vm.newList(); for (auto& e : M(args)->entries) if (e.alive) r->items.push_back(e.key); return Value::object(r); }
NATIVE(m_values) { UNUSED; ObjList* r = vm.newList(); for (auto& e : M(args)->entries) if (e.alive) r->items.push_back(M(args)->isSet ? e.key : e.value); return Value::object(r); }
NATIVE(m_entries) {
    UNUSED;
    ObjList* r = vm.newList();
    vm.tempRoots.push_back(Value::object(r));
    for (auto& e : M(args)->entries) {
        if (!e.alive) continue;
        ObjList* pair = vm.newList();
        pair->items = {e.key, e.value};
        r->items.push_back(Value::object(pair));
    }
    vm.tempRoots.pop_back();
    return Value::object(r);
}
NATIVE(m_has) { UNUSED; return Value::boolean(M(args)->find(args[0]) != nullptr); }
NATIVE(m_get) { UNUSED; Value* v = M(args)->find(args[0]); return v ? *v : (argc > 1 ? args[1] : Value()); }
NATIVE(m_set) { if (args[0].isNull()) vm.raise("La clave de un mapa no puede ser null"); M(args)->set(args[0], argc > 1 ? args[1] : Value()); return args[-1]; }
NATIVE(m_remove) { UNUSED; return Value::boolean(M(args)->remove(args[0])); }
NATIVE(m_clear) { UNUSED; M(args)->clear(); return args[-1]; }
NATIVE(m_isEmpty) { UNUSED; return Value::boolean(M(args)->live == 0); }
NATIVE(m_copy) {
    UNUSED;
    ObjMap* r = vm.newMap(M(args)->isSet);
    for (auto& e : M(args)->entries) if (e.alive) r->set(e.key, e.value);
    return Value::object(r);
}
NATIVE(m_forEach) {
    UNUSED;
    Value fn = args[0];
    std::vector<ObjMap::Entry> entries = M(args)->entries;
    for (auto& e : entries) if (e.alive) { Value a[2] = {e.key, e.value}; vm.call(fn, M(args)->isSet ? 1 : 2, a); }
    return Value();
}
NATIVE(m_merge) {
    ObjMap* other = argMap(vm, args, argc, 0, "merge");
    for (auto& e : other->entries) if (e.alive) M(args)->set(e.key, e.value);
    return args[-1];
}
NATIVE(s_add) { UNUSED; for (int i = 0; i < argc; i++) M(args)->set(args[i], Value::boolean(true)); return args[-1]; }
NATIVE(s_toList) { UNUSED; ObjList* r = vm.newList(); for (auto& e : M(args)->entries) if (e.alive) r->items.push_back(e.key); return Value::object(r); }
NATIVE(n_set) {
    ObjMap* s = vm.newMap(true);
    vm.tempRoots.push_back(Value::object(s));
    if (argc == 1 && isObjType(args[0], OT::List)) for (auto& v : static_cast<ObjList*>(args[0].o)->items) s->set(v, Value::boolean(true));
    else for (int i = 0; i < argc; i++) s->set(args[i], Value::boolean(true));
    vm.tempRoots.pop_back();
    return Value::object(s);
}

// ---------------------------------------------------------------------------
// number, Range, Task, Event, Subscription, function, Enum
// ---------------------------------------------------------------------------

NATIVE(num_toFixed) {
    double x = args[-1].n;
    int d = (int)argNum(vm, args, argc, 0, "toFixed", 0);
    d = std::clamp(d, 0, 20);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", d, x);
    return vm.str(buf);
}
NATIVE(num_isInteger) { UNUSED; double x = args[-1].n; return Value::boolean(std::isfinite(x) && std::floor(x) == x); }
NATIVE(num_toText) { UNUSED; return vm.str(formatNumber(args[-1].n)); }

static ObjRange* R(Value* args) { return static_cast<ObjRange*>(args[-1].o); }
NATIVE(r_start) { UNUSED; return Value(R(args)->start); }
NATIVE(r_end) { UNUSED; return Value(R(args)->end); }
NATIVE(r_step) { UNUSED; return Value(R(args)->step); }
NATIVE(r_length) { UNUSED; return Value(R(args)->count()); }
NATIVE(r_toList) {
    UNUSED;
    ObjRange* r = R(args);
    ObjList* l = vm.newList();
    double n = r->count();
    if (n > 1e8) vm.raise("toList: el rango es demasiado grande");
    for (double k = 0; k < n; k++) l->items.push_back(Value(r->start + k * r->step));
    return Value::object(l);
}
NATIVE(r_contains) { UNUSED; return Value::boolean(vm.contains(args[-1], args[0])); }

static ObjTask* T(Value* args) { return static_cast<ObjTask*>(args[-1].o); }
NATIVE(task_done) { UNUSED; return Value::boolean(T(args)->state != TaskState::Pending); }
NATIVE(task_failed) { UNUSED; return Value::boolean(T(args)->state == TaskState::Failed); }
NATIVE(task_result) { UNUSED; return T(args)->result; }
NATIVE(task_error) { UNUSED; return T(args)->error; }
NATIVE(task_cancel) {
    UNUSED;
    ObjTask* t = T(args);
    if (t->state != TaskState::Pending) return Value::boolean(false);
    t->cancelRequested = true;
    t->state = TaskState::Cancelled;
    ObjFiber* f = t->fiber;
    if (f && f->state == FiberState::Suspended && f->waitingOn) {
        auto& w = f->waitingOn->waiters;
        w.erase(std::remove(w.begin(), w.end(), f), w.end());
        f->state = FiberState::Done;
        f->frames.clear();
        f->sp = 0;
    }
    vm.wakeWaiters(t);
    return Value::boolean(true);
}
NATIVE(task_then) {
    ObjTask* t = T(args);
    if (t->state == TaskState::Done) { Value r = t->result; vm.call(args[0], 1, &r); }
    else if (t->state == TaskState::Pending) t->callbacks.push_back(args[0]);
    return args[-1];
}

NATIVE(ev_name) { UNUSED; return Value::object(static_cast<ObjEvent*>(args[-1].o)->name); }
NATIVE(ev_count) {
    UNUSED;
    double n = 0;
    for (auto* s : static_cast<ObjEvent*>(args[-1].o)->subs) if (s->active) n++;
    return Value(n);
}
NATIVE(ev_clear) { Value e = args[-1]; return n_off(vm, 1, &e); }
NATIVE(sub_cancel) { UNUSED; auto* s = static_cast<ObjSubscription*>(args[-1].o); s->active = false; s->event->needsCleanup = true; return Value(); }
NATIVE(sub_active) { UNUSED; return Value::boolean(static_cast<ObjSubscription*>(args[-1].o)->active); }

NATIVE(fn_name) {
    UNUSED;
    Value f = args[-1];
    if (isObjType(f, OT::Closure)) return Value::object(static_cast<ObjClosure*>(f.o)->fn->name);
    if (isObjType(f, OT::Native)) return Value::object(static_cast<ObjNative*>(f.o)->name);
    return vm.str("<método>");
}
NATIVE(fn_call) { UNUSED; return vm.call(args[-1], argc, args); }

static ObjEnum* E(Value* args) { return static_cast<ObjEnum*>(args[-1].o); }
NATIVE(en_values) { UNUSED; ObjList* l = vm.newList(); for (auto* v : E(args)->values) l->items.push_back(Value::object(v)); return Value::object(l); }
NATIVE(en_count) { UNUSED; return Value((double)E(args)->values.size()); }
NATIVE(en_name) { UNUSED; return Value::object(E(args)->name); }
NATIVE(en_fromName) {
    std::string n = argStr(vm, args, argc, 0, "fromName");
    for (auto* v : E(args)->values) if (v->name->str == n) return Value::object(v);
    return Value();
}
NATIVE(en_fromValue) { UNUSED; for (auto* v : E(args)->values) if (valuesEqual(v->value, args[0])) return Value::object(v); return Value(); }

NATIVE(err_toString) {
    UNUSED;
    auto* e = static_cast<ObjInstance*>(args[-1].o);
    std::string kind = isString(e->fields[1]) ? asString(e->fields[1])->str : "Error";
    return vm.str(kind + ": " + (isString(e->fields[0]) ? asString(e->fields[0])->str : std::string()));
}

NATIVE(evd_stop) { UNUSED; static_cast<ObjInstance*>(args[-1].o)->fields[3] = Value::boolean(true); return Value(); }
NATIVE(evd_prevent) { UNUSED; static_cast<ObjInstance*>(args[-1].o)->fields[4] = Value::boolean(true); return Value(); }

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

static void jsonWrite(VM& vm, const Value& v, std::string& out, int indent, int depth) {
    auto nl = [&](int d) { if (indent > 0) { out += '\n'; out += std::string(d * indent, ' '); } };
    if (depth > 64) vm.raise("JSON.stringify: estructura demasiado profunda (¿referencia circular?)");
    switch (v.t) {
        case VT::Null: case VT::Undef: out += "null"; return;
        case VT::Bool: out += v.b ? "true" : "false"; return;
        case VT::Num: out += std::isfinite(v.n) ? formatNumber(v.n) : "null"; return;
        case VT::Obj: break;
    }
    if (isString(v)) {
        out += '"';
        for (unsigned char c : asString(v)->str) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                    else out += (char)c;
            }
        }
        out += '"';
        return;
    }
    if (isObjType(v, OT::List)) {
        auto& items = static_cast<ObjList*>(v.o)->items;
        out += '[';
        for (size_t i = 0; i < items.size(); i++) { if (i) out += ','; nl(depth + 1); jsonWrite(vm, items[i], out, indent, depth + 1); }
        if (!items.empty()) nl(depth);
        out += ']';
        return;
    }
    if (isObjType(v, OT::Map)) {
        auto* m = static_cast<ObjMap*>(v.o);
        if (m->isSet) { ObjList tmp; for (auto& e : m->entries) if (e.alive) tmp.items.push_back(e.key); jsonWrite(vm, Value::object(&tmp), out, indent, depth); return; }
        out += '{';
        bool first = true;
        for (auto& e : m->entries) {
            if (!e.alive) continue;
            if (!first) out += ',';
            first = false;
            nl(depth + 1);
            jsonWrite(vm, isString(e.key) ? e.key : vm.str(vm.toString(e.key)), out, indent, depth + 1);
            out += indent > 0 ? ": " : ":";
            jsonWrite(vm, e.value, out, indent, depth + 1);
        }
        if (!first) nl(depth);
        out += '}';
        return;
    }
    if (isObjType(v, OT::Instance)) {
        auto* inst = static_cast<ObjInstance*>(v.o);
        out += '{';
        for (size_t i = 0; i < inst->fields.size(); i++) {
            if (i) out += ',';
            nl(depth + 1);
            jsonWrite(vm, Value::object(inst->cls->fieldNames[i]), out, indent, depth + 1);
            out += indent > 0 ? ": " : ":";
            jsonWrite(vm, inst->fields[i], out, indent, depth + 1);
        }
        if (!inst->fields.empty()) nl(depth);
        out += '}';
        return;
    }
    if (isObjType(v, OT::Vec)) {
        auto* vec = static_cast<ObjVec*>(v.o);
        out += '[';
        for (int i = 0; i < vec->dims(); i++) { if (i) out += ','; out += formatNumber(vec->v[i]); }
        out += ']';
        return;
    }
    if (isObjType(v, OT::EnumValue)) { jsonWrite(vm, Value::object(static_cast<ObjEnumValue*>(v.o)->name), out, indent, depth); return; }
    out += "null";
}

struct JsonReader {
    VM& vm;
    const std::string& s;
    size_t i = 0;
    [[noreturn]] void fail(const std::string& m) { vm.raise("JSON.parse: " + m + " (posición " + std::to_string(i) + ")"); }
    void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) i++; }
    Value parse() {
        ws();
        if (i >= s.size()) fail("fin inesperado");
        char c = s[i];
        if (c == '{') {
            i++;
            ObjMap* m = vm.newMap();
            vm.tempRoots.push_back(Value::object(m));
            ws();
            if (i < s.size() && s[i] == '}') { i++; vm.tempRoots.pop_back(); return Value::object(m); }
            for (;;) {
                ws();
                if (i >= s.size() || s[i] != '"') fail("se esperaba una clave entre comillas");
                Value k = parse();
                vm.tempRoots.push_back(k);
                ws();
                if (i >= s.size() || s[i] != ':') fail("se esperaba ':'");
                i++;
                Value v = parse();
                m->set(k, v);
                vm.tempRoots.pop_back();
                ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == '}') { i++; break; }
                fail("se esperaba ',' o '}'");
            }
            vm.tempRoots.pop_back();
            return Value::object(m);
        }
        if (c == '[') {
            i++;
            ObjList* l = vm.newList();
            vm.tempRoots.push_back(Value::object(l));
            ws();
            if (i < s.size() && s[i] == ']') { i++; vm.tempRoots.pop_back(); return Value::object(l); }
            for (;;) {
                l->items.push_back(parse());
                ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == ']') { i++; break; }
                fail("se esperaba ',' o ']'");
            }
            vm.tempRoots.pop_back();
            return Value::object(l);
        }
        if (c == '"') {
            i++;
            std::string out;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    i++;
                    switch (s[i]) {
                        case 'n': out += '\n'; break;
                        case 't': out += '\t'; break;
                        case 'r': out += '\r'; break;
                        case 'b': out += '\b'; break;
                        case 'f': out += '\f'; break;
                        case 'u': {
                            if (i + 4 >= s.size()) fail("escape \\u incompleto");
                            uint32_t cp = (uint32_t)std::strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16);
                            i += 4;
                            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < s.size() && s[i + 1] == '\\' && s[i + 2] == 'u') {
                                uint32_t lo = (uint32_t)std::strtoul(s.substr(i + 3, 4).c_str(), nullptr, 16);
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                i += 6;
                            }
                            utf8Append(out, cp);
                            break;
                        }
                        default: out += s[i];
                    }
                    i++;
                } else out += s[i++];
            }
            if (i >= s.size()) fail("texto sin cerrar");
            i++;
            return vm.str(out);
        }
        if (s.compare(i, 4, "true") == 0) { i += 4; return Value::boolean(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return Value::boolean(false); }
        if (s.compare(i, 4, "null") == 0) { i += 4; return Value(); }
        if (c == '-' || std::isdigit((unsigned char)c)) {
            char* end = nullptr;
            double d = std::strtod(s.c_str() + i, &end);
            i = end - s.c_str();
            return Value(d);
        }
        fail(std::string("carácter inesperado '") + c + "'");
    }
};

NATIVE(json_stringify) {
    std::string out;
    int indent = argc > 1 && args[1].isNum() ? (int)args[1].n : 0;
    jsonWrite(vm, args[0], out, indent, 0);
    return vm.str(out);
}
NATIVE(json_parse) {
    std::string s = argStr(vm, args, argc, 0, "JSON.parse");
    JsonReader r{vm, s};
    Value v = r.parse();
    r.ws();
    if (r.i < s.size()) r.fail("sobra texto después del JSON");
    return v;
}

// ---------------------------------------------------------------------------
// Registro
// ---------------------------------------------------------------------------

void registerStdlib(VM& vm) {
    // Globales
    vm.defineGlobalFn("print", "(...valores: any): void", "Muestra valores en la consola separados por espacios, con salto de línea.", n_print, "print(\"Hola\", nombre)");
    vm.defineGlobalFn("println", "(...valores: any): void", "Igual que print.", n_print);
    vm.defineGlobalFn("write", "(...valores: any): void", "Muestra valores sin salto de línea.", n_write);
    vm.defineGlobalFn("input", "(mensaje?: text): text?", "Lee una línea escrita por el usuario en la consola.", n_input, "var nombre = input(\"¿Cómo te llamas? \")");
    vm.defineGlobalFn("len", "(valor: any): number", "Longitud de un texto, lista, mapa, conjunto o rango.", n_len);
    vm.defineGlobalFn("typeof", "(valor: any): text", "Nombre del tipo de un valor (\"number\", \"text\", \"list\", nombre de clase...).", n_typeof);
    vm.defineGlobalFn("str", "(valor: any): text", "Convierte cualquier valor en texto.", n_str);
    vm.defineGlobalFn("toNumber", "(valor: any, porDefecto?: any): number?", "Convierte texto o bool en número. Devuelve porDefecto (o null) si no es posible.", n_toNumber);
    vm.defineGlobalFn("toInt", "(valor: number): int", "Quita los decimales de un número.", n_toInt);
    vm.defineGlobalFn("assert", "(condicion: any, mensaje?: text): void", "Lanza un error si la condición es falsa.", n_assert);
    vm.defineGlobalFn("exit", "(codigo?: number): void", "Termina el programa inmediatamente.", n_exit);
    vm.defineGlobalFn("range", "(inicio: number, fin?: number, paso?: number = 1): Range", "Crea un rango de números (fin excluido).", n_range);
    vm.defineGlobalFn("time", "(): number", "Segundos transcurridos desde que empezó el programa (o el juego).", n_time);
    vm.defineGlobalFn("now", "(): number", "Hora actual del sistema en segundos desde 1970 (con milisegundos).", n_now);
    vm.defineGlobalFn("wait", "(segundos: number): Task", "Tarea que se completa tras N segundos. Úsala con await dentro de funciones async.", n_wait, "await wait(1.5)");
    vm.defineGlobalFn("nextFrame", "(): Task", "Tarea que se completa en el siguiente fotograma.", n_nextFrame, "await nextFrame()");
    vm.defineGlobalFn("spawn", "(funcion: function, ...args: any): Task", "Ejecuta una función (normalmente async) en segundo plano y devuelve su tarea.", n_spawn);
    vm.defineGlobalFn("off", "(suscripcion: any): void", "Cancela una suscripción de 'on', o todas las de un evento.", n_off);
    vm.defineGlobalFn("set", "(...valores: any): set", "Crea un conjunto (valores sin repetir).", n_set, "var vistos = set(1, 2, 3)");
    // Matemáticas
    vm.defineGlobalFn("abs", "(x: number): number", "Valor absoluto.", n_abs);
    vm.defineGlobalFn("sqrt", "(x: number): number", "Raíz cuadrada.", n_sqrt);
    vm.defineGlobalFn("pow", "(base: number, exponente: number): number", "Potencia.", n_pow);
    vm.defineGlobalFn("floor", "(x: number): number", "Redondea hacia abajo.", n_floor);
    vm.defineGlobalFn("ceil", "(x: number): number", "Redondea hacia arriba.", n_ceil);
    vm.defineGlobalFn("round", "(x: number, decimales?: number = 0): number", "Redondea al entero (o a N decimales) más cercano.", n_round);
    {
        ObjNative* f = vm.defineGlobalFn("format", "(valor: any, formato: text): text",
            "Da formato a un valor como \"${valor:formato}\": \".2f\" (2 decimales), \">8\" (a la derecha en 8), \"^8\" (centrado), "
            "\",\" (miles), \"08.3f\", \"+\", \"%\" (porcentaje), \"x\" (hexadecimal), \"b\" (binario), \"e\" (científica).",
            n_format, "format(3.14159, \".2f\")  // \"3.14\"");
        vm.builtins[vm.builtinSlot("__format")] = Value::object(f); // lo usa "${x:.2f}" (no se puede tapar con una función propia)
    }
    vm.defineGlobalFn("sign", "(x: number): number", "-1, 0 o 1 según el signo.", n_sign);
    vm.defineGlobalFn("min", "(...valores: number|list): number", "El menor de los valores (o de una lista).", n_min);
    vm.defineGlobalFn("max", "(...valores: number|list): number", "El mayor de los valores (o de una lista).", n_max);
    vm.defineGlobalFn("clamp", "(x: number, minimo: number, maximo: number): number", "Limita x al intervalo [minimo, maximo].", n_clamp);
    vm.defineGlobalFn("lerp", "(a: number, b: number, t: number): number", "Interpolación lineal entre a y b (t de 0 a 1).", n_lerp);
    vm.defineGlobalFn("sin", "(radianes: number): number", "Seno.", n_sin);
    vm.defineGlobalFn("cos", "(radianes: number): number", "Coseno.", n_cos);
    vm.defineGlobalFn("tan", "(radianes: number): number", "Tangente.", n_tan);
    vm.defineGlobalFn("asin", "(x: number): number", "Arcoseno (radianes).", n_asin);
    vm.defineGlobalFn("acos", "(x: number): number", "Arcocoseno (radianes).", n_acos);
    vm.defineGlobalFn("atan", "(x: number): number", "Arcotangente (radianes).", n_atan);
    vm.defineGlobalFn("atan2", "(y: number, x: number): number", "Ángulo (radianes) del vector (x, y).", n_atan2);
    vm.defineGlobalFn("hypot", "(x: number, y: number): number", "Longitud de (x, y).", n_hypot);
    vm.defineGlobalFn("exp", "(x: number): number", "e elevado a x.", n_exp);
    vm.defineGlobalFn("log", "(x: number, base?: number): number", "Logaritmo natural (o en la base indicada).", n_log);
    vm.defineGlobalFn("log10", "(x: number): number", "Logaritmo en base 10.", n_log10);
    vm.defineGlobalFn("radians", "(grados: number): number", "Convierte grados a radianes.", n_radians);
    vm.defineGlobalFn("degrees", "(radianes: number): number", "Convierte radianes a grados.", n_degrees);
    vm.defineGlobalFn("random", "(min?: number, max?: number): number", "Número aleatorio: random() en [0,1), random(max) o random(min, max).", n_random);
    vm.defineGlobalFn("randomInt", "(min: number, max: number): int", "Entero aleatorio entre min y max (ambos incluidos).", n_randomInt);
    vm.defineGlobalFn("randomChoice", "(lista: list): any", "Elemento aleatorio de una lista.", n_randomChoice);
    vm.defineGlobalFn("randomSeed", "(semilla: number): void", "Fija la semilla del generador aleatorio (resultados repetibles).", n_randomSeed);
    vm.defineGlobal("PI", Value(M_PI), "number", "π ≈ 3.14159");
    vm.defineGlobal("TAU", Value(2 * M_PI), "number", "2π ≈ 6.28318 (una vuelta completa)");
    vm.defineGlobal("E", Value(M_E), "number", "Número e ≈ 2.71828");
    vm.defineGlobal("INFINITY", Value(INFINITY), "number", "Infinito");

    // Error
    vm.nativeClass("Error", "Error lanzable con throw. Campos: message, name, stack, file, line, column.", n_Error, "(mensaje?: text, tipo?: text)");
    vm.defineMethod(vm.errorClass, "toString", "(): text", "Texto 'Tipo: mensaje'.", err_toString);
    for (const char* f : {"message", "name", "file"}) vm.addCatalogField(vm.errorClass, f, "text", "");
    vm.addCatalogField(vm.errorClass, "stack", "list<text>", "Pila de llamadas cuando se creó el error.");
    vm.addCatalogField(vm.errorClass, "line", "number", "");
    vm.addCatalogField(vm.errorClass, "column", "number", "");

    // EventData
    vm.nativeClass("EventData", "Datos que recibe un manejador 'on'. Además de estos campos tiene uno por cada parámetro del evento.");
    vm.defineMethod(vm.eventDataClass, "stopPropagation", "(): void", "Detiene el despacho: los manejadores siguientes no se ejecutan.", evd_stop);
    vm.defineMethod(vm.eventDataClass, "preventDefault", "(): void", "Marca el evento para que el emisor no haga su acción por defecto.", evd_prevent);
    vm.addCatalogField(vm.eventDataClass, "name", "text", "Nombre del evento.");
    vm.addCatalogField(vm.eventDataClass, "timestamp", "number", "Momento de la emisión (segundos).");
    vm.addCatalogField(vm.eventDataClass, "target", "any", "Objeto que emitió el evento (si aplica).");
    vm.addCatalogField(vm.eventDataClass, "data", "list", "Todos los valores emitidos, en orden.");
    vm.addCatalogField(vm.eventDataClass, "defaultPrevented", "bool", "true si se llamó a preventDefault().");

    // text
    ObjClass* t = vm.textClass;
    vm.defineClassProp(t, "length", "number", "Número de caracteres.", t_length);
    vm.defineMethod(t, "upper", "(): text", "Texto en mayúsculas.", t_upper);
    vm.defineMethod(t, "lower", "(): text", "Texto en minúsculas.", t_lower);
    vm.defineMethod(t, "trim", "(): text", "Quita espacios al principio y al final.", t_trim);
    vm.defineMethod(t, "trimStart", "(): text", "Quita espacios al principio.", t_trimStart);
    vm.defineMethod(t, "trimEnd", "(): text", "Quita espacios al final.", t_trimEnd);
    vm.defineMethod(t, "split", "(separador?: text): list<text>", "Divide el texto (por espacios si no se indica separador).", t_split);
    vm.defineMethod(t, "contains", "(sub: text): bool", "¿Contiene el subtexto?", t_contains);
    vm.defineMethod(t, "startsWith", "(prefijo: text): bool", "¿Empieza por...?", t_startsWith);
    vm.defineMethod(t, "endsWith", "(sufijo: text): bool", "¿Termina en...?", t_endsWith);
    vm.defineMethod(t, "replace", "(buscar: text, poner: text): text", "Reemplaza todas las apariciones.", t_replace);
    vm.defineMethod(t, "indexOf", "(sub: text, desde?: number = 0): number", "Posición de la primera aparición a partir de 'desde' (o -1).", t_indexOf);
    vm.defineMethod(t, "lastIndexOf", "(sub: text): number", "Posición de la última aparición (o -1).", t_lastIndexOf);
    vm.defineMethod(t, "substring", "(inicio: number, fin?: number): text", "Parte del texto entre inicio y fin (excluido).", t_substring);
    vm.defineMethod(t, "slice", "(inicio: number, fin?: number): text", "Igual que substring; admite índices negativos.", t_substring);
    vm.defineMethod(t, "repeat", "(veces: number): text", "Repite el texto.", t_repeat);
    vm.defineMethod(t, "padStart", "(longitud: number, relleno?: text = \" \"): text", "Rellena por la izquierda hasta la longitud.", t_padStart);
    vm.defineMethod(t, "padEnd", "(longitud: number, relleno?: text = \" \"): text", "Rellena por la derecha hasta la longitud.", t_padEnd);
    vm.defineMethod(t, "chars", "(): list<text>", "Lista de caracteres.", t_chars);
    vm.defineMethod(t, "lines", "(): list<text>", "Lista de líneas.", t_lines);
    vm.defineMethod(t, "isEmpty", "(): bool", "¿Está vacío?", t_isEmpty);
    vm.defineMethod(t, "toNumber", "(porDefecto?: any): number?", "Convierte a número.", t_toNumber);
    vm.defineMethod(t, "reverse", "(): text", "Texto al revés.", t_reverse);
    vm.defineMethod(t, "count", "(sub: text): number", "Cuántas veces aparece sub.", t_count);

    // list
    ObjClass* l = vm.listClass;
    vm.defineClassProp(l, "length", "number", "Número de elementos.", l_length);
    vm.defineMethod(l, "add", "(...elementos: any): list", "Añade elementos al final.", l_push);
    vm.defineMethod(l, "push", "(...elementos: any): list", "Añade elementos al final.", l_push);
    vm.defineMethod(l, "pop", "(): T", "Quita y devuelve el último elemento (o null).", l_pop);
    vm.defineMethod(l, "shift", "(): T", "Quita y devuelve el primer elemento (o null).", l_shift);
    vm.defineMethod(l, "insert", "(indice: number, valor: any): list", "Inserta en una posición.", l_insert);
    vm.defineMethod(l, "removeAt", "(indice: number): T", "Quita el elemento de una posición y lo devuelve.", l_removeAt);
    vm.defineMethod(l, "remove", "(valor: any): bool", "Quita la primera aparición de un valor.", l_remove);
    vm.defineMethod(l, "contains", "(valor: any): bool", "¿Contiene el valor?", l_contains);
    vm.defineMethod(l, "indexOf", "(valor: any): number", "Posición del valor (o -1).", l_indexOf);
    vm.defineMethod(l, "clear", "(): list", "Vacía la lista.", l_clear);
    vm.defineMethod(l, "isEmpty", "(): bool", "¿Está vacía?", l_isEmpty);
    vm.defineMethod(l, "first", "(): T", "Primer elemento (o null).", l_first);
    vm.defineMethod(l, "last", "(): T", "Último elemento (o null).", l_last);
    vm.defineMethod(l, "copy", "(): list", "Copia superficial.", l_copy);
    vm.defineMethod(l, "reverse", "(): list", "Invierte el orden (en el sitio).", l_reverse);
    vm.defineMethod(l, "reversed", "(): list", "Copia invertida.", l_reversed);
    vm.defineMethod(l, "slice", "(inicio?: number, fin?: number): list", "Sublista entre inicio y fin (excluido).", l_slice);
    vm.defineMethod(l, "join", "(separador?: text = \", \"): text", "Une los elementos en un texto.", l_join);
    vm.defineMethod(l, "shuffle", "(): list", "Mezcla al azar (en el sitio).", l_shuffle);
    vm.defineMethod(l, "random", "(): T", "Elemento aleatorio (o null).", l_random);
    vm.defineMethod(l, "sum", "(): number", "Suma de los elementos numéricos.", l_sum);
    vm.defineMethod(l, "min", "(): T", "Menor elemento.", l_min);
    vm.defineMethod(l, "max", "(): T", "Mayor elemento.", l_max);
    vm.defineMethod(l, "map", "(f: function): list", "Nueva lista con f(x) para cada elemento.", l_map);
    vm.defineMethod(l, "filter", "(f: function): list", "Nueva lista con los elementos donde f(x) es verdadero.", l_filter);
    vm.defineMethod(l, "reduce", "(f: function, inicial?: any): any", "Acumula: f(acumulado, x).", l_reduce);
    vm.defineMethod(l, "find", "(f: function): T", "Primer elemento que cumple f (o null).", l_find);
    vm.defineMethod(l, "findIndex", "(f: function): number", "Posición del primero que cumple f (o -1).", l_findIndex);
    vm.defineMethod(l, "any", "(f: function): bool", "¿Alguno cumple f?", l_any);
    vm.defineMethod(l, "all", "(f: function): bool", "¿Todos cumplen f?", l_all);
    vm.defineMethod(l, "count", "(f?: function): number", "Cuántos cumplen f (o el total).", l_count);
    vm.defineMethod(l, "forEach", "(f: function): void", "Llama a f(x) para cada elemento.", l_forEach);
    vm.defineMethod(l, "sort", "(comparar?: function): list", "Ordena en el sitio (opcional: comparar(a, b) < 0).", l_sort);
    vm.defineMethod(l, "sortBy", "(clave: function): list", "Ordena en el sitio por clave(x).", l_sortBy);
    vm.defineMethod(l, "unique", "(): list", "Copia sin elementos repetidos.", l_unique);

    // map y set
    ObjClass* m = vm.mapClass;
    vm.defineClassProp(m, "length", "number", "Número de entradas.", m_length);
    vm.defineMethod(m, "keys", "(): list", "Lista de claves.", m_keys);
    vm.defineMethod(m, "values", "(): list", "Lista de valores.", m_values);
    vm.defineMethod(m, "entries", "(): list", "Lista de pares [clave, valor].", m_entries);
    vm.defineMethod(m, "has", "(clave: any): bool", "¿Existe la clave?", m_has);
    vm.defineMethod(m, "contains", "(clave: any): bool", "¿Existe la clave?", m_has);
    vm.defineMethod(m, "get", "(clave: any, porDefecto?: any): any", "Valor de la clave (o porDefecto).", m_get);
    vm.defineMethod(m, "set", "(clave: any, valor: any): map", "Asigna un valor.", m_set);
    vm.defineMethod(m, "remove", "(clave: any): bool", "Elimina una clave.", m_remove);
    vm.defineMethod(m, "clear", "(): map", "Vacía el mapa.", m_clear);
    vm.defineMethod(m, "isEmpty", "(): bool", "¿Está vacío?", m_isEmpty);
    vm.defineMethod(m, "copy", "(): map", "Copia superficial.", m_copy);
    vm.defineMethod(m, "forEach", "(f: function): void", "Llama a f(clave, valor).", m_forEach);
    vm.defineMethod(m, "merge", "(otro: map): map", "Copia las entradas de otro mapa.", m_merge);
    ObjClass* s = vm.setClass;
    vm.defineClassProp(s, "length", "number", "Número de elementos.", m_length);
    vm.defineMethod(s, "add", "(...valores: any): set", "Añade valores.", s_add);
    vm.defineMethod(s, "has", "(valor: any): bool", "¿Contiene el valor?", m_has);
    vm.defineMethod(s, "contains", "(valor: any): bool", "¿Contiene el valor?", m_has);
    vm.defineMethod(s, "remove", "(valor: any): bool", "Quita un valor.", m_remove);
    vm.defineMethod(s, "clear", "(): set", "Vacía el conjunto.", m_clear);
    vm.defineMethod(s, "isEmpty", "(): bool", "¿Está vacío?", m_isEmpty);
    vm.defineMethod(s, "toList", "(): list", "Convierte en lista.", s_toList);
    vm.defineMethod(s, "copy", "(): set", "Copia.", m_copy);
    vm.defineMethod(s, "forEach", "(f: function): void", "Llama a f(valor).", m_forEach);

    // number
    vm.defineMethod(vm.numberClass, "toFixed", "(decimales?: number = 0): text", "Texto con N decimales fijos.", num_toFixed);
    vm.defineMethod(vm.numberClass, "isInteger", "(): bool", "¿No tiene decimales?", num_isInteger);
    vm.defineMethod(vm.numberClass, "toText", "(): text", "Convierte en texto.", num_toText);

    // Range
    vm.defineClassProp(vm.rangeClass, "start", "number", "Inicio.", r_start);
    vm.defineClassProp(vm.rangeClass, "end", "number", "Fin.", r_end);
    vm.defineClassProp(vm.rangeClass, "step", "number", "Paso.", r_step);
    vm.defineClassProp(vm.rangeClass, "length", "number", "Cantidad de números.", r_length);
    vm.defineMethod(vm.rangeClass, "toList", "(): list<number>", "Lista con todos los números.", r_toList);
    vm.defineMethod(vm.rangeClass, "contains", "(x: number): bool", "¿Está x en el rango?", r_contains);

    // Task
    vm.defineClassProp(vm.taskClass, "done", "bool", "¿Ha terminado (bien, con error o cancelada)?", task_done);
    vm.defineClassProp(vm.taskClass, "failed", "bool", "¿Terminó con error?", task_failed);
    vm.defineClassProp(vm.taskClass, "result", "any", "Resultado (si terminó bien).", task_result);
    vm.defineClassProp(vm.taskClass, "error", "any", "Error (si falló).", task_error);
    vm.defineMethod(vm.taskClass, "cancel", "(): bool", "Cancela la tarea (cooperativo: se detiene en su siguiente await).", task_cancel);
    vm.defineMethod(vm.taskClass, "then", "(f: function): Task", "Llama a f(resultado) cuando termine bien.", task_then);

    // Event y Subscription
    vm.defineClassProp(vm.eventClass, "name", "text", "Nombre del evento.", ev_name);
    vm.defineClassProp(vm.eventClass, "listenerCount", "number", "Número de manejadores activos.", ev_count);
    vm.defineMethod(vm.eventClass, "clear", "(): void", "Quita todos los manejadores.", ev_clear);
    vm.defineMethod(vm.subscriptionClass, "cancel", "(): void", "Deja de escuchar el evento.", sub_cancel);
    vm.defineClassProp(vm.subscriptionClass, "active", "bool", "¿Sigue activa?", sub_active);

    // function
    vm.defineClassProp(vm.functionClass, "name", "text", "Nombre de la función.", fn_name);
    vm.defineMethod(vm.functionClass, "call", "(...args: any): any", "Llama a la función con los argumentos.", fn_call);

    // Enum
    vm.defineMethod(vm.enumClass, "values", "(): list", "Todos los valores del enum en orden.", en_values);
    vm.defineClassProp(vm.enumClass, "count", "number", "Cantidad de valores.", en_count);
    vm.defineClassProp(vm.enumClass, "name", "text", "Nombre del enum.", en_name);
    vm.defineMethod(vm.enumClass, "fromName", "(nombre: text): any", "Valor con ese nombre (o null).", en_fromName);
    vm.defineMethod(vm.enumClass, "fromValue", "(valor: any): any", "Valor con ese valor asociado (o null).", en_fromValue);

    // JSON
    ObjModule* json = vm.nativeModule("JSON", "Convertir datos a texto JSON y viceversa.");
    vm.defineFn(json, "stringify", "(valor: any, sangria?: number = 0): text", "Convierte un valor en texto JSON.", json_stringify, "JSON.stringify({vida: 3}, 2)");
    vm.defineFn(json, "parse", "(texto: text): any", "Convierte texto JSON en mapas, listas, números...", json_parse);

    registerStdlibExtra(vm);
    vm.defineLibrary("HyperCore", {"JSON", "File", "System", "Console"}, "Núcleo: tipos, texto, colecciones, JSON, archivos, sistema, consola, tareas y eventos.");
}

} // namespace hz
