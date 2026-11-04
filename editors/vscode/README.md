# HyperEZ para Visual Studio Code

**HyperEZ** es un lenguaje de programación en español para aprender y para hacer de todo: juegos, inteligencia artificial, webs, análisis de datos, gráficos, sonido y herramientas de sistema. Esta extensión lo convierte en un entorno completo dentro de VS Code.

```hz
class Jugador {
    var nombre: text
    var vida = 3
    constructor(nombre: text) { this.nombre = nombre }
    get vivo(): bool { return vida > 0 }
}

var ana = Jugador("Ana")
print("¡Hola, ${ana.nombre}! Vidas: ${ana.vida}")
```

## ✨ Funciones

| | |
|---|---|
| 🎨 **Resaltado** | de todo el lenguaje, también en los bloques ` ```hz ` de Markdown, y el tema **HyperEZ Noche** |
| 🩺 **Errores en vivo** | mientras escribes, en español y con sugerencias (`pirnt` → *¿quisiste decir print?*) |
| 💡 **Arreglos rápidos** | la bombilla corrige nombres mal escritos con un clic |
| ⌨️ **Autocompletado** | según el tipo: `jugador.`, `"texto".`, `Http.`, `Matrix`... |
| 📖 **Documentación** | al pasar el ratón y ayuda de parámetros al escribir una llamada |
| 🧭 **Navegación** | ir a la definición (F12), referencias (Mayús+F12), renombrar en todos los archivos (F2), esquema |
| ▶️ **Ejecutar con F5** | en una terminal con colores y teclado; un archivo nuevo se guarda como `.hez` |
| 🔘 **Botones encima del código** | **▶ Ejecutar · ⟳ Repetir al guardar · ✓ Comprobar**, y **▶ Probar** sobre cada `test(...)` |
| 📐 **Formato** | **Mayús+Alt+F** ordena la sangría y los espacios (`hyper fmt`); opcional al guardar |
| 🔬 **Bytecode** | *Ver el bytecode del archivo*: las instrucciones que genera el compilador |
| 📝 **HyperEZ Editor** | *Abrir en HyperEZ Editor*: el editor clásico que se instala con HyperEZ (Windows) |
| 🔁 **Modo watch** | **Ctrl+Alt+F5**: se vuelve a ejecutar cada vez que guardas |
| 💬 **Consola interactiva** | escribe código y ve el resultado al momento |
| 🧪 **Pruebas** | `test(...)` + *HyperEZ: Ejecutar pruebas* |
| 🚀 **Proyectos** | juego, aplicación, IA o herramienta, con carpetas ordenadas |
| 🎓 **Tutorial** | *Empezar con HyperEZ* en la página de bienvenida |
| ✂️ **Fragmentos** | `clase`, `funcion`, `para`, `segun`, `servidor`, `red`, `imagen`, `sonido`, `db`, `regex`... |
| 🌈 **Muestras de color** | para `"#ff8800"` y `Color(255, 136, 0)` con selector |

## ⌨️ Atajos y comandos

| Comando | Atajo |
|---|---|
| HyperEZ: Ejecutar archivo | F5 / Ctrl+F5 |
| HyperEZ: Ejecutar y repetir al guardar (watch) | Ctrl+Alt+F5 |
| HyperEZ: Comprobar errores | Ctrl+Mayús+F5 |
| HyperEZ: Dar formato al archivo | Mayús+Alt+F |
| HyperEZ: Ver el bytecode del archivo · Abrir en HyperEZ Editor · Ejecutar las pruebas de este archivo | |
| HyperEZ: Abrir consola interactiva | |
| HyperEZ: Ejecutar pruebas | |
| HyperEZ: Nuevo proyecto · Nuevo archivo | |
| HyperEZ: Empezar con HyperEZ (tutorial) | |
| HyperEZ: Abrir la guía del lenguaje · la referencia de la API · los ejemplos | |

## 📦 Qué trae el lenguaje

- **Sintaxis clara**: tipos opcionales, clases, `match`, eventos, `async`/`await`, desestructuración, sobrecarga de operadores.
- **Biblioteca para todo**: archivos y procesos, HTTP (cliente y servidor), JSON y CSV, base de datos, expresiones regulares, `Bytes`, compresión y `.zip`, matrices (con inversa y sistemas de ecuaciones) y redes neuronales, imágenes PNG/JPG, gráficas SVG, sonido WAV, plantillas, Markdown, colas, grafos y caminos con A*, consola para juegos.
- **Herramientas**: HyperEZ Editor, consola interactiva, modo watch, formateador, pruebas con salidas esperadas, instaladores para Windows, Linux y macOS.

## ⚙️ Requisitos

La extensión incluye `hyper.exe` para Windows. Si instalas HyperEZ con su instalador o tienes `hyper` en el `PATH` (Linux y macOS), se usa esa versión; también puedes indicar una ruta en `hyperez.path`.

## 🔧 Ajustes

| Ajuste | |
|---|---|
| `hyperez.path` | ruta de `hyper` |
| `hyperez.languageServer.enabled` | errores en vivo, autocompletado y demás |
| `hyperez.run.saveBeforeRun` | guardar antes de ejecutar |
| `hyperez.run.reuseTerminal` | reutilizar la terminal de ejecución |
| `hyperez.colorDecorators` | muestras de color |
| `hyperez.codeLens` | botones ▶ Ejecutar / ⟳ / ✓ encima del código |
| `hyperez.formatOnSave` | dar formato al guardar |

## 🔗 Enlaces

- [Código, documentación y descargas](https://github.com/cuevacaravacaadrian-ux/HyperEZ)
- [Informar de un error o proponer una idea](https://github.com/cuevacaravacaadrian-ux/HyperEZ/issues)

Licencia MIT © 2026 Adrián Cueva Caravaca.
