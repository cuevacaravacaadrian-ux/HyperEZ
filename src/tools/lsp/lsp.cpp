// Servidor de lenguaje de HyperEZ (Language Server Protocol 3.17) por stdin/stdout.
// Reutiliza el analizador del compilador: diagnósticos, autocompletado, hover, firmas,
// ir a la definición, referencias, renombrar, resaltado, esquema, arreglos rápidos y formato.
#include "tools/tools.h"
#include "lang/vm/vm.h"
#include "lang/sema/analyzer.h"
#include "lang/syntax/format.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <set>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace hz {
namespace {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Transporte JSON-RPC
// ---------------------------------------------------------------------------

class Transport {
public:
    // Lee un mensaje completo; false al cerrarse la entrada.
    bool read(json& out) {
        size_t length = 0;
        for (;;) {
            std::string line;
            if (!readLine(line)) return false;
            if (line.empty()) break;
            if (toLower(line).rfind("content-length:", 0) == 0) length = (size_t)std::strtoull(line.c_str() + 15, nullptr, 10);
        }
        std::string body;
        if (!readBytes(body, length)) return false;
        out = json::parse(body, nullptr, false);
        return true;
    }
    void write(const json& msg) {
        std::string body = msg.dump(-1, ' ', false, json::error_handler_t::replace);
        if (outbox) { outbox->push_back(body); return; } // en memoria (editor web)
        std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        writeRaw(header);
        writeRaw(body);
    }
    // ¿Hay más datos esperando? (para no analizar en cada pulsación si llegan varias seguidas)
    bool pending() {
        if (pos < buf.size()) return true;
#ifdef _WIN32
        DWORD avail = 0;
        if (PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), nullptr, 0, nullptr, &avail, nullptr)) return avail > 0;
        return false;
#else
        pollfd p{0, POLLIN, 0};
        return poll(&p, 1, 0) > 0;
#endif
    }

    std::vector<std::string>* outbox = nullptr; // si no es nulo, los mensajes van aquí en lugar de a stdout

private:
    std::string buf;
    size_t pos = 0;
    bool fill() {
        if (pos > 0) { buf.erase(0, pos); pos = 0; }
        char tmp[65536];
#ifdef _WIN32
        DWORD n = 0;
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), tmp, sizeof tmp, &n, nullptr) || n == 0) return false;
#else
        ssize_t n = ::read(0, tmp, sizeof tmp);
        if (n <= 0) return false;
#endif
        buf.append(tmp, (size_t)n);
        return true;
    }
    bool readLine(std::string& line) {
        for (;;) {
            size_t e = buf.find("\r\n", pos);
            if (e != std::string::npos) { line = buf.substr(pos, e - pos); pos = e + 2; return true; }
            if (!fill()) return false;
        }
    }
    bool readBytes(std::string& out, size_t n) {
        while (buf.size() - pos < n) if (!fill()) return false;
        out = buf.substr(pos, n);
        pos += n;
        return true;
    }
    void writeRaw(const std::string& s) {
#ifdef _WIN32
        DWORD w;
        size_t off = 0;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        while (off < s.size() && WriteFile(h, s.data() + off, (DWORD)(s.size() - off), &w, nullptr)) off += w;
#else
        size_t off = 0;
        while (off < s.size()) { ssize_t w = ::write(1, s.data() + off, s.size() - off); if (w <= 0) break; off += (size_t)w; }
#endif
    }
};

// ---------------------------------------------------------------------------
// Posiciones: el compilador usa offsets en bytes (UTF-8); LSP usa líneas y unidades UTF-16.
// ---------------------------------------------------------------------------

struct LineIndex {
    const std::string* text = nullptr;
    std::vector<size_t> starts{0};
    explicit LineIndex(const std::string& t) : text(&t) {
        for (size_t i = 0; i < t.size(); i++) if (t[i] == '\n') starts.push_back(i + 1);
    }
    json pos(size_t off) const {
        off = std::min(off, text->size());
        size_t line = std::upper_bound(starts.begin(), starts.end(), off) - starts.begin() - 1;
        size_t ch = 0;
        for (size_t i = starts[line]; i < off;) {
            unsigned char c = (unsigned char)(*text)[i];
            size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
            ch += len == 4 ? 2 : 1;
            i += len;
        }
        return {{"line", line}, {"character", ch}};
    }
    size_t offset(int line, int character) const {
        if (line < 0) return 0;
        if ((size_t)line >= starts.size()) return text->size();
        size_t i = starts[line];
        int ch = 0;
        while (i < text->size() && (*text)[i] != '\n' && ch < character) {
            unsigned char c = (unsigned char)(*text)[i];
            size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
            ch += len == 4 ? 2 : 1;
            i += len;
        }
        return i;
    }
    json range(size_t a, size_t b) const { return {{"start", pos(a)}, {"end", pos(std::max(a, b))}}; }
};

// file:///c%3A/Users/x.hz -> c:/Users/x.hz
std::string uriToPath(const std::string& uri) {
    std::string s = uri;
    if (s.rfind("file://", 0) == 0) s = s.substr(7);
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) { r += (char)std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
        else r += s[i];
    }
#ifdef _WIN32
    if (r.size() > 2 && r[0] == '/' && r[2] == ':') r = r.substr(1);
    if (r.size() > 1 && r[1] == ':') r[0] = (char)std::toupper((unsigned char)r[0]);
#endif
    return pathNormalize(r);
}

std::string pathToUri(const std::string& path) {
    std::string p = path;
    for (auto& c : p) if (c == '\\') c = '/';
    std::string r = "file://";
    if (!p.empty() && p[0] != '/') r += '/';
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : p) {
        if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') r += (char)c;
        else { r += '%'; r += hex[c >> 4]; r += hex[c & 15]; }
    }
    return r;
}

