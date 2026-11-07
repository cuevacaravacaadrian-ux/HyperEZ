// HyperEZ nativo: compila el subconjunto "de sistema" de HyperEZ a código máquina x86-64, sin máquina virtual ni
// sistema operativo debajo. Es lo que hace falta para escribir un kernel en HyperEZ (HyperEZX está hecho así).
//
//   hyper build kernel.hez -o BOOTX64.EFI            aplicación UEFI (PE32+), arranca desde un USB o una ISO
//   hyper build kernel.hez -o kernel.bin --formato bin   binario plano independiente de la posición
//
// El subconjunto (docs/lenguaje/12-nativo.md):
//   - todos los valores son enteros de 64 bits (las direcciones también); true = 1, false = 0, null = 0
//   - funciones, variables y constantes globales y locales, if/else, while, for i in a..b [step s], repeat, break,
//     continue, return, do/while, ternario, and/or/not y todos los operadores enteros y de bits
//   - struct con campos de tamaño fijo (u8, i8, u16, i16, u32, i32, int/u64/i64, punteros a otros struct) y métodos;
//     una variable de tipo struct guarda la dirección del struct: Ventana(dir) la convierte, sizeof(Ventana) su tamaño
//   - enum: constantes enteras (Color.Rojo)
//   - variables globales: var x = 5 (un entero), var buf = buffer(4096) (memoria a cero), var t = [1, 2, 3] (tabla de
//     enteros de 64 bits), var s = "texto" (texto UTF-8 terminado en 0), u16("texto") (UTF-16, para UEFI),
//     embed("archivo") (los bytes de un archivo) y embedSize("archivo")
//   - funciones del procesador: peek8/16/32/64, peeks8/16/32, poke8/16/32/64, inb/inw/inl, outb/outw/outl, cli, sti,
//     hlt, pause, rdtsc, cpuid, copy, copy32, fill8, fill32, udiv, umod, ult, callptr (llamadas a punteros a función,
//     con la convención de Microsoft x64 que usa UEFI)
//   - import "./otro.hez": une varios archivos en un mismo programa
// La función main(imagen, sistema) es el punto de entrada (UEFI la llama con su ImageHandle y su SystemTable).
#include "tools/tools.h"
#include "lang/syntax/parser.h"
#include "lang/sema/diagnostics.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace hz {
namespace {

struct CompileError { std::string file; SrcLoc loc; std::string msg; };

struct Field { std::string name; int offset = 0; int size = 8; bool sign = true; std::string type; };
struct StructInfo {
    std::string name, file;
    int size = 0, align = 1;
    std::vector<Field> fields;
    std::map<std::string, FuncDecl*> methods;
    const Field* field(const std::string& n) const { for (auto& f : fields) if (f.name == n) return &f; return nullptr; }
};
struct FuncInfo { FuncDecl* decl = nullptr; std::string owner, file; int label = -1; };
struct GlobalInfo { bool address = false; bool bss = false; int off = 0; std::string type; };
struct LocalInfo2 { int off; std::string type; };
enum class Target { Code, Data, Bss };
struct Fix { int at; Target kind; int value; };

struct SourceFile { std::string path, text; std::vector<StmtP> body; };

class NativeCompiler {
public:
    std::vector<std::unique_ptr<SourceFile>> files;
    std::map<std::string, StructInfo> structs;
    std::map<std::string, FuncInfo> funcs;                  // "nombre" o "Struct.metodo"
    std::map<std::string, GlobalInfo> globals;
    std::map<std::string, int64_t> consts;                  // constantes y enum ("Color.Rojo")
    std::set<std::string> enums;
    std::vector<uint8_t> code, data;
    int bssSize = 0;
    std::vector<int> labels;
    std::vector<Fix> fixes;
    std::map<std::string, int> strings, wides;              // literales ya guardados
    std::map<std::string, std::pair<int, int>> embeds;      // ruta → (desplazamiento, tamaño)

    // estado de la función que se compila
    std::string curFile, curOwner;
    std::vector<std::map<std::string, LocalInfo2>> scopes;
    int slots = 0, depth = 0, retLabel = -1;
    std::vector<int> breakLabels, continueLabels;

    [[noreturn]] void fail(SrcLoc loc, const std::string& msg) { throw CompileError{curFile, loc, msg}; }

    // ------------------------------------------------------------------------------------------------ carga
    void load(const std::string& path, SrcLoc from = {}) {
        std::string abs = pathAbsolute(path);
        for (auto& f : files) if (f->path == abs) return;
        auto sf = std::make_unique<SourceFile>();
        sf->path = abs;
        if (!readFile(abs, sf->text)) throw CompileError{curFile, from, "No se puede leer '" + path + "'"};
        DiagnosticBag bag;
        bag.file = abs;
        Lexer lx(sf->text, bag);
        auto toks = lx.tokenize();
        Parser ps(std::move(toks), bag, lx.comments);
        sf->body = ps.parseProgram();
        for (auto& d : bag.items)
            if (d.severity == Severity::Error) throw CompileError{abs, d.loc, d.message};
        SourceFile* raw = sf.get();
        files.push_back(std::move(sf));
        std::string saved = curFile;
        curFile = abs;
        for (auto& s : raw->body)
            if (s->kind == StmtKind::Import) {
                auto* im = static_cast<ImportStmt*>(s.get());
                std::string p = im->path;
                if (!im->isFile) p = "./" + p;
                if (p.size() < 4 || (p.substr(p.size() - 4) != ".hez" && p.substr(p.size() - 3) != ".hz")) p += ".hez";
                load(pathJoin(pathDir(abs), p), im->loc);
            }
        curFile = saved;
    }

    // ------------------------------------------------------------------------------------------------ declaraciones
    static int typeSize(const std::string& t) {
        if (t == "u8" || t == "i8" || t == "byte") return 1;
        if (t == "u16" || t == "i16" || t == "char16") return 2;
        if (t == "u32" || t == "i32") return 4;
        return 8;
    }
    static bool typeSigned(const std::string& t) { return !(t == "u8" || t == "byte" || t == "u16" || t == "char16" || t == "u32"); }
    std::string structType(const TypeRef* t) { return t && structs.count(t->name) ? t->name : ""; }

