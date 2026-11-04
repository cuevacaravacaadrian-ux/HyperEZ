#include "lang/syntax/parser.h"

namespace hz {

LambdaExpr::~LambdaExpr() = default;
MatchExpr::~MatchExpr() = default;
OnExpr::~OnExpr() = default;
TryStmt::~TryStmt() = default;

std::string TypeRef::toString() const {
    std::string s = name;
    if (!args.empty()) {
        s += "<";
        for (size_t i = 0; i < args.size(); i++) { if (i) s += ", "; s += args[i]->toString(); }
        s += ">";
    }
    if (nullable) s += "?";
    return s;
}

Parser::Parser(std::vector<Token> tokens, DiagnosticBag& d, std::vector<Comment> c)
    : toks(std::move(tokens)), diags(d), comments(std::move(c)) {
    if (toks.empty()) toks.push_back(Token{});
}

void Parser::fail(const SrcLoc& loc, const std::string& msg, const std::string& hint) {
    diags.error(loc, msg, hint, "E0100");
    throw ParseError{};
}

Token Parser::expect(Tok k, const char* what) {
    if (check(k)) return advance();
    std::string found = cur().kind == Tok::Eof ? "el fin del archivo" : "'" + cur().text + "'";
    std::string hint;
    if (k == Tok::LBrace && (prev().kind == Tok::RParen || prev().kind == Tok::Ident))
        hint = "los bloques de HyperEZ siempre van entre llaves: if x > 0 { ... }";
    fail(cur().loc, std::string("Se esperaba ") + what + ", pero se encontró " + found, hint);
}

std::string Parser::expectIdent(const char* what, SrcLoc* loc) {
    if (check(Tok::Ident)) {
        if (loc) *loc = cur().loc;
        return advance().text;
    }
    if (isKeyword(cur().kind))
        fail(cur().loc, "'" + cur().text + "' es una palabra reservada y no puede usarse como " + what,
             "elige otro nombre, por ejemplo '" + cur().text + "_' o '" + cur().text + "Valor'");
    std::string found = cur().kind == Tok::Eof ? "el fin del archivo" : "'" + cur().text + "'";
    fail(cur().loc, std::string("Se esperaba ") + what + ", pero se encontró " + found);
}

bool Parser::atStatementBoundary() const {
    return check(Tok::Semicolon) || check(Tok::RBrace) || check(Tok::Eof) || cur().newlineBefore;
}

void Parser::endStatement() {
    if (match(Tok::Semicolon)) { while (match(Tok::Semicolon)) {} return; }
    if (check(Tok::RBrace) || check(Tok::Eof) || cur().newlineBefore) return;
    fail(cur().loc, "Se esperaba un salto de línea o ';' antes de '" + cur().text + "'",
         "cada sentencia va en su propia línea (o sepáralas con ';')");
}

void Parser::synchronize() {
    // Avanzar hasta el inicio probable de la siguiente sentencia.
    int depth = 0;
    if (!check(Tok::Eof)) advance();
    while (!check(Tok::Eof)) {
        if (prev().kind == Tok::Semicolon && depth == 0) return;
        if (check(Tok::LBrace)) depth++;
        if (check(Tok::RBrace)) {
            if (depth == 0) return;
            depth--;
        }
        if (depth == 0 && cur().newlineBefore) {
            switch (cur().kind) {
                case Tok::KwVar: case Tok::KwLet: case Tok::KwConst: case Tok::KwFunction: case Tok::KwClass:
                case Tok::KwStruct: case Tok::KwIf: case Tok::KwFor: case Tok::KwForeach: case Tok::KwWhile:
                case Tok::KwRepeat: case Tok::KwReturn: case Tok::KwEnum: case Tok::KwInterface: case Tok::KwImport:
                case Tok::KwTry: case Tok::KwAsync: case Tok::Ident:
                    return;
                default: break;
            }
        }
        advance();
    }
}

std::string Parser::docFor(int line) {
    // Comentarios contiguos que terminan justo en la línea anterior a la declaración.
    std::vector<const Comment*> found;
    int want = line - 1;
    for (int i = (int)comments.size() - 1; i >= 0; i--) {
        const Comment& c = comments[i];
        int endLine = c.loc.line;
        if (c.block) for (char ch : c.text) if (ch == '\n') endLine++;
        if (endLine == want) { found.push_back(&c); want = c.loc.line - 1; }
        else if (endLine < want) break;
    }
    std::string doc;
    for (int i = (int)found.size() - 1; i >= 0; i--) {
        std::string t = found[i]->text;
        if (found[i]->block) {
            // quitar /** */ y asteriscos iniciales
            if (startsWith(t, "/**")) t = t.substr(3); else if (startsWith(t, "/*")) t = t.substr(2);
            if (endsWith(t, "*/")) t = t.substr(0, t.size() - 2);
            for (auto& l : splitLines(t)) {
                std::string s = trim(l);
                if (startsWith(s, "*")) s = trim(s.substr(1));
                if (!doc.empty() || !s.empty()) doc += s + "\n";
            }
        } else {
            size_t k = 0;
            while (k < t.size() && t[k] == '/') k++;
            doc += trim(t.substr(k)) + "\n";
        }
    }
    while (!doc.empty() && doc.back() == '\n') doc.pop_back();
    return doc;
}

// ---------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------

std::vector<StmtP> Parser::parseProgram() {
    std::vector<StmtP> body;
    while (!check(Tok::Eof)) {
        try {
            if (match(Tok::Semicolon)) continue;
            auto s = statement(true);
            if (s) body.push_back(std::move(s));
            for (auto& x : extraStmts) body.push_back(std::move(x));
            extraStmts.clear();
        } catch (ParseError&) {
            nlStack.assign(1, true);
            synchronize();
            if (check(Tok::RBrace)) advance();
        }
    }
    return body;
}

ExprP Parser::parseStandaloneExpression() {
    try {
        auto e = expression();
        if (!check(Tok::Eof)) fail(cur().loc, "Sobra '" + cur().text + "' en la expresión");
        return e;
    } catch (ParseError&) {
        return nullptr;
    }
}

StmtP Parser::statement(bool topLevel) {
    const Token& t = cur();
    switch (t.kind) {
        case Tok::KwVar: case Tok::KwLet: case Tok::KwConst: return varDecl(Visibility::Public);
        case Tok::KwFunction: {
            auto s = std::make_unique<FuncStmt>(t.loc);
            s->fn = function(FuncKind::Function, false, Visibility::Public, false);
            return s;
        }
        case Tok::KwAsync:
            if (ahead(1).kind == Tok::KwFunction) {
                advance();
                auto s = std::make_unique<FuncStmt>(t.loc);
                s->fn = function(FuncKind::Function, true, Visibility::Public, false);
                return s;
            }
            break;
        case Tok::KwPublic: case Tok::KwPrivate: case Tok::KwProtected: {
            Visibility vis = t.kind == Tok::KwPrivate ? Visibility::Private : t.kind == Tok::KwProtected ? Visibility::Protected : Visibility::Public;
            advance();
            if (check(Tok::KwVar) || check(Tok::KwLet) || check(Tok::KwConst)) return varDecl(vis);
            bool isAsync = match(Tok::KwAsync);
            if (check(Tok::KwFunction)) {
                auto s = std::make_unique<FuncStmt>(t.loc);
                s->fn = function(FuncKind::Function, isAsync, vis, false);
                return s;
            }
            if (check(Tok::KwClass) || check(Tok::KwStruct)) { bool st = check(Tok::KwStruct); advance(); auto c = classDecl(st); return c; }
            fail(cur().loc, "Se esperaba una declaración (var, function, class...) después del modificador de visibilidad");
        }
        case Tok::KwClass: advance(); return classDecl(false);
        case Tok::KwStruct: advance(); return classDecl(true);
        case Tok::KwInterface: return interfaceDecl();
        case Tok::KwEnum: return enumDecl();
        case Tok::KwIf: return ifStmt();
        case Tok::KwWhile: return whileStmt();
        case Tok::KwFor: case Tok::KwForeach: return forStmt();
        case Tok::KwRepeat: return repeatStmt();
        case Tok::KwReturn: return returnStmt();
        case Tok::KwBreak: { advance(); auto s = std::make_unique<BreakStmt>(t.loc); endStatement(); return s; }
        case Tok::KwContinue: { advance(); auto s = std::make_unique<ContinueStmt>(t.loc); endStatement(); return s; }
        case Tok::KwTry: return tryStmt();
        case Tok::KwThrow: {
            SrcLoc loc = advance().loc;
            auto s = std::make_unique<ThrowStmt>(loc);
            s->value = expression();
            endStatement();
            return s;
        }
        case Tok::KwImport: return importStmt(false);
        case Tok::LBrace: return block();
        case Tok::KwStatic:
            fail(t.loc, "'static' solo puede usarse dentro de una clase");
        case Tok::KwConstructor:
            fail(t.loc, "'constructor' solo puede usarse dentro de una clase");
        case Tok::KwElse:
            fail(t.loc, "'else' sin 'if' previo", "comprueba que la llave '}' del if está justo antes del else");
        case Tok::Ident: {
            const Token& n = ahead(1);
            bool sameLine = !n.newlineBefore;
            if (t.text == "event" && n.kind == Tok::Ident && sameLine) return eventDecl();
            if (t.text == "emit" && n.kind == Tok::Ident && sameLine) return emitStmt();
            if (t.text == "from" && (n.kind == Tok::Ident || n.kind == Tok::String) && sameLine) return importStmt(true);
            break;
        }
        default: break;
    }
    (void)topLevel;
    auto e = assignment();
    auto s = std::make_unique<ExprStmt>(e->loc, std::move(e));
    endStatement();
    return s;
}

StmtP Parser::varDecl(Visibility vis) {
    Token kw = advance();
    auto s = std::make_unique<VarStmt>(kw.loc);
    s->doc = docFor(kw.loc.line);
    s->vis = vis;
    s->declKind = kw.kind == Tok::KwLet ? DeclKind::Let : kw.kind == Tok::KwConst ? DeclKind::Const : DeclKind::Var;
    if (check(Tok::LBracket) || check(Tok::LBrace)) return destructure(kw, vis, s->declKind);
    s->name = expectIdent("nombre de variable", &s->nameLoc);
    if (match(Tok::Colon)) s->typeAnn = type();
    if (match(Tok::Eq)) s->init = expression();
    else if (s->declKind == DeclKind::Const)
        diags.error(s->nameLoc, "La constante '" + s->name + "' necesita un valor inicial", "const " + s->name + " = valor", "E0101");
    endStatement();
    return s;
}

// var [a, b, ...resto] = lista    var {nombre, vida: puntos} = jugador
// Se traduce a una variable oculta con el valor y una declaración por cada nombre:
//   var __d1 = lista; var a = __d1[0]; var b = __d1[1]; var resto = __d1.skip(2)
StmtP Parser::destructure(const Token& kw, Visibility vis, DeclKind kind) {
    bool isList = check(Tok::LBracket);
    Token open = advance();
    struct Item { std::string name, key; SrcLoc loc; bool rest = false; };
    std::vector<Item> items;
    nlStack.push_back(false);
    while (!check(isList ? Tok::RBracket : Tok::RBrace) && !check(Tok::Eof)) {
        Item it;
        if (isList && match(Tok::Ellipsis)) it.rest = true;
        it.name = expectIdent("nombre de variable", &it.loc);
        it.key = it.name;
        if (!isList && match(Tok::Colon)) it.name = expectIdent("nombre de variable", &it.loc); // {clave: nombre}
        if (it.rest && !items.empty() && items.back().rest) diags.error(it.loc, "Solo puede haber un '...' y debe ir al final", "", "E0103");
        items.push_back(it);
        if (!match(Tok::Comma)) break;
        if (it.rest) diags.error(it.loc, "El '...' debe ser el último de la lista", "", "E0103");
    }
    nlStack.pop_back();
    expect(isList ? Tok::RBracket : Tok::RBrace, isList ? "']' al cerrar la desestructuración" : "'}' al cerrar la desestructuración");
    if (items.empty()) diags.error(open.loc, "La desestructuración necesita al menos un nombre", "", "E0103");
    if (!match(Tok::Eq)) {
        diags.error(open.loc, "La desestructuración necesita un valor: var [a, b] = lista", "", "E0103");
        endStatement();
        return nullptr;
    }
    ExprP init = expression();
    endStatement();

    auto hidden = std::make_unique<VarStmt>(kw.loc);
    hidden->name = "__d" + std::to_string(++destructureCount);
    hidden->nameLoc = open.loc;
    hidden->declKind = DeclKind::Var;
    hidden->vis = Visibility::Private;
    hidden->init = std::move(init);
    auto ref = [&](SrcLoc l) { return std::make_unique<IdentExpr>(l, hidden->name); };
    for (size_t i = 0; i < items.size(); i++) {
        auto& it = items[i];
        auto v = std::make_unique<VarStmt>(it.loc);
        v->name = it.name;
        v->nameLoc = it.loc;
        v->declKind = kind;
        v->vis = vis;
        if (!isList) {
            auto m = std::make_unique<MemberExpr>(it.loc);
            m->object = ref(it.loc);
            m->name = it.key;
            m->nameLoc = it.loc;
            v->init = std::move(m);
        } else if (it.rest) {
            auto callee = std::make_unique<MemberExpr>(it.loc);
            callee->object = ref(it.loc);
            callee->name = "skip";
            callee->nameLoc = it.loc;
            auto call = std::make_unique<CallExpr>(it.loc);
            call->callee = std::move(callee);
            Arg a;
            a.value = std::make_unique<NumberExpr>(it.loc, (double)i, true);
            call->args.push_back(std::move(a));
            v->init = std::move(call);
        } else {
            auto ix = std::make_unique<IndexExpr>(it.loc);
            ix->object = ref(it.loc);
            ix->index = std::make_unique<NumberExpr>(it.loc, (double)i, true);
            v->init = std::move(ix);
        }
        extraStmts.push_back(std::move(v));
    }
    return hidden;
}

void Parser::paramList(std::vector<Param>& params) {
    expect(Tok::LParen, "'(' para los parámetros");
    nlStack.push_back(false);
    bool seenDefault = false;
    while (!check(Tok::RParen) && !check(Tok::Eof)) {
        Param prm;
        prm.rest = match(Tok::Ellipsis);
        prm.name = expectIdent("nombre de parámetro", &prm.loc);
        if (match(Tok::Colon)) prm.type = type();
        if (match(Tok::Eq)) { prm.def = expression(); seenDefault = true; }
        else if (seenDefault && !prm.rest)
            diags.error(prm.loc, "El parámetro '" + prm.name + "' necesita un valor por defecto porque va después de otro que lo tiene", "", "E0102");
        if (prm.rest && prm.def) diags.error(prm.loc, "Un parámetro '...' no puede tener valor por defecto", "", "E0102");
        bool wasRest = prm.rest;
        params.push_back(std::move(prm));
        if (!match(Tok::Comma)) break;
        if (wasRest) diags.error(prev().loc, "El parámetro '...' debe ser el último", "", "E0102");
    }
    nlStack.pop_back();
    expect(Tok::RParen, "')' al cerrar los parámetros");
}

std::unique_ptr<FuncDecl> Parser::function(FuncKind kind, bool isAsync, Visibility vis, bool isStatic) {
    auto fn = std::make_unique<FuncDecl>();
    fn->kind = kind;
    fn->isAsync = isAsync;
    fn->vis = vis;
    fn->isStatic = isStatic;
    fn->loc = cur().loc;
    fn->doc = docFor(cur().loc.line);
    if (kind == FuncKind::Constructor) {
        fn->name = "constructor";
        fn->nameLoc = advance().loc;
    } else if (kind == FuncKind::Getter || kind == FuncKind::Setter) {
        advance(); // get/set
        fn->name = expectIdent("nombre de propiedad", &fn->nameLoc);
    } else {
        expect(Tok::KwFunction, "'function'");
        if (check(Tok::Ident)) fn->name = expectIdent("nombre de función", &fn->nameLoc);
        else if (kind != FuncKind::Lambda) fn->name = expectIdent("nombre de función", &fn->nameLoc);
        else fn->name = "";
    }
    paramList(fn->params);
    if (match(Tok::Colon)) fn->ret = type();
    funcDepth++;
    if (match(Tok::Arrow)) {
        nlStack.push_back(true);
        fn->exprBody = assignment();
        nlStack.pop_back();
    } else {
        fn->body = block();
    }
    funcDepth--;
    return fn;
}

std::unique_ptr<BlockStmt> Parser::block() {
    Token open = expect(Tok::LBrace, "'{'");
    auto b = std::make_unique<BlockStmt>(open.loc);
    nlStack.push_back(true);
    size_t depth = nlStack.size();
    while (!check(Tok::RBrace) && !check(Tok::Eof)) {
        try {
            if (match(Tok::Semicolon)) continue;
            b->body.push_back(statement(false));
            for (auto& x : extraStmts) b->body.push_back(std::move(x));
            extraStmts.clear();
        } catch (ParseError&) {
            nlStack.resize(depth);
            synchronize();
            if (check(Tok::Eof)) break;
        }
    }
    nlStack.pop_back();
    b->endLoc = cur().loc;
    if (!check(Tok::RBrace)) {
        diags.error(open.loc, "Falta la llave de cierre '}' para este bloque", "", "E0103");
        throw ParseError{};
    }
    advance();
    return b;
}

StmtP Parser::ifStmt() {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<IfStmt>(loc);
    s->cond = expression();
    s->then = block();
    if (check(Tok::KwElse)) {
        advance();
        if (check(Tok::KwIf)) s->otherwise = ifStmt();
        else s->otherwise = block();
    }
    return s;
}

StmtP Parser::whileStmt() {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<WhileStmt>(loc);
    s->cond = expression();
    s->body = block();
    return s;
}

StmtP Parser::forStmt() {
    Token kw = advance();
    bool paren = false;
    if (check(Tok::LParen)) {
        // for (i in 0..10) — paréntesis opcionales; el estilo C no existe
        size_t save = p;
        advance();
        if ((check(Tok::KwVar) || check(Tok::KwLet))) {
            fail(kw.loc, "HyperEZ no usa el 'for' de estilo C", "usa: for i in 0..10 { ... }  o  foreach x in lista { ... }");
        }
        if (check(Tok::Ident) && (ahead(1).kind == Tok::KwIn || ahead(1).kind == Tok::Comma)) { paren = true; nlStack.push_back(false); }
        else p = save;
    }
    SrcLoc v1loc, v2loc;
    std::string v1 = expectIdent("nombre de variable del bucle", &v1loc), v2;
    if (match(Tok::Comma)) v2 = expectIdent("segundo nombre de variable", &v2loc);
    if (!check(Tok::KwIn)) fail(cur().loc, "Se esperaba 'in' en el bucle", "for elemento in coleccion { ... }");
    advance();
    ExprP iter = expression();
    ExprP step;
    if (checkWord("step")) { advance(); step = expression(); }
    if (paren) { nlStack.pop_back(); expect(Tok::RParen, "')'"); }
    if (iter->kind == ExprKind::Range && v2.empty()) {
        auto* r = static_cast<RangeExpr*>(iter.get());
        auto s = std::make_unique<ForRangeStmt>(kw.loc);
        s->var = v1; s->varLoc = v1loc;
        s->start = std::move(r->start); s->end = std::move(r->end); s->inclusive = r->inclusive;
        s->step = step ? std::move(step) : std::move(r->step);
        s->body = block();
        return s;
    }
    if (step) diags.error(iter->loc, "'step' solo se puede usar con rangos (a..b)", "", "E0104");
    auto s = std::make_unique<ForEachStmt>(kw.loc);
    if (v2.empty()) { s->var = v1; s->varLoc = v1loc; }
    else { s->var = v2; s->varLoc = v2loc; s->var2 = v1; s->var2Loc = v1loc; } // for clave, valor  /  for indice, valor
    s->iterable = std::move(iter);
    s->body = block();
    return s;
}

StmtP Parser::repeatStmt() {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<RepeatStmt>(loc);
    s->count = expression();
    s->body = block();
    return s;
}

StmtP Parser::returnStmt() {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<ReturnStmt>(loc);
    if (!atStatementBoundary()) s->value = expression();
    endStatement();
    return s;
}

StmtP Parser::tryStmt() {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<TryStmt>(loc);
    s->body = block();
    if (check(Tok::KwCatch)) {
        advance();
        bool paren = match(Tok::LParen);
        if (check(Tok::Ident)) {
            s->catchVar = expectIdent("nombre del error", &s->catchLoc);
            if (match(Tok::Colon)) s->catchType = type();
        }
        if (paren) expect(Tok::RParen, "')'");
        s->catchBody = block();
    }
    if (check(Tok::KwFinally)) { advance(); s->finallyBody = block(); }
    if (!s->catchBody && !s->finallyBody) fail(loc, "Un 'try' necesita un 'catch' o un 'finally'");
    return s;
}

StmtP Parser::classDecl(bool isStruct) {
    SrcLoc loc = prev().loc;
    auto c = std::make_unique<ClassStmt>(loc);
    c->doc = docFor(loc.line);
    c->isStruct = isStruct;
    c->name = expectIdent(isStruct ? "nombre de struct" : "nombre de clase", &c->nameLoc);
    if (match(Tok::KwExtends)) {
        c->superLoc = cur().loc;
        c->superName = expectIdent("nombre de la clase base");
        while (match(Tok::Dot)) c->superName += "." + expectIdent("nombre");
        auto id = std::make_unique<IdentExpr>(c->superLoc, c->superName);
        c->superExpr = std::move(id);
    }
    if (match(Tok::KwImplements)) {
        do {
            SrcLoc l;
            std::string n = expectIdent("nombre de interfaz", &l);
            while (match(Tok::Dot)) n += "." + expectIdent("nombre");
            c->interfaces.push_back({n, l});
        } while (match(Tok::Comma));
    }
    expect(Tok::LBrace, "'{' para abrir la clase");
    nlStack.push_back(true);
    size_t depth = nlStack.size();
    while (!check(Tok::RBrace) && !check(Tok::Eof)) {
        try {
            if (match(Tok::Semicolon)) continue;
            Visibility vis = Visibility::Public;
            bool isStatic = false, isAsync = false;
            for (;;) {
                if (match(Tok::KwPublic)) vis = Visibility::Public;
                else if (match(Tok::KwPrivate)) vis = Visibility::Private;
                else if (match(Tok::KwProtected)) vis = Visibility::Protected;
                else if (match(Tok::KwStatic)) isStatic = true;
                else if (match(Tok::KwAsync)) isAsync = true;
                else break;
            }
            if (check(Tok::KwVar) || check(Tok::KwLet) || check(Tok::KwConst)) {
                Token kw = advance();
                FieldDecl f;
                f.doc = docFor(kw.loc.line);
                f.declKind = kw.kind == Tok::KwLet ? DeclKind::Let : kw.kind == Tok::KwConst ? DeclKind::Const : DeclKind::Var;
                f.vis = vis; f.isStatic = isStatic;
                f.name = expectIdent("nombre de campo", &f.loc);
                if (match(Tok::Colon)) f.type = type();
                if (match(Tok::Eq)) f.init = expression();
                c->fields.push_back(std::move(f));
                // Varios campos en una línea: var x = 0, y = 0
                while (match(Tok::Comma)) {
                    FieldDecl g;
                    g.declKind = c->fields.back().declKind; g.vis = vis; g.isStatic = isStatic;
                    g.name = expectIdent("nombre de campo", &g.loc);
                    if (match(Tok::Colon)) g.type = type();
                    if (match(Tok::Eq)) g.init = expression();
                    c->fields.push_back(std::move(g));
                }
                endStatement();
            } else if (check(Tok::KwConstructor)) {
                if (c->ctor) diags.error(cur().loc, "La clase '" + c->name + "' ya tiene un constructor", "", "E0105");
                c->ctor = function(FuncKind::Constructor, false, vis, false);
            } else if (check(Tok::KwFunction)) {
                c->methods.push_back(function(FuncKind::Method, isAsync, vis, isStatic));
            } else if ((checkWord("get") || checkWord("set")) && ahead(1).kind == Tok::Ident && ahead(2).kind == Tok::LParen) {
                bool g = checkWord("get");
                c->methods.push_back(function(g ? FuncKind::Getter : FuncKind::Setter, false, vis, isStatic));
            } else if (check(Tok::Ident) && (ahead(1).kind == Tok::Colon || ahead(1).kind == Tok::Eq)) {
                fail(cur().loc, "Los campos de una clase se declaran con 'var', 'let' o 'const'", "var " + cur().text + " ...");
            } else {
                fail(cur().loc, "Se esperaba un miembro de clase (var, function, constructor...) pero se encontró '" + cur().text + "'");
            }
        } catch (ParseError&) {
            nlStack.resize(depth);
            synchronize();
        }
    }
    nlStack.pop_back();
    c->endLoc = cur().loc;
    expect(Tok::RBrace, "'}' para cerrar la clase");
    return c;
}

StmtP Parser::interfaceDecl() {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<InterfaceStmt>(loc);
    s->doc = docFor(loc.line);
    s->name = expectIdent("nombre de interfaz", &s->nameLoc);
    if (match(Tok::KwExtends)) {
        do { SrcLoc l; std::string n = expectIdent("interfaz", &l); s->extends.push_back({n, l}); } while (match(Tok::Comma));
    }
    expect(Tok::LBrace, "'{'");
    nlStack.push_back(true);
    while (!check(Tok::RBrace) && !check(Tok::Eof)) {
        if (match(Tok::Semicolon)) continue;
        match(Tok::KwPublic);
        match(Tok::KwAsync);
        if (check(Tok::KwFunction)) {
            advance();
            InterfaceMethod m;
            m.name = expectIdent("nombre de método", &m.loc);
            paramList(m.params);
            if (match(Tok::Colon)) m.ret = type();
            s->methods.push_back(std::move(m));
            endStatement();
        } else if (check(Tok::KwVar) || check(Tok::KwLet)) {
            // Propiedades requeridas: se aceptan como documentación.
            advance(); expectIdent("nombre");
            if (match(Tok::Colon)) type();
            endStatement();
        } else {
            nlStack.pop_back();
            fail(cur().loc, "Dentro de una interfaz solo puede haber firmas 'function nombre(...): Tipo'");
        }
    }
    nlStack.pop_back();
    expect(Tok::RBrace, "'}'");
    return s;
}

StmtP Parser::enumDecl() {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<EnumStmt>(loc);
    s->doc = docFor(loc.line);
    s->name = expectIdent("nombre de enum", &s->nameLoc);
    expect(Tok::LBrace, "'{'");
    nlStack.push_back(false);
    while (!check(Tok::RBrace) && !check(Tok::Eof)) {
        EnumMember m;
        m.name = expectIdent("nombre de valor del enum", &m.loc);
        if (match(Tok::Eq)) m.value = expression();
        s->members.push_back(std::move(m));
        if (!match(Tok::Comma) && !check(Tok::RBrace) && !cur().newlineBefore) {
            nlStack.pop_back();
            fail(cur().loc, "Se esperaba ',' entre los valores del enum");
        }
    }
    nlStack.pop_back();
    expect(Tok::RBrace, "'}'");
    return s;
}

StmtP Parser::eventDecl() {
    SrcLoc loc = advance().loc; // 'event'
    auto s = std::make_unique<EventStmt>(loc);
    s->doc = docFor(loc.line);
    s->name = expectIdent("nombre de evento", &s->nameLoc);
    if (check(Tok::LParen) && !cur().newlineBefore) paramList(s->params);
    endStatement();
    return s;
}

StmtP Parser::emitStmt() {
    SrcLoc loc = advance().loc; // 'emit'
    auto s = std::make_unique<EmitStmt>(loc);
    SrcLoc nl;
    std::string name = expectIdent("nombre de evento", &nl);
    ExprP ev = std::make_unique<IdentExpr>(nl, name);
    while (check(Tok::Dot)) {
        advance();
        auto m = std::make_unique<MemberExpr>(cur().loc);
        m->object = std::move(ev);
        m->name = expectIdent("nombre", &m->nameLoc);
        ev = std::move(m);
    }
    s->event = std::move(ev);
    if (check(Tok::LParen) && !cur().newlineBefore) callArgs(s->args);
    endStatement();
    return s;
}

StmtP Parser::importStmt(bool fromForm) {
    SrcLoc loc = advance().loc;
    auto s = std::make_unique<ImportStmt>(loc);
    auto modulePath = [&]() {
        if (check(Tok::String)) {
            Token t = advance();
            s->isFile = true;
            s->path = t.parts.empty() ? "" : t.parts[0].text;
        } else {
            s->path = expectIdent("nombre de módulo");
            while (match(Tok::Dot)) s->path += "." + expectIdent("nombre de módulo");
        }
    };
    modulePath();
    if (fromForm) {
        if (!check(Tok::KwImport)) fail(cur().loc, "Se esperaba 'import' en 'from módulo import nombre'");
        advance();
        do {
            SrcLoc l;
            std::string n = expectIdent("nombre a importar", &l);
            std::string alias = n;
            if (checkWord("as")) { advance(); alias = expectIdent("alias"); }
            s->names.push_back({n, l});
            s->nameAliases.push_back(alias);
        } while (match(Tok::Comma));
        s->nameRes.resize(s->names.size());
        s->nameSyms.resize(s->names.size(), nullptr);
    } else if (checkWord("as")) {
        advance();
        s->alias = expectIdent("alias", &s->aliasLoc);
    }
    endStatement();
    return s;
}

TypeRefP Parser::type() {
    auto t = std::make_unique<TypeRef>();
    t->loc = cur().loc;
    if (check(Tok::KwFunction)) { advance(); t->name = "function"; }
    else if (check(Tok::KwNull)) { advance(); t->name = "null"; }
    else if (check(Tok::LParen)) {
        // (a, b) => R  — tipo de función; se trata como "function"
        int depth = 0;
        do {
            if (check(Tok::LParen)) depth++;
            if (check(Tok::RParen)) depth--;
            advance();
        } while (depth > 0 && !check(Tok::Eof));
        if (match(Tok::Arrow) || match(Tok::ThinArrow)) type();
        t->name = "function";
    } else {
        t->name = expectIdent("nombre de tipo");
        while (check(Tok::Dot)) { advance(); t->name += "." + expectIdent("nombre de tipo"); }
        if (check(Tok::Less) && !cur().newlineBefore) {
            advance();
            nlStack.push_back(false);
            do { t->args.push_back(type()); } while (match(Tok::Comma));
            nlStack.pop_back();
            if (check(Tok::Shr)) { toks[p].kind = Tok::Greater; toks[p].text = ">"; toks[p].loc.col++; toks[p].loc.offset++; }
            else expect(Tok::Greater, "'>' al cerrar el tipo genérico");
        }
    }
    if (check(Tok::Question) && !cur().newlineBefore) {
        // "x is Raw ? a : b": si tras el ? empieza una expresión, el ? es el operador condicional
        Tok n = ahead(1).kind;
        bool startsExpr = !ahead(1).newlineBefore &&
                          (n == Tok::Ident || n == Tok::Number || n == Tok::String || n == Tok::LParen || n == Tok::LBracket ||
                           n == Tok::KwTrue || n == Tok::KwFalse || n == Tok::KwNull || n == Tok::KwThis ||
                           n == Tok::Minus || n == Tok::Bang || n == Tok::KwNot || n == Tok::KwAwait || n == Tok::KwMatch);
        if (!startsExpr) { advance(); t->nullable = true; }
    }
    t->loc.length = prev().loc.offset + prev().loc.length - t->loc.offset;
    return t;
}

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------

bool Parser::continuesOnNewline(const Token& t) const {
    if (!newlinesMatter() || !t.newlineBefore) return true;
    switch (t.kind) {
        case Tok::Minus: case Tok::Plus: case Tok::LParen: case Tok::LBracket: case Tok::Bang:
        case Tok::PlusPlus: case Tok::MinusMinus: case Tok::Tilde: case Tok::Star:
            return false;
        default:
            return true;
    }
}

ExprP Parser::assignment() {
    ExprP target = expression();
    Tok k = cur().kind;
    if (k == Tok::Eq || k == Tok::PlusEq || k == Tok::MinusEq || k == Tok::StarEq || k == Tok::SlashEq ||
        k == Tok::PercentEq || k == Tok::QuestionQuestionEq) {
        Token op = advance();
        if (target->kind != ExprKind::Ident && target->kind != ExprKind::Member && target->kind != ExprKind::Index) {
            diags.error(target->loc, "No se puede asignar a esta expresión", "solo se puede asignar a variables, propiedades (obj.x) o índices (lista[i])", "E0106");
        }
        auto a = std::make_unique<AssignExpr>(op.loc, k);
        a->target = std::move(target);
        a->value = assignment();
        a->loc = a->target->loc;
        return a;
    }
    return target;
}

ExprP Parser::expression() { return ternary(); }

ExprP Parser::ternary() {
    ExprP cond = nullish();
    if (check(Tok::Question) && continuesOnNewline(cur())) {
        Token q = advance();
        auto t = std::make_unique<TernaryExpr>(cond->loc);
        t->cond = std::move(cond);
        nlStack.push_back(false);
        t->then = ternary();
        nlStack.pop_back();
        expect(Tok::Colon, "':' en la expresión condicional (cond ? a : b)");
        t->otherwise = ternary();
        (void)q;
        return t;
    }
    return cond;
}

ExprP Parser::nullish() {
    ExprP left = logicOr();
    while (check(Tok::QuestionQuestion) && continuesOnNewline(cur())) {
        Token op = advance();
        left = std::make_unique<LogicalExpr>(op.loc, Tok::QuestionQuestion, std::move(left), logicOr());
    }
    return left;
}

ExprP Parser::logicOr() {
    ExprP left = logicAnd();
    while ((check(Tok::KwOr) || check(Tok::PipePipe)) && continuesOnNewline(cur())) {
        Token op = advance();
        left = std::make_unique<LogicalExpr>(op.loc, Tok::KwOr, std::move(left), logicAnd());
    }
    return left;
}

ExprP Parser::logicAnd() {
    ExprP left = logicNot();
    while ((check(Tok::KwAnd) || check(Tok::AmpAmp)) && continuesOnNewline(cur())) {
        Token op = advance();
        left = std::make_unique<LogicalExpr>(op.loc, Tok::KwAnd, std::move(left), logicNot());
    }
    return left;
}

ExprP Parser::logicNot() {
    if (check(Tok::KwNot)) {
        Token op = advance();
        return std::make_unique<UnaryExpr>(op.loc, Tok::KwNot, logicNot());
    }
    return equality();
}

ExprP Parser::equality() {
    ExprP left = comparison();
    while ((check(Tok::EqEq) || check(Tok::NotEq)) && continuesOnNewline(cur())) {
        Token op = advance();
        left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), comparison());
    }
    return left;
}

