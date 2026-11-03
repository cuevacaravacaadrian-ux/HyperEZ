// Valores y objetos del runtime de HyperEZ.
#pragma once

#include "../core/common.h"
#include "bytecode.h"

#include <cmath>
#include <unordered_set>

namespace hz {

struct Obj;
struct ObjString;
class VM;

enum class VT : uint8_t { Null, Bool, Num, Obj, Undef };

struct Value {
    VT t = VT::Null;
    union { bool b; double n; Obj* o; };
    Value() : n(0) {}
    Value(double d) : t(VT::Num), n(d) {}
    Value(int i) : t(VT::Num), n((double)i) {}
    static Value boolean(bool v) { Value r; r.t = VT::Bool; r.b = v; return r; }
    static Value object(Obj* p) { Value r; r.t = VT::Obj; r.o = p; return r; }
    static Value undef() { Value r; r.t = VT::Undef; return r; }
    static Value null() { return Value(); }
    bool isNull() const { return t == VT::Null; }
    bool isBool() const { return t == VT::Bool; }
    bool isNum() const { return t == VT::Num; }
    bool isObj() const { return t == VT::Obj; }
    bool isUndef() const { return t == VT::Undef; }
};

enum class OT : uint8_t {
    String, Function, Closure, Upvalue, Native, Bound, Class, Instance, List, Map, Range, Module,
    Enum, EnumValue, Event, Subscription, Fiber, Task, Vec, Foreign,
};

struct Obj {
    OT type;
    bool marked = false;
    Obj* next = nullptr;
    explicit Obj(OT t) : type(t) {}
    virtual ~Obj() = default;
};

inline bool isObjType(const Value& v, OT t) { return v.t == VT::Obj && v.o->type == t; }

struct ObjString : Obj {
    std::string str;
    size_t hash = 0;
    ObjString() : Obj(OT::String) {}
};

struct LineInfo { int offset; int line; int col; };

struct ObjModule;
struct ObjClass;
struct ObjClosure;

struct ObjFunction : Obj {
    std::vector<uint8_t> code;
    std::vector<Value> constants;
    std::vector<LineInfo> lines;
    ObjString* name = nullptr;
    ObjString* className = nullptr;
    ObjModule* module = nullptr;
    std::vector<ObjString*> paramNames;
    int arity = 0;      // parámetros declarados (sin contar el ...resto)
    int minArity = 0;   // obligatorios
    bool hasRest = false;
    bool isAsync = false;
    bool isMethod = false; // slot 0 = this
    bool isCtor = false;   // RETURN devuelve this
    int upvalueCount = 0;
    int maxSlots = 0;
    // Caché en línea de OP_INVOKE: (clase del receptor, método). El GC marca ambos.
    struct InvokeCache { ObjClass* cls = nullptr; ObjClosure* method = nullptr; };
    int invokeCacheSize = 0;
    std::vector<InvokeCache> invokeCache;
    ObjFunction() : Obj(OT::Function) {}
    SrcLoc locAt(size_t offset) const;
};

struct ObjFiber;

struct ObjUpvalue : Obj {
    ObjFiber* fiber = nullptr;
    int slot = 0;
    bool open = true;
    Value closed;
    ObjUpvalue* nextOpen = nullptr;
    ObjUpvalue() : Obj(OT::Upvalue) {}
};

struct ObjClosure : Obj {
    ObjFunction* fn;
    std::vector<ObjUpvalue*> upvalues;
    ObjClass* owner = nullptr; // clase en la que se definió (para super)
    explicit ObjClosure(ObjFunction* f) : Obj(OT::Closure), fn(f) {}
};

// Las funciones nativas reciben args[0..argc) y en args[-1] el receptor o la propia función.
using NativeFn = Value (*)(VM& vm, int argc, Value* args);

struct ObjNative : Obj {
    NativeFn fn = nullptr;
    ObjString* name = nullptr;
    int minArgs = 0, maxArgs = -1;
    void* data = nullptr;
    bool acceptsNamed = false; // los argumentos con nombre llegan como mapa final
    ObjNative() : Obj(OT::Native) {}
};

struct ObjBound : Obj {
    Value receiver, method;
    ObjBound() : Obj(OT::Bound) {}
};

struct NativeProp { NativeFn get = nullptr; NativeFn set = nullptr; };

struct PtrHash { size_t operator()(const ObjString* s) const { return std::hash<const void*>()(s); } };
template <class V> using StrMap = std::unordered_map<ObjString*, V, PtrHash>;

struct ObjClass : Obj {
    ObjString* name = nullptr;
    ObjClass* super = nullptr;
    StrMap<Value> methods;   // instancia (incluye heredados)
    StrMap<Value> statics;   // estáticos (métodos y campos)
    StrMap<Value> getters, setters, staticGetters, staticSetters;
    std::vector<ObjString*> fieldNames;
    StrMap<int> fieldIndex;
    std::vector<Value> fieldInits; // cadena de inicializadores (base -> derivada)
    Value ctor;              // constructor propio o heredado
    std::vector<ObjString*> interfaces;
    bool isStruct = false;
    bool isNative = false;   // clase integrada (Vec2, Texture, Error...)
    bool isInterface = false;
    NativeFn nativeCtor = nullptr;
    OT instanceType = OT::Instance; // tipo de objeto que crea (Vec, Foreign...)
    int vecKind = -1;        // para clases de vectores
    ObjModule* module = nullptr;
    ObjClass() : Obj(OT::Class) {}
    bool isSubclassOf(const ObjClass* other) const {
        for (auto* c = this; c; c = c->super) if (c == other) return true;
        return false;
    }
    bool implements(const ObjString* iface) const {
        for (auto* c = this; c; c = c->super)
            for (auto* i : c->interfaces) if (i == iface) return true;
        return false;
    }
};

struct ObjInstance : Obj {
    ObjClass* cls;
    std::vector<Value> fields;
    std::unique_ptr<StrMap<Value>> extra; // propiedades no declaradas
    explicit ObjInstance(ObjClass* c) : Obj(OT::Instance), cls(c) {}
};

struct ObjList : Obj {
    std::vector<Value> items;
    ObjList() : Obj(OT::List) {}
};

struct ValueHash { size_t operator()(const Value& v) const; };
struct ValueKeyEq { bool operator()(const Value& a, const Value& b) const; };

struct ObjMap : Obj {
    struct Entry { Value key, value; bool alive = true; };
    std::vector<Entry> entries;
    std::unordered_map<Value, int, ValueHash, ValueKeyEq> index;
    int live = 0;
    bool isSet = false;
    ObjMap() : Obj(OT::Map) {}
    Value* find(const Value& k) {
        auto it = index.find(k);
        return it == index.end() ? nullptr : &entries[it->second].value;
    }
    void set(const Value& k, const Value& v) {
        auto it = index.find(k);
        if (it != index.end()) { entries[it->second].value = v; return; }
        index[k] = (int)entries.size();
        entries.push_back({k, v, true});
        live++;
    }
    bool remove(const Value& k) {
        auto it = index.find(k);
        if (it == index.end()) return false;
        entries[it->second].alive = false;
        entries[it->second].key = Value();
        entries[it->second].value = Value();
        index.erase(it);
        live--;
        if (entries.size() > 32 && live < (int)entries.size() / 2) compact();
        return true;
    }
    void compact() {
        std::vector<Entry> e;
        e.reserve(live);
        index.clear();
        for (auto& x : entries) if (x.alive) { index[x.key] = (int)e.size(); e.push_back(x); }
        entries.swap(e);
    }
    void clear() { entries.clear(); index.clear(); live = 0; }
};

struct ObjRange : Obj {
    double start = 0, end = 0, step = 1;
    bool inclusive = false;
    ObjRange() : Obj(OT::Range) {}
    double count() const {
        if (step == 0) return 0;
        double span = (end - start) / step;
        if (span < 0) return 0;
        double n = std::floor(span);
        if (inclusive) return n + 1;
        return (n == span) ? n : n + 1;
    }
};

struct ObjModule : Obj {
    ObjString* name = nullptr;
    std::string path;
    std::vector<Value> globals;
    std::vector<ObjString*> globalNames;
    StrMap<int> exports;           // nombre -> índice en globals (públicos)
    StrMap<int> allGlobals;        // nombre -> índice (todos)
    StrMap<NativeProp> props;      // propiedades nativas (Window.width...)
    bool isNative = false;
    bool initialized = false;
    std::string doc;
    ObjModule() : Obj(OT::Module) {}
};

struct ObjEnumValue;
struct ObjEnum : Obj {
    ObjString* name = nullptr;
    std::vector<ObjEnumValue*> values;
    StrMap<ObjEnumValue*> byName;
    ObjEnum() : Obj(OT::Enum) {}
};

struct ObjEnumValue : Obj {
    ObjEnum* owner = nullptr;
    ObjString* name = nullptr;
    Value value;
    int ordinal = 0;
    ObjEnumValue() : Obj(OT::EnumValue) {}
};

struct ObjSubscription;
struct ObjEvent : Obj {
    ObjString* name = nullptr;
    std::vector<ObjString*> params;
    std::vector<ObjSubscription*> subs;
    int dispatching = 0;
    bool needsCleanup = false;
    uint64_t emitCount = 0;
    ObjEvent() : Obj(OT::Event) {}
};

struct ObjSubscription : Obj {
    ObjEvent* event = nullptr;
    Value handler;
    double priority = 0;
    uint64_t order = 0;
    bool once = false;
    bool active = true;
    Value owner; // objeto dueño opcional (se cancela al destruirlo)
    ObjSubscription() : Obj(OT::Subscription) {}
};

struct CallFrame {
    ObjClosure* closure;
    const uint8_t* ip;
    int base;
};

struct Handler {
    int frameIndex;
    const uint8_t* catchIp;
    int stackHeight;
};

enum class FiberState : uint8_t { New, Running, Suspended, Done, Failed };

struct ObjTask;
struct ObjFiber : Obj {
    std::vector<Value> stack;
    int sp = 0;
    std::vector<CallFrame> frames;
    std::vector<Handler> handlers;
    ObjUpvalue* openUpvalues = nullptr;
    FiberState state = FiberState::New;
    ObjTask* task = nullptr;
    ObjTask* waitingOn = nullptr;
    Value closure;               // función de entrada
    ObjFiber() : Obj(OT::Fiber) { stack.resize(256); }
    void ensure(int extra) {
        if (sp + extra + 8 > (int)stack.size()) {
            size_t n = stack.size();
            while ((int)n < sp + extra + 8) n *= 2;
            stack.resize(n);
        }
    }
};

enum class TaskState : uint8_t { Pending, Done, Failed, Cancelled };

struct ObjTask : Obj {
    TaskState state = TaskState::Pending;
    ObjFiber* fiber = nullptr;
    Value result;
    Value error;
    std::vector<ObjFiber*> waiters;
    std::vector<Value> callbacks; // funciones .then()
    double wakeAt = -1;           // para wait()
    bool cancelRequested = false;
    bool errorObserved = false;
    ObjString* label = nullptr;
    ObjTask() : Obj(OT::Task) {}
};

enum VecKind : int { VK_VEC2 = 0, VK_VEC3 = 1, VK_VEC4 = 2, VK_COLOR = 3, VK_QUAT = 4 };

struct ObjVec : Obj {
    int kind = VK_VEC3;
    double v[4] = {0, 0, 0, 0};
    ObjVec() : Obj(OT::Vec) {}
    int dims() const { return kind == VK_VEC2 ? 2 : kind == VK_VEC3 ? 3 : 4; }
};

// Recurso nativo (textura, sonido, modelo...) con finalizador.
struct ObjForeign : Obj {
    ObjClass* cls = nullptr;
    void* ptr = nullptr;
    void (*finalize)(void*) = nullptr;
    std::vector<Value> refs; // valores retenidos (marcados por el GC)
    ObjForeign() : Obj(OT::Foreign) {}
    ~ObjForeign() override { if (finalize && ptr) finalize(ptr); }
};

// ---- Utilidades sobre valores ------------------------------------------------

inline ObjString* asString(const Value& v) { return static_cast<ObjString*>(v.o); }
inline bool isString(const Value& v) { return isObjType(v, OT::String); }
inline bool isFalsy(const Value& v) {
    switch (v.t) {
        case VT::Null: case VT::Undef: return true;
        case VT::Bool: return !v.b;
        case VT::Num: return v.n == 0 || std::isnan(v.n);
        case VT::Obj: return v.o->type == OT::String && asString(v)->str.empty();
    }
    return false;
}
bool valuesEqual(const Value& a, const Value& b, int depth = 0);

} // namespace hz
