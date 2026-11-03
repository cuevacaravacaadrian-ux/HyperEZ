// Análisis semántico: resolución de nombres, tipos graduales, diagnósticos
// y tabla de símbolos (usada también por el servidor de lenguaje).
#pragma once

#include "ast.h"
#include "api.h"

#include <map>

namespace hz {

class VM;
struct ObjModule;
struct ObjFunction;
struct ReplSession;

enum class TK : uint8_t {
    Any, Void, Null, Bool, Number, Int, Text, List, Map, Set, Func, Instance, Class, Enum, EnumValue,
    Module, Event, Task, Range, Subscription,
};

struct FuncSig;
struct ClassInfo;
struct EnumInfo;
struct ModuleInfo;
struct EventInfo;

struct Type {
    TK kind = TK::Any;
    std::vector<TypeP> args;
    std::shared_ptr<FuncSig> sig;
    ClassInfo* cls = nullptr;
    EnumInfo* en = nullptr;
    ModuleInfo* mod = nullptr;
    EventInfo* ev = nullptr;
    bool nullable = false;
    bool placeholder = false; // "T": tipo del elemento de la colección receptora
};

struct SigParam { std::string name; TypeP type; bool optional = false; bool rest = false; std::string def; };
struct FuncSig {
    std::vector<SigParam> params;
    TypeP ret;
    bool isAsync = false;
    bool acceptsNamedOptions = false; // nativo: argumentos con nombre -> mapa de opciones
    int minArgs() const { int n = 0; for (auto& p : params) if (!p.optional && !p.rest) n++; return n; }
    int maxArgs() const { for (auto& p : params) if (p.rest) return -1; return (int)params.size(); }
};

enum class SymKind : uint8_t {
    Variable, Constant, Parameter, Function, Class, Struct, Interface, Enum, EnumMember, Event, Field, Method,
    Property, Module, Library,
};

struct Symbol {
    std::string name;
    SymKind kind = SymKind::Variable;
    std::string file;
    SrcLoc loc;
    std::string detail;   // firma o tipo legible
    std::string doc;
    TypeP type;
    std::string container; // clase o módulo dueño
    bool builtin = false;
    bool used = false;
    bool isStatic = false;
    bool assignOnce = false; // 'let' global con valor inicial
    Visibility vis = Visibility::Public;
    int scopeStart = 0, scopeEnd = 1 << 30; // rango de visibilidad (offsets)
};

struct MemberInfo {
    std::string name;
    enum Kind : uint8_t { Field, Method, Getter, Setter, Property, Constant, EventM, Submodule } kind = Field;
    TypeP type;           // tipo del campo o tipo función del método
    bool isStatic = false;
    bool readonly = false;
    bool hasSetter = false;
    Visibility vis = Visibility::Public;
    DeclKind declKind = DeclKind::Var;
    Symbol* sym = nullptr;
};

struct ClassInfo {
    std::string name;
    std::string module;
    ClassInfo* super = nullptr;
    std::vector<ClassInfo*> interfaces;
    bool isInterface = false, isStruct = false, isNative = false;
    std::map<std::string, MemberInfo> members;
    std::shared_ptr<FuncSig> ctor;   // nullptr = sin constructor propio
    Symbol* sym = nullptr;
    std::string doc;
    const MemberInfo* find(const std::string& n) const {
        for (auto* c = this; c; c = c->super) {
            auto it = c->members.find(n);
            if (it != c->members.end()) return &it->second;
        }
        return nullptr;
    }
    std::shared_ptr<FuncSig> findCtor() const {
        for (auto* c = this; c; c = c->super) if (c->ctor) return c->ctor;
        return nullptr;
    }
    bool isSubclassOf(const ClassInfo* o) const {
        for (auto* c = this; c; c = c->super) {
            if (c == o) return true;
            for (auto* i : c->interfaces) if (i == o) return true;
        }
        return false;
    }
};

struct EnumInfo { std::string name; std::vector<std::string> members; Symbol* sym = nullptr; std::map<std::string, Symbol*> memberSyms; };
struct EventInfo { std::string name; std::vector<SigParam> params; Symbol* sym = nullptr; };

struct ModuleInfo {
    std::string name;
    bool isNative = false;
    std::map<std::string, MemberInfo> members;
    std::string doc;
    struct ModuleUnit* unit = nullptr;
};

// Alcance léxico registrado para el autocompletado.
struct ScopeRecord { int start = 0, end = 0; std::vector<Symbol*> symbols; };
// Referencia de un identificador a su símbolo (hover, definición, referencias, renombrar).
struct RefRecord { SrcLoc loc; Symbol* sym; bool isDecl = false; };

// Unidad de compilación: un archivo .hz con su AST, símbolos y bytecode.
struct ModuleUnit {
    std::string path;
    std::string name;
    std::unique_ptr<Program> program;
    DiagnosticBag diags;
    ObjModule* module = nullptr;
    ObjFunction* script = nullptr;
    bool analyzing = false, analyzed = false, compiled = false;
    bool isPrelude = false;
    ReplSession* repl = nullptr; // entrada de la consola interactiva
    std::vector<std::unique_ptr<Symbol>> symbols;
    std::vector<ScopeRecord> scopes;
    std::vector<RefRecord> refs;
    std::vector<std::unique_ptr<ClassInfo>> classes;
    std::vector<std::unique_ptr<EnumInfo>> enums;
    std::vector<std::unique_ptr<EventInfo>> events;
    std::unique_ptr<ModuleInfo> info; // vista exportada
    std::map<std::string, int> globalIndex;
    std::vector<std::string> globalNames;
    std::map<std::string, Symbol*> globalSyms;
    std::map<std::string, TypeP> globalTypes;
    std::vector<ModuleUnit*> imports;
    Symbol* newSymbol(const std::string& name, SymKind k, SrcLoc loc);
};

// Mundo estático: tipos y símbolos de la API nativa y del preludio.
struct World {
    std::map<std::string, std::unique_ptr<ClassInfo>> nativeClasses;
    std::map<std::string, std::unique_ptr<ModuleInfo>> nativeModules;
    std::map<std::string, TypeP> builtinTypes;      // nombre global -> tipo
    std::map<std::string, Symbol*> builtinSyms;
    std::vector<std::unique_ptr<Symbol>> symbols;
    std::map<std::string, ClassInfo*> preludeClasses;
    std::map<std::string, EnumInfo*> preludeEnums;
    std::vector<std::unique_ptr<EventInfo>> events;
    ClassInfo* findClass(const std::string& n) const;
};

World& worldOf(VM& vm);
void releaseWorld(VM& vm); // la llama ~VM()
void buildWorld(VM& vm);

// Utilidades de tipos
TypeP tAny();
TypeP tVoid();
TypeP tNull();
TypeP tBool();
TypeP tNum();
TypeP tInt();
TypeP tText();
TypeP tList(TypeP elem = nullptr);
TypeP tMap(TypeP k = nullptr, TypeP v = nullptr);
TypeP tSet(TypeP e = nullptr);
TypeP tTask(TypeP r = nullptr);
TypeP tInstance(ClassInfo* c);
TypeP tFunc(std::shared_ptr<FuncSig> sig);
std::string typeStr(const TypeP& t);
std::string sigStr(const FuncSig& s, const std::string& name = "");
TypeP parseTypeString(VM& vm, const std::string& s);

class Analyzer {
public:
    Analyzer(VM& vm, ModuleUnit& unit);
    void run();

private:
    VM& vm;
    ModuleUnit& unit;
    World& world;
    DiagnosticBag& diags;

