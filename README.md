<p align="center">
  <img src="assets/branding/banner.png" alt="HyperEZ: programa en español. Juegos, IA, web, datos y más." width="100%">
</p>

<p align="center">
  <a href="https://github.com/cuevacaravacaadrian-ux/HyperEZ/actions/workflows/ci.yml"><img src="https://github.com/cuevacaravacaadrian-ux/HyperEZ/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/cuevacaravacaadrian-ux/HyperEZ/releases/latest"><img src="https://img.shields.io/github/v/release/cuevacaravacaadrian-ux/HyperEZ?label=descargar&color=2f81f7" alt="Última versión"></a>
  <img src="https://img.shields.io/badge/Windows%20%C2%B7%20Linux%20%C2%B7%20macOS-0f1424" alt="Windows, Linux y macOS">
  <a href="LICENSE"><img src="https://img.shields.io/badge/licencia-MIT-7ee787" alt="Licencia MIT"></a>
</p>

<p align="center">
  <a href="docs/empezar/instalacion.md"><b>Instalar</b></a> ·
  <a href="docs/empezar/primeros-pasos.md"><b>Primeros pasos</b></a> ·
  <a href="docs/GUIA.md"><b>Guía</b></a> ·
  <a href="docs/README.md"><b>Documentación</b></a> ·
  <a href="examples"><b>Ejemplos</b></a> ·
  <a href="docs/hoja-de-ruta.md"><b>Hoja de ruta</b></a>
</p>

**HyperEZ** es un lenguaje de programación con mensajes en español, pensado para aprender a programar y para hacer de todo: juegos, inteligencia artificial, webs, análisis de datos, gráficos, sonido, scripts del sistema y herramientas de bajo nivel. Todo cabe en un único ejecutable, `hyper`, sin nada más que instalar.

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

Si te equivocas, HyperEZ te lo dice en español antes de ejecutar y te propone el arreglo:

```text
error[E0205]: No existe nada llamado 'pirnt'
  --> juego.hz:3:1
 3 | pirnt("¡Hola!")
   | ^^^^^
   = ayuda: ¿quisiste decir 'print'?
```

## 🚀 Instalar