std::string pathKey(const std::string& p) {
#ifdef _WIN32
    return toLower(pathNormalize(p));
#else
    return pathNormalize(p);
#endif
}

bool isIdentByte(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }

// ---------------------------------------------------------------------------
// Documentos y análisis
// ---------------------------------------------------------------------------

struct Analysis {
    std::unique_ptr<VM> vm;
    ModuleUnit* unit = nullptr;
};

struct Doc {
    std::string uri, path, text;
    int version = 0;
    Analysis an;
    bool dirty = true;
};

const char* kKeywords[] = {"var", "let", "const", "function", "return", "if", "else", "for", "foreach", "in", "while", "repeat", "break",
                           "continue", "class", "struct", "constructor", "this", "super", "extends", "implements", "interface", "enum",
                           "import", "from", "as", "public", "private", "protected", "static", "true", "false", "null", "and", "or",
                           "not", "is", "new", "try", "catch", "finally", "throw", "async", "await", "match", "event", "on", "once",
                           "emit", "get", "set", "step", "priority"};

int completionKind(SymKind k) {
    switch (k) {
        case SymKind::Variable: return 6;
        case SymKind::Constant: return 21;
        case SymKind::Parameter: return 6;
        case SymKind::Function: return 3;
        case SymKind::Class: return 7;
        case SymKind::Struct: return 22;
        case SymKind::Interface: return 8;
        case SymKind::Enum: return 13;
        case SymKind::EnumMember: return 20;
        case SymKind::Event: return 23;
        case SymKind::Field: return 5;
        case SymKind::Method: return 2;
        case SymKind::Property: return 10;
        case SymKind::Module: case SymKind::Library: return 9;
    }
    return 1;
}

int symbolKind(SymKind k) { // LSP SymbolKind
    switch (k) {
        case SymKind::Variable: return 13;
        case SymKind::Constant: return 14;
        case SymKind::Parameter: return 13;
        case SymKind::Function: return 12;
        case SymKind::Class: return 5;
        case SymKind::Struct: return 23;
        case SymKind::Interface: return 11;
        case SymKind::Enum: return 10;
        case SymKind::EnumMember: return 22;
        case SymKind::Event: return 24;
        case SymKind::Field: return 8;
        case SymKind::Method: return 6;
        case SymKind::Property: return 7;
        case SymKind::Module: case SymKind::Library: return 2;
    }
    return 13;
}

const char* kindWord(SymKind k) {
    switch (k) {
        case SymKind::Variable: return "var";
        case SymKind::Constant: return "const";
        case SymKind::Parameter: return "parámetro";
        case SymKind::Function: return "function";
        case SymKind::Class: return "class";
        case SymKind::Struct: return "struct";
        case SymKind::Interface: return "interface";
        case SymKind::Enum: return "enum";
        case SymKind::EnumMember: return "valor";
        case SymKind::Event: return "event";
        case SymKind::Field: return "campo";
        case SymKind::Method: return "método";
        case SymKind::Property: return "propiedad";
        case SymKind::Module: return "módulo";
        case SymKind::Library: return "biblioteca";
    }
    return "";
}

std::string symbolHeader(const Symbol* s) {
    if (!s->detail.empty()) return s->detail;
    std::string t = s->type && s->type->kind != TK::Any ? ": " + typeStr(s->type) : "";
    return std::string(kindWord(s->kind)) + " " + s->name + t;
}

// Miembro con nombre, tipo y símbolo (para completar y resolver cadenas a.b.c)
struct Member {
    std::string name;
    TypeP type;
    Symbol* sym = nullptr;
    int kind = 10;
    bool callable = false;
    bool hasParams = false;
};

class Server {
public:
    int run();
    // Un mensaje JSON-RPC (texto) → las respuestas y notificaciones que produce, como lista JSON (texto)
    std::string handleText(const std::string& text);

private:
    Transport io;
    std::map<std::string, Doc> docs; // uri -> documento
    bool shutdownRequested = false;

