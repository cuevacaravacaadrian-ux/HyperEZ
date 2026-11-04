#include "lang/syntax/lexer.h"

#include <cmath>
#include <cstring>

namespace hz {

static const struct { const char* word; Tok kind; } kKeywords[] = {
    {"var", Tok::KwVar}, {"let", Tok::KwLet}, {"const", Tok::KwConst}, {"function", Tok::KwFunction},
    {"return", Tok::KwReturn}, {"if", Tok::KwIf}, {"else", Tok::KwElse}, {"for", Tok::KwFor},
    {"foreach", Tok::KwForeach}, {"in", Tok::KwIn}, {"while", Tok::KwWhile}, {"repeat", Tok::KwRepeat},
    {"break", Tok::KwBreak}, {"continue", Tok::KwContinue}, {"class", Tok::KwClass}, {"struct", Tok::KwStruct},
    {"constructor", Tok::KwConstructor}, {"this", Tok::KwThis}, {"super", Tok::KwSuper},
    {"extends", Tok::KwExtends}, {"implements", Tok::KwImplements}, {"interface", Tok::KwInterface},
    {"enum", Tok::KwEnum}, {"import", Tok::KwImport}, {"public", Tok::KwPublic}, {"private", Tok::KwPrivate},
    {"protected", Tok::KwProtected}, {"static", Tok::KwStatic}, {"true", Tok::KwTrue}, {"false", Tok::KwFalse},
    {"null", Tok::KwNull}, {"and", Tok::KwAnd}, {"or", Tok::KwOr}, {"not", Tok::KwNot}, {"is", Tok::KwIs},
    {"new", Tok::KwNew}, {"try", Tok::KwTry}, {"catch", Tok::KwCatch}, {"finally", Tok::KwFinally},
    {"throw", Tok::KwThrow}, {"async", Tok::KwAsync}, {"await", Tok::KwAwait}, {"match", Tok::KwMatch},
};

Tok keywordKind(std::string_view word) {
    for (auto& k : kKeywords) if (word == k.word) return k.kind;
    return Tok::Ident;
}

bool isKeyword(Tok t) { return t >= Tok::KwVar; }

const char* tokName(Tok t) {
    switch (t) {
        case Tok::Eof: return "fin del archivo";
        case Tok::Error: return "error";
        case Tok::Ident: return "identificador";
        case Tok::Number: return "número";
        case Tok::String: return "texto";
        case Tok::LParen: return "'('";
        case Tok::RParen: return "')'";
        case Tok::LBracket: return "'['";
        case Tok::RBracket: return "']'";
        case Tok::LBrace: return "'{'";
        case Tok::RBrace: return "'}'";
        case Tok::Comma: return "','";
        case Tok::Dot: return "'.'";
        case Tok::Semicolon: return "';'";
        case Tok::Colon: return "':'";
        case Tok::Question: return "'?'";
        case Tok::QuestionDot: return "'?.'";
        case Tok::QuestionQuestion: return "'??'";
        case Tok::Arrow: return "'=>'";
        case Tok::ThinArrow: return "'->'";
        case Tok::DotDot: return "'..'";
        case Tok::DotDotEq: return "'..='";
        case Tok::Ellipsis: return "'...'";
        case Tok::At: return "'@'";
        case Tok::Plus: return "'+'";
        case Tok::Minus: return "'-'";
        case Tok::Star: return "'*'";
        case Tok::Slash: return "'/'";
        case Tok::Percent: return "'%'";
        case Tok::StarStar: return "'**'";
        case Tok::Eq: return "'='";
        case Tok::PlusEq: return "'+='";
        case Tok::MinusEq: return "'-='";
        case Tok::StarEq: return "'*='";
        case Tok::StarStarEq: return "'**='";
        case Tok::PipeGreater: return "'|>'";
        case Tok::SlashEq: return "'/='";
        case Tok::PercentEq: return "'%='";
        case Tok::QuestionQuestionEq: return "'?\?='";
        case Tok::EqEq: return "'=='";
        case Tok::NotEq: return "'!='";
        case Tok::Less: return "'<'";
        case Tok::LessEq: return "'<='";
        case Tok::Greater: return "'>'";
        case Tok::GreaterEq: return "'>='";
        case Tok::AmpAmp: return "'&&'";
        case Tok::PipePipe: return "'||'";
        case Tok::Bang: return "'!'";
        case Tok::Amp: return "'&'";
        case Tok::Pipe: return "'|'";
        case Tok::Caret: return "'^'";
        case Tok::Tilde: return "'~'";
        case Tok::Shl: return "'<<'";
        case Tok::Shr: return "'>>'";
        case Tok::PlusPlus: return "'++'";
        case Tok::MinusMinus: return "'--'";
        default: break;
    }
    for (auto& k : kKeywords) if (k.kind == t) return k.word;
    return "?";
}

Lexer::Lexer(std::string_view source, DiagnosticBag& d, SrcLoc base) : src(source), diags(d) {
    if (base.line > 0) { line = base.line; col = base.col; baseOffset = base.offset; }
    // Saltar BOM UTF-8
    if (src.size() >= 3 && (unsigned char)src[0] == 0xEF && (unsigned char)src[1] == 0xBB && (unsigned char)src[2] == 0xBF) pos = 3;
    // "#!/usr/bin/env hyper" en la primera línea (scripts ejecutables en Linux y macOS): se ignora
    if (base.line <= 0 && src.compare(pos, 2, "#!") == 0) {
        while (pos < src.size() && src[pos] != '\n') pos++;
    }
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') { line++; col = 1; }
    else col++;
    return c;
}

bool Lexer::match(char c) {
    if (peek() == c) { advance(); return true; }
    return false;
}

void Lexer::skipTrivia() {
    for (;;) {
        char c = peek();
        if (c == '\n') { sawNewline = true; advance(); }
        else if (c == ' ' || c == '\t' || c == '\r') advance();
        else if (c == '/' && peek(1) == '/') {
            SrcLoc start = here();
            bool doc = peek(2) == '/' && peek(3) != '/';
            size_t s = pos;
            while (pos < src.size() && peek() != '\n') advance();
            comments.push_back({start, std::string(src.substr(s, pos - s)), false, doc});
        } else if (c == '/' && peek(1) == '*') {
            SrcLoc start = here();
            bool doc = peek(2) == '*' && peek(3) != '/';
            size_t s = pos;
            advance(); advance();
            int depth = 1;
            while (pos < src.size() && depth > 0) {
                if (peek() == '/' && peek(1) == '*') { advance(); advance(); depth++; }
                else if (peek() == '*' && peek(1) == '/') { advance(); advance(); depth--; }
                else { if (peek() == '\n') sawNewline = true; advance(); }
            }
            if (depth > 0) diags.error(start, "Comentario de bloque sin cerrar", "añade */ al final del comentario", "E0001");
            comments.push_back({start, std::string(src.substr(s, pos - s)), true, doc});
        } else break;
    }
}

Token Lexer::make(Tok k, SrcLoc start) {
    Token t;
    t.kind = k;
    t.loc = start;
    t.loc.length = (int)pos + baseOffset - start.offset;
    t.text = std::string(src.substr(start.offset - baseOffset, t.loc.length));
    t.newlineBefore = sawNewline;
    sawNewline = false;
    return t;
}

static bool isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_' || (unsigned char)c >= 0x80; }
static bool isIdentPart(char c) { return std::isalnum((unsigned char)c) || c == '_' || (unsigned char)c >= 0x80; }

Token Lexer::number(SrcLoc start) {
    std::string digits;
    bool isInt = true;
    double value = 0;
    auto readRadix = [&](int radix) {
        advance(); advance();
        std::string d;
        while (std::isxdigit((unsigned char)peek()) || peek() == '_') { char c = advance(); if (c != '_') d += c; }
        if (d.empty()) { diags.error(start, "Número mal formado", "", "E0002"); return 0.0; }
        double v = 0;
        for (char c : d) {
            int k = std::isdigit((unsigned char)c) ? c - '0' : std::tolower((unsigned char)c) - 'a' + 10;
            if (k >= radix) { diags.error(start, std::string("Dígito '") + c + "' no válido en este número", "", "E0002"); return 0.0; }
            v = v * radix + k;
        }
        return v;
    };
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) value = readRadix(16);
    else if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) value = readRadix(2);
    else if (peek() == '0' && (peek(1) == 'o' || peek(1) == 'O')) value = readRadix(8);
    else {
        while (std::isdigit((unsigned char)peek()) || peek() == '_') { char c = advance(); if (c != '_') digits += c; }
        if (peek() == '.' && std::isdigit((unsigned char)peek(1))) {
            isInt = false;
            digits += advance();
            while (std::isdigit((unsigned char)peek()) || peek() == '_') { char c = advance(); if (c != '_') digits += c; }
        }
        if ((peek() == 'e' || peek() == 'E') &&
            (std::isdigit((unsigned char)peek(1)) || ((peek(1) == '+' || peek(1) == '-') && std::isdigit((unsigned char)peek(2))))) {
            isInt = false;
            digits += advance();
            if (peek() == '+' || peek() == '-') digits += advance();
            while (std::isdigit((unsigned char)peek())) digits += advance();
        }
        value = std::strtod(digits.c_str(), nullptr);
    }
    if (isIdentStart(peek())) {
        SrcLoc l = here();
        diags.error(l, "Un número no puede ir seguido de letras", "separa el número del identificador con un espacio u operador", "E0002");
        while (isIdentPart(peek())) advance();
    }
    Token t = make(Tok::Number, start);
    t.number = value;
    t.isInt = isInt;
    return t;
}

