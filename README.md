<p align="center">
  <img src="assets/icons/hyperez-256.png" width="120" alt="Logotipo de HyperEZ">
</p>

<h1 align="center">HyperEZ</h1>

<p align="center">
  Un lenguaje de programación sencillo, con mensajes en español, para aprender a programar y crear juegos.
</p>

<p align="center">
  <a href="https://github.com/cuevacaravacaadrian-ux/HyperEZ/actions/workflows/ci.yml"><img src="https://github.com/cuevacaravacaadrian-ux/HyperEZ/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/cuevacaravacaadrian-ux/HyperEZ/releases/latest"><img src="https://img.shields.io/github/v/release/cuevacaravacaadrian-ux/HyperEZ?label=descargar" alt="Última versión"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/licencia-MIT-blue" alt="Licencia MIT"></a>
</p>

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

Los programas se guardan en archivos `.hz` (o `.hez`). Si te equivocas, HyperEZ te lo dice en español y te sugiere cómo arreglarlo:

```text
error[E0205]: No existe nada llamado 'pirnt'
  --> juego.hz:3:1
 3 | pirnt("¡Hola!")
   | ^^^^^
   = ayuda: ¿quisiste decir 'print'?
```

## Qué incluye

- **Un lenguaje completo**: variables con tipos opcionales, funciones, clases, structs, enums, interfaces, `match`, errores con `try`/`catch`, eventos (`event`/`on`/`emit`), tareas asíncronas (`async`/`await`), módulos y pruebas integradas.
- **Biblioteca para juegos de consola**: teclas sin pulsar Intro, colores, cursor, vectores (`Vec2`, `Vec3`), colores, números aleatorios, archivos y JSON para guardar partidas.
- **Resultados al instante**: consola interactiva, modo *watch* que vuelve a ejecutar al guardar y **F5** en VS Code.
- **Extensión de Visual Studio Code**: errores mientras escribes, autocompletado según el tipo, documentación al pasar el ratón, ir a la definición, renombrar, arreglos rápidos y fragmentos en español.
- **Instalador para Windows**: asocia `.hz` y `.hez` con su icono (doble clic para ejecutar), añade `hyper` al `PATH` e instala la extensión de VS Code. No necesita permisos de administrador.

## Instalar

1. Descarga **`HyperEZ-Setup-0.5.0.exe`** de la [última versión](https://github.com/cuevacaravacaadrian-ux/HyperEZ/releases/latest).
2. Ejecútalo y pulsa **Instalar**. Si Windows SmartScreen avisa ("Windows protegió su PC"), pulsa *Más información → Ejecutar de todas formas*: el instalador no está firmado.
3. Abre una terminal nueva y comprueba que funciona: `hyper version`.

El instalador copia HyperEZ en `%LOCALAPPDATA%\Programs\HyperEZ`, asocia `.hz`/`.hez`, añade `hyper` al `PATH`, crea accesos en el menú Inicio e instala la extensión si tienes VS Code. Se desinstala desde *Configuración → Aplicaciones*. Para una instalación silenciosa: `HyperEZ-Setup-0.5.0.exe /S` (opciones `/D=carpeta`, `/NOASSOC`, `/NOPATH`, `/NOSHORTCUTS`, `/NOVSCODE`).

## Empezar en un minuto

**Con VS Code** (lo más cómodo): crea `hola.hz`, escribe `print("¡Hola!")` y pulsa **F5**. Con **Ctrl+Alt+F5** el programa se vuelve a ejecutar cada vez que guardas.

**Con la consola interactiva**: escribe `hyper` en una terminal (o abre *HyperEZ (consola interactiva)* en el menú Inicio) y prueba código línea a línea. Lo que defines se conserva:

```text
hz> var vidas = 3
hz> vidas * 10
30
hz> function doble(n: number): number { return n * 2 }
hz> doble(vidas)
6
hz> .guardar prueba.hz
✓ Guardado en prueba.hz (ejecútalo con: hyper prueba.hz)
```

**Con un proyecto**:

```text
hyper new mi-juego      crea un proyecto con un juego de ejemplo y sus pruebas
cd mi-juego
hyper watch             ejecuta main.hz y lo repite cada vez que guardas
hyper test              ejecuta las pruebas de tests/
```

| Comando | Qué hace |
|---|---|
| `hyper` | Consola interactiva (también `hyper repl`) |
| `hyper archivo.hz [args]` | Ejecuta un programa (`System.args` recibe los argumentos) |
| `hyper watch [archivo.hz]` | Ejecuta y vuelve a ejecutar cada vez que guardas un `.hz` |
| `hyper check <archivos o carpetas>` | Busca errores sin ejecutar |
| `hyper test [carpeta]` | Ejecuta las pruebas; si hay un `.out` al lado, compara la salida |
| `hyper new <nombre>` | Crea un proyecto |
| `hyper docs [salida.md]` | Genera la referencia completa de la API |
| `hyper lsp` | Servidor de lenguaje para editores |
| `hyper register` / `unregister` | Asocia (o desasocia) `.hz`/`.hez` con ese `hyper.exe` |

## Aprender

- [Guía del lenguaje](docs/GUIA.md): recorrido completo con ejemplos que puedes copiar.
- [Ejemplos](examples/): serpiente, tres en raya con una IA que nunca pierde, una mazmorra por turnos y adivina el número.
- `hyper docs API.md`: referencia de todas las funciones, clases y módulos.

## Compilar desde el código

Requisitos: Windows, [MSYS2](https://www.msys2.org/) con GCC, CMake y Ninja de UCRT64, Python 3 y Node.js 20+ (para la extensión).

```text
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

```powershell
./build.ps1            # compila (con PGO), pasa las pruebas, empaqueta la extensión y crea el instalador
./build.ps1 -NoPGO     # compilación más rápida, sin optimización guiada por perfil
./build.ps1 -SkipVsix  # sin la extensión de VS Code
```

El resultado queda en `dist/`. Paso a paso, sin el script:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build                 # hyper.exe y el instalador
ctest --test-dir build              # pruebas
```

## Estructura

| Carpeta | Contenido |
|---|---|
| `src/core` | Utilidades comunes (texto, UTF-8, archivos, consola) |
| `src/lang` | Lexer, parser, analizador (tipos graduales), generador de bytecode, VM con GC, fibras async y eventos, biblioteca estándar |
| `src/tools` | La herramienta `hyper`: CLI, consola interactiva, modo watch, servidor de lenguaje, integración con Windows |
| `editors/vscode` | Extensión de VS Code |
| `packaging` | Iconos e instalador de Windows |
| `tests` | Pruebas del lenguaje (`hyper test tests`) y benchmarks |
| `examples` | Ejemplos |
| `tools` | Scripts de mantenimiento (versión, documentación, usuario de GitHub) |

## Contribuir

Las ideas, los avisos de errores y los cambios son bienvenidos. Lee [CONTRIBUTING.md](CONTRIBUTING.md) para compilar, probar y enviar cambios.

## Licencia

[MIT](LICENSE) © 2026 Adrián Cueva Caravaca. Las bibliotecas de `third_party/` mantienen sus propias licencias.
