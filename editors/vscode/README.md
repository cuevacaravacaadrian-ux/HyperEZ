# HyperEZ para Visual Studio Code

Soporte oficial del lenguaje **HyperEZ** (`.hz` y `.hez`): un lenguaje sencillo, en español, pensado para aprender a programar y crear juegos.

## Funciones

- **Resaltado de sintaxis** completo, también dentro de los bloques ` ```hz ` de Markdown.
- **Errores en vivo** mientras escribes, con el mismo analizador que el compilador (tipos, nombres mal escritos, argumentos que faltan...).
- **Arreglos rápidos**: si escribes `pirnt`, la bombilla te ofrece cambiarlo por `print`.
- **Autocompletado** de variables, funciones, clases, módulos y miembros según el tipo (`jugador.`, `"texto".`, `Console.`...).
- **Documentación al pasar el ratón** y **ayuda de parámetros** al escribir una llamada.
- **Ir a la definición** (F12), **buscar referencias** (Mayús+F12), **renombrar** (F2) y **esquema** del archivo.
- **Ejecutar con F5** (o el botón ▶): el programa se abre en una terminal propia, con colores y teclado.
- **Ejecutar y repetir al guardar** (Ctrl+Alt+F5): cada vez que guardas un `.hz` el programa se vuelve a ejecutar.
- **Consola interactiva**: prueba código línea a línea y ve el resultado al momento.
- **Comprobar errores** (Ctrl+Mayús+F5) y **ejecutar pruebas** (`test(...)` + `hyper test`).
- **Fragmentos** en español: `clase`, `funcion`, `para`, `paracada`, `segun`, `evento`, `prueba`, `juego`...
- **Muestras de color** para `"#ff8800"`, `Color(255, 136, 0)` y `Color.RED`, con selector.
- **Tareas** (`hyper check`, `hyper test`) con detector de problemas.

## Comandos

| Comando | Atajo |
|---|---|
| HyperEZ: Ejecutar archivo | F5 / Ctrl+F5 |
| HyperEZ: Ejecutar y repetir al guardar (watch) | Ctrl+Alt+F5 |
| HyperEZ: Abrir consola interactiva | |
| HyperEZ: Comprobar errores | Ctrl+Mayús+F5 |
| HyperEZ: Ejecutar pruebas | |
| HyperEZ: Nuevo proyecto | |
| HyperEZ: Abrir la referencia de la API | |
| HyperEZ: Reiniciar el servidor de lenguaje | |

## Requisitos

La extensión incluye una copia de `hyper.exe`. Si instalas HyperEZ con su instalador, o tienes `hyper` en el `PATH`, se usa esa versión. También puedes indicar una ruta en el ajuste `hyperez.path`.

## Ajustes

- `hyperez.path`: ruta de `hyper.exe`.
- `hyperez.languageServer.enabled`: activa o desactiva el servidor de lenguaje.
- `hyperez.run.saveBeforeRun`: guarda antes de ejecutar.
- `hyperez.run.reuseTerminal`: reutiliza la terminal de ejecución.
- `hyperez.colorDecorators`: muestras de color.

## Un ejemplo

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
