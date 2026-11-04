// Intérprete de bytecode: bucle principal, llamadas, propiedades, fibras.
#include "lang/vm/vm.h"
#include "lang/sema/analyzer.h"

#include <algorithm>
#include <cmath>

namespace hz {

static constexpr int kMaxFrames = 8000;
static constexpr int kMaxNativeDepth = 220;

// ---------------------------------------------------------------------------
// Upvalues
// ---------------------------------------------------------------------------

ObjUpvalue* VM::captureUpvalue(int slot) {
    ObjFiber* f = fiber;
    ObjUpvalue* prev = nullptr;
    ObjUpvalue* up = f->openUpvalues;
    while (up && up->slot > slot) { prev = up; up = up->nextOpen; }
    if (up && up->slot == slot) return up;
    ObjUpvalue* created = alloc<ObjUpvalue>();
    created->fiber = f;
    created->slot = slot;
    created->nextOpen = up;
    if (prev) prev->nextOpen = created; else f->openUpvalues = created;
    return created;
}

void VM::closeUpvalues(int from) {
    ObjFiber* f = fiber;
    while (f->openUpvalues && f->openUpvalues->slot >= from) {
        ObjUpvalue* u = f->openUpvalues;
        u->closed = f->stack[u->slot];
        u->open = false;
        f->openUpvalues = u->nextOpen;
    }
}

// ---------------------------------------------------------------------------
// Excepciones
// ---------------------------------------------------------------------------

bool VM::throwValue(Value err) {
    ObjFiber* f = fiber;
    bool isErr = isObjType(err, OT::Instance) && static_cast<ObjInstance*>(err.o)->cls->isSubclassOf(errorClass);
    lastTrace = isErr ? std::vector<std::string>{} : stackTrace();
    while (!f->handlers.empty()) {
        Handler h = f->handlers.back();
        if (h.frameIndex < currentExitDepth) break;
        f->handlers.pop_back();
        if (h.frameIndex >= (int)f->frames.size()) continue;
        closeUpvalues(h.stackHeight);
        f->frames.resize(h.frameIndex + 1);
        f->frames.back().ip = h.catchIp;
        f->sp = h.stackHeight;
        f->ensure(1);
        f->stack[f->sp++] = err;
        return true;
    }
    pendingError = err;
    hasPendingError = true;
    lastError = err;
    if ((int)f->frames.size() > currentExitDepth) {
        int base = f->frames[currentExitDepth].base;
        closeUpvalues(base);
        f->frames.resize(currentExitDepth);
        f->sp = base;
    }
    return false;
}

void VM::errorInit(ObjInstance* inst, Value message) {
    if (inst->fields.size() < 6) return;
    inst->fields[0] = isString(message) ? message : str(message.isNull() ? "" : toString(message));
    inst->fields[1] = Value::object(inst->cls->name);
    ObjList* st = newList();
    inst->fields[2] = Value::object(st);
    for (auto& s : stackTrace()) st->items.push_back(str(s));
    if (fiber && !fiber->frames.empty()) {
        auto& fr = fiber->frames.back();
        ObjFunction* fn = fr.closure->fn;
        size_t off = fr.ip > fn->code.data() ? (size_t)(fr.ip - fn->code.data() - 1) : 0;
        SrcLoc l = fn->locAt(off);
        inst->fields[3] = fn->module ? str(fn->module->path) : Value();
        inst->fields[4] = Value((double)l.line);
        inst->fields[5] = Value((double)l.col);
    }
}

// ---------------------------------------------------------------------------
// Llamadas
// ---------------------------------------------------------------------------

static std::string fnLabel(ObjFunction* fn) {
    std::string n = fn->name ? fn->name->str : "?";
    if (fn->className && n != "<lambda>") n = fn->className->str + "." + n;
    return "'" + n + "'";
}

bool VM::callClosure(ObjClosure* c, int argc) {
    ObjFunction* fn = c->fn;
    ObjFiber* f = fiber;
    if (argc > fn->arity && !fn->hasRest) {
        if (fn->isCtor) raise("El constructor de " + (fn->className ? fn->className->str : std::string("?")) + " espera " + std::to_string(fn->arity) +
                              " argumento(s) pero recibió " + std::to_string(argc));
        raise("La función " + fnLabel(fn) + " espera " + std::to_string(fn->arity) + " argumento(s) pero recibió " + std::to_string(argc));
    }
    if (argc < fn->minArity) {
        std::string missing;
        for (int i = argc; i < fn->minArity && i < (int)fn->paramNames.size(); i++) missing += (missing.empty() ? "'" : ", '") + fn->paramNames[i]->str + "'";
        raise("Faltan argumentos para " + fnLabel(fn) + ": " + missing);
    }
    f->ensure(fn->arity + 2);
    while (argc < fn->arity) { f->stack[f->sp++] = Value::undef(); argc++; }
    if (fn->hasRest) {
        int extra = argc - fn->arity;
        ObjList* rest = newList();
        for (int i = 0; i < extra; i++) rest->items.push_back(f->stack[f->sp - extra + i]);
        f->sp -= extra;
        f->stack[f->sp++] = Value::object(rest);
        argc = fn->arity + 1;
    }
    if (fn->isAsync) return startAsync(c, argc);
    if ((int)f->frames.size() >= kMaxFrames)
        raise("Desbordamiento de pila: demasiadas llamadas anidadas (¿recursión infinita en " + fnLabel(fn) + "?)");
    f->ensure(fn->maxSlots + 64);
    f->frames.push_back({c, fn->code.data(), f->sp - argc - 1});
    return true;
}

namespace {
struct DepthGuard {
    int& d;
    explicit DepthGuard(int& x) : d(x) { d++; }
    ~DepthGuard() { d--; }
};
}

bool VM::callNative(ObjNative* n, int argc) {
    ObjFiber* f = fiber;
    if (argc < n->minArgs || (n->maxArgs >= 0 && argc > n->maxArgs)) {
        std::string expect = n->minArgs == n->maxArgs ? std::to_string(n->minArgs)
                           : n->maxArgs < 0 ? "al menos " + std::to_string(n->minArgs)
                           : "entre " + std::to_string(n->minArgs) + " y " + std::to_string(n->maxArgs);
        raise("'" + n->name->str + "' espera " + expect + " argumento(s) pero recibió " + std::to_string(argc));
    }
    Value local[18];
    std::vector<Value> big;
    Value* buf = local;
    if (argc + 1 > 18) { big.resize(argc + 1); buf = big.data(); }
    int base = f->sp - argc - 1;
    for (int i = 0; i <= argc; i++) buf[i] = f->stack[base + i];
    if (nativeDepth > kMaxNativeDepth) raise("Demasiadas llamadas anidadas entre código nativo y script");
    Value r;
    {
        DepthGuard g(nativeDepth);
        r = n->fn(*this, argc, buf + 1);
    }
    f = fiber;
    f->sp = base;
    f->stack[f->sp++] = r;
    return true;
}

bool VM::callMethodValue(Value method, int argc) {
    if (isObjType(method, OT::Closure)) return callClosure(static_cast<ObjClosure*>(method.o), argc);
    if (isObjType(method, OT::Native)) return callNative(static_cast<ObjNative*>(method.o), argc);
    // otro invocable (campo con función): sustituir el receptor
    fiber->stack[fiber->sp - argc - 1] = method;
    return callValue(method, argc);
}

bool VM::callValue(Value callee, int argc) {
    if (callee.isObj()) {
        switch (callee.o->type) {
            case OT::Closure: return callClosure(static_cast<ObjClosure*>(callee.o), argc);
            case OT::Native: return callNative(static_cast<ObjNative*>(callee.o), argc);
            case OT::Class: return instantiate(static_cast<ObjClass*>(callee.o), argc);
            case OT::Bound: {
                auto* b = static_cast<ObjBound*>(callee.o);
                fiber->stack[fiber->sp - argc - 1] = b->receiver;
                return callMethodValue(b->method, argc);
            }
            case OT::Event:
                raise("Los eventos no se llaman como funciones: usa 'emit " + static_cast<ObjEvent*>(callee.o)->name->str + "(...)'");
            default: break;
        }
    }
    if (callee.isNull()) raise("Se intentó llamar a null como si fuera una función");
    raise("Un valor de tipo " + typeName(callee) + " no es una función");
}

bool VM::reorderNamed(Value callee, int& argc, ObjList* names) {
    ObjFiber* f = fiber;
    int base = f->sp - argc;
    std::vector<ObjString*> params;
    int minArity = 0;
    std::string what;
    Value target = callee;
    if (isObjType(target, OT::Bound)) target = static_cast<ObjBound*>(target.o)->method;
    bool nativeOpts = false;
    if (isObjType(target, OT::Closure)) {
        auto* fn = static_cast<ObjClosure*>(target.o)->fn;
        params = fn->paramNames;
        if (fn->hasRest) params.pop_back();
        minArity = fn->minArity;
        what = fnLabel(fn);
    } else if (isObjType(target, OT::Class)) {
        auto* cls = static_cast<ObjClass*>(target.o);
        what = "el constructor de " + cls->name->str;
        if (isObjType(cls->ctor, OT::Closure)) {
            auto* fn = static_cast<ObjClosure*>(cls->ctor.o)->fn;
            params = fn->paramNames;
            minArity = fn->minArity;
        } else if (cls->isStruct) {
            params = cls->fieldNames;
        } else if (cls->nativeCtor) nativeOpts = true;
        else raise("La clase " + cls->name->str + " no tiene constructor y no acepta argumentos con nombre");
    } else if (isObjType(target, OT::Native)) {
        nativeOpts = true;
    } else {
        raise("Un valor de tipo " + typeName(callee) + " no es una función");
    }
    if (nativeOpts) {
        // Los argumentos con nombre se agrupan en un mapa de opciones al final
        std::vector<Value> positional;
        ObjMap* opts = newMap();
        tempRoots.push_back(Value::object(opts));
        for (int i = 0; i < argc; i++) {
            Value nm = names->items[i];
            if (nm.isNull()) positional.push_back(f->stack[base + i]);
            else opts->set(nm, f->stack[base + i]);
        }
        f->sp = base;
        for (auto& v : positional) f->stack[f->sp++] = v;
        f->stack[f->sp++] = Value::object(opts);
        argc = (int)positional.size() + 1;
        tempRoots.pop_back();
        return true;
    }
    std::vector<Value> ordered(params.size(), Value::undef());
    std::vector<Value> extra;
    int pos = 0;
    for (int i = 0; i < argc; i++) {
        Value nm = names->items[i];
        Value v = f->stack[base + i];
        if (nm.isNull()) {
            if (pos < (int)ordered.size()) ordered[pos++] = v;
            else extra.push_back(v);
            continue;
        }
        int found = -1;
        for (size_t k = 0; k < params.size(); k++) if (params[k] == nm.o) found = (int)k;
        if (found < 0) raise(what + " no tiene ningún parámetro llamado '" + asString(nm)->str + "'");
        if (!ordered[found].isUndef()) raise("El parámetro '" + asString(nm)->str + "' recibió dos valores");
        ordered[found] = v;
    }
    for (int i = 0; i < minArity && i < (int)ordered.size(); i++)
        if (ordered[i].isUndef()) raise("Falta el argumento '" + params[i]->str + "' para " + what);
    int n = (int)ordered.size();
    while (n > 0 && ordered[n - 1].isUndef()) n--;
    f->sp = base;
    f->ensure(n + (int)extra.size() + 4);
    for (int i = 0; i < n; i++) f->stack[f->sp++] = ordered[i];
    for (auto& v : extra) f->stack[f->sp++] = v;
    argc = n + (int)extra.size();
    return true;
}

bool VM::instantiate(ObjClass* cls, int argc) {
    ObjFiber* f = fiber;
    int base = f->sp - argc - 1;
    if (cls->isInterface) raise("No se puede crear un objeto de la interfaz '" + cls->name->str + "'");
    if (cls->nativeCtor) {
        Value local[18];
        std::vector<Value> big;
        Value* buf = local;
        if (argc + 1 > 18) { big.resize(argc + 1); buf = big.data(); }
        for (int i = 0; i <= argc; i++) buf[i] = f->stack[base + i];
        Value r;
        { DepthGuard g(nativeDepth); r = cls->nativeCtor(*this, argc, buf + 1); }
        f = fiber;
        f->sp = base;
        f->stack[f->sp++] = r;
        return true;
    }
    if (cls->isNative && !cls->isSubclassOf(errorClass)) raise("No se pueden crear objetos de '" + cls->name->str + "' directamente");
    ObjInstance* inst = newInstance(cls);
    Value self = Value::object(inst);
    f->stack[base] = self;
    for (auto& fi : cls->fieldInits) callBound(fi, self, 0, nullptr);
    f = fiber;
    if (isObjType(cls->ctor, OT::Closure)) return callClosure(static_cast<ObjClosure*>(cls->ctor.o), argc);
    if (cls->isStruct) {
        if (argc > (int)cls->fieldNames.size())
            raise("El struct " + cls->name->str + " tiene " + std::to_string(cls->fieldNames.size()) + " campo(s) pero recibió " + std::to_string(argc) + " argumentos");
        for (int i = 0; i < argc; i++) {
            Value v = f->stack[base + 1 + i];
            if (!v.isUndef()) inst->fields[i] = v;
        }
        f->sp = base + 1;
        return true;
    }
    if (cls->isSubclassOf(errorClass)) {
        errorInit(inst, argc > 0 ? f->stack[base + 1] : Value());
        f->sp = base + 1;
        return true;
    }
    if (argc > 0) raise("La clase '" + cls->name->str + "' no tiene constructor y no acepta argumentos");
    f->sp = base + 1;
    return true;
}

Value VM::bindMethod(Value receiver, Value method) {
    ObjBound* b = alloc<ObjBound>();
    b->receiver = receiver;
    b->method = method;
    return Value::object(b);
}

static Value callNativeAccessor(VM& vm, Value fn, Value self, int argc, const Value* args) {
    auto* n = static_cast<ObjNative*>(fn.o);
    Value buf[3];
    buf[0] = self;
    for (int i = 0; i < argc && i < 2; i++) buf[1 + i] = args[i];
    return n->fn(vm, argc, buf + 1);
}

// ---------------------------------------------------------------------------
// Llamadas desde C++
// ---------------------------------------------------------------------------

Value VM::call(Value fnv, int argc, const Value* args) {
    if (!fiber) {
        if (!hostFiber) hostFiber = alloc<ObjFiber>();
        fiber = hostFiber;
        struct Reset { VM* vm; ~Reset() { if (vm->fiber == vm->hostFiber && vm->hostFiber->frames.empty()) vm->fiber = nullptr; } } reset{this};
        return call(fnv, argc, args);
    }
    ObjFiber* f = fiber;
    f->ensure(argc + 8);
    int base = f->sp;
    f->stack[f->sp++] = fnv;
    for (int i = 0; i < argc; i++) f->stack[f->sp++] = args[i];
    int depth = (int)f->frames.size();
    try {
        callValue(fnv, argc);
    } catch (NativeError& e) {
        f->sp = base;
        pendingError = makeError(e.message);
        hasPendingError = true;
        lastError = pendingError;
        throw PropagateError{};
    }
    if ((int)f->frames.size() > depth) {
        RunResult r = run(depth);
        if (r == RunResult::Error) { f->sp = base; throw PropagateError{}; }
    }
    Value result = f->stack[f->sp - 1];
    f->sp = base;
    return result;
}

Value VM::callBound(Value method, Value self, int argc, const Value* args) {
    if (isObjType(method, OT::Native)) {
        std::vector<Value> buf(argc + 1);
        buf[0] = self;
        for (int i = 0; i < argc; i++) buf[i + 1] = args[i];
        DepthGuard g(nativeDepth);
        return static_cast<ObjNative*>(method.o)->fn(*this, argc, buf.data() + 1);
    }
    if (!isObjType(method, OT::Closure)) return call(method, argc, args);
    if (!fiber) {
        if (!hostFiber) hostFiber = alloc<ObjFiber>();
        fiber = hostFiber;
        struct Reset { VM* vm; ~Reset() { if (vm->fiber == vm->hostFiber && vm->hostFiber->frames.empty()) vm->fiber = nullptr; } } reset{this};
        return callBound(method, self, argc, args);
    }
    ObjFiber* f = fiber;
    f->ensure(argc + 8);
    int base = f->sp;
    f->stack[f->sp++] = self;
    for (int i = 0; i < argc; i++) f->stack[f->sp++] = args[i];
    int depth = (int)f->frames.size();
    try {
        callClosure(static_cast<ObjClosure*>(method.o), argc);
    } catch (NativeError& e) {
        f->sp = base;
        pendingError = makeError(e.message);
        hasPendingError = true;
        lastError = pendingError;
        throw PropagateError{};
    }
    if ((int)f->frames.size() > depth) {
        RunResult r = run(depth);
        if (r == RunResult::Error) { f->sp = base; throw PropagateError{}; }
    }
    Value result = f->stack[f->sp - 1];
    f->sp = base;
    return result;
}

Value VM::callMethod(Value receiver, const char* name, int argc, const Value* args) {
    if (!fiber) {
        if (!hostFiber) hostFiber = alloc<ObjFiber>();
        fiber = hostFiber;
        struct Reset { VM* vm; ~Reset() { if (vm->fiber == vm->hostFiber && vm->hostFiber->frames.empty()) vm->fiber = nullptr; } } reset{this};
        return callMethod(receiver, name, argc, args);
    }
    ObjFiber* f = fiber;
    f->ensure(argc + 8);
    int base = f->sp;
    f->stack[f->sp++] = receiver;
    for (int i = 0; i < argc; i++) f->stack[f->sp++] = args[i];
    int depth = (int)f->frames.size();
    try {
        invoke(intern(name), argc);
    } catch (NativeError& e) {
        f->sp = base;
        pendingError = makeError(e.message);
        hasPendingError = true;
        lastError = pendingError;
        throw PropagateError{};
    }
    if ((int)f->frames.size() > depth) {
        RunResult r = run(depth);
        if (r == RunResult::Error) { f->sp = base; throw PropagateError{}; }
    }
    Value result = f->stack[f->sp - 1];
    f->sp = base;
    return result;
}

bool VM::tryCall(Value fn, int argc, const Value* args, Value* out) {
    try {
        Value r = call(fn, argc, args);
        if (out) *out = r;
        return true;
    } catch (PropagateError&) {
        lastError = pendingError;
        hasPendingError = false;
        pendingError = Value();
        return false;
    } catch (NativeError& e) {
        lastError = makeError(e.message);
        return false;
    }
}

// ---------------------------------------------------------------------------
// Invocación de métodos
// ---------------------------------------------------------------------------

bool VM::invoke(ObjString* name, int argc) {
    ObjFiber* f = fiber;
    Value recv = f->stack[f->sp - argc - 1];
    if (recv.isObj()) {
        switch (recv.o->type) {
            case OT::Instance: {
                auto* inst = static_cast<ObjInstance*>(recv.o);
                auto fi = inst->cls->fieldIndex.find(name);
                if (fi != inst->cls->fieldIndex.end()) {
                    Value v = inst->fields[fi->second];
                    f->stack[f->sp - argc - 1] = v;
                    return callValue(v, argc);
                }
                auto m = inst->cls->methods.find(name);
                if (m != inst->cls->methods.end()) return callMethodValue(m->second, argc);
                if (inst->extra) {
                    auto e = inst->extra->find(name);
                    if (e != inst->extra->end()) { f->stack[f->sp - argc - 1] = e->second; return callValue(e->second, argc); }
                }
                auto g = inst->cls->getters.find(name);
                if (g != inst->cls->getters.end()) {
                    Value v = isObjType(g->second, OT::Native) ? callNativeAccessor(*this, g->second, recv, 0, nullptr) : callBound(g->second, recv, 0, nullptr);
                    f = fiber;
                    f->stack[f->sp - argc - 1] = v;
                    return callValue(v, argc);
                }
                std::vector<std::string> cands;
                for (auto& [k, v] : inst->cls->methods) cands.push_back(k->str);
                std::string sug = closestMatch(name->str, cands);
                raise("El objeto " + inst->cls->name->str + " no tiene el método '" + name->str + "'" + (sug.empty() ? "" : " (¿quisiste decir '" + sug + "'?)"));
            }
            case OT::Class: {
                auto* cls = static_cast<ObjClass*>(recv.o);
                for (auto* c = cls; c; c = c->super) {
                    auto s = c->statics.find(name);
                    if (s != c->statics.end()) {
                        Value v = s->second;
                        if (isObjType(v, OT::Closure) || isObjType(v, OT::Native)) return callMethodValue(v, argc);
                        f->stack[f->sp - argc - 1] = v;
                        return callValue(v, argc);
                    }
                }
                if (cls->methods.count(name))
                    raise("'" + name->str + "' es un método de instancia; necesitas un objeto de " + cls->name->str);
                raise("La clase " + cls->name->str + " no tiene el método estático '" + name->str + "'");
            }
            case OT::Module: {
                auto* m = static_cast<ObjModule*>(recv.o);
                auto it = m->exports.find(name);
                if (it != m->exports.end()) {
                    Value v = m->globals[it->second];
                    if (v.isUndef()) raise("'" + m->name->str + "." + name->str + "' todavía no tiene valor");
                    f->stack[f->sp - argc - 1] = v;
                    return callValue(v, argc);
                }
                auto p = m->props.find(name);
                if (p != m->props.end() && p->second.get) {
                    Value self = recv;
                    Value v = p->second.get(*this, 0, &self + 1);
                    f = fiber;
                    f->stack[f->sp - argc - 1] = v;
                    return callValue(v, argc);
                }
                std::vector<std::string> cands;
                for (auto& [k, v] : m->exports) cands.push_back(k->str);
                std::string sug = closestMatch(name->str, cands);
                raise("'" + m->name->str + "' no tiene ninguna función llamada '" + name->str + "'" + (sug.empty() ? "" : " (¿quisiste decir '" + sug + "'?)"));
            }
            case OT::Map: {
                auto* map = static_cast<ObjMap*>(recv.o);
                ObjClass* cls = map->isSet ? setClass : mapClass;
                auto m = cls->methods.find(name);
                if (m != cls->methods.end()) return callMethodValue(m->second, argc);
                if (!map->isSet) {
                    if (Value* v = map->find(Value::object(name))) {
                        Value fnv = *v;
                        f->stack[f->sp - argc - 1] = fnv;
                        return callValue(fnv, argc);
                    }
                }
                raise(std::string(map->isSet ? "El conjunto" : "El mapa") + " no tiene el método '" + name->str + "'");
            }
            default: break;
        }
    }
    if (recv.isNull()) raise("No se puede llamar al método '" + name->str + "' de null");
    ObjClass* cls = classOf(recv);
    if (cls) {
        auto m = cls->methods.find(name);
        if (m != cls->methods.end()) return callMethodValue(m->second, argc);
        auto g = cls->getters.find(name);
        if (g != cls->getters.end()) {
            Value v = isObjType(g->second, OT::Native) ? callNativeAccessor(*this, g->second, recv, 0, nullptr) : callBound(g->second, recv, 0, nullptr);
            f = fiber;
            f->stack[f->sp - argc - 1] = v;
            return callValue(v, argc);
        }
        std::vector<std::string> cands;
        for (auto& [k, v] : cls->methods) cands.push_back(k->str);
        std::string sug = closestMatch(name->str, cands);
        raise("El tipo " + typeName(recv) + " no tiene el método '" + name->str + "'" + (sug.empty() ? "" : " (¿quisiste decir '" + sug + "'?)"));
    }
    raise("No se puede llamar a métodos de un valor de tipo " + typeName(recv));
}

// ---------------------------------------------------------------------------
// Propiedades e índices
// ---------------------------------------------------------------------------

static const char* kVecNames[5][4] = {{"x", "y", "", ""}, {"x", "y", "z", ""}, {"x", "y", "z", "w"}, {"r", "g", "b", "a"}, {"x", "y", "z", "w"}};

static int vecComponent(ObjVec* v, const std::string& n) {
    for (int i = 0; i < v->dims(); i++) if (n == kVecNames[v->kind][i]) return i;
    if (v->kind == VK_COLOR) return -1;
    return -1;
}

Value VM::getProperty(Value obj, ObjString* name, bool optional) {
    if (obj.isObj()) {
        switch (obj.o->type) {
            case OT::Instance: {
                auto* inst = static_cast<ObjInstance*>(obj.o);
                auto fi = inst->cls->fieldIndex.find(name);
                if (fi != inst->cls->fieldIndex.end()) return inst->fields[fi->second];
                auto g = inst->cls->getters.find(name);
                if (g != inst->cls->getters.end())
                    return isObjType(g->second, OT::Native) ? callNativeAccessor(*this, g->second, obj, 0, nullptr) : callBound(g->second, obj, 0, nullptr);
                if (inst->extra) {
                    auto e = inst->extra->find(name);
                    if (e != inst->extra->end()) return e->second;
                }
                auto m = inst->cls->methods.find(name);
                if (m != inst->cls->methods.end()) return bindMethod(obj, m->second);
                if (inst->cls == eventDataClass || inst->cls->isSubclassOf(eventDataClass)) return Value();
                std::vector<std::string> cands;
                for (auto* fnm : inst->cls->fieldNames) cands.push_back(fnm->str);
                for (auto& [k, v] : inst->cls->methods) cands.push_back(k->str);
                for (auto& [k, v] : inst->cls->getters) cands.push_back(k->str);
                std::string sug = closestMatch(name->str, cands);
                raise("El objeto " + inst->cls->name->str + " no tiene la propiedad '" + name->str + "'" + (sug.empty() ? "" : " (¿quisiste decir '" + sug + "'?)"));
            }
            case OT::Vec: {
                auto* v = static_cast<ObjVec*>(obj.o);
                int c = vecComponent(v, name->str);
                if (c >= 0) return Value(v->v[c]);
                break;
            }
            case OT::Class: {
                auto* cls = static_cast<ObjClass*>(obj.o);
                for (auto* c = cls; c; c = c->super) {
                    auto s = c->statics.find(name);
                    if (s != c->statics.end()) return s->second;
                    auto g = c->staticGetters.find(name);
                    if (g != c->staticGetters.end())
                        return isObjType(g->second, OT::Native) ? callNativeAccessor(*this, g->second, obj, 0, nullptr) : callBound(g->second, obj, 0, nullptr);
                }
                if (name == s_name) return Value::object(cls->name);
                if (cls->methods.count(name)) raise("'" + name->str + "' es un miembro de instancia; necesitas un objeto de " + cls->name->str);
                raise("La clase " + cls->name->str + " no tiene el miembro estático '" + name->str + "'");
            }
            case OT::Module: {
                auto* m = static_cast<ObjModule*>(obj.o);
                auto it = m->exports.find(name);
                if (it != m->exports.end()) {
                    Value v = m->globals[it->second];
                    if (v.isUndef()) raise("'" + m->name->str + "." + name->str + "' todavía no tiene valor");
                    return v;
                }
                auto p = m->props.find(name);
                if (p != m->props.end() && p->second.get) {
                    Value buf[2] = {obj, Value()};
                    return p->second.get(*this, 0, buf + 1);
                }
                std::vector<std::string> cands;
                for (auto& [k, v] : m->exports) cands.push_back(k->str);
                for (auto& [k, v] : m->props) cands.push_back(k->str);
                std::string sug = closestMatch(name->str, cands);
                raise("'" + m->name->str + "' no tiene ningún miembro llamado '" + name->str + "'" + (sug.empty() ? "" : " (¿quisiste decir '" + sug + "'?)"));
            }
            case OT::Map: {
                auto* map = static_cast<ObjMap*>(obj.o);
                if (!map->isSet) if (Value* v = map->find(Value::object(name))) return *v;
                ObjClass* cls = map->isSet ? setClass : mapClass;
                auto g = cls->getters.find(name);
                if (g != cls->getters.end()) return callNativeAccessor(*this, g->second, obj, 0, nullptr);
                auto m = cls->methods.find(name);
                if (m != cls->methods.end()) return bindMethod(obj, m->second);
                return Value();
            }
            case OT::Enum: {
                auto* e = static_cast<ObjEnum*>(obj.o);
                auto it = e->byName.find(name);
                if (it != e->byName.end()) return Value::object(it->second);
                break;
            }
            case OT::EnumValue: {
                auto* e = static_cast<ObjEnumValue*>(obj.o);
                if (name->str == "name") return Value::object(e->name);
                if (name->str == "value") return e->value;
                if (name->str == "ordinal") return Value((double)e->ordinal);
                break;
            }
            default: break;
        }
    }
    if (obj.isNull()) {
        if (optional) return Value();
        raise("No se puede leer la propiedad '" + name->str + "' de null");
    }
    ObjClass* cls = classOf(obj);
    if (cls) {
        auto g = cls->getters.find(name);
        if (g != cls->getters.end())
            return isObjType(g->second, OT::Native) ? callNativeAccessor(*this, g->second, obj, 0, nullptr) : callBound(g->second, obj, 0, nullptr);
        auto m = cls->methods.find(name);
        if (m != cls->methods.end()) return bindMethod(obj, m->second);
        std::vector<std::string> cands;
        for (auto& [k, v] : cls->methods) cands.push_back(k->str);
        for (auto& [k, v] : cls->getters) cands.push_back(k->str);
        std::string sug = closestMatch(name->str, cands);
        raise("El tipo " + typeName(obj) + " no tiene la propiedad '" + name->str + "'" + (sug.empty() ? "" : " (¿quisiste decir '" + sug + "'?)"));
    }
    raise("Un valor de tipo " + typeName(obj) + " no tiene propiedades");
}

void VM::setProperty(Value obj, ObjString* name, Value v) {
    if (obj.isObj()) {
        switch (obj.o->type) {
            case OT::Instance: {
                auto* inst = static_cast<ObjInstance*>(obj.o);
                auto fi = inst->cls->fieldIndex.find(name);
                if (fi != inst->cls->fieldIndex.end()) { inst->fields[fi->second] = v; return; }
                auto s = inst->cls->setters.find(name);
                if (s != inst->cls->setters.end()) {
                    if (isObjType(s->second, OT::Native)) callNativeAccessor(*this, s->second, obj, 1, &v);
                    else callBound(s->second, obj, 1, &v);
                    return;
                }
                if (inst->cls->getters.count(name)) raise("La propiedad '" + name->str + "' de " + inst->cls->name->str + " es de solo lectura");
                if (inst->cls->methods.count(name)) raise("No se puede reemplazar el método '" + name->str + "'");
                if (!inst->extra) inst->extra = std::make_unique<StrMap<Value>>();
                (*inst->extra)[name] = v;
                return;
            }
            case OT::Vec: {
                auto* vec = static_cast<ObjVec*>(obj.o);
                int c = vecComponent(vec, name->str);
                if (c >= 0) {
                    if (!v.isNum()) raise("El componente '" + name->str + "' debe ser un número, no " + typeName(v));
                    vec->v[c] = v.n;
                    return;
                }
                break;
            }
            case OT::Class: {
                auto* cls = static_cast<ObjClass*>(obj.o);
                for (auto* c = cls; c; c = c->super) {
                    auto s = c->staticSetters.find(name);
                    if (s != c->staticSetters.end()) {
                        if (isObjType(s->second, OT::Native)) callNativeAccessor(*this, s->second, obj, 1, &v);
                        else callBound(s->second, obj, 1, &v);
                        return;
                    }
                }
                for (auto* c = cls; c; c = c->super) {
                    auto it = c->statics.find(name);
                    if (it != c->statics.end()) { it->second = v; return; }
                }
                cls->statics[name] = v;
                return;
            }
            case OT::Module: {
                auto* m = static_cast<ObjModule*>(obj.o);
                auto p = m->props.find(name);
                if (p != m->props.end()) {
                    if (!p->second.set) raise("'" + m->name->str + "." + name->str + "' es de solo lectura");
                    Value buf[2] = {obj, v};
                    p->second.set(*this, 1, buf + 1);
                    return;
                }
                if (m->isNative) raise("No se puede cambiar '" + m->name->str + "." + name->str + "'");
                auto it = m->exports.find(name);
                if (it != m->exports.end()) { m->globals[it->second] = v; return; }
                raise("'" + m->name->str + "' no tiene ningún miembro llamado '" + name->str + "'");
            }
            case OT::Map: {
                auto* map = static_cast<ObjMap*>(obj.o);
                if (map->isSet) raise("No se pueden asignar propiedades a un conjunto");
                map->set(Value::object(name), v);
                return;
            }
            case OT::Foreign: {
                auto* fo = static_cast<ObjForeign*>(obj.o);
                auto s = fo->cls->setters.find(name);
                if (s != fo->cls->setters.end()) { callNativeAccessor(*this, s->second, obj, 1, &v); return; }
                if (fo->cls->getters.count(name)) raise("La propiedad '" + name->str + "' de " + fo->cls->name->str + " es de solo lectura");
                raise(fo->cls->name->str + " no tiene la propiedad '" + name->str + "'");
            }
            default: break;
        }
    }
    if (obj.isNull()) raise("No se puede asignar la propiedad '" + name->str + "' de null");
    ObjClass* cls = classOf(obj);
    if (cls) {
        auto s = cls->setters.find(name);
        if (s != cls->setters.end()) { callNativeAccessor(*this, s->second, obj, 1, &v); return; }
    }
    raise("No se puede asignar la propiedad '" + name->str + "' a un valor de tipo " + typeName(obj));
}

static long checkIndex(VM& vm, Value idx, size_t size, const char* what) {
    if (!idx.isNum()) vm.raise(std::string("El índice de ") + what + " debe ser un número, no " + vm.typeName(idx));
    double d = idx.n;
    if (d != std::floor(d)) vm.raise("El índice debe ser un número entero, no " + formatNumber(d));
    long i = (long)d;
    if (i < 0) i += (long)size;
    if (i < 0 || i >= (long)size)
        vm.raise(std::string("Índice fuera de rango: ") + formatNumber(d) + " (" + what + " tiene " + std::to_string(size) + " elemento" + (size == 1 ? "" : "s") + ")");
    return i;
}

Value VM::getIndex(Value obj, Value idx) {
    if (obj.isObj()) {
        switch (obj.o->type) {
            case OT::List: {
                auto& items = static_cast<ObjList*>(obj.o)->items;
                if (isObjType(idx, OT::Range)) {
                    auto* r = static_cast<ObjRange*>(idx.o);
                    ObjList* out = newList();
                    double n = r->count();
                    for (double k = 0; k < n; k++) {
                        long i = (long)(r->start + k * r->step);
                        if (i < 0) i += (long)items.size();
                        if (i >= 0 && i < (long)items.size()) out->items.push_back(items[i]);
                    }
                    return Value::object(out);
                }
                return items[checkIndex(*this, idx, items.size(), "la lista")];
            }
            case OT::Map: {
                auto* m = static_cast<ObjMap*>(obj.o);
                if (m->isSet) raise("Los conjuntos no se indexan; usa 'x in conjunto'");
                Value* v = m->find(idx);
                return v ? *v : Value();
            }
            case OT::String: {
                const std::string& s = asString(obj)->str;
                size_t len = utf8Length(s);
                if (isObjType(idx, OT::Range)) {
                    auto* r = static_cast<ObjRange*>(idx.o);
                    long a = (long)r->start, b = (long)r->end + (r->inclusive ? 1 : 0);
                    if (a < 0) a += (long)len;
                    if (b < 0) b += (long)len;
                    a = std::clamp(a, 0L, (long)len);
                    b = std::clamp(b, a, (long)len);
                    size_t sa = utf8Offset(s, a), sb = utf8Offset(s, b);
                    return str(s.substr(sa, sb - sa));
                }
                long i = checkIndex(*this, idx, len, "el texto");
                size_t off = utf8Offset(s, i);
                size_t end = off;
                utf8Decode(s, end);
                return str(s.substr(off, end - off));
            }
            case OT::Instance: case OT::Foreign: case OT::Class: case OT::Module:
                if (isString(idx)) return getProperty(obj, asString(idx), false);
                if (obj.o->type == OT::Instance || obj.o->type == OT::Foreign) {
                    ObjClass* cls = classOf(obj);
                    auto m = cls->methods.find(intern("get"));
                    if (m != cls->methods.end()) return callBound(m->second, obj, 1, &idx);
                }
                break;
            case OT::Vec: {
                auto* v = static_cast<ObjVec*>(obj.o);
                return Value(v->v[checkIndex(*this, idx, v->dims(), "el vector")]);
            }
            case OT::Range: {
                auto* r = static_cast<ObjRange*>(obj.o);
                long i = checkIndex(*this, idx, (size_t)r->count(), "el rango");
                return Value(r->start + i * r->step);
            }
            default: break;
        }
    }
    if (obj.isNull()) raise("No se puede indexar null");
    raise("No se puede indexar un valor de tipo " + typeName(obj));
}

void VM::setIndex(Value obj, Value idx, Value v) {
    if (obj.isObj()) {
        switch (obj.o->type) {
            case OT::List: {
                auto& items = static_cast<ObjList*>(obj.o)->items;
                items[checkIndex(*this, idx, items.size(), "la lista")] = v;
                return;
            }
            case OT::Map: {
                auto* m = static_cast<ObjMap*>(obj.o);
                if (m->isSet) raise("Los conjuntos no se indexan; usa conjunto.add(x)");
                if (idx.isNull()) raise("La clave de un mapa no puede ser null");
                m->set(idx, v);
                return;
            }
            case OT::Instance: case OT::Foreign: case OT::Class: case OT::Module:
                if (isString(idx)) { setProperty(obj, asString(idx), v); return; }
                if (obj.o->type == OT::Instance || obj.o->type == OT::Foreign) {
                    ObjClass* cls = classOf(obj);
                    auto m = cls->methods.find(intern("set"));
                    if (m != cls->methods.end()) {
                        Value pair[2] = {idx, v};
                        callBound(m->second, obj, 2, pair);
                        return;
                    }
                }
                break;
            case OT::Vec: {
                auto* vec = static_cast<ObjVec*>(obj.o);
                if (!v.isNum()) raise("Los componentes de un vector son números");
                vec->v[checkIndex(*this, idx, vec->dims(), "el vector")] = v.n;
                return;
            }
            case OT::String: raise("Los textos no se pueden modificar; crea uno nuevo");
            default: break;
        }
    }
    if (obj.isNull()) raise("No se puede indexar null");
    raise("No se puede asignar por índice a un valor de tipo " + typeName(obj));
}

// ---------------------------------------------------------------------------
// Operadores
// ---------------------------------------------------------------------------

static const char* opWord(uint8_t op) {
    switch (op) {
        case OP_ADD: return "sumar";
        case OP_SUB: return "restar";
        case OP_MUL: return "multiplicar";
        case OP_DIV: return "dividir";
        case OP_MOD: return "calcular el módulo de";
        case OP_POW: return "elevar";
        case OP_LT: case OP_LE: case OP_GT: case OP_GE: return "comparar";
        default: return "operar con";
    }
}

bool VM::objectsEqual(const Value& a, const Value& b) {
    if (ObjClass* cls = classOf(a)) {
        auto m = cls->methods.find(intern("equals"));
        if (m != cls->methods.end()) return !isFalsy(callBound(m->second, a, 1, &b));
    }
    return valuesEqual(a, b);
}

Value VM::arith(uint8_t op, Value a, Value b) {
    if (a.isNum() && b.isNum()) {
        double x = a.n, y = b.n;
        switch (op) {
            case OP_ADD: return Value(x + y);
            case OP_SUB: return Value(x - y);
            case OP_MUL: return Value(x * y);
            case OP_DIV: if (y == 0) raise("División entre cero"); return Value(x / y);
            case OP_MOD: { if (y == 0) raise("Módulo entre cero"); double r = std::fmod(x, y); if (r != 0 && ((r < 0) != (y < 0))) r += y; return Value(r); }
            case OP_POW: return Value(std::pow(x, y));
            case OP_LT: return Value::boolean(x < y);
            case OP_LE: return Value::boolean(x <= y);
            case OP_GT: return Value::boolean(x > y);
            case OP_GE: return Value::boolean(x >= y);
            case OP_BAND: return Value((double)((int64_t)x & (int64_t)y));
            case OP_BOR: return Value((double)((int64_t)x | (int64_t)y));
            case OP_BXOR: return Value((double)((int64_t)x ^ (int64_t)y));
            case OP_SHL: return Value((double)((int64_t)x << ((int64_t)y & 63)));
            case OP_SHR: return Value((double)((int64_t)x >> ((int64_t)y & 63)));
        }
    }
    if (op == OP_ADD && (isString(a) || isString(b))) {
        std::string s;
        if (isString(a) && isString(b)) {
            s.reserve(asString(a)->str.size() + asString(b)->str.size());
            s = asString(a)->str;
            s += asString(b)->str;
        } else {
            s = isString(a) ? asString(a)->str : toString(a);
            s += isString(b) ? asString(b)->str : toString(b);
        }
        return Value::object(internMove(std::move(s)));
    }
    if (isString(a) && isString(b) && (op == OP_LT || op == OP_LE || op == OP_GT || op == OP_GE)) {
        int c = asString(a)->str.compare(asString(b)->str);
        switch (op) {
            case OP_LT: return Value::boolean(c < 0);
            case OP_LE: return Value::boolean(c <= 0);
            case OP_GT: return Value::boolean(c > 0);
            default: return Value::boolean(c >= 0);
        }
    }
    if (op == OP_MUL && isString(a) && b.isNum()) {
        std::string s;
        int n = (int)b.n;
        if (n > 0) {
            if ((double)asString(a)->str.size() * n > 1e9) raise("El texto repetido sería demasiado grande");
            s.reserve(asString(a)->str.size() * (size_t)n);
            for (int i = 0; i < n; i++) s += asString(a)->str;
        }
        return str(s);
    }
    if (op == OP_MUL && isObjType(a, OT::List) && b.isNum()) { // [0] * 5 = [0, 0, 0, 0, 0]
        ObjList* r = newList();
        auto& x = static_cast<ObjList*>(a.o)->items;
        long long n = (long long)b.n;
        if (n > 0) {
            if ((double)x.size() * (double)n > 1e8) raise("La lista repetida sería demasiado grande");
            r->items.reserve(x.size() * (size_t)n);
            for (long long i = 0; i < n; i++) r->items.insert(r->items.end(), x.begin(), x.end());
        }
        return Value::object(r);
    }
    if (op == OP_ADD && isObjType(a, OT::List) && isObjType(b, OT::List)) {
        ObjList* r = newList();
        auto& x = static_cast<ObjList*>(a.o)->items;
        auto& y = static_cast<ObjList*>(b.o)->items;
        r->items.reserve(x.size() + y.size());
        r->items.insert(r->items.end(), x.begin(), x.end());
        r->items.insert(r->items.end(), y.begin(), y.end());
        return Value::object(r);
    }
    bool av = isObjType(a, OT::Vec), bv = isObjType(b, OT::Vec);
    if ((av || bv) && op <= OP_POW) {
        if (av && bv) {
            auto* x = static_cast<ObjVec*>(a.o);
            auto* y = static_cast<ObjVec*>(b.o);
            if (x->kind != y->kind) raise("No se puede " + std::string(opWord(op)) + " " + typeName(a) + " y " + typeName(b));
            ObjVec* r = newVec(x->kind);
            for (int i = 0; i < 4; i++) {
                switch (op) {
                    case OP_ADD: r->v[i] = x->v[i] + y->v[i]; break;
                    case OP_SUB: r->v[i] = x->v[i] - y->v[i]; break;
                    case OP_MUL: r->v[i] = x->v[i] * y->v[i]; break;
                    case OP_DIV: r->v[i] = y->v[i] == 0 ? 0 : x->v[i] / y->v[i]; break;
                    default: raise("Operación no válida entre vectores");
                }
            }
            return Value::object(r);
        }
        if (av && b.isNum() && (op == OP_MUL || op == OP_DIV)) {
            auto* x = static_cast<ObjVec*>(a.o);
            if (op == OP_DIV && b.n == 0) raise("División entre cero");
            ObjVec* r = newVec(x->kind);
            for (int i = 0; i < 4; i++) r->v[i] = op == OP_MUL ? x->v[i] * b.n : x->v[i] / b.n;
            if (x->kind == VK_COLOR) r->v[3] = x->v[3];
            return Value::object(r);
        }
        if (bv && a.isNum() && op == OP_MUL) {
            auto* y = static_cast<ObjVec*>(b.o);
            ObjVec* r = newVec(y->kind);
            for (int i = 0; i < 4; i++) r->v[i] = y->v[i] * a.n;
            if (y->kind == VK_COLOR) r->v[3] = y->v[3];
            return Value::object(r);
        }
    }
    // Sobrecarga de operadores (clases propias y nativas como Matrix): métodos add, sub, mul, div, mod y pow,
    // y compareTo (devuelve un número negativo, 0 o positivo) para <, <=, > y >=.
    auto opMethod = [&](const Value& v, const char* name) -> Value* {
        if (!(isObjType(v, OT::Instance) || isObjType(v, OT::Foreign))) return nullptr;
        ObjClass* cls = classOf(v);
        auto m = cls->methods.find(intern(name));
        return m == cls->methods.end() ? nullptr : &m->second;
    };
    const char* mname = op == OP_ADD ? "add" : op == OP_SUB ? "sub" : op == OP_MUL ? "mul" : op == OP_DIV ? "div"
                      : op == OP_MOD ? "mod" : op == OP_POW ? "pow" : nullptr;
    if (mname) {
        if (Value* m = opMethod(a, mname)) return callBound(*m, a, 1, &b);
        // número + objeto y número * objeto: se usan los del objeto (son conmutativas)
        if (a.isNum() && (op == OP_ADD || op == OP_MUL))
            if (Value* m = opMethod(b, mname)) return callBound(*m, b, 1, &a);
    } else if (op == OP_LT || op == OP_LE || op == OP_GT || op == OP_GE) {
        if (Value* m = opMethod(a, "compareTo")) {
            Value r = callBound(*m, a, 1, &b);
            if (!r.isNum()) raise("compareTo debe devolver un número (negativo, 0 o positivo), no " + typeName(r));
            double c = r.n;
            return Value::boolean(op == OP_LT ? c < 0 : op == OP_LE ? c <= 0 : op == OP_GT ? c > 0 : c >= 0);
        }
    }
    if (a.isNull() || b.isNull())
        raise("No se puede " + std::string(opWord(op)) + " " + typeName(a) + " y " + typeName(b) + " (uno de los valores es null)");
    raise("No se puede " + std::string(opWord(op)) + " " + typeName(a) + " y " + typeName(b));
}

bool VM::contains(Value c, Value item) {
    if (c.isObj()) {
        switch (c.o->type) {
            case OT::List:
                for (auto& v : static_cast<ObjList*>(c.o)->items) if (valuesEqual(v, item)) return true;
                return false;
            case OT::Map: return static_cast<ObjMap*>(c.o)->find(item) != nullptr;
            case OT::String:
                if (!isString(item)) raise("Para buscar dentro de un texto necesitas otro texto");
                return asString(c)->str.find(asString(item)->str) != std::string::npos;
            case OT::Range: {
                if (!item.isNum()) return false;
                auto* r = static_cast<ObjRange*>(c.o);
                double k = (item.n - r->start) / r->step;
                return k >= 0 && k < r->count() && std::floor(k) == k;
            }
            case OT::Enum:
                for (auto* v : static_cast<ObjEnum*>(c.o)->values) if (item.isObj() && item.o == v) return true;
                return false;
            default: break;
        }
    }
    raise("'in' necesita una lista, mapa, conjunto, texto o rango, no " + typeName(c));
}

// ---------------------------------------------------------------------------
// Fibras
// ---------------------------------------------------------------------------

bool VM::startAsync(ObjClosure* c, int argc) {
    ObjFiber* caller = fiber;
    ObjFiber* nf = alloc<ObjFiber>();
    ObjTask* task = newTask();
    task->fiber = nf;
    nf->task = task;
    nf->closure = Value::object(c);
    tempRoots.push_back(Value::object(task));
    int base = caller->sp - argc - 1;
    nf->ensure(c->fn->maxSlots + argc + 64);
    for (int i = 0; i <= argc; i++) nf->stack[i] = caller->stack[base + i];
    nf->sp = argc + 1;
    caller->sp = base;
    nf->frames.push_back({c, c->fn->code.data(), 0});
    pendingTasks.push_back(task);
    runFiber(nf);
    caller->ensure(1);
    caller->stack[caller->sp++] = Value::object(task);
    tempRoots.pop_back();
    return true;
}

RunResult VM::runFiber(ObjFiber* nf, bool resume, Value v, bool isError) {
    ObjFiber* saved = fiber;
    if (saved) fiberStack.push_back(saved);
    fiber = nf;
    nf->state = FiberState::Running;
    nf->waitingOn = nullptr;
    int savedExit = currentExitDepth;
    currentExitDepth = 0;
    RunResult r = RunResult::Ok;
    bool started = true;
    if (resume) {
        if (isError) {
            if (!throwValue(v)) { r = RunResult::Error; started = false; }
        } else {
            nf->ensure(1);
            nf->stack[nf->sp++] = v;
        }
    }
    if (started) {
        try {
            r = run(0);
        } catch (NativeError& e) {
            pendingError = makeError(e.message);
            hasPendingError = true;
            r = RunResult::Error;
        } catch (PropagateError&) {
            r = RunResult::Error;
        }
    }
    currentExitDepth = savedExit;
    fiber = saved;
    if (saved) fiberStack.pop_back();
    if (r == RunResult::Ok) {
        Value result = nf->sp > 0 ? nf->stack[nf->sp - 1] : Value();
        nf->sp = 0;
        nf->state = FiberState::Done;
        if (nf->task) completeTask(nf->task, result);
    } else if (r == RunResult::Yield) {
        nf->state = FiberState::Suspended;
    } else {
        nf->state = FiberState::Failed;
        Value err = pendingError;
        hasPendingError = false;
        pendingError = Value();
        lastError = err;
        nf->frames.clear();
        nf->sp = 0;
        if (nf->task) failTask(nf->task, err);
    }
    return r;
}

void VM::resumeFiber(ObjFiber* f, Value v, bool isError) {
    if (f == mainFiber && !f->task) {
        RunResult r = runFiber(f, true, v, isError);
        if (r == RunResult::Error) { hadUncaught = true; reportError(lastError); }
        return;
    }
    runFiber(f, true, v, isError);
}

bool VM::runModule(ModuleUnit* unit) {
    if (!unit || !unit->script) return false;
    unit->module->initialized = true;
    ObjFiber* f = alloc<ObjFiber>();
    ObjClosure* c = alloc<ObjClosure>(unit->script);
    f->closure = Value::object(c);
    f->stack[0] = Value::object(c);
    f->sp = 1;
    f->ensure(unit->script->maxSlots + 64);
    f->frames.push_back({c, c->fn->code.data(), 0});
    bool isMain = unit == entryUnit;
    if (isMain) mainFiber = f;
    tempRoots.push_back(Value::object(f));
    RunResult r = runFiber(f);
    tempRoots.pop_back();
    if (r == RunResult::Error) {
        hadUncaught = true;
        reportError(lastError);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Bucle principal
// ---------------------------------------------------------------------------

RunResult VM::run(int exitDepth) {
    int savedExit = currentExitDepth;
    currentExitDepth = exitDepth;
    struct Restore { VM* vm; int v; ~Restore() { vm->currentExitDepth = v; } } restore{this, savedExit};

    ObjFiber* f = fiber;
    CallFrame* frame = nullptr;
    const uint8_t* ip = nullptr;
    ObjFunction* fn = nullptr;
    Value* slots = nullptr;
    const Value* consts = nullptr;

#define LOAD()                                         \
    do {                                               \
        frame = &f->frames.back();                     \
        ip = frame->ip;                                \
        fn = frame->closure->fn;                       \
        slots = f->stack.data() + frame->base;         \
        consts = fn->constants.data();                 \
    } while (0)
#define SAVE() (frame->ip = ip)
#define RD8() (*ip++)
#define RD16() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define STK (f->stack)
#define PUSH(val)                                                         \
    do {                                                                  \
        Value _pv = (val);                                                \
        if (f->sp >= (int)STK.size()) { f->ensure(64); slots = STK.data() + frame->base; } \
        STK[f->sp++] = _pv;                                               \
    } while (0)
#define POP() (STK[--f->sp])
// Entra en una función del script sin pasar por callValue si los argumentos encajan exactamente.
#define TRY_FAST_CALL(closure, argcount)                                                   \
    do {                                                                                   \
        ObjClosure* fc_ = (closure);                                                       \
        ObjFunction* ff_ = fc_->fn;                                                        \
        if ((argcount) == ff_->arity && !ff_->hasRest && !ff_->isAsync && (int)f->frames.size() < kMaxFrames) { \
            SAVE();                                                                        \
            f->ensure(ff_->maxSlots + 64);                                                 \
            f->frames.push_back({fc_, ff_->code.data(), f->sp - (argcount) - 1});          \
            LOAD();                                                                        \
            goto next;                                                                     \
        }                                                                                  \
    } while (0)
#define PEEK(d) (STK[f->sp - 1 - (d)])
#define THROWV(errv)                                  \
    do {                                              \
        SAVE();                                       \
        Value _e = (errv);                            \
        if (!throwValue(_e)) return RunResult::Error; \
        LOAD();                                       \
        goto next;                                    \
    } while (0)
#define ERR(msg) THROWV(makeError(msg))
#define GUARD(stmt)                                                         \
    do {                                                                    \
        SAVE();                                                             \
        try {                                                               \
            stmt;                                                           \
        } catch (NativeError & e__) {                                       \
            Value _e = makeError(e__.message);                              \
            if (!throwValue(_e)) return RunResult::Error;                   \
            LOAD();                                                         \
            goto next;                                                      \
        } catch (PropagateError&) {                                         \
            Value _e = pendingError;                                        \
            hasPendingError = false;                                        \
            if (!throwValue(_e)) return RunResult::Error;                   \
            LOAD();                                                         \
            goto next;                                                      \
        }                                                                   \
        f = fiber;                                                          \
        LOAD();                                                             \
    } while (0)


    LOAD();
    for (;;) {
    next:
        uint8_t op = RD8();
        switch (op) {
            case OP_CONST: PUSH(consts[RD16()]); break;
            case OP_NULL: PUSH(Value()); break;
            case OP_TRUE: PUSH(Value::boolean(true)); break;
            case OP_FALSE: PUSH(Value::boolean(false)); break;
            case OP_UNDEF: PUSH(Value::undef()); break;
            case OP_POP: f->sp--; break;
            case OP_DUP: PUSH(PEEK(0)); break;
            case OP_DUP2: { Value a = PEEK(1), b = PEEK(0); PUSH(a); PUSH(b); break; }
            case OP_SWAP: std::swap(STK[f->sp - 1], STK[f->sp - 2]); break;
            case OP_GET_LOCAL: PUSH(slots[RD16()]); break;
            case OP_SET_LOCAL: slots[RD16()] = PEEK(0); break;
            case OP_SET_LOCAL_POP: slots[RD16()] = POP(); break;
            case OP_GET_UPVAL: {
                ObjUpvalue* u = frame->closure->upvalues[RD16()];
                PUSH(u->open ? u->fiber->stack[u->slot] : u->closed);
                break;
            }
            case OP_SET_UPVAL: {
                ObjUpvalue* u = frame->closure->upvalues[RD16()];
                if (u->open) u->fiber->stack[u->slot] = PEEK(0); else u->closed = PEEK(0);
                break;
            }
            case OP_GET_GLOBAL: {
                uint16_t i = RD16();
                Value v = fn->module->globals[i];
                if (v.isUndef()) ERR("La variable '" + fn->module->globalNames[i]->str + "' se usa antes de tener un valor");
                PUSH(v);
                break;
            }
            case OP_SET_GLOBAL: fn->module->globals[RD16()] = PEEK(0); break;
            case OP_DEFINE_GLOBAL: fn->module->globals[RD16()] = POP(); break;
            case OP_GET_BUILTIN: PUSH(builtins[RD16()]); break;
            case OP_GET_PROP: {
                ObjString* name = asString(consts[RD16()]);
                uint8_t* cache = const_cast<uint8_t*>(ip++);
                Value obj = PEEK(0);
                if (isObjType(obj, OT::Instance)) {
                    auto* inst = static_cast<ObjInstance*>(obj.o);
                    uint8_t ci = *cache;
                    if (ci < inst->fields.size() && inst->cls->fieldNames[ci] == name) { STK[f->sp - 1] = inst->fields[ci]; break; }
                    auto it = inst->cls->fieldIndex.find(name);
                    if (it != inst->cls->fieldIndex.end()) {
                        if (it->second < 0xFF) *cache = (uint8_t)it->second;
                        STK[f->sp - 1] = inst->fields[it->second];
                        break;
                    }
                }
                Value r;
                GUARD(r = getProperty(obj, name, false));
                STK[f->sp - 1] = r;
                break;
            }
            case OP_SET_PROP: {
                ObjString* name = asString(consts[RD16()]);
                uint8_t* cache = const_cast<uint8_t*>(ip++);
                Value v = PEEK(0), obj = PEEK(1);
                if (isObjType(obj, OT::Instance)) {
                    auto* inst = static_cast<ObjInstance*>(obj.o);
                    uint8_t ci = *cache;
                    if (ci < inst->fields.size() && inst->cls->fieldNames[ci] == name) { inst->fields[ci] = v; STK[f->sp - 2] = v; f->sp--; break; }
                    auto it = inst->cls->fieldIndex.find(name);
                    if (it != inst->cls->fieldIndex.end()) {
                        if (it->second < 0xFF) *cache = (uint8_t)it->second;
                        inst->fields[it->second] = v; STK[f->sp - 2] = v; f->sp--;
                        break;
                    }
                }
                GUARD(setProperty(obj, name, v));
                STK[f->sp - 2] = v;
                f->sp--;
                break;
            }
            case OP_GET_INDEX: {
                Value idx = PEEK(0), obj = PEEK(1);
                if (isObjType(obj, OT::List) && idx.isNum()) {
                    auto& items = static_cast<ObjList*>(obj.o)->items;
                    double d = idx.n;
                    long i = (long)d;
                    if (d == (double)i) {
                        if (i < 0) i += (long)items.size();
                        if (i >= 0 && i < (long)items.size()) { STK[f->sp - 2] = items[i]; f->sp--; break; }
                    }
                }
                Value r;
                GUARD(r = getIndex(obj, idx));
                STK[f->sp - 2] = r;
                f->sp--;
                break;
            }
            case OP_SET_INDEX: {
                Value v = PEEK(0), idx = PEEK(1), obj = PEEK(2);
                GUARD(setIndex(obj, idx, v));
                STK[f->sp - 3] = v;
                f->sp -= 2;
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = asString(consts[RD16()]);
                ObjClass* owner = frame->closure->owner;
                if (!owner || !owner->super) ERR("'super' no está disponible aquí");
                auto m = owner->super->methods.find(name);
                if (m == owner->super->methods.end()) {
                    auto g = owner->super->getters.find(name);
                    if (g != owner->super->getters.end()) {
                        Value self = PEEK(0), r;
                        GUARD(r = callBound(g->second, self, 0, nullptr));
                        STK[f->sp - 1] = r;
                        break;
                    }
                    ERR("La clase base " + owner->super->name->str + " no tiene '" + name->str + "'");
                }
                STK[f->sp - 1] = bindMethod(PEEK(0), m->second);
                break;
            }
            case OP_ADD: {
                Value b = PEEK(0), a = PEEK(1);
                if (a.isNum() && b.isNum()) { STK[f->sp - 2] = Value(a.n + b.n); f->sp--; break; }
                Value r;
                GUARD(r = arith(OP_ADD, a, b));
                STK[f->sp - 2] = r;
                f->sp--;
                break;
            }
            // Operaciones con dos números: cada una tiene su propio caso (sin un segundo switch) y, si los
            // operandos no son números, todas comparten el camino lento (arith: textos, vectores, sobrecarga).
#define NUM_BINOP(OPC, EXPR)                                         \
            case OPC: {                                              \
                Value b_ = PEEK(0), a_ = PEEK(1);                    \
                if (a_.isNum() && b_.isNum()) {                      \
                    double x = a_.n, y = b_.n;                       \
                    (void)x; (void)y;                                \
                    STK[f->sp - 2] = EXPR;                           \
                    f->sp--;                                         \
                    break;                                           \
                }                                                    \
                goto arithSlow;                                      \
            }
            NUM_BINOP(OP_SUB, Value(x - y))
            NUM_BINOP(OP_MUL, Value(x * y))
            NUM_BINOP(OP_LT, Value::boolean(x < y))
            NUM_BINOP(OP_LE, Value::boolean(x <= y))
            NUM_BINOP(OP_GT, Value::boolean(x > y))
            NUM_BINOP(OP_GE, Value::boolean(x >= y))
#undef NUM_BINOP
            case OP_DIV: {
                Value b = PEEK(0), a = PEEK(1);
                if (a.isNum() && b.isNum() && b.n != 0) { STK[f->sp - 2] = Value(a.n / b.n); f->sp--; break; }
                goto arithSlow;
            }
            case OP_MOD: {
                Value b = PEEK(0), a = PEEK(1);
                if (a.isNum() && b.isNum() && b.n != 0) {
                    double x = a.n, y = b.n, m;
                    // Enteros de 32 bits (lo normal: i % 7): la división de 32 bits es mucho más rápida
                    if (x >= -2147483648.0 && x <= 2147483647.0 && y >= -2147483648.0 && y <= 2147483647.0 &&
                        x == (double)(int32_t)x && y == (double)(int32_t)y) {
                        int32_t yi = (int32_t)y;
                        m = yi == -1 ? 0.0 : (double)((int32_t)x % yi);
                    } else if (x >= -9e15 && x <= 9e15 && y >= -9e15 && y <= 9e15 && x == (double)(int64_t)x && y == (double)(int64_t)y) {
                        m = (double)((int64_t)x % (int64_t)y);
                    } else {
                        m = std::fmod(x, y);
                    }
                    if (m != 0 && ((m < 0) != (y < 0))) m += y;
                    STK[f->sp - 2] = Value(m);
                    f->sp--;
                    break;
                }
                goto arithSlow;
            }
            case OP_POW:
            case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR:
            arithSlow: {
                Value b = PEEK(0), a = PEEK(1);
                Value r;
                GUARD(r = arith(op, a, b));
                STK[f->sp - 2] = r;
                f->sp--;
                break;
            }
            case OP_NEG: {
                Value a = PEEK(0);
                if (a.isNum()) { STK[f->sp - 1] = Value(-a.n); break; }
                if (isObjType(a, OT::Vec)) {
                    auto* v = static_cast<ObjVec*>(a.o);
                    ObjVec* r = newVec(v->kind, -v->v[0], -v->v[1], -v->v[2], -v->v[3]);
                    STK[f->sp - 1] = Value::object(r);
                    break;
                }
                ERR("No se puede negar un valor de tipo " + typeName(a));
            }
            case OP_NOT: STK[f->sp - 1] = Value::boolean(isFalsy(PEEK(0))); break;
            case OP_BNOT: {
                Value a = PEEK(0);
                if (!a.isNum()) ERR("El operador '~' necesita un número");
                STK[f->sp - 1] = Value((double)(~(int64_t)a.n));
                break;
            }
            case OP_EQ: case OP_NE: {
                Value b = PEEK(0), a = PEEK(1);
                bool e;
                if (a.t == VT::Obj && b.t == VT::Obj && a.o != b.o && (a.o->type == OT::Instance || a.o->type == OT::Foreign)) {
                    GUARD(e = objectsEqual(a, b)); // puede llamar a equals() del script
                } else {
                    e = valuesEqual(a, b);
                }
                f->sp--;
                STK[f->sp - 1] = Value::boolean(op == OP_EQ ? e : !e);
                break;
            }
            case OP_IN: {
                Value c = PEEK(0), item = PEEK(1);
                bool r = false;
                GUARD(r = contains(c, item));
                STK[f->sp - 2] = Value::boolean(r);
                f->sp--;
                break;
            }
            case OP_IS: {
                ObjString* tn = asString(consts[RD16()]);
                std::string n = tn->str;
                size_t dot = n.rfind('.');
                if (dot != std::string::npos) n = n.substr(dot + 1);
                STK[f->sp - 1] = Value::boolean(isInstanceOf(PEEK(0), n));
                break;
            }
            case OP_JUMP: { uint16_t o = RD16(); ip += o; break; }
            case OP_JUMP_IF_FALSE: { uint16_t o = RD16(); if (isFalsy(POP())) ip += o; break; }
            case OP_JUMP_IF_NOT_LT: case OP_JUMP_IF_NOT_LE: case OP_JUMP_IF_NOT_GT: case OP_JUMP_IF_NOT_GE: {
                uint16_t o = RD16();
                Value b = PEEK(0), a = PEEK(1);
                bool r;
                if (a.isNum() && b.isNum()) {
                    r = op == OP_JUMP_IF_NOT_LT ? a.n < b.n : op == OP_JUMP_IF_NOT_LE ? a.n <= b.n : op == OP_JUMP_IF_NOT_GT ? a.n > b.n : a.n >= b.n;
                } else {
                    uint8_t cmp = op == OP_JUMP_IF_NOT_LT ? OP_LT : op == OP_JUMP_IF_NOT_LE ? OP_LE : op == OP_JUMP_IF_NOT_GT ? OP_GT : OP_GE;
                    Value v;
                    GUARD(v = arith(cmp, a, b));
                    r = !isFalsy(v);
                }
                f->sp -= 2;
                if (!r) ip += o;
                break;
            }
            case OP_JUMP_IF_NOT_EQ: case OP_JUMP_IF_NOT_NE: {
                uint16_t o = RD16();
                Value b = PEEK(0), a = PEEK(1);
                bool e;
                if (a.t == VT::Num && b.t == VT::Num) {
                    e = a.n == b.n;
                } else if (a.t == VT::Obj && b.t == VT::Obj && a.o != b.o && (a.o->type == OT::Instance || a.o->type == OT::Foreign)) {
                    GUARD(e = objectsEqual(a, b));
                } else {
                    e = valuesEqual(a, b);
                }
                f->sp -= 2;
                if (op == OP_JUMP_IF_NOT_EQ ? !e : e) ip += o;
                break;
            }
            case OP_JUMP_IF_TRUE: { uint16_t o = RD16(); if (!isFalsy(POP())) ip += o; break; }
            case OP_JUMP_IF_FALSE_KEEP: { uint16_t o = RD16(); if (isFalsy(PEEK(0))) ip += o; break; }
            case OP_JUMP_IF_TRUE_KEEP: { uint16_t o = RD16(); if (!isFalsy(PEEK(0))) ip += o; break; }
            case OP_JUMP_IF_NOT_NULL_KEEP: { uint16_t o = RD16(); if (!PEEK(0).isNull()) ip += o; break; }
            case OP_JUMP_IF_NULL_KEEP: { uint16_t o = RD16(); if (PEEK(0).isNull()) ip += o; break; }
            case OP_LOOP: {
                uint16_t o = RD16();
                ip -= o;
                if (bytesAllocated > nextGC && gcEnabled) { SAVE(); collectGarbage(); }
                if (g_interrupt.load(std::memory_order_relaxed)) [[unlikely]] {
                    g_interrupt = false;
                    ERR("Ejecución interrumpida (Ctrl+C)");
                }
                break;
            }
            case OP_CALL: {
                int argc = RD8();
                if (bytesAllocated > nextGC && gcEnabled) { SAVE(); collectGarbage(); }
                Value callee = PEEK(argc);
                // Camino rápido: función del script con exactamente sus argumentos (el caso más común)
                if (callee.t == VT::Obj && callee.o->type == OT::Closure) TRY_FAST_CALL(static_cast<ObjClosure*>(callee.o), argc);
                GUARD(callValue(callee, argc));
                break;
            }
            case OP_CALL_NAMED: {
                int argc = RD8();
                ObjList* names = static_cast<ObjList*>(consts[RD16()].o);
                Value callee = PEEK(argc);
                GUARD({ int n = argc; reorderNamed(callee, n, names); callValue(callee, n); });
                break;
            }
            case OP_CALL_SPREAD: {
                Value lst = POP();
                Value callee = PEEK(0);
                if (!isObjType(lst, OT::List)) ERR("Error interno: argumentos expandidos no válidos");
                auto items = static_cast<ObjList*>(lst.o)->items;
                for (auto& v : items) PUSH(v);
                GUARD(callValue(callee, (int)items.size()));
                break;
            }
            case OP_INVOKE: {
                ObjString* name = asString(consts[RD16()]);
                int argc = RD8();
                uint16_t ci = RD16();
                if (bytesAllocated > nextGC && gcEnabled) { SAVE(); collectGarbage(); }
                Value recv = PEEK(argc);
                if (isObjType(recv, OT::Instance)) {
                    auto* inst = static_cast<ObjInstance*>(recv.o);
                    ObjFunction::InvokeCache* ic = nullptr;
                    if (ci < fn->invokeCacheSize) {
                        if (fn->invokeCache.empty()) fn->invokeCache.resize(fn->invokeCacheSize);
                        ic = &fn->invokeCache[ci];
                        if (ic->cls == inst->cls) {
                            TRY_FAST_CALL(ic->method, argc);
                            GUARD(callClosure(ic->method, argc));
                            break;
                        }
                    }
                    if (!inst->cls->fieldIndex.count(name)) {
                        auto m = inst->cls->methods.find(name);
                        if (m != inst->cls->methods.end() && isObjType(m->second, OT::Closure)) {
                            auto* method = static_cast<ObjClosure*>(m->second.o);
                            if (ic) { ic->cls = inst->cls; ic->method = method; }
                            TRY_FAST_CALL(method, argc);
                            GUARD(callClosure(method, argc));
                            break;
                        }
                    }
                }
                GUARD(invoke(name, argc));
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* name = asString(consts[RD16()]);
                int argc = RD8();
                ObjClass* owner = frame->closure->owner;
                if (!owner || !owner->super) ERR("'super' no está disponible aquí");
                auto m = owner->super->methods.find(name);
                if (m == owner->super->methods.end()) ERR("La clase base " + owner->super->name->str + " no tiene el método '" + name->str + "'");
                Value method = m->second;
                GUARD(callMethodValue(method, argc));
                break;
            }
            case OP_SUPER_CALL: case OP_SUPER_INIT_AUTO: {
                int argc = op == OP_SUPER_CALL ? RD8() : 0;
                ObjClass* owner = frame->closure->owner;
                if (!owner || !owner->super) ERR("Esta clase no tiene clase base");
                ObjClass* sup = owner->super;
                Value self = PEEK(argc);
                if (isObjType(sup->ctor, OT::Closure)) {
                    if (op == OP_SUPER_INIT_AUTO && static_cast<ObjClosure*>(sup->ctor.o)->fn->minArity > 0)
                        ERR("El constructor de " + sup->name->str + " necesita argumentos: llama a super(...) al principio del constructor de " + owner->name->str);
                    Value c = sup->ctor;
                    GUARD(callClosure(static_cast<ObjClosure*>(c.o), argc));
                } else if (sup->isSubclassOf(errorClass) && isObjType(self, OT::Instance)) {
                    Value msg = argc > 0 ? PEEK(argc - 1) : Value();
                    GUARD(errorInit(static_cast<ObjInstance*>(self.o), msg));
                    f->sp -= argc;
                } else {
                    if (argc > 0) ERR("La clase base " + sup->name->str + " no tiene constructor y no acepta argumentos");
                }
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* pf = static_cast<ObjFunction*>(consts[RD16()].o);
                ObjClosure* c = alloc<ObjClosure>(pf);
                c->owner = frame->closure->owner;
                PUSH(Value::object(c));
                c->upvalues.reserve(pf->upvalueCount);
                for (int i = 0; i < pf->upvalueCount; i++) {
                    uint8_t isLocal = RD8();
                    uint16_t idx = RD16();
                    c->upvalues.push_back(isLocal ? captureUpvalue(frame->base + idx) : frame->closure->upvalues[idx]);
                }
                break;
            }
            case OP_CLOSE_UPVALUE: closeUpvalues(f->sp - 1); f->sp--; break;
            case OP_RETURN: {
                Value result = POP();
                closeUpvalues(frame->base);
                if (fn->isCtor) result = slots[0];
                int idx = (int)f->frames.size() - 1;
                while (!f->handlers.empty() && f->handlers.back().frameIndex >= idx) f->handlers.pop_back();
                f->sp = frame->base;
                f->frames.pop_back();
                STK[f->sp++] = result;
                if ((int)f->frames.size() <= exitDepth) return RunResult::Ok;
                LOAD();
                break;
            }
            case OP_CLASS: {
                ObjString* name = asString(consts[RD16()]);
                uint8_t flags = RD8();
                ObjClass* c = alloc<ObjClass>();
                c->name = name;
                c->isStruct = flags & 1;
                c->isInterface = flags & 2;
                c->module = fn->module;
                PUSH(Value::object(c));
                break;
            }
            case OP_INHERIT: {
                Value sv = POP();
                auto* cls = static_cast<ObjClass*>(PEEK(0).o);
                if (!isObjType(sv, OT::Class)) ERR("La clase base de " + cls->name->str + " no es una clase");
                auto* sup = static_cast<ObjClass*>(sv.o);
                if (sup->isInterface) ERR("No se puede heredar de una interfaz");
                if (sup->nativeCtor && sup != errorClass) ERR("No se puede heredar de la clase integrada " + sup->name->str);
                cls->super = sup;
                cls->methods = sup->methods;
                cls->getters = sup->getters;
                cls->setters = sup->setters;
                cls->fieldNames = sup->fieldNames;
                cls->fieldIndex = sup->fieldIndex;
                cls->fieldInits = sup->fieldInits;
                cls->ctor = sup->ctor;
                for (auto& [k, v] : sup->statics) if (isObjType(v, OT::Closure) || isObjType(v, OT::Native)) cls->statics[k] = v;
                break;
            }
            case OP_FIELD: {
                ObjString* name = asString(consts[RD16()]);
                (void)RD8();
                auto* cls = static_cast<ObjClass*>(PEEK(0).o);
                if (!cls->fieldIndex.count(name)) {
                    cls->fieldIndex[name] = (int)cls->fieldNames.size();
                    cls->fieldNames.push_back(name);
                }
                break;
            }
            case OP_METHOD: {
                ObjString* name = asString(consts[RD16()]);
                uint8_t kind = RD8();
                Value mv = POP();
                auto* cls = static_cast<ObjClass*>(PEEK(0).o);
                auto* c = static_cast<ObjClosure*>(mv.o);
                c->owner = cls;
                switch (kind) {
                    case MK_METHOD: cls->methods[name] = mv; break;
                    case MK_STATIC: cls->statics[name] = mv; break;
                    case MK_GETTER: cls->getters[name] = mv; break;
                    case MK_SETTER: cls->setters[name] = mv; break;
                    case MK_STATIC_GETTER: cls->staticGetters[name] = mv; break;
                    case MK_STATIC_SETTER: cls->staticSetters[name] = mv; break;
                    case MK_CTOR: cls->ctor = mv; c->fn->isCtor = true; break;
                    case MK_FIELDINIT: cls->fieldInits.push_back(mv); break;
                }
                break;
            }
            case OP_IMPLEMENTS: {
                ObjString* name = asString(consts[RD16()]);
                f->sp--;
                auto* cls = static_cast<ObjClass*>(PEEK(0).o);
                cls->interfaces.push_back(name);
                break;
            }
            case OP_LIST: {
                int n = RD16();
                ObjList* l = newList();
                l->items.assign(STK.begin() + (f->sp - n), STK.begin() + f->sp);
                f->sp -= n;
                PUSH(Value::object(l));
                break;
            }
            case OP_LIST_APPEND: { Value v = POP(); static_cast<ObjList*>(PEEK(0).o)->items.push_back(v); break; }
            case OP_LIST_EXTEND: {
                Value v = POP();
                auto* l = static_cast<ObjList*>(PEEK(0).o);
                if (isObjType(v, OT::List)) { auto items = static_cast<ObjList*>(v.o)->items; l->items.insert(l->items.end(), items.begin(), items.end()); }
                else if (isObjType(v, OT::Map)) { for (auto& e : static_cast<ObjMap*>(v.o)->entries) if (e.alive) l->items.push_back(e.key); }
                else if (isObjType(v, OT::Range)) { auto* r = static_cast<ObjRange*>(v.o); double n = r->count(); for (double k = 0; k < n; k++) l->items.push_back(Value(r->start + k * r->step)); }
                else ERR("No se puede expandir (...) un valor de tipo " + typeName(v));
                break;
            }
            case OP_MAP: {
                int n = RD16();
                ObjMap* m = newMap();
                for (int i = 0; i < n; i++) {
                    Value k = STK[f->sp - 2 * n + 2 * i], v = STK[f->sp - 2 * n + 2 * i + 1];
                    if (k.isNull()) ERR("La clave de un mapa no puede ser null");
                    m->set(k, v);
                }
                f->sp -= 2 * n;
                PUSH(Value::object(m));
                break;
            }
            case OP_RANGE: {
                bool incl = RD8();
                Value b = PEEK(0), a = PEEK(1);
                if (!a.isNum() || !b.isNum()) ERR("Los límites de un rango deben ser números (" + typeName(a) + ".." + typeName(b) + ")");
                ObjRange* r = newRange(a.n, b.n, 1, incl);
                f->sp -= 2;
                PUSH(Value::object(r));
                break;
            }
            case OP_RANGE_STEP: {
                Value s = POP();
                if (!s.isNum() || s.n == 0) ERR("El paso (step) de un rango debe ser un número distinto de 0");
                static_cast<ObjRange*>(PEEK(0).o)->step = s.n;
                break;
            }
            case OP_ITER_PREP: {
                Value v = PEEK(0);
                if (isString(v)) {
                    const std::string& s = asString(v)->str;
                    ObjList* chars = newList();
                    STK[f->sp - 1] = Value::object(chars);
                    for (size_t i = 0; i < s.size();) { size_t a = i; utf8Decode(s, i); chars->items.push_back(str(s.substr(a, i - a))); }
                } else if (isObjType(v, OT::Enum)) {
                    ObjList* vals = newList();
                    for (auto* e : static_cast<ObjEnum*>(v.o)->values) vals->items.push_back(Value::object(e));
                    STK[f->sp - 1] = Value::object(vals);
                } else if (!isObjType(v, OT::List) && !isObjType(v, OT::Map) && !isObjType(v, OT::Range)) {
                    if (v.isNum()) ERR("No se puede recorrer un número; para repetir N veces usa: for i in 0.." + formatNumber(v.n) + " { }");
                    ERR("No se puede recorrer un valor de tipo " + typeName(v));
                }
                PUSH(Value(0.0));
                break;
            }
            case OP_ITER_NEXT: {
                uint16_t slot = RD16();
                uint16_t jump = RD16();
                Value coll = slots[slot];
                double st = slots[slot + 1].n;
                if (isObjType(coll, OT::List)) {
                    auto& items = static_cast<ObjList*>(coll.o)->items;
                    if (st >= (double)items.size()) { ip += jump; break; }
                    Value item = items[(size_t)st];
                    slots[slot + 1] = Value(st + 1);
                    PUSH(Value(st));
                    PUSH(item);
                } else if (isObjType(coll, OT::Map)) {
                    auto* m = static_cast<ObjMap*>(coll.o);
                    size_t i = (size_t)st;
                    while (i < m->entries.size() && !m->entries[i].alive) i++;
                    if (i >= m->entries.size()) { ip += jump; break; }
                    slots[slot + 1] = Value((double)(i + 1));
                    Value k = m->entries[i].key, v = m->entries[i].value;
                    if (m->isSet) { PUSH(Value((double)i)); PUSH(k); }
                    else { PUSH(k); PUSH(v); }
                } else {
                    auto* r = static_cast<ObjRange*>(coll.o);
                    if (st >= r->count()) { ip += jump; break; }
                    slots[slot + 1] = Value(st + 1);
                    PUSH(Value(st));
                    PUSH(Value(r->start + st * r->step));
                }
                break;
            }
            case OP_FOR_CHECK: {
                uint16_t slot = RD16();
                bool incl = RD8();
                uint16_t jump = RD16();
                Value c = slots[slot], e = slots[slot + 1], s = slots[slot + 2];
                if (!c.isNum() || !e.isNum() || !s.isNum()) ERR("Los límites del bucle 'for' deben ser números");
                if (s.n == 0) ERR("El paso (step) del bucle no puede ser 0");
                bool go = s.n > 0 ? (incl ? c.n <= e.n : c.n < e.n) : (incl ? c.n >= e.n : c.n > e.n);
                if (!go) ip += jump;
                break;
            }
            case OP_FOR_CHECK_V: {
                uint16_t slot = RD16();
                bool incl = RD8();
                uint16_t var = RD16();
                uint16_t jump = RD16();
                Value c = slots[slot], e = slots[slot + 1], s = slots[slot + 2];
                if (!c.isNum() || !e.isNum() || !s.isNum()) ERR("Los límites del bucle 'for' deben ser números");
                if (s.n == 0) ERR("El paso (step) del bucle no puede ser 0");
                bool go = s.n > 0 ? (incl ? c.n <= e.n : c.n < e.n) : (incl ? c.n >= e.n : c.n > e.n);
                if (go) slots[var] = c;
                else ip += jump;
                break;
            }
            case OP_FOR_NEXT_V: {
                uint16_t slot = RD16();
                bool incl = RD8();
                uint16_t var = RD16();
                uint16_t back = RD16();
                double s = slots[slot + 2].n;
                double c = (slots[slot].n += s), e = slots[slot + 1].n;
                if (s > 0 ? (incl ? c <= e : c < e) : (incl ? c >= e : c > e)) {
                    slots[var] = Value(c);
                    ip -= back;
                    if (bytesAllocated > nextGC && gcEnabled) { SAVE(); collectGarbage(); }
                    if (g_interrupt.load(std::memory_order_relaxed)) [[unlikely]] {
                        g_interrupt = false;
                        ERR("Ejecución interrumpida (Ctrl+C)");
                    }
                }
                break;
            }
            case OP_FOR_INCR: { uint16_t slot = RD16(); slots[slot].n += slots[slot + 2].n; break; }
            case OP_FOR_NEXT: {
                uint16_t slot = RD16();
                bool incl = RD8();
                uint16_t back = RD16();
                double s = slots[slot + 2].n;
                double c = (slots[slot].n += s), e = slots[slot + 1].n; // FOR_CHECK ya comprobó que son números
                if (s > 0 ? (incl ? c <= e : c < e) : (incl ? c >= e : c > e)) {
                    ip -= back;
                    if (bytesAllocated > nextGC && gcEnabled) { SAVE(); collectGarbage(); }
                    if (g_interrupt.load(std::memory_order_relaxed)) [[unlikely]] {
                        g_interrupt = false;
                        ERR("Ejecución interrumpida (Ctrl+C)");
                    }
                }
                break;
            }
            case OP_CHECK_NUMBER: {
                uint16_t slot = RD16();
                uint16_t pn = RD16();
                if (!slots[slot].isNum()) [[unlikely]] {
                    Value v = slots[slot];
                    std::string who = fn->name ? fn->name->str : "?";
                    if (fn->className) who = fn->className->str + "." + who;
                    const std::string& name = asString(consts[pn])->str;
                    if (v.isNull()) ERR("El argumento '" + name + "' de " + who + " no puede ser null (tipo number; usa number? para permitir null)");
                    ERR("El argumento '" + name + "' de " + who + " debe ser number, pero se recibió " + typeName(v));
                }
                break;
            }
            case OP_REPEAT_PREP: {
                Value v = PEEK(0);
                if (!v.isNum()) ERR("repeat necesita un número, no " + typeName(v));
                STK[f->sp - 1] = Value(v.n > 0 ? std::floor(v.n) : 0.0);
                break;
            }
            case OP_REPEAT_NEXT: {
                uint16_t slot = RD16();
                uint16_t jump = RD16();
                if (slots[slot].n <= 0) { ip += jump; break; }
                slots[slot].n -= 1;
                break;
            }
            case OP_TRY: {
                uint16_t o = RD16();
                f->handlers.push_back({(int)f->frames.size() - 1, ip + o, f->sp});
                break;
            }
            case OP_END_TRY: if (!f->handlers.empty()) f->handlers.pop_back(); break;
            case OP_THROW: {
                Value v = POP();
                if (v.isNull()) v = makeError("Se lanzó null");
                THROWV(v);
            }
            case OP_AWAIT: {
                Value v = PEEK(0);
                if (!isObjType(v, OT::Task)) break;
                auto* t = static_cast<ObjTask*>(v.o);
                if (t->state == TaskState::Done) { STK[f->sp - 1] = t->result; break; }
                if (t->state == TaskState::Failed) { t->errorObserved = true; f->sp--; THROWV(t->error); }
                if (t->state == TaskState::Cancelled) { f->sp--; ERR("La tarea fue cancelada"); }
                if (exitDepth != 0)
                    ERR("'await' no puede pausar aquí porque se está ejecutando dentro de una llamada síncrona (por ejemplo un callback de lista o un evento del motor). Usa una función async.");
                f->sp--;
                t->waiters.push_back(f);
                f->waitingOn = t;
                SAVE();
                return RunResult::Yield;
            }
            case OP_EMIT: {
                int argc = RD8();
                Value ev = PEEK(argc);
                if (!isObjType(ev, OT::Event)) ERR("'emit' necesita un evento, no " + typeName(ev));
                auto* e = static_cast<ObjEvent*>(ev.o);
                if (argc != (int)e->params.size())
                    ERR("El evento " + e->name->str + " espera " + std::to_string(e->params.size()) + " dato(s) pero recibió " + std::to_string(argc));
                std::vector<Value> args(STK.begin() + (f->sp - argc), STK.begin() + f->sp);
                GUARD(emitEvent(e, argc, args.data()));
                f->sp -= argc + 1;
                break;
            }
            case OP_ON: {
                uint8_t flags = RD8();
                double prio = 0;
                if (flags & 2) {
                    Value p = POP();
                    if (!p.isNum()) ERR("La prioridad de un manejador debe ser un número");
                    prio = p.n;
                }
                Value handler = POP();
                Value ev = PEEK(0);
                if (!isObjType(ev, OT::Event)) ERR("'on' necesita un evento, no " + typeName(ev));
                ObjSubscription* s = subscribe(static_cast<ObjEvent*>(ev.o), handler, flags & 1, prio);
                STK[f->sp - 1] = Value::object(s);
                break;
            }
            case OP_ENUM: {
                ObjString* name = asString(consts[RD16()]);
                int n = RD16();
                ObjEnum* e = alloc<ObjEnum>();
                e->name = name;
                for (int i = 0; i < n; i++) {
                    auto* ev = alloc<ObjEnumValue>();
                    ev->owner = e;
                    ev->name = asString(STK[f->sp - 2 * n + 2 * i]);
                    ev->value = STK[f->sp - 2 * n + 2 * i + 1];
                    ev->ordinal = i;
                    e->values.push_back(ev);
                    e->byName[ev->name] = ev;
                }
                f->sp -= 2 * n;
                PUSH(Value::object(e));
                break;
            }
            case OP_EVENT: {
                ObjString* name = asString(consts[RD16()]);
                auto* params = static_cast<ObjList*>(consts[RD16()].o);
                ObjEvent* e = alloc<ObjEvent>();
                e->name = name;
                for (auto& p : params->items) e->params.push_back(asString(p));
                PUSH(Value::object(e));
                break;
            }
            case OP_IMPORT: {
                ObjString* path = asString(consts[RD16()]);
                Value m;
                GUARD(m = importModule(path->str));
                PUSH(m);
                break;
            }
            case OP_IMPORT_NAME: {
                ObjString* name = asString(consts[RD16()]);
                Value m = POP();
                Value r;
                GUARD(r = getProperty(m, name, false));
                PUSH(r);
                break;
            }
            case OP_TO_STRING: {
                Value v = PEEK(0);
                if (isString(v)) break;
                std::string s;
                GUARD(s = toString(v));
                STK[f->sp - 1] = str(s);
                break;
            }
            case OP_CONCAT: {
                int n = RD16();
                size_t total = 0;
                for (int i = f->sp - n; i < f->sp; i++) total += asString(STK[i])->str.size();
                std::string s;
                s.reserve(total);
                for (int i = f->sp - n; i < f->sp; i++) s += asString(STK[i])->str;
                f->sp -= n;
                PUSH(Value::object(internMove(std::move(s))));
                break;
            }
            case OP_CHECK_TYPE: {
                uint16_t slot = RD16();
                ObjString* tn = asString(consts[RD16()]);
                ObjString* pn = asString(consts[RD16()]);
                Value v = slots[slot];
                const std::string& t = tn->str;
                bool quick = (v.t == VT::Num && t.size() == 6 && t == "number") || (isString(v) && t.size() == 4 && t == "text");
                if (!quick && !isInstanceOf(v, t)) {
                    std::string who = fn->name ? fn->name->str : "?";
                    if (fn->className) who = fn->className->str + "." + who;
                    if (v.isNull() && tn->str.back() != '?')
                        ERR("El argumento '" + pn->str + "' de " + who + " no puede ser null (tipo " + tn->str + "; usa " + tn->str + "? para permitir null)");
                    ERR("El argumento '" + pn->str + "' de " + who + " debe ser " + tn->str + ", pero se recibió " + typeName(v));
                }
                break;
            }
            case OP_DEFAULT_ARG: {
                uint16_t slot = RD16();
                uint16_t jump = RD16();
                if (!slots[slot].isUndef()) ip += jump;
                break;
            }
            case OP_MATCH_TEST: {
                Value pat = PEEK(0), subj = PEEK(1);
                bool r;
                if (isObjType(pat, OT::Range)) r = subj.isNum() && contains(pat, subj);
                else if (isObjType(pat, OT::Class)) r = isInstanceOf(subj, static_cast<ObjClass*>(pat.o)->name->str);
                else r = valuesEqual(subj, pat);
                f->sp -= 2;
                PUSH(Value::boolean(r));
                break;
            }
            case OP_CHECK_RETURN: (void)RD16(); break;
            case OP_SLIDE: {
                int n = RD8();
                Value v = POP();
                closeUpvalues(f->sp - n);
                f->sp -= n;
                PUSH(v);
                break;
            }
            default:
                ERR("Error interno: instrucción desconocida " + std::to_string(op));
        }
    }
#undef LOAD
#undef SAVE
#undef RD8
#undef RD16
#undef STK
#undef PUSH
#undef POP
#undef PEEK
#undef THROWV
#undef ERR
#undef GUARD
}

} // namespace hz
