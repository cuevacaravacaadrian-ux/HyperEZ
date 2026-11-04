// Más biblioteca estándar (0.7): matemáticas (mcd, primos, ruido de Perlin...), textos (tildes, slug,
// distancia entre palabras...), listas, mapas y conjuntos (operaciones de conjuntos, conteos, ventanas),
// números (formato con separador de miles, romanos), fechas (sumar días, nombres en español) y JSON en archivos.
#include "lang/stdlib/native.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <numeric>

namespace hz {

namespace {

ObjList* L(Value* args) { return static_cast<ObjList*>(args[-1].o); }
ObjMap* M(Value* args) { return static_cast<ObjMap*>(args[-1].o); }
const std::string& S(Value* args) { return asString(args[-1])->str; }
double N(Value* args) { return args[-1].n; }

// Llama a una función del script con solo los argumentos que declara (como en extra.cpp)
Value callFlex(VM& vm, Value fn, int argc, Value* args) {
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
bool truthy(const Value& v) { return !(v.isNull() || v.isUndef() || (v.isBool() && !v.b)); }
ObjList* listArg(VM& vm, Value* args, int argc, int i, const char* fname) {
    Value v = optArg(args, argc, i);
    if (isObjType(v, OT::List)) return static_cast<ObjList*>(v.o);
    vm.raise(std::string(fname) + ": se esperaba una lista, no " + vm.typeName(v));
}
ObjMap* setArg(VM& vm, Value* args, int argc, int i, const char* fname) {
    Value v = optArg(args, argc, i);
    if (isObjType(v, OT::Map)) return static_cast<ObjMap*>(v.o);
    if (isObjType(v, OT::List)) { // una lista también vale: se convierte en conjunto
        ObjMap* s = vm.newMap(true);
        for (auto& x : static_cast<ObjList*>(v.o)->items) s->set(x, Value::boolean(true));
        return s;
    }
    vm.raise(std::string(fname) + ": se esperaba un set (o una lista), no " + vm.typeName(v));
}
std::vector<uint32_t> codepoints(const std::string& s) {
    std::vector<uint32_t> out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) out.push_back(utf8Decode(s, i));
    return out;
}
std::string fromCodepoints(const std::vector<uint32_t>& cps, size_t a = 0, size_t b = std::string::npos) {
    std::string out;
    b = std::min(b, cps.size());
    for (size_t i = a; i < b; i++) utf8Append(out, cps[i]);
    return out;
}
std::string lowerOf(VM& vm, const std::string& s) { return asString(vm.callMethod(vm.str(s), "lower", 0, nullptr))->str; }
std::string upperOf(VM& vm, const std::string& s) { return asString(vm.callMethod(vm.str(s), "upper", 0, nullptr))->str; }

// ---------------------------------------------------------------------------
// Matemáticas
// ---------------------------------------------------------------------------

double intArg(VM& vm, Value* args, int argc, int i, const char* fname) {
    double x = argNum(vm, args, argc, i, fname);
    if (std::floor(x) != x || std::fabs(x) > 9007199254740992.0) vm.raise(std::string(fname) + ": se esperaba un número entero, no " + formatNumber(x));
    return x;
}
NATIVE(n_gcd) {
    long long a = std::llabs((long long)intArg(vm, args, argc, 0, "gcd")), b = std::llabs((long long)intArg(vm, args, argc, 1, "gcd"));
    return Value((double)std::gcd(a, b));
}
NATIVE(n_lcm) {
    long long a = std::llabs((long long)intArg(vm, args, argc, 0, "lcm")), b = std::llabs((long long)intArg(vm, args, argc, 1, "lcm"));
    return Value(a == 0 || b == 0 ? 0.0 : (double)(a / std::gcd(a, b) * b));
}
NATIVE(n_factorial) {
    double n = intArg(vm, args, argc, 0, "factorial");
    if (n < 0) vm.raise("factorial: no existe el factorial de un número negativo");
    if (n > 170) return Value(INFINITY);
    double r = 1;
    for (int i = 2; i <= (int)n; i++) r *= i;
    return Value(r);
}
bool primeTest(long long n) {
    if (n < 2) return false;
    if (n % 2 == 0) return n == 2;
    if (n % 3 == 0) return n == 3;
    for (long long i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) return false;
    return true;
}
NATIVE(n_isPrime) {
    double x = argNum(vm, args, argc, 0, "isPrime");
    return Value::boolean(std::floor(x) == x && primeTest((long long)x));
}
NATIVE(n_primes) {
    double lim = argNum(vm, args, argc, 0, "primes");
    if (lim > 5e7) vm.raise("primes: el límite máximo es 50.000.000");
    int n = (int)std::max(0.0, std::floor(lim));
    std::vector<char> comp((size_t)n + 1, 0);
    ObjList* out = vm.newList();
    for (int i = 2; i <= n; i++) {
        if (comp[(size_t)i]) continue;
        out->items.push_back(Value(i));
        for (long long j = (long long)i * i; j <= n; j += i) comp[(size_t)j] = 1;
    }
    return Value::object(out);
}
NATIVE(n_divisors) {
    long long n = std::llabs((long long)intArg(vm, args, argc, 0, "divisors"));
    std::vector<long long> lo, hi;
    for (long long i = 1; i * i <= n; i++)
        if (n % i == 0) { lo.push_back(i); if (i != n / i) hi.push_back(n / i); }
    ObjList* out = vm.newList();
    for (auto x : lo) out->items.push_back(Value((double)x));
    for (auto it = hi.rbegin(); it != hi.rend(); ++it) out->items.push_back(Value((double)*it));
    return Value::object(out);
}
NATIVE(n_cbrt) { return Value(std::cbrt(argNum(vm, args, argc, 0, "cbrt"))); }
NATIVE(n_trunc) { return Value(std::trunc(argNum(vm, args, argc, 0, "trunc"))); }
NATIVE(n_fract) { double x = argNum(vm, args, argc, 0, "fract"); return Value(x - std::floor(x)); }
NATIVE(n_sinh) { return Value(std::sinh(argNum(vm, args, argc, 0, "sinh"))); }
NATIVE(n_cosh) { return Value(std::cosh(argNum(vm, args, argc, 0, "cosh"))); }
NATIVE(n_tanh) { return Value(std::tanh(argNum(vm, args, argc, 0, "tanh"))); }
NATIVE(n_randomGaussian) {
    double mean = argNum(vm, args, argc, 0, "randomGaussian", 0), sd = argNum(vm, args, argc, 1, "randomGaussian", 1);
    double u1 = std::max(randomUnit(), 1e-300), u2 = randomUnit(); // Box-Muller (usa la semilla de randomSeed)
    return Value(mean + sd * std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2));
}

// Ruido de Perlin mejorado (el mismo en todos los sistemas): terrenos, nubes, movimientos naturales
const uint8_t kPerm[256] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240,
    21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,
    237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111,
    229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,
    209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217,
    226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,
    223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,
    108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241,
    81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,
    150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};
double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
double grad(int h, double x, double y, double z) {
    h &= 15;
    double u = h < 8 ? x : y, v = h < 4 ? y : (h == 12 || h == 14) ? x : z;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}