    void declare() {
        // 1. nombres de struct y enum (los campos pueden apuntar a otros struct)
        for (auto& f : files) {
            curFile = f->path;
            for (auto& s : f->body) {
                if (s->kind == StmtKind::Class) {
                    auto* c = static_cast<ClassStmt*>(s.get());
                    if (structs.count(c->name)) fail(c->nameLoc, "'" + c->name + "' ya está declarado");
                    structs[c->name].name = c->name;
                    structs[c->name].file = f->path;
                } else if (s->kind == StmtKind::Enum) {
                    auto* e = static_cast<EnumStmt*>(s.get());
                    enums.insert(e->name);
                    int64_t v = 0;
                    for (auto& m : e->members) {
                        if (m.value && !fold(m.value.get(), v)) fail(m.loc, "El valor de un enum tiene que ser constante");
                        consts[e->name + "." + m.name] = v++;
                    }
                }
            }
        }
        // 2. campos, métodos, funciones, constantes y variables globales
        for (auto& f : files) {
            curFile = f->path;
            for (auto& s : f->body) {
                switch (s->kind) {
                    case StmtKind::Class: {
                        auto* c = static_cast<ClassStmt*>(s.get());
                        if (!c->superName.empty()) fail(c->superLoc, "En HyperEZ nativo los struct no heredan");
                        if (c->ctor) fail(c->ctor->loc, "En HyperEZ nativo un struct no tiene constructor: reserva la memoria y rellena sus campos");
                        StructInfo& si = structs[c->name];
                        int off = 0;
                        for (auto& fd : c->fields) {
                            if (fd.isStatic) fail(fd.loc, "Campos estáticos: usa una variable global");
                            std::string t = fd.type ? fd.type->name : "int";
                            int sz = structs.count(t) ? 8 : typeSize(t);
                            off = (off + sz - 1) / sz * sz;
                            si.fields.push_back({fd.name, off, sz, structs.count(t) ? true : typeSigned(t), structs.count(t) ? t : ""});
                            off += sz;
                            si.align = std::max(si.align, sz);
                        }
                        si.size = (off + si.align - 1) / si.align * si.align;
                        for (auto& m : c->methods) {
                            if (m->kind != FuncKind::Method && m->kind != FuncKind::Getter) fail(m->loc, "Solo se admiten métodos y get en un struct nativo");
                            si.methods[m->name] = m.get();
                            funcs[c->name + "." + m->name] = {m.get(), c->name, f->path};
                        }
                        break;
                    }
                    case StmtKind::Function: {
                        auto* fs = static_cast<FuncStmt*>(s.get());
                        if (funcs.count(fs->fn->name)) fail(fs->fn->nameLoc, "La función '" + fs->fn->name + "' ya existe");
                        funcs[fs->fn->name] = {fs->fn.get(), "", f->path};
                        break;
                    }
                    case StmtKind::Var: declareGlobal(static_cast<VarStmt*>(s.get())); break;
                    case StmtKind::Enum: case StmtKind::Import: break;
                    default: fail(s->loc, "En un programa nativo, las órdenes van dentro de funciones (el programa empieza en main)");
                }
            }
        }
        if (!funcs.count("main")) throw CompileError{files[0]->path, {}, "Falta la función main(imagen, sistema): es donde empieza el programa"};
    }

    void declareGlobal(VarStmt* v) {
        if (globals.count(v->name) || consts.count(v->name)) fail(v->nameLoc, "'" + v->name + "' ya está declarado");
        int64_t k;
        if (v->declKind == DeclKind::Const && v->init && fold(v->init.get(), k)) { consts[v->name] = k; return; }
        GlobalInfo g;
        g.type = structType(v->typeAnn.get());
        Expr* init = v->init.get();
        if (init && init->kind == ExprKind::Call) {
            auto* c = static_cast<CallExpr*>(init);
            std::string fn = c->callee->kind == ExprKind::Ident ? static_cast<IdentExpr*>(c->callee.get())->name : "";
            if (fn == "buffer") {
                if (c->args.size() != 1 || !fold(c->args[0].value.get(), k) || k <= 0) fail(c->loc, "buffer(n) necesita un tamaño constante");
                bssSize = (bssSize + 15) & ~15;
                g.address = true; g.bss = true; g.off = bssSize; bssSize += (int)k;
                globals[v->name] = g;
                return;
            }
            if (fn == "embed" || fn == "u16") {
                g.address = true; g.off = constantData(c);
                globals[v->name] = g;
                return;
            }
        }
        if (init && init->kind == ExprKind::String) {
            g.address = true; g.off = stringData(static_cast<StringExpr*>(init));
            globals[v->name] = g;
            return;
        }
        if (init && init->kind == ExprKind::List) {
            auto* l = static_cast<ListExpr*>(init);
            align(8);
            g.address = true; g.off = (int)data.size();
            for (auto& it : l->items) {
                if (!fold(it.get(), k)) fail(it->loc, "Una tabla global solo puede tener valores constantes");
                emit64(data, k);
            }
            globals[v->name] = g;
            return;
        }
        align(8);
        g.off = (int)data.size();
        k = 0;
        if (init && !fold(init, k)) fail(init->loc, "Una variable global empieza con un valor constante: dale su valor dentro de main");
        emit64(data, k);
        globals[v->name] = g;
    }

    void align(int n) { while (data.size() % n) data.push_back(0); }
    static void emit64(std::vector<uint8_t>& v, int64_t x) { for (int i = 0; i < 8; i++) v.push_back((uint8_t)(x >> (8 * i))); }

    int stringData(StringExpr* s) {
        if (s->parts.empty()) { int off = (int)data.size(); data.push_back(0); return off; }
        if (!s->isPlain()) fail(s->loc, "En HyperEZ nativo los textos no admiten ${...}");
        auto it = strings.find(s->parts[0].text);
        if (it != strings.end()) return it->second;
        int off = (int)data.size();
        data.insert(data.end(), s->parts[0].text.begin(), s->parts[0].text.end());
        data.push_back(0);
        strings[s->parts[0].text] = off;
        return off;
    }
    // u16("...") y embed("...")
    int constantData(CallExpr* c) {
        std::string fn = static_cast<IdentExpr*>(c->callee.get())->name;
        if (c->args.size() != 1 || c->args[0].value->kind != ExprKind::String || !static_cast<StringExpr*>(c->args[0].value.get())->isPlain())
            fail(c->loc, fn + "(...) necesita un texto entre comillas");
        std::string text = static_cast<StringExpr*>(c->args[0].value.get())->parts[0].text;
        if (fn == "u16") {
            auto it = wides.find(text);
            if (it != wides.end()) return it->second;
            align(2);
            int off = (int)data.size();
            for (size_t i = 0; i < text.size();) { // UTF-8 → UTF-16 (BMP)
                unsigned char ch = (unsigned char)text[i];
                uint32_t cp; int n;
                if (ch < 0x80) { cp = ch; n = 1; } else if ((ch >> 5) == 6) { cp = ch & 31; n = 2; } else if ((ch >> 4) == 14) { cp = ch & 15; n = 3; } else { cp = ch & 7; n = 4; }
                for (int k = 1; k < n && i + k < text.size(); k++) cp = (cp << 6) | ((unsigned char)text[i + k] & 63);
                i += n;
                if (cp > 0xFFFF) cp = '?';
                data.push_back((uint8_t)cp); data.push_back((uint8_t)(cp >> 8));
            }
            data.push_back(0); data.push_back(0);
            wides[text] = off;
            return off;
        }
        return embedFile(text, c->loc).first;
    }
    std::pair<int, int> embedFile(const std::string& rel, SrcLoc loc) {
        std::string path = pathAbsolute(pathJoin(pathDir(curFile), rel));
        auto it = embeds.find(path);
        if (it != embeds.end()) return it->second;
        std::string bytes;
        if (!readFile(path, bytes)) fail(loc, "No se puede leer '" + rel + "' para incrustarlo");
        align(16);
        int off = (int)data.size();
        data.insert(data.end(), bytes.begin(), bytes.end());
        data.push_back(0);
        return embeds[path] = {off, (int)bytes.size()};
    }

    // ------------------------------------------------------------------------------------------------ constantes
    int64_t literal(NumberExpr* n) {
        if (!n->isInt) fail(n->loc, "HyperEZ nativo solo trabaja con números enteros");
        // el valor exacto sale del texto (un double pierde precisión por encima de 2^53)
        std::string t;
        for (auto& f : files) if (f->path == curFile) t = f->text.substr((size_t)n->loc.offset, (size_t)n->loc.length);
        std::string clean;
        for (char ch : t) if (ch != '_') clean += ch;
        try {
            if (clean.size() > 2 && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) return (int64_t)std::stoull(clean.substr(2), nullptr, 16);
            if (clean.size() > 2 && clean[0] == '0' && (clean[1] == 'b' || clean[1] == 'B')) return (int64_t)std::stoull(clean.substr(2), nullptr, 2);
            if (!clean.empty() && std::isdigit((unsigned char)clean[0])) return (int64_t)std::stoull(clean, nullptr, 10);
        } catch (...) {}
        return (int64_t)n->value;
    }

