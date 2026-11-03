// hyper repl — consola interactiva: escribe código y ve el resultado al momento.
//
// Cada entrada se analiza junto con las anteriores (así conoce sus variables, funciones y tipos),
// pero solo se genera y ejecuta el código nuevo. Los globales viven en un módulo compartido.
#include "../lang/vm.h"
#include "../lang/analyzer.h"
#include "../lang/lexer.h"
#include "../lang/parser.h"
#include "tools.h"

#include <algorithm>
#include <csignal>
#include <set>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace hz {
namespace {

constexpr const char* kName = "<consola>";
bool g_shown = false; // la última entrada mostró un valor

struct Shape {
    std::set<std::string> names; // nombres globales que declara (para poder redefinirlos)
    bool singleExpr = false;     // es una sola expresión (se guarda como print(...))
};

Shape shapeOf(const std::string& code) {
    DiagnosticBag diags;
    auto prog = parseSource(kName, code, diags);
    Shape shape;
    auto& out = shape.names;
    if (prog->body.size() == 1 && prog->body[0]->kind == StmtKind::Expr) {
        auto k = static_cast<ExprStmt*>(prog->body[0].get())->expr->kind;
        shape.singleExpr = k != ExprKind::Assign && k != ExprKind::IncDec;
    }
    for (auto& s : prog->body) {
        switch (s->kind) {
            case StmtKind::Var: out.insert(static_cast<VarStmt*>(s.get())->name); break;
            case StmtKind::Function: out.insert(static_cast<FuncStmt*>(s.get())->fn->name); break;
            case StmtKind::Class: out.insert(static_cast<ClassStmt*>(s.get())->name); break;
            case StmtKind::Interface: out.insert(static_cast<InterfaceStmt*>(s.get())->name); break;
            case StmtKind::Enum: out.insert(static_cast<EnumStmt*>(s.get())->name); break;
            case StmtKind::Event: out.insert(static_cast<EventStmt*>(s.get())->name); break;
            default: break;
        }
    }
    return shape;
}

// ¿La entrada sigue en la línea siguiente? (paréntesis, corchetes o llaves sin cerrar, texto """
// o comentario /* sin cerrar, o una línea que acaba en operador)
bool needsMore(const std::string& code) {
    DiagnosticBag diags;
    Lexer lx(code, diags);
    auto toks = lx.tokenize();
    for (auto& d : diags.items) if (d.code == "E0001") return true;
    size_t triple = 0;
    for (size_t p = 0; (p = code.find("\"\"\"", p)) != std::string::npos; p += 3) triple++;
    if (triple % 2) return true;
    int depth = 0;
    const Token* last = nullptr;
    for (auto& t : toks) {
        if (t.kind == Tok::Eof) break;
        if (t.kind == Tok::LParen || t.kind == Tok::LBracket || t.kind == Tok::LBrace) depth++;
        if (t.kind == Tok::RParen || t.kind == Tok::RBracket || t.kind == Tok::RBrace) depth--;
        last = &t;
    }
    if (depth > 0) return true;
    if (!last) return false;
    switch (last->kind) {
        case Tok::Plus: case Tok::Minus: case Tok::Star: case Tok::Slash: case Tok::Percent: case Tok::StarStar:
        case Tok::Eq: case Tok::PlusEq: case Tok::MinusEq: case Tok::StarEq: case Tok::SlashEq: case Tok::PercentEq:
        case Tok::EqEq: case Tok::NotEq: case Tok::Less: case Tok::LessEq: case Tok::Greater: case Tok::GreaterEq:
        case Tok::AmpAmp: case Tok::PipePipe: case Tok::KwAnd: case Tok::KwOr: case Tok::KwNot: case Tok::Comma:
        case Tok::Dot: case Tok::Arrow: case Tok::QuestionQuestion: case Tok::Colon:
            return true;
        default:
            return false;
    }
}

// Muestra el valor de la última expresión de una entrada.
Value showValue(VM& vm, int argc, Value* args) {
    if (argc < 1 || args[0].isNull() || args[0].isUndef()) return Value();
    const Value& v = args[0];
    const char* c = isString(v) ? color::green() : (v.isNum() || v.isBool()) ? color::yellow() : "";
    if ((vm.output.empty() ? vm.lastOutputChar : vm.output.back()) != '\n') vm.print("\n"); // tras un write() sin salto
    g_shown = true;
    vm.print(std::string(c) + vm.toString(v, true) + color::reset() + "\n");
    return Value();
}

class Session {
public:
    Session() {
        setupVM(vm);
        vm.projectRoot = currentDir();
        path = pathJoin(currentDir(), kName);
        rs.echoSlot = vm.builtinSlot("__consola_mostrar");
        vm.builtins[rs.echoSlot] = Value::object(vm.newNative("mostrar", showValue, 1, 1));
        vm.repl = &rs;
        vm.errorHook = [this](const std::string& msg, const std::string& file, int line, int col, const std::vector<std::string>& trace) {
            return runtimeError(msg, file, line, col, trace);
        };
    }

