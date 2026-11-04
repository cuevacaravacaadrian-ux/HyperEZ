# 8. Errores

Hay dos clases de errores:

- **De compilación**: HyperEZ los encuentra antes de ejecutar (nombres mal escritos, tipos que no encajan...). El programa no se ejecuta hasta que se arreglan. El [catálogo de errores](../errores.md) explica cada código.
- **En ejecución**: ocurren mientras el programa funciona (dividir entre cero, un índice fuera de la lista, un archivo que no existe...). Si nadie los captura, el programa termina y muestra la línea exacta y la pila de llamadas.

## `try`, `catch` y `finally`

```hz
try {
    var lista = [1, 2, 3]
    print(lista[10])
} catch e {
    print("Algo falló:", e.message)
} finally {
    print("finally se ejecuta siempre, haya error o no")
}
```

El error capturado (`e`) tiene `message` (el texto), `name` (el tipo de error), `stack` (la pila de llamadas), `file`, `line` y `column`.

## Lanzar errores: `throw`

```hz
function dividir(a: number, b: number): number {
    if b == 0 { throw Error("no se puede dividir entre cero") }
    return a / b
}
try { dividir(1, 0) } catch e { print(e.message) }
```

## Errores propios

Heredando de `Error` se crean tipos de error que se pueden distinguir con `is`:

```hz
class SinMana extends Error {}
class SinVida extends Error {}

function lanzarHechizo(mana: number) {
    if mana < 5 { throw SinMana("no tienes suficiente maná") }
    print("¡Bola de fuego!")
}

try {
    lanzarHechizo(2)
} catch e {
    if e is SinMana { print("Bebe una poción:", e.message) }
    else { throw e } // los demás errores siguen su camino
}
```

## Comprobar condiciones: `assert`

`assert(condición, mensaje)` lanza un error si la condición es falsa. Es útil para detectar pronto lo que "no debería pasar":

```hz
var vida = 3
assert(vida >= 0, "la vida nunca debería ser negativa")
print("todo en orden")
```
