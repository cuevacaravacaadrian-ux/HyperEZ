# Patrones de texto: `Regex`

Una expresión regular describe un patrón de texto: "uno o más dígitos", "una palabra seguida de @"... HyperEZ usa la sintaxis de JavaScript. Escribe los patrones con `r"..."` para no tener que duplicar las barras invertidas.

```hz
var numero = Regex(r"\d+")
print(numero.test("tengo 3 gatos"))               // ¿aparece?
print(numero.find("año 2026").text)                // primera coincidencia
print(numero.findAll("1, 22 y 333").map(m => m.text))
print(numero.replace("x1 y22", "#"))
print(Regex(r"\s*,\s*").split("a , b,c"))
```

## Coincidencias

`find` devuelve `{text, start, end, groups}` (o `null`); las posiciones se cuentan en caracteres. `groups` tiene lo que capturan los paréntesis:

```hz
var fecha = Regex(r"(\d{2})/(\d{2})/(\d{4})")
var m = fecha.find("Nací el 25/12/2001.")
print(m.text, m.start, m.groups)
print(fecha.replace("25/12/2001", "$3-$2-$1"))   // $1, $2... son los grupos
```

El reemplazo también puede ser una función que recibe cada coincidencia:

```hz
print(Regex(r"\d+").replace("3 manzanas y 12 peras", m => str(toNumber(m.text) * 2)))
```

## Validar

`matches` comprueba que el texto **entero** encaja:

```hz
var email = Regex(r"^[\w.+-]+@[\w-]+\.[a-z]{2,}$", "i")   // "i": sin distinguir mayúsculas
print(email.matches("ana@correo.es"), email.matches("no es un email"))
```

Opciones (segundo argumento): `"i"` ignora mayúsculas, `"m"` hace que `^` y `$` funcionen en cada línea.

## Atajos en los textos

```hz
print("v1.2.3".matches(r"\d"), "v1.2.3".replaceRegex(r"\d", "#"))
print(Regex.escape("1+1=2?"))   // para buscar un texto literal con caracteres especiales
```

## Chuleta

| Patrón | Significa |
|---|---|
| `.` | cualquier carácter |
| `\d` `\w` `\s` | dígito, letra/dígito/`_`, espacio |
| `\D` `\W` `\S` | lo contrario |
| `[abc]` `[^abc]` `[a-z]` | uno de esos / ninguno de esos / un intervalo |
| `x*` `x+` `x?` `x{2,4}` | 0 o más, 1 o más, 0 o 1, entre 2 y 4 |
| `^` `$` | principio y final |
| `(...)` | grupo (se captura) |
| `a\|b` | a o b |
| `\b` | límite de palabra |

Nota: las clases como `\w` solo reconocen letras sin acento; para palabras en español usa por ejemplo `[^\s.,;:!?]+`.
