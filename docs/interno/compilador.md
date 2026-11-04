# El compilador

## Lexer

- Lee UTF-8 (salta el BOM y una primera línea `#!`).
- Los textos se dividen en partes literales y partes `${expresión}`; cada expresión se vuelve a analizar como código. Los textos `r"..."` no procesan escapes ni interpolación.
- Los saltos de línea se recuerdan en cada token (`newlineBefore`): el parser decide con ellos dónde termina una sentencia.

## Parser

Descenso recursivo con una función por nivel de precedencia (ver la [gramática](../lenguaje/gramatica.md)).

- **Saltos de línea**: una pila (`nlStack`) indica si dentro de la construcción actual los saltos de línea terminan sentencias (no lo hacen dentro de paréntesis, corchetes ni argumentos).
- **Recuperación de errores**: ante un error se descarta hasta el siguiente punto seguro (fin de línea, `}`, siguiente caso de un `match`) y se sigue, para mostrar todos los errores de una vez sin cascadas falsas.
- **Azúcar sintáctico**: `var [a, b] = lista` se traduce a una variable oculta y una declaración por nombre; así el resto del compilador no necesita saber nada de la desestructuración.

## Analizador

Hace dos pasadas por cada módulo:

1. **Declaraciones globales**: importaciones, clases, enums, eventos, funciones y variables, para que se puedan usar antes de su definición (las funciones y clases se "elevan").
2. **Cuerpo**: resuelve cada identificador a un `Symbol` (local con su *slot*, *upvalue* capturado por una clausura, global del módulo, integrado), calcula el tipo de cada expresión y comprueba llamadas, asignaciones, miembros, `return`, `match`, etc.

Los tipos son graduales: `any` (o un tipo desconocido) desactiva las comprobaciones, así que un programa sin anotaciones compila igual. Los tipos deducidos (`var x = 5` es `number`) dan errores útiles sin escribir nada.

El analizador también es la fuente del editor: guarda ámbitos (para el autocompletado), referencias (para ir a la definición, buscar referencias y renombrar) y la documentación `///` de cada símbolo.

### Errores en español

Cada diagnóstico tiene un código, un mensaje y, a menudo, una sugerencia: los nombres mal escritos proponen el más parecido (distancia de Levenshtein sobre los nombres visibles). El [catálogo de errores](../errores.md) se genera a partir del código del compilador.

## Generador de código

Produce, por cada función, un `ObjFunction` con su bytecode, constantes y tabla de líneas (para los errores en ejecución).

- **Variables locales** en *slots* de la pila; las capturadas por clausuras se cierran al salir de su ámbito (*upvalues* al estilo de Lua).
- **Llamadas a métodos** con `OP_INVOKE` y una **caché en línea** por sitio de llamada: si la clase del objeto es la misma que la vez anterior, se salta la búsqueda del método.
- **Acceso a campos** con una caché de un byte por instrucción con la posición del campo.
- **Optimizaciones de mirilla**:
  - comparación + salto condicional → una sola instrucción (`OP_JUMP_IF_NOT_LT`...), salvo si algún salto llega entre las dos;
  - bucles `for` numéricos con una instrucción por vuelta (`OP_FOR_NEXT`);
  - si ninguna función captura la variable del `for` (lo normal), esa variable tiene un hueco fijo y `OP_FOR_CHECK_V`/`OP_FOR_NEXT_V` le copian el contador: sin `GET_LOCAL` al empezar cada vuelta ni `POP` al terminarla. Si una función la captura, cada vuelta sigue teniendo su propia variable;
  - una asignación usada como sentencia (`x = ...`, `x += ...`, `x++`) guarda y descarta en una instrucción: `SET_GLOBAL + POP` pasa a ser `DEFINE_GLOBAL` y `SET_LOCAL + POP`, `SET_LOCAL_POP`;
  - parámetros `: number` comprobados con `OP_CHECK_NUMBER` (un byte en lugar de comparar nombres de tipos).

Todas estas optimizaciones solo se aplican si ningún salto llega entre las instrucciones que se juntan. Para ver el resultado: `hyper bytecode archivo.hz` (ver [la orden hyper](../herramientas/cli.md#el-bytecode-hyper-bytecode)). La lista completa de instrucciones está en [bytecode.md](bytecode.md).

## Formateador

`hyper fmt` (y *Formatear* en los editores) reutiliza el lexer: calcula la sangría de cada línea según los paréntesis, corchetes y llaves que siguen abiertos (los que se abren en la misma línea cuentan como un nivel), añade un nivel a las líneas que continúan una expresión (las que empiezan por `.`, `and`, `??`... o siguen a una que acaba en un operador) y respeta la alineación hecha a mano. Al final vuelve a pasar el lexer sobre el resultado y comprueba que los tokens son idénticos: si no lo fueran, no cambia nada (`src/lang/syntax/format.cpp`).

## La consola interactiva

Cada entrada se analiza junto con las anteriores (así conoce todas sus declaraciones y tipos), pero solo se genera código para la nueva. Los globales viven en un único módulo compartido y conservan su índice entre entradas, de modo que las funciones definidas antes ven los cambios posteriores. Al redefinir algo, la entrada antigua se sustituye por líneas vacías para no alterar los números de línea de los mensajes.
