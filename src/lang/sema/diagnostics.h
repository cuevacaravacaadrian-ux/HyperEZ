// Diagnósticos del compilador: errores, avisos y pistas con ubicación exacta.
#pragma once

#include "core/common.h"

namespace hz {

enum class Severity { Error, Warning, Info, Hint };

struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;     // p. ej. "E0101"
    std::string message;
    std::string file;
    SrcLoc loc;
    std::string hint;     // sugerencia opcional ("¿quisiste decir ...?")
    std::vector<std::string> tags; // "unnecessary", "deprecated" (para el LSP)
};

class DiagnosticBag {
public:
    std::vector<Diagnostic> items;
    std::string file;

    void error(const SrcLoc& loc, std::string msg, std::string hint = "", std::string code = "") {
        items.push_back({Severity::Error, std::move(code), std::move(msg), file, loc, std::move(hint), {}});
    }
    void warning(const SrcLoc& loc, std::string msg, std::string hint = "", std::string code = "") {
        items.push_back({Severity::Warning, std::move(code), std::move(msg), file, loc, std::move(hint), {}});
    }
    void hintUnused(const SrcLoc& loc, std::string msg) {
        Diagnostic d{Severity::Hint, "W0201", std::move(msg), file, loc, "", {"unnecessary"}};
        items.push_back(std::move(d));
    }
    bool hasErrors() const {
        for (auto& d : items) if (d.severity == Severity::Error) return true;
        return false;
    }
    int errorCount() const {
        int n = 0;
        for (auto& d : items) if (d.severity == Severity::Error) n++;
        return n;
    }
};

// Imprime un diagnóstico al estilo:
//   error: mensaje
//     --> archivo.hz:3:5
//      3 | código
//        |     ^^^
void printDiagnostic(FILE* out, const Diagnostic& d, std::string_view source, const char* labelOverride = nullptr);

} // namespace hz
