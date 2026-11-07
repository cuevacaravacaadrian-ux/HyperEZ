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

## Un dominio propio (gratis)

Con [FreeDomain](https://github.com/DigitalPlatDev/FreeDomain) se consigue gratis un dominio como `hyperez.dpdns.org`:

1. Regístrate en el [panel de DigitalPlat](https://dash.domain.digitalplat.org) y pide el nombre.
2. FreeDomain no tiene editor de registros: apunta el dominio a un DNS gratuito (por ejemplo, Cloudflare) con sus servidores de nombres.
3. En ese DNS, crea un registro **CNAME** del dominio hacia `cuevacaravacaadrian.github.io`.
4. En el repositorio, crea la variable `WEB_DOMINIO` con el dominio (*Settings → Secrets and variables → Actions → Variables*) y, en *Settings → Pages → Custom domain*, escribe el mismo dominio y activa *Enforce HTTPS*.

En la siguiente publicación, la web se genera para ese dominio (enlaces, sitemap, datos para buscadores y `CNAME`).

## Que la encuentren los buscadores

La web ya lleva lo que miran Google, Bing y compañía: título y descripción de cada página, dirección canónica, Open Graph y Twitter (la vista previa al compartir un enlace), datos estructurados de schema.org (el lenguaje, el programa, el editor web, el autor y las preguntas frecuentes), `sitemap.xml` con fechas, `robots.txt`, `manifest.webmanifest` y `humans.txt`.

- **IndexNow** (Bing, Yandex, Seznam, Naver): al publicar, `web.yml` les avisa de todas las páginas del `sitemap.xml`. La clave está en `construir.hez` (`INDEXNOW`) y se publica como `<clave>.txt`.
- **Google Search Console** y **Bing Webmaster Tools**: añade la propiedad `https://cuevacaravacaadrian.github.io/HyperEZ/`, elige la verificación por *etiqueta HTML* y copia solo el código (`content="..."`) en una variable del repositorio (*Settings → Secrets and variables → Actions → Variables*): `GOOGLE_SITE_VERIFICATION` o `BING_SITE_VERIFICATION`. En la siguiente publicación la web lleva la etiqueta; entonces pulsa *Verificar* y envía el `sitemap.xml`. En tu equipo: `construir.hez --google <código> --bing <código>`.
