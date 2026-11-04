// Tipos de proyecto de 'hyper new': cada uno con la misma estructura de carpetas
//   main.hz      punto de entrada
//   src/         módulos del proyecto (se importan con from "./src/modulo.hz" import ...)
//   tests/       pruebas (hyper test)
//   datos/       archivos que usa el programa (solo si hacen falta)
#include "tools/tools.h"

namespace hz {

static std::string readme(const std::string& title, const std::string& what, const std::string& extra) {
    return "# " + title + "\n\n" + what + "\n\nProyecto creado con [HyperEZ](" HYPEREZ_REPO_URL ") " HYPEREZ_VERSION ".\n\n"
           "## Carpetas\n\n| Carpeta | Contenido |\n|---|---|\n"
           "| `main.hz` | Punto de entrada del programa |\n"
           "| `src/` | Módulos del proyecto |\n"
           "| `tests/` | Pruebas (`hyper test`) |\n" + extra +
           "\n## Comandos\n\n| Comando | Qué hace |\n|---|---|\n"
           "| `hyper run` | Ejecuta `main.hz` |\n"
           "| `hyper watch` | Ejecuta `main.hz` y lo repite cada vez que guardas |\n"
           "| `hyper test` | Ejecuta las pruebas de `tests/` |\n"
           "| `hyper check` | Busca errores sin ejecutar |\n"
           "| `hyper` | Consola interactiva para probar código |\n\n"
           "En VS Code: **F5** ejecuta y **Ctrl+Alt+F5** ejecuta cada vez que guardas.\n";
}

// ---- juego -----------------------------------------------------------------

static std::vector<ProjectFile> gameProject(const std::string& title) {
    std::string main = projectTemplateMain(title);
    std::string test = projectTemplateTest();
    auto fix = [](std::string& s, const std::string& from, const std::string& to) {
        size_t p = s.find(from);
        if (p != std::string::npos) s.replace(p, from.size(), to);
    };
    fix(main, "from \"./jugador.hz\"", "from \"./src/jugador.hz\"");
    fix(test, "from \"../jugador.hz\"", "from \"../src/jugador.hz\"");
    return {
        {"main.hz", main},
        {"src/jugador.hz", projectTemplateModule()},
        {"tests/test_jugador.hz", test},
        {"README.md", readme(title, "Un juego de consola: lleva al héroe hasta el tesoro esquivando las trampas.", "")},
    };
}

// ---- aplicación de consola -------------------------------------------------

static std::vector<ProjectFile> appProject(const std::string& title) {
    return {
        {"main.hz", "// " + title + R"HZ( — aplicación de consola: lista de tareas que se guarda en datos/tareas.json
import cli
from "./src/tareas.hz" import ListaTareas

var lista = ListaTareas(Path.join(System.scriptDir, "datos/tareas.json"))

if not Console.interactive {
    // sin teclado (por ejemplo en las pruebas): una pequeña demostración
    print(lista.resumen())
    Process.exit(0)
}

var seguir = true
while seguir {
    print()
    print(lista.resumen())
    print("1) Añadir  2) Completar  3) Borrar completadas  4) Salir")
    match cli.ask("Elige", "4") {
        "1" => lista.agregar(cli.ask("Tarea"))
        "2" => lista.completar(toNumber(cli.ask("Número")) - 1)
        "3" => print("Borradas: ${lista.limpiar()}")
        else => seguir = false
    }
}
)HZ"},
        {"src/tareas.hz", R"HZ(// Lógica de la lista de tareas (separada de main.hz para poder probarla).

struct Tarea { var texto: text = ""; var hecha: bool = false }

class ListaTareas {
    var tareas: list = []
    var archivo: text

    constructor(archivo: text) {
        this.archivo = archivo
        if File.exists(archivo) {
            tareas = JSON.parse(File.read(archivo)).map(t => Tarea(t.texto, t.hecha))
        }
    }

    function agregar(texto: text) {
        if texto.trim() == "" { return }
        tareas.add(Tarea(texto.trim(), false))
        guardar()
    }

    function completar(i: number): bool {
        if i < 0 or i >= tareas.length { return false }
        tareas[i].hecha = true
        guardar()
        return true
    }

    /// Borra las tareas hechas y devuelve cuántas borró.
    function limpiar(): number {
        var antes = tareas.length
        tareas = tareas.filter(t => not t.hecha)
        guardar()
        return antes - tareas.length
    }

    get pendientes(): number { return tareas.count(t => not t.hecha) }

    function resumen(): text {
        if tareas.isEmpty() { return "No hay tareas." }
        var lineas = []
        foreach i, t in tareas { lineas.add("${i + 1}. [${t.hecha ? "x" : " "}] ${t.texto}") }
        return lineas.join("\n") + "\n${pendientes} pendiente(s)"
    }

    function guardar() {
        File.write(archivo, JSON.stringify(tareas.map(t => ({ texto: t.texto, hecha: t.hecha })), 2))
    }
}
)HZ"},
        {"tests/test_tareas.hz", R"HZ(from "../src/tareas.hz" import ListaTareas

test("añadir, completar y limpiar", () => {
    var ruta = Path.join(Path.temp, "tareas_prueba.json")
    File.delete(ruta)
    var l = ListaTareas(ruta)
    l.agregar("comprar pan")
    l.agregar("estudiar")
    assertEqual(l.pendientes, 2)
    l.completar(0)
    assertEqual(l.pendientes, 1)
    assertEqual(ListaTareas(ruta).tareas.length, 2) // se guardó en el archivo
    assertEqual(l.limpiar(), 1)
    File.delete(ruta)
})
)HZ"},
        {"datos/.gitkeep", ""},
        {"README.md", readme(title, "Aplicación de consola: una lista de tareas que se guarda en un archivo JSON.",
                             "| `datos/` | Archivos del programa (`tareas.json`) |\n")},
    };
}

// ---- inteligencia artificial ----------------------------------------------

static std::vector<ProjectFile> aiProject(const std::string& title) {
    return {
        {"main.hz", "// " + title + R"HZ( — inteligencia artificial: una red neuronal aprende a clasificar flores.
import ai
from "./src/datos.hz" import cargar

randomSeed(1)
var datos = cargar(Path.join(System.scriptDir, "datos/flores.csv"))
print("Ejemplos: ${datos.entradas.length}, clases: ${datos.nombres}")

var red = ai.NeuralNetwork([2, 8, datos.nombres.length], "relu", "softmax")
red.train(datos.entradas, ai.oneHot(datos.clases, datos.nombres.length), 500, 0.2, 0, true)

var acierto = ai.accuracy(red.classify(datos.entradas), datos.clases)
print("Aciertos: ${round(acierto * 100)} %")
foreach nueva in [[1.4, 0.2], [4.5, 1.5], [6.0, 2.2]] {
    print("Una flor con pétalo ${nueva[0]} x ${nueva[1]} cm es: ${datos.nombres[red.classify([datos.escalar(nueva)])[0]]}")
}
)HZ"},
        {"src/datos.hz", R"HZ(// Carga y prepara los datos: entradas escaladas a [0, 1] y clases como números.
import csv

class Datos {
    var entradas: list = []
    var clases: list = []
    var nombres: list = []
    var minimos: list = []
    var maximos: list = []

    /// Escala una entrada nueva igual que las de entrenamiento.
    function escalar(fila: list): list {
        return range(fila.length).toList().map(j => (fila[j] - minimos[j]) / (maximos[j] - minimos[j]))
    }
}

function cargar(ruta: text): Datos {
    var d = Datos()
    var filas = csv.read(ruta)
    d.nombres = filas.map(f => f.especie).unique()
    var crudas = filas.map(f => [f.largo, f.ancho])
    d.minimos = [0, 1].map(j => crudas.map(c => c[j]).min())
    d.maximos = [0, 1].map(j => crudas.map(c => c[j]).max())
    d.entradas = crudas.map(c => d.escalar(c))
    d.clases = filas.map(f => d.nombres.indexOf(f.especie))
    return d
}
)HZ"},
        {"datos/flores.csv", "largo,ancho,especie\n1.4,0.2,setosa\n1.3,0.2,setosa\n1.5,0.3,setosa\n1.7,0.4,setosa\n1.6,0.2,setosa\n"
                             "4.7,1.4,versicolor\n4.5,1.5,versicolor\n4.0,1.3,versicolor\n4.9,1.5,versicolor\n4.2,1.2,versicolor\n"
                             "6.0,2.5,virginica\n5.6,2.2,virginica\n5.8,1.8,virginica\n6.1,2.3,virginica\n5.1,1.9,virginica\n"},
        {"tests/test_datos.hz", R"HZ(from "../src/datos.hz" import cargar

test("los datos se cargan y se escalan", () => {
    var d = cargar(Path.join(System.scriptDir, "../datos/flores.csv"))
    assertEqual(d.nombres, ["setosa", "versicolor", "virginica"])
    assertEqual(d.entradas.length, 15)
    foreach e in d.entradas { assert(e.all(v => v >= 0 and v <= 1), "entre 0 y 1") }
})
)HZ"},
        {"README.md", readme(title, "Inteligencia artificial: una red neuronal (biblioteca `ai`) aprende a clasificar flores a partir de un CSV.",
                             "| `datos/` | Datos de entrenamiento (`flores.csv`) |\n")},
    };
}

// ---- herramienta de línea de órdenes ---------------------------------------

static std::vector<ProjectFile> scriptProject(const std::string& title) {
    return {
        {"main.hz", "// " + title + R"HZ( — herramienta de línea de órdenes.
// Uso:  hyper main.hz <carpeta> [--ext .hz] [--top 5]
// Muestra cuántas líneas tiene cada archivo de una carpeta, de más a menos.
import cli
from "./src/contar.hz" import contarLineas

var a = cli.parseArgs(System.args)
var carpeta = a.positional.length > 0 ? a.positional[0] : System.scriptDir
var resultado = contarLineas(carpeta, a.options.get("ext", ""))
var top = toNumber(a.options.get("top", "10"))

var filas = [["archivo", "líneas"]] + resultado.take(top).map(r => [r.archivo, r.lineas])
print(cli.table(filas))
print("Total: ${resultado.map(r => r.lineas).sum()} líneas en ${resultado.length} archivos")
)HZ"},
        {"src/contar.hz", R"HZ(/// Lista de {archivo, lineas} de una carpeta (y subcarpetas), ordenada de más a menos líneas.
function contarLineas(carpeta: text, extension: text = ""): list {
    var archivos = File.listAll(carpeta, extension)
    var cuentas = archivos.map(f => ({ archivo: f, lineas: File.readLines(Path.join(carpeta, f)).length }))
    return cuentas.sortBy(r => -r.lineas)
}
)HZ"},
        {"tests/test_contar.hz", R"HZ(from "../src/contar.hz" import contarLineas

test("cuenta las líneas de cada archivo", () => {
    var dir = Path.join(Path.temp, "prueba_contar")
    File.deleteDir(dir)
    File.write(Path.join(dir, "a.txt"), "1\n2\n3\n")
    File.write(Path.join(dir, "b.txt"), "1\n")
    var r = contarLineas(dir)
    assertEqual(r.map(x => x.archivo), ["a.txt", "b.txt"])
    assertEqual(r[0].lineas, 3)
    File.deleteDir(dir)
})
)HZ"},
        {"README.md", readme(title, "Herramienta de línea de órdenes que cuenta las líneas de los archivos de una carpeta.", "")},
    };
}

// ---- aplicación web --------------------------------------------------------

static std::vector<ProjectFile> webProject(const std::string& title) {
    return {
        {"main.hz", "// " + title + R"HZ( — aplicación web: páginas con plantillas, un formulario, una API JSON y archivos estáticos.
// Ejecuta:  hyper main.hz   y abre http://localhost:8080   (Ctrl+C para pararla)
from "./src/app.hz" import crearApp

crearApp(System.scriptDir).listen(8080, { open: System.args.contains("--abrir") })
)HZ"},
        {"src/app.hz", R"HZ(import web

var mensajes: list = []

/// Crea la aplicación con todas sus rutas. carpeta: la del proyecto (donde están plantillas/ y publico/).
/// Las pruebas la usan sin arrancar el servidor.
function crearApp(carpeta: text): web.App {
    var app = web.App()
    app.staticDir("/publico", Path.join(carpeta, "publico"))
    var plantillas = Path.join(carpeta, "plantillas")

    app.get("/", req => web.view(Path.join(plantillas, "inicio.html"), {
        titulo: "Inicio",
        mensajes: mensajes.reversed(),
        hay: mensajes.length > 0
    }, Path.join(plantillas, "base.html")))

    // Formulario: <form method="post" action="/mensajes">
    app.post("/mensajes", req => {
        var datos = web.form(req)
        if datos.get("texto", "").trim() != "" {
            mensajes.add({ autor: datos.get("autor", "anónimo"), texto: datos.texto.trim(), hora: Time.format(Time.now(), "%H:%M") })
        }
        return web.redirect("/")
    })

    // API JSON
    app.get("/api/mensajes", req => mensajes)
    app.get("/api/hora", req => ({ hora: Time.format(Time.now(), "%H:%M:%S") }))
    return app
}
)HZ"},
        {"plantillas/base.html", R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{{titulo}}</title>
<link rel="stylesheet" href="/publico/estilo.css">
</head>
<body>
<header><strong>)HTML" + title + R"HTML(</strong> · hecho con HyperEZ</header>
<main>
{{{contenido}}}
</main>
</body>
</html>
)HTML"},
        {"plantillas/inicio.html", R"HTML(<h1>Mensajes</h1>
<form method="post" action="/mensajes">
  <input name="autor" placeholder="Tu nombre">
  <input name="texto" placeholder="Escribe un mensaje" required>
  <button>Enviar</button>
</form>
{{#if hay}}
<ul>
{{#each mensajes}}
  <li><strong>{{autor}}</strong> ({{hora}}): {{texto}}</li>
{{/each}}
</ul>
{{else}}
<p>Todavía no hay mensajes. ¡Escribe el primero!</p>
{{/if}}
<p><a href="/api/mensajes">Ver los mensajes en JSON</a></p>
)HTML"},
        {"publico/estilo.css", R"CSS(body { font-family: system-ui, "Segoe UI", sans-serif; margin: 0; color: #1f2328; background: #f6f8fa; }
header { background: #2f81f7; color: white; padding: 1rem 1.5rem; }
main { max-width: 42rem; margin: 2rem auto; background: white; padding: 1.5rem 2rem; border-radius: 12px; box-shadow: 0 1px 3px #0002; }
form { display: flex; gap: .5rem; flex-wrap: wrap; }
input { flex: 1; padding: .55rem .7rem; border: 1px solid #d0d7de; border-radius: 8px; font: inherit; }
button { padding: .55rem 1.2rem; border: 0; border-radius: 8px; background: #2f81f7; color: white; font: inherit; cursor: pointer; }
li { margin: .4rem 0; }
)CSS"},
        {"tests/test_app.hz", R"HZ(import web
from "../src/app.hz" import crearApp

var proyecto = Path.join(System.scriptDir, "..")

test("la página de inicio responde", () => {
    var r = crearApp(proyecto).handle(web.request("GET", "/"))
    assertEqual(r.status, 200)
    assert(r.body.contains("Mensajes"))
})

test("el formulario guarda el mensaje y la API lo devuelve", () => {
    var app = crearApp(proyecto)
    var req = web.request("POST", "/mensajes")
    req.body = "autor=Ana&texto=Hola+mundo"
    assertEqual(app.handle(req).status, 302)
    var lista = app.handle(web.request("GET", "/api/mensajes"))
    assertEqual(lista.last().texto, "Hola mundo")
})
)HZ"},
        {"README.md", readme(title, "Una aplicación web con plantillas HTML, un formulario, una API JSON y archivos estáticos.",
                             "| `plantillas/` | Páginas HTML con `{{variables}}` (import template) |\n| `publico/` | CSS, imágenes y JavaScript |\n") +
                          "\nAbre http://localhost:8080 después de `hyper main.hz` (o `hyper main.hz --abrir`).\n"},
    };
}

// ---- sitio web estático ----------------------------------------------------

static std::vector<ProjectFile> siteProject(const std::string& title) {
    return {
        {"main.hz", "// " + title + R"HZ( — sitio web hecho con Markdown: cada archivo de contenido/ es una página.
// Genera la web en la carpeta web/:     hyper main.hz
// Para verla mientras escribes:         hyper watch main.hz   (regenera al guardar)
//                                 y en otra terminal:  hyper serve web   (el navegador se recarga solo)
import site

var hechos = site.build(Path.join(System.scriptDir, "contenido"), Path.join(System.scriptDir, "web"), {
    title: ")HZ" + title + R"HZ(",
    color: "#2f81f7",
    footer: "Hecho con HyperEZ"
})
print("✓ ${hechos.length} archivos en web/")
)HZ"},
        {"contenido/index.md", "---\norden: 1\n---\n# " + title + R"MD(

¡Bienvenido! Esta web se genera con **HyperEZ** a partir de archivos Markdown.

- Edita los archivos de la carpeta `contenido/`.
- Ejecuta `hyper main.hz` para generar la web en `web/`.
- Mírala con `hyper serve web`.

Sigue leyendo en [Sobre mí](sobre-mi.md).
)MD"},
        {"contenido/sobre-mi.md", R"MD(---
titulo: Sobre mí
orden: 2
---
# Sobre mí

Escribe aquí quién eres. Puedes usar *cursiva*, **negrita**, [enlaces](index.md), listas, tablas y código:

```hz
print("¡Hola desde HyperEZ!")
```

| Lenguaje | Me gusta |
|---|---|
| HyperEZ | mucho |
)MD"},
        {"tests/test_web.hz", R"HZ(import site

test("se generan las páginas con su menú", () => {
    var destino = Path.join(Path.temp, "prueba-sitio-" + Hash.uuid())
    var hechos = site.build(Path.join(System.scriptDir, "../contenido"), destino, { title: "Prueba" })
    assertEqual(hechos.length, 2)
    var inicio = File.read(Path.join(destino, "index.html"))
    assert(inicio.contains("sobre-mi.html"))
    File.deleteDir(destino)
})
)HZ"},
        {"README.md", readme(title, "Un sitio web estático escrito en Markdown (blog, documentación, portfolio...).",
                             "| `contenido/` | Las páginas en Markdown (`index.md` es la de inicio) |\n| `web/` | La web generada (súbela a cualquier hosting) |\n") +
                          "\nGenera la web con `hyper main.hz` y mírala con `hyper serve web`.\n"},
        {".gitignore", "web/\n"},
    };
}

std::vector<std::string> projectKinds() { return {"juego", "app", "ia", "script", "web", "sitio"}; }

std::vector<ProjectFile> projectFiles(const std::string& kind, const std::string& title) {
    std::vector<ProjectFile> files;
    if (kind == "app") files = appProject(title);
    else if (kind == "ia") files = aiProject(title);
    else if (kind == "script") files = scriptProject(title);
    else if (kind == "web") files = webProject(title);
    else if (kind == "sitio") files = siteProject(title);
    else files = gameProject(title);
    bool hasIgnore = false;
    for (auto& f : files) if (f.path == ".gitignore") { f.content = "*.tmp\n" + f.content; hasIgnore = true; }
    if (!hasIgnore) files.push_back({".gitignore", "*.tmp\n"});
    return files;
}

} // namespace hz
