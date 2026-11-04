"""Kit para proponer HyperEZ a GitHub Linguist, la biblioteca que decide el lenguaje de cada archivo en GitHub
(la barra de lenguajes de los repositorios, los colores del código y la búsqueda language:HyperEZ).

    python tools/linguist_kit.py                  regenera editors/linguist/ (entrada, muestras, gramática y PR)
    python tools/linguist_kit.py --check <hyper>  comprueba el kit como lo harían las pruebas de Linguist
    python tools/linguist_kit.py --progress       cuenta los archivos .hez de GitHub (usa GITHUB_TOKEN o las
                                                  credenciales de git; Linguist pide 2000 en el último año)

Todo se genera desde el repositorio: la gramática de VS Code, los ejemplos y las bibliotecas. No edites a mano
lo que hay en editors/linguist/ salvo README.md y OBJETIVOS.md.
"""
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request

for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KIT = os.path.join(ROOT, "editors", "linguist")
REPO = "cuevacaravacaadrian-ux/HyperEZ"
GRAMMAR_REPO = "cuevacaravacaadrian-ux/hyperez-grammar"   # repositorio solo con la gramática (ver README.md)
AUTHORS = {"cuevacaravacaadrian-ux", "cuevacaravacaadrian"}  # Linguist no cuenta los repositorios del autor

# La entrada de lib/linguist/languages.yml. Solo .hez: .hz ya lo usan miles de archivos que no son HyperEZ
# (configuración de Hertz para Go, firmware...), así que GitHub nunca se lo daría a HyperEZ.
LANGUAGE = {
    "name": "HyperEZ",
    "type": "programming",
    "color": "#62d962",
    "extensions": [".hez"],
    "interpreters": ["hyper"],
    "tm_scope": "source.hyperez",
    "ace_mode": "text",
}
COLOR_RATIONALE = ("the green of the code in HyperEZ's logo and banner; among the brand's greens, blues and indigos "
                   "it is the one farthest from every existing Linguist color (nearest: Elvish #55bb55, CIELAB ΔE 13.1)")
ALLOWED_FIELDS = {"type", "color", "extensions", "interpreters", "tm_scope", "ace_mode", "codemirror_mode",
                  "codemirror_mime_type", "aliases", "filenames", "group", "wrap", "fs_name", "searchable", "language_id"}

# Muestras reales (Linguist no acepta "hola mundo" ni ejemplos de tutorial): (origen, nombre, shebang)
SAMPLES = [
    ("examples/juegos/tres_en_raya.hz", "tres_en_raya.hez", False),
    ("examples/algoritmos/rutas.hz", "rutas.hez", False),
    ("examples/web/pagina_markdown.hz", "pagina_markdown.hez", False),
    ("examples/ia/red_neuronal.hz", "red_neuronal.hez", False),
    ("examples/sistema/organizar.hz", "organizar.hez", True),   # script con #!/usr/bin/env hyper
    ("src/lib/collections.hz", "collections.hez", False),
    ("src/lib/validate.hz", "validate.hez", False),
]
SHEBANG = "#!/usr/bin/env hyper\n"
GRAMMAR_SRC = "editors/vscode/syntaxes/hyperez.tmLanguage.json"


def rel(*p):
    return os.path.join(ROOT, *p)


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read().replace("\r\n", "\n")


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    old = read(path) if os.path.exists(path) else None
    if old != text:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    return os.path.relpath(path, ROOT).replace("\\", "/")


def languages_yml():
    L = LANGUAGE
    out = ["# Entrada para github-linguist/linguist: lib/linguist/languages.yml (en orden alfabético).",
           "# Generada por tools/linguist_kit.py. Linguist añade el language_id con script/update-ids.",
           L["name"] + ":",
           "  type: " + L["type"],
           '  color: "%s"' % L["color"],
           "  extensions:"]
    out += ['  - "%s"' % e for e in L["extensions"]]
    out.append("  interpreters:")
    out += ["  - " + i for i in L["interpreters"]]
    out += ["  tm_scope: " + L["tm_scope"], "  ace_mode: " + L["ace_mode"]]
    return "\n".join(out) + "\n"


def sample_text(src, shebang):
    text = read(rel(src))
    if shebang and not text.startswith("#!"):
        text = SHEBANG + text
    return text


def grammar_readme():
    return """# HyperEZ grammar

TextMate grammar for [HyperEZ](https://github.com/%s), a Spanish-language programming language
(`.hez` files, scope `source.hyperez`). It is the grammar of the HyperEZ extension for Visual Studio Code, published
here on its own so that [GitHub Linguist](https://github.com/github-linguist/linguist) can use it to highlight
HyperEZ code on GitHub.

Do not edit it here: it is generated in the main repository (`editors/vscode/tools/gen_grammar.py`) and copied by
`tools/linguist_kit.py`. Issues and pull requests go to https://github.com/%s.

| File | |
|---|---|
| `hyperez.tmLanguage.json` | the grammar (`source.hyperez`) |
| `LICENSE` | MIT |
""" % (REPO, REPO)


