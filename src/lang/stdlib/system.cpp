// Sistema operativo: procesos (Process), rutas (Path), tiempo y fechas (Time) y más
// funciones de archivos (File) y del sistema (System).
#include "lang/stdlib/native.h"
#include "core/process.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace hz {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Process
// ---------------------------------------------------------------------------

static ProcessOptions processOptions(VM& vm, Value* args, int argc, int i, const char* fname) {
    ProcessOptions o;
    Value v = optArg(args, argc, i);
    if (v.isNull()) return o;
    if (!isObjType(v, OT::Map)) vm.raise(std::string(fname) + ": las opciones deben ser un mapa {cwd, input, timeout, env}");
    auto* m = static_cast<ObjMap*>(v.o);
    Value cwd = optField(vm, m, "cwd"), input = optField(vm, m, "input"), timeout = optField(vm, m, "timeout"), env = optField(vm, m, "env");
    if (!cwd.isNull()) o.cwd = resolvePath(vm, vm.toString(cwd));
    if (!input.isNull()) o.input = vm.toString(input);
    if (timeout.isNum()) o.timeoutMs = (int)(timeout.n * 1000);
    if (isObjType(env, OT::Map)) {
        for (auto& e : static_cast<ObjMap*>(env.o)->entries)
            if (e.alive) o.env.push_back(vm.toString(e.key) + "=" + vm.toString(e.value));
    }
    if (o.cwd.empty() && !vm.projectRoot.empty()) o.cwd = vm.projectRoot;
    return o;
}

static Value processResult(VM& vm, const ProcessResult& r, const std::string& what) {
    if (!r.started) vm.raise("No se pudo ejecutar " + what + " (¿está instalado y en el PATH?)");
    ObjMap* m = vm.newMap();
    TempRoot keep(vm, m);
    put(vm, m, "code", Value(r.code));
    put(vm, m, "output", vm.str(r.out));
    put(vm, m, "error", vm.str(r.err));
    put(vm, m, "ok", Value::boolean(r.code == 0 && !r.timedOut));
    put(vm, m, "timedOut", Value::boolean(r.timedOut));
    return Value::object(m);
}

NATIVE(proc_run) {
    std::string prog = argStr(vm, args, argc, 0, "Process.run");
    std::vector<std::string> argv{prog};
    for (auto& a : argStrList(vm, args, argc, 1, "Process.run")) argv.push_back(a);
    vm.flushOutput();
    return processResult(vm, runCapture(argv, processOptions(vm, args, argc, 2, "Process.run")), "'" + prog + "'");
}
NATIVE(proc_shell) {
    std::string cmd = argStr(vm, args, argc, 0, "Process.shell");
    vm.flushOutput();
    return processResult(vm, runShell(cmd, processOptions(vm, args, argc, 1, "Process.shell")), "la orden");
}
NATIVE(proc_exit) {
    int code = (int)argNum(vm, args, argc, 0, "Process.exit", 0);
    vm.flushOutput();
    std::fflush(stderr);
    std::exit(code);
}
NATIVE(proc_open) { return Value::boolean(openWithSystem(argStr(vm, args, argc, 0, "Process.open"))); }
NATIVE(proc_pid) { UNUSED; return Value(currentProcessId()); }

// ---------------------------------------------------------------------------
// Path
// ---------------------------------------------------------------------------

