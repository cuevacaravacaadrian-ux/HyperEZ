// HyperEZ — utilidades comunes compartidas por el lenguaje, el motor y las herramientas.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>

#include "core/version.h"

namespace hz {

// Ubicación en el código fuente (1-based).
struct SrcLoc {
    int line = 0;
    int col = 0;
    int offset = 0;
    int length = 0;
};

// ---- Texto ---------------------------------------------------------------

std::string formatNumber(double n);
std::string escapeString(std::string_view s, char quote = '"');
std::string toLower(std::string_view s);
std::string trim(std::string_view s);
std::vector<std::string> splitLines(std::string_view s);
bool startsWith(std::string_view s, std::string_view prefix);
bool endsWith(std::string_view s, std::string_view suffix);
int levenshtein(std::string_view a, std::string_view b);
// Devuelve la candidata más parecida o "" si ninguna es suficientemente similar.
std::string closestMatch(std::string_view word, const std::vector<std::string>& candidates);

// ---- UTF-8 ---------------------------------------------------------------

// Decodifica un punto de código en s[i]; avanza i. Devuelve 0xFFFD si es inválido.
uint32_t utf8Decode(std::string_view s, size_t& i);
void utf8Append(std::string& out, uint32_t cp);
size_t utf8Length(std::string_view s);
// Índice de byte del carácter número `charIndex` (o s.size()).
size_t utf8Offset(std::string_view s, size_t charIndex);

// ---- Sistema de archivos --------------------------------------------------

bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, std::string_view data);
// Escritura atómica: escribe a un temporal y lo renombra.
bool writeFileAtomic(const std::string& path, std::string_view data);
bool fileExists(const std::string& path);
bool dirExists(const std::string& path);
bool makeDirs(const std::string& path);
std::string pathJoin(const std::string& a, const std::string& b);
std::string pathDir(const std::string& p);
std::string pathFilename(const std::string& p);
std::string pathStem(const std::string& p);
std::string pathExt(const std::string& p);
std::string pathNormalize(const std::string& p);
std::string pathAbsolute(const std::string& p);
std::string currentDir();
std::string exePath();
std::string userDataDir(const std::string& app);
std::vector<std::string> listFilesRecursive(const std::string& dir);

// ---- Consola --------------------------------------------------------------

void consoleInit();
bool consoleColors();
// Lee una línea de la entrada estándar en UTF-8 (sin el salto de línea). false al final de la entrada.
bool readLine(std::string& out);
bool stdinIsConsole();
namespace color {
const char* red();
const char* yellow();
const char* blue();
const char* cyan();
const char* green();
const char* magenta();
const char* gray();
const char* bold();
const char* reset();
}

double nowSeconds();

} // namespace hz
