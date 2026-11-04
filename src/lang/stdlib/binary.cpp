// Datos binarios: Bytes (bloques de bytes con lectura y escritura de enteros y decimales),
// Hash (crc32, fnv1a, sha256, sha1, md5, hmac, uuid), Base64 y File.readBytes/writeBytes.
// Base para formatos de archivo, protocolos, emuladores e imágenes de disco.
#include "lang/stdlib/native.h"
#include "core/process.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <random>

namespace hz {

using Buf = std::vector<uint8_t>;

static void freeBuf(void* p) { delete static_cast<Buf*>(p); }
static ObjClass* bytesClass(VM& vm) { return static_cast<ObjClass*>(vm.builtins[vm.builtinSlot("Bytes")].o); }
static Value makeBytes(VM& vm, Buf data) {
    return Value::object(vm.newForeign(bytesClass(vm), new Buf(std::move(data)), freeBuf));
}
static bool isBytes(VM& vm, const Value& v) {
    return isObjType(v, OT::Foreign) && static_cast<ObjForeign*>(v.o)->cls == bytesClass(vm);
}
static Buf& B(Value* args) { return *static_cast<Buf*>(static_cast<ObjForeign*>(args[-1].o)->ptr); }
static Buf& asBuf(VM& vm, const Value& v, const char* fname) {
    if (!isBytes(vm, v)) vm.raise(std::string(fname) + ": se esperaba Bytes, no " + vm.typeName(v));
    return *static_cast<Buf*>(static_cast<ObjForeign*>(v.o)->ptr);
}
// Texto o Bytes como secuencia de bytes
static Buf dataOf(VM& vm, const Value& v, const char* fname) {
    if (isString(v)) { auto& s = asString(v)->str; return Buf(s.begin(), s.end()); }
    if (isBytes(vm, v)) return *static_cast<Buf*>(static_cast<ObjForeign*>(v.o)->ptr);
    vm.raise(std::string(fname) + ": se esperaba un texto o Bytes, no " + vm.typeName(v));
}
static uint8_t byteValue(VM& vm, const Value& v, const char* fname) {
    if (!v.isNum()) vm.raise(std::string(fname) + ": cada byte debe ser un número de 0 a 255, no " + vm.typeName(v));
    double d = v.n;
    if (d < -128 || d > 255 || std::floor(d) != d) vm.raise(std::string(fname) + ": " + formatNumber(d) + " no es un byte (0 a 255)");
    return (uint8_t)(int)d;
}
static size_t indexArg(VM& vm, Value* args, int argc, int i, size_t len, size_t width, const char* fname) {
    double d = argNum(vm, args, argc, i, fname);
    if (d < 0) d += (double)len;
    if (d < 0 || std::floor(d) != d || d + width > (double)len)
        vm.raise(std::string(fname) + ": posición " + formatNumber(d) + " fuera de rango (hay " + std::to_string(len) + " bytes)");
    return (size_t)d;
}

Value newBytes(VM& vm, Buf data) { return makeBytes(vm, std::move(data)); }
Buf* bytesData(VM& vm, const Value& v) { return isBytes(vm, v) ? static_cast<Buf*>(static_cast<ObjForeign*>(v.o)->ptr) : nullptr; }

// ---- Bytes --------------------------------------------------------------

NATIVE(c_bytes) {
    Buf b;
    Value v = optArg(args, argc, 0);
    if (v.isNum()) {
        if (v.n < 0 || v.n > 1e9) vm.raise("Bytes: tamaño no válido");
        b.assign((size_t)v.n, (uint8_t)(argc > 1 ? byteValue(vm, args[1], "Bytes") : 0));
    } else if (isString(v)) {
        auto& s = asString(v)->str;
        b.assign(s.begin(), s.end());
    } else if (isObjType(v, OT::List)) {
        for (auto& x : static_cast<ObjList*>(v.o)->items) b.push_back(byteValue(vm, x, "Bytes"));
    } else if (isBytes(vm, v)) {
        b = asBuf(vm, v, "Bytes");
    } else if (!v.isNull()) {
        vm.raise("Bytes: se esperaba un tamaño, una lista de números o un texto");
    }
    auto* cls = static_cast<ObjClass*>(args[-1].o);
    return Value::object(vm.newForeign(cls, new Buf(std::move(b)), freeBuf));
}
NATIVE(b_length) { UNUSED; return Value((double)B(args).size()); }
NATIVE(b_get) {
    auto& b = B(args);
    return Value((double)b[indexArg(vm, args, argc, 0, b.size(), 1, "Bytes.get")]);
}
NATIVE(b_set) {
    auto& b = B(args);
    b[indexArg(vm, args, argc, 0, b.size(), 1, "Bytes.set")] = byteValue(vm, optArg(args, argc, 1), "Bytes.set");
    return Value();
}
NATIVE(b_push) {
    auto& b = B(args);
    for (int i = 0; i < argc; i++) {
        if (isObjType(args[i], OT::List)) for (auto& x : static_cast<ObjList*>(args[i].o)->items) b.push_back(byteValue(vm, x, "Bytes.push"));
        else if (isString(args[i])) { auto& s = asString(args[i])->str; b.insert(b.end(), s.begin(), s.end()); }
        else if (isBytes(vm, args[i])) { Buf o = asBuf(vm, args[i], "Bytes.push"); b.insert(b.end(), o.begin(), o.end()); }
        else b.push_back(byteValue(vm, args[i], "Bytes.push"));
    }
    return args[-1];
}
NATIVE(b_slice) {
    auto& b = B(args);
    double n = (double)b.size();
    double a = argNum(vm, args, argc, 0, "Bytes.slice", 0), e = argNum(vm, args, argc, 1, "Bytes.slice", n);
    if (a < 0) a += n;
    if (e < 0) e += n;
    a = std::clamp(a, 0.0, n);
    e = std::clamp(e, a, n);
    return makeBytes(vm, Buf(b.begin() + (size_t)a, b.begin() + (size_t)e));
}
NATIVE(b_concat) {
    Buf r = B(args);
    Buf o = dataOf(vm, optArg(args, argc, 0), "Bytes.concat");
    r.insert(r.end(), o.begin(), o.end());
    return makeBytes(vm, std::move(r));
}
NATIVE(b_fill) {
    uint8_t v = byteValue(vm, optArg(args, argc, 0), "Bytes.fill");
    std::fill(B(args).begin(), B(args).end(), v);
    return args[-1];
}
NATIVE(b_resize) {
    double n = argNum(vm, args, argc, 0, "Bytes.resize");
    if (n < 0 || n > 1e9) vm.raise("Bytes.resize: tamaño no válido");
    B(args).resize((size_t)n, 0);
    return args[-1];
}
NATIVE(b_copy) { UNUSED; return makeBytes(vm, B(args)); }
NATIVE(b_toList) {
    UNUSED;
    ObjList* l = vm.newList();
    for (uint8_t x : B(args)) l->items.push_back(Value((double)x));
    return Value::object(l);
}
static const char* kHex = "0123456789abcdef";
static std::string toHex(const Buf& b, const std::string& sep = "") {
    std::string s;
    s.reserve(b.size() * (2 + sep.size()));
    for (size_t i = 0; i < b.size(); i++) {
        if (i && !sep.empty()) s += sep;
        s += kHex[b[i] >> 4];
        s += kHex[b[i] & 15];
    }
    return s;
}
NATIVE(b_toHex) { return vm.str(toHex(B(args), argStr(vm, args, argc, 0, "Bytes.toHex", ""))); }
NATIVE(b_toText) { UNUSED; auto& b = B(args); return vm.str(std::string(b.begin(), b.end())); }
NATIVE(b_indexOf) {
    auto& b = B(args);
    Value v = optArg(args, argc, 0);
    Buf needle = v.isNum() ? Buf{byteValue(vm, v, "Bytes.indexOf")} : dataOf(vm, v, "Bytes.indexOf");
    size_t from = (size_t)std::max(0.0, argNum(vm, args, argc, 1, "Bytes.indexOf", 0));
    if (needle.empty() || from >= b.size()) return Value(-1);
    auto it = std::search(b.begin() + from, b.end(), needle.begin(), needle.end());
    return Value(it == b.end() ? -1.0 : (double)(it - b.begin()));
}
NATIVE(b_equals) {
    Value v = optArg(args, argc, 0);
    return Value::boolean(isBytes(vm, v) && asBuf(vm, v, "Bytes.equals") == B(args));
}
NATIVE(b_toString) {
    UNUSED;
    auto& b = B(args);
    Buf head(b.begin(), b.begin() + std::min<size_t>(b.size(), 16));
    return vm.str("Bytes(" + std::to_string(b.size()) + (b.empty() ? "" : ": " + toHex(head, " ")) + (b.size() > 16 ? " ...)" : ")"));
}

// Lectura y escritura de números: entero sin signo / con signo de 8, 16, 32 y 64 bits, y decimales de 32 y 64.
template <class T> static T loadLE(const uint8_t* p, bool little) {
    T v{};
    uint8_t tmp[sizeof(T)];
    for (size_t i = 0; i < sizeof(T); i++) tmp[i] = little ? p[i] : p[sizeof(T) - 1 - i];
    std::memcpy(&v, tmp, sizeof(T));
    return v;
}
template <class T> static void storeLE(uint8_t* p, T v, bool little) {
    uint8_t tmp[sizeof(T)];
    std::memcpy(tmp, &v, sizeof(T));
    for (size_t i = 0; i < sizeof(T); i++) p[i] = little ? tmp[i] : tmp[sizeof(T) - 1 - i];
}
template <class T> static T toInt(VM& vm, double d, const char* fname) {
    if (!std::isfinite(d)) vm.raise(std::string(fname) + ": el valor no es un número finito");
    return (T)(int64_t)d;
}
#define READER(NAME, T)                                                                           \
    NATIVE(NAME) {                                                                                \
        auto& b = B(args);                                                                        \
        size_t at = indexArg(vm, args, argc, 0, b.size(), sizeof(T), "Bytes.read");               \
        return Value((double)loadLE<T>(b.data() + at, argBool(args, argc, 1, true)));             \
    }
#define WRITER(NAME, T, CONV)                                                                     \
    NATIVE(NAME) {                                                                                \
        auto& b = B(args);                                                                        \
        size_t at = indexArg(vm, args, argc, 0, b.size(), sizeof(T), "Bytes.write");              \
        double d = argNum(vm, args, argc, 1, "Bytes.write");                                      \
        storeLE<T>(b.data() + at, CONV, argBool(args, argc, 2, true));                            \
        return Value();                                                                           \
    }
#define PUSHER(NAME, T, CONV)                                                                     \
    NATIVE(NAME) {                                                                                \
        auto& b = B(args);                                                                        \
        double d = argNum(vm, args, argc, 0, "Bytes.push");                                       \
        size_t at = b.size();                                                                     \
        b.resize(at + sizeof(T));                                                                 \
        storeLE<T>(b.data() + at, CONV, argBool(args, argc, 1, true));                            \
        return args[-1];                                                                          \
    }
READER(b_readU8, uint8_t) READER(b_readI8, int8_t) READER(b_readU16, uint16_t) READER(b_readI16, int16_t)
READER(b_readU32, uint32_t) READER(b_readI32, int32_t) READER(b_readI64, int64_t) READER(b_readF32, float) READER(b_readF64, double)
WRITER(b_writeU8, uint8_t, toInt<uint8_t>(vm, d, "Bytes.writeU8")) WRITER(b_writeU16, uint16_t, toInt<uint16_t>(vm, d, "Bytes.writeU16"))
WRITER(b_writeU32, uint32_t, toInt<uint32_t>(vm, d, "Bytes.writeU32")) WRITER(b_writeI64, int64_t, toInt<int64_t>(vm, d, "Bytes.writeI64"))
WRITER(b_writeF32, float, (float)d) WRITER(b_writeF64, double, d)
PUSHER(b_pushU16, uint16_t, toInt<uint16_t>(vm, d, "Bytes.pushU16")) PUSHER(b_pushU32, uint32_t, toInt<uint32_t>(vm, d, "Bytes.pushU32"))
PUSHER(b_pushI64, int64_t, toInt<int64_t>(vm, d, "Bytes.pushI64")) PUSHER(b_pushF32, float, (float)d) PUSHER(b_pushF64, double, d)

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
NATIVE(b_fromHex) {
    std::string s = argStr(vm, args, argc, 0, "Bytes.fromHex");
    Buf b;
    int hi = -1;
    for (char c : s) {
        if (c == ' ' || c == ':' || c == '-' || c == '\n' || c == '\r' || c == '\t') continue;
        int d = hexDigit(c);
        if (d < 0) vm.raise(std::string("Bytes.fromHex: '") + c + "' no es un dígito hexadecimal");
        if (hi < 0) hi = d; else { b.push_back((uint8_t)(hi * 16 + d)); hi = -1; }
    }
    if (hi >= 0) vm.raise("Bytes.fromHex: falta un dígito (el número de dígitos debe ser par)");
    return makeBytes(vm, std::move(b));
}

// ---- Base64 -------------------------------------------------------------

static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64Encode(const Buf& in) {
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        uint32_t n = in[i] << 16 | (i + 1 < in.size() ? in[i + 1] << 8 : 0) | (i + 2 < in.size() ? in[i + 2] : 0);
        out += kB64[n >> 18 & 63];
        out += kB64[n >> 12 & 63];
        out += i + 1 < in.size() ? kB64[n >> 6 & 63] : '=';
        out += i + 2 < in.size() ? kB64[n & 63] : '=';
    }
    return out;
}
static bool base64Decode(const std::string& in, Buf& out) {
    int val = 0, bits = -8;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        const char* p = std::strchr(kB64, c);
        if (c == '-') p = kB64 + 62; // variante para URL
        else if (c == '_') p = kB64 + 63;
        if (!p || !c) return false;
        val = (val << 6) + (int)(p - kB64);
        bits += 6;
        if (bits >= 0) {
            out.push_back((uint8_t)((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return true;
}
NATIVE(b_toBase64) { UNUSED; return vm.str(base64Encode(B(args))); }
NATIVE(b_fromBase64) {
    Buf b;
    if (!base64Decode(argStr(vm, args, argc, 0, "Bytes.fromBase64"), b)) vm.raise("Bytes.fromBase64: el texto no es Base64 válido");
    return makeBytes(vm, std::move(b));
}
NATIVE(b64_encode) { return vm.str(base64Encode(dataOf(vm, optArg(args, argc, 0), "Base64.encode"))); }
NATIVE(b64_decode) {
    Buf b;
    if (!base64Decode(argStr(vm, args, argc, 0, "Base64.decode"), b)) vm.raise("Base64.decode: el texto no es Base64 válido");
    return vm.str(std::string(b.begin(), b.end()));
}
NATIVE(b64_decodeBytes) {
    Buf b;
    if (!base64Decode(argStr(vm, args, argc, 0, "Base64.decodeBytes"), b)) vm.raise("Base64.decodeBytes: el texto no es Base64 válido");
    return makeBytes(vm, std::move(b));
}

// ---- Hash ---------------------------------------------------------------

static uint32_t crc32(const Buf& d) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : d) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static uint32_t fnv1a(const Buf& d) {
    uint32_t h = 2166136261u;
    for (uint8_t b : d) { h ^= b; h *= 16777619u; }
    return h;
}

// SHA-256 (FIPS 180-4)
static Buf sha256(const Buf& msg) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
        0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
        0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    Buf m = msg;
    uint64_t bitLen = (uint64_t)msg.size() * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; i--) m.push_back((uint8_t)(bitLen >> (i * 8)));
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)m[off + i * 4] << 24 | (uint32_t)m[off + i * 4 + 1] << 16 | (uint32_t)m[off + i * 4 + 2] << 8 | m[off + i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    Buf out;
    for (uint32_t x : h) for (int i = 3; i >= 0; i--) out.push_back((uint8_t)(x >> (i * 8)));
    return out;
}

// SHA-1 (FIPS 180-4): el de Git; no usar para seguridad nueva
static Buf sha1(const Buf& msg) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    Buf m = msg;
    uint64_t bitLen = (uint64_t)msg.size() * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; i--) m.push_back((uint8_t)(bitLen >> (i * 8)));
    auto rotl = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)m[off + i * 4] << 24 | (uint32_t)m[off + i * 4 + 1] << 16 | (uint32_t)m[off + i * 4 + 2] << 8 | m[off + i * 4 + 3];
        for (int i = 16; i < 80; i++) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    Buf out;
    for (uint32_t x : h) for (int i = 3; i >= 0; i--) out.push_back((uint8_t)(x >> (i * 8)));
    return out;
}

