<p align="center">
  <img src="https://raw.githubusercontent.com/cuevacaravacaadrian/HyperEZ/main/assets/branding/banner.png" alt="HyperEZ: programa en español. Juegos, IA, web, datos y más." width="100%">
</p>

<p align="center">
  <a href="https://cuevacaravacaadrian.github.io/HyperEZ/"><img src="https://img.shields.io/badge/web-HyperEZ-62d962" alt="La web de HyperEZ"></a>
  <a href="https://marketplace.visualstudio.com/items?itemName=hyperez.hyperezprogramminglanguagevscodeextension"><img src="https://vsmarketplacebadges.dev/installs-short/hyperez.hyperezprogramminglanguagevscodeextension.svg?label=instalaciones&color=2f81f7" alt="Instalaciones"></a>
  <a href="https://github.com/cuevacaravacaadrian/HyperEZ/releases/latest"><img src="https://img.shields.io/github/v/release/cuevacaravacaadrian/HyperEZ?label=HyperEZ&color=e8572a" alt="Última versión"></a>
  <img src="https://img.shields.io/badge/idioma-espa%C3%B1ol-c8102e" alt="En español">
  <img src="https://img.shields.io/badge/Windows%20%C2%B7%20Linux%20%C2%B7%20macOS-0f1424" alt="Windows, Linux y macOS">
  <a href="https://github.com/cuevacaravacaadrian/HyperEZ/blob/main/LICENSE"><img src="https://img.shields.io/badge/licencia-MIT-2e7dd7" alt="Licencia MIT"></a>
  <a href="https://github.com/cuevacaravacaadrian/HyperEZ"><img src="https://img.shields.io/github/stars/cuevacaravacaadrian/HyperEZ?style=flat&label=estrellas" alt="Estrellas en GitHub"></a>
</p>

# HyperEZ para Visual Studio Code

**HyperEZ** es un lenguaje de programación **en español**, sencillo y rápido, para aprender y para hacer de todo: **juegos, webs, inteligencia artificial, análisis de datos, gráficos, sonido y herramientas**. Esta extensión convierte VS Code en un entorno completo para HyperEZ: colores, errores en vivo, autocompletado, **F5 para ejecutar**, formato, pruebas, consola interactiva, un tema propio y un tutorial.

