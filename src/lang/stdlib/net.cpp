// Red: peticiones HTTP y HTTPS (Http). Windows usa WinHTTP; Linux y macOS, la orden curl.
// Sirve para consultar servicios web y APIs (también de IA), descargar archivos o enviar datos.
#include "lang/stdlib/native.h"
#include "core/process.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#endif

namespace hz {

struct HttpRequest {
    std::string method = "GET", url, body;
    std::vector<std::pair<std::string, std::string>> headers;
    int timeoutMs = 30000;
};
struct HttpResponse {
    int status = 0;
    std::string body, error;
    std::vector<std::pair<std::string, std::string>> headers;
};

static void parseHeaderBlock(const std::string& raw, HttpResponse& r) {
    r.headers.clear();
    for (auto& line : splitLines(raw)) {
        size_t c = line.find(':');
        if (c == std::string::npos || line.rfind("HTTP/", 0) == 0) continue;
        std::string v = trim(line.substr(c + 1));
        if (!v.empty() && v.back() == '\r') v.pop_back();
        r.headers.push_back({toLower(trim(line.substr(0, c))), v});
    }
}

#ifdef _WIN32
static std::wstring W(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
static std::string N(const wchar_t* w, size_t len) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, (int)len, nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, (int)len, s.data(), n, nullptr, nullptr);
    return s;
}
static std::string winHttpError(DWORD e) {
    switch (e) {
        case 12007: return "no se encontró el servidor (¿hay conexión a internet? ¿está bien escrita la dirección?)";
        case 12002: return "se agotó el tiempo de espera";
        case 12029: case 12030: return "no se pudo conectar con el servidor";
        case 12175: case 12045: case 12037: case 12038: return "el certificado de seguridad (HTTPS) del servidor no es válido";
        case 12005: case 12006: return "la dirección no es válida (debe empezar por http:// o https://)";
        default: return "error de red " + std::to_string(e);
    }
}
static HttpResponse httpSend(const HttpRequest& q) {
    HttpResponse r;
    std::wstring url = W(q.url);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { r.error = winHttpError(GetLastError()); return r; }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring object = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength) + std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (object.empty()) object = L"/";
    std::wstring agent = W(std::string("HyperEZ/") + HYPEREZ_VERSION);
    HINTERNET s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) { r.error = winHttpError(GetLastError()); return r; }
    WinHttpSetTimeouts(s, q.timeoutMs, q.timeoutMs, q.timeoutMs, q.timeoutMs);
    HINTERNET c = WinHttpConnect(s, host.c_str(), uc.nPort, 0);
    HINTERNET h = c ? WinHttpOpenRequest(c, W(q.method).c_str(), object.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                    : nullptr;
    std::wstring hdrs;
    for (auto& [k, v] : q.headers) hdrs += W(k) + L": " + W(v) + L"\r\n";
    bool ok = h && WinHttpSendRequest(h, hdrs.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdrs.c_str(), hdrs.empty() ? 0 : (DWORD)-1L,
                                      q.body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)q.body.data(), (DWORD)q.body.size(),
                                      (DWORD)q.body.size(), 0) &&
              WinHttpReceiveResponse(h, nullptr);
    if (!ok) {
        r.error = winHttpError(GetLastError());
    } else {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
                            WINHTTP_NO_HEADER_INDEX);
        r.status = (int)status;
        DWORD len = 0;
        WinHttpQueryHeaders(h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &len, WINHTTP_NO_HEADER_INDEX);
        if (len) {
            std::wstring raw(len / sizeof(wchar_t), L'\0');
            if (WinHttpQueryHeaders(h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &len, WINHTTP_NO_HEADER_INDEX))
                parseHeaderBlock(N(raw.data(), len / sizeof(wchar_t)), r);
        }
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(h, &avail) || avail == 0) break;
            size_t at = r.body.size();
            r.body.resize(at + avail);
            DWORD read = 0;
            if (!WinHttpReadData(h, r.body.data() + at, avail, &read)) { r.body.resize(at); break; }
            r.body.resize(at + read);
            if (read == 0) break;
        }
    }
    if (h) WinHttpCloseHandle(h);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return r;
}
#else
static HttpResponse httpSend(const HttpRequest& q) {
    HttpResponse r;
    std::string tmp = "/tmp/hyperez-http-" + std::to_string(currentProcessId()) + "-" + std::to_string((long long)(nowSeconds() * 1e6));
    std::string bodyFile = tmp + ".body", headFile = tmp + ".head";
    std::vector<std::string> argv{"curl", "-sS", "-L", "-X", q.method, "--max-time", std::to_string(std::max(1, q.timeoutMs / 1000)),
                                  "-o", bodyFile, "-D", headFile, "-w", "%{http_code}", "-A", std::string("HyperEZ/") + HYPEREZ_VERSION};
    for (auto& [k, v] : q.headers) { argv.push_back("-H"); argv.push_back(k + ": " + v); }
    ProcessOptions o;
    if (!q.body.empty()) { argv.push_back("--data-binary"); argv.push_back("@-"); o.input = q.body; }
    argv.push_back(q.url);
    ProcessResult p = runCapture(argv, o);
    if (!p.started) {
        r.error = "hace falta la orden 'curl' para usar Http en este sistema";
    } else if (p.code != 0) {
        r.error = trim(p.err).empty() ? "error de red (curl " + std::to_string(p.code) + ")" : trim(p.err);
    } else {
        r.status = std::atoi(p.out.c_str());
        readFile(bodyFile, r.body);
        std::string head;
        readFile(headFile, head);
        // con redirecciones hay varios bloques de cabeceras: el último es el bueno
        size_t last = head.rfind("HTTP/");
        parseHeaderBlock(last == std::string::npos ? head : head.substr(last), r);
    }
    std::remove(bodyFile.c_str());
    std::remove(headFile.c_str());
    return r;
}
#endif

