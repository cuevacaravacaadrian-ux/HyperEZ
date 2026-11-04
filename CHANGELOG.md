# Cambios

Todas las versiones de HyperEZ. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

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
