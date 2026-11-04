# Validar datos

`import validate` comprueba si un dato tiene la forma correcta: correos, webs, teléfonos, documentos españoles (DNI, NIE) con su letra de control, IBAN, tarjetas, ISBN, códigos postales, fechas... Y también comprueba un **formulario o un JSON entero** con un esquema de reglas, con los mensajes de error en español.

```hz
import validate

print(validate.email("ana@ejemplo.es"))                 // true
print(validate.dni("12345678Z"))                        // true: la letra es la correcta
print(validate.dni("12345678A"))                        // false
print(validate.iban("ES91 2100 0418 4502 0005 1332"))   // true: dígitos de control (módulo 97)
print(validate.card("4539 1488 0343 6467"))             // true: algoritmo de Luhn
print(validate.dniLetter(12345678))                     // Z
```

## Comprobadores

| Función | Comprueba |
|---|---|
| `email(t)` | un correo con forma válida (`nombre@dominio.es`) |
| `url(t)` | una dirección `http://` o `https://` |
| `phone(t, pais = "ES")` | un teléfono español (9 cifras que empiezan por 6, 7, 8 o 9, con `+34` opcional); con otro país, de 7 a 15 cifras |
| `dni(t)` · `nie(t)` · `nif(t)` | DNI (`12345678Z`), NIE (`X1234567L`) o cualquiera de los dos, con la letra de control |
| `dniLetter(numero)` | la letra que le corresponde a un número de DNI |
| `iban(t)` | un IBAN: país, longitud y dígitos de control |
| `card(t)` | un número de tarjeta (12 a 19 cifras, algoritmo de Luhn) |
| `isbn(t)` | un ISBN-10 o ISBN-13 |
| `postalCode(t, pais = "ES")` | un código postal español (01000 a 52999) |
| `hexColor(t)` | un color `#f80`, `#ff8800` o `#ff8800cc` |
| `date(t)` | una fecha real `AAAA-MM-DD` (con los días de cada mes y los bisiestos) |
| `strongPassword(t, minimo = 8)` | una contraseña con minúsculas, mayúsculas y números |

Los espacios, guiones y puntos del medio no importan: `"12.345.678-Z"` y `"ES91 2100 ..."` valen.

## Formularios y JSON: `check`

`validate.check(datos, reglas)` devuelve un mapa `{campo: "error"}` **solo con los campos que fallan**; si todo está bien, el mapa está vacío.

```hz
import validate

var reglas = {
    nombre: { required: true, min: 2 },
    edad: { type: "number", min: 18, max: 120 },
    correo: "email",                       // atajo de { type: "email", required: true }
    rol: { oneOf: ["admin", "jugador"] },
    alias: { pattern: r"[a-z0-9_]{3,12}", message: "solo minúsculas, números y _" },
}
var errores = validate.check({ nombre: "A", edad: "15", correo: "x@", rol: "rey", alias: "Ana!" }, reglas)
foreach campo, error in errores { print("${campo:<8} ${error}") }
```

```text
nombre   debe tener al menos 2 caracteres
edad     debe ser como mínimo 18
correo   no es un email válido
rol      debe ser uno de: admin, jugador
alias    solo minúsculas, números y _
```

| Regla | Qué pide |
|---|---|
| `required: true` | que el campo exista y no esté vacío (sin `required`, un campo vacío vale) |
| `type` | `"text"`, `"number"` (también un texto con un número, como los de un formulario), `"integer"`, `"bool"`, `"list"`, `"map"` o cualquier comprobador: `"email"`, `"url"`, `"phone"`, `"dni"`, `"nie"`, `"nif"`, `"iban"`, `"card"`, `"isbn"`, `"postalCode"`, `"hexColor"`, `"date"` |
| `min` · `max` | el valor (números) o la longitud (textos, listas y mapas) |
| `pattern` | una expresión regular que debe cumplir todo el texto |
| `oneOf` | una lista de valores permitidos |
| `custom` | una función: devuelve `true` si vale, o `false` o un texto con el error |
| `message` | el mensaje que se muestra en lugar del de serie |

También están `validate.valid(datos, reglas)` (`true` o `false`) y `validate.ensure(datos, reglas)`, que lanza un `Error` con todos los fallos juntos y, si todo está bien, devuelve los datos.

## En una web

Junto con `web.form` comprueba lo que envía un formulario antes de usarlo:

```hz
import web
import validate

var app = web.App()
app.post("/registro", req => {
    var datos = web.form(req)
    var errores = validate.check(datos, { correo: "email", edad: { type: "number", min: 18 } })
    if not errores.isEmpty() { return web.json({ errores: errores }, 422) }
    return web.html("¡Bienvenido!")
})
print(app.handle(web.request("POST", "/registro", "correo=ana@ejemplo.es&edad=20")).body)
```