ExprP Parser::comparison() {
    ExprP left = bitOr();
    for (;;) {
        if (!continuesOnNewline(cur())) break;
        Tok k = cur().kind;
        if (k == Tok::Less || k == Tok::LessEq || k == Tok::Greater || k == Tok::GreaterEq) {
            Token op = advance();
            left = std::make_unique<BinaryExpr>(op.loc, k, std::move(left), bitOr());
        } else if (k == Tok::KwIs) {
            Token op = advance();
            auto is = std::make_unique<IsExpr>(op.loc);
            is->negate = match(Tok::KwNot);
            is->operand = std::move(left);
            is->target = type();
            left = std::move(is);
        } else if (k == Tok::KwIn) {
            Token op = advance();
            left = std::make_unique<BinaryExpr>(op.loc, Tok::KwIn, std::move(left), bitOr());
        } else if (k == Tok::KwNot && ahead(1).kind == Tok::KwIn) {
            Token op = advance(); advance();
            auto b = std::make_unique<BinaryExpr>(op.loc, Tok::KwIn, std::move(left), bitOr());
            b->negate = true;
            left = std::move(b);
        } else break;
    }
    return left;
}

ExprP Parser::bitOr() {
    ExprP left = bitXor();
    while (check(Tok::Pipe) && continuesOnNewline(cur())) { Token op = advance(); left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), bitXor()); }
    return left;
}
ExprP Parser::bitXor() {
    ExprP left = bitAnd();
    while (check(Tok::Caret) && continuesOnNewline(cur())) { Token op = advance(); left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), bitAnd()); }
    return left;
}
ExprP Parser::bitAnd() {
    ExprP left = shift();
    while (check(Tok::Amp) && continuesOnNewline(cur())) { Token op = advance(); left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), shift()); }
    return left;
}
ExprP Parser::shift() {
    ExprP left = range();
    while ((check(Tok::Shl) || check(Tok::Shr)) && continuesOnNewline(cur())) { Token op = advance(); left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), range()); }
    return left;
}