def pull_request():
    L = LANGUAGE
    blob = "https://github.com/%s/blob/main/" % REPO
    sources = "\n".join("      - %s%s" % (blob, src) for src, _, _ in SAMPLES)
    search = "https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.hez+-user%3Acuevacaravacaadrian-ux"
    return """Add HyperEZ

<!-- Texto preparado para la pull request a github-linguist/linguist (en inglés, como pide Linguist).
     Generado por tools/linguist_kit.py: antes de abrirla, comprueba la búsqueda y pon las cifras de hoy. -->

## Description

[HyperEZ](https://github.com/{repo}) is a general-purpose programming language with Spanish documentation and error
messages, aimed at learners and used for games, web applications, data analysis, AI and tooling. It ships an
installer for Windows, packages for Linux and macOS, a language server, a formatter and a VS Code extension
([Marketplace](https://marketplace.visualstudio.com/items?itemName=hyperez.hyperezprogramminglanguage)).
Files use the `.hez` extension (not used by any other language in Linguist); scripts may start with
`#!/usr/bin/env hyper`.

## Checklist:

- [x] **I am adding a new language.**
  - [ ] The extension of the new language is used in hundreds of repositories on GitHub.com.
    - Search results for each extension:
      - {search}
  - [x] I have included a real-world usage sample for all extensions added in this PR:
    - Sample source(s):
{sources}
    - Sample license(s): MIT (https://github.com/{repo}/blob/main/LICENSE)
  - [x] I have included a syntax highlighting grammar: https://github.com/{grammar}
  - [x] I have added a color
    - Hex value: `{color}`
    - Rationale: {rationale}.
  - [x] I have updated the heuristics to distinguish my language from others using the same extension.
    - Not needed: no other language uses `.hez`.
""".format(repo=REPO, search=search, sources=sources, grammar=GRAMMAR_REPO, color=L["color"], rationale=COLOR_RATIONALE)


def generate():
    done = [write(os.path.join(KIT, "languages.yml"), languages_yml())]
    wanted = {name for _, name, _ in SAMPLES}
    sdir = os.path.join(KIT, "samples")
    if os.path.isdir(sdir):
        for f in os.listdir(sdir):
            if f not in wanted:
                os.remove(os.path.join(sdir, f))
                done.append("(borrada) editors/linguist/samples/" + f)
    for src, name, shebang in SAMPLES:
        done.append(write(os.path.join(sdir, name), sample_text(src, shebang)))
    gdir = os.path.join(KIT, "grammar")
    done.append(write(os.path.join(gdir, "hyperez.tmLanguage.json"), read(rel(GRAMMAR_SRC))))
    done.append(write(os.path.join(gdir, "LICENSE"), read(rel("LICENSE"))))
    done.append(write(os.path.join(gdir, "README.md"), grammar_readme()))
    done.append(write(os.path.join(KIT, "PULL_REQUEST.md"), pull_request()))
    for d in done:
        print("  " + d)