    struct LocalVar {
        std::string name;
        int depth;
        bool captured = false;
        bool initialized = true;
        DeclKind declKind = DeclKind::Var;
        TypeP type;
        Symbol* sym = nullptr;
        bool isParam = false;
        bool hidden = false;
    };
    struct FnCtx {
        FnCtx* enclosing = nullptr;
        FuncDecl* decl = nullptr;
        std::vector<LocalVar> locals;
        int scopeDepth = 0;
        ClassInfo* cls = nullptr;
        bool isStatic = false;
        bool isScript = false;
        int loopDepth = 0;
        TypeP retType;
        bool sawReturnValue = false;
        int maxSlots = 1;
    };
    FnCtx* fn = nullptr;
    ClassInfo* currentClass = nullptr;
    std::vector<ScopeRecord*> scopeStack;
    bool matchBlocksAllowed = false;
    TypeP pendingHandlerType;
    int topLevelOffset = 0; // posición de la sentencia de nivel superior actual

    // Declaraciones
    void declareGlobals();
    int declareGlobal(const std::string& name, SrcLoc loc, SymKind kind, TypeP type, const std::string& doc = "");
    void buildClass(ClassStmt* c);
    void linkClass(ClassStmt* c);
    TypeP resolveTypeRef(TypeRef* t);
    std::shared_ptr<FuncSig> sigOf(FuncDecl* f);

    // Ámbitos
    void beginScope(int startOffset);
    void endScope(BlockStmt* block, int endOffset);
    int declareLocal(const std::string& name, SrcLoc loc, SymKind kind, TypeP type, DeclKind dk = DeclKind::Var, bool hidden = false);
    int resolveLocal(FnCtx* f, const std::string& name);
    int resolveUpvalue(FnCtx* f, const std::string& name);
    int addUpvalue(FnCtx* f, bool isLocal, int index, const std::string& name);
    bool resolveName(const std::string& name, SrcLoc loc, Resolution& res, Symbol*& sym, TypeP& type, bool forWrite);
    bool resolveThis(Resolution& res);
    void popScopeLocals(std::vector<LocalInfo>* out);

    // Recorrido
    void function(FuncDecl* f, ClassInfo* cls);
    void stmt(Stmt* s);
    void block(BlockStmt* b, bool newScope = true);
    TypeP expr(Expr* e);
    TypeP call(CallExpr* c);
    TypeP member(MemberExpr* m);
    void assignTo(Expr* target, TypeP valueType, SrcLoc loc, bool compound);
    void checkArgs(const std::string& what, const FuncSig& sig, std::vector<Arg>& args, SrcLoc loc);
    TypeP memberType(TypeP objType, const std::string& name, SrcLoc loc, bool report, Symbol** symOut = nullptr);
    void checkAssignable(TypeP to, TypeP from, SrcLoc loc, const std::string& context);
    bool alwaysReturns(Stmt* s);
    bool blockReturns(const std::vector<StmtP>& body);
    void addRef(SrcLoc loc, Symbol* sym, bool isDecl = false);
    std::vector<std::string> visibleNames();
    void checkUnreachable(const std::vector<StmtP>& body);
    void reportUnused(LocalVar& lv);
};

} // namespace hz