ExprP Parser::range() {
    ExprP left = additive();
    if ((check(Tok::DotDot) || check(Tok::DotDotEq)) && continuesOnNewline(cur())) {
        Token op = advance();
        auto r = std::make_unique<RangeExpr>(left->loc);
        r->inclusive = op.kind == Tok::DotDotEq;
        r->start = std::move(left);
        r->end = additive();
        return r;
    }
    return left;
}

ExprP Parser::additive() {
    ExprP left = multiplicative();
    while ((check(Tok::Plus) || check(Tok::Minus)) && continuesOnNewline(cur())) {
        Token op = advance();
        left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), multiplicative());
    }
    return left;
}

ExprP Parser::multiplicative() {
    ExprP left = unary();
    while ((check(Tok::Star) || check(Tok::Slash) || check(Tok::Percent)) && continuesOnNewline(cur())) {
        Token op = advance();
        left = std::make_unique<BinaryExpr>(op.loc, op.kind, std::move(left), unary());
    }
    return left;
}

ExprP Parser::unary() {
    Tok k = cur().kind;
    if (k == Tok::Minus || k == Tok::Plus || k == Tok::Bang || k == Tok::Tilde) {
        Token op = advance();
        ExprP operand = unary();
        // Plegado de literales negativos
        if (k == Tok::Minus && operand->kind == ExprKind::Number) {
            auto* n = static_cast<NumberExpr*>(operand.get());
            n->value = -n->value;
            n->loc = op.loc;
            return operand;
        }
        return std::make_unique<UnaryExpr>(op.loc, k == Tok::Bang ? Tok::KwNot : k, std::move(operand));
    }
    if (k == Tok::KwAwait) {
        Token op = advance();
        auto a = std::make_unique<AwaitExpr>(op.loc);
        a->operand = unary();
        return a;
    }
    if (k == Tok::PlusPlus || k == Tok::MinusMinus) {
        Token op = advance();
        auto e = std::make_unique<IncDecExpr>(op.loc);
        e->inc = k == Tok::PlusPlus;
        e->prefix = true;
        e->target = unary();
        return e;
    }
    if (k == Tok::KwNew) {
        Token op = advance();
        ExprP e = postfix();
        if (e->kind != ExprKind::Call) fail(e->loc, "'new' debe ir seguido de una llamada al constructor", "new Clase(argumentos)");
        static_cast<CallExpr*>(e.get())->isNew = true;
        e->loc = op.loc;
        return e;
    }
    return power();
}

