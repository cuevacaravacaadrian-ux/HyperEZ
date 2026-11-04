#include "lang/stdlib/mathtypes.h"

#include <algorithm>
#include <cmath>

namespace hz {

static ObjVec* V(Value* args) { return static_cast<ObjVec*>(args[-1].o); }
#define NATIVE(name) static Value name(VM& vm, int argc, Value* args)

// ---- Colores ---------------------------------------------------------------

static const struct { const char* name; uint8_t r, g, b, a; } kColors[] = {
    {"white", 255, 255, 255, 255}, {"black", 0, 0, 0, 255}, {"red", 230, 41, 55, 255}, {"green", 0, 228, 48, 255},
    {"blue", 0, 121, 241, 255}, {"yellow", 253, 249, 0, 255}, {"orange", 255, 161, 0, 255}, {"purple", 200, 122, 255, 255},
    {"violet", 135, 60, 190, 255}, {"pink", 255, 109, 194, 255}, {"cyan", 0, 220, 230, 255}, {"magenta", 255, 0, 255, 255},
    {"gray", 130, 130, 130, 255}, {"grey", 130, 130, 130, 255}, {"darkgray", 80, 80, 80, 255}, {"lightgray", 200, 200, 200, 255},
    {"brown", 127, 106, 79, 255}, {"skyblue", 102, 191, 255, 255}, {"lime", 50, 205, 50, 255}, {"gold", 255, 203, 0, 255},
    {"maroon", 190, 33, 55, 255}, {"darkblue", 0, 82, 172, 255}, {"darkgreen", 0, 117, 44, 255}, {"beige", 211, 176, 131, 255},
    {"transparent", 0, 0, 0, 0}, {"blanco", 255, 255, 255, 255}, {"negro", 0, 0, 0, 255}, {"rojo", 230, 41, 55, 255},
    {"verde", 0, 228, 48, 255}, {"azul", 0, 121, 241, 255}, {"amarillo", 253, 249, 0, 255}, {"naranja", 255, 161, 0, 255},
    {"morado", 200, 122, 255, 255}, {"rosa", 255, 109, 194, 255}, {"gris", 130, 130, 130, 255}, {"marron", 127, 106, 79, 255},
};

bool parseColorName(const std::string& s0, RGBA& out) {
    std::string s = toLower(trim(s0));
    if (!s.empty() && s[0] == '#') {
        std::string h = s.substr(1);
        if (h.size() == 3 || h.size() == 4) { std::string e; for (char c : h) { e += c; e += c; } h = e; }
        if (h.size() != 6 && h.size() != 8) return false;
        for (char c : h) if (!std::isxdigit((unsigned char)c)) return false;
        auto byte = [&](int i) { return (float)std::strtoul(h.substr(i, 2).c_str(), nullptr, 16) / 255.0f; };
        out = {byte(0), byte(2), byte(4), h.size() == 8 ? byte(6) : 1.0f};
        return true;
    }
    for (auto& c : kColors)
        if (s == c.name) { out = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}; return true; }
    return false;
}

RGBA toColor(VM& vm, Value v, const char* fname, RGBA def) {
    if (v.isNull() || v.isUndef()) return def;
    if (isObjType(v, OT::Vec)) {
        auto* c = static_cast<ObjVec*>(v.o);
        if (c->kind == VK_COLOR) return {(float)(c->v[0] / 255), (float)(c->v[1] / 255), (float)(c->v[2] / 255), (float)(c->v[3] / 255)};
        return {(float)c->v[0], (float)c->v[1], (float)c->v[2], c->kind == VK_VEC4 ? (float)c->v[3] : 1.0f};
    }
    if (isString(v)) {
        RGBA r;
        if (parseColorName(asString(v)->str, r)) return r;
        vm.raise(std::string(fname) + ": color desconocido \"" + asString(v)->str + "\" (usa un nombre como \"red\" o \"#ff8800\")");
    }
    if (isObjType(v, OT::List)) {
        auto& it = static_cast<ObjList*>(v.o)->items;
        if (it.size() >= 3) {
            auto n = [&](size_t i) { return (i < it.size() && it[i].isNum()) ? (float)(it[i].n / 255.0) : 1.0f; };
            return {n(0), n(1), n(2), it.size() > 3 ? n(3) : 1.0f};
        }
    }
    vm.raise(std::string(fname) + ": se esperaba un color (Color, \"red\", \"#ff8800\" o [r, g, b]) y se recibió " + vm.typeName(v));
}

