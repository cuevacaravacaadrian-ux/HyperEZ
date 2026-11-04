// Formateador de HyperEZ: sangría por niveles de llaves, sin cambiar los tokens.
#include "lang/syntax/format.h"
#include "lang/syntax/lexer.h"

#include <algorithm>
#include <vector>

namespace hz {

namespace {

bool isOpener(Tok k) { return k == Tok::LParen || k == Tok::LBracket || k == Tok::LBrace; }
bool isCloser(Tok k) { return k == Tok::RParen || k == Tok::RBracket || k == Tok::RBrace; }

// Una línea que empieza así continúa la expresión de la anterior
bool startsContinuation(Tok k) {
    return k == Tok::Dot || k == Tok::QuestionDot || k == Tok::KwAnd || k == Tok::KwOr || k == Tok::AmpAmp || k == Tok::PipePipe ||
           k == Tok::QuestionQuestion;
}
// Una línea que acaba así sigue en la siguiente
bool endsContinuation(Tok k) {
    switch (k) {
        case Tok::Plus: case Tok::Minus: case Tok::Star: case Tok::Slash: case Tok::Percent: case Tok::StarStar:
        case Tok::Eq: case Tok::PlusEq: case Tok::MinusEq: case Tok::StarEq: case Tok::SlashEq: case Tok::PercentEq:
        case Tok::QuestionQuestionEq: case Tok::EqEq: case Tok::NotEq: case Tok::Less: case Tok::LessEq: case Tok::Greater:
        case Tok::GreaterEq: case Tok::AmpAmp: case Tok::PipePipe: case Tok::KwAnd: case Tok::KwOr: case Tok::QuestionQuestion:
        case Tok::Arrow: case Tok::ThinArrow: case Tok::Amp: case Tok::Pipe: case Tok::Caret: case Tok::Shl: case Tok::Shr:
            return true;
        default: return false;
    }
}

int lineOfOffset(const std::vector<size_t>& lineStarts, size_t off) {
    // búsqueda binaria: última línea cuyo inicio es <= off (líneas desde 1)
    size_t lo = 0, hi = lineStarts.size();
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (lineStarts[mid] <= off) lo = mid;
        else hi = mid;
    }
    return (int)lo + 1;
}

bool sameTokens(const std::vector<Token>& a, const std::vector<Token>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].kind != b[i].kind || a[i].text != b[i].text || a[i].parts.size() != b[i].parts.size()) return false;
        if (a[i].kind == Tok::Number && a[i].number != b[i].number) return false;
        for (size_t k = 0; k < a[i].parts.size(); k++)
            if (a[i].parts[k].text != b[i].parts[k].text || a[i].parts[k].isExpr != b[i].parts[k].isExpr) return false;
    }
    return true;
}

} // namespace

