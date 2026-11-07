// Servidor web (Http.serve) para crear páginas, APIs y servicios con HyperEZ,
// y procesos en segundo plano (Process.start).
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "lang/stdlib/native.h"
#include "core/process.h"

#include <algorithm>
#include <cctype>
#include <csignal>

namespace hz {

#ifdef _WIN32
using sock_t = SOCKET;
static const sock_t kBadSock = INVALID_SOCKET;
static void closeSock(sock_t s) { closesocket(s); }
static bool netInit() {
    static bool ok = [] { WSADATA d; return WSAStartup(MAKEWORD(2, 2), &d) == 0; }();
    return ok;
}
#else
using sock_t = int;
static const sock_t kBadSock = -1;
static void closeSock(sock_t s) { close(s); }
static bool netInit() { return true; }
#endif

static std::string urlDecode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') r += ' ';
        else if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) && std::isxdigit((unsigned char)s[i + 2])) {
            r += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else r += s[i];
    }
    return r;
}

static const char* mimeType(const std::string& ext) {
    static const std::pair<const char*, const char*> types[] = {
        {".html", "text/html; charset=utf-8"}, {".htm", "text/html; charset=utf-8"}, {".css", "text/css; charset=utf-8"},
        {".js", "text/javascript; charset=utf-8"}, {".json", "application/json"}, {".txt", "text/plain; charset=utf-8"},
        {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".gif", "image/gif"}, {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"}, {".wasm", "application/wasm"}, {".hz", "text/plain; charset=utf-8"}, {".hez", "text/plain; charset=utf-8"},
        {".md", "text/markdown; charset=utf-8"}, {".csv", "text/csv"}, {".webp", "image/webp"}, {".xml", "application/xml"},
        {".mp3", "audio/mpeg"}, {".wav", "audio/wav"}, {".pdf", "application/pdf"}};
    for (auto& [e, t] : types) if (ext == e) return t;
    return "application/octet-stream";
}

static const char* statusText(int s) {
    switch (s) {
        case 200: return "OK"; case 201: return "Created"; case 204: return "No Content"; case 301: return "Moved Permanently";
        case 302: return "Found"; case 304: return "Not Modified"; case 400: return "Bad Request"; case 401: return "Unauthorized";
        case 403: return "Forbidden"; case 404: return "Not Found"; case 405: return "Method Not Allowed"; case 500: return "Internal Server Error";
        default: return "";
    }
}

static bool sendAll(sock_t c, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        int n = (int)send(c, data.data() + off, (int)std::min<size_t>(data.size() - off, 1 << 20), 0);
        if (n <= 0) return false;
        off += (size_t)n;
    }
    return true;
}

struct Request {
    std::string method, target, path, query, body;
    std::vector<std::pair<std::string, std::string>> headers;
};

