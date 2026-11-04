"""Genera las gramáticas TextMate de HyperEZ (syntaxes/*.json).

Se mantiene en Python porque las expresiones regulares son más legibles aquí que
escapadas dentro de JSON. Ejecuta:  python tools/gen_grammar.py
"""
import json
import os
import sys

# La consola de Windows (y la de GitHub Actions) puede no ser UTF-8: los ✓ y las tildes no deben romper el script
for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "syntaxes")

ID = r"[\p{L}_][\p{L}\p{N}_]*"
TYPE = r"(?:[\p{L}_][\p{L}\p{N}_.]*)(?:<[^<>()=]*(?:<[^<>()=]*>[^<>()=]*)*>)?\??"
PRIM = ("number|int|float|double|text|string|char|bool|boolean|list|array|map|dict|set|any|object|void|null|"
        "function|Function|Task|Promise|Range|Event|Subscription")
BUILTIN_FN = ("print|println|write|input|len|typeof|str|toNumber|toInt|assert|assertEqual|assertThrows|test|exit|range|time|now|"
              "wait|nextFrame|spawn|off|abs|sqrt|pow|floor|ceil|round|sign|min|max|clamp|lerp|sin|cos|tan|asin|acos|atan|atan2|"
              "hypot|exp|log|log10|radians|degrees|random|randomInt|randomChoice|randomSeed|remap|wrap|approach|smoothstep|"
              "chance|distance|isNaN|awaitAll|awaitAny|parseInt|gcd|lcm|factorial|isPrime|primes|divisors|cbrt|trunc|fract|"
              "sinh|cosh|tanh|randomGaussian|noise|chr|ord")
BUILTIN_CLS = ("Console|File|System|JSON|Vec2|Vec3|Color|Error|EventData|HyperCore|HyperMath|Process|Path|Time|Bytes|Hash|"
               "Base64|Regex|Http|Matrix|Image|ProcessHandle")


def name(n):
    return {"name": n}


def string_rule(scope, begin, end):
    return {
        "name": scope, "begin": begin, "end": end,
        "beginCaptures": {"0": name("punctuation.definition.string.begin.hyperez")},
        "endCaptures": {"0": name("punctuation.definition.string.end.hyperez")},
        "patterns": [{"include": "#escapes"}, {"include": "#interpolation"}],
    }


def typed(*groups):
    return {"patterns": [{"include": "#type-name"}]}


