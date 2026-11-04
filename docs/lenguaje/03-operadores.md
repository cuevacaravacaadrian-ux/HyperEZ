# 3. Operadores

## Aritméticos

```hz
var a = 7
var b = 2
print(a + b, a - b, a * b, a / b, a % b, a ** b)  // 9 5 14 3.5 1 49
print(-a, -7 % 3)                                 // el módulo tiene el signo del divisor: 2
```

`+` también une textos (`"a" + "b"`), listas (`[1] + [2]`) y, si un lado es texto, convierte el otro: `"Puntos: " + 10`. `*` repite textos: `"ab" * 3`.

## Asignación

```hz
var x = 10
x += 5; x -= 2; x *= 3; x /= 3; x %= 7
x++
x--
var nombre = null
nombre ??= "anónimo" // asigna solo si es null
print(x, nombre)
```

`**=` eleva y guarda: `p **= 2` es `p = p ** 2`.

## Comparación

`==`, `!=`, `<`, `<=`, `>`, `>=`. Los textos se comparan en orden alfabético; las listas y mapas, por contenido; los `struct`, por sus campos; los objetos de clases, por identidad (salvo que definan `equals`).

```hz
print(3 == 3.0, "a" < "b", [1, 2] == [1, 2], { x: 1 } == { x: 1 })
```

## Lógicos

`and`, `or` y `not`. `and` y `or` no evalúan el lado derecho si no hace falta, y devuelven uno de los dos valores:

```hz
var usuario = null
print(true and false, true or false, not true)
print(usuario or "invitado")
```

## Bits

Para números enteros: `&` (y), `|` (o), `^` (o exclusivo), `~` (negación), `<<` y `>>` (desplazamientos).

```hz
print(6 & 3, 6 | 3, 6 ^ 3, ~5, 1 << 4, 256 >> 2)  // 2 7 5 -6 16 64
var permisos = 0b101
print((permisos & 0b100) != 0)
```

## Null: `??` y `?.`

```hz
var jugador = { nombre: "Ana", arma: null }
print(jugador.arma ?? "manos")        // valor por defecto si es null
print(jugador.arma?.nombre)           // acceso seguro: null en vez de error
var lista: list? = null
print(lista?.length ?? 0)
```

## `in`

¿Está un elemento en una lista, una clave en un mapa, un texto dentro de otro o un número en un rango?

```hz
print(2 in [1, 2, 3], "vida" in { vida: 3 }, "mun" in "mundo", 5 in 1..10)
```

## Rangos

`a..b` va de `a` a `b` sin incluir `b`; `a..=b` incluye `b`. En los bucles `for`, `step` elige el salto (`for i in 10..0 step -3`).

```hz
print((0..5).toList(), (1..=3).toList(), (5..0).toList())
```

## Tubería `|>`

`valor |> f` es lo mismo que `f(valor)`: sirve para encadenar pasos de izquierda a derecha en lugar de anidar paréntesis. Si a la derecha hay una llamada, el valor entra como **primer argumento**.

```hz
function doble(x: number): number { return x * 2 }
function sumar(a: number, b: number): number { return a + b }

print(5 |> doble)                 // 10        (doble(5))
print(5 |> doble |> sumar(1))     // 11        (sumar(doble(5), 1))
var pares = [1, 2, 3, 4, 5, 6]
    |> (l => l.filter(n => n % 2 == 0))
    |> (l => l.map(n => n * 10))
print(pares)                      // [20, 40, 60]
```

Una línea que empieza por `|>` continúa la anterior. La tubería va después de todo lo demás: `2 + 3 |> doble` es `doble(2 + 3)`.

## Condicional en una expresión

```hz
var vida = 0
print(vida > 0 ? "vivo" : "fin de la partida")
```

## Precedencia

De mayor a menor (los de arriba se calculan primero). Usa paréntesis cuando dudes.

| Operadores | |
|---|---|
| `x.y` `x?.y` `x[i]` `f()` | acceso y llamada |
| `**` | potencia (por la derecha: `2 ** 3 ** 2` es `2 ** 9`; `-2 ** 2` es `-4`) |
| `-x` `+x` `!x` `~x` `++x` `--x` `await x` | unarios |
| `*` `/` `%` | |
| `+` `-` | |
| `..` `..=` | rangos (`0..n + 1` es `0..(n + 1)`) |
| `<<` `>>` | |
| `&` | |
| `^` | |
| `\|` | |
| `<` `<=` `>` `>=` `is` `is not` `in` `not in` | |
| `==` `!=` | |
| `not` | (`not a == b` es `not (a == b)`) |
| `and` | |
| `or` | |
| `??` | |
| `? :` | condicional |
| `\|>` | tubería |
| `=` `+=` `-=` `*=` `/=` `%=` `**=` `??=` | asignación |

## Operadores en tus propias clases

Una clase puede definir qué hacen `+`, `-`, `*`, `/`, `%`, `**`, las comparaciones, `==` y `obj[i]`. Está explicado en [Clases: sobrecarga de operadores](07-clases.md#sobrecarga-de-operadores).
