// hyper — herramienta de línea de comandos de HyperEZ.
#include "../lang/vm.h"
#include "../lang/analyzer.h"
#include "tools.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#endif

using namespace hz;

static void printHelp() {
    std::printf(
        "%sHyperEZ %s%s — lenguaje de programación sencillo para aprender y crear juegos\n\n"
        "%sUso:%s\n"
        "  hyper <archivo.hz> [args...]     Ejecuta un programa\n"
        "  hyper run [archivo.hz] [args...] Ejecuta un programa (por defecto main.hz)\n"
        "  hyper watch [archivo.hz]         Ejecuta y repite cada vez que guardas un .hz\n"
        "  hyper repl                       Consola interactiva (también: hyper sin nada)\n"
        "  hyper check <archivos|carpetas>  Busca errores sin ejecutar\n"
        "  hyper test [carpeta|archivos]    Ejecuta las pruebas (por defecto ./tests)\n"
        "  hyper new <nombre>               Crea un proyecto nuevo\n"
        "  hyper docs [salida.md]           Genera la referencia de la API\n"
        "  hyper lsp                        Servidor de lenguaje (para editores)\n"
#ifdef _WIN32
        "  hyper register                   Asocia .hz y .hez a este hyper.exe\n"
        "  hyper unregister                 Quita la asociación de archivos\n"
#endif
        "  hyper version                    Muestra la versión\n"
        "  hyper help                       Muestra esta ayuda\n\n"
        "%sOpciones de run:%s\n"
        "  --pause    Espera una tecla al terminar (al abrir con doble clic)\n"
        "  --         Todo lo que sigue se pasa al programa en System.args\n",
        color::bold(), HYPEREZ_VERSION, color::reset(), color::bold(), color::reset(), color::bold(), color::reset());
}

void hz::setupVM(VM& vm) {
    registerStdlib(vm);
    registerMathTypes(vm);
    buildWorld(vm);
    if (!vm.loadPrelude()) std::exit(70);
}

// ---------------------------------------------------------------------------
// check
// ---------------------------------------------------------------------------

static void collectSources(const std::string& path, std::vector<std::string>& out) {
    if (dirExists(path)) {
        for (auto& f : listFilesRecursive(path)) {
            std::string e = pathExt(f);
            if ((e == ".hz" || e == ".hez") && f.find("/hz_packages/") == std::string::npos) out.push_back(f);
        }
    } else {
        out.push_back(path);
    }
}

static int checkOne(const std::string& file, int& errors, int& warnings) {
    VM vm;
    setupVM(vm);
    vm.projectRoot = pathDir(pathAbsolute(file));
    ModuleUnit* unit = vm.compileFile(file);
    vm.printDiagnostics(unit, stderr, true);
    int e = 0, w = 0;
    for (auto& u : vm.units) {
        if (u.get() == vm.preludeUnit) continue;
        for (auto& d : u->diags.items) {
            if (d.severity == Severity::Error) e++;
            else if (d.severity == Severity::Warning) w++;
        }
    }
    errors += e;
    warnings += w;
    if (e == 0)
        std::fprintf(stderr, "%s✓%s %s: sin errores%s\n", color::green(), color::reset(), pathFilename(file).c_str(),
                     w ? (" (" + std::to_string(w) + " aviso" + (w == 1 ? "" : "s") + ")").c_str() : "");
    return e;
}

static int cmdCheck(const std::vector<std::string>& paths) {
    std::vector<std::string> files;
    for (auto& p : paths) collectSources(p, files);
    if (files.empty()) { std::fprintf(stderr, "No hay archivos .hz que comprobar\n"); return 2; }
    int errors = 0, warnings = 0, bad = 0;
    for (auto& f : files) if (checkOne(f, errors, warnings)) bad++;
    if (errors)
        std::fprintf(stderr, "%s✗%s %d error%s, %d aviso%s%s\n", color::red(), color::reset(), errors, errors == 1 ? "" : "es", warnings,
                     warnings == 1 ? "" : "s", files.size() > 1 ? (" en " + std::to_string(bad) + " archivo(s)").c_str() : "");
    return errors ? 1 : 0;
}

// ---------------------------------------------------------------------------
// run
// ---------------------------------------------------------------------------