ExprP Parser::power() {
    ExprP base = postfix();
    if (check(Tok::StarStar) && continuesOnNewline(cur())) {
        Token op = advance();
        return std::make_unique<BinaryExpr>(op.loc, Tok::StarStar, std::move(base), unary());
    }
    return base;
}

void Parser::callArgs(std::vector<Arg>& args) {
    expect(Tok::LParen, "'('");
    nlStack.push_back(false);
    bool seenNamed = false;
    while (!check(Tok::RParen) && !check(Tok::Eof)) {
        Arg a;
        if (match(Tok::Ellipsis)) {
            a.spread = true;
        } else if (check(Tok::Ident) && ahead(1).kind == Tok::Colon) {
            a.nameLoc = cur().loc;
            a.name = advance().text;
            advance();
            seenNamed = true;
        } else if (seenNamed) {
            diags.error(cur().loc, "Los argumentos posicionales deben ir antes que los argumentos con nombre", "", "E0107");
        }
        a.value = expression();
        args.push_back(std::move(a));
        if (!match(Tok::Comma)) break;
    }
    nlStack.pop_back();
    expect(Tok::RParen, "')' al cerrar la llamada");
}

ExprP Parser::postfix() {
    ExprP e = primary();
    for (;;) {
        const Token& t = cur();
        bool sameLine = !t.newlineBefore || !newlinesMatter();
        if (t.kind == Tok::Dot) {
            advance();
            auto m = std::make_unique<MemberExpr>(t.loc);
            m->object = std::move(e);
            if (check(Tok::Ident) || isKeyword(cur().kind)) { m->nameLoc = cur().loc; m->name = advance().text; }
            else fail(cur().loc, "Se esperaba el nombre de una propiedad después de '.'");
            m->loc = m->object->loc;
            e = std::move(m);
        } else if (t.kind == Tok::QuestionDot) {
            advance();
            if (check(Tok::LParen)) {
                auto c = std::make_unique<CallExpr>(e->loc);
                c->callee = std::move(e);
                c->optional = true;
                callArgs(c->args);
                e = std::move(c);
            } else if (check(Tok::LBracket)) {
                Token lb = advance();
                auto ix = std::make_unique<IndexExpr>(e->loc);
                ix->object = std::move(e);
                ix->optional = true;
                nlStack.push_back(false);
                ix->index = expression();
                nlStack.pop_back();
                expect(Tok::RBracket, "']'");
                (void)lb;
                e = std::move(ix);
            } else {
                auto m = std::make_unique<MemberExpr>(e->loc);
                m->object = std::move(e);
                m->optional = true;
                if (check(Tok::Ident) || isKeyword(cur().kind)) { m->nameLoc = cur().loc; m->name = advance().text; }
                else fail(cur().loc, "Se esperaba el nombre de una propiedad después de '?.'");
                e = std::move(m);
            }
        } else if (t.kind == Tok::LParen && sameLine) {
            auto c = std::make_unique<CallExpr>(e->loc);
            c->callee = std::move(e);
            callArgs(c->args);
            e = std::move(c);
        } else if (t.kind == Tok::LBracket && sameLine) {
            advance();
            auto ix = std::make_unique<IndexExpr>(e->loc);
            ix->object = std::move(e);
            nlStack.push_back(false);
            ix->index = expression();
            nlStack.pop_back();
            expect(Tok::RBracket, "']'");
            e = std::move(ix);
        } else if ((t.kind == Tok::PlusPlus || t.kind == Tok::MinusMinus) && !t.newlineBefore) {
            Token op = advance();
            auto ie = std::make_unique<IncDecExpr>(op.loc);
            ie->inc = op.kind == Tok::PlusPlus;
            ie->prefix = false;
            ie->target = std::move(e);
            ie->loc = ie->target->loc;
            e = std::move(ie);
        } else break;
    }
    return e;
}