    void reply(const json& id, const json& result) { io.write({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}}); }
    void replyError(const json& id, int code, const std::string& msg) {
        io.write({{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", msg}}}});
    }
    void notify(const std::string& method, const json& params) { io.write({{"jsonrpc", "2.0"}, {"method", method}, {"params", params}}); }

    Doc* findByPath(const std::string& path) {
        std::string k = pathKey(path);
        for (auto& [u, d] : docs) if (pathKey(d.path) == k) return &d;
        return nullptr;
    }
    std::string uriFor(const std::string& path) {
        if (Doc* d = findByPath(path)) return d->uri;
        return pathToUri(path);
    }

    Analysis analyzeText(const std::string& path, const std::string& text) {
        Analysis a;
        a.vm = std::make_unique<VM>();
        setupVM(*a.vm);
        a.vm->projectRoot = pathDir(path);
        a.vm->readSource = [this](const std::string& p, std::string& out) {
            if (Doc* d = findByPath(p)) { out = d->text; return true; }
            return false;
        };
        a.unit = a.vm->compileSource(path, text, true, false);
        return a;
    }
    void ensure(Doc& d) {
        if (!d.dirty && d.an.unit) return;
        d.an = analyzeText(d.path, d.text);
        d.dirty = false;
        publish(d);
    }
    void publish(Doc& d);

    // Consultas
    std::vector<Symbol*> visibleSymbols(Analysis& a, size_t off);
    ClassInfo* enclosingClass(Analysis& a, size_t off);
    TypeP typeOfName(Analysis& a, const std::string& name, size_t off);
    std::vector<Member> membersOf(Analysis& a, const TypeP& t);
    TypeP memberType(Analysis& a, const TypeP& t, const std::string& name);
    TypeP resolveChain(Analysis& a, const std::string& text, size_t end, size_t off, std::string* lastName = nullptr);
    const RefRecord* refAt(Analysis& a, size_t off);
    const Symbol* origin(Analysis& a, const Symbol* s);
    std::set<const Symbol*> related(Analysis& a, const Symbol* s, bool sameNameOnly);
    std::string sourceOf(Analysis& a, const std::string& path);

    void onCompletion(const json& id, const json& p);
    void onHover(const json& id, const json& p);
    void onSignatureHelp(const json& id, const json& p);
    void onDefinition(const json& id, const json& p);
    void onReferences(const json& id, const json& p, bool highlight);
    void onRename(const json& id, const json& p);
    void onPrepareRename(const json& id, const json& p);
    void onDocumentSymbol(const json& id, const json& p);
    void onCodeAction(const json& id, const json& p);
    void onFormatting(const json& id, const json& p);
    void handle(const json& msg);
};

void Server::publish(Doc& d) {
    json diags = json::array();
    if (d.an.unit) {
        LineIndex li(d.text);
        for (auto& dg : d.an.unit->diags.items) {
            size_t a = dg.loc.line > 0 ? (size_t)dg.loc.offset : 0;
            size_t b = a + (size_t)std::max(dg.loc.length, 0);
            if (b == a && dg.loc.line > 0) { // al menos una palabra o un carácter
                b = a;
                while (b < d.text.size() && isIdentByte((unsigned char)d.text[b])) b++;
                if (b == a && b < d.text.size() && d.text[b] != '\n') b++;
            }
            json j = {{"range", li.range(a, b)}, {"source", "hyperez"},
                      {"severity", dg.severity == Severity::Error ? 1 : dg.severity == Severity::Warning ? 2 : dg.severity == Severity::Info ? 3 : 4},
                      {"message", dg.message + (dg.hint.empty() ? "" : "\n" + dg.hint)}};
            if (!dg.code.empty()) j["code"] = dg.code;
            json tags = json::array();
            for (auto& t : dg.tags) { if (t == "unnecessary") tags.push_back(1); else if (t == "deprecated") tags.push_back(2); }
            if (!tags.empty()) j["tags"] = tags;
            size_t q = dg.hint.find("¿quisiste decir '");
            if (q != std::string::npos) {
                size_t s = q + std::strlen("¿quisiste decir '");
                size_t e = dg.hint.find('\'', s);
                if (e != std::string::npos) j["data"] = {{"replacement", dg.hint.substr(s, e - s)}};
            }
            diags.push_back(j);
        }
    }
    notify("textDocument/publishDiagnostics", {{"uri", d.uri}, {"version", d.version}, {"diagnostics", diags}});
}

std::string Server::sourceOf(Analysis& a, const std::string& path) {
    for (auto& u : a.vm->units) if (u->path == path && u->program) return u->program->source;
    if (Doc* d = findByPath(path)) return d->text;
    std::string s;
    readFile(path, s);
    return s;
}

std::vector<Symbol*> Server::visibleSymbols(Analysis& a, size_t off) {
    std::vector<Symbol*> out;
    std::set<std::string> seen;
    auto add = [&](Symbol* s) {
        if (!s || s->name.empty() || s->name.rfind("__", 0) == 0) return;
        if (seen.insert(s->name).second) out.push_back(s);
    };
    // Locales: los ámbitos más internos primero
    std::vector<const ScopeRecord*> scopes;
    for (auto& sc : a.unit->scopes) if ((size_t)sc.start <= off && off <= (size_t)sc.end) scopes.push_back(&sc);
    std::sort(scopes.begin(), scopes.end(), [](auto* x, auto* y) { return x->start > y->start; });
    for (auto* sc : scopes)
        for (auto* s : sc->symbols) if ((size_t)s->scopeStart <= off) add(s);
    // Miembros de la clase que contiene el cursor
    if (ClassInfo* c = enclosingClass(a, off))
        for (auto* k = c; k; k = k->super)
            for (auto& [n, m] : k->members) add(m.sym);
    for (auto& [n, s] : a.unit->globalSyms) add(s);
    for (auto& [n, s] : worldOf(*a.vm).builtinSyms) add(s);
    return out;
}

ClassInfo* Server::enclosingClass(Analysis& a, size_t off) {
    if (!a.unit->program) return nullptr;
    for (auto& st : a.unit->program->body) {
        if (st->kind != StmtKind::Class) continue;
        auto* c = static_cast<ClassStmt*>(st.get());
        if ((size_t)c->loc.offset <= off && off <= (size_t)c->endLoc.offset) return c->info;
    }
    return nullptr;
}

TypeP Server::typeOfName(Analysis& a, const std::string& name, size_t off) {
    if (name == "this") {
        if (ClassInfo* c = enclosingClass(a, off)) return tInstance(c);
        return tAny();
    }
    for (auto* s : visibleSymbols(a, off)) {
        if (s->name != name) continue;
        if (s->type) return s->type;
        auto g = a.unit->globalTypes.find(name);
        if (g != a.unit->globalTypes.end() && g->second) return g->second;
        auto b = worldOf(*a.vm).builtinTypes.find(name);
        if (b != worldOf(*a.vm).builtinTypes.end()) return b->second;
        return tAny();
    }
    auto b = worldOf(*a.vm).builtinTypes.find(name);
    return b != worldOf(*a.vm).builtinTypes.end() ? b->second : tAny();
}

static Member memberFrom(const std::string& name, const MemberInfo& m) {
    Member r;
    r.name = name;
    r.type = m.type;
    r.sym = m.sym;
    switch (m.kind) {
        case MemberInfo::Field: r.kind = 5; break;
        case MemberInfo::Method: r.kind = 2; break;
        case MemberInfo::Getter: case MemberInfo::Setter: case MemberInfo::Property: r.kind = 10; break;
        case MemberInfo::Constant: r.kind = 21; break;
        case MemberInfo::EventM: r.kind = 23; break;
        case MemberInfo::Submodule: r.kind = 9; break;
    }
    if (m.kind == MemberInfo::Method && m.type && m.type->kind == TK::Func) {
        r.callable = true;
        r.hasParams = m.type->sig && !m.type->sig->params.empty();
    }
    return r;
}

std::vector<Member> Server::membersOf(Analysis& a, const TypeP& t) {
    std::vector<Member> out;
    if (!t) return out;
    World& w = worldOf(*a.vm);
    std::set<std::string> seen;
    auto fromClass = [&](ClassInfo* c, bool statics) {
        for (auto* k = c; k; k = k->super)
            for (auto& [n, m] : k->members)
                if (m.isStatic == statics && seen.insert(n).second) out.push_back(memberFrom(n, m));
    };
    auto native = [&](const char* n) { if (auto* c = w.findClass(n)) fromClass(c, false); };
    switch (t->kind) {
        case TK::Instance: if (t->cls) fromClass(t->cls, false); break;
        case TK::Class:
            if (t->cls) fromClass(t->cls, true);
            break;
        case TK::Module:
            if (t->mod) for (auto& [n, m] : t->mod->members) if (seen.insert(n).second) out.push_back(memberFrom(n, m));
            break;
        case TK::Enum:
            if (t->en) for (auto& n : t->en->members) {
                Member m;
                m.name = n;
                m.kind = 20;
                auto s = t->en->memberSyms.find(n);
                if (s != t->en->memberSyms.end()) m.sym = s->second;
                m.type = std::make_shared<Type>();
                m.type->kind = TK::EnumValue;
                m.type->en = t->en;
                out.push_back(m);
            }
            native("Enum");
            break;
        case TK::EnumValue:
            for (const char* n : {"name", "value", "ordinal"}) { Member m; m.name = n; m.kind = 10; m.type = std::string(n) == "name" ? tText() : tAny(); out.push_back(m); }
            break;
        case TK::Text: native("text"); break;
        case TK::List: native("list"); break;
        case TK::Map: native("map"); break;
        case TK::Set: native("set"); break;
        case TK::Number: case TK::Int: native("number"); break;
        case TK::Bool: native("bool"); break;
        case TK::Func: native("function"); break;
        case TK::Task: native("Task"); break;
        case TK::Range: native("Range"); break;
        case TK::Event: native("Event"); break;
        case TK::Subscription: native("Subscription"); break;
        default: break;
    }
    return out;
}

TypeP Server::memberType(Analysis& a, const TypeP& t, const std::string& name) {
    for (auto& m : membersOf(a, t)) {
        if (m.name != name) continue;
        TypeP r = m.type ? m.type : tAny();
        // T: el tipo del elemento de la colección
        if (r->placeholder && t && !t->args.empty()) return t->args[0];
        return r;
    }
    return tAny();
}

// Recorre hacia atrás una expresión del tipo  a.b(c)[0].d  que termina en 'end' y devuelve su tipo.
TypeP Server::resolveChain(Analysis& a, const std::string& text, size_t end, size_t off, std::string* lastName) {
    struct Seg { char kind; std::string name; }; // i=identificador, c=llamada, x=índice, s=texto, l=lista, n=número
    std::vector<Seg> segs;
    size_t q = end;
    auto skipBack = [&](char open, char close) -> bool {
        int depth = 0;
        while (q > 0) {
            char c = text[--q];
            if (c == close) depth++;
            else if (c == open) { depth--; if (depth == 0) return true; }
            else if (c == '\n' && depth > 8) return false;
        }
        return false;
    };
    for (int guard = 0; guard < 64 && q > 0; guard++) {
        char c = text[q - 1];
        if (c == ')') { if (!skipBack('(', ')')) return tAny(); segs.push_back({'c', ""}); continue; }
        if (c == ']') {
            if (!skipBack('[', ']')) return tAny();
            if (q > 0 && (isIdentByte((unsigned char)text[q - 1]) || text[q - 1] == ')' || text[q - 1] == ']')) { segs.push_back({'x', ""}); continue; }
            segs.push_back({'l', ""});
            break;
        }
        if (c == '"' || c == '\'') {
            size_t s = q - 1;
            while (s > 0 && text[s - 1] != c) s--;
            segs.push_back({'s', ""});
            break;
        }
        if (isIdentByte((unsigned char)c)) {
            size_t s = q;
            while (s > 0 && isIdentByte((unsigned char)text[s - 1])) s--;
            std::string word = text.substr(s, q - s);
            q = s;
            if (std::isdigit((unsigned char)word[0])) { segs.push_back({'n', ""}); break; }
            segs.push_back({'i', word});
            if (q >= 2 && text[q - 1] == '.' && text[q - 2] != '.') { q--; continue; }
            if (q >= 2 && text[q - 1] == '.' && text[q - 2] == '.') break;
            break;
        }
        return tAny();
    }
    if (segs.empty()) return tAny();
    std::reverse(segs.begin(), segs.end());
    TypeP t;
    switch (segs[0].kind) {
        case 'i': t = typeOfName(a, segs[0].name, off); break;
        case 's': t = tText(); break;
        case 'l': t = tList(); break;
        case 'n': t = tNum(); break;
        default: return tAny();
    }
    if (lastName && segs[0].kind == 'i') *lastName = segs[0].name;
    for (size_t i = 1; i < segs.size(); i++) {
        auto& s = segs[i];
        if (!t) return tAny();
        if (s.kind == 'i') { t = memberType(a, t, s.name); if (lastName) *lastName = s.name; }
        else if (s.kind == 'c') {
            if (t->kind == TK::Func) {
                if (t->sig && t->sig->ret) t = t->sig->isAsync ? tTask(t->sig->ret) : t->sig->ret;
                else t = tAny();
            } else if (t->kind == TK::Class && t->cls) t = tInstance(t->cls);
            else t = tAny();
        } else if (s.kind == 'x') {
            if (t->kind == TK::List && !t->args.empty()) t = t->args[0];
            else if (t->kind == TK::Map && t->args.size() > 1) t = t->args[1];
            else if (t->kind == TK::Text) t = tText();
            else t = tAny();
        }
    }
    return t ? t : tAny();
}

const RefRecord* Server::refAt(Analysis& a, size_t off) {
    const RefRecord* best = nullptr;
    for (auto& r : a.unit->refs) {
        size_t s = (size_t)r.loc.offset, e = s + (size_t)std::max(r.loc.length, (int)r.sym->name.size());
        if (s <= off && off <= e && (!best || r.loc.offset >= best->loc.offset)) best = &r;
    }
    return best;
}

// Un nombre traído con 'from X import nombre' es un símbolo propio del archivo que importa;
// en su posición hay además una referencia al símbolo original del otro módulo.
const Symbol* Server::origin(Analysis& a, const Symbol* s) {
    for (auto& u : a.vm->units) {
        if (!u->program) continue;
        for (auto& st : u->program->body) {
            if (st->kind != StmtKind::Import) continue;
            auto* im = static_cast<ImportStmt*>(st.get());
            for (size_t i = 0; i < im->nameSyms.size() && i < im->names.size(); i++) {
                if (im->nameSyms[i] != s) continue;
                for (auto& r : u->refs)
                    if (r.loc.offset == im->names[i].second.offset && r.sym != s) return r.sym;
            }
        }
    }
    return s;
}

// El símbolo original y todos sus alias de importación (para referencias y renombrar).
std::set<const Symbol*> Server::related(Analysis& a, const Symbol* s, bool sameNameOnly) {
    const Symbol* o = origin(a, s);
    std::set<const Symbol*> out{o};
    for (auto& u : a.vm->units) {
        if (!u->program) continue;
        for (auto& st : u->program->body) {
            if (st->kind != StmtKind::Import) continue;
            auto* im = static_cast<ImportStmt*>(st.get());
            for (size_t i = 0; i < im->nameSyms.size() && i < im->names.size(); i++) {
                if (!im->nameSyms[i] || (sameNameOnly && im->nameSyms[i]->name != o->name)) continue;
                for (auto& r : u->refs)
                    if (r.loc.offset == im->names[i].second.offset && r.sym == o) out.insert(im->nameSyms[i]);
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Peticiones
// ---------------------------------------------------------------------------

static json markdown(const std::string& code, const std::string& doc) {
    std::string v = "```hyperez\n" + code + "\n```";
    if (!doc.empty()) v += "\n\n" + doc;
    return {{"kind", "markdown"}, {"value", v}};
}

void Server::onCompletion(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, json::array()); return; }
    Doc& d = it->second;
    LineIndex li(d.text);
    size_t off = li.offset(p["position"]["line"], p["position"]["character"]);
    size_t wordStart = off;
    while (wordStart > 0 && isIdentByte((unsigned char)d.text[wordStart - 1])) wordStart--;
    // No completar dentro de comentarios de línea
    size_t ls = d.text.rfind('\n', off ? off - 1 : 0);
    ls = ls == std::string::npos ? 0 : ls + 1;
    std::string linePrefix = d.text.substr(ls, off - ls);
    if (linePrefix.find("//") != std::string::npos) { reply(id, json::array()); return; }

    bool member = wordStart > 0 && d.text[wordStart - 1] == '.' && !(wordStart > 1 && d.text[wordStart - 2] == '.');
    json items = json::array();
    if (member) {
        // Se analiza una copia con un nombre provisional tras el punto para que el resto del bloque se lea bien.
        std::string patched = d.text;
        if (wordStart == off) patched.insert(off, "__hz_completar");
        Analysis a = analyzeText(d.path, patched);
        TypeP t = resolveChain(a, patched, wordStart - 1, off);
        for (auto& m : membersOf(a, t)) {
            if (m.name.rfind("__", 0) == 0) continue;
            json item = {{"label", m.name}, {"kind", m.kind}};
            if (m.sym) {
                item["detail"] = symbolHeader(m.sym);
                if (!m.sym->doc.empty()) item["documentation"] = {{"kind", "markdown"}, {"value", m.sym->doc}};
            } else if (m.type) item["detail"] = typeStr(m.type);
            if (m.callable) { item["insertText"] = m.name + (m.hasParams ? "($1)$0" : "()$0"); item["insertTextFormat"] = 2; }
            items.push_back(item);
        }
        reply(id, {{"isIncomplete", false}, {"items", items}});
        return;
    }
    ensure(d);
    for (auto* s : visibleSymbols(d.an, off)) {
        json item = {{"label", s->name}, {"kind", completionKind(s->kind)}, {"detail", symbolHeader(s)}};
        if (!s->doc.empty()) item["documentation"] = {{"kind", "markdown"}, {"value", s->doc}};
        if (s->builtin) item["sortText"] = "2" + s->name;
        else item["sortText"] = "1" + s->name;
        if ((s->kind == SymKind::Function || s->kind == SymKind::Method) && s->type && s->type->kind == TK::Func) {
            bool params = s->type->sig && !s->type->sig->params.empty();
            item["insertText"] = s->name + (params ? "($1)$0" : "()$0");
            item["insertTextFormat"] = 2;
        }
        items.push_back(item);
    }
    for (const char* k : kKeywords) items.push_back({{"label", k}, {"kind", 14}, {"sortText", std::string("3") + k}});
    reply(id, {{"isIncomplete", false}, {"items", items}});
}

void Server::onHover(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, nullptr); return; }
    Doc& d = it->second;
    ensure(d);
    LineIndex li(d.text);
    size_t off = li.offset(p["position"]["line"], p["position"]["character"]);
    const RefRecord* r = refAt(d.an, off);
    if (!r) { reply(id, nullptr); return; }
    const Symbol* s = r->sym;
    std::string head = symbolHeader(s);
    if (!s->container.empty() && head.find(s->container + ".") == std::string::npos && s->kind != SymKind::Parameter &&
        s->kind != SymKind::Variable)
        head = "(" + s->container + ") " + head;
    size_t a = (size_t)r->loc.offset;
    reply(id, {{"contents", markdown(head, s->doc)}, {"range", li.range(a, a + (size_t)std::max(r->loc.length, 1))}});
}

void Server::onSignatureHelp(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, nullptr); return; }
    Doc& d = it->second;
    LineIndex li(d.text);
    size_t off = li.offset(p["position"]["line"], p["position"]["character"]);
    // Paréntesis abierto que contiene el cursor y número de comas a su nivel
    int depth = 0, commas = 0;
    size_t q = off;
    bool found = false;
    bool inStr = false;
    while (q > 0) {
        char c = d.text[--q];
        if (c == '"') { inStr = !inStr; continue; }
        if (inStr) continue;
        if (c == ')' || c == ']' || c == '}') depth++;
        else if (c == '(' || c == '[' || c == '{') {
            if (depth == 0) { if (c == '(') found = true; break; }
            depth--;
        } else if (c == ',' && depth == 0) commas++;
        else if (c == '\n' && off - q > 2000) break;
    }
    if (!found || q == 0) { reply(id, nullptr); return; }
    ensure(d);
    std::string name;
    TypeP t = resolveChain(d.an, d.text, q, off, &name);
    std::shared_ptr<FuncSig> sig;
    std::string doc;
    if (t->kind == TK::Func) sig = t->sig;
    else if (t->kind == TK::Class && t->cls) {
        sig = t->cls->findCtor();
        if (!sig && t->cls->isStruct) {
            sig = std::make_shared<FuncSig>();
            for (auto& [n, m] : t->cls->members)
                if (m.kind == MemberInfo::Field && !m.isStatic) sig->params.push_back({n, m.type, true, false, ""});
        }
        if (t->cls->sym) doc = t->cls->sym->doc;
    }
    if (!sig) { reply(id, nullptr); return; }
    for (auto* s : visibleSymbols(d.an, off)) if (s->name == name && doc.empty()) doc = s->doc;
    if (doc.empty()) {
        // método: buscar el símbolo por nombre entre los miembros conocidos
        for (auto& sym : d.an.unit->symbols) if (sym->name == name && !sym->doc.empty()) { doc = sym->doc; break; }
    }
    std::string label = name + "(";
    json params = json::array();
    for (size_t i = 0; i < sig->params.size(); i++) {
        auto& sp = sig->params[i];
        if (i) label += ", ";
        std::string part = (sp.rest ? "..." : "") + sp.name + (sp.type && sp.type->kind != TK::Any ? ": " + typeStr(sp.type) : "") +
                           (sp.optional && !sp.rest ? (sp.def.empty() ? "?" : " = " + sp.def) : "");
        // offsets en UTF-16
        auto u16len = [](const std::string& s) { size_t n = 0; for (size_t i = 0; i < s.size();) { unsigned char c = (unsigned char)s[i]; size_t l = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4; n += l == 4 ? 2 : 1; i += l; } return n; };
        size_t start = u16len(label);
        label += part;
        params.push_back({{"label", {start, u16len(label)}}});
    }
    label += ")";
    if (sig->ret && sig->ret->kind != TK::Any) label += ": " + typeStr(sig->isAsync ? tTask(sig->ret) : sig->ret);
    int active = commas;
    if (!sig->params.empty() && sig->params.back().rest) active = std::min(active, (int)sig->params.size() - 1);
    json sigj = {{"label", label}, {"parameters", params}};
    if (!doc.empty()) sigj["documentation"] = {{"kind", "markdown"}, {"value", doc}};
    reply(id, {{"signatures", json::array({sigj})}, {"activeSignature", 0}, {"activeParameter", active}});
}