static void waitForKey() {
#ifdef _WIN32
    std::fflush(stdout);
    std::fprintf(stderr, "\n%sPulsa una tecla para cerrar...%s", color::gray(), color::reset());
    std::fflush(stderr);
    _getch();
#endif
}

static int cmdRun(const std::string& file, const std::vector<std::string>& args, bool pause) {
    int code = 0;
    {
        VM vm;
        setupVM(vm);
        vm.projectRoot = pathDir(pathAbsolute(file));
        vm.scriptArgs = args;
        ModuleUnit* unit = vm.compileFile(file);
        if (vm.unitHasErrors(unit)) {
            vm.printDiagnostics(unit, stderr, true);
            std::fprintf(stderr, "%sNo se puede ejecutar: corrige los errores de arriba.%s\n", color::red(), color::reset());
            code = 1;
        } else {
            bool ok = vm.runModule(unit);
            if (ok) ok = vm.drainScheduler();
            vm.flushOutput();
            if (vm.testsPassed + vm.testsFailed > 0) {
                std::fprintf(stderr, "\n%s%d prueba%s:%s %s%d correcta%s%s", color::bold(), vm.testsPassed + vm.testsFailed,
                             vm.testsPassed + vm.testsFailed == 1 ? "" : "s", color::reset(), color::green(), vm.testsPassed,
                             vm.testsPassed == 1 ? "" : "s", color::reset());
                if (vm.testsFailed) std::fprintf(stderr, ", %s%d fallida%s%s", color::red(), vm.testsFailed, vm.testsFailed == 1 ? "" : "s", color::reset());
                std::fprintf(stderr, "\n");
            }
            code = ok && !vm.hadUncaught && vm.testsFailed == 0 ? 0 : 1;
        }
    }
    if (pause) waitForKey();
    return code;
}

// ---------------------------------------------------------------------------
// test
// ---------------------------------------------------------------------------

static std::string normalizeOutput(std::string s, const std::string& dir) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) if (s[i] != '\r') out += s[i];
    // rutas absolutas -> relativas a la carpeta de la prueba
    std::string prefix = dir + "/";
    for (size_t p; (p = out.find(prefix)) != std::string::npos;) out.erase(p, prefix.size());
    // espacios finales de cada línea y líneas vacías del final
    std::string r;
    for (auto& line : splitLines(out)) {
        std::string l = line;
        while (!l.empty() && (l.back() == ' ' || l.back() == '\t')) l.pop_back();
        r += l + "\n";
    }
    while (r.size() > 1 && r[r.size() - 1] == '\n' && r[r.size() - 2] == '\n') r.pop_back();
    if (r == "\n") r.clear();
    return r;
}

static int cmdTest(const std::vector<std::string>& paths, bool update) {
    std::vector<std::string> roots = paths;
    if (roots.empty()) roots.push_back(dirExists("tests") ? "tests" : ".");
    std::vector<std::string> files;
    for (auto& p : roots) collectSources(p, files);
    // los archivos que empiezan por "_" son auxiliares (se importan desde otras pruebas)
    files.erase(std::remove_if(files.begin(), files.end(), [](const std::string& f) {
        std::string n = pathFilename(f);
        return n.empty() || n[0] == '_' || f.find("/bench/") != std::string::npos;
    }), files.end());
    if (files.empty()) { std::fprintf(stderr, "No se encontraron pruebas (.hz) en %s\n", roots[0].c_str()); return 2; }
    std::string self = exePath();
    int passed = 0, failed = 0;
    double t0 = nowSeconds();
    for (auto& f : files) {
        std::string abs = pathAbsolute(f);
        std::string dir = pathDir(abs);
        std::string expectedPath = abs.substr(0, abs.size() - pathExt(abs).size()) + ".out";
        std::string expected;
        bool hasExpected = readFile(expectedPath, expected);
        std::string output;
        double t = nowSeconds();
        int exitCode = runProcess({self, "run", abs}, dir, output, 20000);
        double ms = (nowSeconds() - t) * 1000;
        std::string got = normalizeOutput(output, dir);
        bool ok;
        std::string why;
        if (update) {
            writeFile(expectedPath, got);
            ok = true;
            why = "actualizado";
        } else if (hasExpected) {
            std::string want = normalizeOutput(expected, dir);
            ok = got == want;
            if (!ok) {
                auto a = splitLines(want), b = splitLines(got);
                for (size_t i = 0; i < std::max(a.size(), b.size()); i++) {
                    std::string x = i < a.size() ? a[i] : "<fin>", y = i < b.size() ? b[i] : "<fin>";
                    if (x != y) { why = "línea " + std::to_string(i + 1) + ":\n        esperado: " + x + "\n        obtenido: " + y; break; }
                }
            }
        } else {
            ok = exitCode == 0;
            if (exitCode == -1) why = "tiempo agotado (20 s)";
            else if (!ok) {
                why = "terminó con código " + std::to_string(exitCode);
                auto lines = splitLines(got);
                int shown = 0;
                for (auto& l : lines) if (!l.empty() && shown < 12) { why += "\n        " + l; shown++; }
            }
        }
        std::string rel = f;
        if (ok) passed++; else failed++;
        std::printf("  %s%s%s %s %s(%.0f ms)%s%s%s\n", ok ? color::green() : color::red(), ok ? "✓" : "✗", color::reset(), rel.c_str(),
                    color::gray(), ms, color::reset(), why.empty() ? "" : "\n      ", why.c_str());
    }
    std::printf("\n%s%d archivo%s de prueba:%s %s%d correcto%s%s", color::bold(), passed + failed, passed + failed == 1 ? "" : "s", color::reset(),
                color::green(), passed, passed == 1 ? "" : "s", color::reset());
    if (failed) std::printf(", %s%d fallido%s%s", color::red(), failed, failed == 1 ? "" : "s", color::reset());
    std::printf(" %s(%.1f s)%s\n", color::gray(), nowSeconds() - t0, color::reset());
    return failed ? 1 : 0;
}