Value makeColor(VM& vm, RGBA c) {
    return Value::object(vm.newVec(VK_COLOR, std::round(c.r * 255), std::round(c.g * 255), std::round(c.b * 255), std::round(c.a * 255)));
}

// ---- Constructores -----------------------------------------------------------

NATIVE(c_vec2) { return Value::object(vm.newVec(VK_VEC2, argNum(vm, args, argc, 0, "Vec2", 0), argNum(vm, args, argc, 1, "Vec2", 0))); }
NATIVE(c_vec3) { return Value::object(vm.newVec(VK_VEC3, argNum(vm, args, argc, 0, "Vec3", 0), argNum(vm, args, argc, 1, "Vec3", 0), argNum(vm, args, argc, 2, "Vec3", 0))); }
NATIVE(c_color) {
    if (argc == 1 && (isString(args[0]) || isObjType(args[0], OT::List))) return makeColor(vm, toColor(vm, args[0], "Color"));
    return Value::object(vm.newVec(VK_COLOR, argNum(vm, args, argc, 0, "Color", 255), argNum(vm, args, argc, 1, "Color", 255),
                                   argNum(vm, args, argc, 2, "Color", 255), argNum(vm, args, argc, 3, "Color", 255)));
}

// ---- Métodos de vectores ------------------------------------------------------------

static int dims(ObjVec* v) { return v->kind == VK_COLOR ? 4 : v->dims(); }
static double len(ObjVec* v) { double s = 0; for (int i = 0; i < dims(v); i++) s += v->v[i] * v->v[i]; return std::sqrt(s); }
static ObjVec* same(VM& vm, Value* args, int argc, int i, const char* f) { return argVec(vm, args, argc, i, f, V(args)->kind); }

NATIVE(v_length) { (void)vm; (void)argc; return Value(len(V(args))); }
NATIVE(v_lengthSq) { (void)vm; (void)argc; double l = len(V(args)); return Value(l * l); }
NATIVE(v_normalized) {
    (void)argc;
    ObjVec* a = V(args);
    double l = len(a);
    ObjVec* r = vm.newVec(a->kind);
    for (int i = 0; i < dims(a); i++) r->v[i] = l > 0 ? a->v[i] / l : 0;
    return Value::object(r);
}
NATIVE(v_dot) {
    ObjVec* a = V(args);
    ObjVec* b = same(vm, args, argc, 0, "dot");
    double s = 0;
    for (int i = 0; i < dims(a); i++) s += a->v[i] * b->v[i];
    return Value(s);
}
NATIVE(v_cross) {
    ObjVec* a = V(args);
    ObjVec* b = same(vm, args, argc, 0, "cross");
    if (a->kind == VK_VEC2) return Value(a->v[0] * b->v[1] - a->v[1] * b->v[0]);
    return Value::object(vm.newVec(VK_VEC3, a->v[1] * b->v[2] - a->v[2] * b->v[1], a->v[2] * b->v[0] - a->v[0] * b->v[2], a->v[0] * b->v[1] - a->v[1] * b->v[0]));
}
NATIVE(v_distance) {
    ObjVec* a = V(args);
    ObjVec* b = same(vm, args, argc, 0, "distance");
    double s = 0;
    for (int i = 0; i < dims(a); i++) s += (a->v[i] - b->v[i]) * (a->v[i] - b->v[i]);
    return Value(std::sqrt(s));
}
NATIVE(v_lerp) {
    ObjVec* a = V(args);
    ObjVec* b = same(vm, args, argc, 0, "lerp");
    double t = argNum(vm, args, argc, 1, "lerp");
    ObjVec* r = vm.newVec(a->kind);
    for (int i = 0; i < 4; i++) r->v[i] = a->v[i] + (b->v[i] - a->v[i]) * t;
    return Value::object(r);
}
NATIVE(v_copy) { (void)argc; ObjVec* a = V(args); return Value::object(vm.newVec(a->kind, a->v[0], a->v[1], a->v[2], a->v[3])); }
NATIVE(v_set) {
    ObjVec* a = V(args);
    for (int i = 0; i < dims(a) && i < argc; i++) a->v[i] = argNum(vm, args, argc, i, "set");
    return args[-1];
}
NATIVE(v_angle) { (void)vm; (void)argc; ObjVec* a = V(args); return Value(std::atan2(a->v[1], a->v[0])); }
NATIVE(v_rotate) {
    ObjVec* a = V(args);
    double ang = argNum(vm, args, argc, 0, "rotate");
    double c = std::cos(ang), s = std::sin(ang);
    return Value::object(vm.newVec(VK_VEC2, a->v[0] * c - a->v[1] * s, a->v[0] * s + a->v[1] * c));
}
NATIVE(v_toList) { (void)argc; ObjVec* a = V(args); ObjList* l = vm.newList(); for (int i = 0; i < dims(a); i++) l->items.push_back(Value(a->v[i])); return Value::object(l); }
NATIVE(v_limit) {
    ObjVec* a = V(args);
    double m = argNum(vm, args, argc, 0, "limit");
    double l = len(a);
    ObjVec* r = vm.newVec(a->kind, a->v[0], a->v[1], a->v[2], a->v[3]);
    if (l > m && l > 0) for (int i = 0; i < dims(a); i++) r->v[i] = a->v[i] / l * m;
    return Value::object(r);
}
NATIVE(v_moveTowards) {
    ObjVec* a = V(args);
    ObjVec* b = same(vm, args, argc, 0, "moveTowards");
    double maxD = argNum(vm, args, argc, 1, "moveTowards");
    double d2 = 0;
    for (int i = 0; i < dims(a); i++) d2 += (b->v[i] - a->v[i]) * (b->v[i] - a->v[i]);
    double d = std::sqrt(d2);
    ObjVec* r = vm.newVec(a->kind);
    for (int i = 0; i < 4; i++) r->v[i] = (d <= maxD || d == 0) ? b->v[i] : a->v[i] + (b->v[i] - a->v[i]) / d * maxD;
    return Value::object(r);
}