    // Compila y ejecuta una entrada. false si tiene errores de compilación (y entonces no se guarda).
    bool eval(const std::string& code) {
        Shape shape = shapeOf(code);
        auto& names = shape.names;
        // Redefinir: una entrada anterior cuyos nombres se declaran todos de nuevo se sustituye por
        // líneas vacías (así no cambian los números de línea del resto).
        std::vector<size_t> replaced;
        for (size_t i = 0; i < entries.size(); i++) {
            auto& e = entries[i];
            if (e.removed || e.names.empty()) continue;
            if (std::all_of(e.names.begin(), e.names.end(), [&](const std::string& n) { return names.count(n) > 0; }))
                replaced.push_back(i);
        }
        std::string source;
        for (size_t i = 0; i < entries.size(); i++) {
            auto& e = entries[i];
            bool blank = e.removed || std::find(replaced.begin(), replaced.end(), i) != replaced.end();
            source += blank ? std::string(std::count(e.code.begin(), e.code.end(), '\n'), '\n') : e.code;
            source += '\n';
        }
        int firstLine = (int)std::count(source.begin(), source.end(), '\n') + 1;
        rs.start = (int)source.size();
        source += code;
        source += '\n';

        ModuleUnit* unit = vm.compileSource(path, source, true);
        if (report(unit, code, firstLine)) return false;

        rs.module = unit->module;
        rs.globals = unit->globalNames;
        for (size_t i : replaced) entries[i].removed = true;
        entries.push_back({code, names, firstLine});
        last = unit;

        g_interrupt = false;
        g_shown = false;
        vm.hadUncaught = false;
        bool ok = vm.runModule(unit);
        if (ok) ok = vm.drainScheduler();
        vm.flushOutput();
        if (vm.lastOutputChar != '\n') {
            std::fputc('\n', stdout); // que el siguiente "hz>" empiece en su propia línea
            vm.lastOutputChar = '\n';
        }
        entries.back().failed = !ok || vm.hadUncaught;
        entries.back().printable = shape.singleExpr && g_shown;
        if (g_interrupt.exchange(false)) {
            vm.cancelAllTasks();
            std::fprintf(stderr, "%sEjecución interrumpida (Ctrl+C)%s\n", color::yellow(), color::reset());
        }
        std::fflush(stdout);
        return true;
    }

    // Código de la sesión. Para un archivo, las expresiones cuyo valor se mostró se escriben como
    // print(...) y se omiten las entradas que fallaron sin declarar nada.
    std::string code(bool forFile = false) const {
        std::string out;
        for (auto& e : entries) {
            if (e.removed) continue;
            if (forFile && e.failed && e.names.empty()) continue;
            out += forFile && e.printable ? "print(" + e.code + ")\n" : e.code + "\n";
        }
        return out;
    }

    void showVars() {
        if (!last || !rs.module) { std::printf("%s(todavía no hay nada definido)%s\n", color::gray(), color::reset()); return; }
        int shown = 0;
        for (size_t i = 0; i < rs.globals.size() && i < rs.module->globals.size(); i++) {
            const std::string& name = rs.globals[i];
            auto it = last->globalSyms.find(name);
            if (it == last->globalSyms.end() || !it->second) continue;
            Symbol* s = it->second;
            const Value& v = rs.module->globals[i];
            if (s->kind == SymKind::Variable || s->kind == SymKind::Constant) {
                std::string val = v.isUndef() ? "(sin valor)" : vm.toString(v, true);
                if (utf8Length(val) > 70) val = val.substr(0, utf8Offset(val, 67)) + "...";
                std::printf("  %s%s%s = %s\n", color::bold(), s->detail.c_str(), color::reset(), val.c_str());
            } else {
                std::string d = s->detail.empty() ? name : s->detail;
                // "class Jugador Jugador(nombre: text)" -> "class Jugador": el constructor sobra en la lista
                size_t p = d.find(" " + name + "(");
                if ((s->kind == SymKind::Class || s->kind == SymKind::Struct) && p != std::string::npos) {
                    size_t q = d.find(')', p);
                    d.erase(p, q == std::string::npos ? std::string::npos : q - p + 1);
                }
                std::printf("  %s%s%s\n", color::cyan(), d.c_str(), color::reset());
            }
            shown++;
        }
        if (!shown) std::printf("%s(todavía no hay nada definido)%s\n", color::gray(), color::reset());
    }

