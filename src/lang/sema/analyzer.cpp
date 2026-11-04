#include "lang/sema/analyzer.h"
#include "lang/vm/vm.h"

#include <algorithm>
#include <set>

namespace hz {

// ---------------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------------

static TypeP mk(TK k) { auto t = std::make_shared<Type>(); t->kind = k; return t; }
TypeP tAny() { static TypeP t = mk(TK::Any); return t; }
TypeP tVoid() { static TypeP t = mk(TK::Void); return t; }
TypeP tNull() { static TypeP t = mk(TK::Null); return t; }
TypeP tBool() { static TypeP t = mk(TK::Bool); return t; }
TypeP tNum() { static TypeP t = mk(TK::Number); return t; }
TypeP tInt() { static TypeP t = mk(TK::Int); return t; }
TypeP tText() { static TypeP t = mk(TK::Text); return t; }
static TypeP tRange() { static TypeP t = mk(TK::Range); return t; }
static TypeP tSub() { static TypeP t = mk(TK::Subscription); return t; }
TypeP tList(TypeP e) { auto t = mk(TK::List); t->args.push_back(e ? e : tAny()); return t; }
TypeP tMap(TypeP k, TypeP v) { auto t = mk(TK::Map); t->args.push_back(k ? k : tAny()); t->args.push_back(v ? v : tAny()); return t; }
TypeP tSet(TypeP e) { auto t = mk(TK::Set); t->args.push_back(e ? e : tAny()); return t; }
TypeP tTask(TypeP r) { auto t = mk(TK::Task); t->args.push_back(r ? r : tAny()); return t; }
TypeP tInstance(ClassInfo* c) { auto t = mk(TK::Instance); t->cls = c; return t; }
static TypeP tClassRef(ClassInfo* c) { auto t = mk(TK::Class); t->cls = c; return t; }
static TypeP tEnumRef(EnumInfo* e) { auto t = mk(TK::Enum); t->en = e; return t; }
static TypeP tEnumVal(EnumInfo* e) { auto t = mk(TK::EnumValue); t->en = e; return t; }
static TypeP tModule(ModuleInfo* m) { auto t = mk(TK::Module); t->mod = m; return t; }
static TypeP tEvent(EventInfo* e) { auto t = mk(TK::Event); t->ev = e; return t; }
TypeP tFunc(std::shared_ptr<FuncSig> sig) { auto t = mk(TK::Func); t->sig = std::move(sig); return t; }
static TypeP nullableOf(TypeP t) { if (t->nullable || t->kind == TK::Any) return t; auto c = std::make_shared<Type>(*t); c->nullable = true; return c; }
static bool isNumeric(const TypeP& t) { return t->kind == TK::Number || t->kind == TK::Int; }
static bool isKnown(const TypeP& t) { return t && t->kind != TK::Any; }
static TypeP widen(TypeP t) { return (t && t->kind == TK::Int) ? tNum() : t; }

std::string sigStr(const FuncSig& s, const std::string& name) {
    std::string r = name + "(";
    for (size_t i = 0; i < s.params.size(); i++) {
        auto& p = s.params[i];
        if (i) r += ", ";
        if (p.rest) r += "...";
        r += p.name;
        if (p.type && p.type->kind != TK::Any) r += ": " + typeStr(p.type);
        if (p.optional && !p.rest) r += p.def.empty() ? "?" : " = " + p.def;
    }
    r += ")";
    if (s.ret && s.ret->kind != TK::Any) r += ": " + typeStr(s.isAsync ? tTask(s.ret) : s.ret);
    return r;
}

// Firma de un constructor: sin el tipo de retorno.
static std::string ctorStr(const FuncSig& s, const std::string& name) {
    FuncSig copy = s;
    copy.ret = nullptr;
    return sigStr(copy, name);
}

std::string typeStr(const TypeP& t) {
    if (!t) return "any";
    std::string s;
    switch (t->kind) {
        case TK::Any: return "any";
        case TK::Void: s = "void"; break;
        case TK::Null: return "null";
        case TK::Bool: s = "bool"; break;
        case TK::Number: s = "number"; break;
        case TK::Int: s = "int"; break;
        case TK::Text: s = "text"; break;
        case TK::List: s = t->args.empty() || t->args[0]->kind == TK::Any ? "list" : "list<" + typeStr(t->args[0]) + ">"; break;
        case TK::Set: s = t->args.empty() || t->args[0]->kind == TK::Any ? "set" : "set<" + typeStr(t->args[0]) + ">"; break;
        case TK::Map:
            s = (t->args.size() < 2 || (t->args[0]->kind == TK::Any && t->args[1]->kind == TK::Any)) ? "map"
                : "map<" + typeStr(t->args[0]) + ", " + typeStr(t->args[1]) + ">";
            break;
        case TK::Func: s = t->sig ? "function " + sigStr(*t->sig) : "function"; break;
        case TK::Instance: s = t->cls ? t->cls->name : "object"; break;
        case TK::Class: s = t->cls ? (t->cls->isInterface ? "interface " : "class ") + t->cls->name : "class"; break;
        case TK::Enum: s = t->en ? "enum " + t->en->name : "enum"; break;
        case TK::EnumValue: s = t->en ? t->en->name : "enum"; break;
        case TK::Module: s = t->mod ? "módulo " + t->mod->name : "módulo"; break;
        case TK::Event: s = t->ev ? "event " + t->ev->name : "event"; break;
        case TK::Task: s = t->args.empty() || t->args[0]->kind == TK::Any ? "Task" : "Task<" + typeStr(t->args[0]) + ">"; break;
        case TK::Range: s = "Range"; break;
        case TK::Subscription: s = "Subscription"; break;
    }
    if (t->nullable) s += "?";
    return s;
}

ClassInfo* World::findClass(const std::string& n) const {
    auto it = nativeClasses.find(n);
    if (it != nativeClasses.end()) return it->second.get();
    auto p = preludeClasses.find(n);
    if (p != preludeClasses.end()) return p->second;
    return nullptr;
}

static TypeP parseTypeImpl(VM& vm, std::string_view s, size_t& i);

TypeP parseTypeString(VM& vm, const std::string& s) {
    size_t i = 0;
    return parseTypeImpl(vm, s, i);
}

static TypeP namedType(VM& vm, const std::string& n, std::vector<TypeP> args) {
    if (n == "T" || n == "K" || n == "V") { auto t = mk(TK::Any); t->placeholder = true; return t; }
    if (n == "any" || n == "object") return tAny();
    if (n == "number" || n == "float" || n == "double") return tNum();
    if (n == "int") return tInt();
    if (n == "text" || n == "string" || n == "char") return tText();
    if (n == "bool" || n == "boolean") return tBool();
    if (n == "void") return tVoid();
    if (n == "null") return tNull();
    if (n == "list" || n == "array") return tList(args.empty() ? nullptr : args[0]);
    if (n == "set") return tSet(args.empty() ? nullptr : args[0]);
    if (n == "map" || n == "dict") return tMap(args.size() > 0 ? args[0] : nullptr, args.size() > 1 ? args[1] : nullptr);
    if (n == "Task" || n == "Promise" || n == "Future") return tTask(args.empty() ? nullptr : args[0]);
    if (n == "function" || n == "Function") return tFunc(nullptr);
    if (n == "Range") return tRange();
    if (n == "Subscription") return tSub();
    if (n == "Event" || n == "event") { auto t = mk(TK::Event); return t; }
    World& w = worldOf(vm);
    if (auto* c = w.findClass(n)) return tInstance(c);
    return tAny();
}

static TypeP parseTypeImpl(VM& vm, std::string_view s, size_t& i) {
    auto skip = [&] { while (i < s.size() && s[i] == ' ') i++; };
    skip();
    if (i < s.size() && s[i] == '(') {
        // tipo función (a) => b : se trata como function
        int depth = 0;
        while (i < s.size()) { if (s[i] == '(') depth++; if (s[i] == ')') { depth--; if (depth == 0) { i++; break; } } i++; }
        skip();
        if (i + 1 < s.size() && s[i] == '=' && s[i + 1] == '>') { i += 2; parseTypeImpl(vm, s, i); }
        return tFunc(nullptr);
    }
    std::string name;
    while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '.')) name += s[i++];
    std::vector<TypeP> args;
    skip();
    if (i < s.size() && s[i] == '<') {
        i++;
        for (;;) {
            args.push_back(parseTypeImpl(vm, s, i));
            skip();
            if (i < s.size() && s[i] == ',') { i++; continue; }
            if (i < s.size() && s[i] == '>') i++;
            break;
        }
    }
    TypeP t = namedType(vm, name, args);
    skip();
    if (i < s.size() && s[i] == '?') { i++; t = nullableOf(t); }
    skip();
    if (i < s.size() && s[i] == '|') { i++; parseTypeImpl(vm, s, i); return tAny(); }
    return t;
}

static bool assignable(const TypeP& to, const TypeP& from) {
    if (!to || !from) return true;
    if (to->kind == TK::Any || from->kind == TK::Any) return true;
    if (from->kind == TK::Null) return to->nullable || to->kind == TK::Null;
    if (from->kind == TK::Void) return to->kind == TK::Void;
    switch (to->kind) {
        case TK::Number: case TK::Int: return isNumeric(from);
        case TK::Text: return from->kind == TK::Text;
        case TK::Bool: return from->kind == TK::Bool;
        case TK::List: return from->kind == TK::List;
        case TK::Map: return from->kind == TK::Map;
        case TK::Set: return from->kind == TK::Set;
        case TK::Range: return from->kind == TK::Range;
        case TK::Func: return from->kind == TK::Func || from->kind == TK::Class;
        case TK::Task: return from->kind == TK::Task;
        case TK::Event: return from->kind == TK::Event;
        case TK::Subscription: return from->kind == TK::Subscription;
        case TK::EnumValue: return from->kind == TK::EnumValue && (!to->en || !from->en || to->en == from->en);
        case TK::Instance:
            if (!to->cls) return true;
            if (to->cls->name == "Color" && from->kind == TK::Text) return true; // "red", "#ff0000"
            if (from->kind == TK::Instance && from->cls) return from->cls->isSubclassOf(to->cls);
            return false;
        case TK::Class: case TK::Enum: case TK::Module: return from->kind == to->kind;
        case TK::Void: return false;
        default: return true;
    }
}

// ---------------------------------------------------------------------------
// Mundo (API nativa)
// ---------------------------------------------------------------------------

static std::unordered_map<VM*, std::unique_ptr<World>>& worldTable() {
    static std::unordered_map<VM*, std::unique_ptr<World>> worlds;
    return worlds;
}

World& worldOf(VM& vm) {
    auto& w = worldTable()[&vm];
    if (!w) w = std::make_unique<World>();
    return *w;
}

void releaseWorld(VM& vm) { worldTable().erase(&vm); }

static std::shared_ptr<FuncSig> sigFromApi(VM& vm, const ApiEntry& e) {
    auto s = std::make_shared<FuncSig>();
    for (auto& p : e.params) {
        SigParam sp;
        sp.name = p.name;
        sp.type = parseTypeString(vm, p.type);
        sp.optional = p.optional;
        sp.rest = p.rest;
        sp.def = p.def;
        s->params.push_back(sp);
    }
    s->ret = parseTypeString(vm, e.returns);
    s->acceptsNamedOptions = true;
    return s;
}

