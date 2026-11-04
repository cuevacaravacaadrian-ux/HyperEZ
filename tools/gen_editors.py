"""Genera el soporte de HyperEZ para otros editores a partir de la gramática de VS Code:

    editors/sublime/HyperEZ.tmLanguage          Sublime Text, TextMate y los editores que leen gramáticas TextMate
    editors/notepad++/HyperEZ.xml              Notepad++ (Lenguaje → Definir tu idioma → Importar)
    editors/vim/syntax/hyperez.vim + ftdetect  Vim y Neovim
    editors/highlightjs/hyperez.js             highlight.js (colorear código HyperEZ en páginas web)
    (el kit para GitHub Linguist, en editors/linguist/, lo genera tools/linguist_kit.py)

    python tools/gen_editors.py           (después de cambiar la gramática con editors/vscode/tools/gen_grammar.py)
"""
import json
import os
import plistlib
import sys

for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GRAMMAR = os.path.join(ROOT, "editors", "vscode", "syntaxes", "hyperez.tmLanguage.json")

KEYWORDS = ["var", "let", "const", "function", "return", "if", "else", "for", "foreach", "in", "while", "repeat", "break",
            "continue", "class", "struct", "constructor", "this", "super", "extends", "implements", "interface", "enum",
            "import", "from", "as", "public", "private", "protected", "static", "new", "try", "catch", "finally", "throw",
            "async", "await", "match", "event", "on", "once", "emit", "step", "get", "set", "and", "or", "not", "is"]
CONSTANTS = ["true", "false", "null"]
TYPES = ["number", "int", "text", "bool", "list", "map", "set", "any", "void", "function", "Task", "Range", "Event"]
BUILTINS = ["print", "println", "write", "input", "len", "typeof", "str", "toNumber", "toInt", "assert", "assertEqual", "assertThrows",
            "test", "exit", "range", "time", "wait", "spawn", "abs", "sqrt", "pow", "floor", "ceil", "round", "min", "max", "clamp",
            "random", "randomInt", "randomChoice", "randomSeed", "Console", "File", "System", "JSON", "Path", "Time", "Process",
            "Http", "Regex", "Bytes", "Hash", "Matrix", "Image", "Vec2", "Vec3", "Color", "Error"]


def write(rel, text):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    data = text if isinstance(text, bytes) else text.encode("utf-8")
    with open(path, "wb") as f:
        f.write(data)
    return rel


def sublime():
    g = json.load(open(GRAMMAR, encoding="utf-8"))
    g.pop("$schema", None)
    g["uuid"] = "6f1c6f8e-7b3a-4c2e-9d2f-48a1a5c0f7e1"
    return write("editors/sublime/HyperEZ.tmLanguage", plistlib.dumps(g, sort_keys=False))


