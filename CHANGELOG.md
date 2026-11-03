# Cambios

Todas las versiones de HyperEZ. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

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
