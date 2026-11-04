# 4. Control del flujo

## `if`

Las llaves son obligatorias; los paréntesis alrededor de la condición, no.

```hz
var vida = 2
if vida <= 0 {
    print("Fin")
} else if vida == 1 {
    print("¡Cuidado!")
} else {
    print("Todo bien")
}
```

## `while`

```hz
var n = 1
while n < 100 {
    n *= 2
}
print(n) // 128
```

## `for` con números

`for i in a..b` cuenta desde `a` hasta `b` sin incluirlo; `..=` lo incluye; `step` elige el salto (también negativo).

```hz
for i in 0..3 { print("i =", i) }
for i in 1..=3 { print("hasta 3 incluido:", i) }
for i in 10..0 step -5 { print("cuenta atrás:", i) }
```

## `foreach` y `for` con listas

```hz
var frutas = ["pera", "uva"]
foreach f in frutas { print(f) }
for i, f in frutas { print(i, f) }             // con índice
foreach clave, valor in { a: 1, b: 2 } { print(clave, valor) }
foreach letra in "hola" { print(letra) }        // los textos se recorren por caracteres
```

## `repeat`

Repite un bloque un número de veces:

```hz
repeat 3 { print("¡hola!") }
```

## `break` y `continue`

```hz
for i in 0..10 {
    if i % 2 == 0 { continue } // salta al siguiente
    if i > 6 { break }         // sale del bucle
    print(i)                   // 1 3 5
}
```

## `match`

`match` compara un valor con varios casos y devuelve el resultado del primero que encaja. Cada caso puede tener:

- uno o varios valores separados por comas;
- rangos (`1..5`, `1..=5`);
- clases o tipos (`is`-test implícito al poner una clase);
- una guarda `else if condición`;
- un bloque `{ ... }`: su última expresión es el resultado.

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
    "space" => {
        print("¡salto!")
        "saltar"
    }
    "escape" => "salir"
    else => "nada"
}
print(accion)
```

`match` también se usa como sentencia, sin aprovechar el resultado:

```hz
var estado = "jugando"
match estado {
    "menu" => print("Pulsa una tecla")
    "jugando" => print("¡A jugar!")
    else => print("...")
}
```

Con `match true` se escribe una cadena de condiciones de forma compacta:

```hz
var temperatura = 31
print(match true {
    temperatura < 0 => "hiela"
    temperatura < 25 => "templado"
    else => "calor"
})
```
