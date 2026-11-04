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
