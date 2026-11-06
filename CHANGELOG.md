# Cambios

Todas las versiones de HyperEZ. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

## [0.8.3] - 2026-11-06

Las carpetas de HyperEZ (`hypersettings`, `hypercode`, `hypertests`...) con sus iconos, y los dos editores, el de Windows y el del navegador, con las mismas funciones.

### Añadido

**Las carpetas de un proyecto HyperEZ** ([Proyectos y carpetas](docs/herramientas/proyectos.md))
- `hyper new` crea los proyectos con `hypersettings/proyecto.json` (nombre, versión, programa principal...), `hypercode/` (los módulos), `hypertests/` (las pruebas) y `hyperdata/`. También se reconocen `hyperlibs/`, `hypertools/`, `hyperassets/`, `hyperdocs/` y `hyperbuild/` (que el `.gitignore` ya excluye).
- **Importar por nombre** desde cualquier carpeta del proyecto: `from jugador import Jugador` encuentra `hypercode/jugador.hez` (y `hyperlibs/<nombre>/main.hez`), también desde las pruebas de `hypertests/`.
- `hyper run` sin archivo ejecuta el `principal` de `proyecto.json`; `hyper test` sin carpeta, las pruebas de `hypertests/` (o `tests/` en los proyectos de antes), desde cualquier subcarpeta.
- `hyper tool`: lista las herramientas de `hypertools/` (con su primer comentario) y `hyper tool <nombre> [args]` ejecuta una.
- `System.projectDir`: la raíz del proyecto (la carpeta con `hypersettings/`), para llegar a `hyperdata/` y `hyperassets/` se ejecute desde donde se ejecute.
- **Iconos de HyperEZ** para cada carpeta (cerrada y abierta) y para `.hez`, `proyecto.json`, Markdown, JSON, imágenes, datos, HTML, CSS y JavaScript: en HyperEZ Editor, en el editor del navegador y como tema de iconos de VS Code. Los dibuja `tools/gen_iconos.js` (`assets/icons/tema`).

**HyperEZ Editor**
- **Tema oscuro HyperEZ Noche** (*Ver → Tema oscuro*), con los colores de VS Code y del navegador y la barra de título oscura; al imprimir se usan los claros.
- **Nuevo proyecto** (Ctrl+Mayús+N): juego, aplicación, IA, herramienta, web o sitio, en `Documentos\HyperEZ`, abierto en el Explorador.
- El Explorador muestra los iconos de HyperEZ y, al abrir un archivo de un proyecto, el proyecto entero con su nombre.
- **Ir a la definición** (F12 o Ctrl+clic), también en otros archivos del proyecto, con el servidor de lenguaje de VS Code y del navegador.
- **Compartir en el editor web**: un enlace con el programa, que se abre y ejecuta en el navegador.
- *Ayuda → La web de HyperEZ* y *HyperEZ en el navegador*.

**Editor en el navegador**
- Los iconos de HyperEZ y un proyecto de bienvenida con `hypersettings`, `hypercode` y `hypertests`.
- **Nuevo proyecto** (Ctrl+Mayús+N): los mismos de `hyper new`, generados por HyperEZ en WebAssembly (`hz_new_project`).
- **Ejecutar las pruebas** (Ctrl+Mayús+T), **repetir al cambiar** (Ctrl+F5), **imprimir** (Ctrl+P), elegir la **fuente**, zoom con **Ctrl+rueda** e **INS/SOB**, como en HyperEZ Editor.
- F5 sobre un archivo que no es código ejecuta el programa principal del proyecto.

**La web**
- Preguntas frecuentes en la portada; canónicas, Open Graph y Twitter en todas las páginas; datos estructurados (schema.org) del lenguaje, el programa, el editor web, el autor y las preguntas; `sitemap.xml` con fechas y prioridades, `manifest.webmanifest` y `humans.txt`.
- Aviso a los buscadores con **IndexNow** en cada publicación, y verificación de Google Search Console y Bing Webmaster Tools con las variables del repositorio `GOOGLE_SITE_VERIFICATION` y `BING_SITE_VERIFICATION`.
- Página *Proyectos y carpetas* en la documentación.

