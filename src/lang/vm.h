// Máquina virtual de HyperEZ: ejecución de bytecode, recolector de basura,
// fibras (async/await), eventos y registro de la API nativa.
#pragma once

#include "value.h"
#include "api.h"
#include "diagnostics.h"

#include <atomic>
#include <deque>
#include <random>

namespace hz {

struct Program;
struct ModuleUnit;
class Analyzer;

// Error lanzado por funciones nativas; la VM lo convierte en excepción del script.
struct NativeError { std::string message; };
// Señal interna: hay una excepción del script pendiente que debe propagarse.
struct PropagateError {};

enum class RunResult { Ok, Yield, Error };

// Consola interactiva (hyper repl): cada entrada se analiza junto con las anteriores, pero solo se
// genera código para la nueva. Los globales viven en un único módulo compartido por todas las entradas.
struct ReplSession {
    ObjModule* module = nullptr;      // módulo compartido (nullptr hasta la primera entrada)
    std::vector<std::string> globals; // globales ya creados, en orden de índice
    int start = 0;                    // desplazamiento en el código donde empieza la entrada nueva
    int echoSlot = -1;                // integrado que muestra el valor de la última expresión
};

// Ctrl+C: los bucles del programa lo comprueban y lanzan el error "Ejecución interrumpida".
extern std::atomic<bool> g_interrupt;

// Información de tipos de la API para el analizador (se rellena al registrar nativos).
struct BuiltinInfo {
    std::string name;
    int index = -1;
    std::string doc;
};

class VM {
public:
    VM();
    ~VM();
    VM(const VM&) = delete;

    // ---- Configuración del host ------------------------------------------------
    std::function<void(std::string_view)> printHook;  // salida de print()
    std::function<std::string(const std::string&)> inputHook;
    std::function<bool(const std::string&, std::string&)> readSource; // VFS (paquetes exportados)
    std::function<double()> clock;                    // tiempo para wait() (juego o real)
    bool gameMode = false;
    std::string projectRoot;
    std::vector<std::string> searchPaths;
    std::vector<std::string> warnings;
    std::vector<std::string> scriptArgs;  // System.args
    int testsPassed = 0, testsFailed = 0; // contadores de test()
    ReplSession* repl = nullptr;          // sesión de la consola interactiva (solo hyper repl)

    // ---- Compilación y ejecución --------------------------------------------------
    // Compila (sin ejecutar) un módulo. Siempre devuelve la unidad; usa unitHasErrors() para saber si se puede ejecutar.
    ModuleUnit* compileFile(const std::string& path);
    ModuleUnit* compileSource(const std::string& path, const std::string& source, bool isEntry = true, bool codegen = true);
    bool unitHasErrors(ModuleUnit* unit);
    void printDiagnostics(ModuleUnit* unit, FILE* out, bool includeWarnings = true);
    bool loadPrelude();
    // Ejecuta el código de nivel superior del módulo (en la fibra principal). false si hubo error.
    bool runModule(ModuleUnit* unit);
    // Procesa tareas pendientes (temporizadores/await) hasta que no quede nada; para modo consola.
    bool drainScheduler();
    // Avanza el planificador una vez (modo juego): reanuda las fibras listas.
    void pumpScheduler();
    bool hasPendingTasks() const;
    void cancelAllTasks(); // descarta temporizadores y tareas pendientes (Ctrl+C en la consola)

    // Resolución de módulos (usado por el analizador)
    ModuleUnit* resolveImport(const std::string& path, bool isFile, const std::string& fromFile, std::string& error);
    Value libraryModule(const std::string& name);
    std::vector<std::string> libraryNames() const;
    std::vector<std::unique_ptr<ModuleUnit>> units;
    ModuleUnit* preludeUnit = nullptr;
    ModuleUnit* entryUnit = nullptr;

    // ---- Llamadas desde código nativo ----------------------------------------------
    Value call(Value fn, int argc, const Value* args); // lanza PropagateError si falla
    Value call(Value fn, std::initializer_list<Value> args) { return call(fn, (int)args.size(), args.begin()); }
    Value callMethod(Value receiver, const char* name, int argc, const Value* args);
    Value callBound(Value method, Value self, int argc, const Value* args); // método con this explícito
    void errorInit(ObjInstance* inst, Value message);
    ObjFiber* hostFiber = nullptr;
    bool tryCall(Value fn, int argc, const Value* args, Value* out = nullptr); // registra el error y devuelve false
    Value getGlobal(ModuleUnit* unit, const std::string& name);
    Value findFunction(ModuleUnit* unit, const std::string& name);

    // ---- Errores ----------------------------------------------------------------------
    [[noreturn]] void raise(const std::string& msg);
    void reportError(const Value& err, FILE* out = stderr);
    std::string errorMessage(const Value& err);
    Value lastError;
    bool hadUncaught = false;
    // Si devuelve true, el host ya mostró el error (la consola interactiva usa sus propios números de línea).
    std::function<bool(const std::string& message, const std::string& file, int line, int col,
                       const std::vector<std::string>& trace)> errorHook;