double perlin(double x, double y, double z) {
    auto P = [](int i) { return (int)kPerm[i & 255]; };
    int X = (int)std::floor(x) & 255, Y = (int)std::floor(y) & 255, Z = (int)std::floor(z) & 255;
    x -= std::floor(x); y -= std::floor(y); z -= std::floor(z);
    double u = fade(x), v = fade(y), w = fade(z);
    int A = P(X) + Y, AA = P(A) + Z, AB = P(A + 1) + Z, B = P(X + 1) + Y, BA = P(B) + Z, BB = P(B + 1) + Z;
    auto lerp = [](double t, double a, double b) { return a + t * (b - a); };
    return lerp(w, lerp(v, lerp(u, grad(P(AA), x, y, z), grad(P(BA), x - 1, y, z)), lerp(u, grad(P(AB), x, y - 1, z), grad(P(BB), x - 1, y - 1, z))),
                lerp(v, lerp(u, grad(P(AA + 1), x, y, z - 1), grad(P(BA + 1), x - 1, y, z - 1)),
                     lerp(u, grad(P(AB + 1), x, y - 1, z - 1), grad(P(BB + 1), x - 1, y - 1, z - 1))));
}
NATIVE(n_noise) {
    double x = argNum(vm, args, argc, 0, "noise"), y = argNum(vm, args, argc, 1, "noise", 0.5), z = argNum(vm, args, argc, 2, "noise", 0.5);
    return Value(perlin(x, y, z));
}
NATIVE(n_chr) {
    double c = argNum(vm, args, argc, 0, "chr");
    if (c < 0 || c > 0x10FFFF || std::floor(c) != c) vm.raise("chr: " + formatNumber(c) + " no es un código de carácter válido");
    std::string s;
    utf8Append(s, (uint32_t)c);
    return vm.str(s);
}
NATIVE(n_ord) {
    std::string s = argStr(vm, args, argc, 0, "ord");
    if (s.empty()) vm.raise("ord: el texto está vacío");
    size_t i = 0;
    return Value((double)utf8Decode(s, i));
}

// ---------------------------------------------------------------------------
// Textos
// ---------------------------------------------------------------------------

// Letra sin tilde ni diéresis (á → a, Ñ → N, ç → c, ß → ss...); 0 si no cambia
const char* plainLetter(uint32_t c) {
    if (c < 0xC0) return nullptr;
    static const char* latin1[64] = {"A", "A", "A", "A", "A", "A", "AE", "C", "E", "E", "E", "E", "I", "I", "I", "I",
                                     "D", "N", "O", "O", "O", "O", "O", nullptr, "O", "U", "U", "U", "U", "Y", nullptr, "ss",
                                     "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
                                     "d", "n", "o", "o", "o", "o", "o", nullptr, "o", "u", "u", "u", "u", "y", nullptr, "y"};
    if (c <= 0xFF) return latin1[c - 0xC0];
    switch (c) {
        case 0x152: return "OE"; case 0x153: return "oe"; case 0x160: return "S"; case 0x161: return "s";
        case 0x17D: return "Z"; case 0x17E: return "z"; case 0x141: return "L"; case 0x142: return "l";
        case 0x178: return "Y"; case 0x10C: return "C"; case 0x10D: return "c"; case 0x158: return "R"; case 0x159: return "r";
        default: return nullptr;
    }
}
std::string stripAccents(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        size_t st = i;
        uint32_t c = utf8Decode(s, i);
        if (const char* p = plainLetter(c)) out += p;
        else out.append(s, st, i - st);
    }
    return out;
}
NATIVE(t_removeAccents) { UNUSED; return vm.str(stripAccents(S(args))); }

// Palabras para cambiar de estilo: separa por espacios, guiones, _ y cambios de minúscula a mayúscula
std::vector<std::string> splitIdentifier(const std::string& s) {
    std::vector<std::string> words;
    std::string cur;
    auto cps = codepoints(s);
    auto isUpperAscii = [](uint32_t c) { return c >= 'A' && c <= 'Z'; };
    auto isLowerAscii = [](uint32_t c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
    for (size_t i = 0; i < cps.size(); i++) {
        uint32_t c = cps[i];
        if (c == ' ' || c == '-' || c == '_' || c == '.' || c == '\t' || c == '/') {
            if (!cur.empty()) { words.push_back(cur); cur.clear(); }
            continue;
        }
        if (isUpperAscii(c) && !cur.empty() && i > 0 && isLowerAscii(cps[i - 1])) { words.push_back(cur); cur.clear(); }
        utf8Append(cur, c);
    }
    if (!cur.empty()) words.push_back(cur);
    return words;
}
std::string capitalizeWord(VM& vm, const std::string& w) {
    if (w.empty()) return w;
    size_t i = 0;
    uint32_t c = utf8Decode(w, i);
    std::string first;
    utf8Append(first, c);
    return upperOf(vm, first) + lowerOf(vm, w.substr(i));
}
NATIVE(t_title) {
    UNUSED;
    // Mayúscula al principio de cada palabra; respeta los espacios originales
    const std::string& s = S(args);
    std::string out, word;
    auto flush = [&] { out += capitalizeWord(vm, word); word.clear(); };
    for (size_t i = 0; i < s.size();) {
        size_t st = i;
        uint32_t c = utf8Decode(s, i);
        if (c == ' ' || c == '\t' || c == '\n' || c == '-' || c == '(' || c == '"' || c == '\'') { flush(); out.append(s, st, i - st); }
        else word.append(s, st, i - st);
    }
    flush();
    return vm.str(out);
}
NATIVE(t_camelCase) {
    UNUSED;
    auto ws = splitIdentifier(stripAccents(S(args)));
    std::string out;
    for (size_t i = 0; i < ws.size(); i++) out += i == 0 ? lowerOf(vm, ws[i]) : capitalizeWord(vm, ws[i]);
    return vm.str(out);
}
NATIVE(t_pascalCase) {
    UNUSED;
    std::string out;
    for (auto& w : splitIdentifier(stripAccents(S(args)))) out += capitalizeWord(vm, w);
    return vm.str(out);
}
std::string joinLower(VM& vm, const std::string& s, char sep) {
    std::string out;
    for (auto& w : splitIdentifier(stripAccents(s))) {
        if (!out.empty()) out += sep;
        out += lowerOf(vm, w);
    }
    return out;
}
NATIVE(t_snakeCase) { UNUSED; return vm.str(joinLower(vm, S(args), '_')); }
NATIVE(t_kebabCase) { UNUSED; return vm.str(joinLower(vm, S(args), '-')); }
NATIVE(t_slug) {
    UNUSED;
    std::string s = lowerOf(vm, stripAccents(S(args))), out;
    bool dash = false;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { if (dash && !out.empty()) out += '-'; out += (char)c; dash = false; }
        else dash = true;
    }
    return vm.str(out);
}
NATIVE(t_truncate) {
    double n = argNum(vm, args, argc, 0, "truncate");
    std::string end = argStr(vm, args, argc, 1, "truncate", "…");
    auto cps = codepoints(S(args));
    if ((double)cps.size() <= n) return args[-1];
    size_t endLen = codepoints(end).size();
    size_t keep = n > (double)endLen ? (size_t)n - endLen : 0;
    std::string out = fromCodepoints(cps, 0, keep);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return vm.str(out + end);
}
NATIVE(t_wrap) {
    size_t width = (size_t)std::max(1.0, argNum(vm, args, argc, 0, "wrap", 80));
    const std::string& s = S(args);
    std::string out;
    size_t lineStart = 0;
    for (size_t p = 0; p <= s.size();) { // cada párrafo por separado
        size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        std::string para = s.substr(p, e - p), line;
        size_t lineLen = 0;
        size_t i = 0;
        while (i < para.size()) {
            while (i < para.size() && para[i] == ' ') i++;
            size_t j = i;
            while (j < para.size() && para[j] != ' ') j++;
            if (j == i) break;
            std::string word = para.substr(i, j - i);
            size_t wl = codepoints(word).size();
            if (lineLen > 0 && lineLen + 1 + wl > width) { out += line + "\n"; line.clear(); lineLen = 0; }
            if (lineLen > 0) { line += ' '; lineLen++; }
            line += word;
            lineLen += wl;
            i = j;
        }
        out += line;
        if (e < s.size()) out += "\n";
        p = e + 1;
        (void)lineStart;
    }
    return vm.str(out);
}
size_t levenshtein(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) prev[j] = j;
    for (size_t i = 1; i <= a.size(); i++) {
        cur[0] = i;
        for (size_t j = 1; j <= b.size(); j++)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}
