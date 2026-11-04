// Analizador sintáctico de HyperEZ (descenso recursivo con recuperación de errores).
#pragma once

#include "lang/syntax/ast.h"

namespace hz {

class Parser {
public:
    Parser(std::vector<Token> tokens, DiagnosticBag& diags, std::vector<Comment> comments = {});
    std::vector<StmtP> parseProgram();
    StmtP destructure(const Token& kw, Visibility vis, DeclKind kind);
    ExprP parseStandaloneExpression(); // para interpolaciones y el REPL

private:
    struct ParseError {};
    std::vector<Token> toks;
    size_t p = 0;
    DiagnosticBag& diags;
    std::vector<Comment> comments;
    std::vector<bool> nlStack{true}; // true: los saltos de línea terminan sentencias
    std::vector<StmtP> extraStmts;   // sentencias extra que genera una desestructuración (var [a, b] = ...)
    int destructureCount = 0;
    int funcDepth = 0;

    const Token& cur() const { return toks[p]; }
    const Token& ahead(size_t k) const { return toks[std::min(p + k, toks.size() - 1)]; }
    const Token& prev() const { return toks[p > 0 ? p - 1 : 0]; }
    bool check(Tok k) const { return cur().kind == k; }
    bool checkWord(const char* w) const { return cur().kind == Tok::Ident && cur().text == w; }
    bool match(Tok k) { if (check(k)) { p++; return true; } return false; }
    Token advance() { Token t = toks[p]; if (p < toks.size() - 1) p++; return t; }
    Token expect(Tok k, const char* what);
    std::string expectIdent(const char* what, SrcLoc* loc = nullptr);
    [[noreturn]] void fail(const SrcLoc& loc, const std::string& msg, const std::string& hint = "");
    bool newlinesMatter() const { return nlStack.back(); }
    bool atStatementBoundary() const;
    void endStatement();
    void synchronize();
    std::string docFor(int line);

    // Sentencias
    StmtP statement(bool topLevel);
    StmtP varDecl(Visibility vis);
    std::unique_ptr<FuncDecl> function(FuncKind kind, bool isAsync, Visibility vis, bool isStatic);
    void paramList(std::vector<Param>& params);
    std::unique_ptr<BlockStmt> block();
    StmtP ifStmt();
    StmtP whileStmt();
    StmtP forStmt();
    StmtP repeatStmt();
    StmtP returnStmt();
    StmtP tryStmt();
    StmtP classDecl(bool isStruct);
    StmtP interfaceDecl();
    StmtP enumDecl();
    StmtP eventDecl();
    StmtP emitStmt();
    StmtP importStmt(bool fromForm);
    TypeRefP type();

    // Expresiones
    ExprP assignment();
    ExprP expression();
    ExprP ternary();
    ExprP nullish();
    ExprP logicOr();
    ExprP logicAnd();
    ExprP logicNot();
    ExprP equality();
    ExprP comparison();
    ExprP bitOr();
    ExprP bitXor();
    ExprP bitAnd();
    ExprP shift();
    ExprP range();
    ExprP additive();
    ExprP multiplicative();
    ExprP unary();
    ExprP power();
    ExprP postfix();
    ExprP primary();
    ExprP stringLiteral(const Token& t);
    ExprP listLiteral();
    ExprP mapLiteral();
    ExprP lambda(bool isAsync);
    ExprP matchExpr();
    ExprP onExpr(bool once);
    void callArgs(std::vector<Arg>& args);
    bool looksLikeLambda() const;
    bool continuesOnNewline(const Token& t) const;
};

// Utilidad: analiza un archivo completo.
std::unique_ptr<Program> parseSource(const std::string& file, const std::string& source, DiagnosticBag& diags);

} // namespace hz