bool Parser::looksLikeLambda() const {
    // Estamos en '('. Buscar el ')' correspondiente y comprobar si sigue '=>' (o ': Tipo =>').
    size_t i = p;
    int depth = 0;
    for (; i < toks.size(); i++) {
        Tok k = toks[i].kind;
        if (k == Tok::LParen || k == Tok::LBracket || k == Tok::LBrace) depth++;
        else if (k == Tok::RParen || k == Tok::RBracket || k == Tok::RBrace) { depth--; if (depth == 0) break; }
        else if (k == Tok::Eof) return false;
    }
    if (i + 1 >= toks.size()) return false;
    Tok next = toks[i + 1].kind;
    if (next == Tok::Arrow) return true;
    if (next == Tok::Colon) {
        size_t j = i + 2;
        int d = 0;
        while (j < toks.size()) {
            Tok k = toks[j].kind;
            if (k == Tok::Arrow && d == 0) return true;
            if (k == Tok::Less) d++;
            else if (k == Tok::Greater) d--;
            else if (k == Tok::Shr) d -= 2;
            else if (k != Tok::Ident && k != Tok::Dot && k != Tok::Comma && k != Tok::Question && k != Tok::KwFunction && k != Tok::KwNull) return false;
            j++;
        }
    }
    return false;
}