NATIVE(t_distance) {
    std::string o = argStr(vm, args, argc, 0, "distance");
    return Value((double)levenshtein(codepoints(S(args)), codepoints(o)));
}
NATIVE(t_similarity) {
    std::string o = argStr(vm, args, argc, 0, "similarity");
    auto a = codepoints(lowerOf(vm, S(args))), b = codepoints(lowerOf(vm, o));
    size_t m = std::max(a.size(), b.size());
    return Value(m == 0 ? 1.0 : 1.0 - (double)levenshtein(a, b) / (double)m);
}
NATIVE(t_indent) {
    double n = argNum(vm, args, argc, 0, "indent", 4);
    std::string pad = argStr(vm, args, argc, 1, "indent", " ");
    std::string prefix;
    for (int i = 0; i < (int)n; i++) prefix += pad;
    const std::string& s = S(args);
    std::string out;
    size_t p = 0;
    while (p <= s.size()) {
        size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        if (e > p) out += prefix;
        out.append(s, p, e - p);
        if (e < s.size()) out += '\n';
        p = e + 1;
    }
    return vm.str(out);
}
NATIVE(t_dedent) {
    UNUSED;
    const std::string& s = S(args);
    std::vector<std::string> lines;
    size_t p = 0;
    while (p <= s.size()) {
        size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        lines.push_back(s.substr(p, e - p));
        p = e + 1;
    }
    size_t common = std::string::npos;
    for (auto& l : lines) {
        size_t a = l.find_first_not_of(" \t");
        if (a != std::string::npos) common = std::min(common, a);
    }
    if (common == std::string::npos) common = 0;
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i) out += '\n';
        out += lines[i].size() >= common ? lines[i].substr(common) : "";
    }
    return vm.str(out);
}
NATIVE(t_between) {
    std::string a = argStr(vm, args, argc, 0, "between"), b = argStr(vm, args, argc, 1, "between");
    const std::string& s = S(args);
    size_t i = s.find(a);
    if (i == std::string::npos) return Value();
    i += a.size();
    size_t j = s.find(b, i);
    if (j == std::string::npos) return Value();
    return vm.str(s.substr(i, j - i));
}
NATIVE(t_isBlank) {
    UNUSED;
    return Value::boolean(S(args).find_first_not_of(" \t\r\n") == std::string::npos);
}
NATIVE(t_codes) {
    UNUSED;
    ObjList* out = vm.newList();
    for (uint32_t c : codepoints(S(args))) out->items.push_back(Value((double)c));
    return Value::object(out);
}
NATIVE(t_reverseWords) {
    UNUSED;
    std::vector<std::string> ws;
    const std::string& s = S(args);
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') i++;
        size_t j = i;
        while (j < s.size() && s[j] != ' ') j++;
        if (j > i) ws.push_back(s.substr(i, j - i));
        i = j;
    }
    std::string out;
    for (size_t k = ws.size(); k-- > 0;) { out += ws[k]; if (k) out += ' '; }
    return vm.str(out);
}

// ---------------------------------------------------------------------------
// Listas
// ---------------------------------------------------------------------------

