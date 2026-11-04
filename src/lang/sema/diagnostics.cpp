#include "lang/sema/diagnostics.h"

namespace hz {

void printDiagnostic(FILE* out, const Diagnostic& d, std::string_view source, const char* labelOverride) {
    const char* col = color::red();
    const char* label = "error";
    switch (d.severity) {
        case Severity::Error: col = color::red(); label = "error"; break;
        case Severity::Warning: col = color::yellow(); label = "aviso"; break;
        case Severity::Info: col = color::cyan(); label = "info"; break;
        case Severity::Hint: col = color::cyan(); label = "nota"; break;
    }
    if (labelOverride) label = labelOverride;
    std::fprintf(out, "%s%s%s", color::bold(), col, label);
    if (!d.code.empty()) std::fprintf(out, "[%s]", d.code.c_str());
    std::fprintf(out, "%s%s: %s%s\n", color::reset(), color::bold(), d.message.c_str(), color::reset());
    if (d.loc.line <= 0) {
        if (!d.file.empty()) std::fprintf(out, "  %s-->%s %s\n", color::blue(), color::reset(), d.file.c_str());
        return;
    }
    std::fprintf(out, "  %s-->%s %s:%d:%d\n", color::blue(), color::reset(), d.file.c_str(), d.loc.line, d.loc.col);
    // Línea de código
    int line = 1;
    size_t start = 0;
    for (size_t i = 0; i < source.size() && line < d.loc.line; i++) {
        if (source[i] == '\n') { line++; start = i + 1; }
    }
    if (line != d.loc.line) return;
    size_t end = source.find('\n', start);
    if (end == std::string_view::npos) end = source.size();
    std::string text(source.substr(start, end - start));
    if (!text.empty() && text.back() == '\r') text.pop_back();
    std::string num = std::to_string(d.loc.line);
    std::string pad(num.size(), ' ');
    std::string expanded;
    for (char c : text) expanded += (c == '\t') ? std::string("    ") : std::string(1, c);
    std::fprintf(out, " %s%s |%s %s\n", color::blue(), num.c_str(), color::reset(), expanded.c_str());
    // Columna visual (contando tabs como 4 y UTF-8 como 1)
    int visual = 0;
    size_t bytes = 0;
    int target = d.loc.col - 1;
    while (bytes < text.size() && (int)bytes < target) {
        if (text[bytes] == '\t') { visual += 4; bytes++; continue; }
        size_t i = bytes;
        utf8Decode(text, i);
        bytes = i;
        visual++;
    }
    int width = 1;
    if (d.loc.length > 1) {
        std::string_view rest = std::string_view(text).substr(std::min(text.size(), bytes));
        width = (int)utf8Length(rest.substr(0, std::min<size_t>(rest.size(), (size_t)d.loc.length)));
        if (width < 1) width = 1;
    }
    std::fprintf(out, " %s %s|%s %s%s%s%s\n", pad.c_str(), color::blue(), color::reset(), std::string(visual, ' ').c_str(), col,
                 std::string(width, '^').c_str(), color::reset());
    if (!d.hint.empty()) std::fprintf(out, " %s %s= %sayuda:%s %s\n", pad.c_str(), color::blue(), color::green(), color::reset(), d.hint.c_str());
}

} // namespace hz
