// Expresiones regulares (sintaxis de JavaScript/ECMAScript): buscar, validar, reemplazar y dividir texto.
#include "lang/stdlib/native.h"

#include <algorithm>
#include <cstring>
#include <regex>

namespace hz {

struct RegexData {
    std::string pattern, flags;
    std::regex re;
    bool global = true;
};

static void freeRegex(void* p) { delete static_cast<RegexData*>(p); }
static RegexData& R(Value* args) { return *static_cast<RegexData*>(static_cast<ObjForeign*>(args[-1].o)->ptr); }

// Las posiciones se cuentan en caracteres (como en los textos de HyperEZ), no en bytes.
static double charIndex(const std::string& s, size_t byte) { return (double)utf8Length(std::string_view(s).substr(0, byte)); }

// base: posición en bytes desde la que se buscó (las de std::smatch son relativas a ella)
static Value matchToMap(VM& vm, const std::string& text, const std::smatch& m, size_t base = 0) {
    ObjMap* r = vm.newMap();
    TempRoot keep(vm, r);
    put(vm, r, "text", vm.str(m.str(0)));
    put(vm, r, "start", Value(charIndex(text, base + (size_t)m.position(0))));
    put(vm, r, "end", Value(charIndex(text, base + (size_t)(m.position(0) + m.length(0)))));
    ObjList* groups = vm.newList();
    put(vm, r, "groups", Value::object(groups));
    for (size_t i = 1; i < m.size(); i++) groups->items.push_back(m[i].matched ? vm.str(m.str(i)) : Value());
    return Value::object(r);
}

static std::regex compile(VM& vm, const std::string& pattern, const std::string& flags, const char* fname) {
    auto f = std::regex::ECMAScript;
    for (char c : flags) {
        if (c == 'i') f |= std::regex::icase;
        else if (c == 'm') f |= std::regex::multiline;
        else vm.raise(std::string(fname) + ": opción desconocida '" + c + "' (usa \"i\" para ignorar mayúsculas o \"m\" para varias líneas)");
    }
    try {
        return std::regex(pattern, f);
    } catch (const std::regex_error& e) {
        vm.raise(std::string(fname) + ": la expresión '" + pattern + "' no es válida (" + e.what() + ")");
    }
}

NATIVE(c_regex) {
    auto* d = new RegexData;
    d->pattern = argStr(vm, args, argc, 0, "Regex");
    d->flags = argStr(vm, args, argc, 1, "Regex", "");
    try {
        d->re = compile(vm, d->pattern, d->flags, "Regex");
    } catch (...) {
        delete d;
        throw;
    }
    return Value::object(vm.newForeign(static_cast<ObjClass*>(args[-1].o), d, freeRegex));
}
NATIVE(re_pattern) { UNUSED; return vm.str(R(args).pattern); }
NATIVE(re_test) {
    std::string t = argStr(vm, args, argc, 0, "Regex.test");
    return Value::boolean(std::regex_search(t, R(args).re));
}
NATIVE(re_matches) {
    std::string t = argStr(vm, args, argc, 0, "Regex.matches");
    return Value::boolean(std::regex_match(t, R(args).re));
}
NATIVE(re_find) {
    std::string t = argStr(vm, args, argc, 0, "Regex.find");
    size_t from = utf8Offset(t, (size_t)std::max(0.0, argNum(vm, args, argc, 1, "Regex.find", 0)));
    std::smatch m;
    auto begin = t.cbegin() + (std::ptrdiff_t)std::min(from, t.size());
    if (!std::regex_search(begin, t.cend(), m, R(args).re)) return Value();
    return matchToMap(vm, t, m, std::min(from, t.size()));
}
NATIVE(re_findAll) {
    std::string t = argStr(vm, args, argc, 0, "Regex.findAll");
    ObjList* l = vm.newList();
    TempRoot keep(vm, l);
    for (auto it = std::sregex_iterator(t.begin(), t.end(), R(args).re); it != std::sregex_iterator(); ++it)
        l->items.push_back(matchToMap(vm, t, *it));
    return Value::object(l);
}
NATIVE(re_replace) {
    std::string t = argStr(vm, args, argc, 0, "Regex.replace");
    Value with = optArg(args, argc, 1);
    bool all = argBool(args, argc, 2, true);
    auto& re = R(args).re;
    if (isString(with)) {
        return vm.str(std::regex_replace(t, re, asString(with)->str, all ? std::regex_constants::format_default
                                                                      : std::regex_constants::format_first_only));
    }
    // reemplazo con una función: recibe {text, start, end, groups} y devuelve el texto nuevo
    std::string out;
    size_t last = 0;
    for (auto it = std::sregex_iterator(t.begin(), t.end(), re); it != std::sregex_iterator(); ++it) {
        auto& m = *it;
        out.append(t, last, (size_t)m.position(0) - last);
        Value mv = matchToMap(vm, t, m);
        vm.tempRoots.push_back(mv);
        Value r = vm.call(with, 1, &mv);
        vm.tempRoots.pop_back();
        out += isString(r) ? asString(r)->str : vm.toString(r);
        last = (size_t)(m.position(0) + m.length(0));
        if (!all) break;
    }
    out.append(t, last, std::string::npos);
    return vm.str(out);
}
NATIVE(re_split) {
    std::string t = argStr(vm, args, argc, 0, "Regex.split");
    ObjList* l = vm.newList();
    TempRoot keep(vm, l);
    std::sregex_token_iterator it(t.begin(), t.end(), R(args).re, -1), end;
    for (; it != end; ++it) l->items.push_back(vm.str(it->str()));
    if (t.empty()) l->items.clear();
    return Value::object(l);
}
NATIVE(re_toString) { UNUSED; return vm.str("Regex(\"" + R(args).pattern + "\"" + (R(args).flags.empty() ? "" : ", \"" + R(args).flags + "\"") + ")"); }
NATIVE(re_escape) {
    std::string s = argStr(vm, args, argc, 0, "Regex.escape"), r;
    for (char c : s) {
        if (std::strchr("\\^$.|?*+()[]{}/-", c)) r += '\\';
        r += c;
    }
    return vm.str(r);
}

// Atajos en los textos: "a1b2".matches(r"\w+"), "x=1".replaceAll(r"\d", "#")
NATIVE(t_matches) {
    const std::string& s = asString(args[-1])->str;
    std::string p = argStr(vm, args, argc, 0, "text.matches");
    return Value::boolean(std::regex_search(s, compile(vm, p, argStr(vm, args, argc, 1, "text.matches", ""), "text.matches")));
}
NATIVE(t_replaceRegex) {
    const std::string& s = asString(args[-1])->str;
    std::string p = argStr(vm, args, argc, 0, "text.replaceRegex");
    std::string w = argStr(vm, args, argc, 1, "text.replaceRegex");
    return vm.str(std::regex_replace(s, compile(vm, p, "", "text.replaceRegex"), w));
}

void registerRegex(VM& vm) {
    ObjClass* c = vm.nativeClass("Regex",
        "Expresión regular (sintaxis de JavaScript) para buscar patrones en textos. Usa textos r\"...\" para no duplicar las barras: Regex(r\"\\d+\").",
        c_regex, "(patron: text, opciones?: text = \"\")");
    vm.defineClassProp(c, "pattern", "text", "El patrón original.", re_pattern);
    vm.defineMethod(c, "test", "(texto: text): bool", "¿Aparece el patrón en el texto?", re_test);
    vm.defineMethod(c, "matches", "(texto: text): bool", "¿El texto completo encaja con el patrón?", re_matches);
    vm.defineMethod(c, "find", "(texto: text, desde?: number = 0): map?",
                    "Primera coincidencia: {text, start, end, groups} (posiciones en caracteres) o null.", re_find);
    vm.defineMethod(c, "findAll", "(texto: text): list<map>", "Todas las coincidencias.", re_findAll);
    vm.defineMethod(c, "replace", "(texto: text, reemplazo: text|function, todas?: bool = true): text",
                    "Reemplaza las coincidencias. En el texto de reemplazo, $1, $2... son los grupos; también puede ser una función que recibe la coincidencia.",
                    re_replace);
    vm.defineMethod(c, "split", "(texto: text): list<text>", "Divide el texto por las coincidencias.", re_split);
    vm.defineMethod(c, "toString", "(): text", "Descripción.", re_toString);
    vm.defineStatic(c, "escape", "(texto: text): text", "Escapa los caracteres especiales para buscar el texto tal cual.", re_escape);

    vm.defineMethod(vm.textClass, "matches", "(patron: text, opciones?: text = \"\"): bool", "¿Aparece la expresión regular en el texto?", t_matches);
    vm.defineMethod(vm.textClass, "replaceRegex", "(patron: text, reemplazo: text): text",
                    "Reemplaza todas las coincidencias de una expresión regular ($1, $2... son los grupos).", t_replaceRegex);
}

} // namespace hz