    VM vm;

private:
    struct Entry {
        std::string code;
        std::set<std::string> names; // globales que declara
        int firstLine = 1;           // línea de la sesión donde empieza (no cambia nunca)
        bool removed = false;        // sustituida por una redefinición
        bool failed = false;         // terminó con un error en ejecución
        bool printable = false;      // expresión cuyo valor se mostró
    };
    ReplSession rs;
    std::vector<Entry> entries;
    ModuleUnit* last = nullptr;
    std::string path;

    // Entrada que contiene una línea de la sesión; rel recibe la línea dentro de la entrada.
    const Entry* entryAt(int line, int& rel) const {
        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            if (line >= it->firstLine) {
                rel = line - it->firstLine + 1;
                return &*it;
            }
        }
        return nullptr;
    }

    // Error en ejecución con la línea relativa a la entrada en la que está el código.
    bool runtimeError(const std::string& msg, const std::string& file, int line, int col, const std::vector<std::string>& trace) {
        int rel = 0;
        const Entry* e = file == path ? entryAt(line, rel) : nullptr;
        if (!e) return false; // error dentro de un módulo importado: formato normal
        Diagnostic d;
        d.message = msg;
        d.file = kName;
        d.loc.line = rel;
        d.loc.col = col;
        printDiagnostic(stderr, d, e->code, "error en ejecución");
        if (trace.size() > 1) {
            std::string tag = std::string(kName) + ":";
            std::fprintf(stderr, "  %spila de llamadas:%s\n", color::gray(), color::reset());
            for (auto& t : trace) {
                std::string s = t;
                size_t p = s.find(tag);
                if (p != std::string::npos) {
                    size_t q = p + tag.size(), r = q;
                    while (r < s.size() && std::isdigit((unsigned char)s[r])) r++;
                    int l = 0;
                    if (r > q && entryAt(std::stoi(s.substr(q, r - q)), l)) s = s.substr(0, q) + std::to_string(l) + s.substr(r);
                }
                std::fprintf(stderr, "    %sen%s %s\n", color::gray(), color::reset(), s.c_str());
            }
        }
        std::fflush(stderr);
        return true;
    }

    // Muestra los errores (y avisos útiles) de la entrada con números de línea propios. true si hay errores.
    bool report(ModuleUnit* unit, const std::string& code, int firstLine) {
        bool errors = false;
        for (auto& d : unit->diags.items) {
            bool isError = d.severity == Severity::Error;
            if (isError) errors = true;
            if (d.loc.offset >= rs.start) {
                if (!isError && (d.severity != Severity::Warning || d.code == "W0303")) continue;
                Diagnostic local = d;
                local.loc.line -= firstLine - 1;
                local.file = kName;
                printDiagnostic(stderr, local, code);
            } else if (isError) {
                // la entrada nueva rompe código anterior (por ejemplo, cambia el tipo de algo que ya se usaba)
                Diagnostic prev = d;
                prev.message += " (en el código de una entrada anterior)";
                prev.loc = {};
                printDiagnostic(stderr, prev, "");
            }
        }
        for (auto* imp : unit->imports) {
            if (vm.unitHasErrors(imp)) {
                vm.printDiagnostics(imp, stderr, false);
                errors = true;
            }
        }
        return errors || !unit->compiled;
    }
};

void printBanner() {
    std::printf("%sHyperEZ %s%s · consola interactiva\n", color::bold(), HYPEREZ_VERSION, color::reset());
    std::printf("%sEscribe código y pulsa Intro. Lo que definas se conserva entre líneas.\n"
                ".ayuda muestra los comandos · .salir para terminar%s\n\n", color::gray(), color::reset());
}

void printReplHelp() {
    std::printf(
        "%sComandos de la consola:%s\n"
        "  .ayuda               esta ayuda\n"
        "  .vars                variables, funciones y clases definidas\n"
        "  .codigo              todo el código de la sesión\n"
        "  .guardar [archivo]   guarda el código de la sesión (por defecto sesion.hz)\n"
        "  .cargar <archivo>    ejecuta un archivo dentro de la sesión\n"
        "  .limpiar             limpia la pantalla\n"
        "  .reiniciar           empieza de cero\n"
        "  .salir               termina (también Ctrl+Z e Intro)\n\n"
        "%sConsejos:%s\n"
        "  · Escribe una expresión (2 + 3, \"hola\".upper()) y verás su valor.\n"
        "  · Si una línea deja algo abierto ({, (, [) sigue en la siguiente; una línea vacía la termina.\n"
        "  · Puedes redefinir una función o variable escribiéndola otra vez.\n"
        "  · Ctrl+C detiene un bucle que no termina.\n",
        color::bold(), color::reset(), color::bold(), color::reset());
}

