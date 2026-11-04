# Biblioteca estándar

HyperEZ trae de serie todo lo necesario para empezar en cualquier área. La [referencia completa de la API](API.md) (generada a partir del código) lista cada función con su firma; estas guías explican cómo se usa cada área.

## Mapa

| Área | Módulos y clases | Guía |
|---|---|---|
| Texto, listas, mapas, matemáticas | funciones globales, `text`, `list`, `map`, `set`, `Range` | [la referencia del lenguaje](../lenguaje/06-colecciones.md) y [API](API.md) |
| Textos, números y fechas; documentos | `text`, `number`, `Time`, `import template`, `import markdown`, `import ini`, `import log` | [textos-documentos.md](textos-documentos.md) |
| Estructuras de datos y algoritmos | `import collections` (pila, cola, cola de prioridad, grafo...), `import algo` (A*, combinatoria) | [colecciones-algoritmos.md](colecciones-algoritmos.md) |
| Sistema operativo | `Process` (también en segundo plano), `Path`, `File`, `System`, `Time` | [sistema.md](sistema.md) |
| Datos binarios, compresión y .zip | `Bytes`, `Hash`, `Base64`, `compress`, `import zip` | [binario.md](binario.md) |
| Patrones de texto | `Regex`, textos `r"..."` | [regex.md](regex.md) |
| Validar datos y formularios | `import validate` (correos, teléfonos, DNI/NIE, IBAN, tarjetas, ISBN, esquemas) | [validar.md](validar.md) |
| Internet (cliente) | `Http` | [red.md](red.md) |
| Webs, APIs y bases de datos | `Http.serve`, `import web`, `import site`, `import html`, `import db`, `hyper serve` | [web.md](web.md) |
| Gráficos, gráficas y sonido | `Image`, `import svg`, `import audio` | [graficos-sonido.md](graficos-sonido.md) |
| Inteligencia artificial y ciencia de datos | `Matrix`, `import ai`, `import stats`, `import csv` | [ia-datos.md](ia-datos.md) |
| Consola, juegos y herramientas | `Console`, `import cli`, `Vec2`, `Vec3`, `Color` | [consola.md](consola.md) |
| Sistemas operativos y bajo nivel | `Bytes`, `import x86`, operadores de bits | [sistemas-operativos.md](sistemas-operativos.md) |
| Formato de datos | `JSON`, `import csv` | [ia-datos.md](ia-datos.md#datos) |
| Pruebas | `test`, `assert`, `assertEqual`, `assertThrows` | [pruebas](../lenguaje/11-pruebas.md) |
| Eventos y tareas | `event`, `wait`, `spawn`, `awaitAll` | [eventos y tareas](../lenguaje/09-eventos-async.md) |

## Nativo o escrito en HyperEZ

- Los **módulos nativos** (`File`, `Http`, `Matrix`...) están programados en C++ dentro de `hyper` y se usan sin importar nada.
- Las **bibliotecas incluidas** (`ai`, `stats`, `csv`, `cli`, `x86`, `web`, `html`, `db`, `audio`, `collections`, `algo`, `template`, `markdown`, `ini`, `log`, `svg`, `zip`, `site`, `validate`) están escritas en el propio HyperEZ (carpeta `src/lib` del repositorio), se cargan con `import` y van incrustadas en `hyper`, así que tampoco hay que instalar nada. Leer su código es una buena forma de aprender.

```hz
import stats
var datos = [4, 8, 15, 16, 23, 42]
print(stats.mean(datos), File.exists("no-existe.txt"), Path.ext("foto.png"))
```