**VS Code** (extensión 0.8.3)
- Tema de iconos **HyperEZ** y la orden *HyperEZ: Usar los iconos de HyperEZ*, que se ofrece una vez al abrir un proyecto con `hypersettings/`.
- F5 y *Ejecutar pruebas* reconocen la raíz del proyecto por `hypersettings/` o `hypertests/`; enlaces a la web.

### Cambiado
- El servidor de lenguaje vuelve a analizar los archivos abiertos cuando se abre otro (o cambia, en el navegador): los errores de una importación que aún no estaba abierta desaparecen solos.
- Herramientas de la extensión al día (`@vscode/vsce` 4.1) y GitHub Actions con Node 22.

### Arreglado
- En el navegador, `Process.exit(código)` y `exit()` terminaban con un «error interno»: ahora acaban el programa con ese código.
- Vulnerabilidad de `minimatch` en las herramientas de publicación de la extensión (Dependabot).

## [0.8.2] - 2026-11-05

La web de HyperEZ, con el editor en el navegador, y un HyperEZ Editor con Explorador que ejecuta sin guardar.

### Añadido

**La web de HyperEZ** ([cuevacaravacaadrian.github.io/HyperEZ](https://cuevacaravacaadrian.github.io/HyperEZ/), en `projects/web`)
- Portada, descargas (con el botón según tu sistema y los archivos de la última versión, sacados de GitHub), **toda la documentación** con buscador y menú lateral, ejemplos, versiones, créditos y licencias.
- **HyperEZ Editor en el navegador**: el lenguaje de verdad compilado a **WebAssembly**, con Explorador de archivos y carpetas, pestañas, ejecutar sin guardar y ■ detener, entrada para `input()`, errores en vivo, autocompletado, ayuda al pasar el ratón y de parámetros, ir a la definición, cerrar parejas, formato, buscar y reemplazar, ejemplos, compartir por enlace, descargar en `.zip` y tema clásico o HyperEZ Noche.
- Cada bloque de código de la documentación tiene su botón **▶ Probar en el editor**.
- La genera el propio HyperEZ (`projects/web/construir.hez`) y se publica sola en GitHub Pages; sus pruebas abren el editor en un Chrome de verdad.

**HyperEZ Editor**
- **Explorador** (Ctrl+Mayús+E): las carpetas y archivos del proyecto, con el icono de HyperEZ en los `.hez`; nuevo archivo, nueva carpeta, renombrar (F2), eliminar a la papelera, mostrar en el Explorador de Windows. *Archivo → Abrir carpeta en el Explorador*.
- **Ejecutar sin guardar**: F5 y Mayús+F5 ejecutan lo que ves en el editor, aunque no esté guardado ni tenga nombre.
- **Cerrar parejas** `( [ { " '`, saltar el cierre ya escrito, rodear la selección, borrar la pareja vacía e Intro entre `{}`.

**Lenguaje y herramientas**
- La extensión de VS Code se llama ahora **`hyperez.hyperezprogramminglanguagevscodeextension`**; el instalador quita las de los nombres anteriores para que no haya dos.
- `hyper run --as <ruta> <archivo>`: ejecuta un archivo como si estuviera en otra ruta (así ejecutan sin guardar los editores).
- `hyper` en WebAssembly (`emcmake cmake`, `src/wasm/web_api.cpp`) y el servidor de lenguaje en memoria, para el navegador.
- VS Code: los archivos sin título se ejecutan sin guardar; con `hyperez.run.saveBeforeRun` desactivado, también los demás.
- `hyper serve` da el tipo correcto a `.hez`, `.md`, `.webp` y `.xml`.

### Arreglado
- `match`: una guarda o un patrón que terminaba en un nombre (`_ if n < limite => ...`) se leía como una función.
- `lexico.md`: `.hez` es la extensión por defecto.

## [0.8.1] - 2026-11-04

La extensión de VS Code, lista para el Marketplace; textos con formato, `do`-`while`, la biblioteca `validate` y todo preparado para que GitHub reconozca HyperEZ.

### Arreglado
- **VS Code: la salida de F5 ya no desaparece.** VS Code cierra la terminal en cuanto el programa termina bien, así que el resultado de un programa rápido no llegaba a verse. Ahora *Ejecutar*, *Comprobar* y las pruebas esperan una tecla al terminar ("Pulsa una tecla para cerrar...").
- La compilación para Windows con `build.ps1` ya no avisa de "serial compilation of LTRANS jobs" al enlazar con LTO.
- `x is text ? {...} : y` ya funciona: tras `is`, un `?` seguido de un mapa es la condición, no el tipo `text?`.

### Cambiado
- La extensión de VS Code se llama ahora **`hyperez.hyperezprogramminglanguage`** (el nombre `hyperez` estaba cogido en el Marketplace). El instalador lee el identificador de `package.json` y quita la extensión antigua `hyperez.hyperez` para que no haya dos.
- Página de la extensión renovada para el Marketplace: capturas en VS Code, primeros pasos, ejemplos, insignias, palabras clave y la categoría *Formatters*.
- Capturas del editor y del instalador al día en el README y la documentación.
- El repositorio ya es público: la documentación lo indica (kit de Linguist, publicar la extensión).

- **Los proyectos nuevos usan `.hez`** (`main.hez`, `src/*.hez`, `tests/*.hez`), la extensión por defecto, y traen `.github/workflows/pruebas.yml` (las pruebas se ejecutan en GitHub con cada cambio) y la insignia *Hecho con HyperEZ*.

### Añadido

**Lenguaje**
- **Formato en las interpolaciones**, como en Python: `"${precio:.2f}"`, `"${nombre:<10}"`, `"${n:>8,}"`, `"${0.25:%}"`, `"${255:x}"`, `"${7:03}"`, `"${"Ana":*^9}"`... y la función `format(valor, "formato")`.
- **`do { ... } while condición`**: el bloque se ejecuta al menos una vez (`do` solo es especial delante de `{`).

**Bibliotecas**
- **`import validate`**: correos, webs, teléfonos, **DNI/NIE** con su letra, **IBAN** con sus dígitos de control, tarjetas (Luhn), ISBN, códigos postales, colores, fechas y contraseñas; y `validate.check(datos, reglas)` para comprobar formularios y JSON con mensajes en español. Ver [validar.md](docs/biblioteca/validar.md).

**GitHub**
- **Kit para GitHub Linguist** completo (`editors/linguist/`): la entrada (`.hez`, color `#62d962`, intérprete `hyper`), 7 muestras reales, el repositorio de la gramática, el texto de la *pull request* y los [objetivos por fases](editors/linguist/OBJETIVOS.md) para llegar al requisito de uso. `tools/linguist_kit.py` lo genera, `--progress` mide los `.hez` de GitHub y `ctest -R linguist` lo comprueba.
- **Acción `setup-hyperez`** para GitHub Actions: `uses: cuevacaravacaadrian/HyperEZ/actions/setup-hyperez@main` instala `hyper` en Windows, Linux o macOS.

**Herramientas**
- `hyper test --pause` (y `--pause` en la ayuda para `check` y `test`).
- VS Code y HyperEZ Editor colorean `do` y el formato de `${...}`; dos fragmentos nuevos, `hacer` y `formato` (54 en total).
- HyperEZ Editor: el panel de abajo (*Problemas* y *Salida*) cambia de alto arrastrando el separador que lo une al código, y se recuerda.

## [0.8.0] - 2026-11-04

Descargas que funcionan, webs con el lenguaje, sintaxis nueva y un editor todavía más clásico.

### Arreglado
- **Las versiones de GitHub ya se publican**: las pruebas fallaban en GitHub Actions (en Windows, los `✓` de los scripts de Python con la consola en `cp1252`; en Linux y macOS, el orden en que `os.walk` recorre las carpetas al generar `docs/errores.md`), así que nunca se llegaba a crear la versión con el instalador. Ahora pasan en Windows, Linux y macOS.
- `texto[2..]` devolvía un texto vacío en Windows (el final "infinito" no cabía en un `long` de 32 bits).
- El formateador alinea las líneas que empiezan cerrando (`}, {`, `})`) con la línea que abrió el paréntesis.
- La prueba del servidor web busca un puerto libre en lugar de fallar si el elegido estaba ocupado.
- Lo que escribe un programa con `print` mientras atiende un servidor web se ve al momento (antes, al pararlo).

### Añadido

**Lenguaje**
- Tubería `|>`: `x |> f` es `f(x)` y `x |> f(a)` es `f(x, a)`; una línea que empieza por `|>` continúa la anterior.
- Comprensiones de lista y de mapa: `[x * x for x in 1..=5 if x % 2 == 1]`, `[i + x for i, x in lista]`, `{n: n * n for n in 1..=3}` (también con `step`).
- Propagación en mapas: `{...base, color: "rojo"}` (las claves de la derecha ganan).
- `**=` y rangos abiertos en los índices: `lista[2..]`, `lista[..3]`, `texto[-3..]`, `lista[..]`.

**Webs**
- `hyper serve [carpeta]`: servidor para desarrollar que sirve una carpeta y **recarga el navegador al guardar** (`--puerto`, `--abrir`, `--red`, `--sin-recarga`).
- `hyper new mi-web --tipo web` (plantillas HTML, formulario, API JSON y CSS, con pruebas) y `hyper new mi-blog --tipo sitio` (páginas desde Markdown).
- `import web`: `app.staticDir`, `app.patch`, `app.all`, `listen(..., { open: true })`, `web.view` (plantillas con base común), `web.form`, `web.cookies`, `web.withCookie`, `web.json`, `web.html`, `web.text`, `web.file`, `web.error`, `web.logger`, `web.decode`, páginas 404 con estilo y `web.devServer`.
- `import site`: webs completas a partir de archivos Markdown, con menú (también con grupos), diseño adaptable al móvil, datos `---` por página, enlaces `.md` → `.html` y código coloreado.
- `import markdown`: `highlightHz` y la opción `{ highlight: true }` para colorear los bloques de código HyperEZ.

**HyperEZ Editor, más clásico**
- Barra de herramientas con los iconos de Windows y *tooltips*; **pestañas** para varios archivos (Ctrl+N, Ctrl+W, Ctrl+Tab, botón central para cerrar).
- **Panel de salida** (Mayús+F5): el programa se ejecuta dentro del editor y su salida aparece abajo, con el tiempo y el resultado; doble clic en `archivo.hez:12` va a esa línea; ■ lo detiene.
- **Imprimir** (Ctrl+P) con colores y número de página, *Ver → Fuente…*, indicador INS/SOB, mostrar u ocultar las barras, *Cerrar las demás pestañas* y abrir varios archivos a la vez.

**Instalador**
- Dos apartados: **Componentes** (el lenguaje, obligatorio; HyperEZ Editor y la extensión de VS Code, opcionales) e **Integración con Windows**. Opción `/NOEDITOR`.
- *App Paths*: `Win+R → hyper` (y `hyperez-editor`) funciona al momento.
- La **documentación en HTML** (`docs-html/`), generada con el propio HyperEZ, para leerla en el navegador sin conexión; la abren la Ayuda del editor y el menú Inicio.
- Los colores para otros editores en `editors/`.

**Otros editores y GitHub**
- Colores para **Notepad++**, **Sublime Text/TextMate**, **Vim/Neovim** y **highlight.js**, generados desde la gramática de VS Code (`tools/gen_editors.py`); guía para usar `hyper lsp` en Neovim y Helix.
- Kit para proponer HyperEZ a **GitHub Linguist** (`editors/linguist/`) y colores en GitHub mientras tanto (`.gitattributes`).

**Extensión de VS Code**
- Colores para `|>`, `**=` y las comprensiones; 4 fragmentos nuevos (52 en total).

**Pruebas**
- `ctest -R proyectos`: crea un proyecto de cada uno de los 6 tipos y pasa sus pruebas. Pruebas nuevas de la sintaxis y de las webs.

## [0.7.0] - 2026-11-04

Un editor propio, mucha más biblioteca y un compilador más rápido.

### Añadido

**HyperEZ Editor** (Windows, `bin\hyperez-editor.exe`)
- Editor de código clásico con fondo blanco que se instala con HyperEZ y se abre al terminar la instalación.
- Colores con el lexer del lenguaje (también dentro de `${...}`), números de línea, errores mientras escribes con el analizador del compilador (panel con doble clic para ir al error y marca en la línea).
- F5 ejecuta, Ctrl+F5 repite al guardar, F6 consola interactiva, F7 comprueba; autocompletado (Ctrl+Espacio) con todas las funciones, módulos y los nombres del archivo; firma de la función al escribir `(`.
- Sangría automática, Tab/Mayús+Tab, comentar (Ctrl+/), duplicar línea, formatear (Ctrl+Mayús+F), buscar y reemplazar, ir a la línea, zoom, ajuste de línea, archivos recientes, arrastrar y soltar, plantillas y todos los ejemplos en el menú.
- Los programas nuevos se guardan como `.hez` en `Documentos\HyperEZ`.

**Lenguaje y herramientas**
- `hyper fmt [--check]`: formateador que ordena la sangría y los espacios sin cambiar los tokens (lo comprueba); también en el servidor de lenguaje, VS Code y HyperEZ Editor.
- `hyper bytecode archivo`: desensamblador que muestra las instrucciones que genera el compilador.
- Repetir listas: `[0] * 5`. `texto.indexOf(sub, desde)`.

**Biblioteca nativa** (C++)
- Matemáticas: `gcd`, `lcm`, `factorial`, `isPrime`, `primes`, `divisors`, `cbrt`, `trunc`, `fract`, `sinh`, `cosh`, `tanh`, `randomGaussian`, `noise` (ruido de Perlin), `chr` y `ord`.
- Textos: `removeAccents`, `title`, `camelCase`, `pascalCase`, `snakeCase`, `kebabCase`, `slug`, `truncate`, `wrap`, `distance` (Levenshtein), `similarity`, `indent`, `dedent`, `between`, `isBlank`, `codes` y `reverseWords`.
- Listas: `countBy`, `sumBy`, `uniqueBy`, `windowed`, `rotate`, `intersect`, `difference`, `union`, `takeWhile`, `skipWhile`, `none`, `sample`, `toSet`, `compact`, `product`, `lastIndexOf`, `binarySearch`, `scan`, `enumerate`, `toMap`, `median` e `insertSorted`.
- Mapas: `pick`, `omit`, `invert`, `update`, `any`, `all`, `count`, `find`, `mapKeys`, `sortByKey` y `sortByValue`. Conjuntos: `union`, `intersect`, `difference`, `symmetricDifference`, `isSubset`, `isSuperset`, `isDisjoint`, `map` y `filter`.
- Números: `isEven`, `isOdd`, `clamp`, `between`, `toRoman`, `format` (separador de miles al estilo español), `digits`, `times` y `toPercent`.
- `Time`: `today`, `addDays`, `addMonths`, `daysBetween`, `isLeapYear`, `daysInMonth`, `monthName` y `dayName` (en español), `parse`, `duration` y `ago` ("hace 5 minutos").
- `JSON.load` y `JSON.save`. `Hash.md5`, `Hash.sha1` y `Hash.hmac` (HMAC-SHA256). `Bytes.compress(nivel, raw)` y `decompress(raw)` para deflate puro.
- `Matrix`: `det`, `inverse`, `solve` (sistemas de ecuaciones, LU con pivoteo), `trace`, `norm`, `std`, `diag`, `slice` y `Matrix.concat`.
- `Image`: `ellipse`, `polygon`, `gradient`, `blur`, `rotate`, `brightness`, `contrast`, `sepia`, `threshold` y `floodFill`.

**Bibliotecas escritas en HyperEZ** (8 nuevas, 17 en total)
- `collections`: `Stack`, `Queue`, `Deque`, `PriorityQueue`, `Counter`, `LRUCache` y `Graph` (recorridos, Dijkstra, distancias, componentes, orden topológico, ciclos).
- `algo`: permutaciones, combinaciones, producto cartesiano, subconjuntos, subsecuencia común más larga, `memoize` y cuadrículas (A*, vecinos, relleno, dibujar el camino).
- `template` (plantillas con variables, `if`, `unless` y `each`), `markdown` (Markdown a HTML y páginas completas), `ini` (archivos de configuración), `log` (registro con niveles, colores y archivo), `svg` (dibujos y gráficas de barras, líneas y tarta) y `zip` (crear, listar, leer y extraer `.zip` compatibles).

**Instalador**
- Instala HyperEZ Editor y lo abre al terminar; casillas nuevas *Acceso directo en el escritorio* y *Abrir HyperEZ Editor al terminar*; opciones `/NODESKTOP` y `/OPENEDITOR`.
- Clic derecho: *Editar con HyperEZ Editor* (y *Editar con Visual Studio Code* si está), HyperEZ Editor en *Abrir con* y *Nuevo → Programa HyperEZ* (`.hez`).
- Crea `Documentos\HyperEZ` para los programas (al desinstalar solo se quita si está vacía).

**VS Code**
- Botones ▶ Ejecutar · ⟳ Repetir al guardar · ✓ Comprobar encima del código y ▶ Probar sobre cada `test(...)`; dar formato (Mayús+Alt+F, y al guardar si se activa); ver el bytecode; abrir en HyperEZ Editor; `.hez` como extensión por defecto.

**Documentación y ejemplos**
- Guías nuevas: [HyperEZ Editor](docs/herramientas/editor.md), [colecciones y algoritmos](docs/biblioteca/colecciones-algoritmos.md) y [textos, números, fechas y documentos](docs/biblioteca/textos-documentos.md); secciones nuevas sobre `.zip`, SVG, filtros de imagen y álgebra lineal; README más claro con capturas.
- Ejemplos nuevos: laberinto con A*, rutas entre ciudades, gráficas SVG, comprimir en `.zip`, página web desde Markdown y textos y números.
- Pruebas nuevas: formateador (`ctest -R formato`), desensamblador de todo el código (`ctest -R bytecode`) y 32 pruebas de las novedades.

### Cambiado
- Compilador: los `for` numéricos cuya variable no captura ninguna función usan `FOR_CHECK_V`/`FOR_NEXT_V` (dos instrucciones menos por vuelta) y las asignaciones como sentencia guardan y descartan en una sola instrucción (`DEFINE_GLOBAL`, `SET_LOCAL_POP`).
- Máquina virtual: cada operación aritmética y de comparación tiene su propio caso, `%` usa división de 32 bits cuando los números caben, y el hash de los textos largos se calcula por muestras (concatenar a un texto que crece ya no lo recorre entero: `texto` pasa de 148 a 121 ms). Un `for` vacío tarda la mitad. Números en [rendimiento](docs/interno/rendimiento.md).
- El editor de texto, la ayuda y la extensión proponen `.hez` como extensión de los programas nuevos.

## [0.6.0] - 2026-11-03

Una base para hacer de todo: HyperEZ deja de ser solo un lenguaje para juegos.

### Añadido

**Lenguaje**
- Desestructuración: `var [a, b, ...resto] = lista` y `var {nombre, vida: puntos} = objeto` (también con `let` y `const`).
- Sobrecarga de operadores en clases y en clases nativas: `add`, `sub`, `mul`, `div`, `mod`, `pow`, `compareTo` (para `<`, `>`...), `equals` (para `==`) y `get`/`set` (para `obj[i]`); `len()` usa la propiedad `length`.
- Textos sin escapes `r"..."` (expresiones regulares, rutas de Windows).
- Una primera línea `#!/usr/bin/env hyper` se ignora: scripts ejecutables en Linux y macOS.
- `x is Tipo ? a : b` ya no se confunde con un tipo opcional.
- `n.toHex()`, `n.toBinary()`, `n.toBase(b)` y `parseInt(texto, base)`.

**Biblioteca nativa** (C++)
- `Process`: `run`, `shell`, `start` (en segundo plano, con `wait`, `kill` y `running`), `exit`, `open`, `pid`.
- `Path`: `join`, `dir`, `name`, `stem`, `ext`, `withExt`, `absolute`, `normalize`, `relative`, `home`, `temp`.
- `File`: `copy`, `move`, `size`, `modified`, `isFile`, `listAll`, `deleteDir`, `tempFile`, `readBytes`, `writeBytes`.
- `Time`: `now`, `date`, `format`, `make`, `measure`, `clock`. `System`: `cpuCount`, `user`, `hostname`, `exe`, `arch`, `setEnv`.
- `Bytes`: bloques binarios con lectura y escritura de enteros y decimales (little y big endian), hexadecimal, Base64 y compresión zlib (`compress`/`decompress`).
- `Hash` (`sha256`, `crc32`, `fnv1a`, `uuid`) y `Base64`.
- `Regex`: buscar, validar, grupos, reemplazar (también con una función) y dividir; `texto.matches()` y `texto.replaceRegex()`.
- `Http`: cliente HTTP/HTTPS (`get`, `post`, `request`, `getJson`, `postJson`, `download`, `query`, `encode`) y **servidor web** `Http.serve` con archivos estáticos, JSON y CORS.
- `Matrix`: álgebra lineal en C++ con expansión de filas y columnas, producto, transpuesta, activaciones de redes neuronales y reducciones.
- `Image`: imágenes RGBA con rectángulos, líneas, círculos, texto con la fuente DejaVu Sans incluida, transformaciones, y lectura y escritura de PNG, JPG, BMP y TGA.

**Bibliotecas escritas en HyperEZ** (`import nombre`, incluidas en `hyper`)
- `ai`: redes neuronales (perceptrón multicapa con sigmoide, ReLU, tanh y softmax, entrenamiento por lotes), regresión lineal, k vecinos, k-medias, normalización y one-hot.
- `stats`, `csv`, `cli` (argumentos, preguntas, barras de progreso, tablas), `web` (rutas con parámetros y middleware), `html`, `db` (base de datos de documentos en JSON), `audio` (sonidos y música en WAV) y `x86` (ensamblador de 16 bits para sectores de arranque).

**Herramientas**
- `hyper new <nombre> --tipo juego|app|ia|script`, con la estructura `main.hz`, `src/`, `tests/` y `datos/`.
- Linux y macOS: `hyper register` (asociación de archivos con xdg), modo watch, teclas en tiempo real para juegos y paquete `.tar.gz` con `install.sh`.
- Instalador de Windows: HyperEZ queda siempre como programa predeterminado de los `.hz`, nuevas acciones *Ejecutar y repetir al guardar* y *Abrir HyperEZ aquí*, cabecera con los colores de la marca y botón *Abrir la consola* al terminar.
- `hyper help` organizado por secciones y `hyper version` con los datos de compilación.
- VS Code: tutorial de bienvenida, tema *HyperEZ Noche*, asistente de proyecto con tipos, 14 fragmentos nuevos y guía para publicar en el Marketplace.

**Documentación**
- Documentación completa en `docs/`: instalación, primeros pasos, tutoriales (juego e IA), referencia de toda la sintaxis con su gramática, guías de cada área de la biblioteca, herramientas, funcionamiento interno y hoja de ruta.
- Generada a partir del código (y comprobada en las pruebas): referencia de la API, catálogo de códigos de error e instrucciones del bytecode.
- 18 ejemplos organizados en 12 áreas.
- Imágenes de presentación del repositorio generadas con el propio HyperEZ (`tools/banner.hz`).

### Mejorado

- Rendimiento (ver `docs/interno/rendimiento.md`): `fib` 25 → 18 ms (−28 %), métodos con campos 21 → 17 ms (−19 %), operaciones con `Vec2` 62 → 18 ms (3,4 veces más rápido). Gracias a: llamadas directas a funciones y métodos, comprobación de tipos de parámetros sin comparar textos, comparación y salto fusionados, bucle `for` en una instrucción por vuelta y asignador de objetos pequeños.
- Los números aleatorios dan la misma secuencia en Windows, Linux y macOS con la misma semilla.
- Código fuente organizado en carpetas (`syntax`, `sema`, `compiler`, `vm`, `stdlib`, `prelude`, `lib`, `tools/*`).
- Integración continua en Windows, Linux y macOS; las versiones publican también los paquetes de Linux y macOS.

## [0.5.0] - 2026-11-03

Primera versión pública.

### Añadido

- **Consola interactiva**: `hyper repl`, o `hyper` sin argumentos. Escribe código y ve el resultado al momento. Las variables, funciones y clases se conservan entre líneas y se pueden redefinir. Las entradas de varias líneas se detectan solas y Ctrl+C detiene un bucle sin cerrar la sesión. Comandos `.vars`, `.codigo`, `.guardar`, `.cargar`, `.limpiar`, `.reiniciar` y `.ayuda`.
- **Modo watch**: `hyper watch [archivo.hz]` ejecuta el programa y lo repite cada vez que se guarda un `.hz` de su carpeta.
- VS Code: comandos *Ejecutar y repetir al guardar* (Ctrl+Alt+F5) y *Abrir consola interactiva*.
- Instalador: acceso directo *HyperEZ (consola interactiva)* en el menú Inicio. Instala también `LICENSE` y `CHANGELOG.md`.
- Compilación con optimización guiada por perfil (PGO) en `build.ps1`.
- Integración continua y publicación automática de versiones en GitHub Actions.
- `tools/set_version.py` (cambia la versión en todo el proyecto) y `tools/github_usuario.py` (pone el usuario de GitHub en los enlaces).

### Mejorado

- Rendimiento (benchmarks de `tests/bench`): llamadas a funciones un 40 % más rápidas, objetos un 32 %, bucles un 11 % y concatenación de textos un 37 %.
- `input()` lee bien los acentos y la ñ en la consola de Windows.

### Cambiado

- Licencia MIT.

## [0.4.0] - 2026-11-03

### Añadido

- Instalador de Windows (`HyperEZ-Setup.exe`): asociación de `.hz`/`.hez` con icono, menú contextual (*Editar*, *Comprobar errores*), `PATH`, menú Inicio, extensión de VS Code y desinstalador.
- Extensión de VS Code con servidor de lenguaje (`hyper lsp`): diagnósticos, autocompletado, hover, ayuda de firmas, definición, referencias, renombrar, esquema y arreglos rápidos.
- Comandos `hyper test` (con comparación de salida `.out`), `hyper new`, `hyper docs`, `hyper register`.
- Biblioteca estándar: `test`/`assertEqual`/`assertThrows`, módulos `File`, `System` y `Console`, y nuevas funciones de listas, textos y mapas.
- `match` con bloques: la última expresión del bloque es el valor.
- Juegos de ejemplo: serpiente, tres en raya, mazmorra y adivina el número.

### Corregido

- `range(n)` con un solo argumento, `from "./archivo.hz" import ...`, herencia de `Error`, reasignación de `let` globales y errores en cascada tras un `match` con errores.
