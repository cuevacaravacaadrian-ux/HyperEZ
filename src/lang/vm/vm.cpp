// VM: memoria, registro de la API, compilación de módulos, eventos y tareas.
#include "lang/vm/vm.h"
#include "lang/sema/analyzer.h"
#include "lang/compiler/codegen.h"
#include "lang/syntax/parser.h"

#include <algorithm>
#include <cstring>
#include <thread>

namespace hz {

// ---------------------------------------------------------------------------
// Valores
// ---------------------------------------------------------------------------

size_t ValueHash::operator()(const Value& v) const {
    switch (v.t) {
        case VT::Null: return 0x9e3779b9;
        case VT::Undef: return 0x7f4a7c15;
        case VT::Bool: return v.b ? 1231 : 1237;
        case VT::Num: { double d = v.n == 0 ? 0.0 : v.n; uint64_t bits; std::memcpy(&bits, &d, 8); return std::hash<uint64_t>()(bits); }
        case VT::Obj: return std::hash<const void*>()(v.o);
    }
    return 0;
}

bool ValueKeyEq::operator()(const Value& a, const Value& b) const {
    if (a.t != b.t) return false;
    switch (a.t) {
        case VT::Null: case VT::Undef: return true;
        case VT::Bool: return a.b == b.b;
        case VT::Num: return a.n == b.n;
        case VT::Obj: return a.o == b.o;
    }
    return false;
}

bool valuesEqual(const Value& a, const Value& b, int depth) {
    if (a.t != b.t) return false;
    switch (a.t) {
        case VT::Null: case VT::Undef: return true;
        case VT::Bool: return a.b == b.b;
        case VT::Num: return a.n == b.n;
        case VT::Obj: break;
    }
    if (a.o == b.o) return true;
    if (a.o->type != b.o->type || depth > 64) return false;
    switch (a.o->type) {
        case OT::String: return false; // internados
        case OT::List: {
            auto& x = static_cast<ObjList*>(a.o)->items;
            auto& y = static_cast<ObjList*>(b.o)->items;
            if (x.size() != y.size()) return false;
            for (size_t i = 0; i < x.size(); i++) if (!valuesEqual(x[i], y[i], depth + 1)) return false;
            return true;
        }
        case OT::Map: {
            auto* x = static_cast<ObjMap*>(a.o);
            auto* y = static_cast<ObjMap*>(b.o);
            if (x->live != y->live || x->isSet != y->isSet) return false;
            for (auto& e : x->entries) {
                if (!e.alive) continue;
                Value* o = y->find(e.key);
                if (!o || !valuesEqual(e.value, *o, depth + 1)) return false;
            }
            return true;
        }
        case OT::Vec: {
            auto* x = static_cast<ObjVec*>(a.o);
            auto* y = static_cast<ObjVec*>(b.o);
            if (x->kind != y->kind) return false;
            for (int i = 0; i < 4; i++) if (x->v[i] != y->v[i]) return false;
            return true;
        }
        case OT::Range: {
            auto* x = static_cast<ObjRange*>(a.o);
            auto* y = static_cast<ObjRange*>(b.o);
            return x->start == y->start && x->end == y->end && x->step == y->step && x->inclusive == y->inclusive;
        }
        case OT::Instance: {
            auto* x = static_cast<ObjInstance*>(a.o);
            auto* y = static_cast<ObjInstance*>(b.o);
            if (!x->cls->isStruct || x->cls != y->cls) return false;
            for (size_t i = 0; i < x->fields.size(); i++) if (!valuesEqual(x->fields[i], y->fields[i], depth + 1)) return false;
            return true;
        }
        default: return false;
    }
}

SrcLoc ObjFunction::locAt(size_t offset) const {
    SrcLoc l;
    for (auto& e : lines) {
        if ((size_t)e.offset > offset) break;
        l.line = e.line; l.col = e.col;
    }
    return l;
}

// ---------------------------------------------------------------------------
// Construcción
// ---------------------------------------------------------------------------

VM::VM() {
    gcEnabled = false;
    s_constructor = intern("constructor");
    s_toString = intern("toString");
    s_message = intern("message");
    s_stack = intern("stack");
    s_name = intern("name");
    s_length = intern("length");
    s_iterator = intern("iterator");
    registerCore();
    gcEnabled = true;
}

VM::~VM() {
    flushOutput();
    releaseWorld(*this);
    Obj* o = objects;
    while (o) { Obj* n = o->next; delete o; o = n; }
    objects = nullptr;
}

void VM::registerCore() {
    auto mk = [&](const char* n) { ObjClass* c = newClass(n); c->isNative = true; return c; };
    textClass = mk("text"); listClass = mk("list"); mapClass = mk("map"); setClass = mk("set");
    numberClass = mk("number"); boolClass = mk("bool"); functionClass = mk("function"); rangeClass = mk("Range");
    taskClass = mk("Task"); eventClass = mk("Event"); subscriptionClass = mk("Subscription"); enumClass = mk("Enum");
    moduleClass = mk("Module"); enumValueClass = mk("EnumValue");
    errorClass = mk("Error");
    errorClass->isNative = false; // se puede heredar de Error
    for (const char* f : {"message", "name", "stack", "file", "line", "column", "cause"}) {
        errorClass->fieldIndex[intern(f)] = (int)errorClass->fieldNames.size();
        errorClass->fieldNames.push_back(intern(f));
    }
    eventDataClass = mk("EventData");
    for (const char* f : {"name", "timestamp", "target", "stopped", "defaultPrevented"}) {
        eventDataClass->fieldIndex[intern(f)] = (int)eventDataClass->fieldNames.size();
        eventDataClass->fieldNames.push_back(intern(f));
    }
    for (auto* c : {textClass, listClass, mapClass, setClass, numberClass, boolClass, functionClass, rangeClass, taskClass,
                    eventClass, subscriptionClass, enumClass, moduleClass, enumValueClass, errorClass, eventDataClass}) {
        addCatalog(ApiKind::Class, "", c->name->str, "", "");
    }
}

// ---------------------------------------------------------------------------
// Memoria
// ---------------------------------------------------------------------------

std::atomic<bool> g_interrupt{false};

// ---------------------------------------------------------------------------
// Asignador de objetos pequeños: una lista libre por cada múltiplo de 16 bytes hasta 256.
// La memoria se reserva en bloques de 64 KB y se reutiliza (no se devuelve al sistema).
// ---------------------------------------------------------------------------
namespace {
constexpr size_t kGranule = 16, kMaxSmall = 256, kChunk = 64 * 1024;
struct FreeNode { FreeNode* next; };
FreeNode* g_free[kMaxSmall / kGranule + 1] = {};
} // namespace

void* Obj::operator new(size_t n) {
    if (n > kMaxSmall) return ::operator new(n);
    size_t c = (n + kGranule - 1) / kGranule;
    if (FreeNode* f = g_free[c]) {
        g_free[c] = f->next;
        return f;
    }
    size_t size = c * kGranule, count = kChunk / size;
    char* block = static_cast<char*>(::operator new(size * count));
    for (size_t i = 1; i < count; i++) {
        auto* node = reinterpret_cast<FreeNode*>(block + i * size);
        node->next = g_free[c];
        g_free[c] = node;
    }
    return block;
}

void Obj::operator delete(void* p, size_t n) {
    if (!p) return;
    if (n > kMaxSmall) { ::operator delete(p); return; }
    auto* node = static_cast<FreeNode*>(p);
    size_t c = (n + kGranule - 1) / kGranule;
    node->next = g_free[c];
    g_free[c] = node;
}

// Hash de los textos internados. Los cortos se recorren enteros; los largos (más de 256 bytes) no:
// se mezclan la longitud, los 64 primeros y 64 últimos bytes y unas 64 muestras repartidas por el
// medio (la misma idea que Lua). Así concatenar a un texto que crece no cuesta O(n) en el hash, solo
// en la copia. Si dos textos largos distintos coinciden en todo lo muestreado, la tabla los distingue
// igualmente comparándolos enteros.
static inline size_t textHash(std::string_view s) {
    if (s.size() <= 256) return std::hash<std::string_view>()(s);
    uint64_t h = 0xcbf29ce484222325ull ^ (uint64_t)s.size();
    auto mix = [&](uint64_t v) { h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); };
    mix(std::hash<std::string_view>()(s.substr(0, 64)));
    mix(std::hash<std::string_view>()(s.substr(s.size() - 64)));
    size_t step = (s.size() - 128) / 64 + 1;
    for (size_t i = 64; i < s.size() - 64; i += step) h = (h ^ (unsigned char)s[i]) * 0x100000001b3ull;
    return (size_t)h;
}