// MD5 (RFC 1321): sumas de comprobación de descargas antiguas; no usar para seguridad
static Buf md5(const Buf& msg) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
        0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
        0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    Buf m = msg;
    uint64_t bitLen = (uint64_t)msg.size() * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 0; i < 8; i++) m.push_back((uint8_t)(bitLen >> (i * 8)));
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)m[off + i * 4] | (uint32_t)m[off + i * 4 + 1] << 8 | (uint32_t)m[off + i * 4 + 2] << 16 | (uint32_t)m[off + i * 4 + 3] << 24;
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; i++) {
            uint32_t f;
            int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            uint32_t t = d;
            d = c;
            c = b;
            uint32_t x = a + f + K[i] + w[g];
            b = b + ((x << R[i]) | (x >> (32 - R[i])));
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    Buf out;
    for (uint32_t x : h) for (int i = 0; i < 4; i++) out.push_back((uint8_t)(x >> (i * 8)));
    return out;
}

// HMAC-SHA256 (RFC 2104): firmar mensajes con una clave (APIs web, tokens)
static Buf hmacSha256(Buf key, const Buf& msg) {
    if (key.size() > 64) key = sha256(key);
    key.resize(64, 0);
    Buf inner(64), outer(64);
    for (int i = 0; i < 64; i++) { inner[i] = key[i] ^ 0x36; outer[i] = key[i] ^ 0x5c; }
    inner.insert(inner.end(), msg.begin(), msg.end());
    Buf ih = sha256(inner);
    outer.insert(outer.end(), ih.begin(), ih.end());
    return sha256(outer);
}

