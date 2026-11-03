# Guía de HyperEZ

HyperEZ es un lenguaje pensado para aprender a programar y para crear juegos. Los mensajes de error están en español y explican cómo arreglar el problema. Esta guía recorre el lenguaje de principio a fin; la lista completa de funciones está en la referencia de la API (`hyper docs API.md`).

Para ejecutar un programa guárdalo como `programa.hz` y escribe `hyper programa.hz`, haz doble clic en el archivo o pulsa **F5** en VS Code.
Con `hyper watch programa.hz` (o **Ctrl+Alt+F5** en VS Code) se vuelve a ejecutar cada vez que guardas.

Para probar ideas sin crear un archivo, escribe `hyper` en una terminal: es la **consola interactiva**. Cada línea se ejecuta al pulsar Intro, el valor de una expresión se muestra solo y lo que defines se conserva:

```text
hz> var precio = 12
hz> precio * 3
36
hz> "oferta".upper()
"OFERTA"
```

`.ayuda` muestra los comandos de la consola: `.vars` (lo que has definido), `.guardar archivo.hz` (convierte la sesión en un programa), `.cargar archivo.hz`, `.reiniciar` y `.salir`.

## 1. Primer programa

```hz
print("¡Hola, mundo!")
var nombre = "Ana"
print("Hola, ${nombre}. Tu nombre tiene ${len(nombre)} letras.")
```

`print` muestra valores separados por espacios. Dentro de un texto, `${...}` inserta el resultado de cualquier expresión.

## 2. Variables y tipos

```hz
var vidas = 3            // se puede cambiar
const MAX_VIDAS = 5      // no se puede cambiar
let semilla = 1234       // se asigna una sola vez

var puntos: number = 0   // con tipo: el compilador avisa si no encaja
var nombre: text = "Ana"
var jefe: text? = null   // el ? permite null
```

Los tipos son opcionales: si los escribes, HyperEZ comprueba los errores antes de ejecutar (`hyper check`). Tipos básicos: `number`, `int`, `text`, `bool`, `list`, `map`, `set`, `function`, `any` y los nombres de tus clases.

## 3. Números, textos y operadores

```hz
var a = 7
print(a + 2, a - 2, a * 2, a / 2, a % 2, a ** 2)   // 9 5 14 3.5 1 49
print(0xFF, 0b1010, 1_000_000)
print(round(3.14159, 2), floor(2.7), clamp(15, 0, 10), random(1, 6))

var t = "  Hola Mundo  "
print(t.trim().upper(), t.contains("Mundo"), t.trim().split(" "))
print("ab" * 3, "ñandú".length, "{} tiene {} puntos".format("Ana", 10))

var largo = """Un texto
de varias líneas"""
print(largo.lines().length)
```

Comparaciones: `==`, `!=`, `<`, `<=`, `>`, `>=`. Lógica: `and`, `or`, `not`. Valores por defecto: `x ?? "si es null"`; acceso seguro: `jugador?.arma?.nombre`.

## 4. Listas, mapas, conjuntos y rangos

```hz
var numeros = [5, 3, 8, 1]
numeros.add(10)
print(numeros[0], numeros[-1], numeros[1..3])
print(numeros.map(x => x * 2), numeros.filter(x => x > 4), numeros.sum())
print(numeros.copy().sort(), numeros.maxBy(x => x), [1, 2] + [3])

var jugador = { nombre: "Ana", vida: 3 }
jugador.oro = 10
jugador["nivel"] = 2
print(jugador.nombre, jugador.keys(), jugador.get("mana", 0))

var vistos = set(1, 2, 2, 3)
print(vistos.length, 2 in vistos)

print(range(5).toList(), (1..=3).toList())
```

## 5. Decisiones y bucles

```hz
var vida = 2
if vida <= 0 {
    print("Fin de la partida")
} else if vida == 1 {
    print("¡Cuidado!")
} else {
    print("Todo bien")
}

for i in 0..3 { print("i =", i) }          // 0, 1, 2
for i in 10..0 step -5 { print(i) }        // 10, 5
foreach fruta in ["pera", "uva"] { print(fruta) }
for i, fruta in ["pera", "uva"] { print(i, fruta) }
repeat 2 { print("¡hola!") }

var n = 0
while true {
    n += 1
    if n == 2 { continue }
    if n > 3 { break }
    print("n =", n)
}
```

### match

`match` elige un resultado según un valor. Admite varios valores, rangos, guardas con `if` y bloques; en un bloque, la última expresión es el resultado.

```hz
function describir(x: number): text {
    return match x {
        0 => "cero"
        1, 2, 3 => "pequeño"
        4..10 => "medio"
        else if x < 0 => "negativo"
        else => "grande"
    }
}
print(describir(2), describir(7), describir(-1), describir(50))

var tecla = "space"
var accion = match tecla {
    "space" => { print("¡salto!"); "saltar" }
    "escape" => "salir"
    else => "nada"
}
print(accion)
```

## 6. Funciones

```hz
/// Calcula el daño de un ataque. (Los comentarios /// aparecen al pasar el ratón en VS Code.)
function danio(base: number, multiplicador: number = 2, ...extras: number): number {
    return base * multiplicador + extras.sum()
}
print(danio(10), danio(10, 3), danio(base: 5, multiplicador: 4), danio(1, 1, 2, 3))

var doble = x => x * 2
var sumar = (a, b) => a + b
print(doble(4), sumar(2, 3))

function contador() {
    var c = 0
    return () => {
        c += 1
        return c
    }
}
var siguiente = contador()
siguiente()
print(siguiente())   // 2
```

## 7. Clases, structs, enums e interfaces