FormatResult formatSource(std::string_view src) {
    FormatResult r;
    r.text = std::string(src);
    DiagnosticBag bag;
    Lexer lx(src, bag);
    std::vector<Token> toks = lx.tokenize();
    for (auto& d : bag.items) {
        if (d.severity == Severity::Error) {
            r.error = "línea " + std::to_string(d.loc.line) + ": " + d.message;
            return r;
        }
    }

    // Líneas (sin el salto); se recuerda si el archivo usa \r\n
    bool crlf = src.find("\r\n") != std::string_view::npos;
    std::vector<std::string_view> lines;
    std::vector<size_t> lineStarts;
    for (size_t s = 0;;) {
        size_t e = src.find('\n', s);
        lineStarts.push_back(s);
        std::string_view l = src.substr(s, (e == std::string_view::npos ? src.size() : e) - s);
        if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
        lines.push_back(l);
        if (e == std::string_view::npos) break;
        s = e + 1;
    }
    size_t n = lines.size();
    // keep[l]: línea dentro de un texto o comentario de varias líneas (no se toca)
    // raw[l]: en esta línea empieza un texto de varias líneas (no se quitan sus espacios finales)
    std::vector<char> keep(n + 2, 0), raw(n + 2, 0);
    std::vector<std::vector<const Token*>> byLine(n + 2);
    for (auto& t : toks) {
        if (t.kind == Tok::Eof) continue;
        int l0 = t.loc.line;
        if (l0 < 1 || l0 > (int)n) continue;
        byLine[l0].push_back(&t);
        if (t.kind == Tok::String) {
            int l1 = lineOfOffset(lineStarts, (size_t)(t.loc.offset + std::max(0, t.loc.length - 1)));
            if (l1 > l0) {
                raw[l0] = 1;
                for (int l = l0 + 1; l <= l1; l++) keep[l] = 1;
            }
        }
    }
    for (auto& c : lx.comments) {
        if (!c.block) continue;
        size_t end = src.find("*/", (size_t)c.loc.offset + 2);
        if (end == std::string_view::npos) end = src.size();
        int l0 = c.loc.line, l1 = lineOfOffset(lineStarts, end);
        for (int l = l0 + 1; l <= l1 && l <= (int)n; l++) keep[l] = 1;
    }

    // Nivel de sangría de cada línea: número de líneas distintas con paréntesis aún abiertos
    // aligned: tras el paréntesis hay más código en su línea ("foo(a,"): las líneas siguientes pueden
    // estar alineadas a mano con él, y esa alineación se respeta
    struct Open { int line; bool aligned; };
    std::vector<Open> openLines; // aperturas pendientes (líneas no decrecientes)
    auto levelOf = [](const std::vector<Open>& st, size_t count) {
        int lv = 0, last = -1;
        for (size_t i = 0; i < count; i++) if (st[i].line != last) { lv++; last = st[i].line; }
        return lv;
    };
    std::string out;
    out.reserve(src.size() + src.size() / 8);
    const char* eol = crlf ? "\r\n" : "\n";
    Tok prevLast = Tok::Eof;
    int blankRun = 0;
    std::vector<std::string> outLines;
    outLines.reserve(n);
    for (size_t l = 1; l <= n; l++) {
        auto& lt = byLine[l];
        std::string_view text = lines[l - 1];
        if (keep[l]) {
            outLines.emplace_back(text);
            blankRun = 0;
        } else {
            size_t a = text.find_first_not_of(" \t");
            if (a == std::string_view::npos) {
                if (++blankRun <= 2) outLines.emplace_back();
            } else {
                blankRun = 0;
                size_t closers = 0;
                while (closers < lt.size() && isCloser(lt[closers]->kind)) closers++;
                size_t depth = openLines.size() >= closers ? openLines.size() - closers : 0;
                int level = levelOf(openLines, depth);
                bool cont = (!lt.empty() && startsContinuation(lt[0]->kind)) || endsContinuation(prevLast);
                if (cont && closers == 0) level++;
                size_t indent = 4 * (size_t)level;
                size_t original = 0;
                for (size_t i = 0; i < a; i++) original += text[i] == '\t' ? 4 : 1;
                bool alignedInside = depth > 0 && openLines[depth - 1].aligned && openLines[depth - 1].line < (int)l;
                if ((alignedInside || (cont && closers == 0)) && original > indent) indent = original;
                std::string_view body = text.substr(a);
                if (!raw[l]) {
                    size_t b = body.find_last_not_of(" \t");
                    body = body.substr(0, b + 1);
                }
                std::string line(indent, ' ');
                line += body;
                outLines.push_back(std::move(line));
            }
        }
        for (size_t i = 0; i < lt.size(); i++) {
            if (isOpener(lt[i]->kind)) openLines.push_back({(int)l, i + 1 < lt.size()});
            else if (isCloser(lt[i]->kind) && !openLines.empty()) openLines.pop_back();
        }
        if (!lt.empty()) prevLast = lt.back()->kind;
    }
    // Sin líneas en blanco al final; un solo salto de línea
    while (!outLines.empty() && outLines.back().empty()) outLines.pop_back();
    for (auto& l : outLines) { out += l; out += eol; }

    // Red de seguridad: los tokens deben ser exactamente los mismos
    DiagnosticBag bag2;
    Lexer lx2(out, bag2);
    if (!sameTokens(toks, lx2.tokenize())) {
        r.error = "el formateador cambiaría el significado del código (error interno): no se ha tocado";
        return r;
    }
    r.ok = true;
    r.changed = out != src;
    r.text = std::move(out);
    return r;
}

} // namespace hz