Token Lexer::identifier(SrcLoc start) {
    while (isIdentPart(peek())) advance();
    Token t = make(Tok::Ident, start);
    t.kind = keywordKind(t.text);
    return t;
}

bool Lexer::readEscape(std::string& out) {
    SrcLoc l = here();
    advance(); // '\'
    if (pos >= src.size()) return false;
    char c = advance();
    switch (c) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case '0': out += '\0'; break;
        case '\\': out += '\\'; break;
        case '"': out += '"'; break;
        case '\'': out += '\''; break;
        case '$': out += '$'; break;
        case '{': out += '{'; break;
        case '}': out += '}'; break;
        case 'u': {
            bool braces = match('{');
            std::string hex;
            while (std::isxdigit((unsigned char)peek()) && hex.size() < 6) hex += advance();
            if (braces && !match('}')) { diags.error(l, "Escape \\u{...} sin cerrar", "", "E0003"); break; }
            if (hex.empty()) { diags.error(l, "Escape \\u sin dígitos hexadecimales", "", "E0003"); break; }
            utf8Append(out, (uint32_t)std::strtoul(hex.c_str(), nullptr, 16));
            break;
        }
        default:
            l.length = 2;
            diags.error(l, std::string("Secuencia de escape desconocida '\\") + c + "'",
                        "escapes válidos: \\n \\t \\r \\0 \\\\ \\\" \\' \\$ \\u{XXXX}", "E0003");
    }
    return true;
}

