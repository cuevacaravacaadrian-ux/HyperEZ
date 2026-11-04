// Desensamblador del bytecode de HyperEZ (hyper bytecode archivo.hz).
#include "lang/compiler/disasm.h"
#include "lang/compiler/bytecode.h"
#include "lang/vm/vm.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace hz {

namespace {

// Formato de los operandos:
//   k = u16 constante   n = u16 nombre (constante de texto)   s = u16 hueco local   u = u16 upvalue
//   g = u16 global      i = u16 entero   b = u8   c = u8 caché   f = u16 salto adelante   B = u16 salto atrás
struct OpInfo { const char* name; const char* operands; };

OpInfo info(Op op) {
    switch (op) {
        case OP_CONST: return {"CONST", "k"};
        case OP_NULL: return {"NULL", ""};
        case OP_TRUE: return {"TRUE", ""};
        case OP_FALSE: return {"FALSE", ""};
        case OP_UNDEF: return {"UNDEF", ""};
        case OP_POP: return {"POP", ""};
        case OP_DUP: return {"DUP", ""};
        case OP_DUP2: return {"DUP2", ""};
        case OP_SWAP: return {"SWAP", ""};
        case OP_GET_LOCAL: return {"GET_LOCAL", "s"};
        case OP_SET_LOCAL: return {"SET_LOCAL", "s"};
        case OP_GET_UPVAL: return {"GET_UPVAL", "u"};
        case OP_SET_UPVAL: return {"SET_UPVAL", "u"};
        case OP_GET_GLOBAL: return {"GET_GLOBAL", "g"};
        case OP_SET_GLOBAL: return {"SET_GLOBAL", "g"};
        case OP_DEFINE_GLOBAL: return {"DEFINE_GLOBAL", "g"};
        case OP_GET_BUILTIN: return {"GET_BUILTIN", "i"};
        case OP_GET_PROP: return {"GET_PROP", "nc"};
        case OP_SET_PROP: return {"SET_PROP", "nc"};
        case OP_GET_INDEX: return {"GET_INDEX", ""};
        case OP_SET_INDEX: return {"SET_INDEX", ""};
        case OP_GET_SUPER: return {"GET_SUPER", "n"};
        case OP_ADD: return {"ADD", ""};
        case OP_SUB: return {"SUB", ""};
        case OP_MUL: return {"MUL", ""};
        case OP_DIV: return {"DIV", ""};
        case OP_MOD: return {"MOD", ""};
        case OP_POW: return {"POW", ""};
        case OP_NEG: return {"NEG", ""};
        case OP_NOT: return {"NOT", ""};
        case OP_BNOT: return {"BNOT", ""};
        case OP_BAND: return {"BAND", ""};
        case OP_BOR: return {"BOR", ""};
        case OP_BXOR: return {"BXOR", ""};
        case OP_SHL: return {"SHL", ""};
        case OP_SHR: return {"SHR", ""};
        case OP_EQ: return {"EQ", ""};
        case OP_NE: return {"NE", ""};
        case OP_LT: return {"LT", ""};
        case OP_LE: return {"LE", ""};
        case OP_GT: return {"GT", ""};
        case OP_GE: return {"GE", ""};
        case OP_IN: return {"IN", ""};
        case OP_IS: return {"IS", "k"};
        case OP_JUMP: return {"JUMP", "f"};
        case OP_JUMP_IF_FALSE: return {"JUMP_IF_FALSE", "f"};
        case OP_JUMP_IF_TRUE: return {"JUMP_IF_TRUE", "f"};
        case OP_JUMP_IF_FALSE_KEEP: return {"JUMP_IF_FALSE_KEEP", "f"};
        case OP_JUMP_IF_TRUE_KEEP: return {"JUMP_IF_TRUE_KEEP", "f"};
        case OP_JUMP_IF_NOT_NULL_KEEP: return {"JUMP_IF_NOT_NULL_KEEP", "f"};
        case OP_JUMP_IF_NULL_KEEP: return {"JUMP_IF_NULL_KEEP", "f"};
        case OP_LOOP: return {"LOOP", "B"};
        case OP_CALL: return {"CALL", "b"};
        case OP_CALL_NAMED: return {"CALL_NAMED", "bk"};
        case OP_CALL_SPREAD: return {"CALL_SPREAD", ""};
        case OP_INVOKE: return {"INVOKE", "nbi"};
        case OP_SUPER_INVOKE: return {"SUPER_INVOKE", "nb"};
        case OP_SUPER_CALL: return {"SUPER_CALL", "b"};
        case OP_CLOSURE: return {"CLOSURE", "k"}; // + upvalues (ver abajo)
        case OP_CLOSE_UPVALUE: return {"CLOSE_UPVALUE", ""};
        case OP_RETURN: return {"RETURN", ""};
        case OP_CLASS: return {"CLASS", "nb"};
        case OP_INHERIT: return {"INHERIT", ""};
        case OP_METHOD: return {"METHOD", "nb"};
        case OP_FIELD: return {"FIELD", "nb"};
        case OP_IMPLEMENTS: return {"IMPLEMENTS", "n"};
        case OP_LIST: return {"LIST", "i"};
        case OP_LIST_APPEND: return {"LIST_APPEND", ""};
        case OP_LIST_EXTEND: return {"LIST_EXTEND", ""};
        case OP_MAP: return {"MAP", "i"};
        case OP_RANGE: return {"RANGE", "b"};
        case OP_RANGE_STEP: return {"RANGE_STEP", ""};
        case OP_ITER_PREP: return {"ITER_PREP", ""};
        case OP_ITER_NEXT: return {"ITER_NEXT", "sf"};
        case OP_FOR_CHECK: return {"FOR_CHECK", "sbf"};
        case OP_FOR_INCR: return {"FOR_INCR", "s"};
        case OP_REPEAT_PREP: return {"REPEAT_PREP", ""};
        case OP_REPEAT_NEXT: return {"REPEAT_NEXT", "sf"};
        case OP_TRY: return {"TRY", "f"};
        case OP_END_TRY: return {"END_TRY", ""};
        case OP_THROW: return {"THROW", ""};
        case OP_AWAIT: return {"AWAIT", ""};
        case OP_EMIT: return {"EMIT", "b"};
        case OP_ON: return {"ON", "b"};
        case OP_ENUM: return {"ENUM", "ni"};
        case OP_EVENT: return {"EVENT", "nk"};
        case OP_IMPORT: return {"IMPORT", "k"};
        case OP_IMPORT_NAME: return {"IMPORT_NAME", "n"};
        case OP_TO_STRING: return {"TO_STRING", ""};
        case OP_CONCAT: return {"CONCAT", "i"};
        case OP_CHECK_TYPE: return {"CHECK_TYPE", "skk"};
        case OP_DEFAULT_ARG: return {"DEFAULT_ARG", "sf"};
        case OP_MATCH_TEST: return {"MATCH_TEST", ""};
        case OP_SUPER_INIT_AUTO: return {"SUPER_INIT_AUTO", ""};
        case OP_CHECK_RETURN: return {"CHECK_RETURN", "k"};
        case OP_SLIDE: return {"SLIDE", "b"};
        case OP_JUMP_IF_NOT_LT: return {"JUMP_IF_NOT_LT", "f"};
        case OP_JUMP_IF_NOT_LE: return {"JUMP_IF_NOT_LE", "f"};
        case OP_JUMP_IF_NOT_GT: return {"JUMP_IF_NOT_GT", "f"};
        case OP_JUMP_IF_NOT_GE: return {"JUMP_IF_NOT_GE", "f"};
        case OP_JUMP_IF_NOT_EQ: return {"JUMP_IF_NOT_EQ", "f"};
        case OP_JUMP_IF_NOT_NE: return {"JUMP_IF_NOT_NE", "f"};
        case OP_FOR_NEXT: return {"FOR_NEXT", "sbB"};
        case OP_CHECK_NUMBER: return {"CHECK_NUMBER", "sk"};
        case OP_SET_LOCAL_POP: return {"SET_LOCAL_POP", "s"};
        case OP_FOR_CHECK_V: return {"FOR_CHECK_V", "sbsf"};
        case OP_FOR_NEXT_V: return {"FOR_NEXT_V", "sbsB"};
        case OP_COUNT: break;
    }
    return {nullptr, ""};
}

int sizeOf(char k) { return k == 'b' || k == 'c' ? 1 : 2; }

std::string shortValue(VM& vm, const Value& v) {
    if (isObjType(v, OT::Function)) {
        auto* f = static_cast<ObjFunction*>(v.o);
        return "<función " + (f->name ? f->name->str : std::string("anónima")) + ">";
    }
    std::string s = vm.toString(v, true);
    for (auto& c : s) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    if (s.size() > 48) s = s.substr(0, 45) + "...";
    return s;
}

std::string functionTitle(ObjFunction* fn) {
    std::string name = fn->name ? fn->name->str : "<script>";
    if (fn->className) name = fn->className->str + "." + name;
    return name;
}

void disasmOne(VM& vm, ObjFunction* fn, std::string& out, std::vector<ObjFunction*>& nested) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "== %s ==  %zu bytes · %zu constantes · %d huecos · %d parámetro(s)%s\n", functionTitle(fn).c_str(),
                  fn->code.size(), fn->constants.size(), fn->maxSlots, fn->arity, fn->isAsync ? " · async" : "");
    out += buf;
    const auto& code = fn->code;
    size_t pc = 0;
    int lastLine = -1;
    while (pc < code.size()) {
        size_t at = pc;
        uint8_t op = code[pc++];
        OpInfo oi = op < OP_COUNT ? info((Op)op) : OpInfo{nullptr, ""};
        if (!oi.name) {
            std::snprintf(buf, sizeof buf, "%6zu        ?? código desconocido %u (el resto no se puede leer)\n", at, op);
            out += buf;
            return;
        }
        int line = fn->locAt(at).line;
        std::string lineCol = line != lastLine ? std::to_string(line) : "|";
        lastLine = line;
        std::string args, note;
        bool truncated = false;
        for (const char* k = oi.operands; *k; k++) {
            int n = sizeOf(*k);
            if (pc + n > code.size()) { truncated = true; break; }
            unsigned v = n == 1 ? code[pc] : (unsigned)(code[pc] << 8 | code[pc + 1]);
            pc += n;
            if (!args.empty()) args += ' ';
            switch (*k) {
                case 'k':
                case 'n':
                    args += "#" + std::to_string(v);
                    if (v < fn->constants.size()) {
                        note += (note.empty() ? "" : "  ") + shortValue(vm, fn->constants[v]);
                        if (isObjType(fn->constants[v], OT::Function)) nested.push_back(static_cast<ObjFunction*>(fn->constants[v].o));
                    }
                    break;
                case 's': args += "s" + std::to_string(v); break;
                case 'u': args += "up" + std::to_string(v); break;
                case 'g':
                    args += "g" + std::to_string(v);
                    if (fn->module && v < fn->module->globalNames.size() && fn->module->globalNames[v])
                        note += (note.empty() ? "" : "  ") + fn->module->globalNames[v]->str;
                    break;
                case 'c': args += "[" + std::to_string(v) + "]"; break;
                case 'f': break; // se escribe al final (el destino depende de todos los operandos)
                case 'B': break;
                default: args += std::to_string(v); break;
            }
        }
        if (truncated) { out += "        ?? instrucción cortada al final del código\n"; return; }
        // destinos de los saltos
        size_t p2 = at + 1;
        for (const char* k = oi.operands; *k; k++) {
            int n = sizeOf(*k);
            unsigned v = n == 1 ? code[p2] : (unsigned)(code[p2] << 8 | code[p2 + 1]);
            p2 += n;
            if (*k == 'f') args += (args.empty() ? "" : " ") + std::string("→ ") + std::to_string(pc + v);
            if (*k == 'B') args += (args.empty() ? "" : " ") + std::string("↑ ") + std::to_string(pc - v);
        }
        if (op == OP_CLOSURE) {
            ObjFunction* inner = nullptr;
            unsigned k = (unsigned)(code[at + 1] << 8 | code[at + 2]);
            if (k < fn->constants.size() && isObjType(fn->constants[k], OT::Function)) inner = static_cast<ObjFunction*>(fn->constants[k].o);
            int ups = inner ? inner->upvalueCount : 0;
            for (int u = 0; u < ups && pc + 3 <= code.size(); u++) {
                bool local = code[pc];
                unsigned idx = (unsigned)(code[pc + 1] << 8 | code[pc + 2]);
                pc += 3;
                args += std::string(" ") + (local ? "s" : "up") + std::to_string(idx);
            }
        }
        std::snprintf(buf, sizeof buf, "%6zu %5s  %-22s %-18s%s%s\n", at, lineCol.c_str(), oi.name, args.c_str(), note.empty() ? "" : "; ",
                      note.c_str());
        out += buf;
    }
}

} // namespace

int operandBytes(uint8_t op) {
    if (op >= OP_COUNT || op == OP_CLOSURE) return -1;
    OpInfo oi = info((Op)op);
    if (!oi.name) return -1;
    int n = 0;
    for (const char* k = oi.operands; *k; k++) n += sizeOf(*k);
    return n;
}

const char* opName(Op op) {
    OpInfo oi = op < OP_COUNT ? info(op) : OpInfo{nullptr, ""};
    return oi.name ? oi.name : "?";
}

std::string disassemble(VM& vm, ObjFunction* fn, bool recursive) {
    std::string out;
    std::vector<ObjFunction*> pending{fn};
    std::vector<ObjFunction*> done;
    while (!pending.empty()) {
        ObjFunction* f = pending.front();
        pending.erase(pending.begin());
        if (std::find(done.begin(), done.end(), f) != done.end()) continue;
        done.push_back(f);
        if (!out.empty()) out += "\n";
        std::vector<ObjFunction*> nested;
        disasmOne(vm, f, out, nested);
        if (recursive) pending.insert(pending.end(), nested.begin(), nested.end());
    }
    return out;
}

} // namespace hz
