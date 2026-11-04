# Objetivos: HyperEZ, lenguaje reconocido por GitHub

La meta final es que GitHub Linguist acepte HyperEZ. Lo técnico ya está preparado ([README.md](README.md)); lo que decide es el **uso real**: al menos **2000 archivos `.hez`** indexados en el último año, sin forks y repartidos entre muchas personas y repositorios. Este es el camino, con metas que se pueden medir.

Medición: `python tools/linguist_kit.py --progress` (cuenta los `.hez` de GitHub sin los del autor) y la búsqueda [`NOT is:fork path:*.hez`](https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.hez).

| | Punto de partida (4 de noviembre de 2026) |
|---|---|
| Archivos `.hez` en GitHub de otras personas | 18, de 8 usuarios, y no parecen de HyperEZ |
| Meta de Linguist | 2000 en el último año, de muchas personas |

## Fase 1 — Que se pueda usar y encontrar (ahora)

| Objetivo | Cómo | Hecho |
|---|---|---|
| Descargas que funcionan | versiones en GitHub con instalador, paquetes y `.vsix` | ✅ 0.8.0 |
| Repositorio público con documentación completa | README, guía, tutoriales, API, ejemplos | ✅ |
| `.hez` en todo lo nuevo | `hyper new`, el editor y VS Code crean `.hez` | ✅ 0.8.1 |
| Extensión en el Marketplace de VS Code y en Open VSX | [PUBLICAR.md](../vscode/PUBLICAR.md) | ⬜ |
| Repositorio `hyperez-grammar` | [README.md](README.md#el-repositorio-de-la-gramática) | ⬜ |
| Pruebas en GitHub Actions para cualquier proyecto | la acción `setup-hyperez` y el flujo que crea `hyper new` | ✅ 0.8.1 |
| Versión 0.8.1 publicada | etiqueta `v0.8.1` | ⬜ |

## Fase 2 — Las primeras 25 personas (meta: 250 archivos `.hez`)

- **Un repositorio plantilla** (`hyperez-plantilla`, con *Use this template*): un proyecto de `hyper new` con las pruebas en GitHub Actions. Es la forma más rápida de que alguien suba su primer proyecto.
- **Enseñar con HyperEZ**: el lenguaje está en español y pensado para aprender; ofrecérselo a profesores de informática y tecnología, academias y clubes de programación, con los [tutoriales](../../docs/empezar) y la página de bienvenida de VS Code.
- **Pedir que suban sus programas** a GitHub con archivos `.hez` (ya lo dicen la extensión y la documentación).
- **Publicar proyectos propios de ejemplo** que otras personas quieran copiar y mejorar (un juego, una web, una herramienta). Ojo: los del autor no cuentan para Linguist, pero atraen a quien sí cuenta.
- **Dar a conocer el proyecto** en comunidades hispanohablantes de programación (foros, Reddit, Discord, Mastodon...) con capturas y un enlace a las descargas.

## Fase 3 — Comunidad (meta: 100 personas, 1000 archivos)

- **Retos y ejercicios** semanales o mensuales que se resuelven subiendo un repositorio con `.hez`.
- **Una galería** de proyectos con el tema `hyperez` en GitHub y la insignia *Hecho con HyperEZ* (la ponen los proyectos de `hyper new`).
- **Discusiones** en el repositorio (GitHub Discussions) para preguntas y para enseñar proyectos.
- **Bibliotecas de la comunidad**: que otras personas publiquen sus módulos `.hez`.
- **Rosetta Code y sitios parecidos**: resolver allí tareas en HyperEZ enlazando al repositorio.

## Fase 4 — El umbral y la propuesta (meta: 2000 archivos en el último año)

1. Cuando `--progress` pase de 2000 y los resultados estén repartidos entre muchas personas, revisa a mano la búsqueda: que sean programas de HyperEZ.
2. Crea (o actualiza) el repositorio de la gramática.
3. Abre la *pull request* a Linguist con [PULL_REQUEST.md](PULL_REQUEST.md) y los pasos de [README.md](README.md#la-pull-request-cuando-se-cumpla-el-requisito-de-uso).
4. Si piden cambios, el kit se regenera con `python tools/linguist_kit.py` y `ctest -R linguist` comprueba que sigue bien.

## Qué no hacer

- **No crear cuentas ni repositorios para inflar la cifra.** Linguist revisa la búsqueda, quita a los usuarios que acaparan resultados, rechaza la propuesta y va contra las normas de GitHub.
- **No pedir `.hz`**: lo usan miles de archivos que no son HyperEZ.
- **No abrir la *pull request* antes de tiempo**: la cierran sin revisarla si no llega al requisito, y una segunda propuesta cuesta más.

## Otros sitios donde registrar HyperEZ (no dependen de Linguist)

| Dónde | Qué da | Estado |
|---|---|---|
| Marketplace de VS Code y Open VSX | instalar la extensión desde el editor | ⬜ [PUBLICAR.md](../vscode/PUBLICAR.md) |
| Temas de GitHub (`hyperez`, `programming-language`...) | aparecer en las búsquedas por tema | ✅ |
| winget (Windows), Scoop, Homebrew | `winget install HyperEZ` y parecidos | ⬜ (necesita versiones estables y algo de uso) |
| highlight.js, Pygments, Prism | colores de HyperEZ en webs y documentación | ⬜ [highlight.js](../highlightjs) ya preparado |
