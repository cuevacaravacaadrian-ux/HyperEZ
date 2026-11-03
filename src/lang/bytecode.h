// Conjunto de instrucciones de la máquina virtual de HyperEZ (máquina de pila).
#pragma once

#include <cstdint>

namespace hz {

enum Op : uint8_t {
    OP_CONST,          // u16 const
    OP_NULL, OP_TRUE, OP_FALSE, OP_UNDEF,
    OP_POP, OP_DUP, OP_DUP2, OP_SWAP,
    OP_GET_LOCAL,      // u16 slot
    OP_SET_LOCAL,      // u16 slot   (deja el valor)
    OP_GET_UPVAL,      // u16
    OP_SET_UPVAL,      // u16
    OP_GET_GLOBAL,     // u16
    OP_SET_GLOBAL,     // u16        (deja el valor)
    OP_DEFINE_GLOBAL,  // u16        (consume el valor)
    OP_GET_BUILTIN,    // u16
    OP_GET_PROP,       // u16 nombre, u8 caché del índice de campo
    OP_SET_PROP,       // u16 nombre, u8 caché   obj valor -> valor
    OP_GET_INDEX,      // obj idx -> v
    OP_SET_INDEX,      // obj idx v -> v
    OP_GET_SUPER,      // u16 nombre  this -> metodo enlazado
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_NEG, OP_NOT, OP_BNOT, OP_BAND, OP_BOR, OP_BXOR, OP_SHL, OP_SHR,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_IN,             // a b -> a in b
    OP_IS,             // u16 const (texto con el nombre del tipo)  v -> bool
    OP_JUMP,           // u16 adelante
    OP_JUMP_IF_FALSE,  // u16 (consume)
    OP_JUMP_IF_TRUE,   // u16 (consume)
    OP_JUMP_IF_FALSE_KEEP, OP_JUMP_IF_TRUE_KEEP, OP_JUMP_IF_NOT_NULL_KEEP, OP_JUMP_IF_NULL_KEEP, // u16
    OP_LOOP,           // u16 atrás
    OP_CALL,           // u8 argc
    OP_CALL_NAMED,     // u8 argc, u16 const (lista de nombres: null = posicional)
    OP_CALL_SPREAD,    // callee lista -> resultado
    OP_INVOKE,         // u16 nombre, u8 argc, u16 entrada de la caché de métodos
    OP_SUPER_INVOKE,   // u16 nombre, u8 argc   (this args...)
    OP_SUPER_CALL,     // u8 argc   (this args...) -> llama al constructor de la clase base
    OP_CLOSURE,        // u16 const función, luego por upvalue: u8 isLocal, u16 index
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,          // u16 nombre, u8 flags(1=struct)
    OP_INHERIT,        // clase super -> clase
    OP_METHOD,         // u16 nombre, u8 tipo   clase closure -> clase
    OP_FIELD,          // u16 nombre, u8 flags  clase -> clase
    OP_IMPLEMENTS,     // u16 nombre            clase iface -> clase
    OP_LIST,           // u16 n
    OP_LIST_APPEND,    // lista v -> lista
    OP_LIST_EXTEND,    // lista iterable -> lista
    OP_MAP,            // u16 n (pares)
    OP_RANGE,          // u8 inclusive  a b -> rango
    OP_RANGE_STEP,     // rango paso -> rango
    OP_ITER_PREP,      // coleccion -> coleccion 0
    OP_ITER_NEXT,      // u16 slot de la colección, u16 salto  -> clave valor  (o salta)
    OP_FOR_CHECK,      // u16 slot contador, u8 inclusive, u16 salto
    OP_FOR_INCR,       // u16 slot contador
    OP_REPEAT_PREP,    // n -> n (valida)
    OP_REPEAT_NEXT,    // u16 slot, u16 salto
    OP_TRY,            // u16 desplazamiento al catch
    OP_END_TRY,
    OP_THROW,
    OP_AWAIT,
    OP_EMIT,           // u8 argc   evento args... -> null
    OP_ON,             // u8 flags(1=once, 2=prioridad)  evento handler [prio] -> suscripción
    OP_ENUM,           // u16 nombre, u16 n  (pares nombre valor) -> enum
    OP_EVENT,          // u16 nombre, u16 const lista de parámetros -> evento
    OP_IMPORT,         // u16 const ruta -> módulo
    OP_IMPORT_NAME,    // u16 nombre   módulo -> valor
    OP_TO_STRING,
    OP_CONCAT,         // u16 n
    OP_CHECK_TYPE,     // u16 slot, u16 const tipo, u16 const nombre del parámetro
    OP_DEFAULT_ARG,    // u16 slot, u16 salto (salta si el argumento NO falta)
    OP_MATCH_TEST,     // sujeto patrón -> bool
    OP_SUPER_INIT_AUTO,// this -> (llama al constructor base sin argumentos si existe)
    OP_CHECK_RETURN,   // u16 const tipo  v -> v
    OP_SLIDE,          // u8 n   l1..ln v -> v   (retira n locales bajo la cima cerrando sus upvalues)
    OP_COUNT
};

// Tipos para OP_METHOD
enum MethodKind : uint8_t { MK_METHOD, MK_STATIC, MK_GETTER, MK_SETTER, MK_CTOR, MK_FIELDINIT, MK_STATIC_GETTER, MK_STATIC_SETTER };

const char* opName(Op op);

} // namespace hz
