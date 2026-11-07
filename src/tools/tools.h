// Piezas compartidas de la herramienta hyper (CLI, consola, watch, servidor de lenguaje, integración con el sistema).
#pragma once

#include "core/common.h"
#include "core/process.h"

namespace hz {

class VM;

// Registra la biblioteca estándar y el preludio en una VM nueva.
void setupVM(VM& vm);

// Consola interactiva (hyper repl)
int runRepl();
// Ejecuta el programa y lo repite cada vez que cambia un archivo .hz de su carpeta (hyper watch)
int runWatch(const std::string& file, const std::vector<std::string>& args);

// Plantillas de 'hyper new': tipos de proyecto (juego, app, ia, script) y sus archivos
struct ProjectFile { std::string path, content; };
std::vector<std::string> projectKinds();
std::vector<ProjectFile> projectFiles(const std::string& kind, const std::string& title);
std::string projectTemplateMain(const std::string& title);
std::string projectTemplateModule();
std::string projectTemplateTest();
std::string projectTemplateReadme(const std::string& title);

// Referencia de la API en Markdown (a partir del catálogo de la VM)
std::string generateApiMarkdown(VM& vm);

// Servidor de lenguaje (Language Server Protocol) por stdin/stdout.
int runLanguageServer();
// El mismo servidor en memoria (editor web): un mensaje JSON-RPC → lista JSON con lo que responde
std::string languageServerMessage(const std::string& message);
// hyper build: HyperEZ nativo → código máquina x86-64 (aplicación UEFI o binario plano)
int nativeBuild(const std::vector<std::string>& args);

#ifdef _WIN32
// Asociación de .hz/.hez, iconos y menú contextual (HKEY_CURRENT_USER; no requiere administrador).
// classesRoot permite redirigir las claves en las pruebas (por defecto Software\Classes).
bool registerFileTypes(const std::string& hyperExe, bool install, const std::wstring& classesRoot = L"Software\\Classes",
                       bool quiet = false);
#elif !defined(__APPLE__)
// Linux/freedesktop: tipo MIME text/x-hyperez, lanzador .desktop e icono en ~/.local/share.
bool registerFileTypes(const std::string& hyperExe, bool install);
#endif

} // namespace hz
