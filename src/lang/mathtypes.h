// Tipos matemáticos integrados (Vec2, Vec3, Color) y conversiones usadas por el motor.
#pragma once

#include "vm.h"

namespace hz {

struct RGBA { float r = 1, g = 1, b = 1, a = 1; };

// Acepta Color, "red", "#ff8800", "#ff880080" o [r, g, b(, a)] en 0..255.
RGBA toColor(VM& vm, Value v, const char* fname, RGBA def = {});
bool parseColorName(const std::string& s, RGBA& out);
Value makeColor(VM& vm, RGBA c);

} // namespace hz