// ---------------------------------------------------------------------------
// new
// ---------------------------------------------------------------------------

static int cmdNew(const std::string& name) {
    if (name.empty()) { std::fprintf(stderr, "Uso: hyper new <nombre>\n"); return 2; }
    if (fileExists(name) || (dirExists(name) && !listFilesRecursive(name).empty())) {
        std::fprintf(stderr, "%sYa existe '%s' y no está vacío.%s\n", color::red(), name.c_str(), color::reset());
        return 1;
    }
    std::string title = pathFilename(pathAbsolute(name));
    makeDirs(pathJoin(name, "tests"));
    writeFile(pathJoin(name, "main.hz"), projectTemplateMain(title));
    writeFile(pathJoin(name, "jugador.hz"), projectTemplateModule());
    writeFile(pathJoin(name, "tests/test_jugador.hz"), projectTemplateTest());
    writeFile(pathJoin(name, "README.md"), projectTemplateReadme(title));
    writeFile(pathJoin(name, ".gitignore"), "*.tmp\n");
    std::printf("%s✓%s Proyecto creado en %s%s%s\n\n", color::green(), color::reset(), color::bold(), name.c_str(), color::reset());
    std::printf("  cd %s\n  hyper run          %s# ejecuta main.hz%s\n  hyper test         %s# ejecuta las pruebas%s\n", name.c_str(),
                color::gray(), color::reset(), color::gray(), color::reset());
    return 0;
}

// ---------------------------------------------------------------------------
// docs
// ---------------------------------------------------------------------------

