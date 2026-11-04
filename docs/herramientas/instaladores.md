# Instaladores y asociación de archivos

## Windows: `HyperEZ-Setup.exe`

Programa propio (`packaging/windows/setup.cpp`) con los archivos dentro: no necesita nada externo ni permisos de administrador. Instala por usuario en `%LOCALAPPDATA%\Programs\HyperEZ`:

```text
bin\hyper.exe          el lenguaje y todas sus herramientas
bin\hyperez-editor.exe HyperEZ Editor
docs\                  documentación (incluida la referencia de la API)
examples\              ejemplos
icons\                 iconos de los archivos .hz
vscode\*.vsix          la extensión de VS Code
uninstall.exe          desinstalador (y uninstall.lst con lo instalado)
```

### Qué toca en el registro

Todo en `HKEY_CURRENT_USER` (solo afecta a tu usuario):

| Clave | Para qué |
|---|---|
| `Software\Classes\.hz`, `.hez` | extensiones → tipo `HyperEZ.Source`, `text/x-hyperez`; *Nuevo → Programa HyperEZ* (solo `.hez`) |
| `Software\Classes\HyperEZ.Source` | icono y acciones: *Ejecutar* (doble clic), *Editar con HyperEZ Editor*, *Editar con Visual Studio Code*, *Comprobar errores*, *Ejecutar y repetir al guardar* |
| `Software\Classes\HyperEZ.Editor`, `Applications\hyperez-editor.exe` | HyperEZ Editor en *Abrir con...* |
| `Software\Classes\Directory\Background\shell\HyperEZ` | *Abrir HyperEZ aquí* |
| `Software\Classes\Applications\hyper.exe` | HyperEZ en *Abrir con...* |
| `...\Explorer\FileExts\.hz\UserChoice` | se borra: si antes elegiste otro programa para `.hz`, HyperEZ vuelve a ser el predeterminado |
| `Environment\Path` | la carpeta `bin` de HyperEZ |
| `Software\Microsoft\Windows\CurrentVersion\Uninstall\HyperEZ` | aparece en *Aplicaciones* |

El doble clic ejecuta `hyper.exe run --pause "archivo"`: la ventana espera una tecla al terminar para que se pueda leer el resultado.

Además crea `Documentos\HyperEZ` (la carpeta donde HyperEZ Editor guarda los programas nuevos) y los accesos directos del menú Inicio y del escritorio. Al desinstalar, esa carpeta solo se borra si está vacía: **tus programas nunca se borran**.

### Opciones

```text
/S                 silencioso
/D=carpeta         carpeta de instalación
/NOASSOC           no asociar .hz/.hez
/NOPATH            no tocar el PATH
/NOSHORTCUTS       sin accesos en el menú Inicio
/NODESKTOP         sin acceso directo en el escritorio
/NOVSCODE          no instalar la extensión de VS Code
/OPENEDITOR        abrir HyperEZ Editor al terminar (el asistente lo hace por defecto)
```

### Crear el instalador

`./build.ps1` lo compila todo y deja en `dist/` el instalador, la extensión y `hyper.exe`. El instalador empaqueta lo que hay en `build/stage` (ver `packaging/windows/stage.cmake`).

## Linux y macOS: `install.sh`

El paquete `HyperEZ-<versión>-<sistema>-<arquitectura>.tar.gz` (lo crea `packaging/unix/package.sh`, y GitHub Actions en cada versión) contiene `bin/hyper`, `share/hyperez/` (ejemplos, documentación, icono) e `install.sh`.

`sh install.sh` instala en `~/.local` y en Linux ejecuta `hyper register`, que:

- registra el tipo MIME `text/x-hyperez` para `*.hz` y `*.hez` (`~/.local/share/mime/packages/hyperez.xml`);
- crea el lanzador `~/.local/share/applications/hyperez.desktop` (ejecuta en una terminal y espera al terminar);
- copia el icono y lo marca como aplicación predeterminada con `xdg-mime`.

`sh install.sh --uninstall` lo deshace todo.