> En Windows no necesitas instalar nada más: la extensión trae `hyper.exe`. Abre o crea un archivo `.hez` y pulsa **F5**.
>
> ¿Sin instalar nada? Prueba HyperEZ en el **[editor del navegador](https://cuevacaravacaadrian.github.io/HyperEZ/editor/index.html)**. Todo lo demás está en **[la web de HyperEZ](https://cuevacaravacaadrian.github.io/HyperEZ/)**.

<p align="center">
  <img src="https://raw.githubusercontent.com/cuevacaravacaadrian/HyperEZ/main/docs/img/vscode-ejecutar.png" alt="Un programa de HyperEZ en VS Code con el tema HyperEZ Noche, los botones Ejecutar, Repetir al guardar y Comprobar, y su resultado en la terminal" width="92%">
</p>

## 🚀 Empieza en un minuto

1. **Instala** esta extensión (en Windows ya incluye el lenguaje; en Linux y macOS instala HyperEZ desde la [página de descargas](https://github.com/cuevacaravacaadrian/HyperEZ/releases/latest)).
2. **Crea un archivo**: `Ctrl+N` → *HyperEZ: Nuevo archivo HyperEZ*, o guarda cualquier archivo como `hola.hez`.
3. Escribe `print("¡Hola, mundo!")` y pulsa **F5**. El resultado aparece en la terminal.
4. ¿Primera vez? Abre *Ayuda → Introducción* → **Empezar con HyperEZ**: un tutorial en cinco pasos.

## ✨ Qué hace la extensión

| | |
|---|---|
| 🎨 **Colores** | todo el lenguaje, también los bloques ` ```hz ` de Markdown, y el tema oscuro **HyperEZ Noche** |
| 🩺 **Errores en vivo** | mientras escribes, en español y con sugerencias (`prnt` → *¿quisiste decir 'print'?*) |
| 💡 **Arreglos rápidos** | la bombilla corrige los nombres mal escritos con un clic |
| ⌨️ **Autocompletado** | según el tipo: los campos de tus clases, `"texto".`, `lista.`, `Http.`, `Matrix`... |
| 📖 **Documentación al pasar el ratón** | y ayuda de parámetros mientras escribes una llamada |
| 🧭 **Navegación** | ir a la definición (F12), referencias (Mayús+F12), renombrar en todos los archivos (F2), esquema |
| ▶️ **Ejecutar con F5** | en una terminal con colores y teclado (para juegos e `input()`); espera una tecla al terminar. Los archivos sin título **se ejecutan sin guardar** |
| 🔘 **Botones encima del código** | **▶ Ejecutar · ⟳ Repetir al guardar · ✓ Comprobar**, y **▶ Probar** sobre cada `test(...)` |
| 🔁 **Modo watch** | **Ctrl+Alt+F5**: el programa se vuelve a ejecutar cada vez que guardas |
| 📐 **Formato** | **Mayús+Alt+F** ordena la sangría y los espacios (`hyper fmt`); opcional al guardar |
| 🧪 **Pruebas** | `test(...)` en tus archivos y *HyperEZ: Ejecutar pruebas* para todo el proyecto |
| 💬 **Consola interactiva** | escribe código y ve el resultado al momento |
| 🚀 **Proyectos** | juego, aplicación, IA, herramienta, **web** o **sitio**, con las carpetas de HyperEZ: `hypersettings`, `hypercode`, `hypertests`... |
| 📁 **Iconos de HyperEZ** | un tema de iconos con cada carpeta del proyecto (`hypersettings` ⚙, `hypercode` &lt;/&gt;, `hypertests` ✓, `hyperlibs`, `hypertools`, `hyperdata`...) y los archivos `.hez` |
| 🔬 **Bytecode** | *Ver el bytecode del archivo*: las instrucciones que genera el compilador |
| 📝 **HyperEZ Editor** | *Abrir en HyperEZ Editor*: el editor clásico que se instala con HyperEZ (Windows) |
| ✂️ **54 fragmentos** | `clase`, `funcion`, `para`, `hacer`, `segun`, `formato`, `servidor`, `web`, `comprension`, `imagen`, `sonido`, `db`, `regex`... |
| 🌈 **Muestras de color** | para `"#ff8800"` y `Color(255, 136, 0)`, con selector |

<table>
  <tr>
    <td width="50%"><img src="https://raw.githubusercontent.com/cuevacaravacaadrian/HyperEZ/main/docs/img/vscode-errores.png" alt="Un error en vivo: No existe nada llamado prnt, ¿quisiste decir print?, con el arreglo rápido Cambiar por print"></td>
    <td width="50%"><img src="https://raw.githubusercontent.com/cuevacaravacaadrian/HyperEZ/main/docs/img/vscode-autocompletado.png" alt="Autocompletado de los campos de una clase: caro, nombre, precio y tipo"></td>
  </tr>
  <tr>
    <td align="center"><b>Errores en español</b>, con arreglo rápido</td>
    <td align="center"><b>Autocompletado</b> que conoce tus clases</td>
  </tr>
</table>

## 🧩 Así es HyperEZ

```hz
// Clases, comprensiones, tubería |> y textos con ${...}
class Objeto {
    var nombre: text
    var precio: number
    constructor(nombre: text, precio: number) { this.nombre = nombre; this.precio = precio }
}

function conDescuento(precio: number, porcentaje: number): number {
    return round(precio * (1 - porcentaje / 100))
}

var tienda = [Objeto("Espada", 120), Objeto("Poción", 15)]
var rebajas = {[o.nombre]: o.precio |> conDescuento(20) for o in tienda}
foreach nombre, precio in rebajas { print("${nombre}: ${precio} monedas") }
```

Con la biblioteca incluida se hace de todo sin instalar paquetes:

```hz
import web                                     // una web con rutas, plantillas y formularios
var app = web.App()
app.get("/hola/:nombre", req => web.html("<h1>¡Hola, ${req.params.nombre}!</h1>"))
print(app.handle(web.request("GET", "/hola/Ana")).body)
// app.listen(8080, { open: true }) la sirve de verdad y abre el navegador
```

```hz
import ai                                      // inteligencia artificial: una red neuronal aprende XOR
var red = ai.NeuralNetwork([2, 6, 1], "tanh", "sigmoid")
red.train([[0, 0], [0, 1], [1, 0], [1, 1]], [[0], [1], [1], [0]], 2000, 0.5)
print(red.predict([1, 0]).get(0, 0))           // ≈ 1
```

**Qué trae el lenguaje:** tipos opcionales, clases, structs, interfaces, enums, `match`, errores con `try`/`catch`, eventos, `async`/`await`, módulos, desestructuración, comprensiones, tubería `|>`, propagación `{...mapa}`, trozos `lista[2..]` y sobrecarga de operadores. **La biblioteca:** archivos y procesos, HTTP (cliente y servidor), webs y sitios desde Markdown, JSON y CSV, base de datos, expresiones regulares, compresión y `.zip`, matrices y redes neuronales, imágenes PNG/JPG, gráficas SVG, sonido WAV, plantillas, grafos y caminos con A*, consola para juegos.

## ⌨️ Atajos y comandos

| Comando | Atajo |
|---|---|
| HyperEZ: Ejecutar archivo | **F5** / Ctrl+F5 |
| HyperEZ: Ejecutar y repetir al guardar (watch) | Ctrl+Alt+F5 |
| HyperEZ: Comprobar errores | Ctrl+Mayús+F5 |
| HyperEZ: Dar formato al archivo | Mayús+Alt+F |
| HyperEZ: Abrir consola interactiva · Ejecutar pruebas · Ejecutar las pruebas de este archivo | |
| HyperEZ: Nuevo proyecto · Nuevo archivo HyperEZ · Usar los iconos de HyperEZ | |
| HyperEZ: Ver el bytecode del archivo · Abrir en HyperEZ Editor | |
| HyperEZ: Empezar con HyperEZ (tutorial) · Abrir la guía · la referencia de la API · los ejemplos | |

## 🔧 Ajustes

| Ajuste | Para qué |
|---|---|
| `hyperez.path` | ruta de `hyper` (si no, se usa el instalado, el del `PATH` o el que trae la extensión) |
| `hyperez.languageServer.enabled` | errores en vivo, autocompletado, documentación y navegación |
| `hyperez.run.saveBeforeRun` | guardar el archivo antes de ejecutarlo |
| `hyperez.run.reuseTerminal` | reutilizar la terminal de ejecución |
| `hyperez.codeLens` | botones ▶ Ejecutar / ⟳ / ✓ encima del código |
| `hyperez.formatOnSave` | dar formato al guardar |
| `hyperez.colorDecorators` | muestras de color |

## 📁 Las carpetas de un proyecto

*HyperEZ: Nuevo proyecto* (o `hyper new`) crea un proyecto con carpetas propias del lenguaje, cada una con su icono:

| Carpeta | Para qué |
|---|---|
| `hypersettings/` | `proyecto.json`: nombre, versión y programa principal (`hyper run` sin archivo lo ejecuta) |
| `hypercode/` | los módulos: se importan por su nombre, `from tienda import Carrito` |
| `hypertests/` | las pruebas: `hyper test` sin carpeta las ejecuta |
| `hyperlibs/` | librerías de otros, también por su nombre |
| `hypertools/` | herramientas del proyecto: `hyper tool <nombre>` |
| `hyperdata/` · `hyperassets/` · `hyperdocs/` | datos, imágenes y sonidos, documentación (`System.projectDir` da la raíz) |

## 💻 Más allá de VS Code

- **[La web de HyperEZ](https://cuevacaravacaadrian.github.io/HyperEZ/)**: documentación, descargas, ejemplos y el **editor en el navegador** (sin instalar nada).
- **Instalador para Windows** con el lenguaje, **HyperEZ Editor** (un editor clásico con pestañas, panel de salida e impresión), ejemplos y la documentación en HTML: [descargar](https://github.com/cuevacaravacaadrian/HyperEZ/releases/latest).
- **Linux y macOS**: un paquete con `sh install.sh`.
- **Otros editores**: colores para Notepad++, Sublime Text, Vim/Neovim y highlight.js, y `hyper lsp` para cualquier editor con servidor de lenguaje.

## 📚 Aprende

- [Guía del lenguaje](https://github.com/cuevacaravacaadrian/HyperEZ/blob/main/docs/GUIA.md) · [Tutorial: tu primer juego](https://github.com/cuevacaravacaadrian/HyperEZ/blob/main/docs/empezar/tutorial-juego.md) · [Referencia de la API](https://github.com/cuevacaravacaadrian/HyperEZ/blob/main/docs/biblioteca/API.md)
- [Ejemplos](https://github.com/cuevacaravacaadrian/HyperEZ/tree/main/examples): juegos, IA, webs, datos, gráficos, sonido y sistema
- [Preguntas frecuentes](https://github.com/cuevacaravacaadrian/HyperEZ/blob/main/docs/preguntas.md) · [Novedades](https://github.com/cuevacaravacaadrian/HyperEZ/blob/main/CHANGELOG.md)

## 🤝 Participa

¿Has encontrado un error o tienes una idea? [Ábrela en GitHub](https://github.com/cuevacaravacaadrian/HyperEZ/issues). Si haces algo con HyperEZ, **súbelo a GitHub con archivos `.hez`**: así ayudas a que GitHub reconozca HyperEZ como lenguaje.

Licencia MIT © 2026 Adrián Cueva Caravaca.