NATIVE(hash_sha1) { return vm.str(toHex(sha1(dataOf(vm, optArg(args, argc, 0), "Hash.sha1")))); }
NATIVE(hash_md5) { return vm.str(toHex(md5(dataOf(vm, optArg(args, argc, 0), "Hash.md5")))); }
NATIVE(hash_hmac) {
    Buf key = dataOf(vm, optArg(args, argc, 0), "Hash.hmac"), msg = dataOf(vm, optArg(args, argc, 1), "Hash.hmac");
    return vm.str(toHex(hmacSha256(key, msg)));
}

NATIVE(hash_crc32) { return Value((double)crc32(dataOf(vm, optArg(args, argc, 0), "Hash.crc32"))); }
NATIVE(hash_fnv1a) { return Value((double)fnv1a(dataOf(vm, optArg(args, argc, 0), "Hash.fnv1a"))); }
NATIVE(hash_sha256) { return vm.str(toHex(sha256(dataOf(vm, optArg(args, argc, 0), "Hash.sha256")))); }
NATIVE(hash_sha256Bytes) { return makeBytes(vm, sha256(dataOf(vm, optArg(args, argc, 0), "Hash.sha256Bytes"))); }
NATIVE(hash_uuid) {
    UNUSED;
    static std::mt19937_64 rng(std::random_device{}() ^ (uint64_t)nowSeconds() ^ (uint64_t)currentProcessId() << 32);
    Buf b(16);
    for (auto& x : b) x = (uint8_t)(rng() & 0xFF);
    b[6] = (b[6] & 0x0F) | 0x40; // versión 4
    b[8] = (b[8] & 0x3F) | 0x80; // variante RFC 4122
    std::string h = toHex(b);
    return vm.str(h.substr(0, 8) + "-" + h.substr(8, 4) + "-" + h.substr(12, 4) + "-" + h.substr(16, 4) + "-" + h.substr(20));
}