    bool fold(Expr* e, int64_t& out) {
        switch (e->kind) {
            case ExprKind::Number: out = literal(static_cast<NumberExpr*>(e)); return true;
            case ExprKind::Bool: out = static_cast<BoolExpr*>(e)->value ? 1 : 0; return true;
            case ExprKind::Null: out = 0; return true;
            case ExprKind::Ident: {
                auto* id = static_cast<IdentExpr*>(e);
                if (!isShadowed(id->name)) { auto it = consts.find(id->name); if (it != consts.end()) { out = it->second; return true; } }
                return false;
            }
            case ExprKind::Member: {
                auto* m = static_cast<MemberExpr*>(e);
                if (m->object->kind == ExprKind::Ident) {
                    std::string o = static_cast<IdentExpr*>(m->object.get())->name;
                    auto it = consts.find(o + "." + m->name);
                    if (it != consts.end() && !isShadowed(o)) { out = it->second; return true; }
                    if (m->name == "size" && structs.count(o) && !isShadowed(o)) { out = structs[o].size; return true; }
                }
                return false;
            }
            case ExprKind::Call: {
                auto* c = static_cast<CallExpr*>(e);
                if (c->callee->kind != ExprKind::Ident || c->args.size() != 1) return false;
                std::string fn = static_cast<IdentExpr*>(c->callee.get())->name;
                if (fn == "sizeof" && c->args[0].value->kind == ExprKind::Ident) {
                    std::string t = static_cast<IdentExpr*>(c->args[0].value.get())->name;
                    if (!structs.count(t)) fail(c->loc, "sizeof(...) necesita el nombre de un struct");
                    out = structs[t].size; return true;
                }
                if (fn == "embedSize" && c->args[0].value->kind == ExprKind::String) {
                    out = embedFile(static_cast<StringExpr*>(c->args[0].value.get())->parts[0].text, c->loc).second; return true;
                }
                return false;
            }
            case ExprKind::Unary: {
                auto* u = static_cast<UnaryExpr*>(e);
                int64_t a; if (!fold(u->operand.get(), a)) return false;
                if (u->op == Tok::Minus) out = -a; else if (u->op == Tok::Tilde) out = ~a;
                else if (u->op == Tok::KwNot || u->op == Tok::Bang) out = !a; else if (u->op == Tok::Plus) out = a; else return false;
                return true;
            }
            case ExprKind::Binary: {
                auto* b = static_cast<BinaryExpr*>(e);
                int64_t x, y;
                if (!fold(b->left.get(), x) || !fold(b->right.get(), y)) return false;
                switch (b->op) {
                    case Tok::Plus: out = x + y; break; case Tok::Minus: out = x - y; break; case Tok::Star: out = x * y; break;
                    case Tok::Slash: if (!y) return false; out = x / y; break; case Tok::Percent: if (!y) return false; out = x % y; break;
                    case Tok::Amp: out = x & y; break; case Tok::Pipe: out = x | y; break; case Tok::Caret: out = x ^ y; break;
                    case Tok::Shl: out = (int64_t)((uint64_t)x << (y & 63)); break; case Tok::Shr: out = x >> (y & 63); break;
                    case Tok::EqEq: out = x == y; break; case Tok::NotEq: out = x != y; break; case Tok::Less: out = x < y; break;
                    case Tok::LessEq: out = x <= y; break; case Tok::Greater: out = x > y; break; case Tok::GreaterEq: out = x >= y; break;
                    default: return false;
                }
                return true;
            }
            default: return false;
        }
    }
    bool isShadowed(const std::string& n) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) if (it->count(n)) return true;
        return false;
    }

    // ------------------------------------------------------------------------------------------------ emisión
    void b(std::initializer_list<int> bs) { for (int x : bs) code.push_back((uint8_t)x); }
    void d32(int64_t v) { for (int i = 0; i < 4; i++) code.push_back((uint8_t)(v >> (8 * i))); }
    void d64(int64_t v) { for (int i = 0; i < 8; i++) code.push_back((uint8_t)(v >> (8 * i))); }
    int newLabel() { labels.push_back(-1); return (int)labels.size() - 1; }
    void place(int l) { labels[(size_t)l] = (int)code.size(); }
    void fix(Target k, int v) { fixes.push_back({(int)code.size(), k, v}); d32(0); }
    void jmp(int l) { b({0xE9}); fix(Target::Code, l); }
    void jz(int l) { b({0x0F, 0x84}); fix(Target::Code, l); }
    void jnz(int l) { b({0x0F, 0x85}); fix(Target::Code, l); }
    void push() { b({0x50}); depth += 8; }
    void popRcx() { b({0x59}); depth -= 8; }
    void popRax() { b({0x58}); depth -= 8; }
    void popRdx() { b({0x5A}); depth -= 8; }
    void movRaxImm(int64_t v) {
        if (v >= 0 && v <= 0xFFFFFFFFLL) { b({0xB8}); d32(v); }                 // mov eax, imm32 (borra la mitad alta)
        else if (v >= INT32_MIN && v < 0) { b({0x48, 0xC7, 0xC0}); d32(v); }     // mov rax, simm32
        else { b({0x48, 0xB8}); d64(v); }                                        // mov rax, imm64
    }
    void loadLocal(int off) { b({0x48, 0x8B, 0x85}); d32(off); }
    void storeLocal(int off) { b({0x48, 0x89, 0x85}); d32(off); }
    Target gTarget(const GlobalInfo& g) { return g.bss ? Target::Bss : Target::Data; }
    void leaGlobal(const GlobalInfo& g) { b({0x48, 0x8D, 0x05}); fix(gTarget(g), g.off); }
    void loadGlobal(const GlobalInfo& g) { b({0x48, 0x8B, 0x05}); fix(gTarget(g), g.off); }
    void storeGlobal(const GlobalInfo& g) { b({0x48, 0x89, 0x05}); fix(gTarget(g), g.off); }
    void leaData(int off) { b({0x48, 0x8D, 0x05}); fix(Target::Data, off); }
    void leaFunc(const std::string& name) { b({0x48, 0x8D, 0x05}); fix(Target::Code, funcLabel(name)); }
    int funcLabel(const std::string& name) { auto& f = funcs[name]; if (f.label < 0) f.label = newLabel(); return f.label; }
    // [rax] ← carga con el tamaño del campo
    void loadSized(int size, bool sign) {
        switch (size) {
            case 1: if (sign) b({0x48, 0x0F, 0xBE, 0x00}); else b({0x0F, 0xB6, 0x00}); break;
            case 2: if (sign) b({0x48, 0x0F, 0xBF, 0x00}); else b({0x0F, 0xB7, 0x00}); break;
            case 4: if (sign) b({0x48, 0x63, 0x00}); else b({0x8B, 0x00}); break;
            default: b({0x48, 0x8B, 0x00});
        }
    }
    // [rax] ← rcx
    void storeSized(int size) {
        switch (size) {
            case 1: b({0x88, 0x08}); break;
            case 2: b({0x66, 0x89, 0x08}); break;
            case 4: b({0x89, 0x08}); break;
            default: b({0x48, 0x89, 0x08});
        }
    }
    void addRaxImm(int v) { if (v) { b({0x48, 0x05}); d32(v); } }
    void testRax() { b({0x48, 0x85, 0xC0}); }
    void setcc(int cc) { b({0x0F, cc, 0xC0, 0x0F, 0xB6, 0xC0}); } // setcc al; movzx eax, al

    // ------------------------------------------------------------------------------------------------ tipos
    std::string typeOf(Expr* e) {
        switch (e->kind) {
            case ExprKind::Ident: {
                auto* id = static_cast<IdentExpr*>(e);
                for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) { auto f = it->find(id->name); if (f != it->end()) return f->second.type; }
                if (!curOwner.empty()) if (auto* fl = structs[curOwner].field(id->name)) return fl->type;
                auto g = globals.find(id->name);
                return g != globals.end() ? g->second.type : "";
            }
            case ExprKind::This: return curOwner;
            case ExprKind::Member: {
                auto* m = static_cast<MemberExpr*>(e);
                std::string t = typeOf(m->object.get());
                if (!t.empty()) {
                    if (auto* f = structs[t].field(m->name)) return f->type;
                    auto mt = structs[t].methods.find(m->name);
                    if (mt != structs[t].methods.end() && mt->second->kind == FuncKind::Getter) return structType(mt->second->ret.get());
                }
                return "";
            }
            case ExprKind::Call: {
                auto* c = static_cast<CallExpr*>(e);
                if (c->callee->kind == ExprKind::Ident) {
                    std::string n = static_cast<IdentExpr*>(c->callee.get())->name;
                    if (structs.count(n) && !isShadowed(n)) return n;            // Ventana(direccion)
                    auto f = funcs.find(n);
                    if (f != funcs.end() && !isShadowed(n)) return structType(f->second.decl->ret.get());
                } else if (c->callee->kind == ExprKind::Member) {
                    auto* m = static_cast<MemberExpr*>(c->callee.get());
                    std::string t = typeOf(m->object.get());
                    if (!t.empty()) { auto mt = structs[t].methods.find(m->name); if (mt != structs[t].methods.end()) return structType(mt->second->ret.get()); }
                }
                return "";
            }
            case ExprKind::Ternary: return typeOf(static_cast<TernaryExpr*>(e)->then.get());
            case ExprKind::Assign: return typeOf(static_cast<AssignExpr*>(e)->value.get());
            default: return "";
        }
    }

    // ------------------------------------------------------------------------------------------------ expresiones
    const LocalInfo2* local(const std::string& n) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) { auto f = it->find(n); if (f != it->end()) return &f->second; }
        return nullptr;
    }

    // El lugar al que se asigna: una local, una global o un campo (dirección en rax tras evalAddress)
    enum class PlaceKind { Local, Global, Field };
    struct Place { PlaceKind kind; int off = 0; const GlobalInfo* g = nullptr; const Field* f = nullptr; Expr* object = nullptr; bool thisField = false; };
    Place place(Expr* e) {
        if (e->kind == ExprKind::Ident) {
            auto* id = static_cast<IdentExpr*>(e);
            if (auto* l = local(id->name)) return {PlaceKind::Local, l->off};
            if (!curOwner.empty()) if (auto* f = structs[curOwner].field(id->name)) { Place p{PlaceKind::Field}; p.f = f; p.thisField = true; return p; }
            auto g = globals.find(id->name);
            if (g != globals.end()) {
                if (g->second.address) fail(e->loc, "'" + id->name + "' es una zona de memoria: escribe en ella con poke");
                Place p{PlaceKind::Global}; p.g = &g->second; return p;
            }
            if (consts.count(id->name)) fail(e->loc, "'" + id->name + "' es una constante");
            fail(e->loc, "No existe nada llamado '" + id->name + "'");
        }
        if (e->kind == ExprKind::Member) {
            auto* m = static_cast<MemberExpr*>(e);
            std::string t = typeOf(m->object.get());
            if (t.empty()) fail(e->loc, "No se sabe de qué struct es '" + m->name + "': declara la variable con su tipo (var v: Ventana = ...)");
            auto* f = structs[t].field(m->name);
            if (!f) fail(m->nameLoc, "El struct " + t + " no tiene el campo '" + m->name + "'");
            Place p{PlaceKind::Field}; p.f = f; p.object = m->object.get(); return p;
        }
        fail(e->loc, "No se puede asignar a esto");
    }
    // rax ← dirección del campo
    void fieldAddress(const Place& p) {
        if (p.thisField) loadLocal(-8); else expr(p.object);
        addRaxImm(p.f->offset);
    }
    void loadPlace(const Place& p) {
        switch (p.kind) {
            case PlaceKind::Local: loadLocal(p.off); break;
            case PlaceKind::Global: loadGlobal(*p.g); break;
            case PlaceKind::Field: fieldAddress(p); loadSized(p.f->size, p.f->sign); break;
        }
    }

    void binop(Tok op, SrcLoc loc) { // rax = rax op rcx
        switch (op) {
            case Tok::Plus: case Tok::PlusEq: b({0x48, 0x01, 0xC8}); break;
            case Tok::Minus: case Tok::MinusEq: b({0x48, 0x29, 0xC8}); break;
            case Tok::Star: case Tok::StarEq: b({0x48, 0x0F, 0xAF, 0xC1}); break;
            case Tok::Slash: case Tok::SlashEq: b({0x48, 0x99, 0x48, 0xF7, 0xF9}); break;
            case Tok::Percent: case Tok::PercentEq: b({0x48, 0x99, 0x48, 0xF7, 0xF9, 0x48, 0x89, 0xD0}); break;
            case Tok::Amp: b({0x48, 0x21, 0xC8}); break;
            case Tok::Pipe: b({0x48, 0x09, 0xC8}); break;
            case Tok::Caret: b({0x48, 0x31, 0xC8}); break;
            case Tok::Shl: b({0x48, 0xD3, 0xE0}); break;
            case Tok::Shr: b({0x48, 0xD3, 0xF8}); break;
            case Tok::EqEq: b({0x48, 0x39, 0xC8}); setcc(0x94); break;
            case Tok::NotEq: b({0x48, 0x39, 0xC8}); setcc(0x95); break;
            case Tok::Less: b({0x48, 0x39, 0xC8}); setcc(0x9C); break;
            case Tok::LessEq: b({0x48, 0x39, 0xC8}); setcc(0x9E); break;
            case Tok::Greater: b({0x48, 0x39, 0xC8}); setcc(0x9F); break;
            case Tok::GreaterEq: b({0x48, 0x39, 0xC8}); setcc(0x9D); break;
            default: fail(loc, std::string("El operador ") + tokName(op) + " no existe en HyperEZ nativo");
        }
    }

    void expr(Expr* e) {
        int64_t k;
        if (e->kind != ExprKind::Assign && e->kind != ExprKind::Call && e->kind != ExprKind::IncDec && fold(e, k)) { movRaxImm(k); return; }
        switch (e->kind) {
            case ExprKind::String: leaData(stringData(static_cast<StringExpr*>(e))); return;
            case ExprKind::This:
                if (curOwner.empty()) fail(e->loc, "this solo existe dentro de los métodos de un struct");
                loadLocal(-8); return;
            case ExprKind::Ident: {
                auto* id = static_cast<IdentExpr*>(e);
                if (local(id->name) || (!curOwner.empty() && structs[curOwner].field(id->name))) { loadPlace(place(e)); return; }
                auto g = globals.find(id->name);
                if (g != globals.end()) { if (g->second.address) leaGlobal(g->second); else loadGlobal(g->second); return; }
                if (funcs.count(id->name)) { leaFunc(id->name); return; } // la dirección de la función
                fail(e->loc, "No existe nada llamado '" + id->name + "'");
            }
            case ExprKind::Member: {
                auto* m = static_cast<MemberExpr*>(e);
                std::string t = typeOf(m->object.get());
                if (!t.empty()) {
                    auto mt = structs[t].methods.find(m->name);
                    if (mt != structs[t].methods.end() && mt->second->kind == FuncKind::Getter) { callMethod(t, m->name, m->object.get(), {}, e->loc); return; }
                }
                loadPlace(place(e));
                return;
            }
            case ExprKind::Unary: {
                auto* u = static_cast<UnaryExpr*>(e);
                expr(u->operand.get());
                if (u->op == Tok::Minus) b({0x48, 0xF7, 0xD8});
                else if (u->op == Tok::Tilde) b({0x48, 0xF7, 0xD0});
                else if (u->op == Tok::KwNot || u->op == Tok::Bang) { testRax(); setcc(0x94); }
                else if (u->op != Tok::Plus) fail(e->loc, "Operador no admitido en HyperEZ nativo");
                return;
            }
            case ExprKind::Binary: {
                auto* bn = static_cast<BinaryExpr*>(e);
                expr(bn->left.get()); push();
                expr(bn->right.get());
                b({0x48, 0x89, 0xC1}); // mov rcx, rax
                popRax();
                binop(bn->op, e->loc);
                return;
            }
            case ExprKind::Logical: {
                auto* l = static_cast<LogicalExpr*>(e);
                if (l->op != Tok::KwAnd && l->op != Tok::KwOr && l->op != Tok::AmpAmp && l->op != Tok::PipePipe) fail(e->loc, "?? no existe en HyperEZ nativo");
                bool isAnd = l->op == Tok::KwAnd || l->op == Tok::AmpAmp;
                int corto = newLabel(), fin = newLabel();
                expr(l->left.get()); testRax(); if (isAnd) jz(corto); else jnz(corto);
                expr(l->right.get()); testRax(); if (isAnd) jz(corto); else jnz(corto);
                movRaxImm(isAnd ? 1 : 0); jmp(fin);
                place(corto); movRaxImm(isAnd ? 0 : 1);
                place(fin);
                return;
            }
            case ExprKind::Ternary: {
                auto* t = static_cast<TernaryExpr*>(e);
                int otro = newLabel(), fin = newLabel();
                expr(t->cond.get()); testRax(); jz(otro);
                expr(t->then.get()); jmp(fin);
                place(otro); expr(t->otherwise.get());
                place(fin);
                return;
            }
            case ExprKind::Assign: assign(static_cast<AssignExpr*>(e)); return;
            case ExprKind::IncDec: {
                auto* id = static_cast<IncDecExpr*>(e);
                Place p = place(id->target.get());
                if (p.kind == PlaceKind::Field) {
                    fieldAddress(p); push();
                    loadSized(p.f->size, p.f->sign);
                    if (!id->prefix) { b({0x48, 0x89, 0xC2}); } // mov rdx, rax (valor anterior)
                    b({0x48, 0x8D, 0x48, id->inc ? 0x01 : 0xFF}); // lea rcx, [rax±1]
                    if (id->prefix) b({0x48, 0x89, 0xCA}); // mov rdx, rcx
                    popRax(); storeSized(p.f->size);
                    b({0x48, 0x89, 0xD0}); // mov rax, rdx
                    return;
                }
                loadPlace(p);
                b({0x48, 0x8D, 0x48, id->inc ? 0x01 : 0xFF}); // lea rcx, [rax±1]
                if (id->prefix) b({0x48, 0x89, 0xC8}); // el resultado es el valor nuevo
                b({0x48, 0x87, 0xC8}); // xchg rax, rcx (rax = nuevo para guardar, rcx = resultado)
                if (p.kind == PlaceKind::Local) storeLocal(p.off); else storeGlobal(*p.g);
                b({0x48, 0x89, 0xC8}); // mov rax, rcx
                return;
            }
            case ExprKind::Call: call(static_cast<CallExpr*>(e)); return;
            case ExprKind::Index: fail(e->loc, "En HyperEZ nativo se lee la memoria con peek8/16/32/64(direccion)");
            default: fail(e->loc, "Esta expresión no existe en HyperEZ nativo (solo enteros, direcciones y struct)");
        }
    }

    void storePlace(const Place& p) { // guarda rax
        if (p.kind == PlaceKind::Local) storeLocal(p.off);
        else if (p.kind == PlaceKind::Global) storeGlobal(*p.g);
        else {
            push();
            fieldAddress(p);
            popRcx();
            storeSized(p.f->size);
            b({0x48, 0x89, 0xC8});
        }
    }

    void assign(AssignExpr* a) {
        Place p = place(a->target.get());
        if (a->op == Tok::Eq) { expr(a->value.get()); storePlace(p); return; }
        if (a->op == Tok::QuestionQuestionEq || a->op == Tok::StarStarEq) fail(a->loc, "Operador no admitido en HyperEZ nativo");
        if (p.kind == PlaceKind::Field) {
            expr(a->value.get()); push();
            fieldAddress(p); push();
            b({0x48, 0x8B, 0x04, 0x24});                  // mov rax, [rsp]  (dirección)
            loadSized(p.f->size, p.f->sign);
            b({0x48, 0x8B, 0x4C, 0x24, 0x08});            // mov rcx, [rsp+8] (valor)
            binop(a->op, a->loc);
            b({0x48, 0x89, 0xC1});                        // mov rcx, rax
            popRax();
            storeSized(p.f->size);
            b({0x48, 0x83, 0xC4, 0x08}); depth -= 8;      // quita el valor de la pila
            b({0x48, 0x89, 0xC8});
            return;
        }
        expr(a->value.get()); push();
        loadPlace(p);
        popRcx();
        binop(a->op, a->loc);
        storePlace(p);
    }

    // ---- llamadas (convención Microsoft x64: rcx, rdx, r8, r9, 32 bytes de "shadow space", pila alineada a 16)
    void emitCall(int nargs, const std::function<void()>& callTarget, int extraPushed) {
        // los nargs argumentos están en la pila (el primero, el más profundo); extraPushed = valores debajo de ellos
        int S = 32 + 8 * std::max(0, nargs - 4);
        if ((depth + S) % 16) S += 8;
        b({0x48, 0x81, 0xEC}); d32(S);
        auto argOff = [&](int i) { return S + 8 * (nargs - 1 - i); };
        for (int i = 4; i < nargs; i++) {
            b({0x48, 0x8B, 0x84, 0x24}); d32(argOff(i));     // mov rax, [rsp+..]
            b({0x48, 0x89, 0x84, 0x24}); d32(32 + 8 * (i - 4)); // mov [rsp+32+..], rax
        }
        static const int regs[4][3] = {{0x48, 0x8B, 0x8C}, {0x48, 0x8B, 0x94}, {0x4C, 0x8B, 0x84}, {0x4C, 0x8B, 0x8C}};
        for (int i = 0; i < std::min(nargs, 4); i++) { b({regs[i][0], regs[i][1], regs[i][2], 0x24}); d32(argOff(i)); }
        callTarget();
        b({0x48, 0x81, 0xC4}); d32(S + 8 * (nargs + extraPushed));
        depth -= 8 * (nargs + extraPushed);
    }
    void callDirect(const std::string& name, const std::vector<Expr*>& args, Expr* receiver, SrcLoc loc) {
        FuncDecl* fd = funcs[name].decl;
        size_t expected = fd->params.size() + (receiver ? 1 : 0);
        size_t given = args.size() + (receiver ? 1 : 0);
        std::vector<Expr*> all;
        if (receiver) all.push_back(receiver);
        all.insert(all.end(), args.begin(), args.end());
        if (given > expected) fail(loc, name + " recibe " + std::to_string(fd->params.size()) + " argumentos");
        for (auto* a : all) { expr(a); push(); }
        for (size_t i = given; i < expected; i++) { // parámetros con valor por defecto
            const Param& pm = fd->params[i - (receiver ? 1 : 0)];
            int64_t k = 0;
            if (!pm.def || !fold(pm.def.get(), k)) fail(loc, "Falta el argumento '" + pm.name + "' de " + name);
            movRaxImm(k); push();
        }
        int label = funcLabel(name);
        emitCall((int)expected, [&] { b({0xE8}); fix(Target::Code, label); }, 0);
    }
    void callMethod(const std::string& st, const std::string& m, Expr* receiver, const std::vector<Expr*>& args, SrcLoc loc) {
        callDirect(st + "." + m, args, receiver, loc);
    }

    std::vector<Expr*> argList(CallExpr* c) {
        std::vector<Expr*> v;
        for (auto& a : c->args) {
            if (!a.name.empty() || a.spread) fail(a.value->loc, "En HyperEZ nativo los argumentos van por orden");
            v.push_back(a.value.get());
        }
        return v;
    }

    void call(CallExpr* c) {
        auto args = argList(c);
        if (c->callee->kind == ExprKind::Member) {
            auto* m = static_cast<MemberExpr*>(c->callee.get());
            std::string t = typeOf(m->object.get());
            if (t.empty()) fail(c->loc, "No se sabe de qué struct es el método '" + m->name + "': declara la variable con su tipo");
            if (!structs[t].methods.count(m->name)) fail(m->nameLoc, "El struct " + t + " no tiene el método '" + m->name + "'");
            callMethod(t, m->name, m->object.get(), args, c->loc);
            return;
        }
        if (c->callee->kind != ExprKind::Ident) fail(c->loc, "En HyperEZ nativo se llama a funciones por su nombre (o con callptr)");
        std::string n = static_cast<IdentExpr*>(c->callee.get())->name;
        bool shadow = isShadowed(n);
        if (!shadow && intrinsic(n, args, c)) return;
        if (!shadow && structs.count(n)) { // Ventana(direccion): ver una dirección como un struct
            if (args.size() != 1) fail(c->loc, n + "(direccion) recibe la dirección del struct");
            expr(args[0]);
            return;
        }
        if (!shadow && funcs.count(n) && funcs[n].owner.empty()) { callDirect(n, args, nullptr, c->loc); return; }
        if (!shadow && !curOwner.empty() && structs[curOwner].methods.count(n)) { // otro método del mismo struct
            ThisExpr self(c->loc);
            callDirect(curOwner + "." + n, args, &self, c->loc);
            return;
        }
        if (local(n) || globals.count(n)) { // una variable con la dirección de una función
            expr(c->callee.get()); push();
            for (auto* a : args) { expr(a); push(); }
            int na = (int)args.size();
            emitCall(na, [&] {
                int S0 = 32 + 8 * std::max(0, na - 4); if ((depth + S0) % 16) S0 += 8;
                b({0x48, 0x8B, 0x84, 0x24}); d32(S0 + 8 * na); // mov rax, [rsp + ...] (la función)
                b({0xFF, 0xD0});
            }, 1);
            return;
        }
        fail(c->callee->loc, "No existe la función '" + n + "'");
    }

    void need(const std::vector<Expr*>& a, size_t n, const std::string& name, SrcLoc loc) {
        if (a.size() != n) fail(loc, name + " recibe " + std::to_string(n) + " argumento" + (n == 1 ? "" : "s"));
    }
    bool intrinsic(const std::string& n, const std::vector<Expr*>& a, CallExpr* c) {
        SrcLoc loc = c->loc;
        static const std::map<std::string, std::pair<int, bool>> peeks = {
            {"peek8", {1, false}}, {"peek16", {2, false}}, {"peek32", {4, false}}, {"peek64", {8, true}},
            {"peeks8", {1, true}}, {"peeks16", {2, true}}, {"peeks32", {4, true}}};
        static const std::map<std::string, int> pokes = {{"poke8", 1}, {"poke16", 2}, {"poke32", 4}, {"poke64", 8}};
        if (auto it = peeks.find(n); it != peeks.end()) { need(a, 1, n, loc); expr(a[0]); loadSized(it->second.first, it->second.second); return true; }
        if (auto it = pokes.find(n); it != pokes.end()) {
            need(a, 2, n, loc);
            expr(a[1]); push(); expr(a[0]); popRcx(); storeSized(it->second); b({0x48, 0x89, 0xC8});
            return true;
        }
        if (n == "inb" || n == "inw" || n == "inl") {
            need(a, 1, n, loc); expr(a[0]); b({0x89, 0xC2}); // mov edx, eax
            if (n == "inb") b({0xEC, 0x0F, 0xB6, 0xC0}); else if (n == "inw") b({0x66, 0xED, 0x0F, 0xB7, 0xC0}); else b({0xED});
            return true;
        }
        if (n == "outb" || n == "outw" || n == "outl") {
            need(a, 2, n, loc); expr(a[0]); push(); expr(a[1]); popRdx();
            if (n == "outb") b({0xEE}); else if (n == "outw") b({0x66, 0xEF}); else b({0xEF});
            return true;
        }
        if (n == "cli") { need(a, 0, n, loc); b({0xFA}); return true; }
        if (n == "sti") { need(a, 0, n, loc); b({0xFB}); return true; }
        if (n == "hlt") { need(a, 0, n, loc); b({0xF4}); return true; }
        if (n == "pause") { need(a, 0, n, loc); b({0xF3, 0x90}); return true; }
        if (n == "rdtsc") { need(a, 0, n, loc); b({0x0F, 0x31, 0x48, 0xC1, 0xE2, 0x20, 0x48, 0x09, 0xD0}); return true; }
        if (n == "copy" || n == "copy32") {
            need(a, 3, n, loc);
            expr(a[0]); push(); expr(a[1]); push(); expr(a[2]);
            b({0x48, 0x89, 0xC1}); popRdx(); popRax();          // rcx = n, rdx = origen, rax = destino
            b({0x56, 0x57, 0x48, 0x89, 0xC7, 0x48, 0x89, 0xD6}); // push rsi; push rdi; mov rdi, rax; mov rsi, rdx
            if (n == "copy") b({0xF3, 0xA4}); else b({0xF3, 0xA5});
            b({0x5F, 0x5E});
            return true;
        }
        if (n == "fill8" || n == "fill32") {
            need(a, 3, n, loc);
            expr(a[0]); push(); expr(a[1]); push(); expr(a[2]);
            b({0x48, 0x89, 0xC1}); popRax(); popRdx();          // rcx = n, rax = valor, rdx = destino
            b({0x57, 0x48, 0x89, 0xD7});                        // push rdi; mov rdi, rdx
            if (n == "fill8") b({0xF3, 0xAA}); else b({0xF3, 0xAB});
            b({0x5F});
            return true;
        }
        if (n == "udiv" || n == "umod" || n == "ult") {
            need(a, 2, n, loc);
            expr(a[0]); push(); expr(a[1]); b({0x48, 0x89, 0xC1}); popRax();
            if (n == "ult") { b({0x48, 0x39, 0xC8}); setcc(0x92); }
            else { b({0x31, 0xD2, 0x48, 0xF7, 0xF1}); if (n == "umod") b({0x48, 0x89, 0xD0}); }
            return true;
        }
        if (n == "cpuid") { // cpuid(hoja, subhoja, destino): eax, ebx, ecx y edx en 16 bytes
            need(a, 3, n, loc);
            expr(a[0]); push(); expr(a[1]); push(); expr(a[2]);
            b({0x49, 0x89, 0xC0}); popRcx(); popRax();          // r8 = destino
            b({0x53, 0x0F, 0xA2, 0x41, 0x89, 0x00, 0x41, 0x89, 0x58, 0x04, 0x41, 0x89, 0x48, 0x08, 0x41, 0x89, 0x50, 0x0C, 0x5B});
            return true;
        }
        if (n == "callptr") {
            if (a.empty()) fail(loc, "callptr(funcion, argumentos...)");
            for (auto* x : a) { expr(x); push(); }
            int na = (int)a.size() - 1;
            emitCall(na, [&] {
                int S0 = 32 + 8 * std::max(0, na - 4); if ((depth + S0) % 16) S0 += 8;
                b({0x48, 0x8B, 0x84, 0x24}); d32(S0 + 8 * na);
                b({0xFF, 0xD0});
            }, 1);
            return true;
        }
        if (n == "u16" || n == "embed") { leaData(constantData(c)); return true; }
        if (n == "buffer") fail(loc, "buffer(n) solo se usa en una variable global (var datos = buffer(4096))");
        if (n == "sizeof" || n == "embedSize") { int64_t k; if (!fold(c, k)) fail(loc, n + " necesita un valor constante"); movRaxImm(k); return true; }
        return false;
    }

    // ------------------------------------------------------------------------------------------------ sentencias
    int newSlot() { slots++; return -8 * slots; }
    void stmt(Stmt* s) {
        switch (s->kind) {
            case StmtKind::Expr: expr(static_cast<ExprStmt*>(s)->expr.get()); return;
            case StmtKind::Var: {
                auto* v = static_cast<VarStmt*>(s);
                if (scopes.back().count(v->name)) fail(v->nameLoc, "'" + v->name + "' ya está declarado aquí");
                std::string t = structType(v->typeAnn.get());
                if (v->init) { expr(v->init.get()); if (t.empty()) t = typeOf(v->init.get()); } else movRaxImm(0);
                int off = newSlot();
                storeLocal(off);
                scopes.back()[v->name] = {off, t};
                return;
            }
            case StmtKind::Block: {
                scopes.emplace_back();
                for (auto& x : static_cast<BlockStmt*>(s)->body) stmt(x.get());
                scopes.pop_back();
                return;
            }
            case StmtKind::If: {
                auto* i = static_cast<IfStmt*>(s);
                int otro = newLabel(), fin = newLabel();
                expr(i->cond.get()); testRax(); jz(otro);
                stmt(i->then.get());
                if (i->otherwise) { jmp(fin); place(otro); stmt(i->otherwise.get()); place(fin); }
                else place(otro);
                return;
            }
            case StmtKind::While: {
                auto* w = static_cast<WhileStmt*>(s);
                int inicio = newLabel(), fin = newLabel();
                place(inicio);
                expr(w->cond.get()); testRax(); jz(fin);
                breakLabels.push_back(fin); continueLabels.push_back(inicio);
                stmt(w->body.get());
                breakLabels.pop_back(); continueLabels.pop_back();
                jmp(inicio);
                place(fin);
                return;
            }
            case StmtKind::ForRange: {
                auto* f = static_cast<ForRangeStmt*>(s);
                scopes.emplace_back();
                int64_t paso = 1;
                if (f->step && !fold(f->step.get(), paso)) fail(f->step->loc, "En HyperEZ nativo el paso (step) tiene que ser constante");
                if (paso == 0) fail(f->step->loc, "El paso no puede ser 0");
                expr(f->start.get()); int iv = newSlot(); storeLocal(iv);
                expr(f->end.get()); int fv = newSlot(); storeLocal(fv);
                scopes.back()[f->var] = {iv, ""};
                int inicio = newLabel(), sig = newLabel(), fin = newLabel();
                place(inicio);
                loadLocal(iv); b({0x48, 0x8B, 0x8D}); d32(fv); // mov rcx, [rbp+fin]
                b({0x48, 0x39, 0xC8});                          // cmp rax, rcx
                int cc = paso > 0 ? (f->inclusive ? 0x8F : 0x8D) : (f->inclusive ? 0x8C : 0x8E); // jg / jge / jl / jle → salir
                b({0x0F, cc}); fix(Target::Code, fin);
                breakLabels.push_back(fin); continueLabels.push_back(sig);
                stmt(f->body.get());
                breakLabels.pop_back(); continueLabels.pop_back();
                place(sig);
                loadLocal(iv); b({0x48, 0x05}); d32(paso); storeLocal(iv);
                jmp(inicio);
                place(fin);
                scopes.pop_back();
                return;
            }
            case StmtKind::Repeat: {
                auto* r = static_cast<RepeatStmt*>(s);
                expr(r->count.get()); int cv = newSlot(); storeLocal(cv);
                int inicio = newLabel(), fin = newLabel();
                place(inicio);
                loadLocal(cv); testRax(); b({0x0F, 0x8E}); fix(Target::Code, fin); // jle fin
                b({0x48, 0xFF, 0xC8}); storeLocal(cv);                            // dec rax
                breakLabels.push_back(fin); continueLabels.push_back(inicio);
                stmt(r->body.get());
                breakLabels.pop_back(); continueLabels.pop_back();
                jmp(inicio);
                place(fin);
                return;
            }
            case StmtKind::Break: if (breakLabels.empty()) fail(s->loc, "break fuera de un bucle"); jmp(breakLabels.back()); return;
            case StmtKind::Continue: if (continueLabels.empty()) fail(s->loc, "continue fuera de un bucle"); jmp(continueLabels.back()); return;
            case StmtKind::Return: {
                auto* r = static_cast<ReturnStmt*>(s);
                if (r->value) expr(r->value.get()); else movRaxImm(0);
                jmp(retLabel);
                return;
            }
            case StmtKind::ForEach: fail(s->loc, "En HyperEZ nativo se recorre con for i in 0..n");
            case StmtKind::Try: case StmtKind::Throw: fail(s->loc, "En HyperEZ nativo los errores son códigos que devuelven las funciones");
            default: fail(s->loc, "Esta sentencia no existe en HyperEZ nativo (o va al nivel superior del archivo)");
        }
    }

    void function(const std::string& name, FuncInfo& fi) {
        curFile = fi.file;
        curOwner = fi.owner;
        FuncDecl* fd = fi.decl;
        place(funcLabel(name));
        scopes.clear(); scopes.emplace_back();
        slots = 0; depth = 0;
        breakLabels.clear(); continueLabels.clear();
        retLabel = newLabel();
        b({0x55, 0x48, 0x89, 0xE5, 0x48, 0x81, 0xEC}); // push rbp; mov rbp, rsp; sub rsp, imm32
        size_t frameAt = code.size();
        d32(0);
        std::vector<std::pair<std::string, std::string>> params;
        if (!fi.owner.empty()) params.push_back({"this", fi.owner});
        for (auto& p : fd->params) {
            if (p.rest) fail(p.loc, "Los parámetros ...resto no existen en HyperEZ nativo");
            params.push_back({p.name, structType(p.type.get())});
        }
        static const int regs[4][3] = {{0x48, 0x89, 0x8D}, {0x48, 0x89, 0x95}, {0x4C, 0x89, 0x85}, {0x4C, 0x89, 0x8D}};
        for (size_t i = 0; i < params.size(); i++) {
            int off = newSlot();
            if (i < 4) { b({regs[i][0], regs[i][1], regs[i][2]}); d32(off); }
            else { b({0x48, 0x8B, 0x85}); d32(48 + 8 * ((int)i - 4)); storeLocal(off); }
            if (params[i].first != "this") scopes.back()[params[i].first] = {off, params[i].second};
        }
        if (fd->body) stmt(fd->body.get());
        else if (fd->exprBody) { expr(fd->exprBody.get()); jmp(retLabel); }
        movRaxImm(0);
        place(retLabel);
        b({0xC9, 0xC3}); // leave; ret
        int frame = (slots * 8 + 15) & ~15;
        for (int i = 0; i < 4; i++) code[frameAt + i] = (uint8_t)(frame >> (8 * i));
        if (depth != 0) fail(fd->loc, "error interno del compilador nativo: la pila no cuadra");
    }

    void compileAll() {
        declare();
        // main primero: así el binario plano empieza por el punto de entrada
        std::vector<std::string> order = {"main"};
        for (auto& [n, f] : funcs) if (n != "main") order.push_back(n);
        for (auto& n : order) function(n, funcs[n]);
        for (auto& [n, f] : funcs) if (f.label >= 0 && labels[(size_t)f.label] < 0) throw CompileError{f.file, f.decl->loc, "error interno: falta '" + n + "'"};
    }

    // ------------------------------------------------------------------------------------------------ enlazar
    void link(int textRVA, int dataRVA, int bssRVA) {
        for (auto& f : fixes) {
            int target = f.kind == Target::Code ? textRVA + labels[(size_t)f.value] : f.kind == Target::Data ? dataRVA + f.value : bssRVA + f.value;
            int disp = target - (textRVA + f.at + 4);
            for (int i = 0; i < 4; i++) code[(size_t)f.at + i] = (uint8_t)(disp >> (8 * i));
        }
    }

    static void put16(std::vector<uint8_t>& v, size_t at, uint32_t x) { v[at] = (uint8_t)x; v[at + 1] = (uint8_t)(x >> 8); }
    static void put32(std::vector<uint8_t>& v, size_t at, uint32_t x) { for (int i = 0; i < 4; i++) v[at + i] = (uint8_t)(x >> (8 * i)); }
    static void put64(std::vector<uint8_t>& v, size_t at, uint64_t x) { for (int i = 0; i < 8; i++) v[at + i] = (uint8_t)(x >> (8 * i)); }
    static uint32_t up(uint32_t x, uint32_t a) { return (x + a - 1) / a * a; }

    // Aplicación UEFI: PE32+ con .text, .data (+ memoria a cero) y una .reloc vacía (el código no tiene direcciones absolutas)
    std::vector<uint8_t> pe() {
        if (data.empty()) data.resize(16, 0);
        const uint32_t SA = 0x1000, FA = 0x200, headers = 0x400;
        uint32_t textRVA = SA, textRaw = up((uint32_t)code.size(), FA);
        uint32_t dataRVA = up(textRVA + (uint32_t)code.size(), SA);
        uint32_t bssOff = up((uint32_t)data.size(), 16);
        uint32_t dataVirt = bssOff + (uint32_t)bssSize, dataRaw = up((uint32_t)data.size(), FA);
        uint32_t relocRVA = up(dataRVA + dataVirt, SA);
        std::vector<uint8_t> reloc(12, 0);
        put32(reloc, 0, textRVA); put32(reloc, 4, 12);
        uint32_t relocRaw = up((uint32_t)reloc.size(), FA);
        uint32_t imageSize = up(relocRVA + (uint32_t)reloc.size(), SA);
        link((int)textRVA, (int)dataRVA, (int)(dataRVA + bssOff));
        int entry = labels[(size_t)funcs["main"].label];

        std::vector<uint8_t> f(headers + textRaw + dataRaw + relocRaw, 0);
        f[0] = 'M'; f[1] = 'Z'; put32(f, 0x3C, 0x80);
        size_t pe = 0x80;
        f[pe] = 'P'; f[pe + 1] = 'E';
        size_t coff = pe + 4;
        put16(f, coff, 0x8664); put16(f, coff + 2, 3); put16(f, coff + 16, 240); put16(f, coff + 18, 0x0022);
        size_t opt = coff + 20;
        put16(f, opt, 0x20B); f[opt + 2] = 1;
        put32(f, opt + 4, textRaw); put32(f, opt + 8, dataRaw + relocRaw); put32(f, opt + 12, (uint32_t)bssSize);
        put32(f, opt + 16, textRVA + (uint32_t)entry); put32(f, opt + 20, textRVA);
        put64(f, opt + 24, 0x10000000ULL);
        put32(f, opt + 32, SA); put32(f, opt + 36, FA);
        put32(f, opt + 56, imageSize); put32(f, opt + 60, headers);
        put16(f, opt + 68, 10);          // EFI_APPLICATION
        put64(f, opt + 72, 0x100000); put64(f, opt + 80, 0x1000); put64(f, opt + 88, 0x100000); put64(f, opt + 96, 0x1000);
        put32(f, opt + 108, 16);
        put32(f, opt + 112 + 5 * 8, relocRVA); put32(f, opt + 112 + 5 * 8 + 4, (uint32_t)reloc.size());
        size_t sec = opt + 240;
        auto section = [&](size_t at, const char* name, uint32_t vsize, uint32_t rva, uint32_t raw, uint32_t ptr, uint32_t ch) {
            std::memcpy(&f[at], name, std::strlen(name));
            put32(f, at + 8, vsize); put32(f, at + 12, rva); put32(f, at + 16, raw); put32(f, at + 20, ptr); put32(f, at + 36, ch);
        };
        uint32_t textPtr = headers, dataPtr = textPtr + textRaw, relocPtr = dataPtr + dataRaw;
        section(sec, ".text", (uint32_t)code.size(), textRVA, textRaw, textPtr, 0x60000020);
        section(sec + 40, ".data", dataVirt, dataRVA, dataRaw, dataPtr, 0xC0000040);
        section(sec + 80, ".reloc", (uint32_t)reloc.size(), relocRVA, relocRaw, relocPtr, 0x42000040);
        std::memcpy(&f[textPtr], code.data(), code.size());
        std::memcpy(&f[dataPtr], data.data(), data.size());
        std::memcpy(&f[relocPtr], reloc.data(), reloc.size());
        return f;
    }

    // Binario plano: código y datos seguidos (main al principio); la memoria a cero va detrás y la pone quien lo carga
    std::vector<uint8_t> flat() {
        uint32_t dataOff = up((uint32_t)code.size(), 16);
        uint32_t bssOff = up(dataOff + (uint32_t)data.size(), 16);
        link(0, (int)dataOff, (int)bssOff);
        std::vector<uint8_t> f(dataOff + data.size(), 0);
        std::memcpy(f.data(), code.data(), code.size());
        if (!data.empty()) std::memcpy(&f[dataOff], data.data(), data.size());
        return f;
    }
};

} // namespace

