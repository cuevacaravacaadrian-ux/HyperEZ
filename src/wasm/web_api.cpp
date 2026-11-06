// HyperEZ en el navegador (WebAssembly, con Emscripten): lo que usa el editor web de projects/web.
//
//   hz_run(ruta)        ejecuta un programa del sistema de archivos virtual (el editor escribe ahí los
//                       archivos del proyecto, así funcionan las importaciones); devuelve el código de salida
//   hz_lsp(mensaje)     el servidor de lenguaje de VS Code en memoria: errores, autocompletado, ayuda...
//   hz_format(código)   el formateador (hyper fmt)
//   hz_version()        la versión
//   hz_new_project(tipo, nombre)  los archivos de 'hyper new' (JSON: [{"path", "content"}]) o "" si el tipo no existe
//   hz_tests_passed() / hz_tests_failed()  las pruebas de la última ejecución (para "Ejecutar las pruebas")
//
// La salida de print va a stdout/stderr: el Worker que carga el módulo la pasa a la página.
#include "core/version.h"
#include "lang/sema/analyzer.h"
#include "lang/syntax/format.h"
#include "lang/vm/vm.h"
#include "tools/tools.h"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cstdio>
#include <string>

void hz::setupVM(VM& vm) {
    registerStdlib(vm);
    registerMathTypes(vm);
    buildWorld(vm);
    vm.loadPrelude();
}

namespace {
std::string g_result; // lo que devuelven hz_lsp, hz_format y hz_new_project (válido hasta la siguiente llamada)
int g_passed = 0, g_failed = 0;

std::string jsonString(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o + "\"";
}
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int hz_run(const char* path) {
    using namespace hz;
    int code = 0;
    g_passed = g_failed = 0;
    {
        VM vm;
        setupVM(vm);
        std::string file = path;
        vm.projectRoot = pathDir(pathAbsolute(file));
        ModuleUnit* unit = vm.compileFile(file);
        if (vm.unitHasErrors(unit)) {
            vm.printDiagnostics(unit, stderr, false);
            std::fprintf(stderr, "No se puede ejecutar: corrige los errores de arriba.\n");
            code = 1;
        } else {
            bool ok = true, exited = false;
            try {
                ok = vm.runModule(unit);
                if (ok) ok = vm.drainScheduler();
            } catch (ProgramExit& e) { // Process.exit(código)
                exited = true;
                code = e.code;
            }
            vm.flushOutput();
            if (vm.testsPassed + vm.testsFailed > 0)
                std::fprintf(stdout, "\n%d prueba%s: %d correcta%s%s\n", vm.testsPassed + vm.testsFailed,
                             vm.testsPassed + vm.testsFailed == 1 ? "" : "s", vm.testsPassed, vm.testsPassed == 1 ? "" : "s",
                             vm.testsFailed ? (", " + std::to_string(vm.testsFailed) + " fallida" + (vm.testsFailed == 1 ? "" : "s")).c_str() : "");
            if (!exited) code = ok && !vm.hadUncaught && vm.testsFailed == 0 ? 0 : 1;
            else if (code == 0 && vm.testsFailed) code = 1;
            g_passed = vm.testsPassed;
            g_failed = vm.testsFailed;
        }
    }
    std::fflush(stdout);
    std::fflush(stderr);
    return code;
}

EMSCRIPTEN_KEEPALIVE const char* hz_lsp(const char* message) {
    g_result = hz::languageServerMessage(message);
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE const char* hz_format(const char* source) {
    auto r = hz::formatSource(source);
    g_result = r.ok ? r.text : std::string("\x01") + r.error; // \x01 delante: no se pudo formatear
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE const char* hz_version() { return HYPEREZ_VERSION; }

EMSCRIPTEN_KEEPALIVE const char* hz_new_project(const char* kind, const char* title) {
    auto kinds = hz::projectKinds();
    g_result.clear();
    if (std::find(kinds.begin(), kinds.end(), std::string(kind)) == kinds.end()) return g_result.c_str();
    g_result = "[";
    bool first = true;
    for (auto& f : hz::projectFiles(kind, title)) {
        if (!first) g_result += ",";
        first = false;
        g_result += "{\"path\":" + jsonString(f.path) + ",\"content\":" + jsonString(f.content) + "}";
    }
    g_result += "]";
    return g_result.c_str();
}

EMSCRIPTEN_KEEPALIVE int hz_tests_passed() { return g_passed; }
EMSCRIPTEN_KEEPALIVE int hz_tests_failed() { return g_failed; }

} // extern "C"
