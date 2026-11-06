# El editor en el navegador

**[cuevacaravacaadrian.github.io/HyperEZ/editor](https://cuevacaravacaadrian.github.io/HyperEZ/editor/index.html)**: HyperEZ Editor dentro de la web, sin instalar nada. Es el mismo lenguaje que instalas en tu equipo, compilado a **WebAssembly**: el compilador, la máquina virtual, las bibliotecas y el servidor de lenguaje de VS Code funcionan dentro de la página, y **tu código no sale de tu navegador**.

## Lo que tiene

| | |
|---|---|
| **El aspecto de HyperEZ Editor** | menú, barra de herramientas, pestañas, números de línea, panel de abajo y barra de estado; tema clásico o **HyperEZ Noche** (*Ver*) |
| **Explorador** | carpetas y archivos de tu proyecto, con los **iconos de HyperEZ** (`hypersettings`, `hypercode`, `hypertests`...): crear, renombrar (F2), eliminar (Supr), arrastrar para mover, y arrastrar archivos desde el equipo para añadirlos |
| **Nuevo proyecto (Ctrl+Mayús+N)** | los mismos de `hyper new` (juego, aplicación, IA, herramienta, web, sitio), generados por HyperEZ en WebAssembly, en una carpeta nueva. Las importaciones por nombre desde `hypercode/` funcionan, también los errores en vivo. Ver [Proyectos y carpetas](proyectos.md) |
| **Ejecutar las pruebas (Ctrl+Mayús+T)** | los `.hez` de `hypertests/` del proyecto, uno tras otro, como `hyper test`, con el recuento al final |
| **Repetir al cambiar (Ctrl+F5)** | el modo *watch*: el programa se vuelve a ejecutar cada vez que dejas de escribir |
| **Ejecutar sin guardar (F5)** | el programa ve los archivos tal como están en el editor (con sus importaciones); **■ Detener** lo para aunque sea un bucle infinito. Los archivos que crea (imágenes, CSV, HTML...) aparecen en el Explorador |
| **Entrada del programa** | lo que escribas en la pestaña *Entrada del programa* es lo que lee `input()`, línea a línea |
| **Errores en vivo** | subrayados y en el panel *Problemas*, en español; el número de la línea se pone en rojo |
| **Autocompletado** | según el tipo, con los campos de tus clases y toda la biblioteca (Ctrl+Espacio para abrirlo) |
| **Ayuda** | al pasar el ratón por un nombre y la de los parámetros al escribir una llamada; **Ctrl+clic** (o F12) va a la definición |
| **Cerrar parejas** | `(`, `[`, `{`, `"` y `'` se cierran solos; Intro entre `{}` deja el cierre debajo |
| **Formato** | Mayús+Alt+F ordena la sangría y los espacios, como `hyper fmt` |
| **Buscar y reemplazar** | Ctrl+F y Ctrl+H; Ctrl+G para ir a una línea |
| **Imprimir (Ctrl+P)** | el código con sus colores y el nombre del archivo |
| **Fuente, zoom e INS/SOB** | *Ver → Fuente…* (Consolas, Cascadia Code, Fira Code...), Ctrl+rueda o Ctrl++ / Ctrl+- para el tamaño, y la tecla Insert (o un clic en *INS* abajo) para sobrescribir |
| **Ejemplos** | todos los de HyperEZ, en el menú *Ejemplos* o desde la página de [ejemplos](https://cuevacaravacaadrian.github.io/HyperEZ/ejemplos.html) |
| **Compartir** | *Compartir* copia un enlace con el código: quien lo abra lo ve y lo ejecuta en su navegador |
| **Descargar** | el archivo (Ctrl+S) o el proyecto entero en un `.zip`, para seguir en tu equipo con `hyper run` y `hyper test` |
| **`Process.exit(código)`** | termina el programa con ese código, igual que en tu equipo |

Tu proyecto se guarda en el navegador (en el almacenamiento de la página): si cambias de navegador o borras sus datos, descárgalo antes en `.zip`. *Archivo → Empezar de cero* vuelve al proyecto de bienvenida.

## Lo que necesita la versión de escritorio

En la web no hay teclado en tiempo real ni acceso a los archivos de tu equipo, a la red o a otros programas. Para **juegos de consola** (`Console.readKey`), servidores web, `Process`, `Http` y lo que use el sistema, [descarga HyperEZ](https://cuevacaravacaadrian.github.io/HyperEZ/descargar.html): en Windows trae HyperEZ Editor.

## Cómo está hecho

- `src/wasm/web_api.cpp` y la opción de CMake para Emscripten compilan el lenguaje a `hyperez.wasm` (unos 3 MB): `hz_run` ejecuta un programa del sistema de archivos virtual, `hz_lsp` es el servidor de lenguaje en memoria y `hz_new_project` crea los proyectos de `hyper new`.
- La página (`projects/web/src/editor`) no usa bibliotecas externas: un `textarea` con el código coloreado debajo, y dos *Workers*, uno para el servidor de lenguaje y otro para cada ejecución.
- La web entera la genera el propio HyperEZ (`projects/web/construir.hez`) y GitHub Actions la publica en GitHub Pages con cada cambio. Ver [projects/web](../../projects/web/README.md).
