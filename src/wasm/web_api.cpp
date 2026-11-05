// HyperEZ en el navegador (WebAssembly, con Emscripten): lo que usa el editor web de projects/web.
//
//   hz_run(ruta)        ejecuta un programa del sistema de archivos virtual (el editor escribe ahí los
//                       archivos del proyecto, así funcionan las importaciones); devuelve el código de salida
//   hz_lsp(mensaje)     el servidor de lenguaje de VS Code en memoria: errores, autocompletado, ayuda...
//   hz_format(código)   el formateador (hyper fmt)
//   hz_version()        la versión
//
// La salida de print va a stdout/stderr: el Worker que carga el módulo la pasa a la página.
#include "core/version.h"
#include "lang/sema/analyzer.h"
#include "lang/syntax/format.h"
#include "lang/vm/vm.h"
#include "tools/tools.h"

#include <emscripten/emscripten.h>

#include <cstdio>
#include <string>

void hz::setupVM(VM& vm) {
    registerStdlib(vm);
    registerMathTypes(vm);
    buildWorld(vm);
    vm.loadPrelude();
}

namespace {
std::string g_result; // lo que devuelven hz_lsp y hz_format (válido hasta la siguiente llamada)
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int hz_run(const char* path) {
    using namespace hz;
    int code = 0;
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
            bool ok = vm.runModule(unit);
            if (ok) ok = vm.drainScheduler();
            vm.flushOutput();
            if (vm.testsPassed + vm.testsFailed > 0)
                std::fprintf(stderr, "\n%d prueba%s: %d correcta%s%s\n", vm.testsPassed + vm.testsFailed,
                             vm.testsPassed + vm.testsFailed == 1 ? "" : "s", vm.testsPassed, vm.testsPassed == 1 ? "" : "s",
                             vm.testsFailed ? (", " + std::to_string(vm.testsFailed) + " fallida" + (vm.testsFailed == 1 ? "" : "s")).c_str() : "");
            code = ok && !vm.hadUncaught && vm.testsFailed == 0 ? 0 : 1;
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

} // extern "C"
