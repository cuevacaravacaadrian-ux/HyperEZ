// hyper — herramienta de línea de comandos de HyperEZ.
#include "lang/vm/vm.h"
#include "lang/sema/analyzer.h"
#include "lang/syntax/format.h"
#include "lang/compiler/disasm.h"
#include "tools/tools.h"

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
    const char *B = color::bold(), *R = color::reset(), *C = color::cyan(), *G = color::gray(), *Y = color::yellow();
    std::printf("\n  %s%sHyperEZ %s%s  %s·  programa en español: juegos, IA, web, datos, gráficos y más%s\n\n", B, C, HYPEREZ_VERSION, R, G, R);
    auto section = [&](const char* t) { std::printf("%s%s%s\n", Y, t, R); };
    auto row = [&](const char* cmd, const char* what) { std::printf("  %s%-34s%s %s\n", B, cmd, R, what); };
    section("Empezar");
    row("hyper", "consola interactiva: escribe código y ve el resultado");
    row("hyper new <nombre> [--tipo T]", "crea un proyecto: juego, app, ia, script, web o sitio");
    std::printf("\n");
    section("Ejecutar");
    row("hyper <archivo.hz> [args...]", "ejecuta un programa (args llegan en System.args)");
    row("hyper run [archivo.hz] [args...]", "igual; sin archivo ejecuta main.hz");
    row("hyper watch [archivo.hz]", "ejecuta y repite cada vez que guardas un .hz");
    row("hyper serve [carpeta] [--puerto N]", "sirve una web y recarga el navegador al guardar");
    std::printf("\n");
    section("Comprobar y probar");
    row("hyper check [archivos|carpetas]", "busca errores sin ejecutar");
    row("hyper test [carpeta] [--update]", "ejecuta las pruebas (por defecto tests/)");
    row("hyper fmt [archivos|carpetas] [--check]", "ordena la sangría y los espacios del código");
    std::printf("\n");
    section("Herramientas");
    row("hyper docs [salida.md]", "referencia de toda la API en Markdown");
    row("hyper bytecode <archivo.hz>", "muestra las instrucciones que genera el compilador");
    row("hyper lsp", "servidor de lenguaje para editores");
#ifndef __APPLE__
    row("hyper register | unregister", "asocia (o no) los .hz y .hez con este hyper");
#endif
    row("hyper version", "versión y datos de compilación");
    std::printf("\n");
    section("Opciones de run");
    row("--pause", "espera una tecla al terminar (doble clic, VS Code); también en check y test");
    row("--as <ruta>", "ejecuta el archivo como si estuviera en <ruta> (los editores, para ejecutar sin guardar)");
    row("--", "lo que sigue se pasa al programa aunque empiece por -");
    std::printf("\n%sDocumentación: %s%s%s\n\n", G, HYPEREZ_REPO_URL "/tree/main/docs", R, "");
}