def notepadpp():
    kw = " ".join(KEYWORDS)
    xml = f"""<?xml version="1.0" encoding="UTF-8" ?>
<!-- HyperEZ para Notepad++: Lenguaje → Definir tu idioma… → Importar… y elige este archivo. Generado por tools/gen_editors.py -->
<NotepadPlus>
    <UserLang name="HyperEZ" ext="hez hz" udlVersion="2.1">
        <Settings>
            <Global caseIgnored="no" allowFoldOfComments="yes" foldCompact="no" forcePureLC="0" decimalSeparator="0" />
            <Prefix Keywords1="no" Keywords2="no" Keywords3="no" Keywords4="no" Keywords5="no" Keywords6="no" Keywords7="no" Keywords8="no" />
        </Settings>
        <KeywordLists>
            <Keywords name="Comments">00// 01 02 03/* 04*/</Keywords>
            <Keywords name="Numbers, prefix1">0x 0b 0o</Keywords>
            <Keywords name="Numbers, prefix2"></Keywords>
            <Keywords name="Numbers, extras1">A B C D E F a b c d e f _</Keywords>
            <Keywords name="Numbers, extras2"></Keywords>
            <Keywords name="Numbers, suffix1"></Keywords>
            <Keywords name="Numbers, suffix2"></Keywords>
            <Keywords name="Numbers, range"></Keywords>
            <Keywords name="Operators1">( ) [ ] , ; : + - * / % = &lt; &gt; ! ? . | &amp; ^ ~</Keywords>
            <Keywords name="Operators2"></Keywords>
            <Keywords name="Folders in code1, open">{{</Keywords>
            <Keywords name="Folders in code1, middle"></Keywords>
            <Keywords name="Folders in code1, close">}}</Keywords>
            <Keywords name="Folders in code2, open"></Keywords>
            <Keywords name="Folders in code2, middle"></Keywords>
            <Keywords name="Folders in code2, close"></Keywords>
            <Keywords name="Folders in comment, open"></Keywords>
            <Keywords name="Folders in comment, middle"></Keywords>
            <Keywords name="Folders in comment, close"></Keywords>
            <Keywords name="Keywords1">{kw}</Keywords>
            <Keywords name="Keywords2">{" ".join(CONSTANTS)}</Keywords>
            <Keywords name="Keywords3">{" ".join(TYPES)}</Keywords>
            <Keywords name="Keywords4">{" ".join(BUILTINS)}</Keywords>
            <Keywords name="Keywords5"></Keywords>
            <Keywords name="Keywords6"></Keywords>
            <Keywords name="Keywords7"></Keywords>
            <Keywords name="Keywords8"></Keywords>
            <Keywords name="Delimiters">00&quot; 01\\ 02&quot; 03&apos; 04\\ 05&apos; 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23</Keywords>
        </KeywordLists>
        <Styles>
            <WordsStyle name="DEFAULT" fgColor="1F2328" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="COMMENTS" fgColor="008000" bgColor="FFFFFF" fontStyle="2" nesting="0" />
            <WordsStyle name="LINE COMMENTS" fgColor="008000" bgColor="FFFFFF" fontStyle="2" nesting="0" />
            <WordsStyle name="NUMBERS" fgColor="098658" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="KEYWORDS1" fgColor="0000DC" bgColor="FFFFFF" fontStyle="1" nesting="0" />
            <WordsStyle name="KEYWORDS2" fgColor="0000DC" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="KEYWORDS3" fgColor="267F99" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="KEYWORDS4" fgColor="795E26" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="KEYWORDS5" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="KEYWORDS6" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="KEYWORDS7" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="KEYWORDS8" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="OPERATORS" fgColor="1F2328" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="FOLDER IN CODE1" fgColor="1F2328" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="FOLDER IN CODE2" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="FOLDER IN COMMENT" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS1" fgColor="A31515" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS2" fgColor="A31515" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS3" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS4" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS5" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS6" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS7" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
            <WordsStyle name="DELIMITERS8" fgColor="000000" bgColor="FFFFFF" fontStyle="0" nesting="0" />
        </Styles>
    </UserLang>
</NotepadPlus>
"""
    return write("editors/notepad++/HyperEZ.xml", xml)


def vim():
    syn = f'''" Vim: colores de HyperEZ (.hez y .hz). Copia syntax/ y ftdetect/ en ~/.vim (o ~/.config/nvim). Generado por tools/gen_editors.py
if exists("b:current_syntax")
  finish
endif

syn keyword hzKeyword {" ".join(KEYWORDS)}
syn keyword hzConstant {" ".join(CONSTANTS)}
syn keyword hzType {" ".join(TYPES)}
syn keyword hzBuiltin {" ".join(BUILTINS)}
syn match hzNumber "\\<\\(0[xX][0-9a-fA-F_]\\+\\|0[bB][01_]\\+\\|[0-9][0-9_]*\\(\\.[0-9_]\\+\\)\\=\\([eE][+-]\\=[0-9]\\+\\)\\=\\)\\>"
syn match hzClassName "\\<[A-Z][A-Za-z0-9_]*\\>"
syn region hzInterp matchgroup=hzInterpDelim start="${{" end="}}" contained contains=TOP
syn region hzString start=+"+ skip=+\\\\.+ end=+"+ contains=hzInterp,hzEscape
syn region hzString start=+'+ skip=+\\\\.+ end=+'+ contains=hzInterp,hzEscape
syn region hzString start=+"""+ end=+"""+ contains=hzInterp
syn region hzRawString start=+r"+ end=+"+
syn region hzRawString start=+r'+ end=+'+
syn match hzEscape "\\\\." contained
syn match hzDocComment "///.*$"
syn match hzComment "//.*$" contains=hzTodo
syn region hzComment start="/\\*" end="\\*/" contains=hzTodo
syn keyword hzTodo TODO FIXME PENDIENTE contained
syn match hzOperator "|>\\|=>\\|->\\|\\.\\.=\\|\\.\\.\\|??\\|?\\."

hi def link hzKeyword Keyword
hi def link hzConstant Constant
hi def link hzType Type
hi def link hzClassName Type
hi def link hzBuiltin Function
hi def link hzNumber Number
hi def link hzString String
hi def link hzRawString String
hi def link hzEscape SpecialChar
hi def link hzInterpDelim Special
hi def link hzComment Comment
hi def link hzDocComment SpecialComment
hi def link hzTodo Todo
hi def link hzOperator Operator

let b:current_syntax = "hyperez"
'''
    det = 'au BufRead,BufNewFile *.hez,*.hz setfiletype hyperez\n'
    return [write("editors/vim/syntax/hyperez.vim", syn), write("editors/vim/ftdetect/hyperez.vim", det)]