void Server::onDefinition(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, nullptr); return; }
    Doc& d = it->second;
    ensure(d);
    LineIndex li(d.text);
    size_t off = li.offset(p["position"]["line"], p["position"]["character"]);
    const RefRecord* r = refAt(d.an, off);
    if (!r) { reply(id, nullptr); return; }
    const Symbol* s = origin(d.an, r->sym);
    if (s->builtin || s->file.empty() || s->file[0] == '<' || s->loc.line <= 0) { reply(id, nullptr); return; }
    std::string src = sourceOf(d.an, s->file);
    LineIndex si(src);
    size_t a = (size_t)s->loc.offset;
    reply(id, {{"uri", uriFor(s->file)}, {"range", si.range(a, a + (size_t)std::max(s->loc.length, (int)s->name.size()))}});
}

void Server::onReferences(const json& id, const json& p, bool highlight) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, json::array()); return; }
    Doc& d = it->second;
    ensure(d);
    LineIndex li(d.text);
    size_t off = li.offset(p["position"]["line"], p["position"]["character"]);
    const RefRecord* r = refAt(d.an, off);
    json out = json::array();
    if (!r) { reply(id, out); return; }
    bool includeDecl = !p.contains("context") || p["context"].value("includeDeclaration", true);
    auto syms = related(d.an, r->sym, false);
    for (auto& u : d.an.vm->units) {
        if (u.get() == d.an.vm->preludeUnit) continue;
        if (highlight && u.get() != d.an.unit) continue;
        std::string src = u->program ? u->program->source : "";
        LineIndex ui(src);
        std::set<int> seen;
        for (auto& ref : u->refs) {
            if (!syms.count(ref.sym) || (!includeDecl && ref.isDecl) || !seen.insert(ref.loc.offset).second) continue;
            size_t a = (size_t)ref.loc.offset;
            json range = ui.range(a, a + (size_t)std::max(ref.loc.length, (int)ref.sym->name.size()));
            if (highlight) out.push_back({{"range", range}, {"kind", ref.isDecl ? 3 : 2}});
            else out.push_back({{"uri", uriFor(u->path)}, {"range", range}});
        }
    }
    reply(id, out);
}