// Constantes estáticas (objetos nuevos en cada acceso: son mutables)
#define VSTATIC(name, kind, x, y, z) NATIVE(name) { (void)argc; (void)args; return Value::object(vm.newVec(kind, x, y, z)); }
VSTATIC(v2_zero, VK_VEC2, 0, 0, 0)
VSTATIC(v2_one, VK_VEC2, 1, 1, 0)
VSTATIC(v2_up, VK_VEC2, 0, -1, 0)
VSTATIC(v2_down, VK_VEC2, 0, 1, 0)
VSTATIC(v2_left, VK_VEC2, -1, 0, 0)
VSTATIC(v2_right, VK_VEC2, 1, 0, 0)
VSTATIC(v3_zero, VK_VEC3, 0, 0, 0)
VSTATIC(v3_one, VK_VEC3, 1, 1, 1)
VSTATIC(v3_up, VK_VEC3, 0, 1, 0)
VSTATIC(v3_down, VK_VEC3, 0, -1, 0)
VSTATIC(v3_left, VK_VEC3, -1, 0, 0)
VSTATIC(v3_right, VK_VEC3, 1, 0, 0)
VSTATIC(v3_forward, VK_VEC3, 0, 0, -1)
VSTATIC(v3_back, VK_VEC3, 0, 0, 1)
NATIVE(v2_fromAngle) {
    double a = argNum(vm, args, argc, 0, "fromAngle"), l = argNum(vm, args, argc, 1, "fromAngle", 1);
    return Value::object(vm.newVec(VK_VEC2, std::cos(a) * l, std::sin(a) * l));
}

// Colores
NATIVE(col_hex) { return makeColor(vm, toColor(vm, vm.str(argStr(vm, args, argc, 0, "Color.hex")), "Color.hex")); }
NATIVE(col_rgb) {
    return Value::object(vm.newVec(VK_COLOR, argNum(vm, args, argc, 0, "Color.rgb"), argNum(vm, args, argc, 1, "Color.rgb"),
                                   argNum(vm, args, argc, 2, "Color.rgb"), argNum(vm, args, argc, 3, "Color.rgb", 255)));
}
NATIVE(col_hsv) {
    double h = std::fmod(argNum(vm, args, argc, 0, "Color.hsv"), 360.0);
    if (h < 0) h += 360;
    double s = argNum(vm, args, argc, 1, "Color.hsv", 1), v = argNum(vm, args, argc, 2, "Color.hsv", 1), a = argNum(vm, args, argc, 3, "Color.hsv", 1);
    double c = v * s, x = c * (1 - std::fabs(std::fmod(h / 60.0, 2) - 1)), m = v - c;
    double r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; } else if (h < 120) { r = x; g = c; } else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; } else if (h < 300) { r = x; b = c; } else { r = c; b = x; }
    return makeColor(vm, {(float)(r + m), (float)(g + m), (float)(b + m), (float)a});
}
NATIVE(col_withAlpha) {
    ObjVec* a = V(args);
    return Value::object(vm.newVec(VK_COLOR, a->v[0], a->v[1], a->v[2], argNum(vm, args, argc, 0, "withAlpha")));
}
NATIVE(col_toHex) {
    (void)argc;
    ObjVec* a = V(args);
    char buf[16];
    auto b = [](double x) { return (int)std::clamp(std::round(x), 0.0, 255.0); };
    if (a->v[3] >= 255) std::snprintf(buf, sizeof buf, "#%02x%02x%02x", b(a->v[0]), b(a->v[1]), b(a->v[2]));
    else std::snprintf(buf, sizeof buf, "#%02x%02x%02x%02x", b(a->v[0]), b(a->v[1]), b(a->v[2]), b(a->v[3]));
    return vm.str(buf);
}
NATIVE(col_brighten) {
    ObjVec* a = V(args);
    double f = argNum(vm, args, argc, 0, "brighten");
    return Value::object(vm.newVec(VK_COLOR, std::clamp(a->v[0] * f, 0.0, 255.0), std::clamp(a->v[1] * f, 0.0, 255.0), std::clamp(a->v[2] * f, 0.0, 255.0), a->v[3]));
}

