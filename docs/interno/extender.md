# Cómo ampliar HyperEZ

## Una biblioteca escrita en HyperEZ

Es la forma más sencilla de añadir funcionalidad a todos los programas:

1. Crea `src/lib/nombre.hz` (por ejemplo `src/lib/geometria.hz`). Las subcarpetas se importan con punto: `src/lib/web/html.hz` → `import web.html`.
2. Documenta cada función o clase pública con `///`: aparece en el editor al pasar el ratón.
3. Añade pruebas en `tests/lib/` y, si hace falta, una sección en la documentación.
4. Compila: `cmake/embed_hz.cmake` la incrusta en `hyper` y ya se puede usar con `import nombre`.

`ctest` comprueba con `hyper check` que no tiene errores.

## Una función nativa (C++)

Para lo que necesita velocidad o acceso al sistema. Las funciones de cada área están en `src/lang/stdlib/` (un archivo por área) y usan las utilidades de `native.h`:

```text
// src/lang/stdlib/system.cpp
NATIVE(sys_cpus) {
    UNUSED;
    return Value((double)std::thread::hardware_concurrency());
}

void registerSystemLibs(VM& vm) {
    ObjModule* sys = existingModule(vm, "System");
    vm.defineProp(sys, "cpuCount", "number", "Número de núcleos del procesador.", sys_cpus);
}
```

- `vm.nativeModule(nombre, doc)` crea un módulo (`File`, `Http`...); `defineFn` añade funciones, `defineProp` propiedades y `defineConst` constantes.
- `vm.defineGlobalFn` añade una función global; `vm.defineMethod(vm.textClass, ...)` un método a los textos (o a `listClass`, `mapClass`, `numberClass`).
- `vm.nativeClass(nombre, doc, constructor, firma)` crea una clase nativa. Los objetos con datos propios son `ObjForeign` con un puntero y una función que lo libera (mira `Bytes` en `binary.cpp` o `Matrix` en `matrix.cpp`).
- La **firma** (`"(ruta: text, veces?: number = 1): list<text>"`) es importante: con ella el analizador comprueba los argumentos y el editor autocompleta. Un `?` marca los opcionales.
- Los argumentos se leen con `argNum`, `argStr`, `argBool`, `argList`, `argMap` y `optArg`; los errores se lanzan con `vm.raise("mensaje en español")`.
- Mientras construyes listas o mapas que pueden disparar el recolector, protégelos con `TempRoot keep(vm, objeto)`.

Si añades un archivo nuevo, regístralo en `native.h` y llama a su función desde `registerStdlibExtra` (`extra.cpp`). Regenera la documentación con `python tools/gen_docs.py build/hyper.exe`.

## Una instrucción nueva de la máquina virtual

1. Añádela al final de `src/lang/compiler/bytecode.h` con un comentario de sus operandos.
2. Emítela en `codegen.cpp`.
3. Impleméntala en el `switch` de `interp.cpp`.
4. Regenera `docs/interno/bytecode.md` con `tools/gen_docs.py` y mide con los benchmarks.

## Sintaxis nueva

Requiere tocar el lexer (si hay símbolos nuevos), el parser, el árbol (`ast.h`), el analizador, el generador de código y la gramática de resaltado de VS Code (`editors/vscode/tools/gen_grammar.py`). Si se puede expresar con construcciones existentes, tradúcela en el parser (como la desestructuración): es mucho menos trabajo y el editor la entiende sola.