def check(hyper):
    errors = []
    err = errors.append
    L = LANGUAGE
    # 1. La entrada: los campos que acepta Linguist y lo que comprueban sus pruebas
    if not re.fullmatch(r"#[0-9a-f]{6}", L["color"]):
        err("el color debe ser #rrggbb en minúsculas: " + L["color"])
    unknown = set(L) - ALLOWED_FIELDS - {"name"}
    if unknown:
        err("campos que Linguist no conoce: " + ", ".join(sorted(unknown)))
    if "language_id" in L:
        err("no pongas language_id: lo asigna Linguist con script/update-ids")
    exts = L["extensions"]
    if any(not e.startswith(".") for e in exts):
        err("las extensiones empiezan por '.'")
    if [e.lower() for e in exts[1:]] != sorted(e.lower() for e in exts[1:]):
        err("las extensiones (salvo la primera) deben ir en orden alfabético")
    yml = os.path.join(KIT, "languages.yml")
    if not os.path.exists(yml) or read(yml) != languages_yml():
        err("editors/linguist/languages.yml no está al día: python tools/linguist_kit.py")
    # 2. Muestras: al día, de una extensión de la entrada, al menos una por extensión, y que compilen
    sdir = os.path.join(KIT, "samples")
    present = sorted(os.listdir(sdir)) if os.path.isdir(sdir) else []
    if present != sorted(name for _, name, _ in SAMPLES):
        err("las muestras no coinciden con la lista SAMPLES: python tools/linguist_kit.py")
    seen = set()
    for src, name, shebang in SAMPLES:
        path = os.path.join(sdir, name)
        if not os.path.exists(path):
            continue
        text = read(path)
        if text != sample_text(src, shebang):
            err("la muestra %s no está al día con %s" % (name, src))
        ext = os.path.splitext(name)[1]
        seen.add(ext)
        if ext not in exts:
            err("%s tiene una extensión (%s) que no está en la entrada" % (name, ext))
        m = re.match(r"#!\s*(?:/usr/bin/env\s+)?(?:\S*/)?(\S+)", text)
        if m and m.group(1) not in L["interpreters"]:
            err("%s usa el intérprete %s, que no está en la entrada" % (name, m.group(1)))
        if text.count("\n") < 20:
            err("%s es demasiado corta: Linguist quiere código real, no ejemplos de tutorial" % name)
        if hyper:
            r = subprocess.run([hyper, "check", path], capture_output=True, text=True, encoding="utf-8",
                               errors="replace", env=dict(os.environ, NO_COLOR="1"))
            if r.returncode != 0:
                err("%s no compila:\n%s" % (name, (r.stdout + r.stderr).strip()[:800]))
    for e in exts:
        if e not in seen:
            err("falta una muestra con la extensión " + e)
    if not any(sh for _, _, sh in SAMPLES):
        err("falta una muestra con #!/usr/bin/env hyper (para el intérprete)")
    # 3. La gramática: JSON válido, el alcance de la entrada, una sola gramática y con licencia
    gdir = os.path.join(KIT, "grammar")
    gpath = os.path.join(gdir, "hyperez.tmLanguage.json")
    try:
        g = json.loads(read(gpath))
        if g.get("scopeName") != L["tm_scope"]:
            err("la gramática tiene el alcance %s, no %s" % (g.get("scopeName"), L["tm_scope"]))
        if read(gpath) != read(rel(GRAMMAR_SRC)):
            err("editors/linguist/grammar no está al día con la gramática de VS Code: python tools/linguist_kit.py")
    except (OSError, ValueError) as e:
        err("gramática ilegible: %s" % e)
    grammars = [f for f in os.listdir(gdir) if re.search(r"\.(tmLanguage(\.json)?|cson|plist|ya?ml)$", f)] if os.path.isdir(gdir) else []
    if len(grammars) != 1:
        err("el repositorio de la gramática debe tener una sola gramática (Linguist rechaza alcances repetidos): %s" % grammars)
    if not os.path.exists(os.path.join(gdir, "LICENSE")):
        err("falta la licencia de la gramática")
    if not os.path.exists(os.path.join(KIT, "PULL_REQUEST.md")) or read(os.path.join(KIT, "PULL_REQUEST.md")) != pull_request():
        err("editors/linguist/PULL_REQUEST.md no está al día: python tools/linguist_kit.py")
    if errors:
        print("✗ El kit de Linguist tiene problemas:")
        for e in errors:
            print("  - " + e.replace("\n", "\n    "))
        return 1
    print("✓ Kit de Linguist correcto: entrada, %d muestras, gramática y texto de la pull request" % len(SAMPLES))
    return 0


def token():
    if os.environ.get("GITHUB_TOKEN"):
        return os.environ["GITHUB_TOKEN"]
    try:
        p = subprocess.run(["git", "credential", "fill"], input="protocol=https\nhost=github.com\n\n",
                           capture_output=True, text=True, timeout=60)
        for line in p.stdout.splitlines():
            if line.startswith("password="):
                return line.split("=", 1)[1]
    except (OSError, subprocess.SubprocessError):
        pass
    return None


def progress():
    tk = token()
    if not tk:
        print("La búsqueda de código de GitHub necesita un token: define GITHUB_TOKEN o inicia sesión con git.")
        return 2
    files, repos, owners = 0, set(), set()
    own = 0
    for page in range(1, 11):  # la API da como mucho 1000 resultados
        url = "https://api.github.com/search/code?" + urllib.parse.urlencode({"q": "extension:hez", "per_page": 100, "page": page})
        req = urllib.request.Request(url, headers={"Authorization": "Bearer " + tk, "Accept": "application/vnd.github+json",
                                                   "User-Agent": "hyperez-linguist-kit"})
        with urllib.request.urlopen(req, timeout=60) as r:
            d = json.load(r)
        items = d.get("items", [])
        for it in items:
            owner = it["repository"]["owner"]["login"]
            if owner in AUTHORS:
                own += 1
                continue
            files += 1
            repos.add(it["repository"]["full_name"])
            owners.add(owner)
        if len(items) < 100:
            break
        time.sleep(7)  # la búsqueda de código admite unas 10 peticiones por minuto
    meta = 2000
    print("Archivos .hez en GitHub (sin contar los %d del autor): %d" % (own, files))
    print("  en %d repositorios de %d usuarios distintos" % (len(repos), len(owners)))
    print("  objetivo de Linguist: %d archivos en el último año, sin forks y repartidos → %.1f %%" % (meta, 100.0 * files / meta))
    print("  (no todos los .hez tienen por qué ser HyperEZ: revisa la búsqueda en")
    print("   https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.hez )")
    return 0


def main():
    args = sys.argv[1:]
    if args[:1] == ["--check"]:
        return check(os.path.abspath(args[1]) if len(args) > 1 else None)
    if args[:1] == ["--progress"]:
        return progress()
    if args:
        print(__doc__)
        return 2
    generate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