// ---- Números en otras bases ---------------------------------------------

static std::string toBase(VM& vm, double d, int base, int digits, const char* fname) {
    if (base < 2 || base > 36) vm.raise(std::string(fname) + ": la base debe estar entre 2 y 36");
    if (!std::isfinite(d) || std::floor(d) != d) vm.raise(std::string(fname) + ": solo funciona con números enteros, no " + formatNumber(d));
    bool neg = d < 0;
    uint64_t n = (uint64_t)(neg ? -d : d);
    std::string s;
    do { s += "0123456789abcdefghijklmnopqrstuvwxyz"[n % (uint64_t)base]; n /= (uint64_t)base; } while (n);
    while ((int)s.size() < digits) s += '0';
    std::reverse(s.begin(), s.end());
    return neg ? "-" + s : s;
}
NATIVE(num_toHex) { return vm.str(toBase(vm, args[-1].n, 16, (int)argNum(vm, args, argc, 0, "toHex", 0), "toHex")); }
NATIVE(num_toBinary) { return vm.str(toBase(vm, args[-1].n, 2, (int)argNum(vm, args, argc, 0, "toBinary", 0), "toBinary")); }
NATIVE(num_toBase) {
    int base = (int)argNum(vm, args, argc, 0, "toBase");
    return vm.str(toBase(vm, args[-1].n, base, (int)argNum(vm, args, argc, 1, "toBase", 0), "toBase"));
}
NATIVE(n_parseInt) {
    std::string s = trim(argStr(vm, args, argc, 0, "parseInt"));
    int base = (int)argNum(vm, args, argc, 1, "parseInt", 10);
    if (base < 2 || base > 36) vm.raise("parseInt: la base debe estar entre 2 y 36");
    bool neg = !s.empty() && s[0] == '-';
    if (neg || (!s.empty() && s[0] == '+')) s.erase(0, 1);
    auto prefix = [&](const char* p) { return s.size() > 2 && s[0] == '0' && std::tolower((unsigned char)s[1]) == p[1]; };
    if (base == 16 && prefix("0x")) s.erase(0, 2);
    else if (base == 2 && prefix("0b")) s.erase(0, 2);
    else if (base == 8 && prefix("0o")) s.erase(0, 2);
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    if (s.empty()) return Value();
    double r = 0;
    for (char c : s) {
        int d = std::isdigit((unsigned char)c) ? c - '0' : std::isalpha((unsigned char)c) ? std::tolower((unsigned char)c) - 'a' + 10 : 99;
        if (d >= base) return Value(); // no es un número en esa base
        r = r * base + d;
    }
    return Value(neg ? -r : r);
}

