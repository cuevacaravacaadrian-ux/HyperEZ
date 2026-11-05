# 1. Léxico

Cómo se escriben las piezas más pequeñas de un programa.

## Archivos

Los programas son archivos de texto UTF-8 con extensión `.hez` (o `.hz`). Las sentencias terminan con el salto de línea; para poner varias en una línea se separan con `;`.

```hz
var a = 1; var b = 2
print(a + b)
```

Una expresión puede continuar en la línea siguiente si la línea queda "abierta": un paréntesis, corchete o llave sin cerrar, o un operador al final.

```hz
var total = 1 +
    2 +
    3
var lista = [
    "uno",
    "dos",
]
print(total, lista)
```

## Comentarios

```hz
// comentario hasta el final de la línea
/* comentario
   de varias líneas */
/// Comentario de documentación: VS Code lo muestra al pasar el ratón sobre lo que va debajo.
function saludo(): text { return "hola" }
print(saludo())
```

## La línea `#!`

En Linux y macOS, si la primera línea empieza por `#!`, se ignora. Así un programa puede ejecutarse directamente (`chmod +x programa.hz` y `./programa.hz`):

```text
#!/usr/bin/env hyper
print("Soy un script ejecutable")
```

## Nombres

Los nombres de variables, funciones y clases empiezan por una letra o `_` y siguen con letras, dígitos o `_`. Se pueden usar letras con acento y la ñ. Se distingue entre mayúsculas y minúsculas.

```hz
var añoActual = 2026
var _interno = true
var Nivel2 = "jefe"
print(añoActual, _interno, Nivel2)
```

Convenciones: `camelCase` para variables y funciones, `PascalCase` para clases y enums, `MAYÚSCULAS` para constantes.

## Palabras clave

Estas palabras están reservadas y no se pueden usar como nombres:

```text
and       as*       async     await     break     catch     class     const
constructor         continue  do*       else      emit*     enum      event*    extends
false     finally   for       foreach   from*     function  get*      if
implements          import    in        interface is        let       match
new       not       null      on*       once*     or        priority* private
protected public    repeat    return    set*      static    step*     struct
super     this      throw     true      try       var       while
```

Las marcadas con `*` solo son especiales en su contexto (por ejemplo `get` delante de un método): en el resto se pueden usar como nombres.

## Números

Todos los números son de 64 bits con decimales (como `double`); los enteros son exactos hasta 2⁵³.

```hz
print(42, -7, 3.14, 1e6, 2.5e-3)   // decimales y notación científica
print(0xFF, 0b1010, 0o17)          // hexadecimal, binario y octal
print(1_000_000)                   // guiones bajos para leer mejor
```

## Textos

Los textos van entre comillas dobles o simples. Dentro, `${expresión}` inserta el valor de cualquier expresión.

```hz
var nombre = "Ana"
print("Hola, ${nombre}. Dentro de un año tendrás ${30 + 1} años.")
print('Comillas simples: "dobles" dentro sin escapar')
```

### Dar formato dentro de `${...}`

Detrás de la expresión, después de `:`, puede ir un **formato** (el mismo que el de Python): decimales, ancho, alineación, separador de miles, porcentaje...

```hz
var precio = 1234.5
print("Total: ${precio:.2f} €")          // Total: 1234.50 €
print("Con miles: ${precio:,.2f}")       // Con miles: 1,234.50
print("[${"Ana":<8}] [${"Ana":>8}] [${"Ana":^9}]")
print("${0.256:.1%} · ${255:x} · ${5:08b} · ${7:03}")   // 25.6% · ff · 00000101 · 007
foreach fila in [["Espada", 120], ["Poción", 15.5]] {
    print("${fila[0]:<10}${fila[1]:>8.2f}")              // una tabla alineada
}
```

| Formato | Qué hace | Ejemplo |
|---|---|---|
| `.2f` | número con 2 decimales | `${3.14159:.2f}` → `3.14` |
| `10` · `>10` · `<10` · `^10` | ancho 10 (los números a la derecha, los textos a la izquierda) · a la derecha · a la izquierda · centrado | `${"Ana":^7}` → `  Ana  ` |
| `*^10` | rellena con otro carácter | `${"Ana":*^7}` → `**Ana**` |
| `05` | rellena con ceros | `${42:05}` → `00042` |
| `,` · `_` | separa los miles | `${1234567:,}` → `1,234,567` |
| `+` | pone el signo también a los positivos | `${42:+}` → `+42` |
| `%` · `.1%` | porcentaje | `${0.256:.1%}` → `25.6%` |
| `d` | entero (redondea) | `${3.7:d}` → `4` |
| `x` `X` `b` `o` | hexadecimal, binario y octal | `${255:x}` → `ff` |
| `e` | notación científica | `${12345.6:.2e}` → `1.23e+04` |
| `.3` en un texto | como mucho 3 caracteres | `${"HyperEZ":.5}` → `Hyper` |

Si la expresión lleva una condición `a ? b : c`, ponla entre paréntesis para darle formato: `${(ok ? 1 : 0):>3}`. La función `format(valor, "formato")` hace lo mismo fuera de un texto, y `numero.format(2)` escribe los números al estilo español (`1.234,50`).

| Escape | Significado |
|---|---|
| `\n` | salto de línea |
| `\t` | tabulador |
| `\r` | retorno de carro |
| `\0` | carácter nulo |
| `\\` | barra invertida |
| `\"` `\'` | comillas |
| `\$` | un `$` literal (para escribir `${` sin interpolar) |
| `\u{1F600}` | carácter Unicode por su código |

```hz
print("Línea 1\nLínea 2\tcon tabulador \u{2764} y un \$ suelto")
```

### Textos de varias líneas

Entre tres comillas, el texto puede ocupar varias líneas (también admite `${...}`):

```hz
var poema = """
En un lugar
de la Mancha
"""
print(poema.trim().lines().length)
```

### Textos sin escapes: `r"..."`

Con una `r` delante, las barras invertidas se quedan tal cual y no hay interpolación. Es lo cómodo para expresiones regulares y rutas de Windows:

```hz
print(r"C:\Users\ana\juego.hz")
print(Regex(r"\d+").findAll("1 22 333").length)
```