// "${precio:.2f}": el formato va al final, detrás de ':' (como en Python). Ese ':' cuenta si está fuera de
// paréntesis, corchetes, llaves y textos, no hay antes un '?' de condición (a ? b : c) y lo que sigue es
// un formato válido: [[relleno]alineación][+][0][ancho][, o _][.precisión][tipo]
static bool isFormatSpec(const std::string& s) {
    auto isAlign = [](char c) { return c == '<' || c == '>' || c == '^'; };
    size_t n = s.size(), i = 0;
    if (n == 0) return false;
    unsigned char c0 = (unsigned char)s[0];
    size_t cl = c0 >= 0xF0 ? 4 : c0 >= 0xE0 ? 3 : c0 >= 0xC0 ? 2 : 1;
    if (n > cl && isAlign(s[cl])) i = cl + 1;
    else if (isAlign(s[0])) i = 1;
    if (i < n && s[i] == '+') i++;
    if (i < n && s[i] == '0') i++;
    while (i < n && std::isdigit((unsigned char)s[i])) i++;
    if (i < n && (s[i] == ',' || s[i] == '_')) i++;
    if (i < n && s[i] == '.') {
        i++;
        if (i >= n || !std::isdigit((unsigned char)s[i])) return false;
        while (i < n && std::isdigit((unsigned char)s[i])) i++;
    }
    if (i < n && std::strchr("feE%dxXbos", s[i])) i++;
    return i == n;
}