static void printVersion() {
#if defined(_WIN32)
    const char* os = "windows";
#elif defined(__APPLE__)
    const char* os = "macos";
#else
    const char* os = "linux";
#endif
#if defined(__x86_64__) || defined(_M_X64)
    const char* arch = "x64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    const char* arch = "arm64";
#else
    const char* arch = "?";
#endif
#if defined(__clang__)
    std::string cc = std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    std::string cc = "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    std::string cc = "MSVC " + std::to_string(_MSC_VER);
#else
    std::string cc = "?";
#endif
    std::printf("HyperEZ %s\n", HYPEREZ_VERSION);
    std::printf("%s%s %s · compilado el %s con %s%s\n", color::gray(), os, arch, __DATE__, cc.c_str(), color::reset());
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
// fmt: sangría y espacios (sin cambiar el significado)
// ---------------------------------------------------------------------------

static int cmdFmt(const std::vector<std::string>& args) {
    bool check = false;
    std::vector<std::string> paths, files;
    for (auto& a : args) { if (a == "--check") check = true; else paths.push_back(a); }
    if (paths.empty()) paths.push_back(".");
    for (auto& p : paths) collectSources(p, files);
    if (files.empty()) { std::fprintf(stderr, "No hay archivos .hz o .hez que formatear\n"); return 2; }
    int changed = 0, failed = 0;
    for (auto& f : files) {
        std::string src;
        if (!readFile(f, src)) { std::fprintf(stderr, "%serror:%s no se puede leer %s\n", color::red(), color::reset(), f.c_str()); failed++; continue; }
        FormatResult r = formatSource(src);
        if (!r.ok) {
            std::fprintf(stderr, "%s✗%s %s: %s\n", color::red(), color::reset(), f.c_str(), r.error.c_str());
            failed++;
            continue;
        }
        if (!r.changed) continue;
        changed++;
        if (check) { std::printf("%s•%s %s necesita formato\n", color::yellow(), color::reset(), f.c_str()); continue; }
        if (!writeFileAtomic(f, r.text)) { std::fprintf(stderr, "%serror:%s no se puede escribir %s\n", color::red(), color::reset(), f.c_str()); failed++; continue; }
        std::printf("%s✓%s %s\n", color::green(), color::reset(), f.c_str());
    }
    if (check) {
        if (changed == 0 && failed == 0) std::printf("%s✓%s %zu archivo(s) con buen formato\n", color::green(), color::reset(), files.size());
        return changed || failed ? 1 : 0;
    }
    std::printf("%zu archivo(s): %d formateado(s), %d ya estaba(n) bien%s\n", files.size(), changed, (int)files.size() - changed - failed,
                failed ? (", " + std::to_string(failed) + " con errores").c_str() : "");
    return failed ? 1 : 0;
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
#else
    std::fflush(stdout);
    std::fprintf(stderr, "\n%sPulsa Intro para cerrar...%s", color::gray(), color::reset());
    std::fflush(stderr);
    std::string line;
    readLine(line);
#endif
}

// asPath: si no está vacío, el código de 'file' se ejecuta como si estuviera en asPath (las importaciones,
// System.scriptDir y los mensajes de error usan asPath). Lo usan los editores para ejecutar sin guardar.
static int cmdRun(const std::string& file, const std::vector<std::string>& args, bool pause, const std::string& asPath = "") {
    int code = 0;
    {
        VM vm;
        setupVM(vm);
        vm.projectRoot = pathDir(pathAbsolute(asPath.empty() ? file : asPath));
        vm.scriptArgs = args;
        ModuleUnit* unit = nullptr;
        std::string source;
        if (!asPath.empty() && readFile(file, source)) unit = vm.compileSource(pathAbsolute(asPath), source, true);
        else unit = vm.compileFile(file);
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
    if (files.empty()) { std::fprintf(stderr, "No se encontraron pruebas (.hez o .hz) en %s\n", roots[0].c_str()); return 2; }
    std::string self = exePath();
    int passed = 0, failed = 0;
    double t0 = nowSeconds();
    for (auto& f : files) {
        std::string abs = pathAbsolute(f);
        std::string dir = pathDir(abs);
        std::string expectedPath = abs.substr(0, abs.size() - pathExt(abs).size()) + ".out";
        std::string expected;
        bool hasExpected = readFile(expectedPath, expected);
        double t = nowSeconds();
        ProcessOptions po;
        po.cwd = dir;
        po.timeoutMs = 20000;
        po.mergeStderr = true;
        po.env = {"NO_COLOR=1"};        // sin colores en la salida capturada
        po.unsetEnv = {"HYPEREZ_COLOR"};
        ProcessResult pr = runCapture({self, "run", abs}, po);
        std::string& output = pr.out;
        int exitCode = pr.timedOut ? -1 : pr.started ? pr.code : -2;
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
// serve: servidor para desarrollar webs (recarga el navegador al guardar)
// ---------------------------------------------------------------------------

static int cmdServe(const std::vector<std::string>& args) {
    std::string folder = ".", port = "8080", host = "127.0.0.1";
    bool open = false, reload = true;
    for (size_t i = 0; i < args.size(); i++) {
        const std::string& a = args[i];
        if ((a == "--puerto" || a == "--port" || a == "-p") && i + 1 < args.size()) port = args[++i];
        else if (a == "--abrir" || a == "--open") open = true;
        else if (a == "--sin-recarga" || a == "--no-reload") reload = false;
        else if (a == "--red" || a == "--lan") host = "0.0.0.0";
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "Uso: hyper serve [carpeta] [--puerto 8080] [--abrir] [--sin-recarga] [--red]\n");
            return 2;
        } else folder = a;
    }
    if (!dirExists(folder)) {
        std::fprintf(stderr, "%serror:%s no existe la carpeta '%s'\n", color::red(), color::reset(), folder.c_str());
        return 1;
    }
    VM vm;
    setupVM(vm);
    vm.projectRoot = pathAbsolute(".");
    vm.scriptArgs = {pathAbsolute(folder), port, open ? "1" : "0", reload ? "1" : "0", host};
    const std::string code = "import web\n"
                             "web.devServer(System.args[0], { port: toNumber(System.args[1]), open: System.args[2] == \"1\",\n"
                             "                                reload: System.args[3] == \"1\", host: System.args[4] })\n";
    ModuleUnit* unit = vm.compileSource(pathJoin(vm.projectRoot, "hyper-serve.hz"), code, true, true);
    if (vm.unitHasErrors(unit)) { vm.printDiagnostics(unit, stderr, false); return 1; }
    bool ok = vm.runModule(unit);
    vm.flushOutput();
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// new
// ---------------------------------------------------------------------------

static int cmdNew(const std::vector<std::string>& args) {
    std::string name, kind = "juego";
    for (size_t i = 0; i < args.size(); i++) {
        if ((args[i] == "--tipo" || args[i] == "-t") && i + 1 < args.size()) kind = args[++i];
        else if (args[i].rfind("--tipo=", 0) == 0) kind = args[i].substr(7);
        else if (name.empty()) name = args[i];
    }
    auto kinds = projectKinds();
    if (name.empty() || std::find(kinds.begin(), kinds.end(), kind) == kinds.end()) {
        std::fprintf(stderr, "Uso: hyper new <nombre> [--tipo juego|app|ia|script|web|sitio]\n\n"
                             "  juego   juego de consola con teclado (por defecto)\n"
                             "  app     aplicación de consola con menú y datos guardados\n"
                             "  ia      red neuronal que aprende de un CSV\n"
                             "  script  herramienta de línea de órdenes con argumentos\n"
                             "  web     aplicación web: plantillas HTML, formulario, API JSON y CSS\n"
                             "  sitio   sitio web estático escrito en Markdown (blog, documentación...)\n");
        return 2;
    }
    if (fileExists(name) || (dirExists(name) && !listFilesRecursive(name).empty())) {
        std::fprintf(stderr, "%sYa existe '%s' y no está vacío.%s\n", color::red(), name.c_str(), color::reset());
        return 1;
    }
    std::string title = pathFilename(pathAbsolute(name));
    std::printf("%s✓%s Proyecto de tipo %s%s%s creado en %s%s%s\n\n", color::green(), color::reset(), color::bold(), kind.c_str(),
                color::reset(), color::bold(), name.c_str(), color::reset());
    for (auto& f : projectFiles(kind, title)) {
        std::string path = pathJoin(name, f.path);
        makeDirs(pathDir(path));
        writeFile(path, f.content);
        if (pathFilename(f.path) != ".gitkeep" && pathFilename(f.path) != ".gitignore")
            std::printf("  %s%s%s\n", color::gray(), f.path.c_str(), color::reset());
    }
    std::printf("\n  cd %s\n  hyper watch        %s# ejecuta main.hez y lo repite al guardar%s\n  hyper test         %s# ejecuta las pruebas%s\n",
                name.c_str(), color::gray(), color::reset(), color::gray(), color::reset());
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
    if (cmd == "version" || cmd == "--version" || cmd == "-v") { printVersion(); return 0; }

    if (cmd == "run" || isSourceFile(cmd)) {
        if (cmd != "run") rest.insert(rest.begin(), cmd);
        bool pause = false;
        std::string file, asPath;
        std::vector<std::string> progArgs;
        bool passthrough = false;
        for (size_t i = 0; i < rest.size(); i++) {
            const std::string& a = rest[i];
            if (passthrough) { progArgs.push_back(a); continue; }
            if (a == "--") { passthrough = true; continue; }
            if (a == "--pause" && file.empty()) { pause = true; continue; }
            if (a == "--as" && file.empty() && i + 1 < rest.size()) { asPath = rest[++i]; continue; }
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
        return cmdRun(file, progArgs, pause, asPath);
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
        bool update = false, pause = false;
        std::vector<std::string> paths;
        for (auto& a : rest) {
            if (a == "--update") update = true;
            else if (a == "--pause") pause = true;
            else paths.push_back(a);
        }
        int r = cmdTest(paths, update);
        if (pause) waitForKey();
        return r;
    }
    if (cmd == "new") return cmdNew(rest);
    if (cmd == "fmt" || cmd == "format") return cmdFmt(rest);
    if (cmd == "serve") return cmdServe(rest);
    if (cmd == "bytecode") {
        if (rest.empty()) { std::fprintf(stderr, "Uso: hyper bytecode <archivo.hz>\n"); return 2; }
        VM vm;
        setupVM(vm);
        vm.projectRoot = pathDir(pathAbsolute(rest[0]));
        ModuleUnit* unit = vm.compileFile(rest[0]);
        if (vm.unitHasErrors(unit)) { vm.printDiagnostics(unit, stderr, false); return 1; }
        std::fputs(disassemble(vm, unit->script).c_str(), stdout);
        return 0;
    }
    if (cmd == "docs") return cmdDocs(rest.empty() ? "" : rest[0]);
#ifndef __APPLE__
    if (cmd == "register") return registerFileTypes(exePath(), true) ? 0 : 1;
    if (cmd == "unregister") return registerFileTypes(exePath(), false) ? 0 : 1;
#endif
    // Scripts sin extensión con "#!/usr/bin/env hyper" en la primera línea
    if (fileExists(cmd)) {
        std::string head;
        if (readFile(cmd, head) && startsWith(head, "#!")) return cmdRun(cmd, rest, false);
    }
    std::fprintf(stderr, "Comando desconocido: %s\n\n", cmd.c_str());
    printHelp();
    return 2;
}