static int cmdDocs(const std::string& out) {
    VM vm;
    setupVM(vm);
    std::string md = generateApiMarkdown(vm);
    if (out.empty() || out == "-") { std::fwrite(md.data(), 1, md.size(), stdout); return 0; }
    std::string dir = pathDir(pathAbsolute(out));
    makeDirs(dir);
    if (!writeFile(out, md)) { std::fprintf(stderr, "No se puede escribir '%s'\n", out.c_str()); return 1; }
    std::fprintf(stderr, "%s✓%s Referencia de la API escrita en %s\n", color::green(), color::reset(), out.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

static std::vector<std::string> commandLineArgs(int argc, char** argv) {
#ifdef _WIN32
    // argv de MinGW usa la página de códigos ANSI: se recupera la línea de comandos en UTF-16.
    int n = 0;
    LPWSTR* w = CommandLineToArgvW(GetCommandLineW(), &n);
    if (w) {
        std::vector<std::string> r;
        for (int i = 1; i < n; i++) {
            int sz = WideCharToMultiByte(CP_UTF8, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
            std::string s(sz > 0 ? sz - 1 : 0, '\0');
            if (sz > 1) WideCharToMultiByte(CP_UTF8, 0, w[i], -1, s.data(), sz, nullptr, nullptr);
            r.push_back(s);
        }
        LocalFree(w);
        return r;
    }
#endif
    return std::vector<std::string>(argv + 1, argv + argc);
}

static bool isSourceFile(const std::string& p) {
    std::string e = pathExt(p);
    return e == ".hz" || e == ".hez";
}

int main(int argc, char** argv) {
    std::vector<std::string> args = commandLineArgs(argc, argv);
    if (!args.empty() && args[0] == "lsp") return runLanguageServer(); // stdout es el canal del protocolo
    consoleInit();
    // Sin argumentos en una consola (o con doble clic en hyper.exe): consola interactiva
    if (args.empty() && stdinIsConsole()) return runRepl();
    if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") { printHelp(); return 0; }
    const std::string cmd = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());
    if (cmd == "version" || cmd == "--version" || cmd == "-v") { std::printf("HyperEZ %s\n", HYPEREZ_VERSION); return 0; }

    if (cmd == "run" || isSourceFile(cmd)) {
        if (cmd != "run") rest.insert(rest.begin(), cmd);
        bool pause = false;
        std::string file;
        std::vector<std::string> progArgs;
        bool passthrough = false;
        for (auto& a : rest) {
            if (passthrough) { progArgs.push_back(a); continue; }
            if (a == "--") { passthrough = true; continue; }
            if (a == "--pause" && file.empty()) { pause = true; continue; }
            if (file.empty()) { file = a; continue; }
            progArgs.push_back(a);
        }
        if (file.empty()) {
            if (fileExists("main.hz")) file = "main.hz";
            else if (fileExists("main.hez")) file = "main.hez";
            else { std::fprintf(stderr, "Uso: hyper run <archivo.hz>  (no hay main.hz en esta carpeta)\n"); return 2; }
        }
        if (!fileExists(file)) {
            std::fprintf(stderr, "%serror:%s no existe el archivo '%s'\n", color::red(), color::reset(), file.c_str());
            if (pause) waitForKey();
            return 1;
        }
        return cmdRun(file, progArgs, pause);
    }
    if (cmd == "repl") return runRepl();
    if (cmd == "watch") {
        std::string file;
        std::vector<std::string> progArgs;
        for (auto& a : rest) {
            if (a == "--") continue;
            if (file.empty() && isSourceFile(a)) file = a;
            else progArgs.push_back(a);
        }
        if (file.empty()) file = fileExists("main.hz") ? "main.hz" : fileExists("main.hez") ? "main.hez" : "";
        if (file.empty()) { std::fprintf(stderr, "Uso: hyper watch <archivo.hz>  (no hay main.hz en esta carpeta)\n"); return 2; }
        if (!fileExists(file)) {
            std::fprintf(stderr, "%serror:%s no existe el archivo '%s'\n", color::red(), color::reset(), file.c_str());
            return 1;
        }
        return runWatch(file, progArgs);
    }
    if (cmd == "check") {
        bool pause = std::find(rest.begin(), rest.end(), "--pause") != rest.end();
        rest.erase(std::remove(rest.begin(), rest.end(), "--pause"), rest.end());
        if (pause) {
            int r = cmdCheck(rest);
            waitForKey();
            return r;
        }
        if (rest.empty()) {
            if (fileExists("main.hz")) rest.push_back(".");
            else { std::fprintf(stderr, "Uso: hyper check <archivo.hz | carpeta>...\n"); return 2; }
        }
        return cmdCheck(rest);
    }
    if (cmd == "test") {
        bool update = false;
        std::vector<std::string> paths;
        for (auto& a : rest) { if (a == "--update") update = true; else paths.push_back(a); }
        return cmdTest(paths, update);
    }
    if (cmd == "new") return cmdNew(rest.empty() ? "" : rest[0]);
    if (cmd == "docs") return cmdDocs(rest.empty() ? "" : rest[0]);
#ifdef _WIN32
    if (cmd == "register") return registerFileTypes(exePath(), true) ? 0 : 1;
    if (cmd == "unregister") return registerFileTypes(exePath(), false) ? 0 : 1;
#endif
    std::fprintf(stderr, "Comando desconocido: %s\n\n", cmd.c_str());
    printHelp();
    return 2;
}
