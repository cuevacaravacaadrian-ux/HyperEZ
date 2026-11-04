// Analizador léxico de HyperEZ.
#pragma once

#include "lang/sema/diagnostics.h"

namespace hz {

enum class Tok : uint8_t {
    Eof, Error,
    Ident, Number, String,
    // Delimitadores
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Comma, Dot, Semicolon, Colon, Question, QuestionDot, QuestionQuestion, Arrow /* => */, ThinArrow /* -> */,
    DotDot, DotDotEq, Ellipsis, At,
    // Operadores
    Plus, Minus, Star, Slash, Percent, StarStar,
    Eq, PlusEq, MinusEq, StarEq, SlashEq, PercentEq, QuestionQuestionEq, StarStarEq,
    EqEq, NotEq, Less, LessEq, Greater, GreaterEq,
    AmpAmp, PipePipe, Bang, Amp, Pipe, Caret, Tilde, Shl, Shr,
    PlusPlus, MinusMinus,
    PipeGreater /* |> */,
    // Palabras clave
    KwVar, KwLet, KwConst, KwFunction, KwReturn, KwIf, KwElse, KwFor, KwForeach, KwIn, KwWhile, KwRepeat,
    KwBreak, KwContinue, KwClass, KwStruct, KwConstructor, KwThis, KwSuper, KwExtends, KwImplements,
    KwInterface, KwEnum, KwImport, KwPublic, KwPrivate,
    KwProtected, KwStatic, KwTrue, KwFalse, KwNull, KwAnd, KwOr, KwNot, KwIs, KwNew, KwTry, KwCatch,
    KwFinally, KwThrow, KwAsync, KwAwait, KwMatch,
    // Contextuales (el lexer las entrega como Ident): event, on, once, emit, from, as, get, set, step, priority
};

const char* tokName(Tok t);
bool isKeyword(Tok t);

struct StrPart {
    bool isExpr = false;
    std::string text;  // literal ya decodificado o código fuente de la expresión
    SrcLoc loc;        // ubicación del inicio del fragmento
};

struct Token {
    Tok kind = Tok::Eof;
    bool newlineBefore = false;
    SrcLoc loc;
    std::string text;            // lexema (para identificadores, palabras clave y errores)
    double number = 0;
    bool isInt = true;           // el literal no tenía parte decimal ni exponente
    std::vector<StrPart> parts;  // para Tok::String
    bool interpolated = false;
};

struct Comment {
    SrcLoc loc;
    std::string text;
    bool block = false;
    bool doc = false; // "///" o "/** */"
};

class Lexer {
public:
    Lexer(std::string_view source, DiagnosticBag& diags, SrcLoc base = {});
    std::vector<Token> tokenize();
    std::vector<Comment> comments;

private:
    std::string_view src;
    DiagnosticBag& diags;
    size_t pos = 0;
    int line = 1, col = 1;
    int baseOffset = 0;
    bool sawNewline = true;

    char peek(size_t k = 0) const { return pos + k < src.size() ? src[pos + k] : '\0'; }
    char advance();
    bool match(char c);
    SrcLoc here() const { return {line, col, (int)pos + baseOffset, 0}; }
    void skipTrivia();
    Token make(Tok k, SrcLoc start);
    Token number(SrcLoc start);
    Token identifier(SrcLoc start);
    Token string(SrcLoc start, char quote, bool raw = false); // raw: r"..." sin escapes ni ${}
    bool readEscape(std::string& out);
};

Tok keywordKind(std::string_view word);

} // namespace hz