NATIVE(l_countBy) {
    Value fn = optArg(args, argc, 0);
    std::vector<Value> items = L(args)->items;
    ObjMap* out = vm.newMap();
    TempRoot keep(vm, out);
    for (size_t i = 0; i < items.size(); i++) {
        Value k = items[i];
        if (!fn.isNull()) { Value a[2] = {items[i], Value((double)i)}; k = callFlex(vm, fn, 2, a); }
        Value* slot = out->find(k);
        if (slot) slot->n += 1;
        else out->set(k, Value(1));
    }
    return Value::object(out);
}
NATIVE(l_sumBy) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    double s = 0;
    for (size_t i = 0; i < items.size(); i++) {
        Value a[2] = {items[i], Value((double)i)};
        Value r = callFlex(vm, fn, 2, a);
        if (!r.isNum()) vm.raise("sumBy: la función debe devolver números, no " + vm.typeName(r));
        s += r.n;
    }
    return Value(s);
}
NATIVE(l_uniqueBy) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    ObjMap* seen = vm.newMap(true);
    TempRoot k1(vm, seen);
    ObjList* out = vm.newList();
    TempRoot k2(vm, out);
    for (auto& v : items) {
        Value k = vm.call(fn, 1, &v);
        if (seen->find(k)) continue;
        seen->set(k, Value::boolean(true));
        out->items.push_back(v);
    }
    return Value::object(out);
}
NATIVE(l_windowed) {
    double n = argNum(vm, args, argc, 0, "windowed"), step = argNum(vm, args, argc, 1, "windowed", 1);
    if (n < 1 || step < 1) vm.raise("windowed: el tamaño y el paso deben ser al menos 1");
    auto& items = L(args)->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    for (size_t i = 0; i + (size_t)n <= items.size(); i += (size_t)step) {
        ObjList* w = vm.newList();
        w->items.assign(items.begin() + (long)i, items.begin() + (long)(i + (size_t)n));
        out->items.push_back(Value::object(w));
    }
    return Value::object(out);
}
NATIVE(l_rotate) {
    double n = argNum(vm, args, argc, 0, "rotate", 1);
    auto& items = L(args)->items;
    ObjList* out = vm.newList();
    out->items = items;
    if (!items.empty()) {
        long long k = ((long long)n % (long long)items.size() + (long long)items.size()) % (long long)items.size();
        std::rotate(out->items.begin(), out->items.begin() + (long)k, out->items.end());
    }
    return Value::object(out);
}
// Operaciones de conjuntos sobre listas (conservan el orden de la primera)
Value listSetOp(VM& vm, Value* args, int argc, int op, const char* fname) {
    ObjList* other = listArg(vm, args, argc, 0, fname);
    ObjMap* inOther = vm.newMap(true);
    TempRoot k1(vm, inOther);
    for (auto& v : other->items) inOther->set(v, Value::boolean(true));
    ObjMap* seen = vm.newMap(true);
    TempRoot k2(vm, seen);
    ObjList* out = vm.newList();
    TempRoot k3(vm, out);
    auto addOnce = [&](const Value& v) { if (!seen->find(v)) { seen->set(v, Value::boolean(true)); out->items.push_back(v); } };
    for (auto& v : L(args)->items) {
        bool there = inOther->find(v) != nullptr;
        if ((op == 0 && there) || (op == 1 && !there) || op == 2) addOnce(v);
    }
    if (op == 2) for (auto& v : other->items) addOnce(v);
    return Value::object(out);
}
NATIVE(l_intersect) { return listSetOp(vm, args, argc, 0, "intersect"); }
NATIVE(l_difference) { return listSetOp(vm, args, argc, 1, "difference"); }
NATIVE(l_union) { return listSetOp(vm, args, argc, 2, "union"); }
NATIVE(l_takeWhile) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    for (auto& v : items) { if (!truthy(vm.call(fn, 1, &v))) break; out->items.push_back(v); }
    return Value::object(out);
}
NATIVE(l_skipWhile) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    size_t i = 0;
    while (i < items.size() && truthy(vm.call(fn, 1, &items[i]))) i++;
    ObjList* out = vm.newList();
    out->items.assign(items.begin() + (long)i, items.end());
    return Value::object(out);
}
NATIVE(l_none) {
    Value fn = args[0];
    std::vector<Value> items = L(args)->items;
    for (auto& v : items) if (truthy(vm.call(fn, 1, &v))) return Value::boolean(false);
    return Value::boolean(true);
}
NATIVE(l_sample) {
    double n = argNum(vm, args, argc, 0, "sample");
    std::vector<Value> items = L(args)->items;
    size_t k = (size_t)std::clamp(n, 0.0, (double)items.size());
    for (size_t i = 0; i < k; i++) std::swap(items[i], items[i + (size_t)randomBetween(0, (long long)(items.size() - i - 1))]);
    ObjList* out = vm.newList();
    out->items.assign(items.begin(), items.begin() + (long)k);
    return Value::object(out);
}
NATIVE(l_toSet) {
    UNUSED;
    ObjMap* s = vm.newMap(true);
    for (auto& v : L(args)->items) s->set(v, Value::boolean(true));
    return Value::object(s);
}
NATIVE(l_compact) {
    UNUSED;
    ObjList* out = vm.newList();
    for (auto& v : L(args)->items) if (!v.isNull() && !v.isUndef()) out->items.push_back(v);
    return Value::object(out);
}
NATIVE(l_product) {
    UNUSED;
    double p = 1;
    for (auto& v : L(args)->items) { if (!v.isNum()) vm.raise("product: la lista contiene " + vm.typeName(v)); p *= v.n; }
    return Value(p);
}
NATIVE(l_lastIndexOf) {
    Value x = optArg(args, argc, 0);
    auto& items = L(args)->items;
    for (size_t i = items.size(); i-- > 0;) if (vm.objectsEqual(items[i], x)) return Value((double)i);
    return Value(-1);
}
NATIVE(l_binarySearch) {
    Value x = optArg(args, argc, 0);
    auto& items = L(args)->items;
    size_t lo = 0, hi = items.size();
    auto less = [&](const Value& a, const Value& b) {
        if (a.isNum() && b.isNum()) return a.n < b.n;
        if (isString(a) && isString(b)) return asString(a)->str < asString(b)->str;
        vm.raise("binarySearch: la lista debe estar ordenada y contener números o textos");
    };
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (less(items[mid], x)) lo = mid + 1;
        else hi = mid;
    }
    return Value(lo < items.size() && !less(x, items[lo]) && !less(items[lo], x) ? (double)lo : -1.0);
}
NATIVE(l_scan) {
    Value fn = args[0];
    Value acc = optArg(args, argc, 1);
    std::vector<Value> items = L(args)->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    size_t start = 0;
    if (argc < 2) { if (items.empty()) return Value::object(out); acc = items[0]; out->items.push_back(acc); start = 1; }
    for (size_t i = start; i < items.size(); i++) {
        Value a[3] = {acc, items[i], Value((double)i)};
        acc = callFlex(vm, fn, 3, a);
        out->items.push_back(acc);
    }
    return Value::object(out);
}
NATIVE(l_enumerate) {
    UNUSED;
    auto& items = L(args)->items;
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    for (size_t i = 0; i < items.size(); i++) {
        ObjList* pair = vm.newList();
        pair->items = {Value((double)i), items[i]};
        out->items.push_back(Value::object(pair));
    }
    return Value::object(out);
}
NATIVE(l_toMap) {
    Value kf = args[0], vf = optArg(args, argc, 1);
    std::vector<Value> items = L(args)->items;
    ObjMap* out = vm.newMap();
    TempRoot keep(vm, out);
    for (auto& v : items) {
        Value k = vm.call(kf, 1, &v);
        out->set(k, vf.isNull() ? v : vm.call(vf, 1, &v));
    }
    return Value::object(out);
}
NATIVE(l_median) {
    UNUSED;
    std::vector<double> xs;
    for (auto& v : L(args)->items) { if (!v.isNum()) vm.raise("median: la lista contiene " + vm.typeName(v)); xs.push_back(v.n); }
    if (xs.empty()) return Value(0);
    std::sort(xs.begin(), xs.end());
    size_t n = xs.size();
    return Value(n % 2 ? xs[n / 2] : (xs[n / 2 - 1] + xs[n / 2]) / 2);
}
NATIVE(l_insertSorted) {
    Value x = optArg(args, argc, 0);
    auto& items = L(args)->items;
    auto it = std::upper_bound(items.begin(), items.end(), x, [&](const Value& a, const Value& b) {
        if (a.isNum() && b.isNum()) return a.n < b.n;
        if (isString(a) && isString(b)) return asString(a)->str < asString(b)->str;
        vm.raise("insertSorted: solo se pueden comparar números o textos");
    });
    size_t pos = (size_t)(it - items.begin());
    items.insert(it, x);
    return Value((double)pos);
}

// ---------------------------------------------------------------------------
// Mapas
// ---------------------------------------------------------------------------

NATIVE(m_pick) {
    ObjList* keys = listArg(vm, args, argc, 0, "pick");
    ObjMap* m = M(args);
    ObjMap* out = vm.newMap();
    for (auto& k : keys->items) if (Value* v = m->find(k)) out->set(k, *v);
    return Value::object(out);
}
NATIVE(m_omit) {
    ObjList* keys = listArg(vm, args, argc, 0, "omit");
    ObjMap* drop = vm.newMap(true);
    TempRoot keep(vm, drop);
    for (auto& k : keys->items) drop->set(k, Value::boolean(true));
    ObjMap* out = vm.newMap();
    for (auto& e : M(args)->entries) if (e.alive && !drop->find(e.key)) out->set(e.key, e.value);
    return Value::object(out);
}
NATIVE(m_invert) {
    UNUSED;
    ObjMap* out = vm.newMap();
    for (auto& e : M(args)->entries) if (e.alive) out->set(e.value, e.key);
    return Value::object(out);
}
NATIVE(m_update) {
    Value key = optArg(args, argc, 0), fn = optArg(args, argc, 1), def = optArg(args, argc, 2);
    ObjMap* m = M(args);
    Value cur = m->find(key) ? *m->find(key) : def;
    Value r = vm.call(fn, 1, &cur);
    m->set(key, r);
    return r;
}
// Recorre las entradas vivas llamando a f(valor, clave); para any/all/count/find
template <class F> void eachEntry(VM& vm, ObjMap* m, Value fn, F onResult) {
    std::vector<std::pair<Value, Value>> es;
    for (auto& e : m->entries) if (e.alive) es.push_back({e.key, e.value});
    for (auto& [k, v] : es) {
        Value a[2] = {v, k};
        if (onResult(k, v, truthy(callFlex(vm, fn, 2, a)))) break;
    }
}
NATIVE(m_any) {
    UNUSED;
    bool r = false;
    eachEntry(vm, M(args), args[0], [&](const Value&, const Value&, bool t) { if (t) r = true; return r; });
    return Value::boolean(r);
}
NATIVE(m_all) {
    UNUSED;
    bool r = true;
    eachEntry(vm, M(args), args[0], [&](const Value&, const Value&, bool t) { if (!t) r = false; return !r; });
    return Value::boolean(r);
}
NATIVE(m_count) {
    UNUSED;
    double n = 0;
    eachEntry(vm, M(args), args[0], [&](const Value&, const Value&, bool t) { if (t) n++; return false; });
    return Value(n);
}
NATIVE(m_find) {
    UNUSED;
    Value key = Value::undef();
    eachEntry(vm, M(args), args[0], [&](const Value& k, const Value&, bool t) { if (t) key = k; return t; });
    return key.isUndef() ? Value() : key;
}
NATIVE(m_mapKeys) {
    Value fn = args[0];
    std::vector<std::pair<Value, Value>> es;
    for (auto& e : M(args)->entries) if (e.alive) es.push_back({e.key, e.value});
    ObjMap* out = vm.newMap();
    TempRoot keep(vm, out);
    for (auto& [k, v] : es) {
        Value a[2] = {k, v};
        out->set(callFlex(vm, fn, 2, a), v);
    }
    (void)argc;
    return Value::object(out);
}
Value sortedMap(VM& vm, ObjMap* m, bool byValue, bool desc, const char* fname) {
    std::vector<std::pair<Value, Value>> es;
    for (auto& e : m->entries) if (e.alive) es.push_back({e.key, e.value});
    std::stable_sort(es.begin(), es.end(), [&](const auto& a, const auto& b) {
        const Value& x = byValue ? a.second : a.first;
        const Value& y = byValue ? b.second : b.first;
        bool lt;
        if (x.isNum() && y.isNum()) lt = desc ? y.n < x.n : x.n < y.n;
        else if (isString(x) && isString(y)) lt = desc ? asString(y)->str < asString(x)->str : asString(x)->str < asString(y)->str;
        else vm.raise(std::string(fname) + ": solo se pueden ordenar números o textos");
        return lt;
    });
    ObjMap* out = vm.newMap();
    for (auto& [k, v] : es) out->set(k, v);
    return Value::object(out);
}
NATIVE(m_sortByKey) { return sortedMap(vm, M(args), false, argBool(args, argc, 0, false), "sortByKey"); }
NATIVE(m_sortByValue) { return sortedMap(vm, M(args), true, argBool(args, argc, 0, false), "sortByValue"); }

