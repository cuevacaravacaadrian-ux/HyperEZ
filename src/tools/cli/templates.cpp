// Plantillas de 'hyper new' y generador de la referencia de la API ('hyper docs').
#include "tools/tools.h"
#include "lang/vm/vm.h"
#include "lang/sema/analyzer.h"

#include <algorithm>
#include <map>
#include <set>

namespace hz {

// ---------------------------------------------------------------------------
// Plantillas
// ---------------------------------------------------------------------------

std::string projectTemplateMain(const std::string& title) {
    return R"HZ(// )HZ" + title + R"HZ( — creado con HyperEZ
// Ejecuta:  hyper run        Pruebas:  hyper test
//
// Mueve al héroe (@) con las flechas o WASD hasta el tesoro ($).
// Cuidado: hay trampas escondidas. Pulsa Esc para salir.

from "./jugador.hez" import Jugador, Direccion

if not Console.interactive {
    print("Este juego se maneja con el teclado: ejecútalo en una terminal (hyper run) o con F5 en VS Code.")
    Process.exit(0)
}

const ANCHO = 14
const ALTO = 8

var jugador = Jugador("Héroe", 1, 1)
var tesoro = { x: randomInt(5, ANCHO - 2), y: randomInt(3, ALTO - 2) }
var trampas = []
repeat 6 {
    trampas.add({ x: randomInt(2, ANCHO - 2), y: randomInt(1, ALTO - 2) })
}

/// Se emite cada vez que el jugador cambia de casilla.
event Movimiento(x: number, y: number)

on Movimiento as e {
    foreach t in trampas {
        if t.x == e.x and t.y == e.y {
            jugador.recibirDanio(1)
        }
    }
}

function celda(x: number, y: number): text {
    if x == jugador.x and y == jugador.y { return "@" }
    if x == tesoro.x and y == tesoro.y { return "$" }
    if x == 0 or y == 0 or x == ANCHO - 1 or y == ALTO - 1 { return "#" }
    return "."
}

function dibujar() {
    Console.clear()
    Console.print("  )HZ" + title + R"HZ(", "gold")
    Console.print("  Vida: ${"♥".repeat(jugador.vida)}   Pasos: ${jugador.pasos}", "pink")
    for y in 0..ALTO {
        Console.write("  ")
        for x in 0..ANCHO {
            var c = celda(x, y)
            var col = match c {
                "@" => "cyan"
                "$" => "gold"
                "#" => "gray"
                else => "darkgray"
            }
            Console.write(c + " ", col)
        }
        Console.print()
    }
}

while jugador.vivo {
    dibujar()
    var tecla = Console.readKey()
    var dir = match tecla {
        "up", "w" => Direccion.Arriba
        "down", "s" => Direccion.Abajo
        "left", "a" => Direccion.Izquierda
        "right", "d" => Direccion.Derecha
        "escape", "q" => null
        else => Direccion.Quieto
    }
    if dir == null { break }
    jugador.mover(dir, ANCHO, ALTO)
    emit Movimiento(jugador.x, jugador.y)
    if jugador.x == tesoro.x and jugador.y == tesoro.y {
        dibujar()
        Console.print("\n  ¡Has encontrado el tesoro en ${jugador.pasos} pasos!", "lime")
        break
    }
}
if not jugador.vivo {
    dibujar()
    Console.print("\n  Has caído en demasiadas trampas...", "red")
}
)HZ";
}

std::string projectTemplateModule() {
    return R"HZ(// Lógica del jugador: separada de main.hez para poder probarla en hypertests/.

/// Direcciones en las que se puede mover el jugador.
enum Direccion { Quieto, Arriba, Abajo, Izquierda, Derecha }

/// El personaje que controla quien juega.
class Jugador {
    var nombre: text
    var x: number
    var y: number
    var vida: number = 3
    var pasos: number = 0

    constructor(nombre: text, x: number, y: number) {
        this.nombre = nombre
        this.x = x
        this.y = y
    }

    /// true mientras le quede vida.
    get vivo(): bool { return vida > 0 }

    /// Mueve una casilla sin atravesar el borde del tablero.
    function mover(dir: Direccion, ancho: number, alto: number) {
        var dx = 0
        var dy = 0
        if dir == Direccion.Arriba { dy = -1 }
        else if dir == Direccion.Abajo { dy = 1 }
        else if dir == Direccion.Izquierda { dx = -1 }
        else if dir == Direccion.Derecha { dx = 1 }
        if dx == 0 and dy == 0 { return }
        x = clamp(x + dx, 1, ancho - 2)
        y = clamp(y + dy, 1, alto - 2)
        pasos += 1
    }

    function recibirDanio(cantidad: number) {
        vida = max(0, vida - cantidad)
    }
}
)HZ";
}