// ---- File (binario) -----------------------------------------------------

NATIVE(file_readBytes) {
    std::string p = argStr(vm, args, argc, 0, "File.readBytes");
    std::string out;
    if (!readFile(resolvePath(vm, p), out)) vm.raise("File.readBytes: no se puede leer el archivo '" + p + "'");
    return makeBytes(vm, Buf(out.begin(), out.end()));
}
NATIVE(file_writeBytes) {
    std::string p = argStr(vm, args, argc, 0, "File.writeBytes");
    Buf d = dataOf(vm, optArg(args, argc, 1), "File.writeBytes");
    std::string full = resolvePath(vm, p);
    if (!pathDir(full).empty()) makeDirs(pathDir(full));
    if (!writeFile(full, std::string_view((const char*)d.data(), d.size()))) vm.raise("File.writeBytes: no se puede escribir '" + p + "'");
    return Value();
}

void registerBinaryLibs(VM& vm) {
    ObjClass* c = vm.nativeClass("Bytes",
        "Bloque de bytes (números de 0 a 255) para datos binarios: formatos de archivo, protocolos, imágenes de disco... "
        "Se lee y escribe con b[i], y con readU16/writeU32... para números de varios bytes.",
        c_bytes, "(contenido?: number|text|list = 0, relleno?: number = 0)");
    vm.defineClassProp(c, "length", "number", "Número de bytes.", b_length);
    vm.defineMethod(c, "get", "(i: number): number", "Byte en la posición i (también b[i]).", b_get);
    vm.defineMethod(c, "set", "(i: number, valor: number): void", "Cambia el byte de la posición i (también b[i] = valor).", b_set);
    vm.defineMethod(c, "push", "(...valores: any): Bytes", "Añade al final números (bytes), listas, textos u otros Bytes.", b_push);
    vm.defineMethod(c, "pushU16", "(valor: number, littleEndian?: bool = true): Bytes", "Añade un entero de 16 bits.", b_pushU16);
    vm.defineMethod(c, "pushU32", "(valor: number, littleEndian?: bool = true): Bytes", "Añade un entero de 32 bits.", b_pushU32);
    vm.defineMethod(c, "pushI64", "(valor: number, littleEndian?: bool = true): Bytes", "Añade un entero de 64 bits.", b_pushI64);
    vm.defineMethod(c, "pushF32", "(valor: number, littleEndian?: bool = true): Bytes", "Añade un decimal de 32 bits.", b_pushF32);
    vm.defineMethod(c, "pushF64", "(valor: number, littleEndian?: bool = true): Bytes", "Añade un decimal de 64 bits.", b_pushF64);
    const char* rdoc = "(posicion: number, littleEndian?: bool = true): number";
    vm.defineMethod(c, "readU8", rdoc, "Lee un byte sin signo.", b_readU8);
    vm.defineMethod(c, "readI8", rdoc, "Lee un byte con signo (-128 a 127).", b_readI8);
    vm.defineMethod(c, "readU16", rdoc, "Lee un entero de 16 bits sin signo.", b_readU16);
    vm.defineMethod(c, "readI16", rdoc, "Lee un entero de 16 bits con signo.", b_readI16);
    vm.defineMethod(c, "readU32", rdoc, "Lee un entero de 32 bits sin signo.", b_readU32);
    vm.defineMethod(c, "readI32", rdoc, "Lee un entero de 32 bits con signo.", b_readI32);
    vm.defineMethod(c, "readI64", rdoc, "Lee un entero de 64 bits con signo.", b_readI64);
    vm.defineMethod(c, "readF32", rdoc, "Lee un decimal de 32 bits (float).", b_readF32);
    vm.defineMethod(c, "readF64", rdoc, "Lee un decimal de 64 bits (double).", b_readF64);
    const char* wdoc = "(posicion: number, valor: number, littleEndian?: bool = true): void";
    vm.defineMethod(c, "writeU8", wdoc, "Escribe un byte.", b_writeU8);
    vm.defineMethod(c, "writeU16", wdoc, "Escribe un entero de 16 bits.", b_writeU16);
    vm.defineMethod(c, "writeU32", wdoc, "Escribe un entero de 32 bits.", b_writeU32);
    vm.defineMethod(c, "writeI64", wdoc, "Escribe un entero de 64 bits.", b_writeI64);
    vm.defineMethod(c, "writeF32", wdoc, "Escribe un decimal de 32 bits.", b_writeF32);
    vm.defineMethod(c, "writeF64", wdoc, "Escribe un decimal de 64 bits.", b_writeF64);
    vm.defineMethod(c, "slice", "(desde?: number = 0, hasta?: number): Bytes", "Copia de una parte (hasta no incluido; admite negativos).", b_slice);
    vm.defineMethod(c, "concat", "(otro: Bytes|text): Bytes", "Nuevos Bytes con los dos bloques seguidos.", b_concat);
    vm.defineMethod(c, "fill", "(valor: number): Bytes", "Pone el mismo byte en todas las posiciones.", b_fill);
    vm.defineMethod(c, "resize", "(tamano: number): Bytes", "Cambia el tamaño (los bytes nuevos valen 0).", b_resize);
    vm.defineMethod(c, "copy", "(): Bytes", "Copia independiente.", b_copy);
    vm.defineMethod(c, "indexOf", "(buscado: number|text|Bytes, desde?: number = 0): number", "Posición de un byte o secuencia, o -1.", b_indexOf);
    vm.defineMethod(c, "equals", "(otro: Bytes): bool", "¿Tienen exactamente los mismos bytes?", b_equals);
    vm.defineMethod(c, "toList", "(): list<number>", "Los bytes como lista de números.", b_toList);
    vm.defineMethod(c, "toHex", "(separador?: text = \"\"): text", "Representación hexadecimal: \"48656c6c6f\".", b_toHex);
    vm.defineMethod(c, "toText", "(): text", "Interpreta los bytes como texto UTF-8.", b_toText);
    vm.defineMethod(c, "toBase64", "(): text", "Codifica en Base64.", b_toBase64);
    vm.defineMethod(c, "toString", "(): text", "Descripción corta.", b_toString);
    vm.defineStatic(c, "fromHex", "(hex: text): Bytes", "Bytes a partir de texto hexadecimal (\"de ad be ef\").", b_fromHex);
    vm.defineStatic(c, "fromBase64", "(texto: text): Bytes", "Bytes a partir de texto Base64.", b_fromBase64);

    ObjModule* hash = vm.nativeModule("Hash", "Huellas de datos: comprobar integridad, comparar archivos, identificadores únicos.");
    vm.defineFn(hash, "crc32", "(datos: text|Bytes): number", "Suma de comprobación CRC-32 (la de ZIP y PNG).", hash_crc32);
    vm.defineFn(hash, "fnv1a", "(datos: text|Bytes): number", "Hash FNV-1a de 32 bits (rápido; no criptográfico).", hash_fnv1a);
    vm.defineFn(hash, "sha256", "(datos: text|Bytes): text", "Huella SHA-256 en hexadecimal (64 caracteres).", hash_sha256,
                "print(Hash.sha256(\"hola\"))");
    vm.defineFn(hash, "sha256Bytes", "(datos: text|Bytes): Bytes", "Huella SHA-256 como 32 Bytes.", hash_sha256Bytes);
    vm.defineFn(hash, "uuid", "(): text", "Identificador único aleatorio (UUID versión 4).", hash_uuid);
    vm.defineFn(hash, "sha1", "(datos: text|Bytes): text", "Huella SHA-1 en hexadecimal (40 caracteres; la de Git). No la uses para seguridad.", hash_sha1);
    vm.defineFn(hash, "md5", "(datos: text|Bytes): text", "Huella MD5 en hexadecimal (32 caracteres). Solo para comprobar archivos, no para seguridad.", hash_md5);
    vm.defineFn(hash, "hmac", "(clave: text|Bytes, mensaje: text|Bytes): text",
                "Firma HMAC-SHA256 en hexadecimal: comprueba que un mensaje viene de quien tiene la clave.", hash_hmac);

    ObjModule* b64 = vm.nativeModule("Base64", "Codificar datos binarios como texto (Base64) y al revés.");
    vm.defineFn(b64, "encode", "(datos: text|Bytes): text", "Codifica en Base64.", b64_encode);
    vm.defineFn(b64, "decode", "(texto: text): text", "Decodifica Base64 a texto.", b64_decode);
    vm.defineFn(b64, "decodeBytes", "(texto: text): Bytes", "Decodifica Base64 a Bytes.", b64_decodeBytes);

    vm.defineMethod(vm.numberClass, "toHex", "(digitos?: number = 0): text", "Entero en hexadecimal: (255).toHex() es \"ff\"; con dígitos rellena con ceros.", num_toHex);
    vm.defineMethod(vm.numberClass, "toBinary", "(digitos?: number = 0): text", "Entero en binario: (5).toBinary(8) es \"00000101\".", num_toBinary);
    vm.defineMethod(vm.numberClass, "toBase", "(base: number, digitos?: number = 0): text", "Entero en cualquier base de 2 a 36.", num_toBase);
    vm.defineGlobalFn("parseInt", "(texto: text, base?: number = 10): number?",
                      "Número entero escrito en una base: parseInt(\"ff\", 16) es 255; admite 0x, 0b y 0o. null si no es válido.", n_parseInt);

    ObjModule* file = existingModule(vm, "File");
    vm.defineFn(file, "readBytes", "(ruta: text): Bytes", "Lee un archivo binario completo.", file_readBytes);
    vm.defineFn(file, "writeBytes", "(ruta: text, datos: Bytes|text): void", "Escribe (o reemplaza) un archivo binario.", file_writeBytes);
}

} // namespace hz
