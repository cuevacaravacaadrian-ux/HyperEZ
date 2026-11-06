# Visual Studio Code

La extensión de HyperEZ convierte VS Code en un entorno completo para el lenguaje. La instala el instalador de Windows; también está en `dist/hyperez-<versión>.vsix` y se publica en el Marketplace.

## Funciones

| Función | Cómo |
|---|---|
| Resaltado de sintaxis | automático en `.hz`/`.hez` y en bloques ` ```hz ` de Markdown |
| Errores mientras escribes | subrayados; la lista en *Problemas* (Ctrl+Mayús+M) |
| Arreglos rápidos | bombilla (Ctrl+.) sobre un error: `pirnt` → `print` |
| Autocompletado | según el tipo: `jugador.`, `"texto".`, `File.`... |
| Documentación al pasar el ratón | incluidos tus comentarios `///` |
| Ayuda de parámetros | al escribir `(` en una llamada |
| Ir a la definición | F12 (también entre archivos) |
| Buscar referencias | Mayús+F12 |
| Renombrar | F2 (en todos los archivos) |
| Esquema del archivo | panel *Esquema* y Ctrl+Mayús+O |
| Botones encima del código | **▶ Ejecutar · ⟳ Repetir al guardar · ✓ Comprobar** en la primera línea y **▶ Probar** encima de cada `test(...)` |
| Ejecutar | **F5** o el botón ▶ (un archivo sin guardar se guarda como `.hez`) |
| Ejecutar y repetir al guardar | **Ctrl+Alt+F5** (`hyper watch`) |
| Consola interactiva | *HyperEZ: Abrir consola interactiva* |
| Comprobar errores | Ctrl+Mayús+F5 |
| Dar formato | **Mayús+Alt+F** o clic derecho → *Dar formato al archivo* (`hyper fmt`) |
| Ver el bytecode | clic derecho → *HyperEZ: Ver el bytecode del archivo* |
| Abrir en HyperEZ Editor | clic derecho → *HyperEZ: Abrir en HyperEZ Editor* (Windows) |
| Pruebas | *HyperEZ: Ejecutar pruebas* |
| Nuevo proyecto | *HyperEZ: Nuevo proyecto* (elige el tipo) |
| Referencia de la API | *HyperEZ: Abrir la referencia de la API* |
| Fragmentos | `funcion`, `clase`, `para`, `paracada`, `segun`, `prueba`, `juego`... (Tab) |
| Muestras de color | `"#ff8800"`, `Color(255, 136, 0)` con selector |
| Bienvenida | *Ayuda → Tutoriales → Empezar con HyperEZ* |
| Tema de colores | *HyperEZ Noche* (Archivo → Preferencias → Tema de color) |
| Iconos de HyperEZ | las carpetas de un proyecto (`hypersettings`, `hypercode`, `hypertests`...) y los `.hez` con su icono: *HyperEZ: Usar los iconos de HyperEZ*, o *Preferencias → Tema de iconos de archivo → HyperEZ*. Se ofrece una vez al abrir un proyecto con `hypersettings/` |

## Ajustes

| Ajuste | Por defecto | |
|---|---|---|
| `hyperez.path` | | ruta de `hyper`; vacío = el instalado, el del `PATH` o el incluido en la extensión |
| `hyperez.languageServer.enabled` | `true` | errores, autocompletado y demás |
| `hyperez.run.saveBeforeRun` | `true` | guardar antes de ejecutar |
| `hyperez.run.reuseTerminal` | `true` | cerrar la terminal de la ejecución anterior |
| `hyperez.colorDecorators` | `true` | muestras de color |
| `hyperez.codeLens` | `true` | botones ▶ Ejecutar / ⟳ / ✓ encima del código |
| `hyperez.formatOnSave` | `false` | dar formato al guardar |

## Tareas

En `.vscode/tasks.json`:

```text
{
  "version": "2.0.0",
  "tasks": [
    { "type": "hyperez", "command": "check", "file": ".", "problemMatcher": ["$hyperez"], "group": "build" },
    { "type": "hyperez", "command": "test", "group": "test" }
  ]
}
```

## Publicar la extensión

Las instrucciones para publicarla en el Marketplace de VS Code están en [editors/vscode/PUBLICAR.md](../../editors/vscode/PUBLICAR.md).