| Sistema | Cómo |
|---|---|
| **Windows** | Descarga y ejecuta **`HyperEZ-Setup-0.6.0.exe`** de la [última versión](https://github.com/cuevacaravacaadrian-ux/HyperEZ/releases/latest). No necesita administrador. |
| **Linux / macOS** | Descarga `HyperEZ-0.6.0-linux-x64.tar.gz` (o `macos-arm64`), descomprímelo y ejecuta `sh install.sh`. |
| **VS Code** | Lo instala el instalador de Windows; también `code --install-extension hyperez-0.6.0.vsix`. |

En Windows, los archivos `.hz` y `.hez` se abren **siempre** con HyperEZ: doble clic para ejecutarlos y, con el clic derecho, *Editar con VS Code*, *Comprobar errores*, *Ejecutar y repetir al guardar* o *Abrir HyperEZ aquí*. Más detalles en la [guía de instalación](docs/empezar/instalacion.md).

## ⚡ Resultados al instante

| Forma | Cómo |
|---|---|
| **Consola interactiva** | escribe `hyper`: cada línea se ejecuta al pulsar Intro y lo que defines se conserva |
| **Modo watch** | `hyper watch juego.hz` (o **Ctrl+Alt+F5** en VS Code) vuelve a ejecutar cada vez que guardas |
| **VS Code** | **F5** ejecuta; errores, autocompletado y documentación mientras escribes |
| **Proyectos** | `hyper new mi-juego --tipo juego\|app\|ia\|script` crea una carpeta ordenada que funciona desde el primer momento |

```text
hz> var precios = [3.5, 12, 7.25]
hz> precios.sum()
22.75
hz> precios.map(p => round(p * 1.21, 2))
[4.24, 14.52, 8.77]
```

## 🧰 Qué se puede hacer

<table>
<tr><td width="50%" valign="top">

**🎮 Juegos de consola**
```hz
var pos = Vec2(0, 0)
foreach tecla in ["right", "right", "down"] {
    pos = pos + match tecla {
        "right" => Vec2(1, 0)
        "down" => Vec2(0, 1)
        else => Vec2(0, 0)
    }
}
print("El héroe está en", pos)
```

</td><td width="50%" valign="top">

**🧠 Inteligencia artificial**
```hz
import ai
randomSeed(1)
var red = ai.NeuralNetwork([2, 8, 1], "tanh")
var X = [[0, 0], [0, 1], [1, 0], [1, 1]]
red.train(X, [[0], [1], [1], [0]], 2000, 0.5)
print("XOR aprendido:", red.classify(X))
```

</td></tr>
<tr><td valign="top">

**🌐 Webs y APIs**
```hz
import web
var app = web.App()
app.get("/hola/:nombre", req => "Hola, " + req.params.nombre)
print(app.handle(web.request("GET", "/hola/Ana")))
// app.listen(8080) para servirla de verdad
```

</td><td valign="top">

**📊 Datos y estadística**
```hz
import csv
import stats
var filas = csv.parse("mes,ventas\nene,120\nfeb,95\nmar,160")
var ventas = filas.map(f => f.ventas)
print(stats.mean(ventas), stats.median(ventas), stats.stdev(ventas))
```

</td></tr>
<tr><td valign="top">

**🖼️ Gráficos y 🔊 sonido**
```hz
import audio
var im = Image(200, 100, "#0f1424")
im.circle(100, 50, 30, "#2f81f7")
im.text(10, 75, "HyperEZ", "white", 16)
im.save(Path.join(Path.temp, "logo.png"))
audio.Sound().melody("C4 E4 G4", 0.2).save(Path.join(Path.temp, "do-mi-sol.wav"))
print("imagen y sonido creados")
```

</td><td valign="top">

**⚙️ Sistema y bajo nivel**
```hz
var r = Process.run(System.exe, ["version"])
print(r.output.lines()[0])
var b = Bytes("HZS1").pushU16(2).pushU32(15)
print(b.toHex(" "), Hash.crc32(b))
print((255).toHex(), Regex(r"\d+").findAll("1 22 333").length)
```

</td></tr>
</table>

Y además: archivos y rutas, fechas, JSON, base de datos de documentos, expresiones regulares, compresión, peticiones HTTPS, servidor web, matrices en C++, ensamblador x86 para sectores de arranque, pruebas integradas, eventos y tareas `async`/`await`. Mira el [mapa de la biblioteca](docs/biblioteca/README.md) y los [ejemplos por áreas](examples).

## ✨ El lenguaje

- **Tipos opcionales** (tipado gradual): escríbelos cuando ayuden; si no, HyperEZ deduce muchos solo.
- **Clases, structs, interfaces y enums**, herencia, propiedades `get`/`set`, sobrecarga de operadores.
- **`match`** con valores, rangos, guardas y bloques.
- **Funciones** con valores por defecto, argumentos con nombre, `...resto`, lambdas y clausuras.
- **Desestructuración**: `var [a, b] = lista`, `var {nombre, vida} = jugador`.
- **Errores** con `try`/`catch`/`finally` y tipos de error propios.
- **Eventos** (`event`, `on`, `emit`) y **tareas asíncronas** (`async`, `await`, `spawn`).
- **Módulos** con `import` y bibliotecas incluidas.

Toda la sintaxis está en la [referencia del lenguaje](docs/lenguaje/README.md), con su [gramática](docs/lenguaje/gramatica.md).

## 🛠️ Herramientas

| Orden | Qué hace |
|---|---|
| `hyper` | consola interactiva |
| `hyper archivo.hz` | ejecuta un programa |
| `hyper watch [archivo.hz]` | ejecuta y repite al guardar |
| `hyper check` | busca errores sin ejecutar |
| `hyper test` | ejecuta las pruebas (`test(...)` y salidas esperadas `.out`) |
| `hyper new <nombre> --tipo T` | crea un proyecto |
| `hyper docs` | genera la referencia de la API |
| `hyper lsp` | servidor de lenguaje para cualquier editor |

La **extensión de VS Code** trae errores en vivo, autocompletado según el tipo, documentación al pasar el ratón, ir a la definición, renombrar, arreglos rápidos, 40 fragmentos, el tema *HyperEZ Noche* y un tutorial de bienvenida. Ver [VS Code](docs/herramientas/vscode.md).

## 🏎️ Rendimiento

Bytecode en una máquina virtual optimizada: cachés en línea, instrucciones fusionadas, llamadas directas, asignador de objetos propio y compilación con LTO y PGO. Lo pesado (matrices, imágenes, expresiones regulares, compresión, HTTP) va en C++. Los números están en [rendimiento](docs/interno/rendimiento.md).

## 🧑‍💻 Compilar desde el código

```powershell
./build.ps1              # Windows: compila (con PGO), pasa las pruebas, empaqueta la extensión y crea el instalador
```

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release     # Linux y macOS
cmake --build build && ctest --test-dir build
```

Requisitos y detalles en [CONTRIBUTING.md](CONTRIBUTING.md). Cómo está hecho por dentro: [arquitectura](docs/interno/arquitectura.md).

## 📁 Estructura del repositorio

| Carpeta | Contenido |
|---|---|
| [`src/core`](src/core) | utilidades comunes: texto, UTF-8, archivos, consola, procesos |
| [`src/lang`](src/lang) | el lenguaje: `syntax` (lexer, parser), `sema` (analizador), `compiler` (bytecode), `vm` (máquina virtual), `stdlib` (biblioteca nativa), `prelude` |
| [`src/lib`](src/lib) | bibliotecas escritas en HyperEZ: `ai`, `stats`, `csv`, `cli`, `web`, `html`, `db`, `audio`, `x86` |
| [`src/tools`](src/tools) | la orden `hyper`: CLI, consola interactiva, watch, servidor de lenguaje, integración con el sistema |
| [`editors/vscode`](editors/vscode) | extensión de VS Code |
| [`packaging`](packaging) | instalador de Windows, paquetes de Linux y macOS, iconos |
| [`examples`](examples) | ejemplos por áreas: juegos, IA, datos, web, gráficos, audio, sistema, sistemas operativos... |
| [`tests`](tests) | pruebas del lenguaje, las bibliotecas, la consola y los errores |
| [`docs`](docs) | documentación completa |
| [`tools`](tools) | scripts de mantenimiento |

## 🤝 Contribuir

Las ideas, los avisos de errores y los cambios son bienvenidos: lee [CONTRIBUTING.md](CONTRIBUTING.md) y el [código de conducta](CODE_OF_CONDUCT.md). Para dudas, [SUPPORT.md](SUPPORT.md).

## 📄 Licencia

[MIT](LICENSE) © 2026 Adrián Cueva Caravaca. Las bibliotecas de `third_party/` y la fuente DejaVu mantienen sus propias licencias.