    // ---- Memoria ----------------------------------------------------------------------
    ObjString* intern(std::string_view s);
    ObjString* internMove(std::string&& s); // como intern, pero sin copiar si la cadena es nueva
    Value str(std::string_view s) { return Value::object(intern(s)); }
    ObjList* newList();
    ObjMap* newMap(bool isSet = false);
    ObjVec* newVec(int kind, double x = 0, double y = 0, double z = 0, double w = 0);
    ObjInstance* newInstance(ObjClass* cls);
    ObjForeign* newForeign(ObjClass* cls, void* ptr, void (*fin)(void*));
    ObjTask* newTask();
    ObjNative* newNative(const std::string& name, NativeFn fn, int minA = 0, int maxA = -1);
    ObjClass* newClass(const std::string& name);
    ObjRange* newRange(double a, double b, double step, bool inclusive);
    Value makeError(const std::string& msg, const std::string& kind = "Error");
    template <class T, class... A> T* alloc(A&&... a) {
        T* o = new T(std::forward<A>(a)...);
        o->next = objects;
        objects = o;
        bytesAllocated += sizeof(T) + 32;
        return o;
    }
    void pin(Value v);   // retener un valor desde C++ (motor)
    void unpin(Value v);
    void collectGarbage();
    void maybeGC() { if (bytesAllocated > nextGC && gcEnabled) collectGarbage(); }
    size_t bytesAllocated = 0, nextGC = 4 * 1024 * 1024;
    bool gcEnabled = true;
    std::vector<Value> tempRoots;
    std::vector<std::function<void(VM&)>> extraRoots; // el motor marca sus propias referencias
    void markValue(const Value& v);
    void markObject(Obj* o);

    // ---- Registro de API nativa ---------------------------------------------------------
    ObjModule* nativeModule(const std::string& name, const std::string& doc);
    ObjNative* defineFn(ObjModule* mod, const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn,
                        const std::string& example = "");
    void defineProp(ObjModule* mod, const std::string& name, const std::string& type, const std::string& doc, NativeFn get,
                    NativeFn set = nullptr);
    void defineConst(ObjModule* mod, const std::string& name, Value v, const std::string& type, const std::string& doc);
    ObjNative* defineGlobalFn(const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn,
                              const std::string& example = "");
    void defineGlobal(const std::string& name, Value v, const std::string& type, const std::string& doc);
    ObjClass* nativeClass(const std::string& name, const std::string& doc, NativeFn ctor = nullptr, const std::string& ctorSig = "");
    ObjNative* defineMethod(ObjClass* cls, const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn);
    ObjNative* defineStatic(ObjClass* cls, const std::string& name, const std::string& sig, const std::string& doc, NativeFn fn);
    void defineClassProp(ObjClass* cls, const std::string& name, const std::string& type, const std::string& doc, NativeFn get,
                         NativeFn set = nullptr, bool isStatic = false);
    void addCatalogField(ObjClass* cls, const std::string& name, const std::string& type, const std::string& doc, bool readonly = false);
    ObjEvent* defineEvent(ObjModule* mod, const std::string& name, const std::string& params, const std::string& doc);
    void defineLibrary(const std::string& lib, const std::vector<std::string>& namespaces, const std::string& doc);
    std::vector<ApiEntry> catalog;
    std::vector<std::pair<std::string, std::string>> libraryDocs;
    std::unordered_map<std::string, std::vector<std::string>> libraryMembers;

    // Integrados (globales visibles en todos los módulos)
    std::vector<Value> builtins;
    std::unordered_map<std::string, int> builtinIndex;
    int builtinSlot(const std::string& name);
    std::unordered_map<std::string, Value> libraries;

    // Clases de tipos integrados
    ObjClass *textClass, *listClass, *mapClass, *setClass, *numberClass, *boolClass, *functionClass, *rangeClass,
        *taskClass, *eventClass, *subscriptionClass, *errorClass, *eventDataClass, *enumClass, *moduleClass,
        *enumValueClass;
    ObjClass* vecClass[5] = {};

    // ---- Utilidades -----------------------------------------------------------------------
    std::string toString(const Value& v, bool quoteStrings = false, int depth = 0);
    std::string typeName(const Value& v);
    bool isInstanceOf(const Value& v, const std::string& typeName);
    ObjClass* classOf(const Value& v);
    void flushOutput();
    void print(std::string_view s);

