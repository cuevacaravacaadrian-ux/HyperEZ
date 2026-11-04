// Formateador de HyperEZ (hyper fmt, el editor y el servidor de lenguaje).
//
// Ordena el aspecto del código sin cambiar nada de lo que significa:
//   - sangría de 4 espacios según las llaves, corchetes y paréntesis abiertos;
//   - una sangría más en las líneas que continúan una expresión (".map(...)", "and ...", "x +");
//   - quita los espacios al final de las líneas y las líneas en blanco de sobra (máximo 2 seguidas);
//   - termina el archivo con un solo salto de línea; respeta \n o \r\n.
// No toca el interior de los textos de varias líneas ni de los comentarios /* */.
// Comprueba que el resultado tiene exactamente los mismos tokens que el original.
#pragma once

#include <string>
#include <string_view>

namespace hz {

struct FormatResult {
    bool ok = false;      // false: el código tiene errores de sintaxis y no se formatea
    bool changed = false; // el texto formateado es distinto del original
    std::string text;     // texto formateado (o el original si !ok)
    std::string error;    // motivo si !ok, con la línea
};

FormatResult formatSource(std::string_view source);

} // namespace hz