ExprP Parser::lambda(bool isAsync) {
    auto fn = std::make_unique<FuncDecl>();
    fn->kind = FuncKind::Lambda;
    fn->isAsync = isAsync;
    fn->loc = cur().loc;
    if (check(Tok::Ident)) {
        Param prm;
        prm.name = cur().text;
        prm.loc = advance().loc;
        fn->params.push_back(std::move(prm));
    } else {
        paramList(fn->params);
        if (match(Tok::Colon)) fn->ret = type();
    }
    expect(Tok::Arrow, "'=>'");
    funcDepth++;
    if (check(Tok::LBrace)) fn->body = block();
    else {
        nlStack.push_back(true);
        fn->exprBody = assignment();
        nlStack.pop_back();
    }
    funcDepth--;
    auto l = std::make_unique<LambdaExpr>(fn->loc);
    l->fn = std::move(fn);
    return l;
}

ExprP Parser::stringLiteral(const Token& t) {
    auto s = std::make_unique<StringExpr>(t.loc);
    for (auto& part : t.parts) {
        StringExpr::Part sp;
        sp.isExpr = part.isExpr;
        sp.text = part.text;
        if (part.isExpr) {
            Lexer lx(part.text, diags, part.loc);
            auto sub = lx.tokenize();
            Parser ps(std::move(sub), diags);
            ps.nlStack.assign(1, false);
            sp.expr = ps.parseStandaloneExpression();
            if (!sp.expr) sp.expr = std::make_unique<NullExpr>(part.loc);
        }
        s->parts.push_back(std::move(sp));
    }
    return s;
}

