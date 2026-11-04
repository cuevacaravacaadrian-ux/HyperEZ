"""Genera la documentación que sale del propio código, para que nunca se quede desfasada.

    python tools/gen_docs.py <hyper> [--check]

  docs/biblioteca/API.md     referencia completa de la API (hyper docs)
  docs/errores.md            catálogo de códigos de error y aviso (del compilador)
  docs/interno/bytecode.md   instrucciones de la máquina virtual (de src/lang/compiler/bytecode.h)

Con --check no escribe nada: falla si algún archivo no está al día (lo usa ctest).
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CATEGORIES = [
    ("E00", "Léxico: caracteres, números y textos mal escritos"),
    ("E01", "Sintaxis: la estructura del código"),
    ("E02", "Nombres y declaraciones"),
    ("E03", "Tipos y expresiones"),
    ("E04", "Clases, objetos y miembros"),
    ("E05", "Funciones, módulos y control de flujo"),
    ("E09", "Límites del compilador"),
    ("W0", "Avisos (el programa se ejecuta igualmente)"),
]


def split_args(s):
    """Divide los argumentos de una llamada C++ al nivel superior (respeta paréntesis y textos)."""
    args, depth, cur, i, quote = [], 0, "", 0, None
    while i < len(s):
        c = s[i]
        if quote:
            cur += c
            if c == "\\" and i + 1 < len(s):
                cur += s[i + 1]
                i += 2
                continue
            if c == quote:
                quote = None
        elif c in "\"'":
            quote = c
            cur += c
        elif c in "([{":
            depth += 1
            cur += c
        elif c in ")]}":
            if depth == 0:
                args.append(cur.strip())
                return args
            depth -= 1
            cur += c
        elif c == "," and depth == 0:
            args.append(cur.strip())
            cur = ""
        else:
            cur += c
        i += 1
    args.append(cur.strip())
    return args


def readable(expr):
    """Texto legible de una expresión C++: los literales tal cual y el resto como «…»."""
    parts = re.split(r'("(?:[^"\\]|\\.)*")', expr)
    out = ""
    for p in parts:
        if p.startswith('"') and p.endswith('"') and len(p) >= 2:
            out += bytes(p[1:-1], "utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")
        elif p.strip(" +\n\t()"):
            if not out.endswith("…"):
                out += "…"
    return re.sub(r"\s+", " ", out).strip()


def error_catalog():
    codes = {}
    for dirpath, _, files in os.walk(os.path.join(ROOT, "src", "lang")):
        for f in sorted(files):
            if not f.endswith(".cpp"):
                continue
            text = open(os.path.join(dirpath, f), encoding="utf-8").read()
            for m in re.finditer(r'(?:error|warning|hintUnused)\s*\(', text):
                args = split_args(text[m.end():m.end() + 1500])
                code = next((a.strip('"') for a in args if re.fullmatch(r'"[EW]\d{4}"', a)), None)
                if not code or len(args) < 2:
                    continue
                msg, hint = readable(args[1]), readable(args[2]) if len(args) > 2 and not re.fullmatch(r'"[EW]\d{4}"', args[2]) else ""
                entry = codes.setdefault(code, {"msgs": [], "hints": []})
                if msg and msg not in entry["msgs"]:
                    entry["msgs"].append(msg)
                if hint and hint != "…" and hint not in entry["hints"]:
                    entry["hints"].append(hint)
    out = ["# Códigos de error y aviso", "",
           "Cada mensaje del compilador lleva un código (`error[E0205]`, `aviso[W0303]`). Aquí están todos, agrupados por tipo.",
           "Donde pone «…» va el nombre o el valor concreto de tu programa.", "",
           "> Este archivo se genera con `python tools/gen_docs.py`: no lo edites a mano.", ""]
    for prefix, title in CATEGORIES:
        group = sorted(c for c in codes if c.startswith(prefix))
        if not group:
            continue
        out += ["## " + title, "", "| Código | Mensaje | Cómo arreglarlo |", "|---|---|---|"]
        for c in group:
            e = codes[c]
            msg = "<br>".join(x.replace("|", "\\|") for x in e["msgs"][:3])
            hint = "<br>".join(x.replace("|", "\\|") for x in e["hints"][:2])
            out.append("| `%s` | %s | %s |" % (c, msg, hint))
        out.append("")
    return "\n".join(out)


def bytecode_table():
    text = open(os.path.join(ROOT, "src", "lang", "compiler", "bytecode.h"), encoding="utf-8").read()
    rows = []
    section = ""
    for line in text.splitlines():
        line = line.strip()
        m = re.match(r"(OP_[A-Z0-9_]+),?\s*(?://\s*(.*))?$", line)
        if m and m.group(1) != "OP_COUNT":
            rows.append((section, m.group(1), (m.group(2) or "").replace("|", "\\|")))
        elif line.startswith("//") and not rows or (line.startswith("//") and "OP_" not in line):
            section = line.lstrip("/ ").strip()
    out = ["# Instrucciones de la máquina virtual", "",
           "El compilador traduce cada función a una lista de instrucciones (bytecode) para una máquina de pila.",
           "Cada instrucción ocupa un byte y puede llevar operandos de 8 bits (`u8`) o 16 bits (`u16`).",
           "La columna «Operandos y pila» indica qué lee de la pila y qué deja (`a b -> r`).", "",
           "> Este archivo se genera con `python tools/gen_docs.py` a partir de `src/lang/compiler/bytecode.h`.", "",
           "| # | Instrucción | Operandos y pila |", "|---|---|---|"]
    for i, (_, name, comment) in enumerate(rows):
        out.append("| %d | `%s` | %s |" % (i, name, comment))
    out.append("")
    out.append("Hay %d instrucciones." % len(rows))
    out.append("")
    return "\n".join(out)


def api_reference(hyper):
    r = subprocess.run([hyper, "docs"], capture_output=True, env=dict(os.environ, NO_COLOR="1"))
    if r.returncode != 0:
        raise SystemExit("hyper docs falló:\n" + r.stderr.decode("utf-8", "replace"))
    return r.stdout.decode("utf-8").replace("\r\n", "\n")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check = "--check" in sys.argv
    if len(args) != 1:
        print(__doc__)
        return 2
    outputs = {
        "docs/biblioteca/API.md": api_reference(os.path.abspath(args[0])),
        "docs/errores.md": error_catalog(),
        "docs/interno/bytecode.md": bytecode_table(),
    }
    stale = []
    for rel, content in outputs.items():
        path = os.path.join(ROOT, rel)
        old = open(path, encoding="utf-8").read().replace("\r\n", "\n") if os.path.exists(path) else None
        if old == content:
            continue
        if check:
            stale.append(rel)
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "w", encoding="utf-8", newline="\n").write(content)
            print("  " + rel)
    if stale:
        print("Documentación desfasada (ejecuta python tools/gen_docs.py <hyper>):\n  " + "\n  ".join(stale))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