static Value jsonCall(VM& vm, const char* fn, Value arg) {
    Value json = vm.builtins[vm.builtinSlot("JSON")];
    Value f = vm.getProperty(json, vm.intern(fn), false);
    return vm.call(f, 1, &arg);
}

// Construye la petición a partir de (url, cuerpo?, opciones?)
static HttpRequest makeRequest(VM& vm, const std::string& method, Value* args, int argc, int urlIdx, int bodyIdx, int optIdx,
                               const char* fname) {
    HttpRequest q;
    q.method = method;
    q.url = argStr(vm, args, argc, urlIdx, fname);
    if (q.url.rfind("http://", 0) != 0 && q.url.rfind("https://", 0) != 0)
        vm.raise(std::string(fname) + ": la dirección debe empezar por http:// o https://");
    bool hasType = false;
    Value opts = optArg(args, argc, optIdx);
    if (!opts.isNull()) {
        if (!isObjType(opts, OT::Map)) vm.raise(std::string(fname) + ": las opciones deben ser un mapa {headers, timeout}");
        auto* m = static_cast<ObjMap*>(opts.o);
        Value h = optField(vm, m, "headers"), t = optField(vm, m, "timeout");
        if (isObjType(h, OT::Map)) {
            for (auto& e : static_cast<ObjMap*>(h.o)->entries) {
                if (!e.alive) continue;
                std::string k = vm.toString(e.key);
                if (toLower(k) == "content-type") hasType = true;
                q.headers.push_back({k, vm.toString(e.value)});
            }
        }
        if (t.isNum()) q.timeoutMs = std::max(1, (int)(t.n * 1000));
    }
    if (bodyIdx >= 0) {
        Value b = optArg(args, argc, bodyIdx);
        if (isString(b)) {
            q.body = asString(b)->str;
        } else if (isObjType(b, OT::Map) || isObjType(b, OT::List)) {
            q.body = vm.toString(jsonCall(vm, "stringify", b));
            if (!hasType) q.headers.push_back({"Content-Type", "application/json"});
        } else if (isObjType(b, OT::Foreign)) {
            Value t = vm.callMethod(b, "toText", 0, nullptr);
            q.body = vm.toString(t);
        } else if (!b.isNull()) {
            q.body = vm.toString(b);
        }
    }
    return q;
}

static HttpResponse send(VM& vm, const HttpRequest& q, const char* fname) {
    vm.flushOutput();
    HttpResponse r = httpSend(q);
    if (!r.error.empty()) vm.raise(std::string(fname) + ": " + r.error + " (" + q.url + ")");
    return r;
}

static Value responseMap(VM& vm, const HttpResponse& r) {
    ObjMap* m = vm.newMap();
    TempRoot keep(vm, m);
    put(vm, m, "status", Value(r.status));
    put(vm, m, "ok", Value::boolean(r.status >= 200 && r.status < 300));
    put(vm, m, "body", vm.str(r.body));
    ObjMap* h = vm.newMap();
    put(vm, m, "headers", Value::object(h));
    for (auto& [k, v] : r.headers) h->set(vm.str(k), vm.str(v));
    return Value::object(m);
}

static Value jsonResult(VM& vm, const HttpResponse& r, const HttpRequest& q, const char* fname) {
    if (r.status >= 400) vm.raise(std::string(fname) + ": el servidor respondió " + std::to_string(r.status) + " (" + q.url + ")");
    return jsonCall(vm, "parse", vm.str(r.body));
}