ObjString* VM::intern(std::string_view s) {
    size_t h = textHash(s);
    auto it = strings.find(InternKey{s, h});
    if (it != strings.end()) return it->second;
    ObjString* o = alloc<ObjString>();
    o->str.assign(s.data(), s.size());
    o->hash = h;
    bytesAllocated += s.size();
    strings.emplace(InternKey{o->str, h}, o);
    return o;
}

ObjString* VM::internMove(std::string&& s) {
    size_t h = textHash(s);
    auto it = strings.find(InternKey{s, h});
    if (it != strings.end()) return it->second;
    ObjString* o = alloc<ObjString>();
    o->str = std::move(s);
    o->hash = h;
    bytesAllocated += o->str.size();
    strings.emplace(InternKey{o->str, h}, o);
    return o;
}

ObjList* VM::newList() { return alloc<ObjList>(); }
ObjMap* VM::newMap(bool isSet) { auto* m = alloc<ObjMap>(); m->isSet = isSet; return m; }
ObjVec* VM::newVec(int kind, double x, double y, double z, double w) {
    auto* v = alloc<ObjVec>();
    v->kind = kind; v->v[0] = x; v->v[1] = y; v->v[2] = z; v->v[3] = w;
    return v;
}
ObjInstance* VM::newInstance(ObjClass* cls) {
    auto* i = alloc<ObjInstance>(cls);
    i->fields.assign(cls->fieldNames.size(), Value());
    return i;
}
ObjForeign* VM::newForeign(ObjClass* cls, void* ptr, void (*fin)(void*)) {
    auto* f = alloc<ObjForeign>();
    f->cls = cls; f->ptr = ptr; f->finalize = fin;
    return f;
}
ObjTask* VM::newTask() { return alloc<ObjTask>(); }
ObjNative* VM::newNative(const std::string& name, NativeFn fn, int minA, int maxA) {
    auto* n = alloc<ObjNative>();
    n->name = intern(name); n->fn = fn; n->minArgs = minA; n->maxArgs = maxA;
    return n;
}
ObjClass* VM::newClass(const std::string& name) {
    auto* c = alloc<ObjClass>();
    c->name = intern(name);
    return c;
}
ObjRange* VM::newRange(double a, double b, double step, bool inclusive) {
    auto* r = alloc<ObjRange>();
    r->start = a; r->end = b; r->step = step; r->inclusive = inclusive;
    return r;
}

Value VM::makeError(const std::string& msg, const std::string& kind) {
    ObjInstance* e = newInstance(errorClass);
    tempRoots.push_back(Value::object(e));
    e->fields[0] = str(msg);
    e->fields[1] = str(kind);
    ObjList* st = newList();
    e->fields[2] = Value::object(st);
    for (auto& s : stackTrace()) st->items.push_back(str(s));
    // archivo y línea del marco superior
    if (fiber && !fiber->frames.empty()) {
        auto& fr = fiber->frames.back();
        ObjFunction* fn = fr.closure->fn;
        size_t off = fr.ip > fn->code.data() ? (size_t)(fr.ip - fn->code.data() - 1) : 0;
        SrcLoc l = fn->locAt(off);
        e->fields[3] = fn->module ? str(fn->module->path) : Value();
        e->fields[4] = Value((double)l.line);
        e->fields[5] = Value((double)l.col);
    }
    tempRoots.pop_back();
    return Value::object(e);
}

void VM::pin(Value v) { if (v.isObj()) pinned[v.o]++; }
void VM::unpin(Value v) {
    if (!v.isObj()) return;
    auto it = pinned.find(v.o);
    if (it != pinned.end() && --it->second <= 0) pinned.erase(it);
}

void VM::markValue(const Value& v) { if (v.t == VT::Obj) markObject(v.o); }
void VM::markObject(Obj* o) {
    if (!o || o->marked) return;
    o->marked = true;
    gray.push_back(o);
}

void VM::blacken(Obj* o) {
    switch (o->type) {
        case OT::String: case OT::Range: case OT::Vec: break;
        case OT::Function: {
            auto* f = static_cast<ObjFunction*>(o);
            markObject(f->name); markObject(f->className); markObject(f->module);
            for (auto& c : f->constants) markValue(c);
            for (auto* p : f->paramNames) markObject(p);
            for (auto& c : f->invokeCache) { markObject(c.cls); markObject(c.method); }
            break;
        }
        case OT::Closure: {
            auto* c = static_cast<ObjClosure*>(o);
            markObject(c->fn); markObject(c->owner);
            for (auto* u : c->upvalues) markObject(u);
            break;
        }
        case OT::Upvalue: {
            auto* u = static_cast<ObjUpvalue*>(o);
            markValue(u->closed);
            markObject(u->fiber);
            break;
        }
        case OT::Native: markObject(static_cast<ObjNative*>(o)->name); break;
        case OT::Bound: markValue(static_cast<ObjBound*>(o)->receiver); markValue(static_cast<ObjBound*>(o)->method); break;
        case OT::Class: {
            auto* c = static_cast<ObjClass*>(o);
            markObject(c->name); markObject(c->super); markObject(c->module);
            for (auto* m : {&c->methods, &c->statics, &c->getters, &c->setters, &c->staticGetters, &c->staticSetters})
                for (auto& [k, v] : *m) { markObject(k); markValue(v); }
            for (auto* f : c->fieldNames) markObject(f);
            for (auto& v : c->fieldInits) markValue(v);
            markValue(c->ctor);
            for (auto* i : c->interfaces) markObject(i);
            break;
        }
        case OT::Instance: {
            auto* i = static_cast<ObjInstance*>(o);
            markObject(i->cls);
            for (auto& v : i->fields) markValue(v);
            if (i->extra) for (auto& [k, v] : *i->extra) { markObject(k); markValue(v); }
            break;
        }
        case OT::List: for (auto& v : static_cast<ObjList*>(o)->items) markValue(v); break;
        case OT::Map: for (auto& e : static_cast<ObjMap*>(o)->entries) if (e.alive) { markValue(e.key); markValue(e.value); } break;
        case OT::Module: {
            auto* m = static_cast<ObjModule*>(o);
            markObject(m->name);
            for (auto& v : m->globals) markValue(v);
            for (auto* n : m->globalNames) markObject(n);
            for (auto& [k, v] : m->exports) markObject(k);
            for (auto& [k, v] : m->allGlobals) markObject(k);
            for (auto& [k, v] : m->props) markObject(k);
            break;
        }
        case OT::Enum: {
            auto* e = static_cast<ObjEnum*>(o);
            markObject(e->name);
            for (auto* v : e->values) markObject(v);
            break;
        }
        case OT::EnumValue: {
            auto* e = static_cast<ObjEnumValue*>(o);
            markObject(e->owner); markObject(e->name); markValue(e->value);
            break;
        }
        case OT::Event: {
            auto* e = static_cast<ObjEvent*>(o);
            markObject(e->name);
            for (auto* p : e->params) markObject(p);
            for (auto* s : e->subs) markObject(s);
            break;
        }
        case OT::Subscription: {
            auto* s = static_cast<ObjSubscription*>(o);
            markObject(s->event); markValue(s->handler); markValue(s->owner);
            break;
        }
        case OT::Fiber: {
            auto* f = static_cast<ObjFiber*>(o);
            for (int i = 0; i < f->sp; i++) markValue(f->stack[i]);
            for (auto& fr : f->frames) markObject(fr.closure);
            for (auto* u = f->openUpvalues; u; u = u->nextOpen) markObject(u);
            markObject(f->task); markObject(f->waitingOn); markValue(f->closure);
            break;
        }
        case OT::Task: {
            auto* t = static_cast<ObjTask*>(o);
            markObject(t->fiber); markValue(t->result); markValue(t->error); markObject(t->label);
            for (auto* w : t->waiters) markObject(w);
            for (auto& c : t->callbacks) markValue(c);
            break;
        }
        case OT::Foreign: {
            auto* f = static_cast<ObjForeign*>(o);
            markObject(f->cls);
            for (auto& v : f->refs) markValue(v);
            break;
        }
    }
}