// Lee una petición HTTP/1.1 completa (cabeceras + cuerpo según Content-Length).
static bool readRequest(sock_t c, Request& r) {
    std::string buf;
    char tmp[8192];
    size_t headEnd;
    while ((headEnd = buf.find("\r\n\r\n")) == std::string::npos) {
        int n = (int)recv(c, tmp, sizeof tmp, 0);
        if (n <= 0 || buf.size() > (1 << 20)) return false;
        buf.append(tmp, (size_t)n);
    }
    auto lines = splitLines(buf.substr(0, headEnd));
    if (lines.empty()) return false;
    std::string first = lines[0];
    if (!first.empty() && first.back() == '\r') first.pop_back();
    size_t a = first.find(' '), b = first.rfind(' ');
    if (a == std::string::npos || b <= a) return false;
    r.method = first.substr(0, a);
    r.target = first.substr(a + 1, b - a - 1);
    size_t q = r.target.find('?');
    r.path = urlDecode(r.target.substr(0, q));
    r.query = q == std::string::npos ? "" : r.target.substr(q + 1);
    size_t length = 0;
    for (size_t i = 1; i < lines.size(); i++) {
        std::string l = lines[i];
        if (!l.empty() && l.back() == '\r') l.pop_back();
        size_t colon = l.find(':');
        if (colon == std::string::npos) continue;
        std::string k = toLower(trim(l.substr(0, colon))), v = trim(l.substr(colon + 1));
        if (k == "content-length") length = (size_t)std::strtoull(v.c_str(), nullptr, 10);
        r.headers.push_back({k, v});
    }
    if (length > (64u << 20)) return false;
    r.body = buf.substr(headEnd + 4);
    while (r.body.size() < length) {
        int n = (int)recv(c, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        r.body.append(tmp, (size_t)n);
    }
    r.body.resize(std::min(r.body.size(), length));
    return true;
}

static Value jsonCallS(VM& vm, const char* fn, Value arg) {
    Value json = vm.builtins[vm.builtinSlot("JSON")];
    Value f = vm.getProperty(json, vm.intern(fn), false);
    return vm.call(f, 1, &arg);
}

static Value requestMap(VM& vm, const Request& r) {
    ObjMap* m = vm.newMap();
    TempRoot keep(vm, m);
    put(vm, m, "method", vm.str(r.method));
    put(vm, m, "path", vm.str(r.path));
    put(vm, m, "url", vm.str(r.target));
    put(vm, m, "body", vm.str(r.body));
    ObjMap* q = vm.newMap();
    put(vm, m, "query", Value::object(q));
    size_t s = 0;
    while (s < r.query.size()) {
        size_t e = r.query.find('&', s);
        if (e == std::string::npos) e = r.query.size();
        std::string part = r.query.substr(s, e - s);
        size_t eq = part.find('=');
        if (!part.empty()) q->set(vm.str(urlDecode(part.substr(0, eq))), vm.str(eq == std::string::npos ? "" : urlDecode(part.substr(eq + 1))));
        s = e + 1;
    }
    ObjMap* h = vm.newMap();
    put(vm, m, "headers", Value::object(h));
    bool isJson = false;
    for (auto& [k, v] : r.headers) {
        h->set(vm.str(k), vm.str(v));
        if (k == "content-type" && v.find("json") != std::string::npos) isJson = true;
    }
    Value json;
    if (isJson && !r.body.empty()) {
        try { json = jsonCallS(vm, "parse", vm.str(r.body)); } catch (...) { json = Value(); }
    }
    put(vm, m, "json", json);
    return Value::object(m);
}

struct Response {
    int status = 200;
    std::string body, type = "text/html; charset=utf-8";
    std::vector<std::pair<std::string, std::string>> headers;
};

// Sirve un archivo de la carpeta estática (sin salir de ella).
static bool staticFile(const std::string& root, const std::string& urlPath, Response& out) {
    if (root.empty() || urlPath.find("..") != std::string::npos) return false;
    std::string rel = urlPath;
    while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
    std::string p = pathJoin(root, rel.empty() ? "index.html" : rel);
    if (dirExists(p)) p = pathJoin(p, "index.html");
    std::string data;
    if (!fileExists(p) || !readFile(p, data)) return false;
    out.status = 200;
    out.body = std::move(data);
    out.type = mimeType(toLower(pathExt(p)));
    return true;
}

static Response toResponse(VM& vm, Value r) {
    Response out;
    if (isString(r)) { out.body = asString(r)->str; return out; }
    if (isObjType(r, OT::Map)) {
        auto* m = static_cast<ObjMap*>(r.o);
        Value status = optField(vm, m, "status"), body = optField(vm, m, "body"), json = optField(vm, m, "json");
        Value file = optField(vm, m, "file"), type = optField(vm, m, "type"), headers = optField(vm, m, "headers");
        bool explicitResp = !status.isNull() || !body.isNull() || !json.isNull() || !file.isNull() || !headers.isNull();
        if (explicitResp) {
            if (status.isNum()) out.status = (int)status.n;
            if (!json.isNull()) { out.body = vm.toString(jsonCallS(vm, "stringify", json)); out.type = "application/json"; }
            else if (!file.isNull()) {
                std::string p = resolvePath(vm, vm.toString(file));
                if (!readFile(p, out.body)) { out.status = 404; out.body = "No existe " + vm.toString(file); }
                else out.type = mimeType(toLower(pathExt(p)));
            } else if (isString(body)) out.body = asString(body)->str;
            else if (!body.isNull()) out.body = vm.toString(body);
            if (isString(type)) out.type = asString(type)->str;
            if (isObjType(headers, OT::Map))
                for (auto& e : static_cast<ObjMap*>(headers.o)->entries)
                    if (e.alive) out.headers.push_back({vm.toString(e.key), vm.toString(e.value)});
            return out;
        }
    }
    if (isObjType(r, OT::Map) || isObjType(r, OT::List)) {
        out.body = vm.toString(jsonCallS(vm, "stringify", r));
        out.type = "application/json";
        return out;
    }
    out.body = vm.toString(r);
    out.type = "text/plain; charset=utf-8";
    return out;
}

NATIVE(http_serve) {
    int port = (int)argNum(vm, args, argc, 0, "Http.serve");
    Value handler = optArg(args, argc, 1);
    Value opts = optArg(args, argc, 2);
    ObjMap* o = isObjType(opts, OT::Map) ? static_cast<ObjMap*>(opts.o) : nullptr;
    std::string host = o && isString(optField(vm, o, "host")) ? asString(optField(vm, o, "host"))->str : "127.0.0.1";
    std::string root = o && isString(optField(vm, o, "static")) ? resolvePath(vm, asString(optField(vm, o, "static"))->str) : "";
    double maxReq = o && optField(vm, o, "maxRequests").isNum() ? optField(vm, o, "maxRequests").n : 0;
    bool quiet = o && !isFalsy(optField(vm, o, "quiet"));
    bool cors = o && !isFalsy(optField(vm, o, "cors"));
    if (port < 1 || port > 65535) vm.raise("Http.serve: el puerto debe estar entre 1 y 65535");
    if (handler.isNull() && root.empty()) vm.raise("Http.serve: falta la función que responde (o la opción static con una carpeta)");
    if (!netInit()) vm.raise("Http.serve: no se pudo iniciar la red");
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN); // un cliente que cierra antes de tiempo no debe terminar el programa
#endif

    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == kBadSock) vm.raise("Http.serve: no se pudo crear el socket");
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (host == "0.0.0.0" || host == "*") addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else if (inet_pton(AF_INET, host == "localhost" ? "127.0.0.1" : host.c_str(), &addr.sin_addr) != 1) {
        closeSock(s);
        vm.raise("Http.serve: dirección no válida '" + host + "'");
    }
    if (bind(s, (sockaddr*)&addr, sizeof addr) != 0 || listen(s, 32) != 0) {
        closeSock(s);
        vm.raise("Http.serve: el puerto " + std::to_string(port) + " está ocupado o no se puede usar");
    }
    if (!quiet) {
        vm.print(std::string("Servidor en http://") + (host == "0.0.0.0" ? "localhost" : host) + ":" + std::to_string(port) +
                 "  (Ctrl+C para parar)\n");
        vm.flushOutput();
    }
    double served = 0;
    g_interrupt = false;
    while (maxReq <= 0 || served < maxReq) {
        if (g_interrupt.load()) break;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s, &set);
        timeval tv{0, 200000};
        if (select((int)s + 1, &set, nullptr, nullptr, &tv) <= 0) continue;
        sock_t c = accept(s, nullptr, nullptr);
        if (c == kBadSock) continue;
        // los navegadores abren conexiones de reserva que no envían nada: sin un límite, el servidor (que atiende de
        // una en una) se quedaría esperando en una y no contestaría a las demás