void buildWorld(VM& vm) {
    World& w = worldOf(vm);
    auto newSym = [&](const std::string& name, SymKind k, const std::string& detail, const std::string& doc, const std::string& container) {
        auto s = std::make_unique<Symbol>();
        s->name = name; s->kind = k; s->detail = detail; s->doc = doc; s->container = container; s->builtin = true;
        Symbol* r = s.get();
        w.symbols.push_back(std::move(s));
        return r;
    };
    // 1) Clases y módulos (para que los tipos se puedan resolver)
    for (auto& e : vm.catalog) {
        if (e.kind == ApiKind::Class && !w.nativeClasses.count(e.name)) {
            auto c = std::make_unique<ClassInfo>();
            c->name = e.name; c->isNative = true; c->doc = e.doc;
            c->sym = newSym(e.name, SymKind::Class, "class " + e.name, e.doc, "");
            w.nativeClasses[e.name] = std::move(c);
        } else if (e.kind == ApiKind::Module && !w.nativeModules.count(e.name)) {
            auto m = std::make_unique<ModuleInfo>();
            m->name = e.name; m->isNative = true; m->doc = e.doc;
            w.nativeModules[e.name] = std::move(m);
        }
    }
    // 2) Miembros
    for (auto& e : vm.catalog) {
        ClassInfo* cls = nullptr;
        ModuleInfo* mod = nullptr;
        if (!e.owner.empty()) {
            auto ci = w.nativeClasses.find(e.owner);
            if (ci != w.nativeClasses.end()) cls = ci->second.get();
            auto mi = w.nativeModules.find(e.owner);
            if (mi != w.nativeModules.end()) mod = mi->second.get();
        }
        MemberInfo m;
        m.name = e.name;
        m.isStatic = e.isStatic;
        m.readonly = e.readonly;
        std::string detail;
        SymKind sk = SymKind::Function;
        switch (e.kind) {
            case ApiKind::Function: case ApiKind::Method: {
                auto sig = sigFromApi(vm, e);
                m.kind = MemberInfo::Method;
                m.type = tFunc(sig);
                detail = sigStr(*sig, e.name);
                sk = cls ? SymKind::Method : SymKind::Function;
                break;
            }
            case ApiKind::Property: case ApiKind::Field:
                m.kind = MemberInfo::Property;
                m.type = parseTypeString(vm, e.type);
                detail = e.name + ": " + e.type + (e.readonly ? " (solo lectura)" : "");
                sk = SymKind::Property;
                break;
            case ApiKind::Constant:
                m.kind = MemberInfo::Constant;
                m.readonly = true;
                m.type = parseTypeString(vm, e.type);
                detail = "const " + e.name + ": " + e.type;
                sk = SymKind::Constant;
                break;
            case ApiKind::Event: {
                w.events.push_back(std::make_unique<EventInfo>());
                auto ev = w.events.back().get();
                ev->name = e.name;
                for (auto& p : e.params) ev->params.push_back({p.name, parseTypeString(vm, p.type), false, false, ""});
                m.kind = MemberInfo::EventM;
                m.type = tEvent(ev);
                std::string ps;
                for (size_t i = 0; i < e.params.size(); i++) ps += (i ? ", " : "") + e.params[i].name + ": " + e.params[i].type;
                detail = "event " + e.name + "(" + ps + ")";
                sk = SymKind::Event;
                break;
            }
            case ApiKind::Class: {
                if (e.owner.empty()) {
                    auto* c = w.nativeClasses[e.name].get();
                    if (!e.params.empty() || e.minArgs > 0 || e.maxArgs != 0) {
                        auto sig = sigFromApi(vm, e);
                        c->ctor = sig;
                    }
                    w.builtinTypes[e.name] = tClassRef(c);
                    w.builtinSyms[e.name] = c->sym;
                    if (c->ctor) c->sym->detail = "class " + e.name + " " + ctorStr(*c->ctor, e.name);
                }
                continue;
            }
            case ApiKind::Module: {
                auto* mi = w.nativeModules[e.name].get();
                if (e.owner.empty()) {
                    w.builtinTypes[e.name] = tModule(mi);
                    w.builtinSyms[e.name] = newSym(e.name, SymKind::Module, "módulo " + e.name, e.doc, "");
                } else if (mod) {
                    MemberInfo sm;
                    sm.name = e.name; sm.kind = MemberInfo::Submodule; sm.type = tModule(mi); sm.readonly = true;
                    sm.sym = newSym(e.name, SymKind::Module, "módulo " + e.name, e.doc, e.owner);
                    mod->members[e.name] = sm;
                }
                continue;
            }
        }
        m.sym = newSym(e.name, sk, detail, e.doc, e.owner);
        m.sym->type = m.type;
        m.sym->isStatic = e.isStatic;
        if (cls) cls->members[e.name] = m;
        else if (mod) mod->members[e.name] = m;
        else {
            w.builtinTypes[e.name] = m.type;
            w.builtinSyms[e.name] = m.sym;
        }
    }
    // 3) Bibliotecas oficiales (HyperAudio...) como alias de módulos
    for (auto& [lib, members] : vm.libraryMembers) {
        if (members.size() == 1 && w.nativeModules.count(members[0])) {
            w.builtinTypes["@lib:" + lib] = tModule(w.nativeModules[members[0]].get());
        } else {
            auto m = std::make_unique<ModuleInfo>();
            m->name = lib; m->isNative = true;
            for (auto& n : members) {
                MemberInfo mi;
                mi.name = n; mi.readonly = true;
                auto bt = w.builtinTypes.find(n);
                mi.type = bt != w.builtinTypes.end() ? bt->second : tAny();
                mi.kind = mi.type->kind == TK::Module ? MemberInfo::Submodule : MemberInfo::Property;
                auto bs = w.builtinSyms.find(n);
                mi.sym = bs != w.builtinSyms.end() ? bs->second : nullptr;
                m->members[n] = mi;
            }
            w.builtinTypes["@lib:" + lib] = tModule(m.get());
            w.nativeModules["@lib:" + lib] = std::move(m);
        }
    }
}

// ---------------------------------------------------------------------------
// Unidad
// ---------------------------------------------------------------------------

Symbol* ModuleUnit::newSymbol(const std::string& n, SymKind k, SrcLoc loc) {
    auto s = std::make_unique<Symbol>();
    s->name = n; s->kind = k; s->loc = loc; s->file = path;
    Symbol* r = s.get();
    symbols.push_back(std::move(s));
    return r;
}

Analyzer::Analyzer(VM& v, ModuleUnit& u) : vm(v), unit(u), world(worldOf(v)), diags(u.diags) {}

void Analyzer::addRef(SrcLoc loc, Symbol* sym, bool isDecl) {
    if (sym && loc.line > 0) unit.refs.push_back({loc, sym, isDecl});
}

// ---------------------------------------------------------------------------
// Ámbitos y resolución
// ---------------------------------------------------------------------------

void Analyzer::beginScope(int startOffset) {
    fn->scopeDepth++;
    unit.scopes.push_back({});
    unit.scopes.back().start = startOffset;
    scopeStack.push_back(nullptr); // el puntero se resuelve por índice al cerrar
    scopeStack.back() = reinterpret_cast<ScopeRecord*>(unit.scopes.size() - 1);
}

void Analyzer::reportUnused(LocalVar& lv) {
    if (!lv.sym || lv.hidden || lv.isParam || lv.sym->used) return;
    if (lv.name.empty() || lv.name[0] == '_' || lv.name == "this") return;
    diags.hintUnused(lv.sym->loc, "La variable '" + lv.name + "' no se usa");
}

void Analyzer::endScope(BlockStmt* blk, int endOffset) {
    size_t idx = reinterpret_cast<size_t>(scopeStack.back());
    scopeStack.pop_back();
    ScopeRecord& rec = unit.scopes[idx];
    rec.end = endOffset;
    std::vector<LocalInfo> infos;
    while (!fn->locals.empty() && fn->locals.back().depth == fn->scopeDepth) {
        LocalVar& lv = fn->locals.back();
        int slot = (int)fn->locals.size() - 1;
        infos.push_back({slot, lv.captured});
        if (lv.sym) { lv.sym->scopeEnd = endOffset; rec.symbols.push_back(lv.sym); }
        reportUnused(lv);
        fn->locals.pop_back();
    }
    std::reverse(infos.begin(), infos.end());
    if (blk) blk->locals = infos;
    fn->scopeDepth--;
}

int Analyzer::declareLocal(const std::string& name, SrcLoc loc, SymKind kind, TypeP type, DeclKind dk, bool hidden) {
    if (!hidden) {
        for (int i = (int)fn->locals.size() - 1; i >= 0; i--) {
            auto& l = fn->locals[i];
            if (l.depth < fn->scopeDepth) break;
            if (l.name == name && !l.hidden) {
                diags.error(loc, "Ya existe una variable llamada '" + name + "' en este bloque", "usa otro nombre o asigna sin 'var'", "E0201");
                break;
            }
        }
    }
    LocalVar lv;
    lv.name = hidden ? " " + name : name;
    lv.depth = fn->scopeDepth;
    lv.declKind = dk;
    lv.type = type ? type : tAny();
    lv.hidden = hidden;
    if (!hidden) {
        lv.sym = unit.newSymbol(name, kind, loc);
        lv.sym->type = lv.type;
        lv.sym->detail = (dk == DeclKind::Const ? "const " : dk == DeclKind::Let ? "let " : kind == SymKind::Parameter ? "" : "var ") + name +
                         (lv.type->kind != TK::Any ? ": " + typeStr(lv.type) : "");
        lv.sym->scopeStart = loc.offset;
        addRef(loc, lv.sym, true);
    }
    fn->locals.push_back(lv);
    int slot = (int)fn->locals.size() - 1;
    fn->maxSlots = std::max(fn->maxSlots, slot + 1);
    return slot;
}

int Analyzer::resolveLocal(FnCtx* f, const std::string& name) {
    for (int i = (int)f->locals.size() - 1; i >= 0; i--)
        if (f->locals[i].name == name) return i;
    return -1;
}

int Analyzer::addUpvalue(FnCtx* f, bool isLocal, int index, const std::string& name) {
    auto& ups = f->decl->upvalues;
    for (size_t i = 0; i < ups.size(); i++)
        if (ups[i].isLocal == isLocal && ups[i].index == index) return (int)i;
    ups.push_back({isLocal, index, name});
    return (int)ups.size() - 1;
}

int Analyzer::resolveUpvalue(FnCtx* f, const std::string& name) {
    if (!f->enclosing || f->isScript) return -1;
    int local = resolveLocal(f->enclosing, name);
    if (local >= 0) {
        f->enclosing->locals[local].captured = true;
        return addUpvalue(f, true, local, name);
    }
    int up = resolveUpvalue(f->enclosing, name);
    if (up >= 0) return addUpvalue(f, false, up, name);
    return -1;
}

bool Analyzer::resolveThis(Resolution& res) {
    int l = resolveLocal(fn, "this");
    if (l >= 0) { res.kind = VarKind::Local; res.index = l; return true; }
    int u = resolveUpvalue(fn, "this");
    if (u >= 0) { res.kind = VarKind::Upvalue; res.index = u; return true; }
    return false;
}

bool Analyzer::resolveName(const std::string& name, SrcLoc loc, Resolution& res, Symbol*& sym, TypeP& type, bool forWrite) {
    (void)forWrite;
    // 1) local
    int l = resolveLocal(fn, name);
    if (l >= 0) {
        auto& lv = fn->locals[l];
        res.kind = VarKind::Local; res.index = l;
        sym = lv.sym; type = lv.type;
        return true;
    }
    // 2) upvalue
    int u = resolveUpvalue(fn, name);
    if (u >= 0) {
        res.kind = VarKind::Upvalue; res.index = u;
        // Buscar el símbolo en los contextos exteriores
        for (FnCtx* f = fn->enclosing; f; f = f->enclosing) {
            int k = resolveLocal(f, name);
            if (k >= 0) { sym = f->locals[k].sym; type = f->locals[k].type; break; }
        }
        return true;
    }
    // 3) miembro implícito de la clase (this.x / Clase.x)
    if (currentClass) {
        if (const MemberInfo* m = currentClass->find(name)) {
            sym = m->sym; type = m->type;
            if (m->kind == MemberInfo::Getter || m->kind == MemberInfo::Setter || m->kind == MemberInfo::Field) type = m->type;
            if (m->isStatic) {
                // Cargar la clase dueña por su nombre global
                const ClassInfo* owner = currentClass;
                for (auto* c = currentClass; c; c = c->super) if (c->members.count(name)) { owner = c; break; }
                Resolution cr; Symbol* cs = nullptr; TypeP ct;
                auto git = unit.globalIndex.find(owner->name);
                if (git != unit.globalIndex.end()) { cr.kind = VarKind::Global; cr.index = git->second; }
                else {
                    auto b = vm.builtinIndex.find(owner->name);
                    if (b != vm.builtinIndex.end()) { cr.kind = VarKind::Builtin; cr.index = b->second; }
                }
                (void)cs; (void)ct;
                if (cr.kind != VarKind::Unresolved) {
                    res.kind = VarKind::StaticMember;
                    res.classKind = cr.kind; res.classIndex = cr.index;
                    return true;
                }
            } else {
                Resolution tr;
                if (resolveThis(tr)) {
                    res.kind = VarKind::ThisMember;
                    res.thisKind = tr.kind; res.thisIndex = tr.index;
                    return true;
                }
                diags.error(loc, "No se puede usar el miembro de instancia '" + name + "' en un método estático",
                            "los métodos 'static' no tienen 'this'", "E0202");
                res.kind = VarKind::Unresolved;
                return true;
            }
        }
    }
    // 4) global del módulo
    auto g = unit.globalIndex.find(name);
    if (g != unit.globalIndex.end()) {
        res.kind = VarKind::Global; res.index = g->second;
        auto gs = unit.globalSyms.find(name);
        sym = gs != unit.globalSyms.end() ? gs->second : nullptr;
        auto gt = unit.globalTypes.find(name);
        type = gt != unit.globalTypes.end() ? gt->second : tAny();
        // ¿Uso en el nivel superior antes de la declaración?
        if (fn->isScript && sym && (sym->kind == SymKind::Variable || sym->kind == SymKind::Constant) &&
            sym->loc.offset > loc.offset && !forWrite) {
            diags.error(loc, "La variable '" + name + "' se usa antes de declararla (línea " + std::to_string(sym->loc.line) + ")",
                        "mueve la declaración 'var " + name + "' más arriba", "E0203");
        }
        return true;
    }
    // 5) integrado
    auto b = vm.builtinIndex.find(name);
    if (b != vm.builtinIndex.end()) {
        res.kind = VarKind::Builtin; res.index = b->second;
        auto bs = world.builtinSyms.find(name);
        sym = bs != world.builtinSyms.end() ? bs->second : nullptr;
        auto bt = world.builtinTypes.find(name);
        type = bt != world.builtinTypes.end() ? bt->second : tAny();
        return true;
    }
    return false;
}

std::vector<std::string> Analyzer::visibleNames() {
    std::vector<std::string> names;
    for (FnCtx* f = fn; f; f = f->enclosing)
        for (auto& l : f->locals) if (!l.hidden && !l.name.empty()) names.push_back(l.name);
    if (currentClass)
        for (auto* c = currentClass; c; c = c->super)
            for (auto& [n, m] : c->members) names.push_back(n);
    for (auto& [n, i] : unit.globalIndex) names.push_back(n);
    for (auto& [n, i] : vm.builtinIndex) names.push_back(n);
    return names;
}

// ---------------------------------------------------------------------------
// Declaraciones globales
// ---------------------------------------------------------------------------

int Analyzer::declareGlobal(const std::string& name, SrcLoc loc, SymKind kind, TypeP type, const std::string& doc) {
    if (unit.globalIndex.count(name)) {
        auto* prev = unit.globalSyms[name];
        diags.error(loc, "'" + name + "' ya está declarado" + (prev ? " en la línea " + std::to_string(prev->loc.line) : ""),
                    "cada nombre global debe ser único en el archivo", "E0204");
        return unit.globalIndex[name];
    }
    int idx = -1;
    if (unit.repl) {
        // consola interactiva: un global de una entrada anterior conserva su índice
        auto& prev = unit.repl->globals;
        auto it = std::find(prev.begin(), prev.end(), name);
        if (it != prev.end()) idx = (int)(it - prev.begin());
    }
    if (idx < 0) {
        idx = (int)unit.globalNames.size();
        unit.globalNames.push_back(name);
    }
    unit.globalIndex[name] = idx;
    Symbol* s = unit.newSymbol(name, kind, loc);
    s->type = type;
    s->doc = doc;
    unit.globalSyms[name] = s;
    unit.globalTypes[name] = type ? type : tAny();
    addRef(loc, s, true);
    return idx;
}

