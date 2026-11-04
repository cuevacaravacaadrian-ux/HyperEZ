# Hacer webs: páginas, aplicaciones, APIs y bases de datos

HyperEZ trae todo lo necesario para hacer webs sin instalar nada más: un servidor web (`Http.serve`), rutas, plantillas, formularios y cookies (`import web`), webs enteras a partir de Markdown (`import site`), un generador de HTML (`import html`), una base de datos de documentos (`import db`) y un servidor para desarrollar que **recarga el navegador cada vez que guardas** (`hyper serve`).

## Una web en un minuto

```text
hyper new mi-web --tipo web        aplicación: plantillas HTML, un formulario, una API JSON y CSS
cd mi-web
hyper main.hez --abrir             abre http://localhost:8080 en el navegador
```

```text
hyper new mi-blog --tipo sitio     sitio estático: cada archivo .md de contenido/ es una página
cd mi-blog
hyper main.hez                     genera la web en web/
hyper serve web --abrir            la sirve y recarga el navegador al guardar
```

Mientras escribes un sitio, deja en una terminal `hyper watch main.hez` (regenera las páginas al guardar) y en otra `hyper serve web`: cada cambio se ve en el navegador al momento. La carpeta `web/` generada se puede subir tal cual a cualquier alojamiento de páginas estáticas.

## `hyper serve`: servidor para desarrollar

```text
hyper serve                 sirve la carpeta actual en http://localhost:8080
hyper serve web --abrir     otra carpeta, y abre el navegador
hyper serve --puerto 3000   otro puerto
hyper serve --red           también para el móvil y otros equipos de tu red (con la IP de este equipo)
hyper serve --sin-recarga   sin la recarga automática
```

Sirve los archivos con su tipo (HTML, CSS, JavaScript, imágenes, fuentes...), las carpetas con su `index.html`, y añade a cada página un pequeño script que la recarga cuando cambia cualquier archivo de la carpeta. Muestra cada petición con la hora. Lo mismo desde un programa: `web.devServer("web", { open: true })`.

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

## Archivos estáticos, plantillas y formularios

```hz
import web

var carpeta = Path.join(Path.temp, "mi-web-" + Hash.uuid())
File.write(Path.join(carpeta, "publico", "estilo.css"), "body { font-family: sans-serif }")
File.write(Path.join(carpeta, "base.html"), "<html><head><link rel=\"stylesheet\" href=\"/publico/estilo.css\"></head><body>{{{contenido}}}</body></html>")
File.write(Path.join(carpeta, "inicio.html"), "<h1>Hola, {{nombre}}</h1>{{#each tareas}}<p>{{this}}</p>{{/each}}")

var tareas = ["comprar pan"]
var app = web.App()
app.staticDir("/publico", Path.join(carpeta, "publico"))          // /publico/estilo.css
app.get("/", req => web.view(Path.join(carpeta, "inicio.html"), { nombre: "Ana", tareas: tareas }, Path.join(carpeta, "base.html")))
app.post("/tareas", req => {
    tareas.add(web.form(req).texto)                                // <form method="post"> con <input name="texto">
    return web.redirect("/")
})
app.get("/api/tareas", req => tareas)                              // una lista o un mapa: JSON
app.get("/api/tareas/:n", req => {
    var n = toNumber(req.params.n)
    return n < tareas.length ? { tarea: tareas[n] } : web.json({ error: "no existe" }, 404)
})

// se prueba sin servidor; con app.listen(8080, { open: true }) se sirve de verdad
var envio = web.request("POST", "/tareas")
envio.body = "texto=regar+las+plantas"
print(app.handle(envio).status, app.handle(web.request("GET", "/api/tareas")))
print(app.handle(web.request("GET", "/")).body)
File.deleteDir(carpeta)
```

| Función | Qué hace |
|---|---|
| `app.get/post/put/delete/patch/all(patron, f)` | rutas; `:nombre` en `req.params`, `*` recoge el resto |
| `app.use(f)` | middleware: si devuelve algo, esa es la respuesta (`app.use(web.logger())` muestra cada petición) |
| `app.staticDir(prefijo, carpeta)` | archivos de una carpeta (las carpetas sirven su `index.html`) |
| `app.listen(puerto, { open: true })` | arranca el servidor (y abre el navegador) |
| `web.view(plantilla, datos, base?)` | página a partir de una plantilla con `{{variables}}` (se escapan); `base` la envuelve en `{{{contenido}}}` |
| `web.form(req)` · `web.cookies(req)` · `web.decode(t)` | campos de un formulario, cookies y textos de una URL |
| `web.json(d, status)` · `web.html(t, status)` · `web.text(t)` · `web.file(ruta)` · `web.error(status, msg)` · `web.redirect(url)` | respuestas |
| `web.withCookie(resp, nombre, valor, { maxAge })` | añade una cookie a una respuesta |

## Webs a partir de Markdown: `import site`

Para blogs, documentación o portfolios no hace falta programar HTML: cada archivo `.md` es una página, con menú, estilo y diseño adaptable al móvil. Esta documentación, en su versión HTML, está hecha así.

```hz
import site

var origen = Path.join(Path.temp, "contenido-" + Hash.uuid())
File.write(Path.join(origen, "index.md"), "# Mi web\n\nBienvenido. Lee [sobre mí](sobre-mi.md).")
File.write(Path.join(origen, "sobre-mi.md"), "---\ntitulo: Sobre mí\norden: 2\n---\n# Sobre mí\n\n```hz\nprint(\"¡Hola!\")\n```")
var destino = Path.join(Path.temp, "web-" + Hash.uuid())
var hechos = site.build(origen, destino, { title: "Mi web", color: "#2f81f7", footer: "Hecho con HyperEZ" })
print(hechos.length, "archivos")
print(File.read(Path.join(destino, "index.html")).contains("sobre-mi.html"))
File.deleteDir(origen)
File.deleteDir(destino)
```

- Las páginas pueden empezar con datos entre `---`: `titulo`, `orden` (posición en el menú) y `oculta: true` (fuera del menú).
- Los enlaces a otras páginas `.md` pasan a ser `.html`; las imágenes y demás archivos se copian.
- Los bloques de código ` ```hz ` salen con colores.
- Opciones: `title`, `color`, `footer`, `index` (otra portada, p. ej. `"README.md"`), `menu` (un menú propio con grupos) y `layout` (tu propia plantilla HTML).