void VM::collectGarbage() {
    // Raíces
    for (auto& v : builtins) markValue(v);
    for (auto& [k, v] : libraries) markValue(v);
    for (auto& u : units) { markObject(u->module); markObject(u->script); }
    for (auto* c : {textClass, listClass, mapClass, setClass, numberClass, boolClass, functionClass, rangeClass, taskClass,
                    eventClass, subscriptionClass, errorClass, eventDataClass, enumClass, moduleClass, enumValueClass})
        markObject(c);
    for (auto* c : vecClass) markObject(c);
    markObject(fiber); markObject(mainFiber);
    for (auto* f : fiberStack) markObject(f);
    for (auto& r : readyQ) { markObject(r.f); markValue(r.v); }
    for (auto* t : timers) markObject(t);
    for (auto* t : nextFrameTasks) markObject(t);
    for (auto* t : pendingTasks) markObject(t);
    for (auto& v : tempRoots) markValue(v);
    for (auto& [o, n] : pinned) markObject(o);
    markValue(pendingError); markValue(lastError);
    for (auto* s : {s_constructor, s_toString, s_message, s_stack, s_name, s_length, s_iterator}) markObject(s);
    for (auto& fn : extraRoots) fn(*this);
    while (!gray.empty()) {
        Obj* o = gray.back();
        gray.pop_back();
        blacken(o);
    }
    sweep();
}

void VM::sweep() {
    Obj** link = &objects;
    size_t live = 0;
    while (*link) {
        Obj* o = *link;
        if (o->marked) {
            o->marked = false;
            link = &o->next;
            live++;
        } else {
            *link = o->next;
            if (o->type == OT::String) {
                auto* so = static_cast<ObjString*>(o);
                strings.erase(InternKey{so->str, so->hash});
            }
            delete o;
        }
    }
    // pendingTasks: quitar las terminadas
    pendingTasks.erase(std::remove_if(pendingTasks.begin(), pendingTasks.end(), [](ObjTask* t) { return t->state != TaskState::Pending; }), pendingTasks.end());
    bytesAllocated = live * 64;
    nextGC = std::max<size_t>(8 * 1024 * 1024, bytesAllocated * 2);
}

// ---------------------------------------------------------------------------
// Salida
// ---------------------------------------------------------------------------

void VM::print(std::string_view s) {
    if (printHook) { printHook(s); return; }
    if (!outputToStdout) return;
    output.append(s.data(), s.size());
    if (output.size() > 1 << 16) flushOutput();
}

