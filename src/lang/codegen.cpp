#include "codegen.h"

#include <algorithm>
#include <set>

namespace hz {

CodeGen::CodeGen(VM& v, ModuleUnit& u) : vm(v), unit(u), diags(u.diags) {}

// ---------------------------------------------------------------------------
// Emisión
// ---------------------------------------------------------------------------

void CodeGen::setLoc(SrcLoc loc) {
    if (loc.line <= 0) return;
    auto& lines = fs->fn->lines;
    if (!lines.empty() && lines.back().line == loc.line && lines.back().col == loc.col) return;
    if (!lines.empty() && lines.back().offset == here()) { lines.back().line = loc.line; lines.back().col = loc.col; return; }
    lines.push_back({here(), loc.line, loc.col});
}

void CodeGen::op(uint8_t o, SrcLoc loc) {
    setLoc(loc);
    fs->fn->code.push_back(o);
}
void CodeGen::u8(uint8_t v) { fs->fn->code.push_back(v); }
// GET_PROP/SET_PROP llevan un byte de caché con el índice del campo (0xFF = desconocido).
void CodeGen::propOp(uint8_t o, int name, SrcLoc loc) { op(o, loc); u16(name); u8(0xFF); }
void CodeGen::u16(int v) {
    if (v < 0 || v > 0xFFFF) { diags.error(fs->lastLoc, "Función demasiado grande (más de 65535 constantes o saltos)", "divide el código en funciones más pequeñas", "E0900"); v = 0; }
    fs->fn->code.push_back((uint8_t)(v >> 8));
    fs->fn->code.push_back((uint8_t)(v & 0xFF));
}
int CodeGen::emitJump(uint8_t o, SrcLoc loc) {
    op(o, loc);
    u8(0xFF); u8(0xFF);
    return here() - 2;
}
void CodeGen::patchJump(int at) { patchJumpTo(at, here()); }
void CodeGen::patchJumpTo(int at, int target) {
    int dist = target - (at + 2);
    if (dist < 0 || dist > 0xFFFF) { diags.error(fs->lastLoc, "Salto demasiado largo en el bytecode", "divide el código en funciones más pequeñas", "E0900"); dist = 0; }
    fs->fn->code[at] = (uint8_t)(dist >> 8);
    fs->fn->code[at + 1] = (uint8_t)(dist & 0xFF);
}
void CodeGen::emitLoop(int start, SrcLoc loc) {
    op(OP_LOOP, loc);
    int dist = here() + 2 - start;
    if (dist > 0xFFFF) diags.error(fs->lastLoc, "Bucle demasiado largo", "", "E0900");
    u16(dist);
}

int CodeGen::addConst(Value v) {
    auto& c = fs->fn->constants;
    if (isString(v)) {
        for (size_t i = 0; i < c.size(); i++) if (isString(c[i]) && c[i].o == v.o) return (int)i;
    } else if (v.isNum()) {
        for (size_t i = 0; i < c.size(); i++) if (c[i].isNum() && c[i].n == v.n && std::signbit(c[i].n) == std::signbit(v.n)) return (int)i;
    }
    c.push_back(v);
    return (int)c.size() - 1;
}
int CodeGen::nameConst(const std::string& s) { return addConst(vm.str(s)); }

// ---------------------------------------------------------------------------
// Locales
// ---------------------------------------------------------------------------

void CodeGen::declareLocal(int expectedSlot, bool captured, SrcLoc loc) {
    if (expectedSlot >= 0 && expectedSlot != (int)fs->live.size()) {
        diags.error(loc, "Error interno del compilador: desajuste de pila (slot " + std::to_string(expectedSlot) + " / " +
                             std::to_string(fs->live.size()) + ")", "por favor, reporta este fallo con el código que lo produce", "E0999");
    }
    fs->live.push_back(captured);
}

void CodeGen::emitPopsTo(int n) {
    for (int i = (int)fs->live.size() - 1; i >= n; i--) op(fs->live[i] ? OP_CLOSE_UPVALUE : OP_POP);
}

void CodeGen::popLocalsTo(int n) {
    emitPopsTo(n);
    if ((int)fs->live.size() > n) fs->live.resize(n);
}

void CodeGen::registerBlock(BlockStmt* b) {
    for (auto& l : b->locals) fs->capturedHint[l.slot] = l.captured;
}

void CodeGen::callFinally(FuncDecl* f, SrcLoc loc) {
    emitClosure(f, nullptr, loc);
    op(OP_CALL, loc); u8(0);
    op(OP_POP);
}

void CodeGen::exitTries(int toDepth) {
    for (int i = (int)fs->tries.size() - 1; i >= toDepth; i--) {
        auto& t = fs->tries[i];
        if (t.handlerActive) op(OP_END_TRY);
        if (t.finallyFn) callFinally(t.finallyFn, {});
    }
}

// ---------------------------------------------------------------------------
// Funciones
// ---------------------------------------------------------------------------

static bool checkableType(const TypeRef* t) {
    if (!t) return false;
    static const std::set<std::string> prim = {"number", "int", "float", "text", "string", "bool", "list", "map", "set", "function"};
    return prim.count(t->name) > 0 || (!t->name.empty() && std::isupper((unsigned char)t->name[0]) && t->args.empty() &&
                                       t->name != "Task" && t->name != "Promise" && t->name != "Future");
}

ObjFunction* CodeGen::compileFunction(FuncDecl* f, ClassStmt* owner, bool isFieldInit, ClassStmt* fieldsOf) {
    FnState state;
    FnState* enclosing = fs;
    fs = &state;
    state.decl = f;
    ObjFunction* fn = vm.alloc<ObjFunction>();
    state.fn = fn;
    fn->module = unit.module;
    fn->name = vm.intern(f->name.empty() ? "<lambda>" : f->name);
    if (owner) fn->className = vm.intern(owner->name);
    fn->isAsync = f->isAsync;
    fn->isMethod = owner && !f->isStatic;
    fn->maxSlots = f->maxSlots;
    int arity = 0, minArity = 0;
    for (auto& p : f->params) {
        fn->paramNames.push_back(vm.intern(p.name));
        if (p.rest) { fn->hasRest = true; continue; }
        arity++;
        if (!p.def) minArity = arity;
    }
    fn->arity = arity;
    fn->minArity = minArity;
    state.lastLoc = f->loc;
    if (f->body) registerBlock(f->body.get());
    state.live.push_back(false); // slot 0
    for (auto& p : f->params) state.live.push_back(state.capturedHint.count(p.slot) ? state.capturedHint[p.slot] : false);

    // Prólogo: valores por defecto y comprobación de tipos
    for (auto& p : f->params) {
        if (p.def) {
            op(OP_DEFAULT_ARG, p.loc); u16(p.slot);
            int skip = here(); u8(0xFF); u8(0xFF);
            expr(p.def.get());
            op(OP_SET_LOCAL); u16(p.slot);
            op(OP_POP);
            patchJump(skip);
        }
    }
    for (auto& p : f->params) {
        if (p.type && !p.rest && checkableType(p.type.get())) {
            op(OP_CHECK_TYPE, p.loc); u16(p.slot); u16(nameConst(p.type->toString())); u16(nameConst(p.name));
        }
    }
    if (f->kind == FuncKind::Constructor && owner && !owner->superName.empty() && !f->hasExplicitSuperCall) {
        op(OP_GET_LOCAL, f->loc); u16(0);
        op(OP_SUPER_INIT_AUTO, f->loc);
        op(OP_POP);
    }

    if (isFieldInit && fieldsOf) {
        for (auto& fd : fieldsOf->fields) {
            if (fd.isStatic || !fd.init) continue;
            op(OP_GET_LOCAL, fd.loc); u16(0);
            expr(fd.init.get());
            propOp(OP_SET_PROP, nameConst(fd.name), fd.loc);
            op(OP_POP);
        }
        op(OP_NULL); op(OP_RETURN);
    } else if (f->body) {
        for (auto& s : f->body->body) stmt(s.get());
        op(OP_NULL, f->body->endLoc);
        op(OP_RETURN);
    } else if (f->exprBody) {
        Expr* e = f->exprBody.get();
        if (e->kind == ExprKind::Assign) assign(static_cast<AssignExpr*>(e), false);
        else expr(e);
        op(OP_RETURN, e->loc);
    } else {
        op(OP_NULL); op(OP_RETURN);
    }
    fn->upvalueCount = (int)f->upvalues.size();
    fs = enclosing;
    return fn;
}

void CodeGen::emitClosure(FuncDecl* f, ClassStmt* owner, SrcLoc loc, bool isFieldInit, ClassStmt* fieldsOf) {
    ObjFunction* fn = compileFunction(f, owner, isFieldInit, fieldsOf);
    int k = addConst(Value::object(fn));
    op(OP_CLOSURE, loc); u16(k);
    for (auto& up : f->upvalues) { u8(up.isLocal ? 1 : 0); u16(up.index); }
}

// ---------------------------------------------------------------------------
// Script
// ---------------------------------------------------------------------------

ObjFunction* CodeGen::compileScript() {
    Program& prog = *unit.program;
    FnState state;
    fs = &state;
    ObjFunction* fn = vm.alloc<ObjFunction>();
    state.fn = fn;
    state.decl = prog.script.get();
    fn->module = unit.module;
    fn->name = vm.intern("<principal>");
    fn->maxSlots = prog.script->maxSlots;
    state.live.push_back(false);

    auto& body = prog.body;
    std::vector<bool> done(body.size(), false);
    // Consola interactiva: las sentencias de entradas anteriores ya se ejecutaron (solo se analizan).
    ReplSession* repl = unit.repl;
    if (repl) for (size_t i = 0; i < body.size(); i++) done[i] = body[i]->loc.offset < repl->start;
    // 1) imports
    for (size_t i = 0; i < body.size(); i++) if (!done[i] && body[i]->kind == StmtKind::Import) { stmt(body[i].get()); done[i] = true; }
    // 2) funciones, eventos, enums, interfaces (izado)
    for (size_t i = 0; i < body.size(); i++) {
        auto k = body[i]->kind;
        if (done[i]) continue;
        if (k == StmtKind::Function || k == StmtKind::Event || k == StmtKind::Enum || k == StmtKind::Interface) { stmt(body[i].get()); done[i] = true; }
    }
    // 3) clases, con la clase base del mismo archivo antes que la derivada
    std::vector<ClassStmt*> classes;
    for (size_t i = 0; i < body.size(); i++) if (!done[i] && body[i]->kind == StmtKind::Class) { classes.push_back(static_cast<ClassStmt*>(body[i].get())); done[i] = true; }
    std::set<std::string> emitted;
    std::set<std::string> local;
    for (auto* c : classes) local.insert(c->name);
    for (size_t guard = 0; guard <= classes.size() && emitted.size() < classes.size(); guard++) {
        for (auto* c : classes) {
            if (emitted.count(c->name)) continue;
            if (!c->superName.empty() && local.count(c->superName) && !emitted.count(c->superName)) continue;
            classDecl(c);
            emitted.insert(c->name);
        }
    }
    // 4) el resto en orden. En la consola se muestra el valor de una expresión final.
    for (size_t i = 0; i < body.size(); i++) {
        if (done[i]) continue;
        Stmt* s = body[i].get();
        Expr* e = s->kind == StmtKind::Expr ? static_cast<ExprStmt*>(s)->expr.get() : nullptr;
        bool echo = repl && repl->echoSlot >= 0 && e && i + 1 == body.size() && e->kind != ExprKind::Assign &&
                    e->kind != ExprKind::IncDec;
        if (!echo) { stmt(s); continue; }
        op(OP_GET_BUILTIN, e->loc); u16(repl->echoSlot);
        expr(e);
        op(OP_CALL, e->loc); u8(1);
        op(OP_POP);
    }
    op(OP_NULL); op(OP_RETURN);
    fs = nullptr;
    return fn;
}

// ---------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------

void CodeGen::block(BlockStmt* b) {
    registerBlock(b);
    int base = (int)fs->live.size();
    for (auto& s : b->body) stmt(s.get());
    popLocalsTo(base);
}

void CodeGen::classDecl(ClassStmt* c) {
    SrcLoc loc = c->loc;
    op(OP_CLASS, loc); u16(nameConst(c->name)); u8(c->isStruct ? 1 : 0);
    // Carga un nombre global (posiblemente "Modulo.Clase") por su nombre
    auto loadNamed = [&](const std::string& full, SrcLoc loc) -> bool {
        std::string head = full.substr(0, full.find('.'));
        Resolution r;
        auto g = unit.globalIndex.find(head);
        if (g != unit.globalIndex.end()) { r.kind = VarKind::Global; r.index = g->second; }
        else { auto b = vm.builtinIndex.find(head); if (b != vm.builtinIndex.end()) { r.kind = VarKind::Builtin; r.index = b->second; } }
        if (r.kind == VarKind::Unresolved) return false;
        load(r, head, loc);
        size_t pos = head.size();
        while (pos < full.size()) {
            size_t next = full.find('.', pos + 1);
            std::string part = full.substr(pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1);
            propOp(OP_GET_PROP, nameConst(part), loc);
            pos = next == std::string::npos ? full.size() : next;
        }
        return true;
    };
    if (!c->superName.empty() && loadNamed(c->superName, c->superLoc)) op(OP_INHERIT, c->superLoc);
    for (auto& [iname, iloc] : c->interfaces) {
        if (!loadNamed(iname, iloc)) continue;
        std::string shortName = iname.substr(iname.rfind('.') == std::string::npos ? 0 : iname.rfind('.') + 1);
        op(OP_IMPLEMENTS, iloc); u16(nameConst(shortName));
    }
    for (auto& f : c->fields) {
        if (f.isStatic) continue;
        op(OP_FIELD, f.loc); u16(nameConst(f.name)); u8(0);
    }
    if (c->fieldInit) {
        emitClosure(c->fieldInit.get(), c, c->loc, true, c);
        op(OP_METHOD); u16(nameConst("<campos>")); u8(MK_FIELDINIT);
    }
    for (auto& m : c->methods) {
        emitClosure(m.get(), c, m->loc);
        uint8_t kind = MK_METHOD;
        if (m->kind == FuncKind::Getter) kind = m->isStatic ? MK_STATIC_GETTER : MK_GETTER;
        else if (m->kind == FuncKind::Setter) kind = m->isStatic ? MK_STATIC_SETTER : MK_SETTER;
        else if (m->isStatic) kind = MK_STATIC;
        op(OP_METHOD); u16(nameConst(m->name)); u8(kind);
    }
    if (c->ctor) {
        emitClosure(c->ctor.get(), c, c->ctor->loc);
        op(OP_METHOD); u16(nameConst("constructor")); u8(MK_CTOR);
    }
    op(OP_DUP);
    op(OP_DEFINE_GLOBAL); u16(c->res.index);
    for (auto& f : c->fields) {
        if (!f.isStatic) continue;
        op(OP_DUP, f.loc);
        if (f.init) expr(f.init.get()); else op(OP_NULL);
        propOp(OP_SET_PROP, nameConst(f.name), f.loc);
        op(OP_POP);
    }
    op(OP_POP);
}

void CodeGen::stmt(Stmt* s) {
    fs->lastLoc = s->loc;
    switch (s->kind) {
        case StmtKind::Expr: {
            Expr* e = static_cast<ExprStmt*>(s)->expr.get();
            if (e->kind == ExprKind::Assign) { assign(static_cast<AssignExpr*>(e), true); break; }
            if (e->kind == ExprKind::IncDec) { incDec(static_cast<IncDecExpr*>(e), true); break; }
            expr(e);
            op(OP_POP);
            break;
        }
        case StmtKind::Var: {
            auto* v = static_cast<VarStmt*>(s);
            if (v->init) expr(v->init.get()); else op(OP_NULL, v->loc);
            if (v->res.kind == VarKind::Global) { op(OP_DEFINE_GLOBAL, v->loc); u16(v->res.index); }
            else declareLocal(v->res.index, fs->capturedHint.count(v->res.index) && fs->capturedHint[v->res.index], v->loc);
            break;
        }
        case StmtKind::Block: block(static_cast<BlockStmt*>(s)); break;
        case StmtKind::If: {
            auto* i = static_cast<IfStmt*>(s);
            expr(i->cond.get());
            int elseJ = emitJump(OP_JUMP_IF_FALSE, i->loc);
            stmt(i->then.get());
            if (i->otherwise) {
                int endJ = emitJump(OP_JUMP);
                patchJump(elseJ);
                stmt(i->otherwise.get());
                patchJump(endJ);
            } else patchJump(elseJ);
            break;
        }
        case StmtKind::While: {
            auto* w = static_cast<WhileStmt*>(s);
            LoopCtx loop;
            loop.start = here();
            loop.breakDepth = loop.continueDepth = (int)fs->live.size();
            loop.tryDepth = (int)fs->tries.size();
            expr(w->cond.get());
            int exitJ = emitJump(OP_JUMP_IF_FALSE, w->loc);
            fs->loops.push_back(loop);
            stmt(w->body.get());
            LoopCtx done = fs->loops.back();
            fs->loops.pop_back();
            for (int c : done.continues) patchJumpTo(c, here());
            emitLoop(loop.start, w->loc);
            patchJump(exitJ);
            for (int b : done.breaks) patchJump(b);
            break;
        }
        case StmtKind::ForRange: {
            auto* f = static_cast<ForRangeStmt*>(s);
            int base = (int)fs->live.size();
            expr(f->start.get()); declareLocal(f->slotCounter, false, f->loc);
            expr(f->end.get()); declareLocal(f->slotEnd, false, f->loc);
            if (f->step) expr(f->step.get()); else { op(OP_CONST); u16(addConst(Value(1.0))); }
            declareLocal(f->slotStep, false, f->loc);
            LoopCtx loop;
            loop.start = here();
            op(OP_FOR_CHECK, f->loc); u16(f->slotCounter); u8(f->inclusive ? 1 : 0);
            int exitJ = here(); u8(0xFF); u8(0xFF);
            op(OP_GET_LOCAL); u16(f->slotCounter);
            declareLocal(f->slotVar, f->varCaptured, f->varLoc);
            loop.breakDepth = base + 3;
            loop.continueDepth = base + 4;
            loop.tryDepth = (int)fs->tries.size();
            fs->loops.push_back(loop);
            stmt(f->body.get());
            LoopCtx done = fs->loops.back();
            fs->loops.pop_back();
            for (int c : done.continues) patchJumpTo(c, here());
            popLocalsTo(base + 3);
            op(OP_FOR_INCR); u16(f->slotCounter);
            emitLoop(loop.start);
            patchJump(exitJ);
            for (int b : done.breaks) patchJump(b);
            popLocalsTo(base);
            break;
        }
        case StmtKind::ForEach: {
            auto* f = static_cast<ForEachStmt*>(s);
            int base = (int)fs->live.size();
            expr(f->iterable.get());
            op(OP_ITER_PREP, f->iterable->loc);
            declareLocal(f->slotIter, false, f->loc);
            declareLocal(f->slotState, false, f->loc);
            LoopCtx loop;
            loop.start = here();
            op(OP_ITER_NEXT, f->loc); u16(f->slotIter);
            int exitJ = here(); u8(0xFF); u8(0xFF);
            declareLocal(f->slotVar2, f->var2Captured, f->loc);
            declareLocal(f->slotVar, f->varCaptured, f->varLoc);
            loop.breakDepth = base + 2;
            loop.continueDepth = base + 4;
            loop.tryDepth = (int)fs->tries.size();
            fs->loops.push_back(loop);
            stmt(f->body.get());
            LoopCtx done = fs->loops.back();
            fs->loops.pop_back();
            for (int c : done.continues) patchJumpTo(c, here());
            popLocalsTo(base + 2);
            emitLoop(loop.start);
            patchJump(exitJ);
            for (int b : done.breaks) patchJump(b);
            popLocalsTo(base);
            break;
        }
        case StmtKind::Repeat: {
            auto* r = static_cast<RepeatStmt*>(s);
            int base = (int)fs->live.size();
            expr(r->count.get());
            op(OP_REPEAT_PREP, r->count->loc);
            declareLocal(r->slotCounter, false, r->loc);
            LoopCtx loop;
            loop.start = here();
            op(OP_REPEAT_NEXT, r->loc); u16(r->slotCounter);
            int exitJ = here(); u8(0xFF); u8(0xFF);
            loop.breakDepth = base + 1;
            loop.continueDepth = base + 1;
            loop.tryDepth = (int)fs->tries.size();
            fs->loops.push_back(loop);
            stmt(r->body.get());
            LoopCtx done = fs->loops.back();
            fs->loops.pop_back();
            for (int c : done.continues) patchJumpTo(c, here());
            emitLoop(loop.start);
            patchJump(exitJ);
            for (int b : done.breaks) patchJump(b);
            popLocalsTo(base);
            break;
        }
        case StmtKind::Break: case StmtKind::Continue: {
            if (fs->loops.empty()) break;
            LoopCtx& loop = fs->loops.back();
            bool isBreak = s->kind == StmtKind::Break;
            exitTries(loop.tryDepth);
            emitPopsTo(isBreak ? loop.breakDepth : loop.continueDepth);
            int j = emitJump(OP_JUMP, s->loc);
            (isBreak ? loop.breaks : loop.continues).push_back(j);
            break;
        }
        case StmtKind::Return: {
            auto* r = static_cast<ReturnStmt*>(s);
            if (r->value) {
                if (r->value->kind == ExprKind::Assign) assign(static_cast<AssignExpr*>(r->value.get()), false);
                else expr(r->value.get());
            } else op(OP_NULL, r->loc);
            exitTries(0);
            op(OP_RETURN, r->loc);
            break;
        }
        case StmtKind::Throw: {
            expr(static_cast<ThrowStmt*>(s)->value.get());
            op(OP_THROW, s->loc);
            break;
        }
        case StmtKind::Try: {
            auto* t = static_cast<TryStmt*>(s);
            FuncDecl* fin = t->finallyFn.get();
            int tryJ = emitJump(OP_TRY, t->loc);
            fs->tries.push_back({true, fin});
            block(t->body.get());
            fs->tries.pop_back();
            op(OP_END_TRY);
            if (fin) callFinally(fin, t->loc);
            int endJ = emitJump(OP_JUMP);
            patchJump(tryJ);
            // Excepción en la cima de la pila
            if (t->catchBody) {
                int base = (int)fs->live.size();
                declareLocal(t->catchSlot, t->catchCaptured, t->catchLoc);
                int try2 = -1;
                if (fin) try2 = emitJump(OP_TRY, t->catchBody->loc);
                fs->tries.push_back({fin != nullptr, fin});
                block(t->catchBody.get());
                fs->tries.pop_back();
                if (fin) op(OP_END_TRY);
                popLocalsTo(base);
                if (fin) {
                    callFinally(fin, t->loc);
                    int end2 = emitJump(OP_JUMP);
                    patchJump(try2);
                    // excepción dentro del catch: finally y relanzar
                    callFinally(fin, t->loc);
                    op(OP_THROW);
                    patchJump(end2);
                }
            } else {
                callFinally(fin, t->loc);
                op(OP_THROW);
            }
            patchJump(endJ);
            break;
        }
        case StmtKind::Function: {
            auto* f = static_cast<FuncStmt*>(s)->fn.get();
            if (f->res.kind == VarKind::Global) {
                emitClosure(f, nullptr, f->loc);
                op(OP_DEFINE_GLOBAL); u16(f->res.index);
            } else {
                // Declarar el slot antes de crear la clausura (recursión)
                declareLocal(f->res.index, fs->capturedHint.count(f->res.index) && fs->capturedHint[f->res.index], f->loc);
                fs->live.pop_back();
                emitClosure(f, nullptr, f->loc);
                fs->live.push_back(fs->capturedHint.count(f->res.index) && fs->capturedHint[f->res.index]);
            }
            break;
        }
        case StmtKind::Class: classDecl(static_cast<ClassStmt*>(s)); break;
        case StmtKind::Interface: {
            auto* i = static_cast<InterfaceStmt*>(s);
            op(OP_CLASS, i->loc); u16(nameConst(i->name)); u8(2);
            op(OP_DEFINE_GLOBAL); u16(i->res.index);
            break;
        }
        case StmtKind::Enum: {
            auto* e = static_cast<EnumStmt*>(s);
            for (auto& m : e->members) {
                op(OP_CONST, m.loc); u16(nameConst(m.name));
                op(OP_CONST); u16(m.isString ? nameConst(m.strValue) : addConst(Value(m.resolved)));
            }
            op(OP_ENUM, e->loc); u16(nameConst(e->name)); u16((int)e->members.size());
            op(OP_DEFINE_GLOBAL); u16(e->res.index);
            break;
        }
        case StmtKind::Event: {
            auto* e = static_cast<EventStmt*>(s);
            ObjList* names = vm.newList();
            for (auto& p : e->params) names->items.push_back(vm.str(p.name));
            op(OP_EVENT, e->loc); u16(nameConst(e->name)); u16(addConst(Value::object(names)));
            op(OP_DEFINE_GLOBAL); u16(e->res.index);
            break;
        }
        case StmtKind::Emit: {
            auto* e = static_cast<EmitStmt*>(s);
            expr(e->event.get());
            bool named = false;
            for (auto& a : e->args) if (!a.name.empty()) named = true;
            if (named && e->event->type && e->event->type->kind == TK::Event && e->event->type->ev) {
                auto& params = e->event->type->ev->params;
                std::vector<Expr*> ordered(params.size(), nullptr);
                size_t pos = 0;
                for (auto& a : e->args) {
                    if (a.name.empty()) { if (pos < ordered.size()) ordered[pos++] = a.value.get(); continue; }
                    for (size_t i = 0; i < params.size(); i++) if (params[i].name == a.name) ordered[i] = a.value.get();
                }
                for (auto* x : ordered) { if (x) expr(x); else op(OP_NULL); }
                op(OP_EMIT, e->loc); u8((uint8_t)ordered.size());
            } else {
                for (auto& a : e->args) expr(a.value.get());
                op(OP_EMIT, e->loc); u8((uint8_t)e->args.size());
            }
            break;
        }
        case StmtKind::Import: {
            auto* i = static_cast<ImportStmt*>(s);
            std::string key = i->isFile || !i->resolvedPath.empty() ? i->resolvedPath : i->path;
            if (key.empty()) key = i->path;
            op(OP_IMPORT, i->loc); u16(nameConst(key));
            if (i->names.empty()) {
                op(OP_DEFINE_GLOBAL); u16(i->res.index);
            } else {
                for (size_t k = 0; k < i->names.size(); k++) {
                    op(OP_DUP);
                    op(OP_IMPORT_NAME, i->names[k].second); u16(nameConst(i->names[k].first));
                    op(OP_DEFINE_GLOBAL); u16(i->nameRes[k].index);
                }
                op(OP_POP);
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------

void CodeGen::load(const Resolution& r, const std::string& name, SrcLoc loc) {
    switch (r.kind) {
        case VarKind::Local: op(OP_GET_LOCAL, loc); u16(r.index); break;
        case VarKind::Upvalue: op(OP_GET_UPVAL, loc); u16(r.index); break;
        case VarKind::Global: op(OP_GET_GLOBAL, loc); u16(r.index); break;
        case VarKind::Builtin: op(OP_GET_BUILTIN, loc); u16(r.index); break;
        case VarKind::ThisMember:
            op(r.thisKind == VarKind::Local ? OP_GET_LOCAL : OP_GET_UPVAL, loc); u16(r.thisIndex);
            propOp(OP_GET_PROP, nameConst(name), loc);
            break;
        case VarKind::StaticMember:
            op(r.classKind == VarKind::Global ? OP_GET_GLOBAL : OP_GET_BUILTIN, loc); u16(r.classIndex);
            propOp(OP_GET_PROP, nameConst(name), loc);
            break;
        case VarKind::Unresolved: op(OP_NULL, loc); break;
    }
}

void CodeGen::store(const Resolution& r, const std::string& name, SrcLoc loc) {
    switch (r.kind) {
        case VarKind::Local: op(OP_SET_LOCAL, loc); u16(r.index); break;
        case VarKind::Upvalue: op(OP_SET_UPVAL, loc); u16(r.index); break;
        case VarKind::Global: op(OP_SET_GLOBAL, loc); u16(r.index); break;
        case VarKind::ThisMember:
            // valor en la cima: [v] -> [this v] -> SET_PROP
            op(r.thisKind == VarKind::Local ? OP_GET_LOCAL : OP_GET_UPVAL, loc); u16(r.thisIndex);
            op(OP_SWAP);
            propOp(OP_SET_PROP, nameConst(name), loc);
            break;
        case VarKind::StaticMember:
            op(r.classKind == VarKind::Global ? OP_GET_GLOBAL : OP_GET_BUILTIN, loc); u16(r.classIndex);
            op(OP_SWAP);
            propOp(OP_SET_PROP, nameConst(name), loc);
            break;
        default: break;
    }
}

uint8_t CodeGen::binOp(Tok t) {
    switch (t) {
        case Tok::Plus: case Tok::PlusEq: case Tok::PlusPlus: return OP_ADD;
        case Tok::Minus: case Tok::MinusEq: case Tok::MinusMinus: return OP_SUB;
        case Tok::Star: case Tok::StarEq: return OP_MUL;
        case Tok::Slash: case Tok::SlashEq: return OP_DIV;
        case Tok::Percent: case Tok::PercentEq: return OP_MOD;
        case Tok::StarStar: return OP_POW;
        case Tok::EqEq: return OP_EQ;
        case Tok::NotEq: return OP_NE;
        case Tok::Less: return OP_LT;
        case Tok::LessEq: return OP_LE;
        case Tok::Greater: return OP_GT;
        case Tok::GreaterEq: return OP_GE;
        case Tok::Amp: return OP_BAND;
        case Tok::Pipe: return OP_BOR;
        case Tok::Caret: return OP_BXOR;
        case Tok::Shl: return OP_SHL;
        case Tok::Shr: return OP_SHR;
        case Tok::KwIn: return OP_IN;
        default: return OP_ADD;
    }
}

void CodeGen::assign(AssignExpr* a, bool discard) {
    Expr* t = a->target.get();
    bool compound = a->op != Tok::Eq;
    bool nullish = a->op == Tok::QuestionQuestionEq;
    SrcLoc loc = a->loc;
    if (t->kind == ExprKind::Ident) {
        auto* id = static_cast<IdentExpr*>(t);
        if (nullish) {
            load(id->res, id->name, id->loc);
            int skip = emitJump(OP_JUMP_IF_NOT_NULL_KEEP);
            op(OP_POP);
            expr(a->value.get());
            store(id->res, id->name, loc);
            patchJump(skip);
        } else {
            if (compound) load(id->res, id->name, id->loc);
            expr(a->value.get());
            if (compound) op(binOp(a->op), loc);
            store(id->res, id->name, loc);
        }
    } else if (t->kind == ExprKind::Member) {
        auto* m = static_cast<MemberExpr*>(t);
        expr(m->object.get());
        int name = nameConst(m->name);
        if (nullish) {
            op(OP_DUP); propOp(OP_GET_PROP, name, m->nameLoc);
            int skip = emitJump(OP_JUMP_IF_NOT_NULL_KEEP);
            op(OP_POP);
            expr(a->value.get());
            propOp(OP_SET_PROP, name, loc);
            int end = emitJump(OP_JUMP);
            patchJump(skip);
            op(OP_SWAP); op(OP_POP);
            patchJump(end);
        } else {
            if (compound) { op(OP_DUP); propOp(OP_GET_PROP, name, m->nameLoc); }
            expr(a->value.get());
            if (compound) op(binOp(a->op), loc);
            propOp(OP_SET_PROP, name, m->nameLoc);
        }
    } else if (t->kind == ExprKind::Index) {
        auto* ix = static_cast<IndexExpr*>(t);
        expr(ix->object.get());
        expr(ix->index.get());
        if (nullish) {
            op(OP_DUP2); op(OP_GET_INDEX, ix->loc);
            int skip = emitJump(OP_JUMP_IF_NOT_NULL_KEEP);
            op(OP_POP);
            expr(a->value.get());
            op(OP_SET_INDEX, loc);
            int end = emitJump(OP_JUMP);
            patchJump(skip);
            op(OP_SWAP); op(OP_POP); op(OP_SWAP); op(OP_POP);
            patchJump(end);
        } else {
            if (compound) { op(OP_DUP2); op(OP_GET_INDEX, ix->loc); }
            expr(a->value.get());
            if (compound) op(binOp(a->op), loc);
            op(OP_SET_INDEX, ix->loc);
        }
    } else {
        expr(a->value.get());
    }
    if (discard) op(OP_POP);
}

void CodeGen::incDec(IncDecExpr* e, bool discard) {
    Expr* t = e->target.get();
    uint8_t o = e->inc ? OP_ADD : OP_SUB;
    int one = addConst(Value(1.0));
    bool wantOld = !discard && !e->prefix;
    if (t->kind == ExprKind::Ident) {
        auto* id = static_cast<IdentExpr*>(t);
        load(id->res, id->name, id->loc);
        if (wantOld) op(OP_DUP);
        op(OP_CONST); u16(one);
        op(o, e->loc);
        store(id->res, id->name, e->loc);
        if (wantOld) op(OP_POP);
    } else if (t->kind == ExprKind::Member) {
        auto* m = static_cast<MemberExpr*>(t);
        expr(m->object.get());
        int name = nameConst(m->name);
        op(OP_DUP); propOp(OP_GET_PROP, name, m->nameLoc);
        op(OP_CONST); u16(one);
        op(o, e->loc);
        propOp(OP_SET_PROP, name, m->nameLoc);
        if (wantOld) { op(OP_CONST); u16(one); op(e->inc ? OP_SUB : OP_ADD); }
    } else if (t->kind == ExprKind::Index) {
        auto* ix = static_cast<IndexExpr*>(t);
        expr(ix->object.get());
        expr(ix->index.get());
        op(OP_DUP2); op(OP_GET_INDEX, ix->loc);
        op(OP_CONST); u16(one);
        op(o, e->loc);
        op(OP_SET_INDEX, ix->loc);
        if (wantOld) { op(OP_CONST); u16(one); op(e->inc ? OP_SUB : OP_ADD); }
    } else {
        expr(t);
    }
    if (discard) op(OP_POP);
}

void CodeGen::args(std::vector<Arg>& as, SrcLoc loc, std::function<void(int, int, bool)> finish) {
    std::vector<int>* savedChain = chainJumps;
    chainJumps = nullptr;
    bool spread = false, named = false;
    for (auto& a : as) { if (a.spread) spread = true; if (!a.name.empty()) named = true; }
    if (spread) {
        op(OP_LIST, loc); u16(0);
        for (auto& a : as) {
            expr(a.value.get());
            op(a.spread ? OP_LIST_EXTEND : OP_LIST_APPEND);
        }
        chainJumps = savedChain;
        finish((int)as.size(), -1, true);
        return;
    }
    for (auto& a : as) expr(a.value.get());
    int namesConst = -1;
    if (named) {
        ObjList* names = vm.newList();
        for (auto& a : as) names->items.push_back(a.name.empty() ? Value() : vm.str(a.name));
        namesConst = addConst(Value::object(names));
    }
    chainJumps = savedChain;
    if (as.size() > 255) diags.error(loc, "Demasiados argumentos (máximo 255)", "", "E0409");
    finish((int)as.size(), namesConst, false);
}

void CodeGen::call(CallExpr* c) {
    Expr* callee = c->callee.get();
    SrcLoc loc = c->loc;
    // super(...)
    if (callee->kind == ExprKind::Super && static_cast<SuperExpr*>(callee)->member.empty()) {
        auto* s = static_cast<SuperExpr*>(callee);
        load(s->thisRes, "this", loc);
        args(c->args, loc, [&](int argc, int names, bool spread) {
            (void)names; (void)spread;
            op(OP_SUPER_CALL, loc); u8((uint8_t)argc);
        });
        return;
    }
    // super.metodo(...)
    if (callee->kind == ExprKind::Super) {
        auto* s = static_cast<SuperExpr*>(callee);
        bool simple = !c->hasNamed();
        for (auto& a : c->args) if (a.spread) simple = false;
        load(s->thisRes, "this", loc);
        if (simple) {
            args(c->args, loc, [&](int argc, int, bool) { op(OP_SUPER_INVOKE, loc); u16(nameConst(s->member)); u8((uint8_t)argc); });
        } else {
            op(OP_GET_SUPER, loc); u16(nameConst(s->member));
            args(c->args, loc, [&](int argc, int names, bool spread) {
                if (spread) op(OP_CALL_SPREAD, loc);
                else { op(OP_CALL_NAMED, loc); u8((uint8_t)argc); u16(names); }
            });
        }
        return;
    }
    // obj.metodo(...)  -> INVOKE
    bool hasSpread = false;
    for (auto& a : c->args) if (a.spread) hasSpread = true;
    if (callee->kind == ExprKind::Member && !c->hasNamed() && !hasSpread && !c->optional) {
        auto* m = static_cast<MemberExpr*>(callee);
        expr(m->object.get());
        if (m->optional && chainJumps) chainJumps->push_back(emitJump(OP_JUMP_IF_NULL_KEEP, m->loc));
        args(c->args, loc, [&](int argc, int, bool) {
            op(OP_INVOKE, m->nameLoc); u16(nameConst(m->name)); u8((uint8_t)argc);
            u16(fs->fn->invokeCacheSize < 0xFFFF ? fs->fn->invokeCacheSize++ : 0xFFFF);
        });
        return;
    }
    expr(callee);
    if (c->optional && chainJumps) chainJumps->push_back(emitJump(OP_JUMP_IF_NULL_KEEP, loc));
    args(c->args, loc, [&](int argc, int names, bool spread) {
        if (spread) op(OP_CALL_SPREAD, loc);
        else if (names >= 0) { op(OP_CALL_NAMED, loc); u8((uint8_t)argc); u16(names); }
        else { op(OP_CALL, loc); u8((uint8_t)argc); }
    });
}

void CodeGen::matchExpr(MatchExpr* m) {
    std::vector<int>* savedChain = chainJumps;
    chainJumps = nullptr;
    expr(m->subject.get());
    std::vector<int> ends;
    bool hasElse = false;
    for (auto& arm : m->arms) {
        std::vector<int> toBody;
        int nextArm = -1;
        if (!arm.isElse) {
            for (auto& p : arm.patterns) {
                op(OP_DUP);
                expr(p.get());
                op(OP_MATCH_TEST, p->loc);
                toBody.push_back(emitJump(OP_JUMP_IF_TRUE));
            }
            nextArm = emitJump(OP_JUMP);
            for (int j : toBody) patchJump(j);
        } else hasElse = true;
        int guardFail = -1;
        if (arm.guard) {
            expr(arm.guard.get());
            guardFail = emitJump(OP_JUMP_IF_FALSE, arm.guard->loc);
        }
        op(OP_POP); // sujeto
        if (arm.hasBlock) {
            // El analizador envuelve el bloque del caso en un BlockStmt con sus locales.
            BlockStmt* blk = arm.block.size() == 1 && arm.block[0]->kind == StmtKind::Block ? static_cast<BlockStmt*>(arm.block[0].get()) : nullptr;
            ExprStmt* last = blk && !blk->body.empty() && blk->body.back()->kind == StmtKind::Expr ? static_cast<ExprStmt*>(blk->body.back().get()) : nullptr;
            if (last && last->isBlockValue) {
                registerBlock(blk);
                int base = (int)fs->live.size();
                for (size_t i = 0; i + 1 < blk->body.size(); i++) stmt(blk->body[i].get());
                expr(last->expr.get());
                int n = (int)fs->live.size() - base;
                if (n > 0) {
                    if (n > 255) diags.error(arm.loc, "Demasiadas variables en un caso de match", "", "E0900");
                    op(OP_SLIDE, arm.loc); u8((uint8_t)std::min(n, 255));
                    fs->live.resize(base);
                }
            } else {
                for (auto& s : arm.block) stmt(s.get());
                op(OP_NULL);
            }
        } else if (arm.body) {
            if (arm.body->kind == ExprKind::Assign) assign(static_cast<AssignExpr*>(arm.body.get()), false);
            else expr(arm.body.get());
        } else op(OP_NULL);
        ends.push_back(emitJump(OP_JUMP));
        if (guardFail >= 0) {
            // el guard falló: el sujeto sigue en la pila; saltamos al siguiente brazo
            int skip = here();
            patchJump(guardFail);
            (void)skip;
        }
        if (nextArm >= 0) patchJump(nextArm);
        if (arm.isElse && !arm.guard) break;
    }
    if (!hasElse || true) {
        op(OP_POP);
        op(OP_NULL);
    }
    for (int j : ends) patchJump(j);
    chainJumps = savedChain;
}

void CodeGen::expr(Expr* e) {
    switch (e->kind) {
        case ExprKind::Number: op(OP_CONST, e->loc); u16(addConst(Value(static_cast<NumberExpr*>(e)->value))); break;
        case ExprKind::String: {
            auto* s = static_cast<StringExpr*>(e);
            if (s->isPlain()) { op(OP_CONST, e->loc); u16(nameConst(s->parts[0].text)); break; }
            std::vector<int>* savedChain = chainJumps;
            chainJumps = nullptr;
            int n = 0;
            for (auto& p : s->parts) {
                if (p.isExpr) { if (p.expr) expr(p.expr.get()); else op(OP_NULL); op(OP_TO_STRING); }
                else { if (p.text.empty()) continue; op(OP_CONST, e->loc); u16(nameConst(p.text)); }
                n++;
            }
            chainJumps = savedChain;
            if (n == 0) { op(OP_CONST); u16(nameConst("")); }
            else if (n > 1) { op(OP_CONCAT, e->loc); u16(n); }
            break;
        }
        case ExprKind::Bool: op(static_cast<BoolExpr*>(e)->value ? OP_TRUE : OP_FALSE, e->loc); break;
        case ExprKind::Null: op(OP_NULL, e->loc); break;
        case ExprKind::Ident: {
            auto* id = static_cast<IdentExpr*>(e);
            load(id->res, id->name, id->loc);
            break;
        }
        case ExprKind::This: load(static_cast<ThisExpr*>(e)->res, "this", e->loc); break;
        case ExprKind::Super: {
            auto* s = static_cast<SuperExpr*>(e);
            load(s->thisRes, "this", e->loc);
            op(OP_GET_SUPER, e->loc); u16(nameConst(s->member));
            break;
        }
        case ExprKind::List: {
            auto* l = static_cast<ListExpr*>(e);
            std::vector<int>* savedChain = chainJumps;
            chainJumps = nullptr;
            bool spread = false;
            for (bool b : l->spread) spread = spread || b;
            if (!spread && l->items.size() < 0xFFFF) {
                for (auto& it : l->items) expr(it.get());
                op(OP_LIST, e->loc); u16((int)l->items.size());
            } else {
                op(OP_LIST, e->loc); u16(0);
                for (size_t i = 0; i < l->items.size(); i++) { expr(l->items[i].get()); op(l->spread[i] ? OP_LIST_EXTEND : OP_LIST_APPEND); }
            }
            chainJumps = savedChain;
            break;
        }
        case ExprKind::Map: {
            auto* m = static_cast<MapExpr*>(e);
            std::vector<int>* savedChain = chainJumps;
            chainJumps = nullptr;
            for (auto& [k, v] : m->entries) { expr(k.get()); expr(v.get()); }
            op(OP_MAP, e->loc); u16((int)m->entries.size());
            chainJumps = savedChain;
            break;
        }
        case ExprKind::Unary: {
            auto* u = static_cast<UnaryExpr*>(e);
            expr(u->operand.get());
            if (u->op == Tok::Minus) op(OP_NEG, e->loc);
            else if (u->op == Tok::KwNot) op(OP_NOT, e->loc);
            else if (u->op == Tok::Tilde) op(OP_BNOT, e->loc);
            break;
        }
        case ExprKind::Binary: {
            auto* b = static_cast<BinaryExpr*>(e);
            std::vector<int>* savedChain = chainJumps;
            chainJumps = nullptr;
            expr(b->left.get());
            expr(b->right.get());
            chainJumps = savedChain;
            op(binOp(b->op), b->loc);
            if (b->negate) op(OP_NOT);
            break;
        }
        case ExprKind::Logical: {
            auto* l = static_cast<LogicalExpr*>(e);
            std::vector<int>* savedChain = chainJumps;
            chainJumps = nullptr;
            expr(l->left.get());
            uint8_t j = l->op == Tok::KwAnd ? OP_JUMP_IF_FALSE_KEEP : l->op == Tok::KwOr ? OP_JUMP_IF_TRUE_KEEP : OP_JUMP_IF_NOT_NULL_KEEP;
            int end = emitJump(j, l->loc);
            op(OP_POP);
            expr(l->right.get());
            patchJump(end);
            chainJumps = savedChain;
            break;
        }
        case ExprKind::Ternary: {
            auto* t = static_cast<TernaryExpr*>(e);
            std::vector<int>* savedChain = chainJumps;
            chainJumps = nullptr;
            expr(t->cond.get());
            int elseJ = emitJump(OP_JUMP_IF_FALSE, t->loc);
            expr(t->then.get());
            int endJ = emitJump(OP_JUMP);
            patchJump(elseJ);
            expr(t->otherwise.get());
            patchJump(endJ);
            chainJumps = savedChain;
            break;
        }
        case ExprKind::Call: case ExprKind::Member: case ExprKind::Index: {
            bool outer = chainJumps == nullptr;
            std::vector<int> jumps;
            if (outer) chainJumps = &jumps;
            if (e->kind == ExprKind::Call) call(static_cast<CallExpr*>(e));
            else if (e->kind == ExprKind::Member) {
                auto* m = static_cast<MemberExpr*>(e);
                expr(m->object.get());
                if (m->optional) chainJumps->push_back(emitJump(OP_JUMP_IF_NULL_KEEP, m->loc));
                propOp(OP_GET_PROP, nameConst(m->name), m->nameLoc);
            } else {
                auto* ix = static_cast<IndexExpr*>(e);
                expr(ix->object.get());
                if (ix->optional) chainJumps->push_back(emitJump(OP_JUMP_IF_NULL_KEEP, ix->loc));
                std::vector<int>* saved = chainJumps;
                chainJumps = nullptr;
                expr(ix->index.get());
                chainJumps = saved;
                op(OP_GET_INDEX, ix->loc);
            }
            if (outer) {
                for (int j : jumps) patchJump(j);
                chainJumps = nullptr;
            }
            break;
        }
        case ExprKind::Lambda: emitClosure(static_cast<LambdaExpr*>(e)->fn.get(), nullptr, e->loc); break;
        case ExprKind::Range: {
            auto* r = static_cast<RangeExpr*>(e);
            expr(r->start.get());
            expr(r->end.get());
            op(OP_RANGE, e->loc); u8(r->inclusive ? 1 : 0);
            if (r->step) { expr(r->step.get()); op(OP_RANGE_STEP, e->loc); }
            break;
        }
        case ExprKind::Await: {
            expr(static_cast<AwaitExpr*>(e)->operand.get());
            op(OP_AWAIT, e->loc);
            break;
        }
        case ExprKind::Is: {
            auto* is = static_cast<IsExpr*>(e);
            expr(is->operand.get());
            op(OP_IS, e->loc); u16(nameConst(is->target->name));
            if (is->negate) op(OP_NOT);
            break;
        }
        case ExprKind::Match: matchExpr(static_cast<MatchExpr*>(e)); break;
        case ExprKind::Assign: assign(static_cast<AssignExpr*>(e), false); break;
        case ExprKind::IncDec: incDec(static_cast<IncDecExpr*>(e), false); break;
        case ExprKind::On: {
            auto* o = static_cast<OnExpr*>(e);
            expr(o->event.get());
            emitClosure(o->handler.get(), nullptr, o->loc);
            uint8_t flags = o->once ? 1 : 0;
            if (o->priority) { expr(o->priority.get()); flags |= 2; }
            op(OP_ON, o->loc); u8(flags);
            break;
        }
    }
}

} // namespace hz