// ---------------------------------------------------------------------------
// Conjuntos (set)
// ---------------------------------------------------------------------------

Value setOp(VM& vm, Value* args, int argc, int op, const char* fname) {
    ObjMap* a = M(args);
    ObjMap* b = setArg(vm, args, argc, 0, fname);
    TempRoot keepB(vm, b);
    ObjMap* out = vm.newMap(true);
    for (auto& e : a->entries) {
        if (!e.alive) continue;
        bool inB = b->find(e.key) != nullptr;
        if (op == 0 || (op == 1 && inB) || (op == 2 && !inB) || (op == 3 && !inB)) out->set(e.key, Value::boolean(true));
    }
    if (op == 0 || op == 3)
        for (auto& e : b->entries)
            if (e.alive && (op == 0 || !a->find(e.key))) out->set(e.key, Value::boolean(true));
    return Value::object(out);
}
NATIVE(s_union) { return setOp(vm, args, argc, 0, "union"); }
NATIVE(s_intersect) { return setOp(vm, args, argc, 1, "intersect"); }
NATIVE(s_difference) { return setOp(vm, args, argc, 2, "difference"); }
NATIVE(s_symmetricDifference) { return setOp(vm, args, argc, 3, "symmetricDifference"); }
bool subsetOf(ObjMap* a, ObjMap* b) {
    for (auto& e : a->entries) if (e.alive && !b->find(e.key)) return false;
    return true;
}
NATIVE(s_isSubset) { ObjMap* b = setArg(vm, args, argc, 0, "isSubset"); TempRoot k(vm, b); return Value::boolean(subsetOf(M(args), b)); }
NATIVE(s_isSuperset) { ObjMap* b = setArg(vm, args, argc, 0, "isSuperset"); TempRoot k(vm, b); return Value::boolean(subsetOf(b, M(args))); }
NATIVE(s_isDisjoint) {
    ObjMap* b = setArg(vm, args, argc, 0, "isDisjoint");
    TempRoot k(vm, b);
    for (auto& e : M(args)->entries) if (e.alive && b->find(e.key)) return Value::boolean(false);
    return Value::boolean(true);
}
NATIVE(s_map) {
    Value fn = args[0];
    std::vector<Value> items;
    for (auto& e : M(args)->entries) if (e.alive) items.push_back(e.key);
    ObjMap* out = vm.newMap(true);
    TempRoot keep(vm, out);
    for (auto& v : items) out->set(vm.call(fn, 1, &v), Value::boolean(true));
    (void)argc;
    return Value::object(out);
}
NATIVE(s_filter) {
    Value fn = args[0];
    std::vector<Value> items;
    for (auto& e : M(args)->entries) if (e.alive) items.push_back(e.key);
    ObjMap* out = vm.newMap(true);
    TempRoot keep(vm, out);
    for (auto& v : items) if (truthy(vm.call(fn, 1, &v))) out->set(v, Value::boolean(true));
    (void)argc;
    return Value::object(out);
}

// ---------------------------------------------------------------------------
// Números
// ---------------------------------------------------------------------------

