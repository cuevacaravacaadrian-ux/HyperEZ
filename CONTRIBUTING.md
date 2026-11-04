# Contribuir a HyperEZ

Gracias por ayudar. Esta guía explica cómo compilar HyperEZ, probar tus cambios y enviarlos.

## Avisar de un error o proponer una idea

Abre una [incidencia](https://github.com/cuevacaravacaadrian-ux/HyperEZ/issues/new/choose). Para un error, incluye el programa `.hz` más pequeño que lo reproduzca, lo que esperabas y lo que pasó (copia el mensaje completo), y la salida de `hyper version`.

## Preparar el entorno

### Windows

- [MSYS2](https://www.msys2.org/) y, en su terminal *UCRT64*:
  `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja`
- Python 3 (pruebas de la documentación y generador de la gramática).
- Node.js 20+ (solo para la extensión de VS Code).

`build.ps1` añade `C:\msys64\ucrt64\bin` al `PATH` por su cuenta. Si MSYS2 está en otra carpeta, indícala en la variable `HYPEREZ_MSYS`.

### Linux y macOS

Un compilador de C++20 (GCC 11+ o Clang 14+), CMake 3.20+, Ninja y Python 3. Para `Http`, la orden `curl`.

```text
sudo apt install g++ cmake ninja-build python3      # Debian, Ubuntu
brew install cmake ninja                            # macOS (con las herramientas de Xcode)
```

## Compilar y probar

```powershell
./build.ps1 -NoPGO -SkipVsix   # Windows, lo más rápido mientras desarrollas
./build.ps1                    # todo, como en una versión publicada (PGO, extensión, instalador)
```

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release     # cualquier sistema
cmake --build build
ctest --test-dir build --output-on-failure
```

Para iterar deprisa, una carpeta aparte sin LTO: `cmake -S . -B build-dev -G Ninja -DHYPEREZ_LTO=OFF -DHYPEREZ_INSTALLER=OFF`.

`ctest` ejecuta:

| Prueba | Qué comprueba |
|---|---|
| `lenguaje` | `hyper test tests`: cada `.hz` de `tests/` (lenguaje, bibliotecas, juegos, errores) |
| `ejemplos` | `hyper test examples`: los ejemplos funcionan y su salida coincide con el `.out` |
| `comprobar` | `hyper check` no encuentra errores en los ejemplos, las pruebas ni las bibliotecas de `src/lib` |
| `consola` | una sesión grabada de la consola interactiva da la salida esperada |
| `documentacion` | cada bloque ` ```hz ` de `docs/` y de los README se ejecuta sin errores |
| `documentacion-generada` | `docs/biblioteca/API.md`, `docs/errores.md` y `docs/interno/bytecode.md` están al día |

Si cambias la API, los mensajes de error o el bytecode, regenera la documentación: `python tools/gen_docs.py build/hyper.exe`.

Para la extensión de VS Code, desde `editors/vscode`: `node test/grammar.test.js` (gramática) y `./build.ps1 -VSCodeTests` (pruebas dentro de VS Code; descarga VS Code Insiders la primera vez).

Para medir el rendimiento: `sh tests/bench/minbench.sh build/hyper.exe 7`.

## Dónde está cada cosa

| Quieres cambiar | Mira en |
|---|---|
| Sintaxis | `src/lang/syntax` (lexer, parser, `ast.h`) y `editors/vscode/tools/gen_grammar.py` |
| Formateador (`hyper fmt`) | `src/lang/syntax/format.cpp` |
| Mensajes de error y tipos | `src/lang/sema/analyzer.cpp` |
| Bytecode y optimizaciones | `src/lang/compiler/codegen.cpp`, `src/lang/vm/interp.cpp`; el desensamblador (`hyper bytecode`) en `src/lang/compiler/disasm.cpp` |
| Biblioteca nativa | `src/lang/stdlib/` (un archivo por área) |
| Bibliotecas en HyperEZ | `src/lib/*.hz` (se incrustan al compilar) |
| La orden `hyper` | `src/tools/cli`, `repl`, `watch`, `lsp` |
| HyperEZ Editor | `src/editor/editor.cpp` (Win32 + RichEdit), recursos en `packaging/windows/editor.rc` |
| Instaladores | `packaging/windows`, `packaging/unix` |

[docs/interno](docs/interno) explica la arquitectura y [cómo ampliar HyperEZ](docs/interno/extender.md) paso a paso.

## Hacer cambios

- **Una prueba por cada error corregido o función nueva** (`tests/lang`, `tests/lib`, `tests/juegos`). Si la prueba compara la salida, genera el `.out` con `hyper test --update <archivo>` y revisa que es correcto.
- **Mensajes en español**, claros y con una sugerencia de cómo arreglar el problema cuando sea posible.
- **Sigue el estilo del código que rodea tu cambio** (C++20, 4 espacios, comentarios en español). En `.editorconfig` están las reglas básicas.
- **Documenta** lo nuevo en `docs/` con ejemplos ` ```hz ` que funcionen (se ejecutan en las pruebas).
- **Formato**: `hyper fmt` antes de enviar (la prueba `formato` comprueba los ejemplos, las pruebas y `src/lib`).
- **Instrucciones nuevas de la VM**: añádelas también a `src/lang/compiler/disasm.cpp` (la prueba `bytecode` desensambla todo el código del repositorio).
- **Portabilidad**: el código debe compilar en Windows, Linux y macOS. Lo específico de un sistema va entre `#ifdef _WIN32` / `#else`.

## Enviar los cambios

1. Haz un *fork* y crea una rama: `git switch -c arregla-range`.
2. Asegúrate de que `ctest` pasa.
3. Abre un *pull request* explicando qué cambia y por qué. La integración continua compila y ejecuta todas las pruebas en Windows, Linux y macOS.

## Publicar una versión (mantenedores)

1. `python tools/set_version.py 0.7.0` y añade la sección `0.7.0` a `CHANGELOG.md`.
2. Haz commit, crea la etiqueta y súbela: `git tag v0.7.0 && git push origin main v0.7.0`.
3. El flujo *Release* compila con PGO, pasa las pruebas y publica el instalador, la extensión (`.vsix`), `hyper.exe` y los paquetes de Linux y macOS.
4. Publica la extensión en el Marketplace siguiendo [editors/vscode/PUBLICAR.md](editors/vscode/PUBLICAR.md).
5. Si cambian el logo o el lema, regenera las imágenes con `hyper tools/banner.hz` y sube `assets/branding/social-preview.png` en *Settings → Social preview* del repositorio.