static bool splitFormatSpec(std::string& code, std::string& spec) {
    int depth = 0;
    bool ternary = false;
    size_t colon = std::string::npos;
    for (size_t i = 0; i < code.size(); i++) {
        char c = code[i];
        if (c == '"' || c == '\'') {
            char q = c;
            for (i++; i < code.size() && code[i] != q; i++) if (code[i] == '\\') i++;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') depth++;
        else if (c == ')' || c == ']' || c == '}') depth--;
        else if (depth == 0 && c == '?') {
            if (i + 1 < code.size() && (code[i + 1] == '?' || code[i + 1] == '.')) { i++; continue; } // ?? y ?.
            ternary = true;
        } else if (depth == 0 && c == ':' && !ternary) colon = i;
    }
    if (colon == std::string::npos || !isFormatSpec(code.substr(colon + 1))) return false;
    spec = code.substr(colon + 1);
    code.erase(colon);
    return true;
}

Token Lexer::string(SrcLoc start, char quote, bool raw) {
    bool triple = peek() == quote && peek(1) == quote && peek(2) == quote;
    if (triple) { advance(); advance(); advance(); }
    else advance();
    std::vector<StrPart> parts;
    std::string cur;
    SrcLoc curLoc = here();
    bool interpolated = false;
    bool closed = false;
    while (pos < src.size()) {
        char c = peek();
        if (triple) {
            if (c == quote && peek(1) == quote && peek(2) == quote) { advance(); advance(); advance(); closed = true; break; }
        } else {
            if (c == quote) { advance(); closed = true; break; }
            if (c == '\n') break;
        }
        if (raw) { cur += advance(); continue; } // r"..." : todo literal (útil en expresiones regulares y rutas)
        if (c == '\\') { if (!readEscape(cur)) break; continue; }
        if (c == '$' && peek(1) == '{') {
            interpolated = true;
            if (!cur.empty() || parts.empty()) parts.push_back({false, cur, "", curLoc});
            cur.clear();
            advance(); advance();
            SrcLoc exprLoc = here();
            size_t s = pos;
            int depth = 1;
            while (pos < src.size()) {
                char d = peek();
                if (d == '\n' && !triple) break;
                if (d == '"' || d == '\'') {
                    char q = advance();
                    while (pos < src.size() && peek() != q && peek() != '\n') { if (peek() == '\\') advance(); advance(); }
                    if (peek() == q) advance();
                    continue;
                }
                if (d == '{') depth++;
                if (d == '}') { depth--; if (depth == 0) break; }
                advance();
            }
            if (depth != 0) { diags.error(exprLoc, "Interpolación ${...} sin cerrar", "añade '}'", "E0004"); break; }
            std::string code(src.substr(s, pos - s));
            advance(); // '}'
            std::string spec;
            splitFormatSpec(code, spec);
            if (trim(code).empty()) diags.error(exprLoc, "Interpolación vacía ${}", "", "E0004");
            parts.push_back({true, code, spec, exprLoc});
            curLoc = here();
            continue;
        }
        cur += advance();
    }
    if (!closed) diags.error(start, "Texto sin cerrar", std::string("falta la comilla de cierre ") + quote, "E0005");
    if (!cur.empty() || parts.empty()) parts.push_back({false, cur, "", curLoc});
    Token t = make(Tok::String, start);
    t.parts = std::move(parts);
    t.interpolated = interpolated;
    return t;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    for (;;) {
        skipTrivia();
        SrcLoc start = here();
        if (pos >= src.size()) { out.push_back(make(Tok::Eof, start)); break; }
        char c = peek();
        if (std::isdigit((unsigned char)c)) { out.push_back(number(start)); continue; }
        if (c == 'r' && (peek(1) == '"' || peek(1) == '\'')) { advance(); out.push_back(string(start, peek(), true)); continue; }
        if (isIdentStart(c)) { out.push_back(identifier(start)); continue; }
        if (c == '"' || c == '\'') { out.push_back(string(start, c)); continue; }
        advance();
        Tok k = Tok::Error;
        switch (c) {
            case '(': k = Tok::LParen; break;
            case ')': k = Tok::RParen; break;
            case '[': k = Tok::LBracket; break;
            case ']': k = Tok::RBracket; break;
            case '{': k = Tok::LBrace; break;
            case '}': k = Tok::RBrace; break;
            case ',': k = Tok::Comma; break;
            case ';': k = Tok::Semicolon; break;
            case ':': k = Tok::Colon; break;
            case '@': k = Tok::At; break;
            case '~': k = Tok::Tilde; break;
            case '^': k = Tok::Caret; break;
            case '.':
                if (match('.')) {
                    if (match('.')) k = Tok::Ellipsis;
                    else if (match('=')) k = Tok::DotDotEq;
                    else k = Tok::DotDot;
                } else k = Tok::Dot;
                break;
            case '?':
                if (match('?')) k = match('=') ? Tok::QuestionQuestionEq : Tok::QuestionQuestion;
                else if (peek() == '.' && !std::isdigit((unsigned char)peek(1))) { advance(); k = Tok::QuestionDot; }
                else k = Tok::Question;
                break;
            case '+': k = match('+') ? Tok::PlusPlus : match('=') ? Tok::PlusEq : Tok::Plus; break;
            case '-': k = match('-') ? Tok::MinusMinus : match('=') ? Tok::MinusEq : match('>') ? Tok::ThinArrow : Tok::Minus; break;
            case '*': k = match('*') ? (match('=') ? Tok::StarStarEq : Tok::StarStar) : match('=') ? Tok::StarEq : Tok::Star; break;
            case '/': k = match('=') ? Tok::SlashEq : Tok::Slash; break;
            case '%': k = match('=') ? Tok::PercentEq : Tok::Percent; break;
            case '=': k = match('=') ? Tok::EqEq : match('>') ? Tok::Arrow : Tok::Eq; break;
            case '!': k = match('=') ? Tok::NotEq : Tok::Bang; break;
            case '<': k = match('=') ? Tok::LessEq : match('<') ? Tok::Shl : Tok::Less; break;
            case '>': k = match('=') ? Tok::GreaterEq : match('>') ? Tok::Shr : Tok::Greater; break;
            case '&': k = match('&') ? Tok::AmpAmp : Tok::Amp; break;
            case '|': k = match('|') ? Tok::PipePipe : match('>') ? Tok::PipeGreater : Tok::Pipe; break;
            default: break;
        }
        if (k == Tok::Error) {
            // Avanzar el resto de un carácter UTF-8 inválido
            while (pos < src.size() && ((unsigned char)peek() & 0xC0) == 0x80) advance();
            Token t = make(Tok::Error, start);
            diags.error(t.loc, "Carácter inesperado '" + t.text + "'", "", "E0006");
            continue;
        }
        out.push_back(make(k, start));
    }
    return out;
}

} // namespace hz