std::string projectTemplateTest() {
    return R"HZ(// Pruebas de la lógica del jugador. Ejecútalas con:  hyper test
from "../jugador.hez" import Jugador, Direccion

test("el jugador se mueve y cuenta los pasos", () => {
    var j = Jugador("Ana", 1, 1)
    j.mover(Direccion.Derecha, 10, 10)
    j.mover(Direccion.Abajo, 10, 10)
    assertEqual([j.x, j.y], [2, 2])
    assertEqual(j.pasos, 2)
})

test("no atraviesa los bordes", () => {
    var j = Jugador("Ana", 1, 1)
    j.mover(Direccion.Izquierda, 10, 10)
    assertEqual(j.x, 1)
})

test("pierde vida y deja de estar vivo", () => {
    var j = Jugador("Ana", 1, 1)
    j.recibirDanio(2)
    assertEqual(j.vivo, true)
    j.recibirDanio(5)
    assertEqual(j.vida, 0)
    assertEqual(j.vivo, false)
})
)HZ";
}

std::string projectTemplateReadme(const std::string& title) {
    return "# " + title + "\n\nProyecto creado con [HyperEZ](" HYPEREZ_REPO_URL ") " HYPEREZ_VERSION ".\n\n"
           "| Comando | Qué hace |\n|---|---|\n"
           "| `hyper run` | Ejecuta `main.hez` |\n"
           "| `hyper watch` | Ejecuta `main.hez` y lo repite cada vez que guardas |\n"
           "| `hyper check` | Busca errores sin ejecutar |\n"
           "| `hyper test` | Ejecuta las pruebas de `hypertests/` |\n"
           "| `hyper` | Consola interactiva para probar código |\n\n"
           "En VS Code, abre `main.hez` y pulsa **F5** (o el botón ▶) para ejecutarlo, o **Ctrl+Alt+F5** para que\n"
           "se repita cada vez que guardas.\n";
}

// ---------------------------------------------------------------------------
// Referencia de la API
// ---------------------------------------------------------------------------

static std::string mdEscape(const std::string& s) {
    std::string r;
    for (char c : s) { if (c == '|') r += "\\|"; else if (c == '\n') r += ' '; else r += c; }
    return r;
}

static std::string anchor(const std::string& s) {
    std::string r;
    for (char c : toLower(s)) if (std::isalnum((unsigned char)c) || c == '-' || c == '_') r += c;
    return r;
}

