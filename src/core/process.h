// Procesos: ejecutar programas capturando su salida, o lanzarlos compartiendo la consola.
// Lo usan 'hyper test', 'hyper watch' y el módulo Process del lenguaje.
#pragma once

#include "core/common.h"

namespace hz {

struct ProcessOptions {
    std::string cwd;                    // carpeta de trabajo (vacío: la actual)
    std::string input;                  // texto que recibe por la entrada estándar
    int timeoutMs = -1;                 // -1: sin límite
    bool mergeStderr = false;           // stderr junto con stdout en `out`
    std::vector<std::string> env;       // variables extra ("NOMBRE=valor")
    std::vector<std::string> unsetEnv;  // variables que se quitan
};

struct ProcessResult {
    bool started = false;   // false: no se pudo ejecutar (programa no encontrado...)
    bool timedOut = false;
    int code = -1;          // código de salida
    std::string out, err;
};

// Ejecuta un programa (argv[0] se busca en el PATH) y espera a que termine.
ProcessResult runCapture(const std::vector<std::string>& argv, const ProcessOptions& opt = {});
// Ejecuta una línea de órdenes con el intérprete del sistema (cmd.exe en Windows, /bin/sh en el resto).
ProcessResult runShell(const std::string& command, const ProcessOptions& opt = {});

// Proceso hijo que comparte la consola (hyper watch). nullptr si no se pudo iniciar.
void* spawnProcess(const std::vector<std::string>& argv);
// true si terminó antes de waitMs (-1: esperar siempre); deja su código en exitCode.
bool processFinished(void* p, int waitMs, int& exitCode);
void endProcess(void* p, bool kill); // libera el proceso (y antes lo termina si kill)

// Abre un archivo, carpeta o URL con la aplicación predeterminada del sistema.
bool openWithSystem(const std::string& target);
int currentProcessId();

} // namespace hz