ExprP Parser::listLiteral() {
    Token open = advance();
    auto l = std::make_unique<ListExpr>(open.loc);
    nlStack.push_back(false);
    while (!check(Tok::RBracket) && !check(Tok::Eof)) {
        bool spread = match(Tok::Ellipsis);
        l->items.push_back(expression());
        l->spread.push_back(spread);
        if (!match(Tok::Comma)) {
            if (check(Tok::RBracket)) break;
            if (cur().newlineBefore) continue;
            nlStack.pop_back();
            fail(cur().loc, "Se esperaba ',' o ']' en la lista");
        }
    }
    nlStack.pop_back();
    expect(Tok::RBracket, "']' al cerrar la lista");
    return l;
}

ExprP Parser::mapLiteral() {
    Token open = advance();
    auto m = std::make_unique<MapExpr>(open.loc);
    nlStack.push_back(false);
    while (!check(Tok::RBrace) && !check(Tok::Eof)) {
        ExprP key;
        if (check(Tok::Ident) || (isKeyword(cur().kind) && ahead(1).kind == Tok::Colon)) {
            Token k = advance();
            auto s = std::make_unique<StringExpr>(k.loc);
            s->parts.push_back({false, k.text, nullptr});
            key = std::move(s);
            if (k.kind == Tok::Ident && (check(Tok::Comma) || check(Tok::RBrace) || cur().newlineBefore) && !check(Tok::Colon)) {
                // Atajo { nombre } == { nombre: nombre }
                m->entries.push_back({std::move(key), std::make_unique<IdentExpr>(k.loc, k.text)});
                if (!match(Tok::Comma) && !check(Tok::RBrace) && !cur().newlineBefore) break;
                continue;
            }
        } else if (check(Tok::String)) {
            key = stringLiteral(advance());
        } else if (check(Tok::Number)) {
            Token n = advance();
            key = std::make_unique<NumberExpr>(n.loc, n.number, n.isInt);
        } else if (match(Tok::LBracket)) {
            key = expression();
            expect(Tok::RBracket, "']'");
        } else {
            nlStack.pop_back();
            fail(cur().loc, "Se esperaba una clave en el mapa (nombre, \"texto\" o [expresión])");
        }
        expect(Tok::Colon, "':' entre la clave y el valor");
        ExprP val = expression();
        m->entries.push_back({std::move(key), std::move(val)});
        if (!match(Tok::Comma)) {
            if (check(Tok::RBrace)) break;
            if (cur().newlineBefore) continue;
            nlStack.pop_back();
            fail(cur().loc, "Se esperaba ',' o '}' en el mapa");
        }
    }
    nlStack.pop_back();
    expect(Tok::RBrace, "'}' al cerrar el mapa");
    return m;
}

