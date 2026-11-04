# Instalación

## Windows

1. Descarga `HyperEZ-Setup-<versión>.exe` de la [página de versiones](https://github.com/cuevacaravacaadrian-ux/HyperEZ/releases/latest).
2. Ejecútalo y pulsa **Instalar**. No necesita permisos de administrador. Si aparece "Windows protegió su PC" (el instalador no está firmado), pulsa *Más información → Ejecutar de todas formas*.
3. Abre una terminal **nueva** y escribe `hyper version`.

El instalador:

- copia HyperEZ en `%LOCALAPPDATA%\Programs\HyperEZ`;
- hace que **todos los archivos `.hz` y `.hez`** se abran siempre con HyperEZ (con su icono). Si antes habías elegido otro programa para ellos, deja de tener prioridad;
- añade al menú contextual de los `.hz`: *Editar con Visual Studio Code*, *Comprobar errores* y *Ejecutar y repetir al guardar*; y al fondo de cualquier carpeta, *Abrir HyperEZ aquí* (consola interactiva);
- añade `hyper` al `PATH`;
- crea accesos en el menú Inicio: consola interactiva, terminal, ejemplos, guía, referencia de la API y desinstalar;
- instala la extensión de VS Code si tienes VS Code.

Se desinstala desde *Configuración → Aplicaciones → HyperEZ* (o con el acceso del menú Inicio).

### Instalación silenciosa

```text
HyperEZ-Setup-0.6.0.exe /S                  sin preguntas
HyperEZ-Setup-0.6.0.exe /S /D=C:\HyperEZ    en otra carpeta
```

Opciones: `/NOASSOC` (no asociar archivos), `/NOPATH`, `/NOSHORTCUTS`, `/NOVSCODE`.

## Linux y macOS

1. Descarga `HyperEZ-<versión>-linux-x64.tar.gz` (o `-macos-arm64`) de la página de versiones.
2. Descomprímelo e instala para tu usuario (sin `sudo`):

```text
tar xzf HyperEZ-0.6.0-linux-x64.tar.gz
cd HyperEZ-0.6.0-linux-x64
sh install.sh
```

Queda en `~/.local/bin/hyper`. En Linux, `install.sh` además asocia los `.hz` y `.hez` en el escritorio (tipo MIME `text/x-hyperez`) para abrirlos con doble clic. Para desinstalar: `sh install.sh --uninstall`.

En cualquier sistema, `hyper register` asocia los archivos con el `hyper` que lo ejecuta y `hyper unregister` lo deshace.

## La extensión de Visual Studio Code

La instala el instalador de Windows. También se puede instalar a mano: en VS Code, *Extensiones → ⋯ → Instalar desde VSIX…* y elige `hyperez-<versión>.vsix`, o desde la terminal:

```text
code --install-extension hyperez-0.6.0.vsix
```

## Desde el código fuente

Mira [CONTRIBUTING.md](../../CONTRIBUTING.md): en Windows basta con `./build.ps1`; en Linux y macOS, CMake y un compilador de C++20:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```