TypeP Analyzer::resolveTypeRef(TypeRef* t) {
    if (!t) return tAny();
    std::vector<TypeP> args;
    for (auto& a : t->args) args.push_back(resolveTypeRef(a.get()));
    TypeP r;
    const std::string& n = t->name;
    static const std::set<std::string> prim = {"any", "number", "float", "double", "int", "text", "string", "char", "bool",
                                               "boolean", "void", "null", "list", "array", "set", "map", "dict", "Task",
                                               "Promise", "Future", "function", "Function", "Range", "Subscription", "Event", "object"};
    if (prim.count(n)) r = namedType(vm, n, args);
    else {
        // Clase/enum/interfaz del módulo, del preludio o nativa
        auto g = unit.globalTypes.find(n);
        if (g != unit.globalTypes.end() && (g->second->kind == TK::Class || g->second->kind == TK::Enum)) {
            r = g->second->kind == TK::Class ? tInstance(g->second->cls) : tEnumVal(g->second->en);
            if (auto s = unit.globalSyms[n]) { s->used = true; addRef(t->loc, s); }
        } else if (auto* c = world.findClass(n)) {
            r = tInstance(c);
            if (c->sym) addRef(t->loc, c->sym);
        } else if (world.preludeEnums.count(n)) {
            r = tEnumVal(world.preludeEnums[n]);
        } else if (n.find('.') != std::string::npos) {
            // Módulo.Tipo
            std::string modName = n.substr(0, n.find('.'));
            std::string rest = n.substr(n.find('.') + 1);
            auto g2 = unit.globalTypes.find(modName);
            if (g2 != unit.globalTypes.end() && g2->second->kind == TK::Module && g2->second->mod) {
                auto it = g2->second->mod->members.find(rest);
                if (it != g2->second->mod->members.end() && it->second.type && it->second.type->kind == TK::Class)
                    r = tInstance(it->second.type->cls);
                else if (it != g2->second->mod->members.end() && it->second.type && it->second.type->kind == TK::Enum)
                    r = tEnumVal(it->second.type->en);
            }
            if (!r) { diags.error(t->loc, "Tipo desconocido '" + n + "'", "", "E0301"); r = tAny(); }
        } else {
            std::vector<std::string> cands(prim.begin(), prim.end());
            for (auto& [k, v] : unit.globalTypes) if (v->kind == TK::Class || v->kind == TK::Enum) cands.push_back(k);
            for (auto& [k, v] : world.nativeClasses) cands.push_back(k);
            for (auto& [k, v] : world.preludeClasses) cands.push_back(k);
            std::string sug = closestMatch(n, cands);
            diags.error(t->loc, "Tipo desconocido '" + n + "'", sug.empty() ? "" : "¿quisiste decir '" + sug + "'?", "E0301");
            r = tAny();
        }
    }
    if (t->nullable) r = nullableOf(r);
    return r;
}

std::shared_ptr<FuncSig> Analyzer::sigOf(FuncDecl* f) {
    auto s = std::make_shared<FuncSig>();
    for (auto& p : f->params) {
        SigParam sp;
        sp.name = p.name;
        sp.type = p.type ? resolveTypeRef(p.type.get()) : tAny();
        sp.optional = p.def != nullptr;
        sp.rest = p.rest;
        if (p.def) {
            // Texto corto del valor por defecto para la firma
            switch (p.def->kind) {
                case ExprKind::Number: sp.def = formatNumber(static_cast<NumberExpr*>(p.def.get())->value); break;
                case ExprKind::String: { auto* se = static_cast<StringExpr*>(p.def.get()); sp.def = se->isPlain() ? escapeString(se->parts[0].text) : "\"...\""; break; }
                case ExprKind::Bool: sp.def = static_cast<BoolExpr*>(p.def.get())->value ? "true" : "false"; break;
                case ExprKind::Null: sp.def = "null"; break;
                default: sp.def = "..."; break;
            }
        }
        s->params.push_back(sp);
    }
    s->ret = f->ret ? resolveTypeRef(f->ret.get()) : (f->kind == FuncKind::Constructor ? tVoid() : tAny());
    s->isAsync = f->isAsync;
    return s;
}

void Analyzer::buildClass(ClassStmt* c) {
    auto ci = std::make_unique<ClassInfo>();
    ci->name = c->name;
    ci->module = unit.name;
    ci->isStruct = c->isStruct;
    ci->doc = c->doc;
    c->info = ci.get();
    int idx = declareGlobal(c->name, c->nameLoc, c->isStruct ? SymKind::Struct : SymKind::Class, tClassRef(ci.get()), c->doc);
    c->res.kind = VarKind::Global;
    c->res.index = idx;
    c->sym = unit.globalSyms[c->name];
    ci->sym = c->sym;
    c->sym->detail = (c->isStruct ? "struct " : "class ") + c->name;
    unit.classes.push_back(std::move(ci));
}

void Analyzer::linkClass(ClassStmt* c) {
    ClassInfo* ci = c->info;
    if (!c->superName.empty()) {
        TypeP st;
        auto g = unit.globalTypes.find(c->superName);
        if (g != unit.globalTypes.end()) st = g->second;
        else if (auto* pc = world.findClass(c->superName)) st = tClassRef(pc);
        else if (c->superName.find('.') != std::string::npos) {
            std::string m = c->superName.substr(0, c->superName.find('.'));
            std::string rest = c->superName.substr(c->superName.find('.') + 1);
            auto gm = unit.globalTypes.find(m);
            if (gm != unit.globalTypes.end() && gm->second->kind == TK::Module && gm->second->mod) {
                auto it = gm->second->mod->members.find(rest);
                if (it != gm->second->mod->members.end()) st = it->second.type;
            }
        }
        if (!st || st->kind != TK::Class || !st->cls) {
            diags.error(c->superLoc, "La clase base '" + c->superName + "' no existe", "", "E0501");
        } else if (st->cls->isInterface) {
            diags.error(c->superLoc, "'" + c->superName + "' es una interfaz: usa 'implements', no 'extends'", "", "E0502");
        } else if (st->cls->isNative && st->cls->name != "Error") { // de Error sí se puede heredar (errores propios)
            diags.error(c->superLoc, "No se puede heredar de la clase integrada '" + c->superName + "'", "", "E0503");
        } else {
            for (auto* k = st->cls; k; k = k->super)
                if (k == ci) { diags.error(c->superLoc, "Herencia circular: '" + c->name + "' no puede heredar de sí misma", "", "E0504"); st = nullptr; break; }
            if (st) {
                ci->super = st->cls;
                if (st->cls->sym) { st->cls->sym->used = true; addRef(c->superLoc, st->cls->sym); }
            }
        }
    }
    for (auto& [iname, iloc] : c->interfaces) {
        TypeP it;
        auto g = unit.globalTypes.find(iname);
        if (g != unit.globalTypes.end()) it = g->second;
        else if (auto* pc = world.findClass(iname)) it = tClassRef(pc);
        if (!it || it->kind != TK::Class || !it->cls || !it->cls->isInterface)
            diags.error(iloc, "'" + iname + "' no es una interfaz conocida", "", "E0505");
        else {
            ci->interfaces.push_back(it->cls);
            if (it->cls->sym) { it->cls->sym->used = true; addRef(iloc, it->cls->sym); }
        }
    }
}