void Server::onPrepareRename(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, nullptr); return; }
    Doc& d = it->second;
    ensure(d);
    LineIndex li(d.text);
    size_t off = li.offset(p["position"]["line"], p["position"]["character"]);
    const RefRecord* r = refAt(d.an, off);
    if (!r || r->sym->builtin) { replyError(id, -32602, "Solo se pueden renombrar símbolos de tu código"); return; }
    size_t a = (size_t)r->loc.offset;
    reply(id, {{"range", li.range(a, a + r->sym->name.size())}, {"placeholder", r->sym->name}});
}

void Server::onRename(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, nullptr); return; }
    Doc& d = it->second;
    ensure(d);
    std::string newName = p["newName"];
    bool valid = !newName.empty() && !std::isdigit((unsigned char)newName[0]);
    for (unsigned char c : newName) if (!isIdentByte(c)) valid = false;
    if (!valid) { replyError(id, -32602, "'" + newName + "' no es un nombre válido"); return; }
    LineIndex li(d.text);
    size_t off = li.offset(p["position"]["line"], p["position"]["character"]);
    const RefRecord* r = refAt(d.an, off);
    if (!r || r->sym->builtin || origin(d.an, r->sym)->builtin) { replyError(id, -32602, "Solo se pueden renombrar símbolos de tu código"); return; }
    auto syms = related(d.an, r->sym, true);
    json changes = json::object();
    for (auto& u : d.an.vm->units) {
        if (u.get() == d.an.vm->preludeUnit) continue;
        std::string src = u->program ? u->program->source : "";
        LineIndex ui(src);
        json edits = json::array();
        std::set<int> seen;
        for (auto& ref : u->refs) {
            if (!syms.count(ref.sym) || !seen.insert(ref.loc.offset).second) continue;
            size_t a = (size_t)ref.loc.offset;
            // la referencia debe contener exactamente el nombre (evita tocar 'this.x' por error)
            if (src.compare(a, ref.sym->name.size(), ref.sym->name) != 0) continue;
            edits.push_back({{"range", ui.range(a, a + ref.sym->name.size())}, {"newText", newName}});
        }
        if (!edits.empty()) changes[uriFor(u->path)] = edits;
    }
    reply(id, {{"changes", changes}});
}

