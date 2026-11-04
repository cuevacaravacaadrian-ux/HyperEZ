# Tutorial: un juego paso a paso

Vamos a hacer **"Caza de monedas"**: un `@` se mueve por un tablero recogiendo monedas `$` antes de que se acabe el tiempo. Cada paso añade una idea nueva; al final tendrás un juego completo de unas 60 líneas.

Crea una carpeta, abre en ella VS Code y crea `caza.hz`. Pulsa **Ctrl+Alt+F5** para que el programa se vuelva a ejecutar cada vez que guardes.

## Paso 1: el tablero

Empezamos dibujando un tablero vacío con un borde:

```hz
const ANCHO = 20
const ALTO = 8

function dibujar() {
    print("#" * ANCHO)
    repeat ALTO - 2 { print("#" + " " * (ANCHO - 2) + "#") }
    print("#" * ANCHO)
}
dibujar()
```

`"#" * 20` repite el texto 20 veces.

## Paso 2: el jugador y las monedas

Guardamos las posiciones con `Vec2` y dibujamos cada casilla según lo que haya en ella:

```hz
const ANCHO = 20
const ALTO = 8
randomSeed(1)

var jugador = Vec2(1, 1)
var monedas = []
repeat 5 { monedas.add(Vec2(randomInt(1, ANCHO - 2), randomInt(1, ALTO - 2))) }

function casilla(x: number, y: number): text {
    if x == 0 or y == 0 or x == ANCHO - 1 or y == ALTO - 1 { return "#" }
    if jugador == Vec2(x, y) { return "@" }
    if monedas.contains(Vec2(x, y)) { return "$" }
    return " "
}

function dibujar() {
    for y in 0..ALTO {
        var linea = ""
        for x in 0..ANCHO { linea += casilla(x, y) }
        print(linea)
    }
}
dibujar()
```

## Paso 3: moverse

`Console.readKey()` espera a que se pulse una tecla y devuelve su nombre. Movemos al jugador con las flechas, sin dejarle salir del tablero (`clamp`):

```text
function mover(tecla: text) {
    var d = match tecla {
        "up", "w" => Vec2(0, -1)
        "down", "s" => Vec2(0, 1)
        "left", "a" => Vec2(-1, 0)
        "right", "d" => Vec2(1, 0)
        else => Vec2(0, 0)
    }
    jugador = Vec2(clamp(jugador.x + d.x, 1, ANCHO - 2), clamp(jugador.y + d.y, 1, ALTO - 2))
}

while true {
    Console.clear()
    dibujar()
    mover(Console.readKey())
}
```

## Paso 4: recoger monedas y ganar

Al pisar una moneda la quitamos de la lista y sumamos puntos. Cuando no quedan, se acaba:

```text
var puntos = 0
function recoger() {
    var i = monedas.indexOf(jugador)
    if i >= 0 {
        monedas.removeAt(i)
        puntos += 10
    }
}
```

## Paso 5: el juego completo

Con un límite de movimientos, colores y la posibilidad de que el juego se juegue solo cuando no hay teclado (así se puede probar con `hyper test`):

```hz
const ANCHO = 20
const ALTO = 8
const MOVIMIENTOS = 60
randomSeed(7)

var jugador = Vec2(1, 1)
var monedas = []
repeat 5 { monedas.add(Vec2(randomInt(2, ANCHO - 2), randomInt(1, ALTO - 2))) }
var puntos = 0
var quedan = MOVIMIENTOS

function casilla(x: number, y: number): text {
    if x == 0 or y == 0 or x == ANCHO - 1 or y == ALTO - 1 { return "#" }
    if jugador == Vec2(x, y) { return "@" }
    if monedas.contains(Vec2(x, y)) { return "$" }
    return " "
}

function dibujar() {
    for y in 0..ALTO {
        var linea = ""
        for x in 0..ANCHO { linea += casilla(x, y) }
        print(linea)
    }
    print("Puntos: ${puntos}   Movimientos: ${quedan}   Monedas: ${monedas.length}")
}

function mover(tecla: text) {
    var d = match tecla {
        "up", "w" => Vec2(0, -1)
        "down", "s" => Vec2(0, 1)
        "left", "a" => Vec2(-1, 0)
        "right", "d" => Vec2(1, 0)
        else => Vec2(0, 0)
    }
    jugador = Vec2(clamp(jugador.x + d.x, 1, ANCHO - 2), clamp(jugador.y + d.y, 1, ALTO - 2))
    quedan -= 1
    var i = monedas.indexOf(jugador)
    if i >= 0 {
        monedas.removeAt(i)
        puntos += 10
    }
}

/// Sin teclado: va hacia la moneda más cercana.
function teclaAutomatica(): text {
    var objetivo = monedas.minBy(m => abs(m.x - jugador.x) + abs(m.y - jugador.y))
    if objetivo.x > jugador.x { return "right" }
    if objetivo.x < jugador.x { return "left" }
    return objetivo.y > jugador.y ? "down" : "up"
}

while monedas.length > 0 and quedan > 0 {
    if Console.interactive {
        Console.clear()
        dibujar()
        mover(Console.readKey())
    } else {
        mover(teclaAutomatica())
    }
}
dibujar()
Console.print(monedas.isEmpty() ? "¡Has ganado!" : "Se acabaron los movimientos", monedas.isEmpty() ? "green" : "red")
```

## Para seguir

- Añade enemigos que se muevan solos (otra lista de `Vec2` que cambia en cada turno).
- Separa la lógica en `src/tablero.hz` y escribe pruebas en `tests/` ([pruebas](../lenguaje/11-pruebas.md)).
- Para un juego en tiempo real, cambia `readKey()` por `pollKey()` y añade `System.sleep(0.1)` al bucle.
- Mira los juegos completos de [examples/juegos](../../examples/juegos).
