# Colecciones y algoritmos

Además de `list`, `map` y `set` (que tienen decenas de métodos), HyperEZ trae estructuras de datos clásicas (`import collections`) y algoritmos listos para usar (`import algo`).

## Métodos de listas, mapas y conjuntos

```hz
var votos = ["sí", "no", "sí", "sí"]
print(votos.countBy())                         // {sí: 3, no: 1}
print([1, 2, 3, 4].windowed(2))                // [[1, 2], [2, 3], [3, 4]]
print([1, 2, 3].scan((a, b) => a + b))         // sumas acumuladas: [1, 3, 6]
print([1, 2, 3, 2].intersect([2, 3, 5]), [1, 2].union([2, 3]), [1, 2, 3].difference([2]))
print([3, 1, 2].median(), [2, 3, 4].product(), [1, null, 2].compact())
print([1, 3, 5, 7].binarySearch(5))            // búsqueda rápida en una lista ordenada: 2
print(["ana", "luis"].toMap(n => n[0]))        // {a: ana, l: luis}
print([0] * 4)                                 // repetir una lista: [0, 0, 0, 0]

var puntos = { ana: 5, luis: 9, eva: 7 }
print(puntos.sortByValue(true).keys())         // ranking: [luis, eva, ana]
print(puntos.pick(["ana"]), puntos.omit(["ana"]), puntos.count(p => p > 6))
var cuenta = {}
cuenta.update("gato", n => n + 1, 0)           // cuenta.gato = 1

var a = set(1, 2, 3)
print(a.union([3, 4]).toList(), a.intersect([2, 3]).toList(), a.isSubset([1, 2, 3, 4]))
```

| Listas | |
|---|---|
| `countBy(f?)`, `sumBy(f)`, `uniqueBy(f)` | contar, sumar o quitar repetidos según una función |
| `windowed(n, paso)`, `rotate(n)`, `enumerate()` | ventanas, girar, pares `[posición, elemento]` |
| `intersect`, `difference`, `union` | operaciones de conjuntos conservando el orden |
| `takeWhile`, `skipWhile`, `none` | mientras se cumpla una condición |
| `sample(n)`, `toSet()`, `compact()`, `product()`, `median()` | |
| `binarySearch(v)`, `insertSorted(v)`, `lastIndexOf(v)` | listas ordenadas |
| `scan(f, inicial?)`, `toMap(clave, valor?)` | |

| Mapas | Conjuntos |
|---|---|
| `pick`, `omit`, `invert`, `update`, `mapKeys` | `union`, `intersect`, `difference`, `symmetricDifference` |
| `any`, `all`, `count`, `find` | `isSubset`, `isSuperset`, `isDisjoint` |
| `sortByKey(desc?)`, `sortByValue(desc?)` | `map`, `filter` |

## `import collections`

```hz
import collections

var pila = collections.Stack()       // el último en entrar sale primero
pila.push("a")
pila.push("b")
print(pila.pop(), pila.peek())       // b a

var cola = collections.Queue()       // el primero en entrar sale primero (rápida aunque sea enorme)
cola.push("Ana")
cola.push("Luis")
print(cola.pop(), len(cola))         // Ana 1

var urgentes = collections.PriorityQueue()   // sale antes la prioridad más baja
urgentes.push("normal", 5)
urgentes.push("urgente", 1)
print(urgentes.pop())                // urgente

var letras = collections.Counter("abracadabra".chars())
print(letras.mostCommon(2))          // [[a, 5], [b, 2]]

var cache = collections.LRUCache(100)          // recuerda los 100 últimos resultados
print(cache.getOrCompute(12, n => n * n))
```

| Clase | Para qué |
|---|---|
| `Stack` | deshacer, recorridos en profundidad, paréntesis |
| `Queue` | turnos, recorridos en anchura, tareas pendientes |
| `Deque` | añadir y quitar por los dos lados |
| `PriorityQueue(mayorPrimero?)` | lo más urgente primero, rutas (Dijkstra), simulaciones |
| `Counter` | frecuencias de palabras, votos, inventarios |
| `LRUCache(capacidad)` | guardar resultados caros sin gastar memoria sin límite |
| `Graph(dirigido?)` | mapas, redes, dependencias |

### Grafos

```hz
import collections

var g = collections.Graph()
g.addEdge("Madrid", "Toledo", 72)
g.addEdge("Toledo", "Ciudad Real", 117)
g.addEdge("Madrid", "Ciudad Real", 190)
print(g.shortestPath("Madrid", "Ciudad Real"))   // {path: [Madrid, Toledo, Ciudad Real], distance: 189}
print(g.distancesFrom("Toledo"), g.bfs("Madrid"), g.components().length)

var tareas = collections.Graph(true)             // dirigido
tareas.addEdge("comprar", "cocinar")
tareas.addEdge("cocinar", "comer")
print(tareas.topologicalSort(), tareas.hasCycle())
```

`addNode`, `addEdge(a, b, peso)`, `removeEdge`, `hasNode`, `hasEdge`, `nodes`, `neighbors`, `weight`, `edges`, `bfs`, `dfs`, `shortestPath` (Dijkstra), `distancesFrom`, `components`, `topologicalSort`, `hasCycle`.

## `import algo`

```hz
import algo

print(algo.permutations([1, 2, 3]).length)       // 6
print(algo.combinations(["a", "b", "c"], 2))     // [[a, b], [a, c], [b, c]]
print(algo.product([1, 2], ["x", "y"]))          // producto cartesiano
print(algo.powerset([1, 2]))                     // todos los subconjuntos
print(algo.lcs("ABCBDAB", "BDCABA"))             // subsecuencia común más larga

var lento = algo.memoize(n => n * n)             // recuerda los resultados
print(lento(12), lento(12))
```

### Caminos en cuadrículas (A*)

Una cuadrícula es una lista de textos (`#` es pared) o una lista de listas (`true` o `1` es pared):

```hz
import algo

var mapa = ["S..#....",
            ".#.#.##.",
            ".#...#G."]
var camino = algo.gridPath(mapa, [0, 0], [6, 2])           // lista de [x, y]
print(camino.length - 1, "pasos")
foreach fila in algo.drawPath(mapa, camino) { print(fila) }
print(algo.floodFill(mapa, 0, 0).length, "casillas alcanzables")
print(algo.neighbors(mapa, 0, 0), algo.walkable(mapa, 3, 0))
```

`gridPath(grid, inicio, fin, diagonales?)` usa A* (el camino más corto, explorando lo mínimo), `floodFill` encuentra la zona conectada, `neighbors` da las casillas vecinas pisables y `drawPath` dibuja el camino para verlo en la consola. Sirve para juegos (enemigos que te persiguen), robots y laberintos: mira [algoritmos/laberinto.hz](../../examples/algoritmos/laberinto.hz).