grammar = {
    "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
    "name": "HyperEZ",
    "scopeName": "source.hyperez",
    "fileTypes": ["hz", "hez"],
    "patterns": [{"include": "#statements"}],
    "repository": {
        "statements": {"patterns": [{"include": "#" + r} for r in [
            "comments", "strings", "numbers", "import", "function-decl", "class-decl", "inherit", "event-decl",
            "event-use", "accessor", "var-decl", "return-type", "keywords", "constants", "builtins", "calls",
            "members", "identifiers", "operators", "punctuation"]]},
        "comments": {"patterns": [
            {"name": "comment.block.documentation.hyperez", "begin": r"/\*\*(?!/)", "end": r"\*/",
             "captures": {"0": name("punctuation.definition.comment.hyperez")}},
            {"name": "comment.block.hyperez", "begin": r"/\*", "end": r"\*/",
             "captures": {"0": name("punctuation.definition.comment.hyperez")}},
            {"name": "comment.line.triple-slash.documentation.hyperez", "begin": r"///", "end": "$",
             "beginCaptures": {"0": name("punctuation.definition.comment.hyperez")}},
            {"name": "comment.line.double-slash.hyperez", "begin": r"//", "end": "$",
             "beginCaptures": {"0": name("punctuation.definition.comment.hyperez")}},
        ]},
        "strings": {"patterns": [
            # r"..." y r'...': sin escapes ni interpolación (expresiones regulares, rutas de Windows)
            {"name": "string.quoted.raw.hyperez", "begin": r'\br"', "end": '"|$',
             "beginCaptures": {"0": name("punctuation.definition.string.begin.hyperez")},
             "endCaptures": {"0": name("punctuation.definition.string.end.hyperez")}},
            {"name": "string.quoted.raw.hyperez", "begin": r"\br'", "end": "'|$",
             "beginCaptures": {"0": name("punctuation.definition.string.begin.hyperez")},
             "endCaptures": {"0": name("punctuation.definition.string.end.hyperez")}},
            string_rule("string.quoted.triple.double.hyperez", '"""', '"""'),
            string_rule("string.quoted.triple.single.hyperez", "'''", "'''"),
            string_rule("string.quoted.double.hyperez", '"', '"|$'),
            string_rule("string.quoted.single.hyperez", "'", "'|$"),
        ]},
        "escapes": {"name": "constant.character.escape.hyperez", "match": r"\\(?:u\{[0-9a-fA-F]+\}|x[0-9a-fA-F]{2}|.)"},
        "interpolation": {
            "name": "meta.interpolation.hyperez", "contentName": "meta.embedded.line.hyperez",
            "begin": r"\$\{", "end": r"\}",
            "beginCaptures": {"0": name("punctuation.section.interpolation.begin.hyperez")},
            "endCaptures": {"0": name("punctuation.section.interpolation.end.hyperez")},
            "patterns": [{"include": "#nested-braces"}, {"include": "#statements"}],
        },
        "nested-braces": {
            "begin": r"\{", "end": r"\}",
            "beginCaptures": {"0": name("punctuation.section.braces.hyperez")},
            "endCaptures": {"0": name("punctuation.section.braces.hyperez")},
            "patterns": [{"include": "#nested-braces"}, {"include": "#statements"}],
        },
        "numbers": {"patterns": [
            {"name": "constant.numeric.hex.hyperez", "match": r"\b0[xX][0-9a-fA-F_]+\b"},
            {"name": "constant.numeric.binary.hyperez", "match": r"\b0[bB][01_]+\b"},
            {"name": "constant.numeric.decimal.hyperez",
             "match": r"(?<![\p{L}_])(?:\d[\d_]*(?:\.(?!\.)\d[\d_]*)?(?:[eE][+-]?\d+)?)\b"},
        ]},
        "import": {"patterns": [
            {"match": r"^\s*(from)\s+(\"[^\"]*\"|" + ID + r"(?:\." + ID + r")*)\s+(import)\b",
             "captures": {"1": name("keyword.control.import.hyperez"), "2": name("entity.name.namespace.hyperez"),
                          "3": name("keyword.control.import.hyperez")}},
            {"match": r"^\s*(import)\s+(\"[^\"]*\"|" + ID + r"(?:\." + ID + r")*)(?:\s+(as)\s+(" + ID + r"))?",
             "captures": {"1": name("keyword.control.import.hyperez"), "2": name("entity.name.namespace.hyperez"),
                          "3": name("keyword.control.as.hyperez"), "4": name("entity.name.namespace.hyperez")}},
            {"match": r"\b(as)\s+(" + ID + r")",
             "captures": {"1": name("keyword.control.as.hyperez"), "2": name("variable.other.readwrite.alias.hyperez")}},
        ]},
        "function-decl": {
            "begin": r"\b(?:(async)\s+)?(?:(static)\s+)?(function)\s+(" + ID + r")\s*(\()",
            "beginCaptures": {"1": name("storage.modifier.async.hyperez"), "2": name("storage.modifier.hyperez"),
                              "3": name("storage.type.function.hyperez"), "4": name("entity.name.function.hyperez"),
                              "5": name("punctuation.definition.parameters.begin.hyperez")},
            "end": r"\)", "endCaptures": {"0": name("punctuation.definition.parameters.end.hyperez")},
            "patterns": [{"include": "#parameters"}],
        },
        "parameters": {"patterns": [
            {"include": "#comments"}, {"include": "#strings"}, {"include": "#numbers"},
            {"match": r"(\.\.\.)?(" + ID + r")\s*(\?)?\s*(:)\s*(" + TYPE + ")",
             "captures": {"1": name("keyword.operator.spread.hyperez"), "2": name("variable.parameter.hyperez"),
                          "3": name("keyword.operator.optional.hyperez"), "4": name("punctuation.separator.type.hyperez"),
                          "5": typed()}},
            {"match": r"(\.\.\.)?(" + ID + r")(?=\s*(?:[,=)]|$))",
             "captures": {"1": name("keyword.operator.spread.hyperez"), "2": name("variable.parameter.hyperez")}},
            {"include": "#constants"}, {"include": "#operators"},
        ]},
        "type-name": {"patterns": [
            {"name": "support.type.primitive.hyperez", "match": r"\b(?:" + PRIM + r")\b"},
            {"name": "entity.name.type.hyperez", "match": ID},
            {"name": "punctuation.definition.typeparameters.hyperez", "match": r"[<>,]"},
            {"name": "keyword.operator.optional.hyperez", "match": r"\?"},
        ]},
        "return-type": {"match": r"(?<=\))\s*(:)\s*(" + TYPE + ")",
                        "captures": {"1": name("punctuation.separator.type.hyperez"), "2": typed()}},
        "class-decl": {"match": r"\b(class|struct|interface|enum)\s+(" + ID + ")",
                       "captures": {"1": name("storage.type.class.hyperez"), "2": name("entity.name.type.class.hyperez")}},
        "inherit": {"match": r"\b(extends|implements)\s+(" + ID + r"(?:\s*,\s*" + ID + r")*)",
                    "captures": {"1": name("storage.modifier.hyperez"),
                                 "2": {"patterns": [{"name": "entity.other.inherited-class.hyperez", "match": ID}]}}},
        "event-decl": {"match": r"\b(event)\s+(" + ID + ")",
                       "captures": {"1": name("storage.type.event.hyperez"), "2": name("entity.name.function.event.hyperez")}},
        "event-use": {"match": r"\b(on|once|emit)\s+(" + ID + r"(?:\." + ID + r")*)",
                      "captures": {"1": name("keyword.control.event.hyperez"), "2": name("entity.name.function.event.hyperez")}},
        "accessor": {"match": r"\b(get|set)\s+(" + ID + r")(?=\s*\()",
                     "captures": {"1": name("storage.type.property.hyperez"), "2": name("entity.name.function.accessor.hyperez")}},
        "var-decl": {"patterns": [
            {"match": r"\b(const)\s+(" + ID + r")(?:\s*(:)\s*(" + TYPE + "))?",
             "captures": {"1": name("storage.type.const.hyperez"), "2": name("variable.other.constant.hyperez"),
                          "3": name("punctuation.separator.type.hyperez"), "4": typed()}},
            {"match": r"\b(var|let)\s+(" + ID + r")(?:\s*(:)\s*(" + TYPE + "))?",
             "captures": {"1": name("storage.type.var.hyperez"), "2": name("variable.other.readwrite.hyperez"),
                          "3": name("punctuation.separator.type.hyperez"), "4": typed()}},
            {"match": r"\b(catch)\s+(" + ID + ")",
             "captures": {"1": name("keyword.control.trycatch.hyperez"), "2": name("variable.parameter.hyperez")}},
            {"match": r"\b(for|foreach)\s+(" + ID + r")(?:\s*,\s*(" + ID + r"))?\s+(in)\b",
             "captures": {"1": name("keyword.control.loop.hyperez"), "2": name("variable.other.readwrite.hyperez"),
                          "3": name("variable.other.readwrite.hyperez"), "4": name("keyword.control.loop.hyperez")}},
        ]},
        "keywords": {"patterns": [
            {"name": "keyword.control.conditional.hyperez", "match": r"\b(?:if|else|match)\b"},
            {"name": "keyword.control.loop.hyperez", "match": r"\b(?:for|foreach|in|while|repeat|break|continue)\b"},
            {"name": "keyword.control.loop.hyperez", "match": r"\bstep\b(?=\s*-?[\d(\p{L}_])"},
            {"name": "keyword.control.flow.hyperez", "match": r"\b(?:return|await|throw)\b"},
            {"name": "keyword.control.trycatch.hyperez", "match": r"\b(?:try|catch|finally)\b"},
            {"name": "keyword.control.event.hyperez", "match": r"\bpriority\b(?=\s*-?[\d(\p{L}_])"},
            {"name": "keyword.control.import.hyperez", "match": r"\bimport\b"},
            {"name": "keyword.operator.word.hyperez", "match": r"\b(?:and|or|not|is|new)\b"},
            {"name": "storage.modifier.hyperez", "match": r"\b(?:public|private|protected|static|async)\b"},
            {"name": "storage.type.hyperez", "match": r"\b(?:var|let|const|function|class|struct|interface|enum|event)\b"},
            {"name": "storage.type.constructor.hyperez", "match": r"\bconstructor\b"},
        ]},
        "constants": {"patterns": [
            {"name": "constant.language.boolean.hyperez", "match": r"\b(?:true|false)\b"},
            {"name": "constant.language.null.hyperez", "match": r"\bnull\b"},
            {"name": "variable.language.this.hyperez", "match": r"\b(?:this|super)\b"},
            {"name": "support.constant.math.hyperez", "match": r"\b(?:PI|TAU|E|INFINITY)\b"},
        ]},
        "builtins": {"patterns": [
            {"name": "support.class.builtin.hyperez", "match": r"\b(?:" + BUILTIN_CLS + r")\b"},
            {"name": "support.function.builtin.hyperez", "match": r"\b(?:" + BUILTIN_FN + r")\b(?=\s*\()"},
        ]},
        "calls": {"match": r"(" + ID + r")(?=\s*\()", "captures": {"1": name("entity.name.function.call.hyperez")}},
        "members": {"patterns": [
            {"match": r"(\??\.)(" + ID + r")(?=\s*\()",
             "captures": {"1": name("punctuation.accessor.hyperez"), "2": name("entity.name.function.member.hyperez")}},
            {"match": r"(\??\.)(\p{Lu}[\p{Lu}\p{N}_]+)\b",
             "captures": {"1": name("punctuation.accessor.hyperez"), "2": name("variable.other.constant.property.hyperez")}},
            {"match": r"(\??\.)(" + ID + r")",
             "captures": {"1": name("punctuation.accessor.hyperez"), "2": name("variable.other.property.hyperez")}},
        ]},
        "identifiers": {"patterns": [
            {"name": "variable.other.constant.hyperez", "match": r"\b\p{Lu}[\p{Lu}\p{N}_]+\b"},
            {"name": "entity.name.type.hyperez", "match": r"\b\p{Lu}[\p{L}\p{N}_]*\b"},
            {"name": "variable.other.readwrite.hyperez", "match": ID},
        ]},
        "operators": {"patterns": [
            {"name": "storage.type.function.arrow.hyperez", "match": r"=>|->"},
            {"name": "keyword.operator.range.hyperez", "match": r"\.\.=|\.\.\.|\.\."},
            {"name": "keyword.operator.nullish.hyperez", "match": r"\?\?=?|\?\."},
            {"name": "keyword.operator.assignment.compound.hyperez", "match": r"\+=|-=|\*=|/=|%="},
            {"name": "keyword.operator.comparison.hyperez", "match": r"==|!=|<=|>=|<|>"},
            {"name": "keyword.operator.logical.hyperez", "match": r"&&|\|\||!"},
            {"name": "keyword.operator.increment.hyperez", "match": r"\+\+|--"},
            {"name": "keyword.operator.arithmetic.hyperez", "match": r"\*\*|[-+*/%]"},
            {"name": "keyword.operator.bitwise.hyperez", "match": r"<<|>>|[&|^~]"},
            {"name": "keyword.operator.assignment.hyperez", "match": r"="},
            {"name": "keyword.operator.ternary.hyperez", "match": r"\?"},
            {"name": "keyword.operator.decorator.hyperez", "match": r"@"},
        ]},
        "punctuation": {"patterns": [
            {"name": "punctuation.separator.comma.hyperez", "match": ","},
            {"name": "punctuation.terminator.statement.hyperez", "match": ";"},
            {"name": "punctuation.separator.key-value.hyperez", "match": ":"},
            {"name": "punctuation.section.block.hyperez", "match": r"[{}]"},
            {"name": "meta.brace.round.hyperez", "match": r"[()]"},
            {"name": "meta.brace.square.hyperez", "match": r"[\[\]]"},
        ]},
    },
}

