// Catálogo de la API nativa: fuente única de verdad para el analizador,
// el LSP (autocompletado, hover, firmas) y el generador de documentación.
#pragma once

#include "../core/common.h"

namespace hz {

struct ApiParam {
    std::string name;
    std::string type = "any";
    std::string def;       // valor por defecto (texto) si es opcional
    bool optional = false;
    bool rest = false;
};

enum class ApiKind : uint8_t { Function, Property, Method, Constant, Event, Class, Module, Field };

struct ApiEntry {
    ApiKind kind = ApiKind::Function;
    std::string owner;      // "Draw", "Vec2", "" (global)
    std::string name;
    std::vector<ApiParam> params;
    std::string returns = "void";
    std::string type;       // para propiedades/constantes
    std::string doc;
    std::string example;
    bool readonly = false;
    bool isStatic = false;
    std::string status = "estable"; // estable | beta | parcial
    int minArgs = 0, maxArgs = 0;   // -1 = sin límite
    std::string signature() const;  // "(x: number, y: number): void"
};

// Analiza "(x: number, y?: number = 2, ...resto: any): Tipo".
bool parseApiSignature(const std::string& sig, std::vector<ApiParam>& params, std::string& ret);

} // namespace hz
