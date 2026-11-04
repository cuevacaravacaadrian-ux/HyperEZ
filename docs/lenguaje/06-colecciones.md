# 6. Colecciones

## Listas

Una lista guarda valores en orden. Los índices empiezan en 0 y los negativos cuentan desde el final.

```hz
var numeros = [5, 3, 8, 1]
numeros.add(10)
print(numeros[0], numeros[-1], numeros.length)
print(numeros[1..3])                 // trozo: índices 1 y 2
numeros[0] = 50
print(numeros.contains(8), numeros.indexOf(8))
print(numeros.copy().sort(), numeros.sum(), numeros.min(), numeros.max())
```

Métodos más usados (la lista completa está en la [API](../biblioteca/API.md)):

| Método | Qué hace |
|---|---|
| `add(x)` `insert(i, x)` | añadir al final / en una posición |
| `pop()` `shift()` `removeAt(i)` `remove(x)` | quitar el último / el primero / por posición / por valor |
| `map(f)` `filter(f)` `reduce(f, inicial)` | transformar, filtrar y acumular |
| `find(f)` `findIndex(f)` `any(f)` `all(f)` `count(f)` | buscar |
| `sort()` `sortBy(f)` `reverse()` `reversed()` `shuffle()` | ordenar (en el sitio o una copia) |
| `join(sep)` `unique()` `take(n)` `skip(n)` `chunk(n)` `zip(otra)` `flat()` | otras |
| `groupBy(f)` `minBy(f)` `maxBy(f)` `average()` `partition(f)` | agrupar y resumir |

```hz
var jugadores = [{ nombre: "Ana", puntos: 120 }, { nombre: "Luis", puntos: 95 }, { nombre: "Eva", puntos: 130 }]
var ranking = jugadores.sortBy(j => -j.puntos).map(j => j.nombre)
print(ranking, jugadores.maxBy(j => j.puntos).nombre, jugadores.map(j => j.puntos).average())
```

## Mapas

Un mapa asocia claves (normalmente textos) con valores. Las claves que son nombres válidos se pueden usar con punto.

```hz
var jugador = { nombre: "Ana", vida: 3 }
jugador.oro = 10
jugador["nivel"] = 2
print(jugador.nombre, jugador["vida"], jugador.keys(), jugador.values())
print(jugador.has("oro"), jugador.get("mana", 0))
jugador.remove("oro")
foreach clave, valor in jugador { print(clave, "=", valor) }
```

## Conjuntos

Un conjunto guarda valores sin repetir:

```hz
var vistos = set(1, 2, 2, 3)
vistos.add(4)
print(vistos.length, 2 in vistos, vistos)
```

## Rangos

```hz
var r = 1..=5
print(r.toList(), 3 in r, len(r))
foreach i in 0..10 step 4 { print(i) }
```

## Desestructuración

Saca varios valores de una lista o de un mapa (u objeto) a la vez:

```hz
var [primero, segundo, ...resto] = [10, 20, 30, 40]
print(primero, segundo, resto)          // 10 20 [30, 40]

var {nombre, vida} = { nombre: "Ana", vida: 3 }
print(nombre, vida)

var {x, y: alto} = Vec2(3, 4)           // {campo: otroNombre} cambia el nombre
print(x, alto)

function extremos(l: list): list { return [l.min(), l.max()] }
const [minimo, maximo] = extremos([4, 9, 1])
print(minimo, maximo)
```

Si a la lista le faltan elementos, es un error en ejecución (índice fuera de rango).

## Repetir una lista

```hz
print([0] * 5)                 // [0, 0, 0, 0, 0]
print([1, 2] * 3)              // [1, 2, 1, 2, 1, 2]
var tablero = range(3).toList().map(i => ["."] * 3)   // cada fila es una lista distinta
tablero[0][0] = "X"
print(tablero)
```

Como `"ab" * 3` con los textos. Ojo: `[[0] * 3] * 3` repetiría *la misma* fila tres veces; para una cuadrícula crea cada fila por separado, como arriba.

## Comprensiones

Una lista (o un mapa) a partir de otra colección, en una sola línea: `[expresión for x in colección if condición]`.

```hz
print([x * x for x in 1..=5])                  // [1, 4, 9, 16, 25]
print([x for x in [5, 1, 8, 3] if x > 2])      // [5, 8, 3]
print([i * 10 + x for i, x in [1, 2, 3]])      // con la posición: [1, 12, 23]
print([x for x in 0..10 step 4])               // [0, 4, 8]
print([[y * x for y in 1..=3] for x in 1..=2]) // anidadas: [[1, 2, 3], [2, 4, 6]]

print({n: n * n for n in 1..=3})               // mapa: {1: 1, 2: 4, 3: 9}
print({k: v * 10 for k, v in {a: 1, b: 2}})    // {a: 10, b: 20}
```

Es lo mismo que un bucle que va añadiendo a una lista, pero más corto. En una comprensión de mapa la clave es una expresión (como si llevara corchetes). Dentro no se puede usar `await`.

## Propagación en mapas

`...otro` copia las entradas de otro mapa dentro de uno nuevo; las claves que van después ganan:

```hz
var base = {color: "azul", tamano: 2}
print({...base, tamano: 5})        // {color: azul, tamano: 5}
print({id: 1, ...base})            // {id: 1, color: azul, tamano: 2}
var copia = {...base}              // copia independiente
copia.color = "rojo"
print(base.color)                  // azul
```

## Trozos con rangos

Un rango dentro de los corchetes devuelve un trozo (de una lista o de un texto). Los extremos se pueden omitir y los negativos cuentan desde el final:

```hz
var l = [1, 2, 3, 4, 5, 6]
print(l[1..3], l[2..], l[..2], l[..=2], l[-2..])   // [2, 3] [3, 4, 5, 6] [1, 2] [1, 2, 3] [5, 6]
print("Programar"[3..], "Programar"[..3])          // gramar Pro
print(l[..])                                         // copia de toda la lista
```
