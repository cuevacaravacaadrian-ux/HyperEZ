// Piezas compartidas de la herramienta hyper (CLI, servidor de lenguaje, integración con Windows).
#pragma once

#include "../core/common.h"

namespace hz {

class VM;

// Registra la biblioteca estándar y el preludio en una VM nueva.
void setupVM(VM& vm);

// Ejecuta un proceso y captura stdout+stderr. Devuelve el código de salida, o -1 si se agotó el tiempo.
int runProcess(const std::vector<std::string>& argv, const std::string& cwd, std::string& output, int timeoutMs);

#ifdef _WIN32
// Proceso hijo que comparte la consola (hyper watch). Devuelve su handle, o nullptr si no se pudo iniciar.
void* spawnProcess(const std::vector<std::string>& argv);
// true si terminó antes de waitMs (-1: esperar siempre); deja su código en exitCode.
bool processFinished(void* p, int waitMs, int& exitCode);
void endProcess(void* p, bool kill); // libera el handle (y antes lo termina si kill)
#endif

// Consola interactiva (hyper repl)
int runRepl();
// Ejecuta el programa y lo repite cada vez que cambia un archivo .hz de su carpeta (hyper watch)
int runWatch(const std::string& file, const std::vector<std::string>& args);

// Plantillas de 'hyper new'
std::string projectTemplateMain(const std::string& title);
std::string projectTemplateModule();
std::string projectTemplateTest();
std::string projectTemplateReadme(const std::string& title);

// Referencia de la API en Markdown (a partir del catálogo de la VM)
std::string generateApiMarkdown(VM& vm);

// Servidor de lenguaje (Language Server Protocol) por stdin/stdout.
int runLanguageServer();

#ifdef _WIN32
// Asociación de .hz/.hez, iconos y menú contextual (HKEY_CURRENT_USER; no requiere administrador).
// classesRoot permite redirigir las claves en las pruebas (por defecto Software\Classes).
bool registerFileTypes(const std::string& hyperExe, bool install, const std::wstring& classesRoot = L"Software\\Classes",
                       bool quiet = false);
#endif

} // namespace hz
