# 2. Variables y tipos

## Declarar variables

| Forma | Significado |
|---|---|
| `var x = 1` | variable normal: se puede cambiar |
| `let x = 1` | se asigna una sola vez (dar otro valor es un error) |
| `const X = 1` | constante: necesita valor y no cambia nunca |

```hz
var vidas = 3
vidas = vidas - 1
let semilla = 1234
const MAX_VIDAS = 5
print(vidas, semilla, MAX_VIDAS)
```

Una variable declarada sin valor empieza en `null`. Las variables existen dentro del bloque `{ }` donde se declaran.

```hz
var resultado
print(resultado) // null
if true {
    var dentro = "solo existe aquí"
    print(dentro)
}
```

## Tipos

Los tipos son **opcionales** (tipado gradual): si los escribes, HyperEZ comprueba los errores antes de ejecutar (`hyper check`, y en VS Code mientras escribes); si no, el programa funciona igual.

```hz
var puntos: number = 0
var nombre: text = "Ana"
var activo: bool = true
var notas: list<number> = [7, 8.5]
var edades: map = { ana: 30 }
print(puntos, nombre, activo, notas, edades)
```

| Tipo | Valores |
|---|---|
| `number` | números (con o sin decimales) |
| `int` | números sin decimales (se comprueba al ejecutar) |
| `text` | textos |
| `bool` | `true` y `false` |
| `list`, `list<T>` | listas (de elementos de tipo `T`) |
| `map` | mapas clave → valor |
| `set` | conjuntos |
| `function` | funciones y lambdas |
| `any` | cualquier cosa (desactiva las comprobaciones) |
| `void` | "no devuelve nada" (solo en funciones) |
| `Task<T>` | resultado de una función `async` |
| Nombres de clases | `Jugador`, `Vec2`, `Matrix`, `Bytes`... |

Aunque no escribas el tipo, HyperEZ lo **deduce** del valor inicial: `var x = 5` es `number`, y después `x = "hola"` se marca como error.

### `null` y los tipos con `?`

Un tipo con `?` admite además `null`:

```hz
var jefe: text? = null
jefe = "Rey Esqueleto"
print(jefe ?? "sin jefe")
```

## Saber el tipo de un valor

```hz
print(typeof(3), typeof("a"), typeof([1]), typeof({}), typeof(null), typeof(print))
var x: any = 3.5
print(x is number, x is text, x is not text)
```

`is` también funciona con clases (`enemigo is Jefe`) e interfaces.

## Convertir entre tipos

```hz
print(toNumber("42") + 1)        // texto -> número (null si no es un número)
print(toNumber("hola"))
print(str(3.5) + "!")            // cualquier valor -> texto
print(toInt(7.9), round(7.5), floor(7.9), ceil(7.1))
print(parseInt("ff", 16), (255).toHex(), (5).toBinary(8))
print((3.14159).toFixed(2))
```

## Valores verdaderos y falsos

En una condición (`if`, `while`, `and`...) cuentan como **falsos** `false`, `null`, `0` y `""`. Todo lo demás es verdadero.

```hz
var lista = []
if not lista.isEmpty() { print("hay cosas") } else { print("vacía") }
if "texto" { print("un texto no vacío es verdadero") }
```