#ifdef _WIN32
BOOL WINAPI onCtrl(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        g_interrupt = true;
        return TRUE;
    }
    return FALSE;
}
#else
void onSigint(int) { g_interrupt = true; }
#endif

enum class Read { Code, End, Cancel };

// Lee una entrada completa (puede ocupar varias líneas).
Read readEntry(std::string& code, bool interactive) {
    code.clear();
    bool first = true;
    for (;;) {
        if (interactive) {
            std::printf("%s%s%s", color::cyan(), first ? "hz> " : "... ", color::reset());
            std::fflush(stdout);
        }
        std::string line;
        g_interrupt = false;
        if (!readLine(line)) {
#ifdef _WIN32
            if (interactive) Sleep(30); // el aviso de Ctrl+C llega en otro hilo
#endif
            if (g_interrupt.exchange(false)) {
                std::printf("\n");
                return Read::Cancel;
            }
            if (!code.empty()) return Read::Code;
            return Read::End;
        }
        if (!first && trim(line).empty()) return Read::Code; // una línea vacía termina la entrada
        code += first ? line : "\n" + line;
        first = false;
        if (!needsMore(code)) return Read::Code;
    }
}

bool isCommand(const std::string& t) {
    if (t == "salir" || t == "exit" || t == "quit") return true;
    return t.size() > 1 && t[0] == '.' && std::isalpha((unsigned char)t[1]);
}

} // namespace

int runRepl() {
    bool interactive = stdinIsConsole();
#ifdef _WIN32
    SetConsoleCtrlHandler(onCtrl, TRUE);
#else
    std::signal(SIGINT, onSigint);
#endif
    if (interactive) printBanner();
    for (;;) {
        Session session;
        bool restart = false;
        for (;;) {
            std::string code;
            Read r = readEntry(code, interactive);
            if (r == Read::End) {
                if (interactive) std::printf("\n");
                return 0;
            }
            if (r == Read::Cancel) continue;
            std::string t = trim(code);
            if (t.empty()) continue;
            if (isCommand(t)) {
                std::string cmd = t, arg;
                size_t sp = t.find(' ');
                if (sp != std::string::npos) { cmd = t.substr(0, sp); arg = trim(t.substr(sp + 1)); }
                if (cmd == ".salir" || cmd == ".exit" || cmd == "salir" || cmd == "exit" || cmd == "quit") return 0;
                if (cmd == ".ayuda" || cmd == ".help") printReplHelp();
                else if (cmd == ".vars") session.showVars();
                else if (cmd == ".codigo" || cmd == ".code") {
                    std::string c = session.code();
                    if (c.empty()) std::printf("%s(la sesión está vacía)%s\n", color::gray(), color::reset());
                    else std::fwrite(c.data(), 1, c.size(), stdout);
                } else if (cmd == ".guardar" || cmd == ".save") {
                    std::string file = arg.empty() ? "sesion.hz" : arg;
                    if (pathExt(file).empty()) file += ".hz";
                    if (writeFile(file, session.code(true)))
                        std::printf("%s✓%s Guardado en %s (ejecútalo con: hyper %s)\n", color::green(), color::reset(), file.c_str(), file.c_str());
                    else
                        std::fprintf(stderr, "%serror:%s no se puede escribir '%s'\n", color::red(), color::reset(), file.c_str());
                } else if (cmd == ".cargar" || cmd == ".load") {
                    std::string src;
                    if (arg.empty()) std::fprintf(stderr, "Uso: .cargar <archivo.hz>\n");
                    else if (!readFile(arg, src)) std::fprintf(stderr, "%serror:%s no se puede leer '%s'\n", color::red(), color::reset(), arg.c_str());
                    else {
                        while (!src.empty() && (src.back() == '\n' || src.back() == '\r')) src.pop_back();
                        src.erase(std::remove(src.begin(), src.end(), '\r'), src.end());
                        session.eval(src);
                    }
                } else if (cmd == ".limpiar" || cmd == ".clear" || cmd == ".cls") {
                    std::printf("\x1b[2J\x1b[3J\x1b[H");
                    if (interactive) printBanner();
                } else if (cmd == ".reiniciar" || cmd == ".reset") {
                    restart = true;
                    std::printf("%sSesión reiniciada.%s\n", color::gray(), color::reset());
                    break;
                } else {
                    std::fprintf(stderr, "Comando desconocido: %s (escribe .ayuda)\n", cmd.c_str());
                }
                std::fflush(stdout);
                continue;
            }
            session.eval(code);
        }
        if (!restart) return 0;
    }
}

} // namespace hz