#ifdef _WIN32
        DWORD espera = 1500;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&espera, sizeof espera);
#else
        timeval espera{1, 500000};
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&espera, sizeof espera);
#endif
        Request req;
        Response resp;
        if (!readRequest(c, req)) { closeSock(c); continue; }
        bool done = false;
        if (req.method == "OPTIONS" && cors) { resp.status = 204; done = true; }
        if (!done && !handler.isNull()) {
            Value rm = requestMap(vm, req);
            vm.tempRoots.push_back(rm);
            Value out;
            bool ok = vm.tryCall(handler, 1, &rm, &out);
            vm.tempRoots.pop_back();
            if (!ok) {
                resp.status = 500;
                resp.type = "text/plain; charset=utf-8";
                resp.body = "Error en el servidor: " + vm.errorMessage(vm.lastError);
                vm.reportError(vm.lastError);
                done = true;
            } else if (!out.isNull()) {
                resp = toResponse(vm, out);
                done = true;
            }
        }
        if (!done && !staticFile(root, req.path, resp)) {
            resp.status = 404;
            resp.type = "text/plain; charset=utf-8";
            resp.body = "No encontrado: " + req.path;
        }
        std::string head = "HTTP/1.1 " + std::to_string(resp.status) + " " + statusText(resp.status) + "\r\n";
        head += "Content-Type: " + resp.type + "\r\nContent-Length: " + std::to_string(resp.body.size()) + "\r\n";
        head += std::string("Server: HyperEZ/") + HYPEREZ_VERSION + "\r\nConnection: close\r\n";
        if (cors) head += "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: *\r\nAccess-Control-Allow-Methods: GET, POST, PUT, DELETE, PATCH, OPTIONS\r\n";
        for (auto& [k, v] : resp.headers) head += k + ": " + v + "\r\n";
        head += "\r\n";
        sendAll(c, head + (req.method == "HEAD" ? std::string() : resp.body));
        closeSock(c);
        served++;
        if (!quiet) vm.print(req.method + " " + req.target + " -> " + std::to_string(resp.status) + "\n");
        vm.flushOutput(); // lo que escriba el manejador (print) se ve al momento, no al parar el servidor
    }
    closeSock(s);
    g_interrupt = false;
    return Value(served);
}

