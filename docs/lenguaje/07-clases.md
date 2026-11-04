# 7. Clases y tipos propios

## Clases

```hz
class Personaje {
    var nombre: text            // campo (con tipo)
    var vida: number = 10       // campo con valor inicial
    static var creados = 0      // compartido por todos los Personaje

    constructor(nombre: text) {
        this.nombre = nombre    // this. es opcional si no hay ambigüedad
        creados += 1
    }

    get vivo(): bool { return vida > 0 }            // propiedad calculada (se lee sin paréntesis)
    function recibir(n: number) { vida = max(0, vida - n) }
    function toString(): text { return "${nombre} (${vida})" }  // cómo se imprime
}

var ana = Personaje("Ana")
ana.recibir(4)
print(ana, ana.vivo, Personaje.creados)
```

- Los objetos se crean llamando a la clase como una función: `Personaje("Ana")`.
- `toString()` decide cómo se ve el objeto en `print` y en `"${...}"`.

### Propiedades con `get` y `set`

```hz
class Termometro {
    var celsius: number = 0
    get fahrenheit(): number { return celsius * 9 / 5 + 32 }
    set fahrenheit(f: number) { celsius = (f - 32) * 5 / 9 }
}
var t = Termometro()
t.fahrenheit = 212
print(t.celsius, t.fahrenheit)
```

### Miembros estáticos

```hz
class Id {
    static var siguiente = 1
    static function nuevo(): number {
        siguiente += 1
        return siguiente - 1
    }
}
print(Id.nuevo(), Id.nuevo())
```

### Visibilidad

`private` delante de un campo o método lo oculta fuera de la clase (y fuera del archivo para declaraciones globales).

## Herencia

```hz
class Animal {
    var nombre: text
    constructor(nombre: text) { this.nombre = nombre }
    function sonido(): text { return "..." }
    function hablar(): text { return "${nombre} dice ${sonido()}" }
}

class Perro extends Animal {
    var raza: text = "mestizo"
    constructor(nombre: text, raza: text) {
        super(nombre)              // constructor de la clase base
        this.raza = raza
    }
    function sonido(): text { return "¡guau!" }  // reemplaza el método de Animal
    function hablar(): text { return super.hablar() + " (" + raza + ")" }
}

var p = Perro("Toby", "beagle")
print(p.hablar(), p is Animal, p is Perro)
```

## Structs

Un `struct` es una clase pensada para datos: tiene un constructor automático con sus campos en orden y se compara **por valor**.

```hz
struct Punto { var x: number = 0; var y: number = 0 }
var a = Punto(1, 2)
var b = Punto(1, 2)
print(a == b, a, Punto(y: 5))
```

## Interfaces

Una interfaz enumera los métodos que debe tener una clase. HyperEZ comprueba que la clase los implementa.

```hz
interface Dibujable {
    function dibujar(): text
}
class Cuadro implements Dibujable {
    function dibujar(): text { return "[]" }
}
var cosas: list = [Cuadro()]
foreach c in cosas { print(c.dibujar(), c is Dibujable) }
```

## Enums

```hz
enum Estado { Menu, Jugando, Pausa }
var e = Estado.Jugando
print(e, e.name, e.ordinal, Estado.count)

enum Dificultad { Facil = 1, Normal = 2, Dificil = 3 }   // valores propios (números o textos)
print(Dificultad.Dificil.value)

var mensaje = match e {
    Estado.Menu => "en el menú"
    Estado.Jugando => "jugando"
    else => "en pausa"
}
print(mensaje)
```

## Sobrecarga de operadores

Una clase (o un `struct`) puede definir qué hacen los operadores con sus objetos mediante métodos con estos nombres:

| Operador | Método |
|---|---|
| `a + b` `a - b` `a * b` `a / b` `a % b` `a ** b` | `add(b)` `sub(b)` `mul(b)` `div(b)` `mod(b)` `pow(b)` |
| `n + a`, `n * a` (número a la izquierda) | `add(n)`, `mul(n)` del objeto |
| `<` `<=` `>` `>=` | `compareTo(b)`: devuelve un número negativo, 0 o positivo |
| `==` `!=` | `equals(b)` |
| `a[i]`, `a[i] = v` | `get(i)`, `set(i, v)` |
| `len(a)` | propiedad `length` |

```hz
class Dinero {
    var centimos: number
    constructor(euros: number) { centimos = round(euros * 100) }
    function add(o: Dinero): Dinero { return Dinero((centimos + o.centimos) / 100) }
    function mul(n: number): Dinero { return Dinero(centimos * n / 100) }
    function compareTo(o: Dinero): number { return centimos - o.centimos }
    function equals(o: Dinero): bool { return centimos == o.centimos }
    function toString(): text { return (centimos / 100).toFixed(2) + " €" }
}
var precio = Dinero(2.5)
var total = precio * 3 + Dinero(1)
print(total, total > precio, Dinero(1) == Dinero(1))
```

Las clases nativas usan el mismo mecanismo: por eso `Matrix` y `Vec2` se suman con `+`.