void Analyzer::declareGlobals() {
    auto& body = unit.program->body;
    // Imports primero (pueden aportar clases base)
    for (auto& sp : body) {
        if (sp->kind != StmtKind::Import) continue;
        auto* s = static_cast<ImportStmt*>(sp.get());
        TypeP modType;
        Symbol* modSym = nullptr;
        std::string err;
        if (!s->isFile && vm.libraries.count(s->path)) {
            auto bt = world.builtinTypes.find("@lib:" + s->path);
            modType = bt != world.builtinTypes.end() ? bt->second : tAny();
        } else {
            ModuleUnit* imp = vm.resolveImport(s->path, s->isFile, unit.path, err);
            if (!imp) {
                std::string hint;
                if (!s->isFile) {
                    auto libs = vm.libraryNames();
                    std::string sug = closestMatch(s->path, libs);
                    if (!sug.empty()) hint = "¿quisiste decir '" + sug + "'?";
                }
                diags.error(s->loc, err.empty() ? "No se encontró el módulo '" + s->path + "'" : err, hint, "E0206");
                modType = tAny();
            } else {
                s->resolvedPath = imp->path;
                unit.imports.push_back(imp);
                modType = tModule(imp->info.get());
            }
        }
        if (s->names.empty()) {
            std::string bind = s->alias;
            if (bind.empty()) {
                bind = s->path;
                if (s->isFile) bind = pathStem(s->path);
                else if (bind.find('.') != std::string::npos) bind = bind.substr(bind.rfind('.') + 1);
            }
            int idx = declareGlobal(bind, s->alias.empty() ? s->loc : s->aliasLoc, SymKind::Module, modType);
            s->res.kind = VarKind::Global; s->res.index = idx;
            s->sym = unit.globalSyms[bind];
            s->sym->detail = "import " + s->path + (s->alias.empty() ? "" : " as " + s->alias);
            if (modType->mod) s->sym->doc = modType->mod->doc;
            modSym = s->sym;
            modSym->used = true; // los imports sin usar no son un error
        } else {
            for (size_t i = 0; i < s->names.size(); i++) {
                TypeP mt = tAny();
                Symbol* target = nullptr;
                if (modType->mod) {
                    auto it = modType->mod->members.find(s->names[i].first);
                    if (it == modType->mod->members.end())
                        diags.error(s->names[i].second, "El módulo '" + s->path + "' no exporta '" + s->names[i].first + "'", "", "E0207");
                    else { mt = it->second.type; target = it->second.sym; }
                }
                int idx = declareGlobal(s->nameAliases[i], s->names[i].second, SymKind::Variable, mt);
                s->nameRes[i].kind = VarKind::Global; s->nameRes[i].index = idx;
                Symbol* gs = unit.globalSyms[s->nameAliases[i]];
                gs->used = true;
                if (target) { gs->detail = target->detail; gs->doc = target->doc; gs->kind = target->kind; addRef(s->names[i].second, target); }
                s->nameSyms[i] = gs;
            }
        }
    }
    // Clases, interfaces, enums, eventos (nombres)
    for (auto& sp : body) {
        switch (sp->kind) {
            case StmtKind::Class: buildClass(static_cast<ClassStmt*>(sp.get())); break;
            case StmtKind::Interface: {
                auto* s = static_cast<InterfaceStmt*>(sp.get());
                auto ci = std::make_unique<ClassInfo>();
                ci->name = s->name; ci->isInterface = true; ci->doc = s->doc; ci->module = unit.name;
                int idx = declareGlobal(s->name, s->nameLoc, SymKind::Interface, tClassRef(ci.get()), s->doc);
                s->res.kind = VarKind::Global; s->res.index = idx;
                s->sym = unit.globalSyms[s->name];
                s->sym->detail = "interface " + s->name;
                ci->sym = s->sym;
                unit.classes.push_back(std::move(ci));
                break;
            }
            case StmtKind::Enum: {
                auto* s = static_cast<EnumStmt*>(sp.get());
                auto ei = std::make_unique<EnumInfo>();
                ei->name = s->name;
                for (auto& m : s->members) ei->members.push_back(m.name);
                int idx = declareGlobal(s->name, s->nameLoc, SymKind::Enum, tEnumRef(ei.get()), s->doc);
                s->res.kind = VarKind::Global; s->res.index = idx;
                s->sym = unit.globalSyms[s->name];
                std::string list;
                for (size_t i = 0; i < s->members.size(); i++) list += (i ? ", " : "") + s->members[i].name;
                s->sym->detail = "enum " + s->name + " { " + list + " }";
                ei->sym = s->sym;
                std::set<std::string> seen;
                for (auto& m : s->members) {
                    if (!seen.insert(m.name).second) diags.error(m.loc, "Valor repetido '" + m.name + "' en el enum", "", "E0208");
                    m.sym = unit.newSymbol(m.name, SymKind::EnumMember, m.loc);
                    m.sym->container = s->name;
                    m.sym->detail = s->name + "." + m.name;
                    m.sym->type = tEnumVal(ei.get());
                    ei->memberSyms[m.name] = m.sym;
                    addRef(m.loc, m.sym, true);
                }
                unit.enums.push_back(std::move(ei));
                break;
            }
            default: break;
        }
    }
    // Enlazar herencia de clases y construir interfaces
    for (auto& sp : body) {
        if (sp->kind == StmtKind::Interface) {
            auto* s = static_cast<InterfaceStmt*>(sp.get());
            ClassInfo* ci = unit.globalTypes[s->name]->cls;
            for (auto& m : s->methods) {
                MemberInfo mi;
                mi.name = m.name; mi.kind = MemberInfo::Method;
                auto sig = std::make_shared<FuncSig>();
                for (auto& p : m.params) sig->params.push_back({p.name, p.type ? resolveTypeRef(p.type.get()) : tAny(), p.def != nullptr, p.rest, ""});
                sig->ret = m.ret ? resolveTypeRef(m.ret.get()) : tAny();
                mi.type = tFunc(sig);
                mi.sym = unit.newSymbol(m.name, SymKind::Method, m.loc);
                mi.sym->detail = sigStr(*sig, m.name);
                mi.sym->container = s->name;
                mi.sym->type = mi.type;
                ci->members[m.name] = mi;
            }
            for (auto& [en, el] : s->extends) {
                auto g = unit.globalTypes.find(en);
                if (g != unit.globalTypes.end() && g->second->kind == TK::Class && g->second->cls && g->second->cls->isInterface)
                    ci->interfaces.push_back(g->second->cls);
                else diags.error(el, "'" + en + "' no es una interfaz conocida", "", "E0505");
            }
        }
    }
    for (auto& sp : body) if (sp->kind == StmtKind::Class) linkClass(static_cast<ClassStmt*>(sp.get()));
    // Eventos
    for (auto& sp : body) {
        if (sp->kind != StmtKind::Event) continue;
        auto* s = static_cast<EventStmt*>(sp.get());
        auto ei = std::make_unique<EventInfo>();
        ei->name = s->name;
        std::string ps;
        for (auto& p : s->params) {
            ei->params.push_back({p.name, p.type ? resolveTypeRef(p.type.get()) : tAny(), false, false, ""});
            ps += (ps.empty() ? "" : ", ") + p.name + (p.type ? ": " + p.type->toString() : "");
        }
        int idx = declareGlobal(s->name, s->nameLoc, SymKind::Event, tEvent(ei.get()), s->doc);
        s->res.kind = VarKind::Global; s->res.index = idx;
        s->sym = unit.globalSyms[s->name];
        s->sym->detail = "event " + s->name + "(" + ps + ")";
        ei->sym = s->sym;
        unit.events.push_back(std::move(ei));
    }
    // Miembros de clases
    for (auto& sp : body) {
        if (sp->kind != StmtKind::Class) continue;
        auto* c = static_cast<ClassStmt*>(sp.get());
        ClassInfo* ci = c->info;
        for (auto& f : c->fields) {
            if (ci->members.count(f.name)) { diags.error(f.loc, "El miembro '" + f.name + "' está repetido", "", "E0506"); continue; }
            MemberInfo mi;
            mi.name = f.name; mi.kind = MemberInfo::Field; mi.isStatic = f.isStatic; mi.vis = f.vis; mi.declKind = f.declKind;
            mi.readonly = f.declKind == DeclKind::Const;
            mi.type = f.type ? resolveTypeRef(f.type.get()) : tAny();
            if (!f.type && f.init) {
                switch (f.init->kind) {
                    case ExprKind::Number: mi.type = tNum(); break;
                    case ExprKind::String: mi.type = tText(); break;
                    case ExprKind::Bool: mi.type = tBool(); break;
                    case ExprKind::List: mi.type = tList(); break;
                    case ExprKind::Map: mi.type = tMap(); break;
                    default: break;
                }
            }
            mi.sym = unit.newSymbol(f.name, SymKind::Field, f.loc);
            mi.sym->container = c->name;
            mi.sym->type = mi.type;
            mi.sym->doc = f.doc;
            mi.sym->isStatic = f.isStatic;
            mi.sym->vis = f.vis;
            mi.sym->detail = std::string(f.isStatic ? "static " : "") + (f.declKind == DeclKind::Const ? "const " : f.declKind == DeclKind::Let ? "let " : "var ") +
                             c->name + "." + f.name + (mi.type->kind != TK::Any ? ": " + typeStr(mi.type) : "");
            addRef(f.loc, mi.sym, true);
            f.sym = mi.sym;
            ci->members[f.name] = mi;
        }
        for (auto& m : c->methods) {
            auto sig = sigOf(m.get());
            MemberInfo mi;
            mi.name = m->name; mi.isStatic = m->isStatic; mi.vis = m->vis;
            mi.kind = m->kind == FuncKind::Getter ? MemberInfo::Getter : m->kind == FuncKind::Setter ? MemberInfo::Setter : MemberInfo::Method;
            mi.type = m->kind == FuncKind::Getter ? (sig->ret ? sig->ret : tAny())
                    : m->kind == FuncKind::Setter ? (sig->params.empty() ? tAny() : sig->params[0].type)
                    : tFunc(sig);
            auto existing = ci->members.find(m->name);
            if (mi.kind == MemberInfo::Setter) mi.hasSetter = true;
            if (existing != ci->members.end()) {
                bool pair = (existing->second.kind == MemberInfo::Getter && mi.kind == MemberInfo::Setter) ||
                            (existing->second.kind == MemberInfo::Setter && mi.kind == MemberInfo::Getter);
                if (!pair) { diags.error(m->nameLoc, "El miembro '" + m->name + "' está repetido", "", "E0506"); continue; }
                if (mi.kind == MemberInfo::Setter) { existing->second.hasSetter = true; m->sym = existing->second.sym; m->owner = ci; continue; }
                mi.hasSetter = true;
            }
            mi.sym = unit.newSymbol(m->name, mi.kind == MemberInfo::Method ? SymKind::Method : SymKind::Property, m->nameLoc);
            mi.sym->container = c->name;
            mi.sym->type = mi.type;
            mi.sym->doc = m->doc;
            mi.sym->isStatic = m->isStatic;
            mi.sym->vis = m->vis;
            mi.sym->detail = (m->isStatic ? "static " : "") + std::string(m->isAsync ? "async " : "") +
                             (mi.kind == MemberInfo::Method ? sigStr(*sig, c->name + "." + m->name)
                                                            : (mi.kind == MemberInfo::Getter ? "get " : "set ") + c->name + "." + m->name + ": " + typeStr(mi.type));
            addRef(m->nameLoc, mi.sym, true);
            m->sym = mi.sym;
            m->owner = ci;
            ci->members[m->name] = mi;
        }
        if (c->ctor) {
            ci->ctor = sigOf(c->ctor.get());
            c->ctor->owner = ci;
            c->sym->detail += " " + ctorStr(*ci->ctor, c->name);
        } else if (c->isStruct) {
            auto sig = std::make_shared<FuncSig>();
            for (auto& f : c->fields) if (!f.isStatic) sig->params.push_back({f.name, ci->members[f.name].type, true, false, ""});
            sig->ret = tVoid();
            ci->ctor = sig;
            c->sym->detail += " " + ctorStr(*sig, c->name);
        }
        if (ci->super) c->sym->detail += " extends " + ci->super->name;
    }
    // Comprobar interfaces
    for (auto& sp : body) {
        if (sp->kind != StmtKind::Class) continue;
        auto* c = static_cast<ClassStmt*>(sp.get());
        for (auto* iface : c->info->interfaces) {
            for (auto* i = iface; i; i = nullptr) {
                std::vector<ClassInfo*> all{i};
                for (size_t k = 0; k < all.size(); k++) for (auto* e : all[k]->interfaces) all.push_back(e);
                for (auto* ii : all)
                    for (auto& [mn, mi] : ii->members) {
                        const MemberInfo* have = c->info->find(mn);
                        if (!have || have->kind != MemberInfo::Method)
                            diags.error(c->nameLoc, "La clase '" + c->name + "' no implementa '" + mn + "' de la interfaz '" + ii->name + "'",
                                        "añade: function " + (mi.sym ? mi.sym->detail : mn + "()") + " { ... }", "E0507");
                    }
            }
        }
    }
    // Funciones, variables y el resto
    for (auto& sp : body) {
        switch (sp->kind) {
            case StmtKind::Function: {
                auto* f = static_cast<FuncStmt*>(sp.get())->fn.get();
                auto sig = sigOf(f);
                int idx = declareGlobal(f->name, f->nameLoc, SymKind::Function, tFunc(sig), f->doc);
                f->res.kind = VarKind::Global; f->res.index = idx;
                f->sym = unit.globalSyms[f->name];
                f->sym->detail = std::string(f->isAsync ? "async " : "") + "function " + sigStr(*sig, f->name);
                break;
            }
            case StmtKind::Var: {
                auto* v = static_cast<VarStmt*>(sp.get());
                TypeP t = v->typeAnn ? resolveTypeRef(v->typeAnn.get()) : tAny();
                int idx = declareGlobal(v->name, v->nameLoc, v->declKind == DeclKind::Const ? SymKind::Constant : SymKind::Variable, t, v->doc);
                v->res.kind = VarKind::Global; v->res.index = idx;
                v->sym = unit.globalSyms[v->name];
                v->sym->detail = (v->declKind == DeclKind::Const ? "const " : v->declKind == DeclKind::Let ? "let " : "var ") + v->name +
                                 (t->kind != TK::Any ? ": " + typeStr(t) : "");
                v->sym->vis = v->vis;
                v->sym->assignOnce = v->declKind == DeclKind::Let && v->init;
                break;
            }
            default: break;
        }
    }
}

// ---------------------------------------------------------------------------
// Recorrido
// ---------------------------------------------------------------------------

void Analyzer::run() {
    Program& prog = *unit.program;
    prog.script = std::make_unique<FuncDecl>();
    prog.script->kind = FuncKind::Script;
    prog.script->name = "<principal>";
    if (unit.repl) unit.globalNames = unit.repl->globals;
    declareGlobals();

    FnCtx script;
    script.decl = prog.script.get();
    script.isScript = true;
    script.locals.push_back({"", 0});
    fn = &script;
    unit.scopes.push_back({0, (int)prog.source.size() + 1, {}});
    checkUnreachable(prog.body);
    for (auto& s : prog.body) {
        topLevelOffset = s->loc.offset;
        stmt(s.get());
    }
    prog.script->maxSlots = script.maxSlots;
    prog.globalCount = (int)unit.globalNames.size();
    prog.globalNames = unit.globalNames;
    // Globales sin usar (solo funciones/vars privadas)
    for (auto& [n, s] : unit.globalSyms) {
        if (s && !s->used && s->vis == Visibility::Private && (s->kind == SymKind::Variable || s->kind == SymKind::Function))
            diags.hintUnused(s->loc, "'" + n + "' es privado y no se usa");
    }
    for (auto& [n, s] : unit.globalSyms) unit.scopes[0].symbols.push_back(s);
    // Vista exportada del módulo
    unit.info = std::make_unique<ModuleInfo>();
    unit.info->name = unit.name;
    unit.info->unit = &unit;
    for (auto& [n, idx] : unit.globalIndex) {
        Symbol* s = unit.globalSyms[n];
        if (s && s->vis == Visibility::Private) continue;
        MemberInfo mi;
        mi.name = n;
        mi.type = unit.globalTypes[n];
        mi.sym = s;
        mi.readonly = !s || s->kind != SymKind::Variable;
        mi.kind = mi.type && mi.type->kind == TK::Func ? MemberInfo::Method : MemberInfo::Property;
        unit.info->members[n] = mi;
    }
    fn = nullptr;
}

void Analyzer::checkUnreachable(const std::vector<StmtP>& body) {
    for (size_t i = 0; i + 1 < body.size(); i++) {
        auto k = body[i]->kind;
        if (k == StmtKind::Return || k == StmtKind::Throw || k == StmtKind::Break || k == StmtKind::Continue) {
            auto nk = body[i + 1]->kind;
            if (nk == StmtKind::Function || nk == StmtKind::Class) continue;
            diags.warning(body[i + 1]->loc, "Código inalcanzable: nunca se ejecuta", "está después de un return/throw/break/continue", "W0301");
            break;
        }
    }
}

bool Analyzer::blockReturns(const std::vector<StmtP>& body) {
    for (auto& s : body) if (alwaysReturns(s.get())) return true;
    return false;
}

bool Analyzer::alwaysReturns(Stmt* s) {
    switch (s->kind) {
        case StmtKind::Return: case StmtKind::Throw: return true;
        case StmtKind::Block: return blockReturns(static_cast<BlockStmt*>(s)->body);
        case StmtKind::If: {
            auto* i = static_cast<IfStmt*>(s);
            return i->otherwise && alwaysReturns(i->then.get()) && alwaysReturns(i->otherwise.get());
        }
        case StmtKind::Try: {
            auto* t = static_cast<TryStmt*>(s);
            if (t->finallyBody && blockReturns(t->finallyBody->body)) return true;
            return blockReturns(t->body->body) && (!t->catchBody || blockReturns(t->catchBody->body));
        }
        case StmtKind::While: {
            auto* w = static_cast<WhileStmt*>(s);
            return w->cond->kind == ExprKind::Bool && static_cast<BoolExpr*>(w->cond.get())->value;
        }
        case StmtKind::Expr: {
            auto* e = static_cast<ExprStmt*>(s)->expr.get();
            if (e->kind == ExprKind::Match) {
                auto* m = static_cast<MatchExpr*>(e);
                bool hasElse = false;
                for (auto& a : m->arms) {
                    if (a.isElse) hasElse = true;
                    if (!a.hasBlock || !blockReturns(a.block)) return false;
                }
                return hasElse;
            }
            return false;
        }
        default: return false;
    }
}

void Analyzer::function(FuncDecl* f, ClassInfo* cls) {
    FnCtx ctx;
    ctx.enclosing = fn;
    ctx.decl = f;
    ctx.cls = cls;
    ctx.isStatic = f->isStatic;
    bool hasThis = cls && !f->isStatic &&
                   (f->kind == FuncKind::Method || f->kind == FuncKind::Constructor || f->kind == FuncKind::Getter ||
                    f->kind == FuncKind::Setter || f->kind == FuncKind::FieldInit);
    ctx.locals.push_back({hasThis ? "this" : "", 0});
    if (hasThis) ctx.locals.back().type = tInstance(cls);
    FnCtx* saved = fn;
    ClassInfo* savedClass = currentClass;
    fn = &ctx;
    if (cls) currentClass = cls;
    bool savedMatch = matchBlocksAllowed;
    TypeP handlerType = pendingHandlerType;
    pendingHandlerType = nullptr;

    auto sig = sigOf(f);
    ctx.retType = f->ret ? sig->ret : nullptr;
    int bodyStart = f->body ? f->body->loc.offset : f->loc.offset;
    int bodyEnd = f->body ? f->body->endLoc.offset + 1 : (f->exprBody ? f->exprBody->loc.offset + 64 : f->loc.offset);
    beginScope(f->loc.offset);
    for (size_t i = 0; i < f->params.size(); i++) {
        auto& p = f->params[i];
        TypeP pt = sig->params[i].type;
        if (p.rest) pt = tList(pt->kind == TK::List ? pt->args[0] : pt);
        if (f->kind == FuncKind::Handler && p.type == nullptr) pt = handlerType ? handlerType : tAny();
        p.slot = declareLocal(p.name, p.loc, SymKind::Parameter, pt);
        fn->locals.back().isParam = true;
        p.sym = fn->locals.back().sym;
        if (p.sym) { p.sym->detail = p.name + (pt->kind != TK::Any ? ": " + typeStr(pt) : ""); p.sym->scopeEnd = bodyEnd; }
    }
    for (auto& p : f->params) {
        if (p.def) {
            TypeP dt = expr(p.def.get());
            if (p.type) checkAssignable(resolveTypeRef(p.type.get()), dt, p.def->loc, "el valor por defecto de '" + p.name + "'");
        }
    }
    if (f->body) {
        checkUnreachable(f->body->body);
        for (auto& s : f->body->body) stmt(s.get());
        if (ctx.retType && ctx.retType->kind != TK::Void && ctx.retType->kind != TK::Any && f->kind != FuncKind::Constructor &&
            !blockReturns(f->body->body)) {
            diags.warning(f->nameLoc.line ? f->nameLoc : f->loc,
                          "La función '" + f->name + "' declara que devuelve " + typeStr(ctx.retType) + " pero puede terminar sin 'return'",
                          "añade 'return valor' al final", "W0302");
        }
    } else if (f->exprBody) {
        matchBlocksAllowed = f->exprBody->kind == ExprKind::Match;
        TypeP rt = expr(f->exprBody.get());
        if (ctx.retType) checkAssignable(ctx.retType, rt, f->exprBody->loc, "el valor devuelto");
    }
    endScope(f->body.get(), bodyEnd);
    (void)bodyStart;
    f->maxSlots = ctx.maxSlots;
    if (f->kind == FuncKind::Handler && f->containsAwait) f->isAsync = true;
    fn = saved;
    currentClass = savedClass;
    matchBlocksAllowed = savedMatch;
}