void Server::onDocumentSymbol(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, json::array()); return; }
    Doc& d = it->second;
    ensure(d);
    LineIndex li(d.text);
    json out = json::array();
    std::vector<Symbol*> tops;
    std::set<int> importLines; // los nombres importados no se muestran en el esquema
    if (d.an.unit->program)
        for (auto& st : d.an.unit->program->body) if (st->kind == StmtKind::Import) importLines.insert(st->loc.line);
    for (auto& [n, s] : d.an.unit->globalSyms)
        if (s && s->file == d.an.unit->path && s->loc.line > 0 && s->kind != SymKind::Module && s->kind != SymKind::Library &&
            !importLines.count(s->loc.line) && n.rfind("__", 0) != 0) // __d1...: variables internas de la desestructuración
            tops.push_back(s);
    std::sort(tops.begin(), tops.end(), [](Symbol* a, Symbol* b) { return a->loc.offset < b->loc.offset; });
    for (auto* s : tops) {
        size_t a = (size_t)s->loc.offset;
        json range = li.range(a, a + std::max<size_t>(s->name.size(), (size_t)s->loc.length));
        json item = {{"name", s->name}, {"kind", symbolKind(s->kind)}, {"range", range}, {"selectionRange", range}};
        if (!s->detail.empty()) item["detail"] = s->detail;
        if (s->kind == SymKind::Class || s->kind == SymKind::Struct || s->kind == SymKind::Interface || s->kind == SymKind::Enum) {
            json children = json::array();
            std::vector<Symbol*> members;
            for (auto& m : d.an.unit->symbols)
                if (m->container == s->name && m->loc.line > 0 && m->file == d.an.unit->path && m.get() != s) members.push_back(m.get());
            std::sort(members.begin(), members.end(), [](Symbol* a, Symbol* b) { return a->loc.offset < b->loc.offset; });
            for (auto* m : members) {
                size_t b = (size_t)m->loc.offset;
                json r2 = li.range(b, b + std::max<size_t>(m->name.size(), (size_t)m->loc.length));
                json c = {{"name", m->name}, {"kind", symbolKind(m->kind)}, {"range", r2}, {"selectionRange", r2}};
                if (!m->detail.empty()) c["detail"] = m->detail;
                children.push_back(c);
            }
            item["children"] = children;
        }
        out.push_back(item);
    }
    reply(id, out);
}

