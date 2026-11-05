# Hoja de ruta

HyperEZ quiere ser un lenguaje **universal y en español**: fácil para aprender y lo bastante potente para cualquier proyecto, desde un juego hasta una IA o las herramientas de un sistema operativo. Esto es lo que ya hay y lo que viene.

## Ya disponible (0.8)

| Área | Qué hay |
|---|---|
| Lenguaje | tipado gradual, clases, structs, interfaces, enums, `match`, errores, eventos, `async`/`await`, módulos, desestructuración, comprensiones, tubería `\|>`, propagación en listas y mapas, trozos `lista[2..]`, sobrecarga de operadores, textos `r"..."` |
| Editores | **HyperEZ Editor** (Windows, con pestañas y panel de salida), extensión de VS Code, colores para Notepad++, Sublime, Vim y highlight.js, servidor de lenguaje para cualquier editor |
| Herramientas | consola interactiva, modo watch, pruebas con salidas esperadas, formateador (`hyper fmt`), desensamblador (`hyper bytecode`), instaladores para Windows, Linux y macOS |
| Sistema | procesos (también en segundo plano), archivos, rutas, fechas (en español, con sumas de días y meses), variables de entorno, `.zip` |
| Datos | JSON, CSV, INI, base de datos de documentos, estadística, plantillas, Markdown, registro de mensajes |
| Estructuras y algoritmos | pilas, colas, colas de prioridad, contadores, cachés, grafos (Dijkstra, orden topológico), A* en cuadrículas, combinatoria |
| IA | matrices en C++ (con inversa, determinante y sistemas de ecuaciones), redes neuronales, regresión, k vecinos, k-medias |
| Web | cliente HTTP/HTTPS, servidor web, rutas, plantillas, formularios, cookies, sitios desde Markdown, `hyper serve` con recarga automática |
| Gráficos y sonido | imágenes PNG/JPG con texto, polígonos, degradados y filtros; gráficas SVG; sonidos WAV |
| Bajo nivel | `Bytes`, bits, hash (SHA-256, SHA-1, MD5, HMAC), compresión, ensamblador x86 para sectores de arranque |

## Próximos pasos

**Lenguaje**
- Genéricos en las clases propias (`class Pila<T>`).
- Iteradores y generadores (`yield`) para recorrer estructuras propias con `foreach`.
- `defer` para liberar recursos al salir de una función.

**Rendimiento**
- Puntero de pila en un registro dentro del intérprete y despacho con saltos directos (*computed goto*).
- Textos que crecen sin copiarse en cada `+=` (cuerdas o búferes compartidos).
- Valores de 8 bytes (*NaN-boxing*) para reducir a la mitad el tráfico de memoria.
- Compilación a código nativo de las funciones más usadas (JIT) o de programas enteros.

**Sistemas operativos y kernels**
- Un *backend* que traduzca HyperEZ a C (y de ahí a código máquina sin sistema operativo debajo), con un subconjunto del lenguaje sin recolector de basura para escribir kernels y controladores.
- Ensamblador x86 de 32 y 64 bits y formatos ELF/PE en la biblioteca `x86`.

**Biblioteca**
- Hilos de trabajo para cálculo en paralelo.
- SQLite, WebSockets y TCP/UDP de bajo nivel.
- Ventanas y gráficos en tiempo real, como una biblioteca opcional separada del núcleo.

**Ecosistema**
- Gestor de paquetes (`hyper add paquete`) con un registro público.
- Más tipos de proyecto en `hyper new` (web, biblioteca) y plantillas de la comunidad.
- HyperEZ Editor también en Linux y macOS, con pestañas y depurador paso a paso.
- Documentación en línea con un área de pruebas en el navegador (HyperEZ compilado a WebAssembly).

¿Echas algo en falta? Propónlo en las [incidencias](https://github.com/cuevacaravacaadrian/HyperEZ/issues/new/choose).