void Analyzer::block(BlockStmt* b, bool newScope) {
    if (newScope) beginScope(b->loc.offset);
    checkUnreachable(b->body);
    for (auto& s : b->body) stmt(s.get());
    if (newScope) endScope(b, b->endLoc.offset + 1);
}

void Analyzer::checkAssignable(TypeP to, TypeP from, SrcLoc loc, const std::string& context) {
    if (!assignable(to, from)) {
        std::string hint;
        if (to->kind == TK::Number && from->kind == TK::Text) hint = "convierte el texto con toNumber(...)";
        else if (to->kind == TK::Text && isNumeric(from)) hint = "convierte el número con str(...) o usa interpolación \"${valor}\"";
        else if (from->kind == TK::Null) hint = "si quieres permitir null, declara el tipo como " + typeStr(to) + "?";
        diags.error(loc, "Tipo incorrecto en " + context + ": se esperaba " + typeStr(to) + " pero es " + typeStr(from), hint, "E0302");
    }
}

void Analyzer::stmt(Stmt* s) {
    switch (s->kind) {
        case StmtKind::Expr: {
            auto* es = static_cast<ExprStmt*>(s);
            Expr* e = es->expr.get();
            matchBlocksAllowed = e->kind == ExprKind::Match;
            expr(e);
            matchBlocksAllowed = false;
            switch (static_cast<ExprStmt*>(s)->isBlockValue ? ExprKind::Call : e->kind) {
                case ExprKind::Binary: case ExprKind::Number: case ExprKind::String: case ExprKind::Bool: case ExprKind::Ident:
                case ExprKind::Member: case ExprKind::Index: case ExprKind::Logical: case ExprKind::List:
                    if (e->kind == ExprKind::Binary && static_cast<BinaryExpr*>(e)->op == Tok::EqEq)
                        diags.warning(e->loc, "Esta comparación no tiene efecto", "¿querías asignar con '=' en vez de comparar con '=='?", "W0303");
                    else
                        diags.warning(e->loc, "Esta expresión no tiene efecto", "", "W0303");
                    break;
                default: break;
            }
            break;
        }
        case StmtKind::Var: {
            auto* v = static_cast<VarStmt*>(s);
            TypeP ann = v->typeAnn ? resolveTypeRef(v->typeAnn.get()) : nullptr;
            TypeP it = nullptr;
            if (v->init) {
                matchBlocksAllowed = v->init->kind == ExprKind::Match;
                it = expr(v->init.get());
                matchBlocksAllowed = false;
                if (it->kind == TK::Void) diags.error(v->init->loc, "Esta expresión no devuelve ningún valor (void)", "", "E0303");
                if (ann) {
                    checkAssignable(ann, it, v->init->loc, "la variable '" + v->name + "'");
                    if (ann->kind == TK::Int && v->init->kind == ExprKind::Number && !static_cast<NumberExpr*>(v->init.get())->isInt)
                        diags.error(v->init->loc, "'" + v->name + "' es int pero el valor tiene decimales", "usa number o round(...)", "E0304");
                }
            }
            TypeP final = ann ? ann : (it && it->kind != TK::Null && it->kind != TK::Void ? widen(it) : tAny());
            if (fn->isScript && fn->scopeDepth == 0) {
                unit.globalTypes[v->name] = final;
                if (v->sym) {
                    v->sym->type = final;
                    v->sym->detail = (v->declKind == DeclKind::Const ? "const " : v->declKind == DeclKind::Let ? "let " : "var ") + v->name +
                                     (final->kind != TK::Any ? ": " + typeStr(final) : "");
                }
            } else {
                int slot = declareLocal(v->name, v->nameLoc, v->declKind == DeclKind::Const ? SymKind::Constant : SymKind::Variable, final, v->declKind);
                v->res.kind = VarKind::Local; v->res.index = slot;
                v->sym = fn->locals.back().sym;
                if (v->sym) v->sym->doc = v->doc;
                if (!v->init && v->declKind == DeclKind::Let) fn->locals.back().initialized = false;
            }
            break;
        }
        case StmtKind::Block: block(static_cast<BlockStmt*>(s)); break;
        case StmtKind::If: {
            auto* i = static_cast<IfStmt*>(s);
            expr(i->cond.get());
            if (i->cond->kind == ExprKind::Assign) diags.error(i->cond->loc, "Asignación dentro de una condición", "para comparar usa '=='", "E0305");
            stmt(i->then.get());
            if (i->otherwise) stmt(i->otherwise.get());
            break;
        }
        case StmtKind::While: {
            auto* w = static_cast<WhileStmt*>(s);
            expr(w->cond.get());
            fn->loopDepth++;
            stmt(w->body.get());
            fn->loopDepth--;
            break;
        }
        case StmtKind::ForRange: {
            auto* f = static_cast<ForRangeStmt*>(s);
            for (Expr* e : {f->start.get(), f->end.get(), f->step.get()}) {
                if (!e) continue;
                TypeP t = expr(e);
                if (isKnown(t) && !isNumeric(t)) diags.error(e->loc, "Los límites de un rango deben ser números, no " + typeStr(t), "", "E0306");
            }
            beginScope(s->loc.offset);
            f->slotCounter = declareLocal("contador", s->loc, SymKind::Variable, tNum(), DeclKind::Var, true);
            f->slotEnd = declareLocal("fin", s->loc, SymKind::Variable, tNum(), DeclKind::Var, true);
            f->slotStep = declareLocal("paso", s->loc, SymKind::Variable, tNum(), DeclKind::Var, true);
            beginScope(f->varLoc.offset);
            f->slotVar = declareLocal(f->var, f->varLoc, SymKind::Variable, tNum());
            f->sym = fn->locals.back().sym;
            if (f->sym) f->sym->used = true;
            fn->loopDepth++;
            stmt(f->body.get());
            fn->loopDepth--;
            int end = f->body->kind == StmtKind::Block ? static_cast<BlockStmt*>(f->body.get())->endLoc.offset + 1 : s->loc.offset;
            f->varCaptured = fn->locals[f->slotVar].captured;
            endScope(nullptr, end);
            endScope(nullptr, end);
            break;
        }
        case StmtKind::ForEach: {
            auto* f = static_cast<ForEachStmt*>(s);
            TypeP it = expr(f->iterable.get());
            TypeP elem = tAny(), key = tAny();
            switch (it->kind) {
                case TK::List: elem = it->args[0]; key = tNum(); break;
                case TK::Set: elem = it->args[0]; key = tNum(); break;
                case TK::Map: key = it->args[0]; elem = it->args[1]; break;
                case TK::Text: elem = tText(); key = tNum(); break;
                case TK::Range: elem = tNum(); key = tNum(); break;
                case TK::Number: case TK::Int: case TK::Bool: case TK::Null: case TK::Void:
                    diags.error(f->iterable->loc, "No se puede recorrer un valor de tipo " + typeStr(it),
                                isNumeric(it) ? "para repetir N veces usa: for i in 0..n { }" : "", "E0307");
                    break;
                default: break;
            }
            beginScope(s->loc.offset);
            f->slotIter = declareLocal("coleccion", s->loc, SymKind::Variable, tAny(), DeclKind::Var, true);
            f->slotState = declareLocal("estado", s->loc, SymKind::Variable, tNum(), DeclKind::Var, true);
            beginScope(f->varLoc.offset);
            if (!f->var2.empty()) {
                f->slotVar2 = declareLocal(f->var2, f->var2Loc, SymKind::Variable, key);
                f->sym2 = fn->locals.back().sym;
                if (f->sym2) f->sym2->used = true;
            } else {
                f->slotVar2 = declareLocal("clave", s->loc, SymKind::Variable, key, DeclKind::Var, true);
            }
            f->slotVar = declareLocal(f->var, f->varLoc, SymKind::Variable, elem);
            f->sym = fn->locals.back().sym;
            if (f->sym) f->sym->used = true;
            fn->loopDepth++;
            stmt(f->body.get());
            fn->loopDepth--;
            int end = f->body->kind == StmtKind::Block ? static_cast<BlockStmt*>(f->body.get())->endLoc.offset + 1 : s->loc.offset;
            f->varCaptured = fn->locals[f->slotVar].captured;
            f->var2Captured = fn->locals[f->slotVar2].captured;
            endScope(nullptr, end);
            endScope(nullptr, end);
            break;
        }
        case StmtKind::Repeat: {
            auto* r = static_cast<RepeatStmt*>(s);
            TypeP t = expr(r->count.get());
            if (isKnown(t) && !isNumeric(t)) diags.error(r->count->loc, "repeat necesita un número, no " + typeStr(t), "", "E0306");
            beginScope(s->loc.offset);
            r->slotCounter = declareLocal("repeticiones", s->loc, SymKind::Variable, tNum(), DeclKind::Var, true);
            fn->loopDepth++;
            stmt(r->body.get());
            fn->loopDepth--;
            endScope(nullptr, s->loc.offset);
            break;
        }
        case StmtKind::Break:
            if (fn->loopDepth == 0) diags.error(s->loc, "'break' fuera de un bucle", "", "E0308");
            break;
        case StmtKind::Continue:
            if (fn->loopDepth == 0) diags.error(s->loc, "'continue' fuera de un bucle", "", "E0308");
            break;
        case StmtKind::Return: {
            auto* r = static_cast<ReturnStmt*>(s);
            if (fn->isScript) { diags.error(s->loc, "'return' fuera de una función", "", "E0309"); if (r->value) expr(r->value.get()); break; }
            bool isCtor = fn->decl->kind == FuncKind::Constructor;
            if (r->value) {
                matchBlocksAllowed = r->value->kind == ExprKind::Match;
                TypeP t = expr(r->value.get());
                matchBlocksAllowed = false;
                if (isCtor) diags.error(r->value->loc, "Un constructor no puede devolver un valor", "", "E0310");
                else if (fn->retType && fn->retType->kind == TK::Void)
                    diags.error(r->value->loc, "La función '" + fn->decl->name + "' es void y no puede devolver un valor", "quita el valor o cambia el tipo de retorno", "E0310");
                else if (fn->retType) checkAssignable(fn->retType, t, r->value->loc, "el valor devuelto");
            } else if (fn->retType && fn->retType->kind != TK::Void && fn->retType->kind != TK::Any && !fn->retType->nullable) {
                diags.error(s->loc, "La función '" + fn->decl->name + "' debe devolver un valor de tipo " + typeStr(fn->retType), "", "E0311");
            }
            break;
        }
        case StmtKind::Throw: expr(static_cast<ThrowStmt*>(s)->value.get()); break;
        case StmtKind::Try: {
            auto* t = static_cast<TryStmt*>(s);
            block(t->body.get());
            if (t->catchBody) {
                beginScope(t->catchBody->loc.offset);
                std::string name = t->catchVar.empty() ? "error" : t->catchVar;
                TypeP et = t->catchType ? resolveTypeRef(t->catchType.get()) : tAny();
                t->catchSlot = declareLocal(name, t->catchVar.empty() ? t->catchBody->loc : t->catchLoc, SymKind::Variable, et, DeclKind::Var, t->catchVar.empty());
                t->catchSym = fn->locals.back().sym;
                if (t->catchSym) t->catchSym->used = true;
                block(t->catchBody.get(), true);
                t->catchCaptured = fn->locals[t->catchSlot].captured;
                endScope(nullptr, t->catchBody->endLoc.offset + 1);
            }
            if (t->finallyBody) {
                t->finallyFn = std::make_unique<FuncDecl>();
                t->finallyFn->kind = FuncKind::Lambda;
                t->finallyFn->name = "finally";
                t->finallyFn->loc = t->finallyBody->loc;
                t->finallyFn->body = std::move(t->finallyBody);
                function(t->finallyFn.get(), currentClass);
            }
            break;
        }
        case StmtKind::Function: {
            auto* f = static_cast<FuncStmt*>(s)->fn.get();
            if (fn->isScript && fn->scopeDepth == 0) {
                function(f, nullptr);
            } else {
                auto sig = sigOf(f);
                int slot = declareLocal(f->name, f->nameLoc, SymKind::Function, tFunc(sig));
                f->res.kind = VarKind::Local; f->res.index = slot;
                f->sym = fn->locals.back().sym;
                if (f->sym) { f->sym->detail = "function " + sigStr(*sig, f->name); f->sym->doc = f->doc; }
                function(f, currentClass);
            }
            break;
        }
        case StmtKind::Class: {
            auto* c = static_cast<ClassStmt*>(s);
            if (!(fn->isScript && fn->scopeDepth == 0)) { diags.error(s->loc, "Las clases solo pueden declararse en el nivel superior del archivo", "", "E0508"); break; }
            ClassInfo* ci = c->info;
            // Inicializadores estáticos (en el contexto del script)
            ClassInfo* saved = currentClass;
            currentClass = ci;
            for (auto& f : c->fields) {
                if (!f.isStatic || !f.init) continue;
                TypeP t = expr(f.init.get());
                if (f.type) checkAssignable(ci->members[f.name].type, t, f.init->loc, "el campo '" + f.name + "'");
            }
            currentClass = saved;
            // Inicializadores de instancia: función sintetizada con this
            bool anyInit = false;
            for (auto& f : c->fields) if (!f.isStatic && f.init) anyInit = true;
            if (anyInit) {
                c->fieldInit = std::make_unique<FuncDecl>();
                c->fieldInit->kind = FuncKind::FieldInit;
                c->fieldInit->name = c->name + ".<campos>";
                c->fieldInit->loc = c->loc;
                FnCtx ctx;
                ctx.enclosing = fn;
                ctx.decl = c->fieldInit.get();
                ctx.cls = ci;
                ctx.locals.push_back({"this", 0});
                ctx.locals.back().type = tInstance(ci);
                FnCtx* sf = fn;
                fn = &ctx;
                currentClass = ci;
                for (auto& f : c->fields) {
                    if (f.isStatic || !f.init) continue;
                    TypeP t = expr(f.init.get());
                    if (f.type) {
                        checkAssignable(ci->members[f.name].type, t, f.init->loc, "el campo '" + f.name + "'");
                    } else if (isKnown(t) && t->kind != TK::Null && t->kind != TK::Void) {
                        ci->members[f.name].type = widen(t);
                        if (f.sym) { f.sym->type = widen(t); f.sym->detail += ": " + typeStr(widen(t)); }
                    }
                }
                c->fieldInit->maxSlots = ctx.maxSlots;
                fn = sf;
                currentClass = saved;
            }
            for (auto& m : c->methods) function(m.get(), ci);
            if (c->ctor) {
                function(c->ctor.get(), ci);
            }
            break;
        }
        case StmtKind::Interface: break;
        case StmtKind::Enum: {
            auto* e = static_cast<EnumStmt*>(s);
            double next = 0;
            for (auto& m : e->members) {
                if (m.value) {
                    Expr* v = m.value.get();
                    if (v->kind == ExprKind::Number) { m.resolved = static_cast<NumberExpr*>(v)->value; next = m.resolved + 1; }
                    else if (v->kind == ExprKind::String && static_cast<StringExpr*>(v)->isPlain()) {
                        m.isString = true; m.strValue = static_cast<StringExpr*>(v)->parts[0].text;
                    } else diags.error(v->loc, "El valor de un enum debe ser un número o un texto literal", "", "E0209");
                } else {
                    m.resolved = next++;
                }
            }
            break;
        }
        case StmtKind::Event: break;
        case StmtKind::Emit: {
            auto* e = static_cast<EmitStmt*>(s);
            TypeP et = expr(e->event.get());
            for (auto& a : e->args) expr(a.value.get());
            if (et->kind == TK::Event && et->ev) {
                FuncSig sig;
                for (auto& p : et->ev->params) sig.params.push_back(p);
                sig.ret = tVoid();
                checkArgs("el evento '" + et->ev->name + "'", sig, e->args, e->event->loc);
            } else if (isKnown(et)) {
                diags.error(e->event->loc, "'emit' necesita un evento, pero esto es " + typeStr(et), "declara el evento con: event Nombre(param: tipo)", "E0401");
            }
            break;
        }
        case StmtKind::Import:
            if (!(fn->isScript && fn->scopeDepth == 0)) diags.error(s->loc, "'import' solo puede usarse en el nivel superior del archivo", "", "E0210");
            break;
    }
}

