# Hoja de ruta

HyperEZ quiere ser un lenguaje **universal y en español**: fácil para aprender y lo bastante potente para cualquier proyecto, desde un juego hasta una IA o las herramientas de un sistema operativo. Esto es lo que ya hay y lo que viene.

## Ya disponible (0.6)

| Área | Qué hay |
|---|---|
| Lenguaje | tipado gradual, clases, structs, interfaces, enums, `match`, errores, eventos, `async`/`await`, módulos, desestructuración, sobrecarga de operadores, textos `r"..."` |
| Herramientas | consola interactiva, modo watch, pruebas con salidas esperadas, servidor de lenguaje, extensión de VS Code, instaladores para Windows, Linux y macOS |
| Sistema | procesos (también en segundo plano), archivos, rutas, fechas, variables de entorno |
| Datos | JSON, CSV, base de datos de documentos, estadística |
| IA | matrices en C++, redes neuronales, regresión, k vecinos, k-medias |
| Web | cliente HTTP/HTTPS, servidor web, rutas, HTML |
| Gráficos y sonido | imágenes PNG/JPG con texto, sonidos WAV |
| Bajo nivel | `Bytes`, bits, hash, compresión, ensamblador x86 para sectores de arranque |

## Próximos pasos

**Lenguaje**
- Genéricos en las clases propias (`class Pila<T>`).
- Iteradores y generadores (`yield`) para recorrer estructuras propias con `foreach`.
- `defer` para liberar recursos al salir de una función.

**Rendimiento**
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
- Documentación en línea con un área de pruebas en el navegador (HyperEZ compilado a WebAssembly).

¿Echas algo en falta? Propónlo en las [incidencias](https://github.com/cuevacaravacaadrian-ux/HyperEZ/issues/new/choose).
