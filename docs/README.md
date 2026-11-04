# Documentación de HyperEZ

## Empezar

- [Instalación](empezar/instalacion.md): Windows, Linux, macOS y VS Code.
- [Primeros pasos](empezar/primeros-pasos.md): HyperEZ Editor, consola interactiva, VS Code, la terminal y proyectos.
- [Guía rápida](GUIA.md): todo el lenguaje en una página.
- [Tutorial: un juego paso a paso](empezar/tutorial-juego.md)
- [Tutorial: una red neuronal](empezar/tutorial-ia.md)

## El lenguaje

[Referencia completa de la sintaxis](lenguaje/README.md):
[léxico](lenguaje/01-lexico.md) ·
[variables y tipos](lenguaje/02-variables-tipos.md) ·
[operadores](lenguaje/03-operadores.md) ·
[control del flujo](lenguaje/04-control.md) ·
[funciones](lenguaje/05-funciones.md) ·
[colecciones](lenguaje/06-colecciones.md) ·
[clases](lenguaje/07-clases.md) ·
[errores](lenguaje/08-errores.md) ·
[eventos y tareas](lenguaje/09-eventos-async.md) ·
[módulos](lenguaje/10-modulos.md) ·
[pruebas](lenguaje/11-pruebas.md) ·
[gramática](lenguaje/gramatica.md)

## La biblioteca

[Mapa de la biblioteca](biblioteca/README.md) y guías por área:

| Área | Guía |
|---|---|
| Archivos, procesos, rutas, fechas | [sistema.md](biblioteca/sistema.md) |
| Datos binarios, hash, compresión | [binario.md](biblioteca/binario.md) |
| Expresiones regulares | [regex.md](biblioteca/regex.md) |
| Textos, números, fechas, plantillas, Markdown, INI, registro | [textos-documentos.md](biblioteca/textos-documentos.md) |
| Colecciones (pilas, colas, grafos) y algoritmos (A*, combinatoria) | [colecciones-algoritmos.md](biblioteca/colecciones-algoritmos.md) |
| Peticiones a internet | [red.md](biblioteca/red.md) |
| **Hacer webs**: páginas, aplicaciones, APIs, sitios desde Markdown, `hyper serve` | [web.md](biblioteca/web.md) |
| Inteligencia artificial y datos | [ia-datos.md](biblioteca/ia-datos.md) |
| Gráficos y sonido | [graficos-sonido.md](biblioteca/graficos-sonido.md) |
| Consola, juegos y herramientas | [consola.md](biblioteca/consola.md) |
| Sistemas operativos y bajo nivel | [sistemas-operativos.md](biblioteca/sistemas-operativos.md) |
| **Referencia de toda la API** | [API.md](biblioteca/API.md) |

## Herramientas

- [HyperEZ Editor](herramientas/editor.md): el editor clásico que se instala con HyperEZ.
- [La orden `hyper`](herramientas/cli.md): ejecutar, comprobar, probar, formatear, servir webs, bytecode...
- [Otros editores](../editors/README.md): Notepad++, Sublime Text, Vim, Neovim, Helix, highlight.js y GitHub.
- [Consola interactiva](herramientas/consola-interactiva.md)
- [Visual Studio Code](herramientas/vscode.md)
- [Instaladores y asociación de archivos](herramientas/instaladores.md)
- [Códigos de error](errores.md)

## Cómo está hecho

- [Arquitectura](interno/arquitectura.md): del texto a la ejecución, carpetas del código.
- [El compilador](interno/compilador.md): lexer, parser, analizador y generador de código.
- [La máquina virtual](interno/maquina-virtual.md): valores, GC, fibras, eventos.
- [Instrucciones del bytecode](interno/bytecode.md)
- [Rendimiento](interno/rendimiento.md): PGO, benchmarks y optimizaciones.
- [Cómo ampliar HyperEZ](interno/extender.md): bibliotecas, funciones nativas, sintaxis.

## El proyecto

- [Hoja de ruta](hoja-de-ruta.md)
- [Preguntas frecuentes](preguntas.md)
- [Cambios de cada versión](../CHANGELOG.md)
- [Cómo contribuir](../CONTRIBUTING.md)