int nativeBuild(const std::vector<std::string>& args) {
    std::string input, output, format = "uefi";
    bool map = false;
    for (size_t i = 0; i < args.size(); i++) {
        if ((args[i] == "-o" || args[i] == "--salida") && i + 1 < args.size()) output = args[++i];
        else if ((args[i] == "--formato" || args[i] == "--format") && i + 1 < args.size()) format = args[++i];
        else if (args[i] == "--mapa") map = true;
        else if (input.empty()) input = args[i];
    }
    if (input.empty() || (format != "uefi" && format != "bin")) {
        std::fprintf(stderr, "Uso: hyper build <programa.hez> [-o salida] [--formato uefi|bin] [--mapa]\n\n"
                             "  Compila HyperEZ nativo a código máquina x86-64 (sin máquina virtual):\n"
                             "  uefi  aplicación UEFI (BOOTX64.EFI), arranca en cualquier PC moderno desde un USB o una ISO\n"
                             "  bin   binario plano independiente de la posición (main al principio)\n");
        return 2;
    }
    if (output.empty()) output = input.substr(0, input.find_last_of('.')) + (format == "uefi" ? ".efi" : ".bin");
    NativeCompiler nc;
    try {
        nc.load(input);
        nc.compileAll();
        std::vector<uint8_t> out = format == "uefi" ? nc.pe() : nc.flat();
        if (!writeFile(output, std::string_view((const char*)out.data(), out.size()))) {
            std::fprintf(stderr, "error: no se puede escribir '%s'\n", output.c_str());
            return 1;
        }
        std::printf("✓ %s: %zu bytes de código, %zu de datos, %d de memoria a cero, %zu funciones (%zu archivos)\n", output.c_str(),
                    nc.code.size(), nc.data.size(), nc.bssSize, nc.funcs.size(), nc.files.size());
        if (map) {
            std::string m;
            std::vector<std::pair<int, std::string>> v;
            for (auto& [n, f] : nc.funcs) if (f.label >= 0) v.push_back({nc.labels[(size_t)f.label], n});
            std::sort(v.begin(), v.end());
            char buf[160];
            for (auto& [off, n] : v) { std::snprintf(buf, sizeof buf, "%08x  %s\n", off, n.c_str()); m += buf; }
            writeFile(output + ".map", m);
        }
        return 0;
    } catch (CompileError& e) {
        std::fprintf(stderr, "%s:%d:%d: error: %s\n", e.file.c_str(), e.loc.line, e.loc.col, e.msg.c_str());
        return 1;
    }
}

} // namespace hz