NATIVE(num_isEven) { UNUSED; double x = N(args); return Value::boolean(std::floor(x) == x && std::fmod(x, 2.0) == 0); }
NATIVE(num_isOdd) { UNUSED; double x = N(args); return Value::boolean(std::floor(x) == x && std::fabs(std::fmod(x, 2.0)) == 1); }
NATIVE(num_clamp) {
    double lo = argNum(vm, args, argc, 0, "clamp"), hi = argNum(vm, args, argc, 1, "clamp");
    return Value(std::min(std::max(N(args), lo), hi));
}
NATIVE(num_between) {
    double lo = argNum(vm, args, argc, 0, "between"), hi = argNum(vm, args, argc, 1, "between");
    return Value::boolean(N(args) >= lo && N(args) <= hi);
}
NATIVE(num_toRoman) {
    UNUSED;
    double x = N(args);
    if (x < 1 || x > 3999 || std::floor(x) != x) vm.raise("toRoman: solo números enteros del 1 al 3999");
    static const int vals[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static const char* syms[] = {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    int n = (int)x;
    std::string out;
    for (int i = 0; i < 13; i++) while (n >= vals[i]) { out += syms[i]; n -= vals[i]; }
    return vm.str(out);
}
// 1234567.891.format(2) → "1.234.567,89" (estilo español); separadores configurables
NATIVE(num_format) {
    int dec = (int)std::clamp(argNum(vm, args, argc, 0, "format", 0), 0.0, 20.0);
    std::string thou = argStr(vm, args, argc, 1, "format", "."), point = argStr(vm, args, argc, 2, "format", ",");
    double x = N(args);
    if (!std::isfinite(x)) return vm.str(formatNumber(x));
    char buf[512];
    std::snprintf(buf, sizeof buf, "%.*f", dec, std::fabs(x));
    std::string s = buf, ip = s, fp;
    size_t dot = s.find('.');
    if (dot != std::string::npos) { ip = s.substr(0, dot); fp = s.substr(dot + 1); }
    std::string out;
    int cnt = 0;
    for (size_t i = ip.size(); i-- > 0;) {
        out.insert(out.begin(), ip[i]);
        if (++cnt % 3 == 0 && i > 0) out.insert(0, thou);
    }
    bool neg = x < 0 && (s.find_first_not_of("0.") != std::string::npos);
    return vm.str((neg ? "-" : "") + out + (fp.empty() ? "" : point + fp));
}
NATIVE(num_digits) {
    UNUSED;
    double x = std::fabs(std::trunc(N(args)));
    std::string s = formatNumber(x);
    ObjList* out = vm.newList();
    for (char c : s) if (c >= '0' && c <= '9') out->items.push_back(Value(c - '0'));
    return Value::object(out);
}
NATIVE(num_times) {
    Value fn = args[0];
    double n = N(args);
    for (double i = 0; i < n; i++) { Value a = Value(i); callFlex(vm, fn, 1, &a); }
    (void)argc;
    return Value();
}
NATIVE(num_toPercent) {
    int dec = (int)std::clamp(argNum(vm, args, argc, 0, "toPercent", 0), 0.0, 10.0);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", dec, N(args) * 100);
    std::string s = buf;
    for (auto& c : s) if (c == '.') c = ',';
    return vm.str(s + " %");
}

// ---------------------------------------------------------------------------
// Fechas (Time)
// ---------------------------------------------------------------------------

std::tm localTime(double t) {
    std::time_t tt = (std::time_t)std::floor(t);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}
double nowSecs() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}
const char* kMonths[] = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"};
const char* kDays[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
int monthDays(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && leap(y) ? 29 : d[(m - 1) % 12];
}
NATIVE(time_addDays) {
    double t = argNum(vm, args, argc, 0, "Time.addDays"), n = argNum(vm, args, argc, 1, "Time.addDays");
    std::tm tm = localTime(t);
    double frac = t - std::floor(t);
    tm.tm_mday += (int)n;
    tm.tm_isdst = -1; // mktime corrige el horario de verano
    return Value((double)std::mktime(&tm) + frac);
}
NATIVE(time_addMonths) {
    double t = argNum(vm, args, argc, 0, "Time.addMonths"), n = argNum(vm, args, argc, 1, "Time.addMonths");
    std::tm tm = localTime(t);
    int total = tm.tm_year * 12 + tm.tm_mon + (int)n;
    tm.tm_year = total / 12;
    tm.tm_mon = total % 12;
    tm.tm_mday = std::min(tm.tm_mday, monthDays(tm.tm_year + 1900, tm.tm_mon + 1)); // 31 de enero + 1 mes = 28/29 de febrero
    tm.tm_isdst = -1;
    return Value((double)std::mktime(&tm));
}
double midnight(double t) {
    std::tm tm = localTime(t);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return (double)std::mktime(&tm);
}
NATIVE(time_daysBetween) {
    double a = argNum(vm, args, argc, 0, "Time.daysBetween"), b = argNum(vm, args, argc, 1, "Time.daysBetween");
    return Value(std::round((midnight(b) - midnight(a)) / 86400.0));
}
NATIVE(time_today) { UNUSED; return Value(midnight(nowSecs())); }
NATIVE(time_isLeapYear) { return Value::boolean(leap((int)argNum(vm, args, argc, 0, "Time.isLeapYear"))); }
NATIVE(time_daysInMonth) {
    int y = (int)argNum(vm, args, argc, 0, "Time.daysInMonth"), m = (int)argNum(vm, args, argc, 1, "Time.daysInMonth");
    if (m < 1 || m > 12) vm.raise("Time.daysInMonth: el mes debe estar entre 1 y 12");
    return Value(monthDays(y, m));
}
NATIVE(time_monthName) {
    int m = (int)argNum(vm, args, argc, 0, "Time.monthName");
    if (m < 1 || m > 12) vm.raise("Time.monthName: el mes debe estar entre 1 y 12");
    return vm.str(kMonths[m - 1]);
}
NATIVE(time_dayName) {
    int d = (int)argNum(vm, args, argc, 0, "Time.dayName");
    return vm.str(kDays[((d % 7) + 7) % 7]);
}
// "2026-11-04", "2026-11-04 18:30", "2026-11-04T18:30:05", "04/11/2026" y "4/11/2026 18:30"
NATIVE(time_parse) {
    std::string s = argStr(vm, args, argc, 0, "Time.parse");
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    int n = std::sscanf(s.c_str(), "%d-%d-%d%*[ T]%d:%d:%d", &y, &mo, &d, &h, &mi, &se);
    if (n < 3) {
        n = std::sscanf(s.c_str(), "%d/%d/%d %d:%d:%d", &d, &mo, &y, &h, &mi, &se);
        if (n < 3) return Value();
    }
    if (mo < 1 || mo > 12 || d < 1 || d > monthDays(y, mo) || h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 60) return Value();
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = se;
    tm.tm_isdst = -1;
    return Value((double)std::mktime(&tm));
}
std::string plural(long long n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }
NATIVE(time_duration) {
    double secs = std::fabs(argNum(vm, args, argc, 0, "Time.duration"));
    long long s = (long long)std::floor(secs);
    long long d = s / 86400, h = s % 86400 / 3600, m = s % 3600 / 60, r = s % 60;
    std::vector<std::string> parts;
    if (d) parts.push_back(std::to_string(d) + " d");
    if (h) parts.push_back(std::to_string(h) + " h");
    if (m) parts.push_back(std::to_string(m) + " min");
    if (r || parts.empty()) {
        if (secs < 10 && secs != std::floor(secs) && parts.empty()) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%.2f s", secs);
            std::string t = buf;
            for (auto& c : t) if (c == '.') c = ',';
            parts.push_back(t);
        } else parts.push_back(std::to_string(r) + " s");
    }
    std::string out;
    for (size_t i = 0; i < parts.size(); i++) { if (i) out += ' '; out += parts[i]; }
    return vm.str(out);
}
NATIVE(time_ago) {
    double t = argNum(vm, args, argc, 0, "Time.ago"), now = argNum(vm, args, argc, 1, "Time.ago", nowSecs());
    double diff = now - t;
    bool future = diff < 0;
    long long s = (long long)std::fabs(diff);
    std::string what;
    if (s < 45) what = "unos segundos";
    else if (s < 3600) what = plural(std::max(1LL, (s + 30) / 60), "minuto", "minutos");
    else if (s < 86400) what = plural((s + 1800) / 3600, "hora", "horas");
    else if (s < 86400 * 30) what = plural((s + 43200) / 86400, "día", "días");
    else if (s < 86400 * 365) what = plural(std::max(1LL, s / (86400 * 30)), "mes", "meses");
    else what = plural(s / (86400 * 365), "año", "años");
    return vm.str((future ? "dentro de " : "hace ") + what);
}

// ---------------------------------------------------------------------------
// JSON en archivos
// ---------------------------------------------------------------------------

Value jsonFn(VM& vm, const char* name) {
    Value json = vm.builtins[vm.builtinSlot("JSON")];
    return vm.getProperty(json, vm.intern(name), false);
}
Value fileFn(VM& vm, const char* name) {
    Value file = vm.builtins[vm.builtinSlot("File")];
    return vm.getProperty(file, vm.intern(name), false);
}
NATIVE(json_load) {
    Value path = vm.str(argStr(vm, args, argc, 0, "JSON.load"));
    Value text = vm.call(fileFn(vm, "read"), 1, &path);
    return vm.call(jsonFn(vm, "parse"), 1, &text);
}
NATIVE(json_save) {
    Value path = vm.str(argStr(vm, args, argc, 0, "JSON.save"));
    Value a[2] = {optArg(args, argc, 1), Value(argNum(vm, args, argc, 2, "JSON.save", 2))};
    Value text = vm.call(jsonFn(vm, "stringify"), 2, a);
    Value w[2] = {path, text};
    vm.call(fileFn(vm, "write"), 2, w);
    return Value();
}

} // namespace