void Analyzer::checkArgs(const std::string& what, const FuncSig& sig, std::vector<Arg>& args, SrcLoc loc) {
    int positional = 0;
    bool hasSpread = false;
    for (auto& a : args) { if (a.spread) hasSpread = true; if (a.name.empty() && !a.spread) positional++; }
    if (hasSpread) return;
    int maxA = sig.maxArgs(), minA = sig.minArgs();
    std::vector<bool> filled(sig.params.size(), false);
    for (int i = 0; i < positional && i < (int)sig.params.size(); i++) filled[i] = true;
    if (sig.params.size() > 0 && sig.params.back().rest)
        for (size_t i = sig.params.size() - 1; i < sig.params.size(); i++) filled[i] = true;
    if (maxA >= 0 && positional > maxA) {
        diags.error(args[maxA].value->loc,
                    "Demasiados argumentos para " + what + ": acepta " + std::to_string(maxA) + " y se pasaron " + std::to_string(positional),
                    "firma: " + sigStr(sig), "E0402");
        return;
    }
    // Con nombre
    for (auto& a : args) {
        if (a.name.empty()) continue;
        int found = -1;
        for (size_t i = 0; i < sig.params.size(); i++) if (sig.params[i].name == a.name) found = (int)i;
        if (found < 0) {
            if (sig.acceptsNamedOptions) continue;
            std::vector<std::string> names;
            for (auto& p : sig.params) names.push_back(p.name);
            std::string sug = closestMatch(a.name, names);
            diags.error(a.nameLoc, what + " no tiene ningún parámetro llamado '" + a.name + "'", sug.empty() ? "firma: " + sigStr(sig) : "¿quisiste decir '" + sug + "'?", "E0403");
            continue;
        }
        if (filled[found] && !sig.params[found].rest) diags.error(a.nameLoc, "El parámetro '" + a.name + "' ya recibió un valor", "", "E0404");
        filled[found] = true;
        if (a.value->type) checkAssignable(sig.params[found].type, a.value->type, a.value->loc, "el argumento '" + a.name + "'");
    }
    std::vector<std::string> missing;
    for (int i = 0; i < (int)sig.params.size(); i++)
        if (!filled[i] && !sig.params[i].optional && !sig.params[i].rest) missing.push_back(sig.params[i].name);
    (void)minA;
    if (!missing.empty()) {
        std::string m;
        for (size_t i = 0; i < missing.size(); i++) m += (i ? ", " : "") + std::string("'") + missing[i] + "'";
        diags.error(loc, "Faltan argumentos para " + what + ": " + m, "firma: " + sigStr(sig), "E0405");
    }
    // Tipos posicionales
    int pi = 0;
    for (auto& a : args) {
        if (!a.name.empty()) continue;
        if (pi >= (int)sig.params.size()) break;
        const SigParam& p = sig.params[pi];
        TypeP want = p.rest ? (p.type->kind == TK::List ? p.type->args[0] : p.type) : p.type;
        if (a.value->type) checkAssignable(want, a.value->type, a.value->loc, "el argumento '" + p.name + "'");
        if (!p.rest) pi++;
    }
}

TypeP Analyzer::memberType(TypeP ot, const std::string& name, SrcLoc loc, bool report, Symbol** symOut) {
    if (!ot) return tAny();
    auto fromNative = [&](const std::string& cname) -> TypeP {
        auto* c = world.findClass(cname);
        if (!c) return tAny();
        const MemberInfo* m = c->find(name);
        if (!m) {
            if (report) {
                std::vector<std::string> cands;
                for (auto& [n, mm] : c->members) if (!mm.isStatic) cands.push_back(n);
                std::string sug = closestMatch(name, cands);
                diags.error(loc, "El tipo " + typeStr(ot) + " no tiene '" + name + "'", sug.empty() ? "" : "¿quisiste decir '" + sug + "'?", "E0310");
            }
            return tAny();
        }
        if (symOut) *symOut = m->sym;
        TypeP t = m->type;
        // Sustituir T (elemento) en métodos de colecciones
        if (t && t->kind == TK::Func && t->sig && (ot->kind == TK::List || ot->kind == TK::Set || ot->kind == TK::Map) && !ot->args.empty()) {
            TypeP elem = ot->kind == TK::Map ? ot->args[1] : ot->args[0];
            if (t->sig->ret && t->sig->ret->placeholder) {
                auto s2 = std::make_shared<FuncSig>(*t->sig);
                s2->ret = elem;
                return tFunc(s2);
            }
        }
        return t;
    };
    switch (ot->kind) {
        case TK::Any: return tAny();
        case TK::Null:
            if (report) diags.error(loc, "No se puede acceder a '" + name + "' porque el valor es null", "usa ?. para acceso seguro: obj?." + name, "E0311");
            return tAny();
        case TK::Text: return fromNative("text");
        case TK::Number: case TK::Int: return fromNative("number");
        case TK::Bool: return tAny();
        case TK::List: return fromNative("list");
        case TK::Set: return fromNative("set");
        case TK::Map: {
            auto* c = world.findClass("map");
            if (c && c->find(name)) return fromNative("map");
            return ot->args.size() > 1 ? ot->args[1] : tAny();
        }
        case TK::Range: return fromNative("Range");
        case TK::Task: return fromNative("Task");
        case TK::Subscription: return fromNative("Subscription");
        case TK::Event: return fromNative("Event");
        case TK::Func: return fromNative("function");
        case TK::EnumValue:
            if (name == "name") return tText();
            if (name == "value" || name == "ordinal") return name == "value" ? tAny() : tNum();
            if (report) diags.error(loc, "Un valor de enum solo tiene 'name', 'value' y 'ordinal'", "", "E0312");
            return tAny();
        case TK::Enum: {
            if (!ot->en) return tAny();
            if (name == "values") { auto s = std::make_shared<FuncSig>(); s->ret = tList(tEnumVal(ot->en)); return tFunc(s); }
            if (name == "count") return tNum();
            if (name == "fromName" || name == "fromValue") {
                auto s = std::make_shared<FuncSig>(); s->params.push_back({"v", tAny(), false, false, ""}); s->ret = nullableOf(tEnumVal(ot->en)); return tFunc(s);
            }
            if (name == "name") return tText();
            for (auto& m : ot->en->members) if (m == name) {
                if (symOut) { auto it = ot->en->memberSyms.find(name); if (it != ot->en->memberSyms.end()) *symOut = it->second; }
                return tEnumVal(ot->en);
            }
            if (report) {
                std::string sug = closestMatch(name, ot->en->members);
                diags.error(loc, "El enum '" + ot->en->name + "' no tiene el valor '" + name + "'", sug.empty() ? "" : "¿quisiste decir '" + sug + "'?", "E0313");
            }
            return tAny();
        }
        case TK::Module: {
            if (!ot->mod) return tAny();
            auto it = ot->mod->members.find(name);
            if (it == ot->mod->members.end()) {
                if (report) {
                    std::vector<std::string> cands;
                    for (auto& [n, m] : ot->mod->members) cands.push_back(n);
                    std::string sug = closestMatch(name, cands);
                    diags.error(loc, "'" + ot->mod->name + "' no tiene ningún miembro llamado '" + name + "'", sug.empty() ? "" : "¿quisiste decir '" + sug + "'?", "E0314");
                }
                return tAny();
            }
            if (symOut) *symOut = it->second.sym;
            return it->second.type ? it->second.type : tAny();
        }
        case TK::Instance: case TK::Class: {
            ClassInfo* c = ot->cls;
            if (!c) return tAny();
            if (c->isNative && ot->kind == TK::Instance) {
                const MemberInfo* m = c->find(name);
                if (m && !m->isStatic) { if (symOut) *symOut = m->sym; return m->type; }
                if (report) {
                    std::vector<std::string> cands;
                    for (auto& [n, mm] : c->members) if (!mm.isStatic) cands.push_back(n);
                    std::string sug = closestMatch(name, cands);
                    diags.error(loc, c->name + " no tiene ningún miembro llamado '" + name + "'", sug.empty() ? "" : "¿quisiste decir '" + sug + "'?", "E0315");
                }
                return tAny();
            }
            const MemberInfo* m = c->find(name);
            if (!m) {
                if (ot->kind == TK::Class && name == "name") return tText();
                if (report) {
                    std::vector<std::string> cands;
                    for (auto* k = c; k; k = k->super) for (auto& [n, mm] : k->members) if (mm.isStatic == (ot->kind == TK::Class)) cands.push_back(n);
                    std::string sug = closestMatch(name, cands);
                    if (ot->kind == TK::Class || c->isInterface)
                        diags.error(loc, "'" + c->name + "' no tiene ningún miembro estático llamado '" + name + "'", sug.empty() ? "" : "¿quisiste decir '" + sug + "'?", "E0316");
                    else
                        diags.warning(loc, "La clase '" + c->name + "' no declara '" + name + "'",
                                      sug.empty() ? "decláralo en la clase: var " + name + " ...  o  function " + name + "() { ... }" : "¿quisiste decir '" + sug + "'?", "W0304");
                }
                return tAny();
            }
            if (symOut) *symOut = m->sym;
            if (ot->kind == TK::Class && !m->isStatic) {
                if (report) diags.error(loc, "'" + name + "' es un miembro de instancia; necesitas un objeto de '" + c->name + "'", "crea uno con " + c->name + "(...)", "E0317");
                return tAny();
            }
            if (ot->kind == TK::Instance && m->isStatic && report)
                diags.warning(loc, "'" + name + "' es estático; accede con " + c->name + "." + name, "", "W0305");
            // Visibilidad
            if (m->vis != Visibility::Public && report) {
                const ClassInfo* owner = c;
                for (auto* k = c; k; k = k->super) if (k->members.count(name)) { owner = k; break; }
                bool inside = currentClass && (currentClass == owner || (m->vis == Visibility::Protected && currentClass->isSubclassOf(owner)));
                if (!inside)
                    diags.error(loc, "'" + name + "' es " + (m->vis == Visibility::Private ? "privado" : "protegido") + " en '" + owner->name + "'", "", "E0318");
            }
            if (m->sym) m->sym->used = true;
            return m->type ? m->type : tAny();
        }
        default: return tAny();
    }
}

TypeP Analyzer::member(MemberExpr* m) {
    TypeP ot = expr(m->object.get());
    if (m->optional && ot->nullable) { ot = std::make_shared<Type>(*ot); ot->nullable = false; }
    Symbol* sym = nullptr;
    TypeP t = memberType(ot, m->name, m->nameLoc, true, &sym);
    if (sym) { m->sym = sym; sym->used = true; addRef(m->nameLoc, sym); }
    if (m->optional) t = nullableOf(t);
    return t;
}

