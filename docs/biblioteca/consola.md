# Consola, juegos y herramientas

## `Console`

Para juegos de texto y aplicaciones de terminal: colores, cursor y teclas sin pulsar Intro.

```hz
Console.print("¡Peligro!", "red")             // en color
Console.write("sin salto de línea ", "cyan")
Console.print("texto", "yellow", "blue")      // color y fondo
print(Console.interactive)                    // false si no hay teclado (pruebas, salida redirigida)
```

| Función | Qué hace |
|---|---|
| `clear()` | borra la pantalla |
| `print(texto, color?, fondo?)` / `write(...)` | escribe con o sin salto de línea |
| `moveTo(columna, fila)` | mueve el cursor |
| `showCursor(visible)` | muestra u oculta el cursor |
| `title(texto)` | título de la ventana |
| `readKey()` | espera una tecla y devuelve su nombre: `"a"`, `"up"`, `"enter"`, `"space"`, `"escape"`... |
| `pollKey()` | la tecla pulsada o `null` (no espera): para bucles de juego |
| `keyAvailable()` | ¿hay una tecla esperando? |
| `width`, `height`, `interactive` | tamaño de la consola y si hay teclado |

Colores: `"black"`, `"red"`, `"green"`, `"yellow"`, `"blue"`, `"magenta"`, `"cyan"`, `"white"`, `"gray"` (y `Color(r, g, b)`).

### Bucle de un juego en tiempo real

```text
var jugando = true
while jugando {
    var tecla = Console.pollKey()           // no espera: el juego sigue aunque no pulses nada
    if tecla == "escape" { jugando = false }
    actualizar(tecla)
    dibujar()
    System.sleep(1 / 15)                    // 15 fotogramas por segundo
}
```

Los ejemplos de [examples/juegos](../../examples/juegos) (serpiente, tres en raya, mazmorra) usan todo esto y además se juegan solos cuando no hay teclado.

## Vectores y colores

```hz
var pos = Vec2(10, 5)
var vel = Vec2(2, -1)
pos = pos + vel * 3
print(pos, pos.length(), pos.normalized(), pos.distance(Vec2(0, 0)))
print(Vec3(1, 2, 3) + Vec3(1, 1, 1), Color("#ff8800"), Color(255, 0, 0).toHex())
```

Funciones útiles para juegos: `clamp`, `lerp`, `remap`, `wrap`, `approach`, `smoothstep`, `distance`, `random`, `randomInt`, `randomChoice`, `chance(probabilidad)` y `randomSeed(n)` (misma semilla, mismos resultados en todos los sistemas).

## `cli`: herramientas de línea de órdenes

```hz
import cli
var a = cli.parseArgs(["informe.txt", "--formato", "csv", "--modo=rapido", "-v"])
print(a.positional, a.options, a.flags)
print(cli.progressBar(0.42))
print(cli.table([["nombre", "puntos"], ["Ana", 120], ["Luis", 95]]))
```

`cli.ask(pregunta, porDefecto)` y `cli.confirm(pregunta)` preguntan al usuario (y devuelven el valor por defecto si no hay teclado). Con `System.args` como entrada, `parseArgs` lee los argumentos reales del programa: `hyper herramienta.hz archivo --opcion valor`.