// ---- Process.start ----------------------------------------------------------

struct ChildProc {
    void* handle = nullptr;
    bool finished = false;
    int code = 0;
};
static void freeChild(void* p) {
    auto* c = static_cast<ChildProc*>(p);
    if (!c->finished) endProcess(c->handle, false); // se suelta el control; el proceso sigue
    delete c;
}
static ChildProc& CP(Value* args) { return *static_cast<ChildProc*>(static_cast<ObjForeign*>(args[-1].o)->ptr); }

NATIVE(proc_start) {
    std::string prog = argStr(vm, args, argc, 0, "Process.start");
    std::vector<std::string> argv{prog};
    for (auto& a : argStrList(vm, args, argc, 1, "Process.start")) argv.push_back(a);
    vm.flushOutput();
    void* h = spawnProcess(argv);
    if (!h) vm.raise("Process.start: no se pudo ejecutar '" + prog + "'");
    auto* c = new ChildProc;
    c->handle = h;
    ObjClass* cls = static_cast<ObjClass*>(vm.builtins[vm.builtinSlot("ProcessHandle")].o);
    return Value::object(vm.newForeign(cls, c, freeChild));
}
NATIVE(ph_wait) {
    ChildProc& c = CP(args);
    if (c.finished) return Value(c.code);
    double t = argNum(vm, args, argc, 0, "ProcessHandle.wait", -1);
    vm.flushOutput();
    int code = 0;
    if (!processFinished(c.handle, t < 0 ? -1 : (int)(t * 1000), code)) return Value();
    c.finished = true;
    c.code = code;
    endProcess(c.handle, false);
    return Value(code);
}
NATIVE(ph_running) {
    UNUSED;
    ChildProc& c = CP(args);
    if (c.finished) return Value::boolean(false);
    int code = 0;
    if (processFinished(c.handle, 0, code)) {
        c.finished = true;
        c.code = code;
        endProcess(c.handle, false);
    }
    return Value::boolean(!c.finished);
}
NATIVE(ph_kill) {
    UNUSED;
    ChildProc& c = CP(args);
    if (c.finished) return Value::boolean(false);
    endProcess(c.handle, true);
    c.finished = true;
    c.code = -1;
    return Value::boolean(true);
}

void registerServer(VM& vm) {
    ObjModule* http = existingModule(vm, "Http");
    vm.defineFn(http, "serve", "(puerto: number, manejador?: function, opciones?: map): number",
                "Servidor web: llama a manejador(peticion) por cada petición y envía lo que devuelva. La petición tiene "
                "{method, path, url, query, headers, body, json}. La respuesta puede ser un texto (HTML), un mapa o lista (JSON) "
                "o {status, body, json, file, type, headers}. Opciones: host (\"127.0.0.1\"; \"0.0.0.0\" para la red local), "
                "static (carpeta de archivos), cors, quiet y maxRequests. Devuelve cuántas peticiones atendió.",
                http_serve, "Http.serve(8080, req => \"<h1>Hola desde ${req.path}</h1>\")");

    ObjModule* proc = existingModule(vm, "Process");
    vm.defineFn(proc, "start", "(programa: text, argumentos?: list<text>): ProcessHandle",
                "Lanza un programa en segundo plano (comparte la consola) y devuelve un control para esperarlo o terminarlo.", proc_start);
    ObjClass* ph = vm.nativeClass("ProcessHandle", "Programa lanzado con Process.start.");
    vm.defineMethod(ph, "wait", "(segundos?: number): number?", "Espera a que termine (como mucho esos segundos) y devuelve su código, o null si sigue.", ph_wait);
    vm.defineMethod(ph, "kill", "(): bool", "Termina el programa.", ph_kill);
    vm.defineClassProp(ph, "running", "bool", "¿Sigue en marcha?", ph_running);
}

} // namespace hz