ExprP Parser::matchExpr() {
    Token kw = advance();
    auto m = std::make_unique<MatchExpr>(kw.loc);
    nlStack.push_back(false);
    m->subject = expression();
    nlStack.pop_back();
    expect(Tok::LBrace, "'{' después de la expresión de match");
    nlStack.push_back(true);
    const size_t base = nlStack.size();
    while (!check(Tok::RBrace) && !check(Tok::Eof)) {
        if (match(Tok::Semicolon) || match(Tok::Comma)) continue;
        // Un error en un caso no debe romper el resto del match ni las llaves de fuera:
        // se informa y se salta hasta el siguiente caso o hasta la '}' del match.
        try {
            MatchArm arm;
            arm.loc = cur().loc;
            if (check(Tok::KwElse) || (check(Tok::Ident) && cur().text == "_" )) {
                advance();
                arm.isElse = true;
            } else {
                nlStack.push_back(false);
                do { arm.patterns.push_back(expression()); } while (match(Tok::Comma));
                nlStack.pop_back();
            }
            if (check(Tok::KwIf)) { advance(); arm.guard = expression(); }
            expect(Tok::Arrow, "'=>' en el caso del match");
            if (check(Tok::LBrace)) {
                auto b = block();
                arm.block = std::move(b->body);
                arm.hasBlock = true;
            } else if (check(Tok::KwReturn) || check(Tok::KwBreak) || check(Tok::KwContinue) || check(Tok::KwThrow)) {
                std::string kw = cur().text;
                fail(cur().loc, "'" + kw + "' es una sentencia y no puede ser el resultado de un caso del match",
                     "ponla entre llaves:  => { " + kw + (kw == "return" || kw == "throw" ? " ..." : "") + " }");
            } else {
                arm.body = assignment();
            }
            bool wasBlock = arm.hasBlock; // tras '}' puede seguir otro caso en la misma línea
            m->arms.push_back(std::move(arm));
            if (!wasBlock && !check(Tok::RBrace) && !match(Tok::Comma) && !match(Tok::Semicolon) && !cur().newlineBefore)
                fail(cur().loc, "Se esperaba un salto de línea o ',' entre los casos del match");
        } catch (ParseError&) {
            nlStack.resize(base);
            int depth = 0;
            while (!check(Tok::Eof)) {
                if (check(Tok::LBrace) || check(Tok::LParen) || check(Tok::LBracket)) depth++;
                else if (check(Tok::RBrace) || check(Tok::RParen) || check(Tok::RBracket)) {
                    if (depth == 0) break;
                    depth--;
                }
                advance();
                if (depth == 0 && cur().newlineBefore) break;
            }
        }
    }
    nlStack.pop_back();
    expect(Tok::RBrace, "'}' al cerrar el match");
    return m;
}

ExprP Parser::onExpr(bool once) {
    Token kw = advance(); // on / once
    auto o = std::make_unique<OnExpr>(kw.loc);
    o->once = once;
    SrcLoc nl;
    std::string name = expectIdent("nombre de evento", &nl);
    ExprP ev = std::make_unique<IdentExpr>(nl, name);
    while (check(Tok::Dot)) {
        advance();
        auto m = std::make_unique<MemberExpr>(ev->loc);
        m->object = std::move(ev);
        m->name = expectIdent("nombre", &m->nameLoc);
        ev = std::move(m);
    }
    o->event = std::move(ev);
    if (checkWord("priority")) { advance(); o->priority = unary(); }
    auto fn = std::make_unique<FuncDecl>();
    fn->kind = FuncKind::Handler;
    fn->loc = kw.loc;
    fn->name = "on " + name;
    Param prm;
    prm.name = "event";
    prm.loc = kw.loc;
    if (checkWord("as")) {
        advance();
        prm.name = expectIdent("nombre para el evento", &prm.loc);
    }
    fn->params.push_back(std::move(prm));
    funcDepth++;
    fn->body = block();
    funcDepth--;
    o->handler = std::move(fn);
    return o;
}

ExprP Parser::primary() {
    const Token& t = cur();
    switch (t.kind) {
        case Tok::Number: { Token n = advance(); return std::make_unique<NumberExpr>(n.loc, n.number, n.isInt); }
        case Tok::String: { Token s = advance(); return stringLiteral(s); }
        case Tok::KwTrue: { Token b = advance(); return std::make_unique<BoolExpr>(b.loc, true); }
        case Tok::KwFalse: { Token b = advance(); return std::make_unique<BoolExpr>(b.loc, false); }
        case Tok::KwNull: { Token b = advance(); return std::make_unique<NullExpr>(b.loc); }
        case Tok::KwThis: { Token b = advance(); return std::make_unique<ThisExpr>(b.loc); }
        case Tok::KwSuper: {
            Token b = advance();
            auto s = std::make_unique<SuperExpr>(b.loc);
            if (match(Tok::Dot)) s->member = expectIdent("método de la clase base");
            else if (!check(Tok::LParen)) fail(cur().loc, "Se esperaba '.' o '(' después de 'super'");
            return s;
        }
        case Tok::Ident: {
            if (ahead(1).kind == Tok::Arrow) return lambda(false);
            if ((t.text == "on" || t.text == "once") && ahead(1).kind == Tok::Ident && !ahead(1).newlineBefore) return onExpr(t.text == "once");
            Token id = advance();
            return std::make_unique<IdentExpr>(id.loc, id.text);
        }
        case Tok::KwAsync: {
            advance();
            if (check(Tok::KwFunction)) {
                auto fn = function(FuncKind::Lambda, true, Visibility::Public, false);
                auto l = std::make_unique<LambdaExpr>(fn->loc);
                l->fn = std::move(fn);
                return l;
            }
            if (check(Tok::Ident) || (check(Tok::LParen) && looksLikeLambda())) return lambda(true);
            fail(cur().loc, "Se esperaba una función después de 'async'");
        }
        case Tok::KwFunction: {
            auto fn = function(FuncKind::Lambda, false, Visibility::Public, false);
            auto l = std::make_unique<LambdaExpr>(fn->loc);
            l->fn = std::move(fn);
            return l;
        }
        case Tok::LParen: {
            if (looksLikeLambda()) return lambda(false);
            advance();
            nlStack.push_back(false);
            ExprP e = assignment();
            nlStack.pop_back();
            expect(Tok::RParen, "')'");
            return e;
        }
        case Tok::LBracket: return listLiteral();
        case Tok::LBrace: return mapLiteral();
        case Tok::KwMatch: return matchExpr();
        case Tok::Eof:
            fail(t.loc, "El archivo terminó cuando se esperaba una expresión", "puede que falte cerrar un paréntesis o una llave");
        default: break;
    }
    if (isKeyword(t.kind))
        fail(t.loc, "No se esperaba la palabra clave '" + t.text + "' aquí");
    fail(t.loc, "No se esperaba '" + t.text + "' aquí");
}

std::unique_ptr<Program> parseSource(const std::string& file, const std::string& source, DiagnosticBag& diags) {
    auto prog = std::make_unique<Program>();
    prog->file = file;
    prog->source = source;
    diags.file = file;
    Lexer lx(prog->source, diags);
    auto tokens = lx.tokenize();
    prog->comments = lx.comments;
    Parser ps(std::move(tokens), diags, prog->comments);
    prog->body = ps.parseProgram();
    return prog;
}

} // namespace hz