std::string generateApiMarkdown(VM& vm) {
    std::string md;
    md += "# Referencia de la API de HyperEZ " HYPEREZ_VERSION "\n\n";
    md += "> Generada automáticamente con `hyper docs` a partir del catálogo de la máquina virtual.\n\n";

    std::vector<const ApiEntry*> globalsFn, globalsConst, classes, modules;
    std::map<std::string, std::vector<const ApiEntry*>> members;
    std::set<std::string> moduleNames;
    for (auto& e : vm.catalog) if (e.kind == ApiKind::Module && e.owner.empty()) moduleNames.insert(e.name);
    for (auto& e : vm.catalog) {
        if (e.owner.empty()) {
            if (e.kind == ApiKind::Function) globalsFn.push_back(&e);
            else if (e.kind == ApiKind::Constant) globalsConst.push_back(&e);
            else if (e.kind == ApiKind::Class) classes.push_back(&e);
            else if (e.kind == ApiKind::Module) modules.push_back(&e);
        } else {
            members[e.owner].push_back(&e);
        }
    }
    auto byName = [](const ApiEntry* a, const ApiEntry* b) { return a->name < b->name; };
    std::sort(globalsFn.begin(), globalsFn.end(), byName);
    std::sort(classes.begin(), classes.end(), byName);
    std::sort(modules.begin(), modules.end(), byName);

    // Índice
    md += "## Contenido\n\n- [Funciones globales](#funciones-globales)\n- [Constantes](#constantes)\n";
    md += "- Tipos y clases: ";
    for (size_t i = 0; i < classes.size(); i++) md += std::string(i ? ", " : "") + "[" + classes[i]->name + "](#" + anchor("clase-" + classes[i]->name) + ")";
    md += "\n- Módulos: ";
    for (size_t i = 0; i < modules.size(); i++) md += std::string(i ? ", " : "") + "[" + modules[i]->name + "](#" + anchor("modulo-" + modules[i]->name) + ")";
    md += "\n- [Bibliotecas para import](#bibliotecas)\n\n";

    auto fnBlock = [&](const ApiEntry& e, const std::string& prefix) {
        md += "#### `" + prefix + e.name + e.signature() + "`\n\n";
        if (!e.doc.empty()) md += e.doc + "\n\n";
        if (!e.example.empty()) md += "```hz\n" + e.example + "\n```\n\n";
    };

    md += "## Funciones globales\n\n";
    for (auto* e : globalsFn) fnBlock(*e, "");
    // Funciones del preludio (escritas en HyperEZ)
    if (vm.preludeUnit) {
        for (auto& [name, sym] : vm.preludeUnit->globalSyms) {
            if (!sym || name.rfind("__", 0) == 0 || sym->vis == Visibility::Private) continue;
            if (sym->kind != SymKind::Function) continue;
            md += "#### `" + sym->detail + "`\n\n";
            if (!sym->doc.empty()) md += sym->doc + "\n\n";
        }
    }

    md += "## Constantes\n\n| Nombre | Tipo | Descripción |\n|---|---|---|\n";
    for (auto* e : globalsConst) md += "| `" + e->name + "` | `" + e->type + "` | " + mdEscape(e->doc) + " |\n";
    md += "\n";

    auto memberTable = [&](const std::string& owner, const std::string& prefix) {
        auto it = members.find(owner);
        if (it == members.end()) return;
        auto list = it->second;
        std::stable_sort(list.begin(), list.end(), byName);
        bool anyProp = false;
        for (auto* e : list) if (e->kind == ApiKind::Property || e->kind == ApiKind::Field || e->kind == ApiKind::Constant) anyProp = true;
        if (anyProp) {
            md += "| Propiedad | Tipo | Descripción |\n|---|---|---|\n";
            for (auto* e : list) {
                if (e->kind != ApiKind::Property && e->kind != ApiKind::Field && e->kind != ApiKind::Constant) continue;
                std::string flags;
                if (e->isStatic) flags += " *(estática)*";
                if (e->readonly) flags += " *(solo lectura)*";
                md += "| `" + prefix + e->name + "` | `" + e->type + "` | " + mdEscape(e->doc) + flags + " |\n";
            }
            md += "\n";
        }
        for (auto* e : list) {
            if (e->kind == ApiKind::Function || e->kind == ApiKind::Method) fnBlock(*e, e->isStatic || moduleNames.count(owner) ? owner + "." : prefix);
            else if (e->kind == ApiKind::Event) {
                md += "#### `event " + owner + "." + e->name + e->signature().substr(0, e->signature().rfind(')') + 1) + "`\n\n";
                if (!e->doc.empty()) md += e->doc + "\n\n";
            }
        }
    };

    for (auto* c : classes) {
        md += "## Clase " + c->name + "\n\n";
        if (!c->doc.empty()) md += c->doc + "\n\n";
        if (!c->params.empty() || c->minArgs > 0) md += "Constructor: `" + c->name + c->signature().substr(0, c->signature().rfind(')') + 1) + "`\n\n";
        memberTable(c->name, ".");
    }
    for (auto* m : modules) {
        md += "## Módulo " + m->name + "\n\n";
        if (!m->doc.empty()) md += m->doc + "\n\n";
        memberTable(m->name, m->name + ".");
    }

    md += "## Bibliotecas\n\nSe pueden importar por nombre: `import HyperCore`.\n\n| Biblioteca | Contiene | Descripción |\n|---|---|---|\n";
    for (auto& [lib, doc] : vm.libraryDocs) {
        std::string contents;
        auto it = vm.libraryMembers.find(lib);
        if (it != vm.libraryMembers.end())
            for (size_t i = 0; i < it->second.size(); i++) contents += (i ? ", " : "") + it->second[i];
        md += "| `" + lib + "` | " + contents + " | " + mdEscape(doc) + " |\n";
    }
    return md;
}

} // namespace hz