void registerMore(VM& vm) {
    // Matemáticas
    vm.defineGlobalFn("gcd", "(a: number, b: number): number", "Máximo común divisor: gcd(12, 18) es 6.", n_gcd);
    vm.defineGlobalFn("lcm", "(a: number, b: number): number", "Mínimo común múltiplo: lcm(4, 6) es 12.", n_lcm);
    vm.defineGlobalFn("factorial", "(n: number): number", "n! = 1 × 2 × ... × n: factorial(5) es 120.", n_factorial);
    vm.defineGlobalFn("isPrime", "(n: number): bool", "¿Es un número primo? isPrime(7) es true.", n_isPrime);
    vm.defineGlobalFn("primes", "(hasta: number): list<number>", "Todos los primos hasta un número (criba de Eratóstenes): primes(20).", n_primes);
    vm.defineGlobalFn("divisors", "(n: number): list<number>", "Divisores de un número, ordenados: divisors(12) es [1, 2, 3, 4, 6, 12].", n_divisors);
    vm.defineGlobalFn("cbrt", "(x: number): number", "Raíz cúbica.", n_cbrt);
    vm.defineGlobalFn("trunc", "(x: number): number", "Quita los decimales (hacia el cero): trunc(-2.7) es -2.", n_trunc);
    vm.defineGlobalFn("fract", "(x: number): number", "Parte decimal: fract(3.25) es 0.25.", n_fract);
    vm.defineGlobalFn("sinh", "(x: number): number", "Seno hiperbólico.", n_sinh);
    vm.defineGlobalFn("cosh", "(x: number): number", "Coseno hiperbólico.", n_cosh);
    vm.defineGlobalFn("tanh", "(x: number): number", "Tangente hiperbólica.", n_tanh);
    vm.defineGlobalFn("randomGaussian", "(media?: number = 0, desviacion?: number = 1): number",
                      "Número aleatorio con distribución normal (campana de Gauss). Usa la semilla de randomSeed.", n_randomGaussian);
    vm.defineGlobalFn("noise", "(x: number, y?: number, z?: number): number",
                      "Ruido de Perlin entre -1 y 1 que cambia suavemente: terrenos, nubes, movimientos naturales.", n_noise,
                      "var altura = noise(x * 0.1, y * 0.1)");
    vm.defineGlobalFn("chr", "(codigo: number): text", "Carácter con ese código Unicode: chr(65) es \"A\", chr(241) es \"ñ\".", n_chr);
    vm.defineGlobalFn("ord", "(caracter: text): number", "Código Unicode del primer carácter: ord(\"A\") es 65.", n_ord);

    // text
    ObjClass* t = vm.textClass;
    vm.defineMethod(t, "removeAccents", "(): text", "Quita tildes y diéresis: \"Canción pingüino\" → \"Cancion pinguino\" (ñ → n).", t_removeAccents);
    vm.defineMethod(t, "title", "(): text", "Mayúscula al principio de cada palabra: \"hola mundo\" → \"Hola Mundo\".", t_title);
    vm.defineMethod(t, "camelCase", "(): text", "\"nombre del jugador\" → \"nombreDelJugador\".", t_camelCase);
    vm.defineMethod(t, "pascalCase", "(): text", "\"nombre del jugador\" → \"NombreDelJugador\".", t_pascalCase);
    vm.defineMethod(t, "snakeCase", "(): text", "\"Nombre del jugador\" → \"nombre_del_jugador\".", t_snakeCase);
    vm.defineMethod(t, "kebabCase", "(): text", "\"Nombre del jugador\" → \"nombre-del-jugador\".", t_kebabCase);
    vm.defineMethod(t, "slug", "(): text", "Texto apto para direcciones web: \"¡Hola, Señor!\" → \"hola-senor\".", t_slug);
    vm.defineMethod(t, "truncate", "(maximo: number, final?: text = \"…\"): text", "Recorta a 'maximo' caracteres añadiendo \"…\".", t_truncate);
    vm.defineMethod(t, "wrap", "(ancho?: number = 80): text", "Reparte el texto en líneas de como mucho 'ancho' caracteres sin cortar palabras.", t_wrap);
    vm.defineMethod(t, "distance", "(otro: text): number", "Distancia de edición (Levenshtein): cuántos cambios hay entre dos textos.", t_distance);
    vm.defineMethod(t, "similarity", "(otro: text): number", "Parecido entre 0 y 1 (sin distinguir mayúsculas): útil para \"¿quisiste decir...?\".",
                    t_similarity);
    vm.defineMethod(t, "indent", "(n?: number = 4, relleno?: text = \" \"): text", "Añade n espacios al principio de cada línea.", t_indent);
    vm.defineMethod(t, "dedent", "(): text", "Quita la sangría común de todas las líneas.", t_dedent);
    vm.defineMethod(t, "between", "(inicio: text, fin: text): text?", "El texto entre la primera aparición de inicio y la siguiente de fin (o null).",
                    t_between);
    vm.defineMethod(t, "isBlank", "(): bool", "¿Está vacío o solo tiene espacios?", t_isBlank);
    vm.defineMethod(t, "codes", "(): list<number>", "Códigos Unicode de cada carácter (lo contrario de chr).", t_codes);
    vm.defineMethod(t, "reverseWords", "(): text", "Invierte el orden de las palabras: \"uno dos tres\" → \"tres dos uno\".", t_reverseWords);

    // list
    ObjClass* l = vm.listClass;
    vm.defineMethod(l, "countBy", "(f?: function): map", "Cuántas veces aparece cada valor (o cada f(elemento)): [\"a\", \"b\", \"a\"].countBy() es {a: 2, b: 1}.",
                    l_countBy);
    vm.defineMethod(l, "sumBy", "(f: function): number", "Suma de f(elemento): jugadores.sumBy(j => j.puntos).", l_sumBy);
    vm.defineMethod(l, "uniqueBy", "(f: function): list", "Sin repetidos según f(elemento) (se queda con el primero).", l_uniqueBy);
    vm.defineMethod(l, "windowed", "(tamano: number, paso?: number = 1): list", "Ventanas deslizantes: [1, 2, 3, 4].windowed(2) es [[1, 2], [2, 3], [3, 4]].",
                    l_windowed);
    vm.defineMethod(l, "rotate", "(n?: number = 1): list", "Lista nueva girada n posiciones a la izquierda (negativo: a la derecha).", l_rotate);
    vm.defineMethod(l, "intersect", "(otra: list): list", "Elementos que están en las dos listas (sin repetir).", l_intersect);
    vm.defineMethod(l, "difference", "(otra: list): list", "Elementos que no están en la otra lista (sin repetir).", l_difference);
    vm.defineMethod(l, "union", "(otra: list): list", "Elementos de ambas listas sin repetir.", l_union);
    vm.defineMethod(l, "takeWhile", "(f: function): list", "Elementos del principio mientras f sea verdadera.", l_takeWhile);
    vm.defineMethod(l, "skipWhile", "(f: function): list", "Salta los del principio mientras f sea verdadera.", l_skipWhile);
    vm.defineMethod(l, "none", "(f: function): bool", "¿Ningún elemento cumple f?", l_none);
    vm.defineMethod(l, "sample", "(n: number): list", "n elementos al azar distintos (usa la semilla de randomSeed).", l_sample);
    vm.defineMethod(l, "toSet", "(): set", "Conjunto con los elementos (sin repetidos).", l_toSet);
    vm.defineMethod(l, "compact", "(): list", "Sin los valores null.", l_compact);
    vm.defineMethod(l, "product", "(): number", "Producto de todos los números.", l_product);
    vm.defineMethod(l, "lastIndexOf", "(valor: any): number", "Posición de la última aparición (o -1).", l_lastIndexOf);
    vm.defineMethod(l, "binarySearch", "(valor: any): number", "Búsqueda binaria en una lista ordenada: posición o -1. Muy rápida en listas grandes.",
                    l_binarySearch);
    vm.defineMethod(l, "scan", "(f: function, inicial?: any): list", "Como reduce, pero devuelve todos los pasos: [1, 2, 3].scan((a, b) => a + b) es [1, 3, 6].",
                    l_scan);
    vm.defineMethod(l, "enumerate", "(): list", "Pares [posición, elemento].", l_enumerate);
    vm.defineMethod(l, "toMap", "(clave: function, valor?: function): map", "Mapa con clave(elemento) → elemento (o valor(elemento)).", l_toMap);
    vm.defineMethod(l, "median", "(): number", "Mediana: el valor del medio una vez ordenados.", l_median);
    vm.defineMethod(l, "insertSorted", "(valor: any): number", "Inserta manteniendo la lista ordenada y devuelve la posición.", l_insertSorted);

    // map
    ObjClass* m = vm.mapClass;
    vm.defineMethod(m, "pick", "(claves: list): map", "Mapa nuevo solo con esas claves.", m_pick);
    vm.defineMethod(m, "omit", "(claves: list): map", "Mapa nuevo sin esas claves.", m_omit);
    vm.defineMethod(m, "invert", "(): map", "Intercambia claves y valores.", m_invert);
    vm.defineMethod(m, "update", "(clave: any, f: function, porDefecto?: any): any",
                    "Cambia el valor con f(valor actual o porDefecto) y lo devuelve: cuenta.update(\"a\", n => n + 1, 0).", m_update);
    vm.defineMethod(m, "any", "(f: function): bool", "¿Alguna entrada cumple f(valor, clave)?", m_any);
    vm.defineMethod(m, "all", "(f: function): bool", "¿Todas las entradas cumplen f(valor, clave)?", m_all);
    vm.defineMethod(m, "count", "(f: function): number", "Cuántas entradas cumplen f(valor, clave).", m_count);
    vm.defineMethod(m, "find", "(f: function): any", "Primera clave cuya entrada cumple f(valor, clave) (o null).", m_find);
    vm.defineMethod(m, "mapKeys", "(f: function): map", "Mapa nuevo con f(clave, valor) como claves.", m_mapKeys);
    vm.defineMethod(m, "sortByKey", "(descendente?: bool = false): map", "Mapa nuevo ordenado por las claves.", m_sortByKey);
    vm.defineMethod(m, "sortByValue", "(descendente?: bool = false): map", "Mapa nuevo ordenado por los valores (p. ej. un ranking).", m_sortByValue);

    // set
    ObjClass* s = vm.setClass;
    vm.defineMethod(s, "union", "(otro: set|list): set", "Elementos de los dos conjuntos.", s_union);
    vm.defineMethod(s, "intersect", "(otro: set|list): set", "Elementos comunes.", s_intersect);
    vm.defineMethod(s, "difference", "(otro: set|list): set", "Elementos que no están en el otro.", s_difference);
    vm.defineMethod(s, "symmetricDifference", "(otro: set|list): set", "Elementos que están en uno solo de los dos.", s_symmetricDifference);
    vm.defineMethod(s, "isSubset", "(otro: set|list): bool", "¿Todos sus elementos están en el otro?", s_isSubset);
    vm.defineMethod(s, "isSuperset", "(otro: set|list): bool", "¿Contiene todos los elementos del otro?", s_isSuperset);
    vm.defineMethod(s, "isDisjoint", "(otro: set|list): bool", "¿No tienen ningún elemento en común?", s_isDisjoint);
    vm.defineMethod(s, "map", "(f: function): set", "Conjunto con f(elemento).", s_map);
    vm.defineMethod(s, "filter", "(f: function): set", "Conjunto con los elementos que cumplen f.", s_filter);

    // number
    ObjClass* n = vm.numberClass;
    vm.defineMethod(n, "isEven", "(): bool", "¿Es par?", num_isEven);
    vm.defineMethod(n, "isOdd", "(): bool", "¿Es impar?", num_isOdd);
    vm.defineMethod(n, "clamp", "(minimo: number, maximo: number): number", "Limita el número al intervalo.", num_clamp);
    vm.defineMethod(n, "between", "(minimo: number, maximo: number): bool", "¿Está entre minimo y maximo (incluidos)?", num_between);
    vm.defineMethod(n, "toRoman", "(): text", "Número romano: (2026).toRoman() es \"MMXXVI\".", num_toRoman);
    vm.defineMethod(n, "format", "(decimales?: number = 0, miles?: text = \".\", decimal?: text = \",\"): text",
                    "Con separador de miles: (1234567.5).format(2) es \"1.234.567,50\".", num_format);
    vm.defineMethod(n, "digits", "(): list<number>", "Cifras de la parte entera: (2026).digits() es [2, 0, 2, 6].", num_digits);
    vm.defineMethod(n, "times", "(f: function): void", "Repite f n veces (recibe 0, 1, 2...): (3).times(i => print(i)).", num_times);
    vm.defineMethod(n, "toPercent", "(decimales?: number = 0): text", "Porcentaje: (0.256).toPercent(1) es \"25,6 %\".", num_toPercent);

    // Time
    ObjModule* time = existingModule(vm, "Time");
    vm.defineFn(time, "today", "(): number", "Hoy a las 00:00 (hora local).", time_today);
    vm.defineFn(time, "addDays", "(momento: number, dias: number): number", "Suma (o resta) días respetando el horario de verano.", time_addDays);
    vm.defineFn(time, "addMonths", "(momento: number, meses: number): number", "Suma meses: el 31 de enero + 1 mes es el último día de febrero.",
                time_addMonths);
    vm.defineFn(time, "daysBetween", "(desde: number, hasta: number): number", "Días de calendario entre dos momentos.", time_daysBetween);
    vm.defineFn(time, "isLeapYear", "(anio: number): bool", "¿Es bisiesto?", time_isLeapYear);
    vm.defineFn(time, "daysInMonth", "(anio: number, mes: number): number", "Días que tiene un mes (1 a 12).", time_daysInMonth);
    vm.defineFn(time, "monthName", "(mes: number): text", "Nombre del mes en español (1 = \"enero\").", time_monthName);
    vm.defineFn(time, "dayName", "(diaSemana: number): text", "Nombre del día en español (0 = \"domingo\").", time_dayName);
    vm.defineFn(time, "parse", "(texto: text): number?",
                "Lee una fecha \"2026-11-04\", \"2026-11-04 18:30\" o \"04/11/2026\" (null si no es válida).", time_parse);
    vm.defineFn(time, "duration", "(segundos: number): text", "Duración legible: Time.duration(3725) es \"1 h 2 min 5 s\".", time_duration);
    vm.defineFn(time, "ago", "(momento: number, ahora?: number): text", "Tiempo relativo: \"hace 5 minutos\", \"dentro de 2 días\".", time_ago);

    // JSON en archivos
    ObjModule* json = existingModule(vm, "JSON");
    vm.defineFn(json, "load", "(ruta: text): any", "Lee un archivo JSON y lo convierte en mapas y listas.", json_load,
                "var ajustes = JSON.load(\"ajustes.json\")");
    vm.defineFn(json, "save", "(ruta: text, valor: any, sangria?: number = 2): void", "Guarda un valor como archivo JSON (legible).", json_save);
}

} // namespace hz
