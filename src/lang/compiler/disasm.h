// Desensamblador del bytecode: muestra las instrucciones que genera el compilador (hyper bytecode).
#pragma once

#include <cstdint>
#include <string>

namespace hz {

class VM;
struct ObjFunction;

// Texto con las instrucciones de la función y, si recursive, de todas las funciones que contiene.
// Si el bytecode no se puede decodificar (no debería pasar) la salida lo indica con "??".
std::string disassemble(VM& vm, ObjFunction* fn, bool recursive = true);

// Número de bytes de operandos de una instrucción (sin contar el código de operación), o -1 si es
// de longitud variable (OP_CLOSURE) o desconocida.
int operandBytes(uint8_t op);

} // namespace hz
