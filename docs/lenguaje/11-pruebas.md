# 11. Pruebas

Las pruebas comprueban automáticamente que tu código hace lo que debe. Así, cuando cambias algo, sabes al momento si has roto otra cosa.

## Escribir pruebas

```hz
function sumar(a: number, b: number): number { return a + b }

test("sumar números", () => {
    assertEqual(sumar(2, 2), 4)
    assertEqual(sumar(-1, 1), 0)
})

test("comprobaciones con mensaje", () => {
    var lista = [3, 1, 2]
    assert(lista.length == 3, "la lista debe tener 3 elementos")
    assertEqual(lista.copy().sort(), [1, 2, 3])
})

test("lanza un error", () => {
    var e = assertThrows(() => { throw Error("boom") })
    assertEqual(e.message, "boom")
})
```

| Función | Qué comprueba |
|---|---|
| `test(nombre, funcion)` | ejecuta la función como una prueba con nombre (también `async`) |
| `assert(condicion, mensaje?)` | que la condición sea verdadera |
| `assertEqual(obtenido, esperado, mensaje?)` | que dos valores sean iguales (los números, con una pequeña tolerancia) |
| `assertThrows(funcion, mensaje?)` | que la función lance un error (y lo devuelve) |

## Ejecutar las pruebas

```text
hyper test                 ejecuta cada .hz de la carpeta tests/
hyper test otra/carpeta    o de otra carpeta
hyper archivo.hz           también se pueden ejecutar sueltas
```

`hyper test` ejecuta cada archivo por separado y resume los resultados. Los archivos que empiezan por `_` se consideran auxiliares (se importan desde otras pruebas) y no se ejecutan solos.

## Comparar la salida completa: archivos `.out`

Si junto a `juego.hz` hay un `juego.out`, `hyper test` compara la salida del programa línea a línea con ese archivo. Es perfecto para programas que imprimen mucho (juegos que se juegan solos, informes...):

```text
hyper test --update        crea o actualiza los .out con la salida actual
```

Revisa el `.out` la primera vez para asegurarte de que la salida es la correcta.

## Pruebas en VS Code

**HyperEZ: Ejecutar pruebas** (en la paleta de comandos) lanza `hyper test` en la carpeta del proyecto. Las pruebas también se pueden ejecutar como tarea (`Terminal → Ejecutar tarea → hyper test`).
