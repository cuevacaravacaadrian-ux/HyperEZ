# Aplicaciones web y bases de datos

HyperEZ trae un servidor web (`Http.serve`), un sistema de rutas (`import web`), un generador de HTML (`import html`) y una base de datos de documentos (`import db`). Con eso se hace una página o una API completa en un solo archivo.

## Un servidor en tres líneas

```text
Http.serve(8080, req => "<h1>Hola desde HyperEZ</h1><p>Has pedido ${req.path}</p>")
```

Abre `http://localhost:8080`. La función recibe la **petición** `{method, path, url, query, headers, body, json}` y devuelve la **respuesta**:

| Devuelve | Se envía |
|---|---|
| un texto | HTML |
| un mapa o una lista | JSON |
| `{status, body, json, file, type, headers}` | lo indicado (`file` envía un archivo) |
| `null` | 404, o el archivo de la carpeta `static` |

Opciones: `host` (`"127.0.0.1"` por defecto; `"0.0.0.0"` para que entren otros equipos de la red), `static` (carpeta de archivos: HTML, CSS, imágenes), `cors` (permitir llamadas desde otras webs), `quiet` (sin registro en la consola) y `maxRequests`. Ctrl+C para el servidor.

## Rutas: `import web`

```hz
import web
var app = web.App()
app.get("/", req => "<h1>Inicio</h1>")
app.get("/saludo/:nombre", req => "Hola, " + req.params.nombre)
app.post("/api/datos", req => ({ status: 201, json: { recibido: req.json } }))
app.use(req => req.path.startsWith("/admin") ? { status: 401, body: "Prohibido" } : null)

// Se puede probar sin arrancar el servidor:
print(app.handle(web.request("GET", "/saludo/Ana")))
print(app.handle(web.request("POST", "/api/datos", { x: 1 })))
print(app.handle(web.request("GET", "/admin/panel")).status)
// app.listen(8080) arranca el servidor de verdad
```

- `:nombre` captura una parte de la ruta en `req.params`; `*` al final captura el resto.
- `use(f)` se ejecuta antes de cada ruta: si devuelve algo, esa es la respuesta (autenticación, registro...).
- `web.redirect(url)` responde con una redirección.

## HTML: `import html`

```hz
import html
var filas = [["Jugador", "Puntos"], ["Ana", 120], ["<script>", 0]]
var cuerpo = html.tag("h1", {}, "Ranking") + str(html.table(filas))
var pagina = html.page("Ranking", html.raw(cuerpo))
print(pagina.contains("&lt;script&gt;"))   // el texto se escapa solo: no hay inyección de HTML
```

`tag(nombre, atributos, ...hijos)`, `list(elementos)`, `table(filas)`, `link(texto, url)`, `escape(texto)`, `raw(html)` (HTML que no se escapa) y `page(título, cuerpo, css?)` con un estilo limpio por defecto.

## Base de datos: `import db`

Documentos (mapas) agrupados en colecciones, guardados en un archivo JSON a cada cambio.

```hz
import db
var base = db.Database()                         // con una ruta se guarda en ese archivo
var jugadores = base.collection("jugadores")
jugadores.insert({ nombre: "Ana", puntos: 120, nivel: 3 })
jugadores.insertMany([{ nombre: "Luis", puntos: 95, nivel: 1 }, { nombre: "Eva", puntos: 130, nivel: 3 }])
print(jugadores.find({ nivel: 3 }).map(j => j.nombre))   // filtro por campos iguales
print(jugadores.find(j => j.puntos > 100).length)         // o con una función
jugadores.update({ nombre: "Luis" }, { puntos: 200 })
jugadores.delete(j => j.nivel < 2)
print(jugadores.count(), jugadores.findOne({ nombre: "Ana" })._id)
```

Métodos de una colección: `insert`, `insertMany`, `find`, `findOne`, `get(id)`, `update`, `delete`, `count` y `all`. Cada documento recibe un `_id` numérico.

## Todo junto

[examples/web/puntuaciones.hz](../../examples/web/puntuaciones.hz) es una aplicación de puntuaciones con página HTML, API JSON y base de datos en unas 50 líneas.