TypeP Analyzer::call(CallExpr* c) {
    // super(...) en un constructor
    if (c->callee->kind == ExprKind::Super && static_cast<SuperExpr*>(c->callee.get())->member.empty()) {
        auto* sup = static_cast<SuperExpr*>(c->callee.get());
        for (auto& a : c->args) expr(a.value.get());
        if (!fn->decl || fn->decl->kind != FuncKind::Constructor || !currentClass) {
            diags.error(c->loc, "super(...) solo puede llamarse dentro de un constructor", "", "E0509");
            return tVoid();
        }
        fn->decl->hasExplicitSuperCall = true;
        if (!currentClass->super) { diags.error(c->loc, "La clase '" + currentClass->name + "' no hereda de ninguna otra", "", "E0510"); return tVoid(); }
        resolveThis(sup->thisRes);
        if (auto sig = currentClass->super->findCtor()) checkArgs("el constructor de " + currentClass->super->name, *sig, c->args, c->loc);
        return tVoid();
    }
    TypeP ct = expr(c->callee.get());
    for (auto& a : c->args) expr(a.value.get());
    std::string what = "la función";
    if (c->callee->kind == ExprKind::Ident) what = "'" + static_cast<IdentExpr*>(c->callee.get())->name + "'";
    else if (c->callee->kind == ExprKind::Member) what = "'" + static_cast<MemberExpr*>(c->callee.get())->name + "'";
    switch (ct->kind) {
        case TK::Func:
            if (ct->sig) {
                checkArgs(what, *ct->sig, c->args, c->loc);
                TypeP r = ct->sig->ret ? ct->sig->ret : tAny();
                return ct->sig->isAsync ? tTask(r) : r;
            }
            return tAny();
        case TK::Class: {
            ClassInfo* ci = ct->cls;
            if (!ci) return tAny();
            if (ci->isInterface) { diags.error(c->loc, "No se puede crear un objeto de la interfaz '" + ci->name + "'", "", "E0511"); return tAny(); }
            if (auto sig = ci->findCtor()) checkArgs("el constructor de " + ci->name, *sig, c->args, c->loc);
            else if (!ci->isNative && !c->args.empty() && !c->hasNamed()) {
                diags.error(c->args[0].value->loc, "La clase '" + ci->name + "' no tiene constructor y no acepta argumentos",
                            "añade: constructor(...) { ... }", "E0402");
            }
            return tInstance(ci);
        }
        case TK::Any: return tAny();
        case TK::Null:
            diags.error(c->callee->loc, "No se puede llamar a null", "", "E0406");
            return tAny();
        case TK::Event:
            diags.error(c->callee->loc, "Los eventos no se llaman: usa 'emit " + (ct->ev ? ct->ev->name : std::string("Evento")) + "(...)'", "", "E0407");
            return tAny();
        case TK::Module:
            diags.error(c->callee->loc, "Un módulo no es una función", "", "E0406");
            return tAny();
        case TK::Enum: case TK::EnumValue: case TK::Number: case TK::Int: case TK::Text: case TK::Bool: case TK::List:
        case TK::Map: case TK::Set: case TK::Range: case TK::Void:
            diags.error(c->callee->loc, what + " no es una función (es " + typeStr(ct) + ")", "", "E0406");
            return tAny();
        default: return tAny();
    }
}

void Analyzer::assignTo(Expr* target, TypeP vt, SrcLoc loc, bool compound) {
    switch (target->kind) {
        case ExprKind::Ident: {
            auto* id = static_cast<IdentExpr*>(target);
            Symbol* sym = nullptr;
            TypeP t;
            if (!resolveName(id->name, id->loc, id->res, sym, t, true)) {
                std::string sug = closestMatch(id->name, visibleNames());
                diags.error(id->loc, "No existe ninguna variable llamada '" + id->name + "'",
                            sug.empty() ? "declárala primero con: var " + id->name + " = ..." : "¿quisiste decir '" + sug + "'? Para crearla usa: var " + id->name + " = ...", "E0205");
                return;
            }
            id->sym = sym;
            addRef(id->loc, sym);
            if (compound && sym) sym->used = true;
            // Constantes / let
            if (id->res.kind == VarKind::Local || id->res.kind == VarKind::Upvalue) {
                LocalVar* lv = nullptr;
                if (id->res.kind == VarKind::Local) lv = &fn->locals[id->res.index];
                else for (FnCtx* f = fn->enclosing; f && !lv; f = f->enclosing) { int k = resolveLocal(f, id->name); if (k >= 0) lv = &f->locals[k]; }
                if (lv) {
                    if (lv->declKind == DeclKind::Const) diags.error(id->loc, "No se puede reasignar la constante '" + id->name + "'", "usa 'var' si necesitas cambiar su valor", "E0212");
                    else if (lv->declKind == DeclKind::Let) {
                        if (lv->initialized) diags.error(id->loc, "'" + id->name + "' se declaró con 'let' y solo puede asignarse una vez", "usa 'var' si necesitas cambiar su valor", "E0213");
                        lv->initialized = true;
                    }
                }
            } else if (sym && (id->res.kind == VarKind::Global || id->res.kind == VarKind::Builtin)) {
                if (sym->assignOnce)
                    diags.error(id->loc, "'" + id->name + "' se declaró con 'let' y solo puede asignarse una vez", "usa 'var' si necesitas cambiar su valor", "E0213");
                switch (sym->kind) {
                    case SymKind::Constant: diags.error(id->loc, "No se puede reasignar la constante '" + id->name + "'", "usa 'var' si necesitas cambiar su valor", "E0212"); break;
                    case SymKind::Function: case SymKind::Class: case SymKind::Struct: case SymKind::Enum: case SymKind::Event:
                    case SymKind::Interface: case SymKind::Module:
                        diags.error(id->loc, "No se puede asignar a '" + id->name + "' (es " +
                                     std::string(sym->kind == SymKind::Function ? "una función" : sym->kind == SymKind::Module ? "un módulo" : "un tipo") + ")", "", "E0214");
                        break;
                    default:
                        if (id->res.kind == VarKind::Builtin) diags.error(id->loc, "No se puede reasignar el integrado '" + id->name + "'", "", "E0214");
                        else if (sym->detail.rfind("let ", 0) == 0 && !fn->isScript)
                            diags.error(id->loc, "'" + id->name + "' se declaró con 'let' y solo puede asignarse una vez", "", "E0213");
                        break;
                }
            } else if (id->res.kind == VarKind::ThisMember || id->res.kind == VarKind::StaticMember) {
                if (currentClass) if (auto* m = currentClass->find(id->name)) {
                    if (m->declKind == DeclKind::Const && !(fn->decl && fn->decl->kind == FuncKind::Constructor))
                        diags.error(id->loc, "El campo '" + id->name + "' es constante", "", "E0212");
                    if (m->kind == MemberInfo::Method) diags.error(id->loc, "No se puede asignar al método '" + id->name + "'", "", "E0214");
                }
            }
            if (!compound && t) checkAssignable(t, vt, loc, "la asignación a '" + id->name + "'");
            break;
        }
        case ExprKind::Member: {
            auto* m = static_cast<MemberExpr*>(target);
            TypeP ot = expr(m->object.get());
            Symbol* sym = nullptr;
            TypeP t = memberType(ot, m->name, m->nameLoc, true, &sym);
            if (sym) { m->sym = sym; addRef(m->nameLoc, sym); }
            if (ot->kind == TK::Module && ot->mod) {
                auto it = ot->mod->members.find(m->name);
                if (it != ot->mod->members.end() && (it->second.readonly || it->second.kind == MemberInfo::Method || it->second.kind == MemberInfo::EventM))
                    diags.error(m->nameLoc, "'" + ot->mod->name + "." + m->name + "' es de solo lectura", "", "E0215");
            } else if ((ot->kind == TK::Instance || ot->kind == TK::Class) && ot->cls) {
                if (const MemberInfo* mi = ot->cls->find(m->name)) {
                    if (mi->kind == MemberInfo::Method) diags.error(m->nameLoc, "No se puede asignar al método '" + m->name + "'", "", "E0214");
                    else if (mi->readonly && !(mi->kind == MemberInfo::Field && fn->decl && fn->decl->kind == FuncKind::Constructor))
                        diags.error(m->nameLoc, "'" + m->name + "' es de solo lectura", "", "E0215");
                    else if (mi->kind == MemberInfo::Getter && !mi->hasSetter)
                        diags.error(m->nameLoc, "La propiedad '" + m->name + "' solo tiene 'get' (es de solo lectura)", "añade: set " + m->name + "(valor) { ... }", "E0215");
                }
            } else if (ot->kind == TK::Enum || ot->kind == TK::EnumValue) {
                diags.error(m->nameLoc, "Los valores de un enum no se pueden cambiar", "", "E0215");
            }
            if (!compound) checkAssignable(t, vt, loc, "la asignación a '" + m->name + "'");
            break;
        }
        case ExprKind::Index: {
            auto* ix = static_cast<IndexExpr*>(target);
            TypeP ot = expr(ix->object.get());
            expr(ix->index.get());
            if (ot->kind == TK::Text) diags.error(ix->loc, "Los textos no se pueden modificar por índice", "crea un texto nuevo, p. ej. con replace()", "E0216");
            break;
        }
        default:
            expr(target);
            break;
    }
}

