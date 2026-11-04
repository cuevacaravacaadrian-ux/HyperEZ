# Textos, números, fechas y documentos

## Textos

```hz
var t = "¡Programar en español es genial!"
print(t.removeAccents())                 // ¡Programar en espanol es genial!
print(t.slug())                          // programar-en-espanol-es-genial  (para direcciones web y archivos)
print("hola mundo".title())              // Hola Mundo
print("nombre del jugador".camelCase(), "nombre del jugador".snakeCase(), "Nombre Jugador".kebabCase())
print("Una frase muy larga".truncate(10))                 // Una frase…
print("uno dos tres cuatro cinco".wrap(10))               // reparte en líneas de 10
print("gato".distance("pato"), "Hola".similarity("hola"))  // 1 1
print("<b>negrita</b>".between("<b>", "</b>"), "   ".isBlank())
print("  a\n    b".dedent(), "x".indent(2))
print("a-b-c".indexOf("-", 2), chr(241), ord("A"), "Añ".codes())
```

`similarity` (de 0 a 1) sirve para sugerir "¿quisiste decir...?" comparando lo que escribió el usuario con las opciones válidas.

## Números

```hz
print((1234567.891).format(2))           // 1.234.567,89  (estilo español)
print((1234567).format(0, ",", "."))     // 1,234,567     (estilo inglés)
print((0.256).toPercent(1))              // 25,6 %
print((2026).toRoman(), (2026).digits(), (7).isOdd(), (15).clamp(0, 10), (5).between(1, 10))
(3).times(i => print("vuelta", i))
print(gcd(12, 18), lcm(4, 6), factorial(5), isPrime(97), primes(20), divisors(12))
print(cbrt(27), trunc(-2.7), fract(3.25), sinh(1), cosh(1), tanh(1))
randomSeed(1)
print(randomGaussian(170, 8))            // estatura aleatoria con distribución normal
print(noise(0.5), noise(0.6))            // ruido de Perlin: cambia suavemente (terrenos, nubes)
```

## Fechas

```hz
var t = Time.make(2026, 1, 31, 18, 30)
print(Time.format(Time.addDays(t, 1), "%d/%m/%Y"))     // 01/02/2026
print(Time.format(Time.addMonths(t, 1), "%d/%m/%Y"))   // 28/02/2026 (último día de febrero)
print(Time.daysBetween(Time.make(2026, 1, 1), t))      // 30
print(Time.monthName(11), Time.dayName(3), Time.isLeapYear(2028), Time.daysInMonth(2026, 2))
print(Time.parse("2026-11-04 18:30") != null, Time.parse("31/02/2026"))   // true null
print(Time.duration(3725), Time.ago(Time.now() - 300))                    // 1 h 2 min 5 s · hace 5 minutos
print(Time.today() <= Time.now())
```

## Plantillas: `import template`

Rellenan un texto con datos: correos, informes, páginas web...

```hz
import template

var t = "Hola, {{nombre}}. Tienes {{mensajes.length}} mensajes:\n{{#each mensajes}}\n- {{asunto}}{{#if @last}} (el último){{/if}}\n{{/each}}"
print(template.render(t, { nombre: "Ana", mensajes: [{ asunto: "Reunión" }, { asunto: "Fotos" }] }))
print(template.renderHtml("<p>{{texto}}</p>", { texto: "<script>" }))   // escapa para HTML
```

| Etiqueta | |
|---|---|
| `{{nombre}}`, `{{a.b.c}}`, `{{lista.length}}` | un valor (en `renderHtml` se escapa; `{{{x}}}` no) |
| `{{#if x}} ... {{else}} ... {{/if}}` | condición: vacío, 0, `false` y `null` son falsos |
| `{{#unless x}} ... {{/unless}}` | lo contrario |
| `{{#each lista}} ... {{/each}}` | repite; dentro: `{{this}}`, `{{@index}}`, `{{@first}}`, `{{@last}}` y los campos de cada elemento. Con un mapa: `{{key}}` y `{{value}}` |
| `{{! comentario }}` | no sale en el resultado |

Una línea que solo tiene una etiqueta de bloque no deja una línea en blanco. `template.renderFile(ruta, datos)` lee la plantilla de un archivo.

## Markdown a HTML: `import markdown`

```hz
import markdown

print(markdown.toHtml("# Título\n\nTexto con **negrita**, *cursiva*, `código` y [un enlace](https://ejemplo.es).\n\n- uno\n- dos"))
var pagina = markdown.page("Mi página", "# Hola\n\n| a | b |\n|---|---|\n| 1 | 2 |")
File.write("pagina.html", pagina)        // ábrela con el navegador
```

Admite títulos, párrafos, negrita, cursiva, tachado, código (en línea y bloques con lenguaje), enlaces, imágenes, listas (también numeradas y de tareas `[ ]`/`[x]`), citas, tablas y líneas horizontales. `markdown.page` añade un estilo limpio.

## Configuración: `import ini`

```hz
import ini

var ajustes = ini.parse("[pantalla]\nancho = 800\ncompleta = no\n\n[jugador]\nnombre = \"Ana López\"")
print(ajustes.pantalla.ancho + 1, ajustes.pantalla.completa, ajustes.jugador.nombre)
ajustes.jugador.nombre = "Luis"
ini.save("ajustes.ini", ajustes)
print(ini.load("ajustes.ini").jugador.nombre)
```

Los números y `sí`/`no`/`true`/`false` se convierten solos (`ini.parse(texto, false)` lo deja todo como texto). Las líneas que empiezan por `;` o `#` son comentarios.

## Registro de mensajes: `import log`

```hz
import log

var registro = log.Logger("servidor", "info")      // niveles: debug, info, warn, error
registro.info("Arrancando en el puerto", 8080)
registro.warn("Queda poco espacio")
registro.debug("no se ve: el nivel es info")
var r = registro.time("calcular", () => range(100000).toList().sum())
print(registro.recent().length)
```

Con un tercer argumento (`log.Logger("juego", "debug", "registro.txt")`) también escribe en un archivo, con fecha. En la consola los niveles salen en colores.

## JSON en archivos

```hz
JSON.save("partida.json", { nivel: 3, vidas: 2 })    // con sangría, legible
print(JSON.load("partida.json").nivel)
```
