# 5. Funciones

## Declarar y llamar

```hz
/// Daño de un ataque (los comentarios /// se ven al pasar el ratón en VS Code).
function danio(base: number, multiplicador: number = 2): number {
    return base * multiplicador
}
print(danio(10), danio(10, 3))
```

- Los tipos de los parámetros y del resultado son opcionales.
- Una función sin `return` (o con `return` solo) devuelve `null`; su tipo es `void`.
- Las funciones se pueden usar antes de declararlas en el mismo archivo.

## Parámetros

```hz
function crear(nombre: text, vida: number = 10, ...objetos: text): text {
    return "${nombre} (${vida}) con ${objetos.length} objetos: ${objetos.join(", ")}"
}
print(crear("Ana"))                        // valores por defecto
print(crear("Ana", 5, "espada", "llave"))  // ...objetos recoge el resto en una lista
print(crear(nombre: "Luis", vida: 3))      // argumentos con nombre, en cualquier orden
print(crear("Eva", 7, ...["arco", "flecha"])) // ... expande una lista como argumentos
```

## Funciones como valores y lambdas

Las funciones son valores: se guardan en variables, se pasan a otras funciones y se devuelven.

```hz
var doble = x => x * 2
var sumar = (a, b) => a + b
var conTipos = (x: number): number => x + 1
var larga = (lista: list) => {
    var total = 0
    foreach x in lista { total += x }
    return total
}
print(doble(4), sumar(2, 3), conTipos(1), larga([1, 2, 3]))
print([3, 1, 2].map(doble), [1, 2, 3, 4].filter(x => x % 2 == 0))
```

Para una lambda que devuelve un mapa, ponlo entre paréntesis (si no, las llaves se leen como un bloque):

```hz
var puntos = [1, 2].map(x => ({ x: x, y: x * 10 }))
print(puntos)
```

## Clausuras

Una función recuerda las variables del sitio donde se creó, aunque ese sitio ya haya terminado:

```hz
function contador() {
    var c = 0
    return () => {
        c += 1
        return c
    }
}
var siguiente = contador()
siguiente()
siguiente()
print(siguiente()) // 3
```

## Recursión

```hz
function factorial(n: number): number {
    return n <= 1 ? 1 : n * factorial(n - 1)
}
print(factorial(10))
```

## Funciones asíncronas

`async function` crea una tarea que puede esperar sin bloquear el programa. Ver [eventos y tareas](09-eventos-async.md).

```hz
async function cargar(nivel: number): text {
    await wait(0.05)
    return "nivel ${nivel} cargado"
}
print(await cargar(1))
```