def highlightjs():
    js = """/*
 * HyperEZ para highlight.js (https://highlightjs.org): colorea código HyperEZ en páginas web.
 *   <script src="highlight.min.js"></script>
 *   <script src="hyperez.js"></script>
 *   <script>hljs.highlightAll()</script>      ...  <pre><code class="language-hz">print("hola")</code></pre>
 * Generado por tools/gen_editors.py
 */
(function () {
  function hyperez(hljs) {
    const INTERP = { className: 'subst', begin: /\\$\\{/, end: /\\}/, keywords: {} };
    const STRING = {
      className: 'string',
      variants: [
        { begin: /r"/, end: /"/ }, { begin: /r'/, end: /'/ },
        { begin: /\"\"\"/, end: /\"\"\"/, contains: [INTERP] },
        { begin: /"/, end: /"/, contains: [hljs.BACKSLASH_ESCAPE, INTERP] },
        { begin: /'/, end: /'/, contains: [hljs.BACKSLASH_ESCAPE, INTERP] }
      ]
    };
    const KEYWORDS = {
      keyword: %KEYWORDS%,
      literal: %CONSTANTS%,
      type: %TYPES%,
      built_in: %BUILTINS%
    };
    INTERP.keywords = KEYWORDS;
    INTERP.contains = [STRING, hljs.C_NUMBER_MODE];
    return {
      name: 'HyperEZ',
      aliases: ['hz', 'hez', 'hyperez'],
      keywords: KEYWORDS,
      contains: [
        hljs.COMMENT('///', '$', { className: 'doctag' }),
        hljs.C_LINE_COMMENT_MODE,
        hljs.C_BLOCK_COMMENT_MODE,
        STRING,
        { className: 'number', begin: /\\b(0[xX][0-9a-fA-F_]+|0[bB][01_]+|\\d[\\d_]*(\\.\\d[\\d_]*)?([eE][+-]?\\d+)?)\\b/ },
        { className: 'title.class', begin: /\\b[A-Z][A-Za-z0-9_]*\\b/ },
        { className: 'title.function', begin: /\\b[a-z_][A-Za-z0-9_]*(?=\\s*\\()/ },
        { className: 'operator', begin: /\\|>|=>|->|\\.\\.=?|\\?\\?|\\?\\./ }
      ]
    };
  }
  if (typeof module !== 'undefined' && module.exports) module.exports = hyperez;
  if (typeof hljs !== 'undefined') hljs.registerLanguage('hyperez', hyperez);
})();
"""
    js = (js.replace("%KEYWORDS%", json.dumps(" ".join(KEYWORDS)))
            .replace("%CONSTANTS%", json.dumps(" ".join(CONSTANTS)))
            .replace("%TYPES%", json.dumps(" ".join(TYPES)))
            .replace("%BUILTINS%", json.dumps(" ".join(BUILTINS))))
    return write("editors/highlightjs/hyperez.js", js)


def main():
    hechos = [sublime(), notepadpp(), *vim(), highlightjs()]
    for h in hechos:
        print("  " + h)


if __name__ == "__main__":
    main()
