// Utilidades compartidas por los archivos de la biblioteca estándar nativa (src/lang/stdlib).
#pragma once

#include "lang/vm/vm.h"

#include <filesystem>

namespace hz {

#define NATIVE(name) static Value name(VM& vm, int argc, Value* args)
#define UNUSED (void)vm; (void)argc; (void)args

// Mantiene un objeto vivo frente al recolector mientras se construye un resultado.
struct TempRoot {
    VM& vm;
    TempRoot(VM& v, Obj* o) : vm(v) { vm.tempRoots.push_back(Value::object(o)); }
    ~TempRoot() { vm.tempRoots.pop_back(); }
    TempRoot(const TempRoot&) = delete;
};

// Ruta de un archivo del programa: las relativas parten de la carpeta del proyecto.
std::string resolvePath(VM& vm, const std::string& p);
inline std::filesystem::path FP(const std::string& s) { return std::filesystem::path((const char8_t*)s.c_str()); }
inline std::string U8(const std::filesystem::path& p) {
    auto u = p.u8string();
    return std::string((const char*)u.data(), u.size());
}

// Módulo nativo ya registrado (para añadirle funciones desde otro archivo).
inline ObjModule* existingModule(VM& vm, const std::string& name) {
    Value v = vm.builtins[vm.builtinSlot(name)];
    return isObjType(v, OT::Module) ? static_cast<ObjModule*>(v.o) : vm.nativeModule(name, "");
}

inline void put(VM& vm, ObjMap* m, const char* key, Value v) { m->set(vm.str(key), v); }

// Texto o lista de textos de un argumento opcional
std::vector<std::string> argStrList(VM& vm, Value* args, int argc, int i, const char* fname);

// Registro de cada área de la biblioteca
void registerSystemLibs(VM& vm); // system.cpp: Process, Path, Time y más File/System
void registerBinaryLibs(VM& vm); // binary.cpp: Bytes, Hash, Base64, File.readBytes/writeBytes
void registerRegex(VM& vm);      // regex.cpp: Regex
void registerNet(VM& vm);        // net.cpp: Http
void registerMatrix(VM& vm);     // matrix.cpp: Matrix (álgebra lineal para IA y ciencia)
void registerImage(VM& vm);      // image.cpp: Image (gráficos) y compresión de Bytes
void registerServer(VM& vm);     // server.cpp: Http.serve y Process.start
void registerMore(VM& vm);       // more.cpp: matemáticas, textos, colecciones, números, fechas y JSON (0.7)

// Acceso directo a los datos de un objeto Bytes (binary.cpp)
Value newBytes(VM& vm, std::vector<uint8_t> data);
std::vector<uint8_t>* bytesData(VM& vm, const Value& v); // nullptr si no es Bytes

} // namespace hz
