# La web de HyperEZ

**[cuevacaravacaadrian.github.io/HyperEZ](https://cuevacaravacaadrian.github.io/HyperEZ/)**: la portada, las descargas, toda la documentación, los ejemplos, las versiones, los créditos y **HyperEZ Editor en el navegador**, con el lenguaje de verdad compilado a WebAssembly.

La genera el propio HyperEZ (`construir.hez`, con las bibliotecas `site`, `markdown` y `template`) a partir del repositorio: los `.md` de `docs/`, el `CHANGELOG.md`, la carpeta `examples/`, las licencias y las imágenes. GitHub Actions (`.github/workflows/web.yml`) la construye, la prueba y la publica en GitHub Pages con cada cambio en `main`.

## Qué hay aquí

| | |
|---|---|
| `construir.hez` | el generador: `hyper projects/web/construir.hez [salida] [--wasm carpeta] [--url ...] [--base /HyperEZ/]` |
| `src/plantilla.html` | la cabecera, el menú y el pie de todas las páginas |
| `src/paginas/` | la portada, descargas, ejemplos, versiones, créditos y la página 404 (plantillas de `template`) |
| `src/muestras/` | los programas de la portada: se ejecutan al construir la web para enseñar su salida real |
| `src/estilos.css`, `src/sitio.js` | el diseño y lo que la web hace en el navegador (botón de descarga según el sistema, archivos de la última versión de GitHub, filtros) |
| `src/editor/` | el editor en el navegador: `index.html`, `editor.css`, `editor.js`, `nucleo.js` (lo que no depende del navegador), `lsp-worker.js` y `run-worker.js` |
| `tests/` | `nucleo.test.js` (el núcleo del editor), `wasm.test.js` (el módulo WebAssembly) y `navegador.test.js` (el editor en un Chrome de verdad) |
| `dist/` | la web generada (no se sube al repositorio) |

## Construirla en tu equipo

1. **HyperEZ en WebAssembly** (para el editor), con [Emscripten](https://emscripten.org/docs/getting_started/downloads.html):

```text
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web                         → build-web/hyperez.js y hyperez.wasm
node projects/web/tests/wasm.test.js build-web/hyperez.js
```

2. **La web** (con el `hyper` normal):

```text
hyper projects/web/construir.hez                → projects/web/dist
hyper serve projects/web/dist --abrir
```

Sin el paso 1 la web se genera igual, pero el editor no puede ejecutar código.

3. **Las pruebas del editor**:

```text
node projects/web/tests/nucleo.test.js
node projects/web/tests/navegador.test.js http://localhost:8080/editor/index.html
```

## Publicarla

Cada `push` a `main` la publica sola. La primera vez hay que activar GitHub Pages: *Settings → Pages → Source: GitHub Actions*.