static NativeFn colorGetter(int i);
template <int I> static Value colorConst(VM& vm, int, Value*) {
    auto& c = kColors[I];
    return Value::object(vm.newVec(VK_COLOR, c.r, c.g, c.b, c.a));
}
template <int... Is> static NativeFn pickColor(int i, std::integer_sequence<int, Is...>) {
    NativeFn fns[] = {&colorConst<Is>...};
    return fns[i];
}
static NativeFn colorGetter(int i) { return pickColor(i, std::make_integer_sequence<int, 25>{}); }

void registerMathTypes(VM& vm) {
    ObjClass* v2 = vm.nativeClass("Vec2", "Vector 2D (x, y). Se suma, resta y multiplica con +, -, *, /.", c_vec2, "(x?: number = 0, y?: number = 0)");
    v2->vecKind = VK_VEC2;
    vm.vecClass[VK_VEC2] = v2;
    ObjClass* v3 = vm.nativeClass("Vec3", "Vector 3D (x, y, z) para posiciones, direcciones y escalas en 3D.", c_vec3, "(x?: number = 0, y?: number = 0, z?: number = 0)");
    v3->vecKind = VK_VEC3;
    vm.vecClass[VK_VEC3] = v3;
    ObjClass* col = vm.nativeClass("Color", "Color RGBA con componentes de 0 a 255. También acepta Color(\"red\") o Color(\"#ff8800\").", c_color,
                                   "(r?: number|text|list = 255, g?: number = 255, b?: number = 255, a?: number = 255)");
    col->vecKind = VK_COLOR;
    vm.vecClass[VK_COLOR] = col;
    // Vec4 y Quat se crean internamente (sin constructor público todavía)
    vm.vecClass[VK_VEC4] = vm.newClass("Vec4");
    vm.vecClass[VK_VEC4]->isNative = true;
    vm.vecClass[VK_QUAT] = vm.newClass("Quat");
    vm.vecClass[VK_QUAT]->isNative = true;

    for (const char* n : {"x", "y"}) vm.addCatalogField(v2, n, "number", std::string("Componente ") + n + ".");
    for (const char* n : {"x", "y", "z"}) vm.addCatalogField(v3, n, "number", std::string("Componente ") + n + ".");
    for (const char* n : {"r", "g", "b", "a"}) vm.addCatalogField(col, n, "number", std::string("Componente ") + n + " (0-255).");

    for (ObjClass* c : {v2, v3}) {
        std::string T = c->name->str;
        vm.defineMethod(c, "length", "(): number", "Longitud (módulo) del vector.", v_length);
        vm.defineMethod(c, "lengthSquared", "(): number", "Longitud al cuadrado (más rápida para comparar).", v_lengthSq);
        vm.defineMethod(c, "normalized", "(): " + T, "Vector de longitud 1 con la misma dirección.", v_normalized);
        vm.defineMethod(c, "dot", "(otro: " + T + "): number", "Producto escalar.", v_dot);
        vm.defineMethod(c, "distance", "(otro: " + T + "): number", "Distancia hasta otro punto.", v_distance);
        vm.defineMethod(c, "lerp", "(otro: " + T + ", t: number): " + T, "Interpolación lineal hacia otro vector.", v_lerp);
        vm.defineMethod(c, "copy", "(): " + T, "Copia independiente.", v_copy);
        vm.defineMethod(c, "set", std::string("(x: number, y: number") + (c == v3 ? ", z: number" : "") + "): " + T, "Cambia los componentes (en el sitio).", v_set);
        vm.defineMethod(c, "limit", "(maximo: number): " + T, "Copia con la longitud limitada a 'maximo'.", v_limit);
        vm.defineMethod(c, "moveTowards", "(destino: " + T + ", maxDistancia: number): " + T, "Avanza hacia destino como mucho maxDistancia.", v_moveTowards);
        vm.defineMethod(c, "toList", "(): list<number>", "Componentes en una lista.", v_toList);
    }
    vm.defineMethod(v2, "cross", "(otro: Vec2): number", "Producto cruzado 2D (escalar).", v_cross);
    vm.defineMethod(v2, "angle", "(): number", "Ángulo en radianes respecto al eje X.", v_angle);
    vm.defineMethod(v2, "rotate", "(radianes: number): Vec2", "Copia rotada.", v_rotate);
    vm.defineMethod(v3, "cross", "(otro: Vec3): Vec3", "Producto vectorial.", v_cross);
    vm.defineStatic(v2, "fromAngle", "(radianes: number, longitud?: number = 1): Vec2", "Vector a partir de un ángulo.", v2_fromAngle);
    vm.defineClassProp(v2, "zero", "Vec2", "(0, 0)", v2_zero, nullptr, true);
    vm.defineClassProp(v2, "one", "Vec2", "(1, 1)", v2_one, nullptr, true);
    vm.defineClassProp(v2, "up", "Vec2", "(0, -1): arriba en pantalla.", v2_up, nullptr, true);
    vm.defineClassProp(v2, "down", "Vec2", "(0, 1): abajo en pantalla.", v2_down, nullptr, true);
    vm.defineClassProp(v2, "left", "Vec2", "(-1, 0)", v2_left, nullptr, true);
    vm.defineClassProp(v2, "right", "Vec2", "(1, 0)", v2_right, nullptr, true);
    vm.defineClassProp(v3, "zero", "Vec3", "(0, 0, 0)", v3_zero, nullptr, true);
    vm.defineClassProp(v3, "one", "Vec3", "(1, 1, 1)", v3_one, nullptr, true);
    vm.defineClassProp(v3, "up", "Vec3", "(0, 1, 0)", v3_up, nullptr, true);
    vm.defineClassProp(v3, "down", "Vec3", "(0, -1, 0)", v3_down, nullptr, true);
    vm.defineClassProp(v3, "left", "Vec3", "(-1, 0, 0)", v3_left, nullptr, true);
    vm.defineClassProp(v3, "right", "Vec3", "(1, 0, 0)", v3_right, nullptr, true);
    vm.defineClassProp(v3, "forward", "Vec3", "(0, 0, -1): hacia delante (OpenGL).", v3_forward, nullptr, true);
    vm.defineClassProp(v3, "back", "Vec3", "(0, 0, 1)", v3_back, nullptr, true);

    vm.defineStatic(col, "hex", "(codigo: text): Color", "Color desde \"#rrggbb\" o \"#rrggbbaa\".", col_hex);
    vm.defineStatic(col, "rgb", "(r: number, g: number, b: number, a?: number = 255): Color", "Color desde componentes 0-255.", col_rgb);
    vm.defineStatic(col, "hsv", "(tono: number, saturacion?: number = 1, brillo?: number = 1, alfa?: number = 1): Color",
                    "Color desde tono (0-360), saturación y brillo (0-1). Ideal para arcoíris.", col_hsv);
    vm.defineMethod(col, "withAlpha", "(a: number): Color", "Copia con otra transparencia (0-255).", col_withAlpha);
    vm.defineMethod(col, "lerp", "(otro: Color, t: number): Color", "Mezcla con otro color.", v_lerp);
    vm.defineMethod(col, "toHex", "(): text", "Texto \"#rrggbb\".", col_toHex);
    vm.defineMethod(col, "brighten", "(factor: number): Color", "Más claro (>1) o más oscuro (<1).", col_brighten);
    vm.defineMethod(col, "copy", "(): Color", "Copia independiente.", v_copy);
    for (int i = 0; i < 25; i++) {
        std::string n = toLower(kColors[i].name);
        for (auto& ch : n) ch = (char)std::toupper((unsigned char)ch);
        vm.defineClassProp(col, n, "Color", std::string("Color ") + kColors[i].name + ".", colorGetter(i), nullptr, true);
    }
    vm.defineLibrary("HyperMath", {"Vec2", "Vec3", "Color"}, "Vectores, colores y funciones matemáticas.");
}

} // namespace hz