NATIVE(http_get) { auto q = makeRequest(vm, "GET", args, argc, 0, -1, 1, "Http.get"); return responseMap(vm, send(vm, q, "Http.get")); }
NATIVE(http_post) { auto q = makeRequest(vm, "POST", args, argc, 0, 1, 2, "Http.post"); return responseMap(vm, send(vm, q, "Http.post")); }
NATIVE(http_request) {
    std::string method = toLower(argStr(vm, args, argc, 0, "Http.request"));
    for (auto& c : method) c = (char)std::toupper((unsigned char)c);
    auto q = makeRequest(vm, method, args, argc, 1, 2, 3, "Http.request");
    return responseMap(vm, send(vm, q, "Http.request"));
}
NATIVE(http_getJson) {
    auto q = makeRequest(vm, "GET", args, argc, 0, -1, 1, "Http.getJson");
    q.headers.push_back({"Accept", "application/json"});
    return jsonResult(vm, send(vm, q, "Http.getJson"), q, "Http.getJson");
}
NATIVE(http_postJson) {
    auto q = makeRequest(vm, "POST", args, argc, 0, 1, 2, "Http.postJson");
    q.headers.push_back({"Accept", "application/json"});
    return jsonResult(vm, send(vm, q, "Http.postJson"), q, "Http.postJson");
}
NATIVE(http_download) {
    auto q = makeRequest(vm, "GET", args, argc, 0, -1, 2, "Http.download");
    std::string dest = resolvePath(vm, argStr(vm, args, argc, 1, "Http.download"));
    HttpResponse r = send(vm, q, "Http.download");
    if (r.status >= 400) vm.raise("Http.download: el servidor respondió " + std::to_string(r.status));
    if (!pathDir(dest).empty()) makeDirs(pathDir(dest));
    if (!writeFile(dest, r.body)) vm.raise("Http.download: no se puede escribir '" + dest + "'");
    return Value((double)r.body.size());
}
static std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string r;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r += (char)c;
        else { r += '%'; r += hex[c >> 4]; r += hex[c & 15]; }
    }
    return r;
}
NATIVE(http_encode) { return vm.str(urlEncode(argStr(vm, args, argc, 0, "Http.encode"))); }
NATIVE(http_query) {
    ObjMap* m = argMap(vm, args, argc, 0, "Http.query");
    std::string r;
    for (auto& e : m->entries) {
        if (!e.alive) continue;
        if (!r.empty()) r += '&';
        r += urlEncode(vm.toString(e.key)) + "=" + urlEncode(isString(e.value) ? asString(e.value)->str : vm.toString(e.value));
    }
    return vm.str(r);
}

void registerNet(VM& vm) {
    ObjModule* http = vm.nativeModule("Http", "Peticiones a servidores web (HTTP y HTTPS): APIs, descargas y envío de datos.");
    const char* resp = "Devuelve {status, ok, body, headers}. Opciones: headers (mapa) y timeout (segundos, 30 por defecto).";
    vm.defineFn(http, "get", "(url: text, opciones?: map): map", std::string("Pide una página o recurso. ") + resp, http_get,
                "var r = Http.get(\"https://example.com\")\nprint(r.status, r.body.length)");
    vm.defineFn(http, "post", "(url: text, cuerpo?: any, opciones?: map): map",
                std::string("Envía datos. Un mapa o lista se envía como JSON. ") + resp, http_post);
    vm.defineFn(http, "request", "(metodo: text, url: text, cuerpo?: any, opciones?: map): map",
                std::string("Petición con cualquier método (PUT, DELETE, PATCH...). ") + resp, http_request);
    vm.defineFn(http, "getJson", "(url: text, opciones?: map): any", "Pide JSON y lo devuelve ya convertido (mapas, listas...).", http_getJson);
    vm.defineFn(http, "postJson", "(url: text, datos: any, opciones?: map): any", "Envía datos como JSON y devuelve la respuesta convertida.", http_postJson);
    vm.defineFn(http, "download", "(url: text, ruta: text, opciones?: map): number", "Descarga a un archivo y devuelve los bytes descargados.", http_download);
    vm.defineFn(http, "encode", "(texto: text): text", "Codifica un texto para ponerlo en una URL (espacios, acentos...).", http_encode);
    vm.defineFn(http, "query", "(parametros: map): text", "Mapa a parámetros de URL: {q: \"hola mundo\", n: 2} -> \"q=hola%20mundo&n=2\".", http_query);
}

} // namespace hz