```hz
interface Danable { function recibir(n: number): void }

class Personaje implements Danable {
    var nombre: text
    var vida: number = 10
    static var creados = 0

    constructor(nombre: text) {
        this.nombre = nombre
        creados += 1
    }
    get vivo(): bool { return vida > 0 }
    function recibir(n: number): void { vida = max(0, vida - n) }
    function toString(): text { return "${nombre} (${vida})" }
}

class Jefe extends Personaje {
    var escudo = 2
    constructor(nombre: text) {
        super(nombre)
        vida = 50
    }
    function recibir(n: number): void { super.recibir(n / escudo) }
}

var heroe = Personaje("Ana")
var jefe = Jefe("Rey")
jefe.recibir(20)
print(heroe, jefe, jefe is Personaje, Personaje.creados)

struct Punto { var x: number = 0; var y: number = 0 }
print(Punto(1, 2) == Punto(1, 2))   // los structs se comparan por valor

enum Estado { Menu, Jugando, Pausa }
var estado = Estado.Jugando
print(estado, estado.name, Estado.count)
```

## 8. Errores

```hz
class SinMana extends Error {}

function lanzarHechizo(mana: number) {
    if mana < 5 { throw SinMana("no tienes suficiente maná") }
    print("¡Bola de fuego!")
}

try {
    lanzarHechizo(2)
} catch e {
    print("Error:", e.message, e is SinMana)
} finally {
    print("esto se ejecuta siempre")
}
```

Si un error no se captura, HyperEZ muestra el mensaje, la línea exacta y la pila de llamadas.

## 9. Eventos

Los eventos separan "lo que pasa" de "quién reacciona". Ideal para juegos.

```hz
event Golpe(objetivo: text, danio: number)

on Golpe as e { print("${e.objetivo} pierde ${e.danio} de vida") }
once Golpe priority 10 as e { print("¡Primer golpe de la partida!") }

emit Golpe("Ana", 5)
emit Golpe(objetivo: "Rey", danio: 7)
```

Cada `on` devuelve una suscripción que se cancela con `off(suscripcion)`; `off(Golpe)` las cancela todas. Dentro de un manejador, `e.stopPropagation()` evita que se ejecuten los siguientes.

## 10. Tareas asíncronas

`async` y `await` permiten esperar sin bloquear el programa: temporizadores, oleadas de enemigos, animaciones de texto...

```hz
async function cuentaAtras(desde: number) {
    for i in desde..0 step -1 {
        print(i)
        await wait(0.2)
    }
    print("¡Ya!")
}

async function cargar(nombre: text): text {
    await wait(0.1)
    return "nivel ${nombre} listo"
}

await cuentaAtras(3)
print(await awaitAll([cargar("1"), cargar("2")]))
var reloj = spawn(async () => {
    while true {
        await wait(1)
        print("tic")
    }
})
reloj.cancel()
```

## 11. Módulos

Cada archivo es un módulo. Todo lo que no es `private` se puede importar desde otro archivo:

```text
// enemigos.hz
class Orco { var vida = 30 }
private var contador = 0

// main.hz
from "./enemigos.hz" import Orco
import "./enemigos.hz" as enemigos
var o = Orco()
var o2 = enemigos.Orco()
```

## 12. Juegos de consola

El módulo `Console` permite leer teclas sin pulsar Intro, escribir en color y mover el cursor. `Console.interactive` indica si hay un teclado (en las pruebas es `false`, y los ejemplos aprovechan para jugar solos).

```hz
var x = 5
var jugando = Console.interactive
while jugando {
    Console.clear()
    Console.print(" ".repeat(x) + "@", "cyan")
    Console.print("← → para moverte, Esc para salir", "gray")
    match Console.readKey() {
        "left" => x = max(0, x - 1)
        "right" => x = min(Console.width - 2, x + 1)
        "escape" => jugando = false
        else => null
    }
}
```

Para juegos en tiempo real usa `Console.pollKey()` (no espera) junto con `System.sleep(segundos)`. Mira `examples/serpiente.hz`, `examples/tres_en_raya.hz` y `examples/mazmorra.hz`.

## 13. Archivos y partidas guardadas

```hz
var partida = { jugador: "Ana", nivel: 3, objetos: ["espada", "llave"] }
File.write("partida.json", JSON.stringify(partida, 2))
var cargada = JSON.parse(File.read("partida.json"))
print(cargada.nivel, cargada.objetos)
File.delete("partida.json")
```

Las rutas relativas parten de la carpeta del programa.

## 14. Pruebas

```hz
function sumar(a: number, b: number): number { return a + b }

test("sumar números", () => {
    assertEqual(sumar(2, 2), 4)
    assertEqual(sumar(-1, 1), 0)
})
test("lanza un error", () => {
    assertThrows(() => { throw Error("boom") })
})
```

`hyper test` ejecuta cada archivo `.hz` de la carpeta `tests/`. Si junto a una prueba hay un archivo `.out` con la salida esperada, se compara línea a línea (`hyper test --update` lo genera).

## 15. Herramientas

| Comando | Qué hace |
|---|---|
| `hyper programa.hz` | Ejecuta |
| `hyper watch programa.hz` | Ejecuta y repite cada vez que guardas |
| `hyper` | Consola interactiva |
| `hyper check programa.hz` | Busca errores sin ejecutar |
| `hyper test` | Ejecuta las pruebas |
| `hyper new nombre` | Crea un proyecto con un juego de ejemplo |
| `hyper docs API.md` | Genera la referencia de la API |

En **VS Code** tienes errores mientras escribes, autocompletado, documentación al pasar el ratón, F12 para ir a la definición, F2 para renombrar, F5 para ejecutar y Ctrl+Alt+F5 para ejecutar cada vez que guardas.