NATIVE(path_join) {
    std::string r;
    for (int i = 0; i < argc; i++) {
        std::string part = vm.toString(args[i]);
        r = r.empty() ? part : pathJoin(r, part);
    }
    return vm.str(r);
}
NATIVE(path_dir) { return vm.str(pathDir(argStr(vm, args, argc, 0, "Path.dir"))); }
NATIVE(path_name) { return vm.str(pathFilename(argStr(vm, args, argc, 0, "Path.name"))); }
NATIVE(path_stem) { return vm.str(pathStem(argStr(vm, args, argc, 0, "Path.stem"))); }
NATIVE(path_ext) { return vm.str(pathExt(argStr(vm, args, argc, 0, "Path.ext"))); }
NATIVE(path_absolute) { return vm.str(pathAbsolute(resolvePath(vm, argStr(vm, args, argc, 0, "Path.absolute")))); }
NATIVE(path_normalize) { return vm.str(pathNormalize(argStr(vm, args, argc, 0, "Path.normalize"))); }
NATIVE(path_isAbsolute) { return Value::boolean(FP(argStr(vm, args, argc, 0, "Path.isAbsolute")).is_absolute()); }
NATIVE(path_relative) {
    std::string p = argStr(vm, args, argc, 0, "Path.relative");
    std::string base = argStr(vm, args, argc, 1, "Path.relative", ".");
    std::error_code ec;
    auto r = fs::relative(FP(resolvePath(vm, p)), FP(resolvePath(vm, base)), ec);
    std::string s = ec ? p : U8(r);
    for (auto& c : s) if (c == '\\') c = '/';
    return vm.str(s);
}
NATIVE(path_withExt) {
    std::string p = argStr(vm, args, argc, 0, "Path.withExt");
    std::string e = argStr(vm, args, argc, 1, "Path.withExt");
    std::string old = pathExt(p);
    if (!e.empty() && e[0] != '.') e = "." + e;
    return vm.str(p.substr(0, p.size() - old.size()) + e);
}
NATIVE(path_home) {
    UNUSED;
#ifdef _WIN32
    const char* h = std::getenv("USERPROFILE");
#else
    const char* h = std::getenv("HOME");
#endif
    std::string s = h ? h : "";
    for (auto& c : s) if (c == '\\') c = '/';
    return vm.str(s);
}
NATIVE(path_temp) {
    UNUSED;
    std::error_code ec;
    std::string s = U8(fs::temp_directory_path(ec));
    for (auto& c : s) if (c == '\\') c = '/';
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return vm.str(s);
}
NATIVE(path_sep) {
    UNUSED;
#ifdef _WIN32
    return vm.str("\\");
#else
    return vm.str("/");
#endif
}

// ---------------------------------------------------------------------------
// File (más funciones)
// ---------------------------------------------------------------------------

