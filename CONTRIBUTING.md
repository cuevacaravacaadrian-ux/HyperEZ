# Contribuir a HyperEZ

Gracias por ayudar. Esta guía explica cómo compilar HyperEZ, probar tus cambios y enviarlos.

## Avisar de un error o proponer una idea

Abre una [incidencia](https://github.com/cuevacaravacaadrian-ux/HyperEZ/issues/new/choose). Para un error, incluye el programa `.hz` más pequeño que lo reproduzca, lo que esperabas y lo que pasó (copia el mensaje completo), y la salida de `hyper version`.

## Preparar el entorno

Necesitas Windows con:

- [MSYS2](https://www.msys2.org/) y, en su terminal *UCRT64*:
  `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja`
- Python 3 (pruebas de la documentación y generador de la gramática).
- Node.js 20+ (solo para la extensión de VS Code).

`build.ps1` añade `C:\msys64\ucrt64\bin` al `PATH` por su cuenta. Si MSYS2 está en otra carpeta, indícala en la variable `HYPEREZ_MSYS`.

## Compilar y probar

```powershell
./build.ps1 -NoPGO -SkipVsix   # lo más rápido mientras desarrollas
./build.ps1                    # todo, como en una versión publicada
```

Las pruebas se ejecutan con `ctest --test-dir build --output-on-failure`. Comprueban cuatro cosas:

| Prueba | Qué comprueba |
|---|---|
| `lenguaje` | `hyper test tests`: cada `.hz` de `tests/` (las llamadas a `test(...)` y, si hay un `.out` al lado, la salida exacta) |
| `ejemplos` | `hyper test examples`: los ejemplos se juegan solos y su salida coincide con el `.out` |
| `comprobar` | `hyper check` no encuentra errores en los ejemplos ni en las pruebas |
| `documentacion` | cada bloque ` ```hz ` de `docs/GUIA.md` y de los README se ejecuta sin errores |

Para la extensión de VS Code, desde `editors/vscode`: `node test/grammar.test.js` (gramática) y `./build.ps1 -VSCodeTests` (pruebas dentro de VS Code; descarga VS Code Insiders la primera vez).

Para medir el rendimiento: `sh tests/bench/minbench.sh build/hyper.exe 7` (mínimo de 7 ejecuciones de cada benchmark).

## Hacer cambios

- **Una prueba por cada error corregido o función nueva.** Las pruebas del lenguaje están en `tests/lang/`; las de lógica de juegos, en `tests/juegos/`. Si la prueba compara la salida, genera el `.out` con `hyper test --update <archivo>` y revisa que el contenido es el correcto.
- **Mensajes en español**, claros y con una sugerencia de cómo arreglar el problema cuando sea posible.
- **Sigue el estilo del código que rodea tu cambio** (C++20, 4 espacios, comentarios en español). En `.editorconfig` están las reglas básicas.
- Si cambias la API de la biblioteca estándar, actualiza `docs/GUIA.md` cuando corresponda: sus ejemplos se ejecutan en las pruebas.

## Enviar los cambios

1. Haz un *fork* y crea una rama: `git switch -c arregla-range`.
2. Asegúrate de que `ctest` pasa.
3. Abre un *pull request* explicando qué cambia y por qué. La integración continua compila y ejecuta todas las pruebas en Windows.

## Publicar una versión (mantenedores)

1. `python tools/set_version.py 0.6.0` y añade la sección `0.6.0` a `CHANGELOG.md`.
2. Haz commit, crea la etiqueta y súbela: `git tag v0.6.0 && git push origin main v0.6.0`.
3. El flujo *Release* de GitHub Actions compila con PGO, pasa las pruebas y publica el instalador, la extensión (`.vsix`) y `hyper.exe` en la página de versiones.