    // Estado de ejecución
    ObjFiber* fiber = nullptr;     // fibra actual
    ObjFiber* mainFiber = nullptr;
    std::vector<ObjFiber*> fiberStack;
    std::deque<ObjFiber*> ready;
    std::vector<ObjTask*> timers;
    std::vector<ObjTask*> pendingTasks; // tareas con fibra viva (para raíces)
    uint64_t subscriptionCounter = 0;
    int nativeDepth = 0;
    double currentTime();

    // Eventos
    void emitEvent(ObjEvent* ev, int argc, const Value* args, Value target = Value());
    ObjSubscription* subscribe(ObjEvent* ev, Value handler, bool once, double priority);
    Value eventPayload(ObjEvent* ev, int argc, const Value* args, Value target);

    // Tareas
    ObjTask* waitTask(double seconds);
    void completeTask(ObjTask* t, Value result);
    void failTask(ObjTask* t, Value err);
    void wakeWaiters(ObjTask* t);

    // Interno (usado por vm.cpp)
    RunResult run(int exitDepth);
    bool callValue(Value callee, int argc); // prepara la llamada en la fibra actual
    bool throwValue(Value err);             // intenta manejar una excepción
    int currentExitDepth = 0;
    std::vector<std::string> lastTrace;
    struct Resume { ObjFiber* f; Value v; bool err; };
    std::deque<Resume> readyQ;
    std::vector<ObjTask*> nextFrameTasks;
    Value pendingError;
    bool hasPendingError = false;
    RunResult runFiber(ObjFiber* f, bool resume = false, Value v = Value(), bool isError = false);
    void resumeFiber(ObjFiber* f, Value v, bool isError);
    ObjUpvalue* captureUpvalue(int slot);
    void closeUpvalues(int fromSlot);
    Value getProperty(Value obj, ObjString* name, bool optional);
    void setProperty(Value obj, ObjString* name, Value v);
    Value getIndex(Value obj, Value idx);
    void setIndex(Value obj, Value idx, Value v);
    bool invoke(ObjString* name, int argc);
    Value bindMethod(Value receiver, Value method);
    bool instantiate(ObjClass* cls, int argc);
    Value importModule(const std::string& path);
    std::vector<std::string> stackTrace();
    std::string callArgsError;

    std::string output;
    char lastOutputChar = '\n'; // último carácter enviado a stdout (la consola lo usa para cerrar la línea)
    bool outputToStdout = true;

    // Nombres internos frecuentes
    ObjString *s_constructor, *s_toString, *s_message, *s_stack, *s_name, *s_length, *s_iterator;

private:
    Obj* objects = nullptr;
    // Tabla de textos internados; la clave lleva el hash ya calculado (se calcula una sola vez por texto).
    struct InternKey {
        std::string_view s;
        size_t h;
        bool operator==(const InternKey& o) const { return h == o.h && s == o.s; }
    };
    struct InternHash { size_t operator()(const InternKey& k) const noexcept { return k.h; } };
    std::unordered_map<InternKey, ObjString*, InternHash> strings;
    std::vector<Obj*> gray;
    std::unordered_map<Obj*, int> pinned;
    void blacken(Obj* o);
    void sweep();
    void registerCore();
    ApiEntry& addCatalog(ApiKind kind, const std::string& owner, const std::string& name, const std::string& sig,
                         const std::string& doc);
public:
    bool callClosure(ObjClosure* c, int argc);
    bool callNative(ObjNative* n, int argc);
    bool callMethodValue(Value method, int argc); // receptor ya en el slot del callee
    bool startAsync(ObjClosure* c, int argc);
    bool reorderNamed(Value callee, int& argc, ObjList* names);
    Value arith(uint8_t op, Value a, Value b);
    bool contains(Value container, Value item);
    void push(Value v) { fiber->ensure(1); fiber->stack[fiber->sp++] = v; }
    friend struct VMRunner;
};

// Helpers para implementar funciones nativas
double argNum(VM& vm, Value* args, int argc, int i, const char* fname, double def = NAN);
std::string argStr(VM& vm, Value* args, int argc, int i, const char* fname, const char* def = nullptr);
bool argBool(Value* args, int argc, int i, bool def);
ObjList* argList(VM& vm, Value* args, int argc, int i, const char* fname);
ObjMap* argMap(VM& vm, Value* args, int argc, int i, const char* fname);
ObjVec* argVec(VM& vm, Value* args, int argc, int i, const char* fname, int kind);
Value optArg(Value* args, int argc, int i);
// Busca una opción en el mapa de opciones (último argumento si es mapa).
Value optField(VM& vm, ObjMap* opts, const char* key);

// Registro de la biblioteca estándar (builtins.cpp)
// Generador aleatorio compartido por random(), randomInt(), chance()... (randomSeed lo reinicia)
std::mt19937_64& randomEngine();
void registerStdlib(VM& vm);
void registerStdlibExtra(VM& vm); // stdlib_extra.cpp (la llama registerStdlib)
void registerMathTypes(VM& vm);

} // namespace hz