markdown = {
    "fileTypes": [],
    "injectionSelector": "L:text.html.markdown",
    "scopeName": "markdown.hyperez.codeblock",
    "patterns": [{"include": "#hyperez-code-block"}],
    "repository": {"hyperez-code-block": {
        "begin": r"(^|\G)(\s*)(`{3,}|~{3,})\s*(?i:(hyperez|hz|hez)((\s+|:|,|\{|\?)[^`]*)?$)",
        "name": "markup.fenced_code.block.markdown",
        "end": r"(^|\G)(\2|\s{0,3})(\3)\s*$",
        "beginCaptures": {"3": name("punctuation.definition.markdown"), "4": name("fenced_code.block.language.markdown"),
                          "5": name("fenced_code.block.language.attributes.markdown")},
        "endCaptures": {"3": name("punctuation.definition.markdown")},
        "patterns": [{"begin": r"(^|\G)(\s*)(.*)", "while": r"(^|\G)(?!\s*([`~]{3,})\s*$)",
                      "contentName": "meta.embedded.block.hyperez", "patterns": [{"include": "source.hyperez"}]}],
    }},
}

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for fname, data in (("hyperez.tmLanguage.json", grammar), ("hyperez.markdown.json", markdown)):
        with open(os.path.join(OUT, fname), "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            f.write("\n")
    print("gramáticas escritas en", os.path.normpath(OUT))