TypeP Analyzer::expr(Expr* e) {
    TypeP t = tAny();
    switch (e->kind) {
        case ExprKind::Number: t = static_cast<NumberExpr*>(e)->isInt ? tInt() : tNum(); break;
        case ExprKind::String: {
            for (auto& p : static_cast<StringExpr*>(e)->parts) {
                if (!p.isExpr || !p.expr) continue;
                TypeP pt = expr(p.expr.get());
                if (pt->kind == TK::Void) diags.error(p.expr->loc, "Esta expresión no devuelve ningún valor (void)", "", "E0303");
            }
            t = tText();
            break;
        }
        case ExprKind::Bool: t = tBool(); break;
        case ExprKind::Null: t = tNull(); break;
        case ExprKind::Ident: {
            auto* id = static_cast<IdentExpr*>(e);
            Symbol* sym = nullptr;
            TypeP it;
            if (!resolveName(id->name, id->loc, id->res, sym, it, false)) {
                std::string sug = closestMatch(id->name, visibleNames());
                std::string hint = sug.empty() ? "" : "¿quisiste decir '" + sug + "'?";
                if (id->name == "self") hint = "en HyperEZ se usa 'this'";
                else if (id->name == "True" || id->name == "False") hint = "en HyperEZ se escribe en minúsculas: true / false";
                else if (id->name == "None" || id->name == "nil" || id->name == "undefined") hint = "en HyperEZ se usa 'null'";
                diags.error(id->loc, "No existe nada llamado '" + id->name + "'", hint, "E0205");
                t = tAny();
                break;
            }
            id->sym = sym;
            if (sym) { sym->used = true; addRef(id->loc, sym); }
            if (id->res.kind == VarKind::Local) {
                auto& lv = fn->locals[id->res.index];
                if (!lv.initialized && lv.declKind == DeclKind::Let)
                    diags.error(id->loc, "'" + id->name + "' se usa antes de asignarle un valor", "", "E0217");
            }
            t = it ? it : tAny();
            break;
        }
        case ExprKind::This: {
            auto* th = static_cast<ThisExpr*>(e);
            if (!currentClass || !resolveThis(th->res)) {
                diags.error(e->loc, "'this' solo puede usarse dentro de los métodos de una clase", "", "E0218");
                th->res.kind = VarKind::Unresolved;
            }
            t = currentClass ? tInstance(currentClass) : tAny();
            break;
        }
        case ExprKind::Super: {
            auto* s = static_cast<SuperExpr*>(e);
            if (!currentClass || !currentClass->super) { diags.error(e->loc, "'super' solo puede usarse en clases que heredan de otra", "", "E0510"); break; }
            if (!resolveThis(s->thisRes)) { diags.error(e->loc, "'super' no está disponible aquí", "", "E0510"); break; }
            Symbol* sym = nullptr;
            t = memberType(tInstance(currentClass->super), s->member, e->loc, true, &sym);
            break;
        }
        case ExprKind::List: {
            auto* l = static_cast<ListExpr*>(e);
            TypeP elem;
            bool uniform = true;
            for (size_t i = 0; i < l->items.size(); i++) {
                TypeP it = widen(expr(l->items[i].get()));
                if (l->spread[i]) { uniform = false; continue; }
                if (!elem) elem = it;
                else if (typeStr(elem) != typeStr(it)) uniform = false;
            }
            t = tList(uniform && elem && elem->kind != TK::Null ? elem : tAny());
            break;
        }
        case ExprKind::Map: {
            auto* m = static_cast<MapExpr*>(e);
            for (auto& [k, v] : m->entries) { expr(k.get()); expr(v.get()); }
            t = tMap(tAny(), tAny());
            break;
        }
        case ExprKind::Unary: {
            auto* u = static_cast<UnaryExpr*>(e);
            TypeP o = expr(u->operand.get());
            if (u->op == Tok::KwNot) t = tBool();
            else {
                if (isKnown(o) && !isNumeric(o) && !(o->kind == TK::Instance && o->cls && o->cls->isNative && u->op == Tok::Minus))
                    diags.error(u->operand->loc, std::string("El operador '") + (u->op == Tok::Minus ? "-" : u->op == Tok::Tilde ? "~" : "+") + "' necesita un número, no " + typeStr(o), "", "E0320");
                t = (o->kind == TK::Instance) ? o : (o->kind == TK::Int ? tInt() : tNum());
            }
            break;
        }
        case ExprKind::Binary: {
            auto* b = static_cast<BinaryExpr*>(e);
            TypeP l = expr(b->left.get()), r = expr(b->right.get());
            auto mismatch = [&](const char* opWord) {
                diags.error(b->loc, std::string("No se puede ") + opWord + " " + typeStr(widen(l)) + " y " + typeStr(widen(r)),
                            (l->kind == TK::Text || r->kind == TK::Text) ? "para convertir texto a número usa toNumber(...)" : "", "E0321");
            };
            bool lk = isKnown(l) && !l->nullable, rk = isKnown(r) && !r->nullable;
            auto isVec = [](const TypeP& x) { return x->kind == TK::Instance && x->cls && x->cls->isNative && (x->cls->name == "Vec2" || x->cls->name == "Vec3" || x->cls->name == "Vec4" || x->cls->name == "Color" || x->cls->name == "Quat"); };
            // Sobrecarga de operadores: la clase define add/sub/mul/div/mod/pow (o compareTo para <, >...)
            auto overload = [&](const TypeP& x, const char* method) -> TypeP {
                if (!x || x->kind != TK::Instance || !x->cls) return nullptr;
                const MemberInfo* m = x->cls->find(method);
                if (!m || m->kind != MemberInfo::Method) return nullptr;
                return m->type && m->type->sig && m->type->sig->ret ? m->type->sig->ret : tAny();
            };
            const char* opMethod = b->op == Tok::Plus ? "add" : b->op == Tok::Minus ? "sub" : b->op == Tok::Star ? "mul"
                                 : b->op == Tok::Slash ? "div" : b->op == Tok::Percent ? "mod" : b->op == Tok::StarStar ? "pow" : nullptr;
            TypeP over = opMethod ? overload(l, opMethod) : nullptr;
            if (!over && opMethod && isNumeric(l) && (b->op == Tok::Plus || b->op == Tok::Star)) over = overload(r, opMethod);
            if (over) { t = over->kind == TK::Void ? tAny() : over; break; }
            if ((b->op == Tok::Less || b->op == Tok::LessEq || b->op == Tok::Greater || b->op == Tok::GreaterEq) && overload(l, "compareTo")) {
                t = tBool();
                break;
            }
            switch (b->op) {
                case Tok::Plus:
                    if (l->kind == TK::Text || r->kind == TK::Text) t = tText();
                    else if (isNumeric(l) && isNumeric(r)) t = (l->kind == TK::Int && r->kind == TK::Int) ? tInt() : tNum();
                    else if (l->kind == TK::List && r->kind == TK::List) t = l;
                    else if (isVec(l)) t = l;
                    else if (lk && rk && !isVec(r)) { mismatch("sumar"); t = tAny(); }
                    else t = tAny();
                    break;
                case Tok::Minus: case Tok::Star: case Tok::Slash: case Tok::Percent: case Tok::StarStar: {
                    const char* w = b->op == Tok::Minus ? "restar" : b->op == Tok::Star ? "multiplicar" : b->op == Tok::Slash ? "dividir" : b->op == Tok::Percent ? "calcular el módulo de" : "elevar";
                    if (isNumeric(l) && isNumeric(r)) t = (b->op != Tok::Slash && b->op != Tok::StarStar && l->kind == TK::Int && r->kind == TK::Int) ? tInt() : tNum();
                    else if (isVec(l)) t = l;
                    else if (isVec(r) && isNumeric(l)) t = r;
                    else if (b->op == Tok::Star && l->kind == TK::Text && isNumeric(r)) t = tText();
                    else if (lk && rk) { mismatch(w); t = tAny(); }
                    else t = (isKnown(l) && isNumeric(l)) || (isKnown(r) && isNumeric(r)) ? tNum() : tAny();
                    break;
                }
                case Tok::Less: case Tok::LessEq: case Tok::Greater: case Tok::GreaterEq:
                    if (lk && rk && !((isNumeric(l) && isNumeric(r)) || (l->kind == TK::Text && r->kind == TK::Text)))
                        diags.error(b->loc, "No se puede comparar " + typeStr(l) + " con " + typeStr(r), "", "E0322");
                    t = tBool();
                    break;
                case Tok::EqEq: case Tok::NotEq:
                    if (lk && rk) {
                        auto prim = [](TK k) { return k == TK::Number || k == TK::Int || k == TK::Text || k == TK::Bool; };
                        if (prim(l->kind) && prim(r->kind) && !(isNumeric(l) && isNumeric(r)) && l->kind != r->kind)
                            diags.warning(b->loc, "Comparar " + typeStr(l) + " con " + typeStr(r) + " siempre da " + (b->op == Tok::EqEq ? "false" : "true"), "", "W0306");
                    }
                    t = tBool();
                    break;
                case Tok::KwIn:
                    if (rk && (isNumeric(r) || r->kind == TK::Bool))
                        diags.error(b->right->loc, "'in' necesita una lista, mapa, conjunto, texto o rango a la derecha", "", "E0323");
                    t = tBool();
                    break;
                case Tok::Amp: case Tok::Pipe: case Tok::Caret: case Tok::Shl: case Tok::Shr:
                    if ((lk && !isNumeric(l)) || (rk && !isNumeric(r))) diags.error(b->loc, "Los operadores de bits necesitan números", "para lógica usa 'and' / 'or'", "E0324");
                    t = tInt();
                    break;
                default: break;
            }
            break;
        }
        case ExprKind::Logical: {
            auto* l = static_cast<LogicalExpr*>(e);
            TypeP a = expr(l->left.get()), b = expr(l->right.get());
            if (l->op == Tok::QuestionQuestion) {
                if (a->kind == TK::Null) t = b;
                else if (isKnown(a)) { t = std::make_shared<Type>(*a); t->nullable = b->nullable; }
                else t = b;
            } else t = (a->kind == TK::Bool && b->kind == TK::Bool) ? tBool() : tAny();
            break;
        }
        case ExprKind::Ternary: {
            auto* tr = static_cast<TernaryExpr*>(e);
            expr(tr->cond.get());
            TypeP a = expr(tr->then.get()), b = expr(tr->otherwise.get());
            if (typeStr(widen(a)) == typeStr(widen(b))) t = widen(a);
            else if (a->kind == TK::Null && isKnown(b)) t = nullableOf(b);
            else if (b->kind == TK::Null && isKnown(a)) t = nullableOf(a);
            else t = tAny();
            break;
        }
        case ExprKind::Call: t = call(static_cast<CallExpr*>(e)); break;
        case ExprKind::Member: t = member(static_cast<MemberExpr*>(e)); break;
        case ExprKind::Index: {
            auto* ix = static_cast<IndexExpr*>(e);
            TypeP ot = expr(ix->object.get());
            TypeP it = expr(ix->index.get());
            switch (ot->kind) {
                case TK::List:
                    if (isKnown(it) && !isNumeric(it) && it->kind != TK::Range) diags.error(ix->index->loc, "El índice de una lista debe ser un número, no " + typeStr(it), "", "E0325");
                    t = it->kind == TK::Range ? ot : ot->args[0];
                    break;
                case TK::Text: t = tText(); break;
                case TK::Map: t = ot->args[1]; break;
                case TK::Number: case TK::Int: case TK::Bool:
                    diags.error(ix->loc, "No se puede indexar un valor de tipo " + typeStr(ot), "", "E0325");
                    break;
                default: break;
            }
            if (ix->optional) t = nullableOf(t);
            break;
        }
        case ExprKind::Lambda: {
            auto* l = static_cast<LambdaExpr*>(e);
            function(l->fn.get(), currentClass);
            t = tFunc(sigOf(l->fn.get()));
            break;
        }
        case ExprKind::Range: {
            auto* r = static_cast<RangeExpr*>(e);
            for (Expr* x : {r->start.get(), r->end.get(), r->step.get()}) {
                if (!x) continue;
                TypeP xt = expr(x);
                if (isKnown(xt) && !isNumeric(xt)) diags.error(x->loc, "Los límites de un rango deben ser números", "", "E0306");
            }
            t = tRange();
            break;
        }
        case ExprKind::Await: {
            auto* a = static_cast<AwaitExpr*>(e);
            TypeP o = expr(a->operand.get());
            bool ok = false;
            FnCtx* f = fn;
            if (f->isScript) ok = !unit.isPrelude;
            else if (f->decl->isAsync || f->decl->kind == FuncKind::Handler) ok = true;
            if (f->decl && !f->isScript) f->decl->containsAwait = true;
            if (!ok)
                diags.error(e->loc, "'await' solo puede usarse dentro de funciones 'async'",
                            fn->decl && !fn->decl->name.empty() ? "declara la función como: async function " + fn->decl->name + "(...)" : "marca la función como async", "E0219");
            t = o->kind == TK::Task ? (o->args.empty() ? tAny() : o->args[0]) : o;
            break;
        }
        case ExprKind::Is: {
            auto* is = static_cast<IsExpr*>(e);
            expr(is->operand.get());
            static const std::set<std::string> extra = {"number", "int", "text", "bool", "list", "map", "set", "function", "null", "Range", "Task", "Event", "Error", "any", "object"};
            if (!extra.count(is->target->name)) resolveTypeRef(is->target.get());
            t = tBool();
            break;
        }
        case ExprKind::Match: {
            auto* m = static_cast<MatchExpr*>(e);
            bool allowBlocks = matchBlocksAllowed;
            matchBlocksAllowed = false;
            expr(m->subject.get());
            TypeP common;
            bool same = true, hasElse = false;
            for (auto& arm : m->arms) {
                if (arm.isElse) hasElse = true;
                for (auto& p : arm.patterns) expr(p.get());
                if (arm.guard) expr(arm.guard.get());
                if (arm.hasBlock) {
                    beginScope(arm.loc.offset);
                    checkUnreachable(arm.block);
                    // La última expresión del bloque es el valor del caso: match x { 1 => { log(); "uno" } }
                    if (!arm.block.empty() && arm.block.back()->kind == StmtKind::Expr) {
                        auto* last = static_cast<ExprStmt*>(arm.block.back().get());
                        if (last->expr->kind != ExprKind::Assign && last->expr->kind != ExprKind::IncDec) last->isBlockValue = true;
                    }
                    for (auto& s : arm.block) stmt(s.get());
                    bool declared = !fn->locals.empty() && fn->locals.back().depth == fn->scopeDepth;
                    if (declared && !allowBlocks)
                        diags.error(arm.loc, "No se pueden declarar variables dentro de un caso de 'match' usado en medio de una expresión",
                                    "guarda el resultado en una variable: var r = match x { ... }", "E0326");
                    auto* holder = new BlockStmt(arm.loc);
                    endScope(holder, arm.loc.offset + 1);
                    // Guardar los locales del brazo en un bloque sintético al inicio de la lista
                    auto blk = std::unique_ptr<BlockStmt>(holder);
                    blk->body = std::move(arm.block);
                    arm.block.clear();
                    arm.block.push_back(std::move(blk));
                    same = false;
                } else if (arm.body) {
                    TypeP at = widen(expr(arm.body.get()));
                    if (!common) common = at;
                    else if (typeStr(common) != typeStr(at)) same = false;
                }
            }
            if (!hasElse && !m->arms.empty() && !allowBlocks && same && common)
                common = nullableOf(common);
            t = (same && common) ? common : tAny();
            matchBlocksAllowed = allowBlocks;
            break;
        }
        case ExprKind::Assign: {
            auto* a = static_cast<AssignExpr*>(e);
            bool savedM = matchBlocksAllowed;
            matchBlocksAllowed = a->value->kind == ExprKind::Match && a->target->kind == ExprKind::Ident;
            TypeP vt = expr(a->value.get());
            matchBlocksAllowed = savedM;
            if (vt->kind == TK::Void) diags.error(a->value->loc, "Esta expresión no devuelve ningún valor (void)", "", "E0303");
            bool compound = a->op != Tok::Eq;
            assignTo(a->target.get(), vt, a->value->loc, compound);
            if (compound && a->op != Tok::QuestionQuestionEq) {
                TypeP tt = a->target->kind == ExprKind::Ident ? static_cast<IdentExpr*>(a->target.get())->sym ? static_cast<IdentExpr*>(a->target.get())->sym->type : tAny() : tAny();
                if (tt && isKnown(tt) && isKnown(vt)) {
                    if (a->op == Tok::PlusEq) {
                        if (!(tt->kind == TK::Text || (isNumeric(tt) && isNumeric(vt)) || tt->kind == TK::List || tt->kind == TK::Instance))
                            diags.error(a->loc, "No se puede usar '+=' con " + typeStr(tt) + " y " + typeStr(vt), "", "E0321");
                    } else if (!(isNumeric(tt) && isNumeric(vt)) && tt->kind != TK::Instance) {
                        diags.error(a->loc, "Este operador necesita números (" + typeStr(tt) + ", " + typeStr(vt) + ")", "", "E0321");
                    }
                }
            }
            t = vt;
            break;
        }
        case ExprKind::IncDec: {
            auto* i = static_cast<IncDecExpr*>(e);
            assignTo(i->target.get(), tNum(), i->loc, true);
            if (i->target->kind == ExprKind::Ident) {
                auto* id = static_cast<IdentExpr*>(i->target.get());
                if (id->sym) {
                    id->sym->used = true;
                    if (id->sym->type && isKnown(id->sym->type) && !isNumeric(id->sym->type))
                        diags.error(i->loc, std::string("'") + (i->inc ? "++" : "--") + "' necesita un número y '" + id->name + "' es " + typeStr(id->sym->type), "", "E0320");
                }
            }
            t = tNum();
            break;
        }
        case ExprKind::On: {
            auto* o = static_cast<OnExpr*>(e);
            TypeP et = expr(o->event.get());
            if (o->priority) expr(o->priority.get());
            if (isKnown(et) && et->kind != TK::Event)
                diags.error(o->event->loc, "'on' necesita un evento, pero esto es " + typeStr(et), "declara el evento con: event Nombre(param: tipo)", "E0408");
            // Tipo del parámetro: datos del evento con sus campos
            if (et->kind == TK::Event && et->ev) {
                std::string key = "@evento:" + et->ev->name + ":" + std::to_string((uintptr_t)et->ev);
                ClassInfo* payload = nullptr;
                for (auto& c : unit.classes) if (c->name == key) payload = c.get();
                if (!payload) {
                    auto ci = std::make_unique<ClassInfo>();
                    ci->name = key;
                    ci->isNative = true;
                    if (auto* base = world.findClass("EventData")) ci->super = base;
                    for (auto& p : et->ev->params) {
                        MemberInfo mi;
                        mi.name = p.name; mi.kind = MemberInfo::Field; mi.type = p.type; mi.readonly = true;
                        mi.sym = unit.newSymbol(p.name, SymKind::Field, et->ev->sym ? et->ev->sym->loc : SrcLoc{});
                        mi.sym->detail = p.name + ": " + typeStr(p.type);
                        mi.sym->container = "event " + et->ev->name;
                        mi.sym->file = et->ev->sym ? et->ev->sym->file : "";
                        mi.sym->builtin = !et->ev->sym || et->ev->sym->builtin;
                        ci->members[p.name] = mi;
                    }
                    payload = ci.get();
                    unit.classes.push_back(std::move(ci));
                }
                auto& prm = o->handler->params[0];
                pendingHandlerType = tInstance(payload);
                function(o->handler.get(), currentClass);
                if (prm.sym) prm.sym->detail = prm.name + ": evento " + et->ev->name;
            } else {
                function(o->handler.get(), currentClass);
            }
            t = tSub();
            break;
        }
    }
    e->type = t;
    return t;
}

} // namespace hz
