# Cambios

Todas las versiones de HyperEZ. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

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