// Dar formato al documento (hyper fmt): un solo cambio con el texto completo
void Server::onFormatting(const json& id, const json& p) {
    auto it = docs.find(p["textDocument"]["uri"].get<std::string>());
    if (it == docs.end()) { reply(id, json::array()); return; }
    FormatResult r = formatSource(it->second.text);
    if (!r.ok || !r.changed) { reply(id, json::array()); return; }
    LineIndex li(it->second.text);
    reply(id, json::array({{{"range", li.range(0, it->second.text.size())}, {"newText", r.text}}}));
}

void Server::onCodeAction(const json& id, const json& p) {
    json out = json::array();
    std::string uri = p["textDocument"]["uri"];
    if (p.contains("context") && p["context"].contains("diagnostics")) {
        for (auto& dg : p["context"]["diagnostics"]) {
            if (!dg.contains("data") || !dg["data"].contains("replacement")) continue;
            std::string rep = dg["data"]["replacement"];
            json edit = {{"changes", {{uri, json::array({{{"range", dg["range"]}, {"newText", rep}}})}}}};
            out.push_back({{"title", "Cambiar por '" + rep + "'"}, {"kind", "quickfix"}, {"isPreferred", true},
                           {"diagnostics", json::array({dg})}, {"edit", edit}});
        }
    }
    reply(id, out);
}

