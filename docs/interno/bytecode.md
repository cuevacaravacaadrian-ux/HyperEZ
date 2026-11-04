# Instrucciones de la máquina virtual

El compilador traduce cada función a una lista de instrucciones (bytecode) para una máquina de pila.
Cada instrucción ocupa un byte y puede llevar operandos de 8 bits (`u8`) o 16 bits (`u16`).
La columna «Operandos y pila» indica qué lee de la pila y qué deja (`a b -> r`).

> Este archivo se genera con `python tools/gen_docs.py` a partir de `src/lang/compiler/bytecode.h`.

| # | Instrucción | Operandos y pila |
|---|---|---|
| 0 | `OP_CONST` | u16 const |
| 1 | `OP_GET_LOCAL` | u16 slot |
| 2 | `OP_SET_LOCAL` | u16 slot   (deja el valor) |
| 3 | `OP_GET_UPVAL` | u16 |
| 4 | `OP_SET_UPVAL` | u16 |
| 5 | `OP_GET_GLOBAL` | u16 |
| 6 | `OP_SET_GLOBAL` | u16        (deja el valor) |
| 7 | `OP_DEFINE_GLOBAL` | u16        (consume el valor) |
| 8 | `OP_GET_BUILTIN` | u16 |
| 9 | `OP_GET_PROP` | u16 nombre, u8 caché del índice de campo |
| 10 | `OP_SET_PROP` | u16 nombre, u8 caché   obj valor -> valor |
| 11 | `OP_GET_INDEX` | obj idx -> v |
| 12 | `OP_SET_INDEX` | obj idx v -> v |
| 13 | `OP_GET_SUPER` | u16 nombre  this -> metodo enlazado |
| 14 | `OP_IN` | a b -> a in b |
| 15 | `OP_IS` | u16 const (texto con el nombre del tipo)  v -> bool |
| 16 | `OP_JUMP` | u16 adelante |
| 17 | `OP_JUMP_IF_FALSE` | u16 (consume) |
| 18 | `OP_JUMP_IF_TRUE` | u16 (consume) |
| 19 | `OP_LOOP` | u16 atrás |
| 20 | `OP_CALL` | u8 argc |
| 21 | `OP_CALL_NAMED` | u8 argc, u16 const (lista de nombres: null = posicional) |
| 22 | `OP_CALL_SPREAD` | callee lista -> resultado |
| 23 | `OP_INVOKE` | u16 nombre, u8 argc, u16 entrada de la caché de métodos |
| 24 | `OP_SUPER_INVOKE` | u16 nombre, u8 argc   (this args...) |
| 25 | `OP_SUPER_CALL` | u8 argc   (this args...) -> llama al constructor de la clase base |
| 26 | `OP_CLOSURE` | u16 const función, luego por upvalue: u8 isLocal, u16 index |
| 27 | `OP_CLOSE_UPVALUE` |  |
| 28 | `OP_RETURN` |  |
| 29 | `OP_CLASS` | u16 nombre, u8 flags(1=struct) |
| 30 | `OP_INHERIT` | clase super -> clase |
| 31 | `OP_METHOD` | u16 nombre, u8 tipo   clase closure -> clase |
| 32 | `OP_FIELD` | u16 nombre, u8 flags  clase -> clase |
| 33 | `OP_IMPLEMENTS` | u16 nombre            clase iface -> clase |
| 34 | `OP_LIST` | u16 n |
| 35 | `OP_LIST_APPEND` | lista v -> lista |
| 36 | `OP_LIST_EXTEND` | lista iterable -> lista |
| 37 | `OP_MAP` | u16 n (pares) |
| 38 | `OP_RANGE` | u8 inclusive  a b -> rango |
| 39 | `OP_RANGE_STEP` | rango paso -> rango |
| 40 | `OP_ITER_PREP` | coleccion -> coleccion 0 |
| 41 | `OP_ITER_NEXT` | u16 slot de la colección, u16 salto  -> clave valor  (o salta) |
| 42 | `OP_FOR_CHECK` | u16 slot contador, u8 inclusive, u16 salto |
| 43 | `OP_FOR_INCR` | u16 slot contador |
| 44 | `OP_REPEAT_PREP` | n -> n (valida) |
| 45 | `OP_REPEAT_NEXT` | u16 slot, u16 salto |
| 46 | `OP_TRY` | u16 desplazamiento al catch |
| 47 | `OP_END_TRY` |  |
| 48 | `OP_THROW` |  |
| 49 | `OP_AWAIT` |  |
| 50 | `OP_EMIT` | u8 argc   evento args... -> null |
| 51 | `OP_ON` | u8 flags(1=once, 2=prioridad)  evento handler [prio] -> suscripción |
| 52 | `OP_ENUM` | u16 nombre, u16 n  (pares nombre valor) -> enum |
| 53 | `OP_EVENT` | u16 nombre, u16 const lista de parámetros -> evento |
| 54 | `OP_IMPORT` | u16 const ruta -> módulo |
| 55 | `OP_IMPORT_NAME` | u16 nombre   módulo -> valor |
| 56 | `OP_TO_STRING` |  |
| 57 | `OP_CONCAT` | u16 n |
| 58 | `OP_CHECK_TYPE` | u16 slot, u16 const tipo, u16 const nombre del parámetro |
| 59 | `OP_DEFAULT_ARG` | u16 slot, u16 salto (salta si el argumento NO falta) |
| 60 | `OP_MATCH_TEST` | sujeto patrón -> bool |
| 61 | `OP_SUPER_INIT_AUTO` | this -> (llama al constructor base sin argumentos si existe) |
| 62 | `OP_CHECK_RETURN` | u16 const tipo  v -> v |
| 63 | `OP_SLIDE` | u8 n   l1..ln v -> v   (retira n locales bajo la cima cerrando sus upvalues) |
| 64 | `OP_JUMP_IF_NOT_LT` | u16 |
| 65 | `OP_JUMP_IF_NOT_LE` | u16 |
| 66 | `OP_JUMP_IF_NOT_GT` | u16 |
| 67 | `OP_JUMP_IF_NOT_GE` | u16 |
| 68 | `OP_JUMP_IF_NOT_EQ` | u16 |
| 69 | `OP_JUMP_IF_NOT_NE` | u16 |
| 70 | `OP_FOR_NEXT` | u16 slot, u8 inclusivo, u16 distancia hacia atrás |
| 71 | `OP_CHECK_NUMBER` | u16 slot, u16 const nombre del parámetro (parámetro ": number") |

Hay 72 instrucciones.