NATIVE(file_copy) {
    std::string a = resolvePath(vm, argStr(vm, args, argc, 0, "File.copy"));
    std::string b = resolvePath(vm, argStr(vm, args, argc, 1, "File.copy"));
    std::error_code ec;
    if (!pathDir(b).empty()) makeDirs(pathDir(b));
    if (dirExists(a)) fs::copy(FP(a), FP(b), fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    else fs::copy_file(FP(a), FP(b), fs::copy_options::overwrite_existing, ec);
    return Value::boolean(!ec);
}
NATIVE(file_move) {
    std::string a = resolvePath(vm, argStr(vm, args, argc, 0, "File.move"));
    std::string b = resolvePath(vm, argStr(vm, args, argc, 1, "File.move"));
    std::error_code ec;
    if (!pathDir(b).empty()) makeDirs(pathDir(b));
    fs::rename(FP(a), FP(b), ec);
    return Value::boolean(!ec);
}
NATIVE(file_size) {
    std::string p = resolvePath(vm, argStr(vm, args, argc, 0, "File.size"));
    std::error_code ec;
    auto n = fs::file_size(FP(p), ec);
    if (ec) vm.raise("File.size: no existe el archivo '" + argStr(vm, args, argc, 0, "File.size") + "'");
    return Value((double)n);
}
NATIVE(file_modified) {
    std::string p = resolvePath(vm, argStr(vm, args, argc, 0, "File.modified"));
    std::error_code ec;
    auto t = fs::last_write_time(FP(p), ec);
    if (ec) vm.raise("File.modified: no existe '" + argStr(vm, args, argc, 0, "File.modified") + "'");
    // file_clock -> system_clock sin clock_cast (no está en todas las bibliotecas estándar)
    auto sys = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(t - fs::file_time_type::clock::now());
    return Value(std::chrono::duration_cast<std::chrono::milliseconds>(sys.time_since_epoch()).count() / 1000.0);
}
NATIVE(file_isFile) {
    std::error_code ec;
    return Value::boolean(fs::is_regular_file(FP(resolvePath(vm, argStr(vm, args, argc, 0, "File.isFile"))), ec));
}
NATIVE(file_listAll) {
    std::string root = resolvePath(vm, argStr(vm, args, argc, 0, "File.listAll", "."));
    std::string ext = argStr(vm, args, argc, 1, "File.listAll", "");
    if (!ext.empty() && ext[0] != '.') ext = "." + ext;
    std::vector<std::string> names;
    std::error_code ec;
    fs::path base = FP(root);
    for (auto it = fs::recursive_directory_iterator(base, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (!ext.empty() && toLower(U8(it->path().extension())) != toLower(ext)) continue;
        std::string rel = U8(fs::relative(it->path(), base, ec));
        for (auto& c : rel) if (c == '\\') c = '/';
        names.push_back(rel);
    }
    std::sort(names.begin(), names.end());
    ObjList* l = vm.newList();
    TempRoot keep(vm, l);
    for (auto& n : names) l->items.push_back(vm.str(n));
    return Value::object(l);
}
NATIVE(file_deleteDir) {
    std::string given = argStr(vm, args, argc, 0, "File.deleteDir");
    std::string p = pathNormalize(pathAbsolute(resolvePath(vm, given)));
    // Protección: nunca la raíz, la carpeta del proyecto ni la del usuario
    std::string root = vm.projectRoot.empty() ? "" : pathNormalize(pathAbsolute(vm.projectRoot));
    fs::path fp = FP(p);
    if (trim(given).empty() || given == "." || fp == fp.root_path() || p == root || !fp.has_relative_path())
        vm.raise("File.deleteDir: por seguridad no se puede borrar '" + given + "'");
    std::error_code ec;
    auto n = fs::remove_all(fp, ec);
    if (ec) vm.raise("File.deleteDir: no se pudo borrar '" + given + "': " + ec.message());
    return Value((double)n);
}
NATIVE(file_tempFile) {
    std::string ext = argStr(vm, args, argc, 0, "File.tempFile", ".tmp");
    if (!ext.empty() && ext[0] != '.') ext = "." + ext;
    std::error_code ec;
    std::string dir = U8(fs::temp_directory_path(ec));
    static unsigned counter = 0;
    std::string p = pathJoin(dir, "hyperez-" + std::to_string(currentProcessId()) + "-" + std::to_string(++counter) + ext);
    return vm.str(p);
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

static double unixNow() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count() / 1e6;
}
static std::tm localTm(double t) {
    std::time_t tt = (std::time_t)std::floor(t);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

NATIVE(time_now) { UNUSED; return Value(unixNow()); }
NATIVE(time_date) {
    double t = argNum(vm, args, argc, 0, "Time.date", unixNow());
    std::tm tm = localTm(t);
    static const char* days[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
    ObjMap* m = vm.newMap();
    TempRoot keep(vm, m);
    put(vm, m, "year", Value(tm.tm_year + 1900));
    put(vm, m, "month", Value(tm.tm_mon + 1));
    put(vm, m, "day", Value(tm.tm_mday));
    put(vm, m, "hour", Value(tm.tm_hour));
    put(vm, m, "minute", Value(tm.tm_min));
    put(vm, m, "second", Value(tm.tm_sec));
    put(vm, m, "millisecond", Value(std::floor(std::fmod(t, 1.0) * 1000)));
    put(vm, m, "weekday", Value(tm.tm_wday));
    put(vm, m, "dayName", vm.str(days[tm.tm_wday]));
    put(vm, m, "yearDay", Value(tm.tm_yday + 1));
    return Value::object(m);
}
NATIVE(time_format) {
    double t = argNum(vm, args, argc, 0, "Time.format", unixNow());
    std::string f = argStr(vm, args, argc, 1, "Time.format", "%Y-%m-%d %H:%M:%S");
    std::tm tm = localTm(t);
    char buf[256];
    size_t n = std::strftime(buf, sizeof buf, f.c_str(), &tm);
    return vm.str(std::string(buf, n));
}
NATIVE(time_make) {
    std::tm tm{};
    tm.tm_year = (int)argNum(vm, args, argc, 0, "Time.make") - 1900;
    tm.tm_mon = (int)argNum(vm, args, argc, 1, "Time.make", 1) - 1;
    tm.tm_mday = (int)argNum(vm, args, argc, 2, "Time.make", 1);
    tm.tm_hour = (int)argNum(vm, args, argc, 3, "Time.make", 0);
    tm.tm_min = (int)argNum(vm, args, argc, 4, "Time.make", 0);
    tm.tm_sec = (int)argNum(vm, args, argc, 5, "Time.make", 0);
    tm.tm_isdst = -1;
    return Value((double)std::mktime(&tm));
}
NATIVE(time_measure) {
    if (argc < 1) vm.raise("Time.measure: falta la función a medir");
    double t0 = nowSeconds();
    vm.call(args[0], 0, nullptr);
    return Value(nowSeconds() - t0);
}
NATIVE(time_clock) { UNUSED; return Value(nowSeconds()); }

// ---------------------------------------------------------------------------
// System (más)
// ---------------------------------------------------------------------------

NATIVE(sys_cpus) { UNUSED; return Value((double)std::max(1u, std::thread::hardware_concurrency())); }
NATIVE(sys_user) {
    UNUSED;
    const char* u = std::getenv("USERNAME");
    if (!u) u = std::getenv("USER");
    return vm.str(u ? u : "");
}
NATIVE(sys_host) {
    UNUSED;
#ifdef _WIN32
    const char* h = std::getenv("COMPUTERNAME");
    return vm.str(h ? h : "");
#else
    char buf[256] = {0};
    gethostname(buf, sizeof buf - 1);
    return vm.str(buf);
#endif
}
NATIVE(sys_exe) { UNUSED; return vm.str(exePath()); }
NATIVE(sys_setEnv) {
    std::string n = argStr(vm, args, argc, 0, "System.setEnv");
    Value v = optArg(args, argc, 1);
#ifdef _WIN32
    _putenv_s(n.c_str(), v.isNull() ? "" : vm.toString(v).c_str());
#else
    if (v.isNull()) unsetenv(n.c_str()); else setenv(n.c_str(), vm.toString(v).c_str(), 1);
#endif
    return Value();
}
NATIVE(sys_arch) {
    UNUSED;
#if defined(__x86_64__) || defined(_M_X64)
    return vm.str("x64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    return vm.str("arm64");
#elif defined(__i386__) || defined(_M_IX86)
    return vm.str("x86");
#else
    return vm.str("desconocida");
#endif
}

void registerSystemLibs(VM& vm) {
    ObjModule* proc = vm.nativeModule("Process", "Ejecutar otros programas y órdenes del sistema, y terminar el programa.");
    vm.defineFn(proc, "run", "(programa: text, argumentos?: list<text>, opciones?: map): map",
                "Ejecuta un programa y espera a que termine. Devuelve {code, output, error, ok, timedOut}. "
                "Opciones: cwd (carpeta), input (texto de entrada), timeout (segundos), env (mapa de variables).",
                proc_run, "var r = Process.run(\"git\", [\"--version\"])\nprint(r.output)");
    vm.defineFn(proc, "shell", "(orden: text, opciones?: map): map",
                "Ejecuta una orden con el intérprete del sistema (cmd en Windows, sh en Linux y macOS). Devuelve lo mismo que run.",
                proc_shell, "print(Process.shell(\"echo hola\").output)");
    vm.defineFn(proc, "exit", "(codigo?: number = 0): void", "Termina el programa con ese código de salida.", proc_exit);
    vm.defineFn(proc, "open", "(destino: text): bool", "Abre un archivo, carpeta o dirección web con la aplicación predeterminada.", proc_open);
    vm.defineProp(proc, "pid", "number", "Identificador del proceso actual.", proc_pid);

    ObjModule* path = vm.nativeModule("Path", "Trabajar con rutas de archivos sin tocar el disco: unir, separar, extensiones...");
    vm.defineFn(path, "join", "(...partes: text): text", "Une partes de una ruta: Path.join(\"datos\", \"nivel1.json\").", path_join);
    vm.defineFn(path, "dir", "(ruta: text): text", "Carpeta que contiene la ruta.", path_dir);
    vm.defineFn(path, "name", "(ruta: text): text", "Nombre del archivo con extensión.", path_name);
    vm.defineFn(path, "stem", "(ruta: text): text", "Nombre del archivo sin extensión.", path_stem);
    vm.defineFn(path, "ext", "(ruta: text): text", "Extensión con el punto (\".json\") o \"\".", path_ext);
    vm.defineFn(path, "withExt", "(ruta: text, extension: text): text", "La misma ruta con otra extensión.", path_withExt);
    vm.defineFn(path, "absolute", "(ruta: text): text", "Ruta completa (las relativas parten de la carpeta del programa).", path_absolute);
    vm.defineFn(path, "normalize", "(ruta: text): text", "Quita . y .. y unifica las barras.", path_normalize);
    vm.defineFn(path, "isAbsolute", "(ruta: text): bool", "¿Es una ruta completa?", path_isAbsolute);
    vm.defineFn(path, "relative", "(ruta: text, desde?: text = \".\"): text", "Ruta relativa a otra carpeta.", path_relative);
    vm.defineProp(path, "home", "text", "Carpeta del usuario.", path_home);
    vm.defineProp(path, "temp", "text", "Carpeta de archivos temporales.", path_temp);
    vm.defineProp(path, "separator", "text", "Separador de carpetas del sistema (\\\\ en Windows, / en el resto).", path_sep);

    ObjModule* file = existingModule(vm, "File");
    vm.defineFn(file, "copy", "(origen: text, destino: text): bool", "Copia un archivo (o una carpeta entera) reemplazando el destino.", file_copy);
    vm.defineFn(file, "move", "(origen: text, destino: text): bool", "Mueve o renombra un archivo o carpeta.", file_move);
    vm.defineFn(file, "size", "(ruta: text): number", "Tamaño del archivo en bytes.", file_size);
    vm.defineFn(file, "modified", "(ruta: text): number", "Fecha de la última modificación (segundos desde 1970, como Time.now()).", file_modified);
    vm.defineFn(file, "isFile", "(ruta: text): bool", "¿Existe y es un archivo (no una carpeta)?", file_isFile);
    vm.defineFn(file, "listAll", "(carpeta?: text = \".\", extension?: text): list<text>",
                "Todos los archivos de la carpeta y sus subcarpetas (rutas relativas), opcionalmente solo con una extensión.", file_listAll);
    vm.defineFn(file, "deleteDir", "(carpeta: text): number",
                "Borra una carpeta con todo su contenido y devuelve cuántos elementos borró. ¡Cuidado: no se puede deshacer!", file_deleteDir);
    vm.defineFn(file, "tempFile", "(extension?: text = \".tmp\"): text", "Ruta nueva en la carpeta temporal (no crea el archivo).", file_tempFile);

    ObjModule* time = vm.nativeModule("Time", "Fechas, horas y medir cuánto tarda algo.");
    vm.defineFn(time, "now", "(): number", "Momento actual en segundos desde el 1 de enero de 1970 (con decimales).", time_now);
    vm.defineFn(time, "date", "(momento?: number): map",
                "Partes de una fecha: {year, month, day, hour, minute, second, millisecond, weekday, dayName, yearDay}.", time_date);
    vm.defineFn(time, "format", "(momento?: number, formato?: text = \"%Y-%m-%d %H:%M:%S\"): text",
                "Fecha como texto. Formato de strftime: %Y año, %m mes, %d día, %H hora, %M minutos, %S segundos.", time_format,
                "print(Time.format(Time.now(), \"%d/%m/%Y\"))");
    vm.defineFn(time, "make", "(anio: number, mes?: number = 1, dia?: number = 1, hora?: number = 0, minuto?: number = 0, segundo?: number = 0): number",
                "Momento (segundos desde 1970) de una fecha y hora locales.", time_make);
    vm.defineFn(time, "measure", "(funcion: function): number", "Ejecuta la función y devuelve cuántos segundos tardó.", time_measure);
    vm.defineFn(time, "clock", "(): number", "Segundos desde que empezó el programa (reloj que nunca va hacia atrás).", time_clock);

    ObjModule* sys = existingModule(vm, "System");
    vm.defineProp(sys, "cpuCount", "number", "Número de núcleos del procesador.", sys_cpus);
    vm.defineProp(sys, "user", "text", "Nombre del usuario.", sys_user);
    vm.defineProp(sys, "hostname", "text", "Nombre del equipo.", sys_host);
    vm.defineProp(sys, "exe", "text", "Ruta del ejecutable de HyperEZ.", sys_exe);
    vm.defineProp(sys, "arch", "text", "Arquitectura del procesador: \"x64\", \"arm64\"...", sys_arch);
    vm.defineFn(sys, "setEnv", "(nombre: text, valor?: any): void", "Cambia (o con null quita) una variable de entorno para este programa y los que lance.", sys_setEnv);
}

} // namespace hz