void Server::handle(const json& msg) {
    if (!msg.is_object()) return;
    std::string method = msg.value("method", "");
    json id = msg.contains("id") ? msg["id"] : json();
    const json& p = msg.contains("params") ? msg["params"] : json::object();
    try {
        if (method == "initialize") {
            json caps = {
                {"textDocumentSync", {{"openClose", true}, {"change", 1}, {"save", {{"includeText", false}}}}},
                {"completionProvider", {{"triggerCharacters", json::array({"."})}, {"resolveProvider", false}}},
                {"hoverProvider", true},
                {"signatureHelpProvider", {{"triggerCharacters", json::array({"(", ","})}, {"retriggerCharacters", json::array({","})}}},
                {"definitionProvider", true},
                {"referencesProvider", true},
                {"documentHighlightProvider", true},
                {"documentSymbolProvider", true},
                {"renameProvider", {{"prepareProvider", true}}},
                {"codeActionProvider", {{"codeActionKinds", json::array({"quickfix"})}}},
                {"documentFormattingProvider", true},
            };
            reply(id, {{"capabilities", caps}, {"serverInfo", {{"name", "hyperez-lsp"}, {"version", HYPEREZ_VERSION}}}});
        } else if (method == "initialized" || method == "$/cancelRequest" || method == "$/setTrace" || method == "workspace/didChangeConfiguration") {
        } else if (method == "shutdown") {
            shutdownRequested = true;
            reply(id, nullptr);
        } else if (method == "textDocument/didOpen") {
            auto& td = p["textDocument"];
            Doc& d = docs[td["uri"].get<std::string>()];
            d.uri = td["uri"];
            d.path = uriToPath(d.uri);
            d.text = td["text"];
            d.version = td.value("version", 0);
            for (auto& [u, o] : docs) o.dirty = true; // los que importan este archivo ya lo encuentran
        } else if (method == "textDocument/didChange") {
            auto itd = docs.find(p["textDocument"]["uri"].get<std::string>());
            if (itd != docs.end() && !p["contentChanges"].empty()) {
                itd->second.text = p["contentChanges"].back()["text"];
                itd->second.version = p["textDocument"].value("version", 0);
                itd->second.dirty = true;
            }
        } else if (method == "textDocument/didSave") {
            for (auto& [u, d] : docs) d.dirty = true; // un import pudo cambiar
        } else if (method == "textDocument/didClose") {
            std::string uri = p["textDocument"]["uri"];
            docs.erase(uri);
            notify("textDocument/publishDiagnostics", {{"uri", uri}, {"diagnostics", json::array()}});
        } else if (method == "textDocument/completion") onCompletion(id, p);
        else if (method == "textDocument/hover") onHover(id, p);
        else if (method == "textDocument/signatureHelp") onSignatureHelp(id, p);
        else if (method == "textDocument/definition") onDefinition(id, p);
        else if (method == "textDocument/references") onReferences(id, p, false);
        else if (method == "textDocument/documentHighlight") onReferences(id, p, true);
        else if (method == "textDocument/prepareRename") onPrepareRename(id, p);
        else if (method == "textDocument/rename") onRename(id, p);
        else if (method == "textDocument/documentSymbol") onDocumentSymbol(id, p);
        else if (method == "textDocument/codeAction") onCodeAction(id, p);
        else if (method == "textDocument/formatting") onFormatting(id, p);
        else if (!id.is_null()) replyError(id, -32601, "Método no soportado: " + method);
    } catch (std::exception& e) {
        if (!id.is_null()) replyError(id, -32603, std::string("Error interno del servidor de HyperEZ: ") + e.what());
    }
}

std::string Server::handleText(const std::string& text) {
    std::vector<std::string> out;
    io.outbox = &out;
    json msg = json::parse(text, nullptr, false);
    if (msg.is_object()) {
        handle(msg);
        // en memoria (el editor web) no hay "guardar": un cambio puede afectar a los que importan ese archivo
        if (msg.value("method", "") == "textDocument/didChange" && docs.size() > 1)
            for (auto& [u, d] : docs) d.dirty = true;
    }
    for (auto& [u, d] : docs) if (d.dirty) ensure(d); // los diagnósticos, al momento
    io.outbox = nullptr;
    std::string r = "[";
    for (size_t i = 0; i < out.size(); i++) r += (i ? "," : "") + out[i];
    return r + "]";
}

int Server::run() {
    for (;;) {
        // Analizar los documentos pendientes cuando no llegan más mensajes
        if (!io.pending())
            for (auto& [u, d] : docs) if (d.dirty) ensure(d);
        json msg;
        if (!io.read(msg)) return shutdownRequested ? 0 : 1;
        if (msg.is_object() && msg.value("method", "") == "exit") return shutdownRequested ? 0 : 1;
        handle(msg);
    }
}

} // namespace

int runLanguageServer() {
    Server s;
    return s.run();
}

std::string languageServerMessage(const std::string& message) {
    static Server* s = new Server(); // vive mientras dure la página
    return s->handleText(message);
}

} // namespace hz