void VM::flushOutput() {
    if (!output.empty()) {
        std::fwrite(output.data(), 1, output.size(), stdout);
        lastOutputChar = output.back();
        output.clear();
    }
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// Texto de valores
// ---------------------------------------------------------------------------

ObjClass* VM::classOf(const Value& v) {
    switch (v.t) {
        case VT::Null: case VT::Undef: return nullptr;
        case VT::Bool: return boolClass;
        case VT::Num: return numberClass;
        case VT::Obj: break;
    }
    switch (v.o->type) {
        case OT::String: return textClass;
        case OT::List: return listClass;
        case OT::Map: return static_cast<ObjMap*>(v.o)->isSet ? setClass : mapClass;
        case OT::Range: return rangeClass;
        case OT::Closure: case OT::Native: case OT::Bound: return functionClass;
        case OT::Task: return taskClass;
        case OT::Event: return eventClass;
        case OT::Subscription: return subscriptionClass;
        case OT::Enum: return enumClass;
        case OT::EnumValue: return enumValueClass;
        case OT::Module: return moduleClass;
        case OT::Instance: return static_cast<ObjInstance*>(v.o)->cls;
        case OT::Vec: return vecClass[static_cast<ObjVec*>(v.o)->kind];
        case OT::Foreign: return static_cast<ObjForeign*>(v.o)->cls;
        default: return nullptr;
    }
}

std::string VM::typeName(const Value& v) {
    switch (v.t) {
        case VT::Null: return "null";
        case VT::Undef: return "sin valor";
        case VT::Bool: return "bool";
        case VT::Num: return "number";
        case VT::Obj: break;
    }
    switch (v.o->type) {
        case OT::String: return "text";
        case OT::List: return "list";
        case OT::Map: return static_cast<ObjMap*>(v.o)->isSet ? "set" : "map";
        case OT::Closure: case OT::Native: case OT::Bound: return "function";
        case OT::Class: return static_cast<ObjClass*>(v.o)->isInterface ? "interface" : "class";
        case OT::Module: return "módulo";
        case OT::Enum: return "enum";
        case OT::EnumValue: return static_cast<ObjEnumValue*>(v.o)->owner->name->str;
        default: break;
    }
    ObjClass* c = classOf(v);
    return c ? c->name->str : "objeto";
}

static std::string vecString(ObjVec* v) {
    static const char* names[] = {"Vec2", "Vec3", "Vec4", "Color", "Quat"};
    std::string s = std::string(names[v->kind]) + "(";
    for (int i = 0; i < v->dims(); i++) { if (i) s += ", "; s += formatNumber(v->v[i]); }
    return s + ")";
}

std::string VM::toString(const Value& v, bool quote, int depth) {
    switch (v.t) {
        case VT::Null: return "null";
        case VT::Undef: return "null";
        case VT::Bool: return v.b ? "true" : "false";
        case VT::Num: return formatNumber(v.n);
        case VT::Obj: break;
    }
    if (depth > 8) return "...";
    switch (v.o->type) {
        case OT::String: return quote ? escapeString(asString(v)->str) : asString(v)->str;
        case OT::List: {
            auto& items = static_cast<ObjList*>(v.o)->items;
            std::string s = "[";
            for (size_t i = 0; i < items.size(); i++) {
                if (i) s += ", ";
                if (i >= 100) { s += "... (" + std::to_string(items.size()) + " elementos)"; break; }
                s += items[i].isObj() && items[i].o == v.o ? "[...]" : toString(items[i], true, depth + 1);
            }
            return s + "]";
        }
        case OT::Map: {
            auto* m = static_cast<ObjMap*>(v.o);
            std::string s = m->isSet ? "set{" : "{";
            int n = 0;
            for (auto& e : m->entries) {
                if (!e.alive) continue;
                if (n++) s += ", ";
                if (n > 100) { s += "..."; break; }
                if (m->isSet) s += toString(e.key, true, depth + 1);
                else {
                    bool ident = isString(e.key) && !asString(e.key)->str.empty();
                    if (ident) for (char c : asString(e.key)->str) if (!(std::isalnum((unsigned char)c) || c == '_')) ident = false;
                    s += ident ? asString(e.key)->str : toString(e.key, true, depth + 1);
                    s += ": ";
                    s += e.value.isObj() && e.value.o == v.o ? "{...}" : toString(e.value, true, depth + 1);
                }
            }
            return s + "}";
        }
        case OT::Range: {
            auto* r = static_cast<ObjRange*>(v.o);
            std::string s = formatNumber(r->start) + (r->inclusive ? "..=" : "..") + formatNumber(r->end);
            if (r->step != 1) s += " step " + formatNumber(r->step);
            return s;
        }
        case OT::Closure: { auto* f = static_cast<ObjClosure*>(v.o)->fn; return "<función " + (f->name ? f->name->str : std::string("?")) + ">"; }
        case OT::Native: return "<función nativa " + static_cast<ObjNative*>(v.o)->name->str + ">";
        case OT::Bound: return "<método>";
        case OT::Class: return std::string(static_cast<ObjClass*>(v.o)->isInterface ? "<interfaz " : "<clase ") + static_cast<ObjClass*>(v.o)->name->str + ">";
        case OT::Module: return "<módulo " + static_cast<ObjModule*>(v.o)->name->str + ">";
        case OT::Enum: return "<enum " + static_cast<ObjEnum*>(v.o)->name->str + ">";
        case OT::EnumValue: { auto* e = static_cast<ObjEnumValue*>(v.o); return e->owner->name->str + "." + e->name->str; }
        case OT::Event: return "<evento " + static_cast<ObjEvent*>(v.o)->name->str + ">";
        case OT::Subscription: return "<suscripción a " + static_cast<ObjSubscription*>(v.o)->event->name->str + ">";
        case OT::Task: {
            auto* t = static_cast<ObjTask*>(v.o);
            const char* st = t->state == TaskState::Pending ? "pendiente" : t->state == TaskState::Done ? "completada" : t->state == TaskState::Failed ? "fallida" : "cancelada";
            return std::string("<tarea ") + st + ">";
        }
        case OT::Vec: return vecString(static_cast<ObjVec*>(v.o));
        case OT::Instance: {
            auto* inst = static_cast<ObjInstance*>(v.o);
            auto it = inst->cls->methods.find(s_toString);
            if (it != inst->cls->methods.end() && !isObjType(it->second, OT::Native)) {
                Value r = callBound(it->second, v, 0, nullptr); // los errores se propagan al llamador
                return isString(r) ? asString(r)->str : toString(r, quote, depth + 1);
            }
            if (inst->cls->isSubclassOf(errorClass) && isString(inst->fields[0]))
                return (isString(inst->fields[1]) ? asString(inst->fields[1])->str : std::string("Error")) + ": " + asString(inst->fields[0])->str;
            if (inst->cls->isStruct || inst->cls == eventDataClass || inst->extra) {
                std::string s = inst->cls == eventDataClass ? "Evento(" : inst->cls->name->str + "(";
                bool first = true;
                for (size_t i = 0; i < inst->fields.size(); i++) {
                    if (inst->cls == eventDataClass) break;
                    if (!first) s += ", ";
                    first = false;
                    s += inst->cls->fieldNames[i]->str + ": " + toString(inst->fields[i], true, depth + 1);
                }
                if (inst->extra) for (auto& [k, val] : *inst->extra) {
                    if (!first) s += ", ";
                    first = false;
                    s += k->str + ": " + toString(val, true, depth + 1);
                }
                return s + ")";
            }
            return "<" + inst->cls->name->str + ">";
        }
        case OT::Foreign: {
            auto* f = static_cast<ObjForeign*>(v.o);
            auto it = f->cls->methods.find(s_toString);
            if (it != f->cls->methods.end()) {
                Value r = callBound(it->second, v, 0, nullptr);
                if (isString(r)) return asString(r)->str;
            }
            return "<" + f->cls->name->str + ">";
        }
        default: return "<objeto>";
    }
}

bool VM::isInstanceOf(const Value& v, std::string_view n) {
    if (!n.empty() && n.back() == '?') { if (v.isNull()) return true; n.remove_suffix(1); }
    // los tipos más habituales primero (se comprueba en cada llamada a funciones con parámetros tipados)
    if (n == "number") return v.isNum();
    if (n == "text") return isString(v);
    if (n == "any" || n == "object") return !v.isNull();
    if (n == "null") return v.isNull();
    if (n == "number" || n == "float" || n == "double") return v.isNum();
    if (n == "int") return v.isNum() && std::floor(v.n) == v.n && std::isfinite(v.n);
    if (n == "bool" || n == "boolean") return v.isBool();
    if (n == "text" || n == "string" || n == "char") return isString(v);
    if (n == "list" || n == "array") return isObjType(v, OT::List);
    if (n == "map" || n == "dict") return isObjType(v, OT::Map) && !static_cast<ObjMap*>(v.o)->isSet;
    if (n == "set") return isObjType(v, OT::Map) && static_cast<ObjMap*>(v.o)->isSet;
    if (n == "function" || n == "Function") return isObjType(v, OT::Closure) || isObjType(v, OT::Native) || isObjType(v, OT::Bound) || isObjType(v, OT::Class);
    if (n == "Range") return isObjType(v, OT::Range);
    if (n == "Task" || n == "Promise") return isObjType(v, OT::Task);
    if (n == "Event") return isObjType(v, OT::Event);
    if (n == "Subscription") return isObjType(v, OT::Subscription);
    ObjClass* c = classOf(v);
    if (!c) return false;
    if (isObjType(v, OT::EnumValue)) return static_cast<ObjEnumValue*>(v.o)->owner->name->str == n;
    for (auto* k = c; k; k = k->super) {
        if (k->name->str == n) return true;
        for (auto* i : k->interfaces) if (i->str == n) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Registro de API
// ---------------------------------------------------------------------------

std::string ApiEntry::signature() const {
    std::string s = "(";
    for (size_t i = 0; i < params.size(); i++) {
        auto& p = params[i];
        if (i) s += ", ";
        if (p.rest) s += "...";
        s += p.name;
        if (p.optional && p.def.empty() && !p.rest) s += "?";
        s += ": " + p.type;
        if (!p.def.empty()) s += " = " + p.def;
    }
    return s + "): " + returns;
}

bool parseApiSignature(const std::string& sig, std::vector<ApiParam>& params, std::string& ret) {
    params.clear();
    ret = "void";
    size_t open = sig.find('(');
    if (open == std::string::npos) return false;
    int depth = 0;
    size_t close = std::string::npos;
    for (size_t i = open; i < sig.size(); i++) {
        if (sig[i] == '(' || sig[i] == '<' || sig[i] == '[' || sig[i] == '{') depth++;
        else if (sig[i] == ')' || sig[i] == '>' || sig[i] == ']' || sig[i] == '}') { depth--; if (depth == 0 && sig[i] == ')') { close = i; break; } }
    }
    if (close == std::string::npos) return false;
    std::string inner = sig.substr(open + 1, close - open - 1);
    std::vector<std::string> parts;
    depth = 0;
    std::string cur;
    bool inStr = false;
    for (char c : inner) {
        if (c == '"') inStr = !inStr;
        if (!inStr && (c == '<' || c == '(' || c == '[' || c == '{')) depth++;
        if (!inStr && (c == '>' || c == ')' || c == ']' || c == '}')) depth--;
        if (c == ',' && depth == 0 && !inStr) { parts.push_back(cur); cur.clear(); continue; }
        cur += c;
    }
    if (!trim(cur).empty()) parts.push_back(cur);
    for (auto& raw : parts) {
        std::string p = trim(raw);
        ApiParam ap;
        if (startsWith(p, "...")) { ap.rest = true; p = p.substr(3); }
        size_t eq = std::string::npos;
        depth = 0; inStr = false;
        for (size_t i = 0; i < p.size(); i++) {
            if (p[i] == '"') inStr = !inStr;
            if (!inStr && p[i] == '=' && (i + 1 >= p.size() || p[i + 1] != '>')) { eq = i; break; }
        }
        if (eq != std::string::npos) { ap.def = trim(p.substr(eq + 1)); ap.optional = true; p = trim(p.substr(0, eq)); }
        size_t colon = p.find(':');
        std::string name = trim(colon == std::string::npos ? p : p.substr(0, colon));
        if (colon != std::string::npos) ap.type = trim(p.substr(colon + 1));
        if (!name.empty() && name.back() == '?') { name.pop_back(); ap.optional = true; }
        ap.name = name;
        if (ap.rest) ap.optional = true;
        params.push_back(ap);
    }
    std::string rest = trim(sig.substr(close + 1));
    if (startsWith(rest, ":")) ret = trim(rest.substr(1));
    else if (startsWith(rest, "->")) ret = trim(rest.substr(2));
    return true;
}

ApiEntry& VM::addCatalog(ApiKind kind, const std::string& owner, const std::string& name, const std::string& sig, const std::string& doc) {
    ApiEntry e;
    e.kind = kind; e.owner = owner; e.name = name; e.doc = doc;
    if (!sig.empty()) {
        parseApiSignature(sig, e.params, e.returns);
        e.minArgs = 0; e.maxArgs = 0;
        for (auto& p : e.params) {
            if (p.rest) { e.maxArgs = -1; continue; }
            if (!p.optional) e.minArgs++;
            if (e.maxArgs >= 0) e.maxArgs++;
        }
    }
    catalog.push_back(e);
    return catalog.back();
}

int VM::builtinSlot(const std::string& name) {
    auto it = builtinIndex.find(name);
    if (it != builtinIndex.end()) return it->second;
    int idx = (int)builtins.size();
    builtins.push_back(Value());
    builtinIndex[name] = idx;
    return idx;
}

ObjModule* VM::nativeModule(const std::string& name, const std::string& doc) {
    auto* m = alloc<ObjModule>();
    m->name = intern(name);
    m->isNative = true;
    m->initialized = true;
    m->doc = doc;
    builtins[builtinSlot(name)] = Value::object(m);
    addCatalog(ApiKind::Module, "", name, "", doc);
    return m;
}

static void addModuleMember(VM& vm, ObjModule* mod, const std::string& name, Value v) {
    ObjString* n = vm.intern(name);
    auto it = mod->exports.find(n);
    if (it != mod->exports.end()) { mod->globals[it->second] = v; return; }
    int idx = (int)mod->globals.size();
    mod->globals.push_back(v);
    mod->globalNames.push_back(n);
    mod->exports[n] = idx;
    mod->allGlobals[n] = idx;
}

ObjNative* VM::defineFn(ObjModule* mod, const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn,
                        const std::string& example) {
    ApiEntry& e = addCatalog(ApiKind::Function, mod->name->str, name, sig, doc);
    e.example = example;
    ObjNative* n = newNative(mod->name->str + "." + name, fn, e.minArgs, e.maxArgs);
    n->acceptsNamed = true;
    addModuleMember(*this, mod, name, Value::object(n));
    return n;
}

void VM::defineProp(ObjModule* mod, const std::string& name, const std::string& type, const std::string& doc, NativeFn get, NativeFn set) {
    ApiEntry& e = addCatalog(ApiKind::Property, mod->name->str, name, "", doc);
    e.type = type;
    e.readonly = set == nullptr;
    mod->props[intern(name)] = {get, set};
}

void VM::defineConst(ObjModule* mod, const std::string& name, Value v, const std::string& type, const std::string& doc) {
    ApiEntry& e = addCatalog(ApiKind::Constant, mod->name->str, name, "", doc);
    e.type = type;
    e.readonly = true;
    addModuleMember(*this, mod, name, v);
}

ObjNative* VM::defineGlobalFn(const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn, const std::string& example) {
    ApiEntry& e = addCatalog(ApiKind::Function, "", name, sig, doc);
    e.example = example;
    ObjNative* n = newNative(name, fn, e.minArgs, e.maxArgs);
    n->acceptsNamed = true;
    builtins[builtinSlot(name)] = Value::object(n);
    return n;
}

void VM::defineGlobal(const std::string& name, Value v, const std::string& type, const std::string& doc) {
    ApiEntry& e = addCatalog(ApiKind::Constant, "", name, "", doc);
    e.type = type;
    e.readonly = true;
    builtins[builtinSlot(name)] = v;
}

ObjClass* VM::nativeClass(const std::string& name, const std::string& doc, NativeFn ctor, const std::string& ctorSig) {
    ObjClass* c = nullptr;
    // reutilizar las clases internas ya creadas (text, list, Error...)
    for (auto* k : {textClass, listClass, mapClass, setClass, numberClass, boolClass, functionClass, rangeClass, taskClass,
                    eventClass, subscriptionClass, errorClass, eventDataClass, enumClass, moduleClass, enumValueClass})
        if (k && k->name->str == name) c = k;
    bool existed = c != nullptr;
    if (!c) { c = newClass(name); c->isNative = true; }
    c->nativeCtor = ctor;
    if (existed) {
        for (auto& e : catalog) if (e.kind == ApiKind::Class && e.name == name && e.owner.empty()) {
            e.doc = doc;
            if (!ctorSig.empty()) { parseApiSignature(ctorSig, e.params, e.returns); e.maxArgs = 0; e.minArgs = 0;
                for (auto& p : e.params) { if (p.rest) { e.maxArgs = -1; continue; } if (!p.optional) e.minArgs++; if (e.maxArgs >= 0) e.maxArgs++; } }
        }
    } else {
        ApiEntry& e = addCatalog(ApiKind::Class, "", name, ctorSig, doc);
        e.returns = name;
    }
    if (ctor || existed) builtins[builtinSlot(name)] = Value::object(c);
    else builtins[builtinSlot(name)] = Value::object(c);
    return c;
}

ObjNative* VM::defineMethod(ObjClass* cls, const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn) {
    ApiEntry& e = addCatalog(ApiKind::Method, cls->name->str, name, sig, doc);
    ObjNative* n = newNative(cls->name->str + "." + name, fn, e.minArgs, e.maxArgs);
    n->acceptsNamed = true;
    cls->methods[intern(name)] = Value::object(n);
    return n;
}

ObjNative* VM::defineStatic(ObjClass* cls, const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn) {
    ApiEntry& e = addCatalog(ApiKind::Method, cls->name->str, name, sig, doc);
    e.isStatic = true;
    ObjNative* n = newNative(cls->name->str + "." + name, fn, e.minArgs, e.maxArgs);
    n->acceptsNamed = true;
    cls->statics[intern(name)] = Value::object(n);
    return n;
}

void VM::defineClassProp(ObjClass* cls, const std::string& name, const std::string& type, const std::string& doc, NativeFn get,
                         NativeFn set, bool isStatic) {
    ApiEntry& e = addCatalog(ApiKind::Property, cls->name->str, name, "", doc);
    e.type = type;
    e.readonly = set == nullptr;
    e.isStatic = isStatic;
    ObjString* n = intern(name);
    if (isStatic) {
        if (get) cls->staticGetters[n] = Value::object(newNative(name, get, 0, 0));
        if (set) cls->staticSetters[n] = Value::object(newNative(name, set, 1, 1));
    } else {
        if (get) cls->getters[n] = Value::object(newNative(name, get, 0, 0));
        if (set) cls->setters[n] = Value::object(newNative(name, set, 1, 1));
    }
}

void VM::addCatalogField(ObjClass* cls, const std::string& name, const std::string& type, const std::string& doc, bool readonly) {
    ApiEntry& e = addCatalog(ApiKind::Field, cls->name->str, name, "", doc);
    e.type = type;
    e.readonly = readonly;
}

ObjEvent* VM::defineEvent(ObjModule* mod, const std::string& name, const std::string& params, const std::string& doc) {
    ApiEntry& e = addCatalog(ApiKind::Event, mod->name->str, name, "(" + params + ")", doc);
    auto* ev = alloc<ObjEvent>();
    ev->name = intern(mod->name->str + "." + name);
    for (auto& p : e.params) ev->params.push_back(intern(p.name));
    addModuleMember(*this, mod, name, Value::object(ev));
    return ev;
}

void VM::defineLibrary(const std::string& lib, const std::vector<std::string>& namespaces, const std::string& doc) {
    libraryMembers[lib] = namespaces;
    libraryDocs.push_back({lib, doc});
    if (namespaces.size() == 1) {
        auto it = builtinIndex.find(namespaces[0]);
        if (it != builtinIndex.end()) libraries[lib] = builtins[it->second];
        return;
    }
    auto* m = alloc<ObjModule>();
    m->name = intern(lib);
    m->isNative = true;
    m->initialized = true;
    m->doc = doc;
    for (auto& n : namespaces) {
        auto it = builtinIndex.find(n);
        if (it != builtinIndex.end()) addModuleMember(*this, m, n, builtins[it->second]);
    }
    libraries[lib] = Value::object(m);
}

Value VM::libraryModule(const std::string& name) {
    auto it = libraries.find(name);
    return it == libraries.end() ? Value() : it->second;
}

std::vector<std::string> VM::libraryNames() const {
    std::vector<std::string> r;
    for (auto& [k, v] : libraries) r.push_back(k);
    return r;
}

// ---------------------------------------------------------------------------
// Compilación de módulos
// ---------------------------------------------------------------------------

// Bibliotecas escritas en HyperEZ e incrustadas en el ejecutable (src/lib, generado por cmake/embed_hz.cmake)
const std::map<std::string, const char*>& embeddedLibraries();
static const char* kLibPrefix = "hz:lib/";

static bool readSourceFile(VM& vm, const std::string& path, std::string& out) {
    if (startsWith(path, kLibPrefix)) {
        std::string name = path.substr(std::strlen(kLibPrefix));
        if (endsWith(name, ".hz")) name.resize(name.size() - 3);
        auto it = embeddedLibraries().find(name);
        if (it == embeddedLibraries().end()) return false;
        out = it->second;
        return true;
    }
    if (vm.readSource && vm.readSource(path, out)) return true;
    return readFile(path, out);
}

std::vector<std::string> VM::bundledLibraryNames() const {
    std::vector<std::string> out;
    for (auto& [name, src] : embeddedLibraries()) out.push_back(name);
    return out;
}

ModuleUnit* VM::compileFile(const std::string& path) {
    std::string src;
    std::string abs = pathAbsolute(path);
    if (!readSourceFile(*this, abs, src)) {
        auto unit = std::make_unique<ModuleUnit>();
        unit->path = abs;
        unit->name = pathStem(abs);
        unit->diags.file = abs;
        unit->diags.error({}, "No se puede leer el archivo '" + path + "'", "", "E0001");
        units.push_back(std::move(unit));
        return units.back().get();
    }
    return compileSource(abs, src, true);
}

ModuleUnit* VM::compileSource(const std::string& path, const std::string& source, bool isEntry, bool doCodegen) {
    auto owned = std::make_unique<ModuleUnit>();
    ModuleUnit* unit = owned.get();
    units.push_back(std::move(owned));
    unit->path = path;
    unit->name = pathStem(path);
    unit->diags.file = path;
    unit->repl = isEntry ? repl : nullptr;
    unit->analyzing = true;
    bool savedGC = gcEnabled;
    gcEnabled = false;
    unit->program = parseSource(path, source, unit->diags);
    Analyzer an(*this, *unit);
    an.run();
    unit->analyzing = false;
    unit->analyzed = true;
    if (unit->repl) {
        // Consola: el código de entradas anteriores ya se ejecutó. Si una redefinición lo deja con errores
        // (p. ej. usa una variable que ahora se declara más abajo) no importa: no se vuelve a generar.
        auto& items = unit->diags.items;
        int start = unit->repl->start;
        items.erase(std::remove_if(items.begin(), items.end(), [&](const Diagnostic& d) { return d.loc.line > 0 && d.loc.offset < start; }),
                    items.end());
    }
    if (isEntry) entryUnit = unit;
    // Módulo en tiempo de ejecución
    // En la consola interactiva todas las entradas comparten el módulo (los globales conservan su valor).
    auto* mod = unit->repl && unit->repl->module ? unit->repl->module : alloc<ObjModule>();
    mod->name = intern(unit->name);
    mod->path = path;
    for (size_t i = 0; i < unit->globalNames.size(); i++) {
        ObjString* n = intern(unit->globalNames[i]);
        if (i < mod->globals.size()) {
            mod->globalNames[i] = n;
        } else {
            mod->globals.push_back(Value::undef());
            mod->globalNames.push_back(n);
        }
        mod->allGlobals[n] = (int)i;
        auto s = unit->globalSyms.find(unit->globalNames[i]);
        if (s == unit->globalSyms.end() || !s->second || s->second->vis != Visibility::Private) mod->exports[n] = (int)i;
    }
    unit->module = mod;
    if (doCodegen && !unit->diags.hasErrors()) {
        CodeGen cg(*this, *unit);
        unit->script = cg.compileScript();
        unit->compiled = !unit->diags.hasErrors();
    }
    gcEnabled = savedGC;
    return unit;
}

bool VM::unitHasErrors(ModuleUnit* unit) {
    if (!unit) return true;
    std::vector<ModuleUnit*> stack{unit};
    std::vector<ModuleUnit*> seen;
    while (!stack.empty()) {
        ModuleUnit* u = stack.back();
        stack.pop_back();
        if (std::find(seen.begin(), seen.end(), u) != seen.end()) continue;
        seen.push_back(u);
        if (u->diags.hasErrors() || (!u->compiled && !u->isPrelude)) return true;
        for (auto* i : u->imports) stack.push_back(i);
    }
    return false;
}

void VM::printDiagnostics(ModuleUnit* unit, FILE* out, bool includeWarnings) {
    std::vector<ModuleUnit*> stack{unit};
    std::vector<ModuleUnit*> seen;
    while (!stack.empty()) {
        ModuleUnit* u = stack.back();
        stack.pop_back();
        if (!u || std::find(seen.begin(), seen.end(), u) != seen.end()) continue;
        seen.push_back(u);
        for (auto* i : u->imports) stack.push_back(i);
        auto items = u->diags.items;
        std::stable_sort(items.begin(), items.end(), [](const Diagnostic& a, const Diagnostic& b) {
            return a.loc.line != b.loc.line ? a.loc.line < b.loc.line : a.loc.col < b.loc.col;
        });
        for (auto& d : items) {
            if (d.severity == Severity::Hint) continue;
            if (d.severity == Severity::Warning && !includeWarnings) continue;
            printDiagnostic(out, d, u->program ? std::string_view(u->program->source) : std::string_view());
            std::fputc('\n', out);
        }
    }
}

std::string VM::projectHome() {
    std::string start = projectRoot.empty() ? currentDir() : projectRoot;
    std::string d = start;
    for (int i = 0; i < 16 && !d.empty(); i++) {
        if (dirExists(pathJoin(d, "hypersettings"))) return d;
        std::string up = pathDir(d);
        if (up.empty() || up == d) break;
        d = up;
    }
    return start;
}

ModuleUnit* VM::resolveImport(const std::string& path, bool isFile, const std::string& fromFile, std::string& error) {
    std::vector<std::string> candidates;
    std::string fromDir = pathDir(fromFile);
    auto addExt = [&](const std::string& base) {
        std::string ext = pathExt(base);
        if (ext == ".hz" || ext == ".hez") candidates.push_back(base);
        else {
            candidates.push_back(base + ".hez");
            candidates.push_back(base + ".hz");
            candidates.push_back(pathJoin(base, "main.hez"));
            candidates.push_back(pathJoin(base, "main.hz"));
        }
    };
    if (isFile) {
        addExt(pathJoin(fromDir, path));
        if (!projectRoot.empty()) addExt(pathJoin(projectRoot, path));
    } else {
        std::string rel = path;
        for (auto& c : rel) if (c == '.') c = '/';
        addExt(pathJoin(fromDir, rel));
        // Las carpetas de un proyecto HyperEZ: hypercode (los módulos del programa) y hyperlibs (bibliotecas de otros)
        std::string home = projectHome();
        addExt(pathJoin(pathJoin(home, "hypercode"), rel));
        addExt(pathJoin(pathJoin(home, "hyperlibs"), rel));
        if (!projectRoot.empty()) {
            addExt(pathJoin(pathJoin(projectRoot, "src"), rel));
            addExt(pathJoin(projectRoot, rel));
            addExt(pathJoin(pathJoin(projectRoot, "hz_packages"), rel));
            addExt(pathJoin(pathJoin(pathJoin(projectRoot, "hz_packages"), rel), "src/main"));
        }
        for (auto& sp : searchPaths) addExt(pathJoin(sp, rel));
    }
    std::string found;
    for (auto& c : candidates) {
        std::string abs = pathAbsolute(c);
        std::string tmp;
        if (fileExists(abs) || (readSource && readSource(abs, tmp))) { found = abs; break; }
    }
    // Bibliotecas incluidas con HyperEZ (import ai, import stats...): solo si no hay un archivo propio con ese nombre
    if (found.empty() && !isFile && embeddedLibraries().count(path)) found = kLibPrefix + path + ".hz";
    if (found.empty()) {
        error = "No se encontró el módulo '" + path + "'";
        return nullptr;
    }
    for (auto& u : units) {
        if (u->path == found) {
            if (u->analyzing) { error = "Importación circular: '" + pathFilename(found) + "' se importa a sí mismo a través de otros módulos"; return nullptr; }
            return u.get();
        }
    }
    std::string src;
    if (!readSourceFile(*this, found, src)) { error = "No se puede leer '" + found + "'"; return nullptr; }
    ModuleUnit* saved = entryUnit;
    ModuleUnit* u = compileSource(found, src, false);
    entryUnit = saved;
    if (u->diags.hasErrors()) error = "El módulo '" + path + "' tiene errores";
    return u;
}

Value VM::importModule(const std::string& path) {
    auto lib = libraries.find(path);
    if (lib != libraries.end()) return lib->second;
    for (auto& u : units) {
        if (u->path != path) continue;
        ObjModule* m = u->module;
        if (!m->initialized) {
            m->initialized = true;
            if (!u->script) raise("El módulo '" + u->name + "' no se pudo compilar");
            ObjClosure* c = alloc<ObjClosure>(u->script);
            call(Value::object(c), 0, nullptr);
        }
        return Value::object(m);
    }
    raise("Módulo no encontrado: " + path);
}

bool VM::loadPrelude() {
    extern const char* kPreludeSource;
    extern const char* kPreludeName;
    ModuleUnit* u = compileSource(kPreludeName, kPreludeSource, false);
    u->isPrelude = true;
    preludeUnit = u;
    entryUnit = nullptr;
    if (u->diags.hasErrors()) {
        std::fprintf(stderr, "Error interno: el preludio de HyperEZ no compila\n");
        printDiagnostics(u, stderr);
        return false;
    }
    // Exportar los nombres del preludio como integrados
    World& w = worldOf(*this);
    std::vector<std::pair<int, int>> copy; // global del preludio -> integrado
    for (auto& [name, idx] : u->globalIndex) {
        Symbol* s = u->globalSyms[name];
        if (!s || s->vis == Visibility::Private || name.rfind("__", 0) == 0) continue;
        int slot = builtinSlot(name);
        copy.push_back({idx, slot});
        w.builtinTypes[name] = u->globalTypes[name];
        s->builtin = true;
        w.builtinSyms[name] = s;
    }
    for (auto& c : u->classes) w.preludeClasses[c->name] = c.get();
    for (auto& e : u->enums) w.preludeEnums[e->name] = e.get();
    // Ejecutar
    if (!runModule(u)) return false;
    for (auto& [g, b] : copy) builtins[b] = u->module->globals[g];
    // los globales internos (__) también son accesibles para el motor por nombre
    return true;
}

Value VM::getGlobal(ModuleUnit* unit, const std::string& name) {
    if (!unit || !unit->module) return Value();
    auto it = unit->module->allGlobals.find(intern(name));
    if (it == unit->module->allGlobals.end()) return Value();
    Value v = unit->module->globals[it->second];
    return v.isUndef() ? Value() : v;
}

Value VM::findFunction(ModuleUnit* unit, const std::string& name) {
    Value v = getGlobal(unit, name);
    if (isObjType(v, OT::Closure) || isObjType(v, OT::Native)) return v;
    return Value();
}

// ---------------------------------------------------------------------------
// Errores
// ---------------------------------------------------------------------------

void VM::raise(const std::string& msg) { throw NativeError{msg}; }

std::vector<std::string> VM::stackTrace() {
    std::vector<std::string> out;
    if (!fiber) return out;
    for (int i = (int)fiber->frames.size() - 1; i >= 0; i--) {
        auto& fr = fiber->frames[i];
        ObjFunction* fn = fr.closure->fn;
        size_t off = fr.ip > fn->code.data() ? (size_t)(fr.ip - fn->code.data() - 1) : 0;
        SrcLoc l = fn->locAt(off);
        std::string name = fn->name ? fn->name->str : "?";
        if (fn->className && name != "<lambda>") name = fn->className->str + "." + name;
        std::string file = fn->module ? pathFilename(fn->module->path) : "?";
        out.push_back(name + " (" + file + ":" + std::to_string(l.line) + ")");
        if (out.size() > 30) { out.push_back("..."); break; }
    }
    return out;
}

std::string VM::errorMessage(const Value& err) {
    if (isObjType(err, OT::Instance)) {
        auto* inst = static_cast<ObjInstance*>(err.o);
        if (inst->cls->isSubclassOf(errorClass) && isString(inst->fields[0])) return asString(inst->fields[0])->str;
    }
    return toString(err);
}

void VM::reportError(const Value& err, FILE* out) {
    flushOutput();
    std::string msg = errorMessage(err);
    std::string file;
    int line = 0, col = 0;
    std::vector<std::string> trace = lastTrace;
    if (isObjType(err, OT::Instance)) {
        auto* inst = static_cast<ObjInstance*>(err.o);
        if (inst->cls->isSubclassOf(errorClass)) {
            if (isString(inst->fields[3])) file = asString(inst->fields[3])->str;
            if (inst->fields[4].isNum()) line = (int)inst->fields[4].n;
            if (inst->fields[5].isNum()) col = (int)inst->fields[5].n;
            if (isObjType(inst->fields[2], OT::List)) {
                trace.clear();
                for (auto& v : static_cast<ObjList*>(inst->fields[2].o)->items) trace.push_back(toString(v));
            }
            std::string kind = isString(inst->fields[1]) ? asString(inst->fields[1])->str : "Error";
            if (kind != "Error" && kind != "ErrorDeEjecucion") msg = kind + ": " + msg;
        }
    }
    if (errorHook && errorHook(msg, file, line, col, trace)) return;
    Diagnostic d;
    d.severity = Severity::Error;
    d.message = msg;
    d.file = file;
    d.loc.line = line;
    d.loc.col = col;
    std::string source;
    for (auto& u : units) if (u->path == file && u->program) source = u->program->source;
    printDiagnostic(out, d, source, "error en ejecución");
    if (!trace.empty()) {
        std::fprintf(out, "  %spila de llamadas:%s\n", color::gray(), color::reset());
        for (auto& t : trace) std::fprintf(out, "    %sen%s %s\n", color::gray(), color::reset(), t.c_str());
    }
    std::fflush(out);
}

// ---------------------------------------------------------------------------
// Eventos
// ---------------------------------------------------------------------------

ObjSubscription* VM::subscribe(ObjEvent* ev, Value handler, bool once, double priority) {
    auto* s = alloc<ObjSubscription>();
    s->event = ev;
    s->handler = handler;
    s->once = once;
    s->priority = priority;
    s->order = ++subscriptionCounter;
    // Insertar ordenado por prioridad (mayor primero) y luego por orden de registro
    auto pos = std::find_if(ev->subs.begin(), ev->subs.end(), [&](ObjSubscription* o) { return o->priority < priority; });
    ev->subs.insert(pos, s);
    return s;
}

Value VM::eventPayload(ObjEvent* ev, int argc, const Value* args, Value target) {
    ObjInstance* p = newInstance(eventDataClass);
    tempRoots.push_back(Value::object(p));
    p->fields[0] = Value::object(ev->name);
    p->fields[1] = Value(currentTime());
    p->fields[2] = target;
    p->fields[3] = Value::boolean(false);
    p->fields[4] = Value::boolean(false);
    p->extra = std::make_unique<StrMap<Value>>();
    for (int i = 0; i < argc && i < (int)ev->params.size(); i++) (*p->extra)[ev->params[i]] = args[i];
    ObjList* data = newList();
    for (int i = 0; i < argc; i++) data->items.push_back(args[i]);
    (*p->extra)[intern("data")] = Value::object(data);
    tempRoots.pop_back();
    return Value::object(p);
}

void VM::emitEvent(ObjEvent* ev, int argc, const Value* args, Value target) {
    ev->emitCount++;
    if (ev->subs.empty()) return;
    Value payload = eventPayload(ev, argc, args, target);
    tempRoots.push_back(payload);
    auto* p = static_cast<ObjInstance*>(payload.o);
    ev->dispatching++;
    // Copia: los manejadores pueden suscribir/cancelar durante el despacho
    std::vector<ObjSubscription*> subs = ev->subs;
    for (auto* s : subs) {
        if (!s->active) continue;
        if (s->once) { s->active = false; ev->needsCleanup = true; }
        Value r;
        if (!tryCall(s->handler, 1, &payload, &r)) {
            std::fprintf(stderr, "%s[evento %s]%s un manejador falló; el resto de manejadores continúa:\n", color::yellow(), ev->name->str.c_str(), color::reset());
            reportError(lastError);
        }
        if (isObjType(r, OT::Task)) { /* manejador async: sigue en segundo plano */ }
        if (p->fields[3].isBool() && p->fields[3].b) break; // stopPropagation()
    }
    ev->dispatching--;
    if (ev->dispatching == 0 && ev->needsCleanup) {
        ev->subs.erase(std::remove_if(ev->subs.begin(), ev->subs.end(), [](ObjSubscription* s) { return !s->active; }), ev->subs.end());
        ev->needsCleanup = false;
    }
    tempRoots.pop_back();
}

// ---------------------------------------------------------------------------
// Tareas y planificador
// ---------------------------------------------------------------------------

double VM::currentTime() { return clock ? clock() : nowSeconds(); }

ObjTask* VM::waitTask(double seconds) {
    ObjTask* t = newTask();
    t->wakeAt = currentTime() + std::max(0.0, seconds);
    timers.push_back(t);
    return t;
}

void VM::wakeWaiters(ObjTask* t) {
    for (auto* w : t->waiters) {
        if (t->state == TaskState::Done) readyQ.push_back({w, t->result, false});
        else if (t->state == TaskState::Failed) { t->errorObserved = true; readyQ.push_back({w, t->error, true}); }
        else readyQ.push_back({w, makeError("La tarea fue cancelada", "TareaCancelada"), true});
    }
    t->waiters.clear();
    for (auto& cb : t->callbacks) {
        Value arg = t->state == TaskState::Done ? t->result : Value();
        if (!tryCall(cb, 1, &arg)) reportError(lastError);
    }
    t->callbacks.clear();
}

void VM::completeTask(ObjTask* t, Value result) {
    if (t->state != TaskState::Pending) return;
    t->state = TaskState::Done;
    t->result = result;
    wakeWaiters(t);
}

void VM::failTask(ObjTask* t, Value err) {
    if (t->state != TaskState::Pending) return;
    t->state = TaskState::Failed;
    t->error = err;
    bool observed = !t->waiters.empty();
    wakeWaiters(t);
    if (!observed) {
        std::fprintf(stderr, "%s[tarea async]%s error no capturado:\n", color::yellow(), color::reset());
        reportError(err);
    }
}

bool VM::hasPendingTasks() const { return !readyQ.empty() || !timers.empty() || !nextFrameTasks.empty(); }

void VM::cancelAllTasks() {
    readyQ.clear();
    timers.clear();
    nextFrameTasks.clear();
    pendingTasks.clear();
}

void VM::pumpScheduler() {
    double now = currentTime();
    // nextFrame()
    std::vector<ObjTask*> frameTasks;
    frameTasks.swap(nextFrameTasks);
    for (auto* t : frameTasks) completeTask(t, Value());
    // temporizadores vencidos
    if (!timers.empty()) {
        std::vector<ObjTask*> due;
        timers.erase(std::remove_if(timers.begin(), timers.end(), [&](ObjTask* t) {
            if (t->state != TaskState::Pending) return true;
            if (t->wakeAt <= now) { due.push_back(t); return true; }
            return false;
        }), timers.end());
        std::sort(due.begin(), due.end(), [](ObjTask* a, ObjTask* b) { return a->wakeAt < b->wakeAt; });
        for (auto* t : due) completeTask(t, Value());
    }
    size_t n = readyQ.size();
    for (size_t i = 0; i < n && !readyQ.empty(); i++) {
        Resume r = readyQ.front();
        readyQ.pop_front();
        if (r.f->state != FiberState::Suspended) continue;
        resumeFiber(r.f, r.v, r.err);
    }
}

bool VM::drainScheduler() {
    while (hasPendingTasks()) {
        if (g_interrupt.load(std::memory_order_relaxed)) {
            cancelAllTasks();
            break;
        }
        if (readyQ.empty() && nextFrameTasks.empty() && !timers.empty()) {
            double next = 1e300;
            for (auto* t : timers) next = std::min(next, t->wakeAt);
            double wait = next - currentTime();
            if (wait > 0) { flushOutput(); std::this_thread::sleep_for(std::chrono::duration<double>(std::min(wait, 0.5))); }
        }
        pumpScheduler();
        if (mainFiber && mainFiber->state == FiberState::Failed) return false;
    }
    if (mainFiber && mainFiber->state == FiberState::Suspended) {
        std::fprintf(stderr, "%saviso:%s el programa terminó con un 'await' que nunca se completó\n", color::yellow(), color::reset());
    }
    return !(mainFiber && mainFiber->state == FiberState::Failed);
}

} // namespace hz
