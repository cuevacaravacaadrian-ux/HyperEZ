"""Cambia la versión de HyperEZ en todos los sitios donde aparece.

    python tools/set_version.py 0.7.0

Actualiza src/core/version.h, CMakeLists.txt, los manifiestos de Windows, la extensión de
VS Code (package.json) y el README. El CHANGELOG.md se escribe a mano.
"""
import json
import os
import re
import sys

# La consola de Windows (y la de GitHub Actions) puede no ser UTF-8: los ✓ y las tildes no deben romper el script
for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sub(path, pattern, repl, count=1):
    full = os.path.join(ROOT, path)
    with open(full, encoding="utf-8", newline="") as f:
        text = f.read()
    new, n = re.subn(pattern, repl, text, count=count, flags=re.M)
    if n == 0:
        raise SystemExit("No se encontró la versión en " + path)
    with open(full, "w", encoding="utf-8", newline="") as f:
        f.write(new)
    print("  " + path)


def main():
    if len(sys.argv) != 2 or not re.fullmatch(r"\d+\.\d+\.\d+", sys.argv[1]):
        print(__doc__)
        return 2
    v = sys.argv[1]
    major, minor, patch = v.split(".")
    print("Versión " + v + ":")
    sub("src/core/version.h", r'HYPEREZ_VERSION "[^"]*"', 'HYPEREZ_VERSION "%s"' % v)
    sub("src/core/version.h", r"HYPEREZ_VERSION_MAJOR \d+", "HYPEREZ_VERSION_MAJOR " + major)
    sub("src/core/version.h", r"HYPEREZ_VERSION_MINOR \d+", "HYPEREZ_VERSION_MINOR " + minor)
    sub("src/core/version.h", r"HYPEREZ_VERSION_PATCH \d+", "HYPEREZ_VERSION_PATCH " + patch)
    sub("CMakeLists.txt", r"project\(HyperEZ VERSION [\d.]+", "project(HyperEZ VERSION " + v)
    for m in ("packaging/windows/hyper.manifest", "packaging/windows/setup.manifest", "packaging/windows/editor.manifest"):
        sub(m, r'(<assemblyIdentity [^>]*version=")[\d.]+"', r'\g<1>%s.0"' % v)
    pkg_path = os.path.join(ROOT, "editors/vscode/package.json")
    with open(pkg_path, encoding="utf-8") as f:
        pkg = json.load(f)
    pkg["version"] = v
    pkg["scripts"]["package"] = re.sub(r"hyperez-[\d.]+\.vsix", "hyperez-%s.vsix" % v, pkg["scripts"]["package"])
    with open(pkg_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(pkg, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print("  editors/vscode/package.json")
    for doc in ("README.md", "docs/empezar/instalacion.md"):
        sub(doc, r"HyperEZ-Setup-[\d.]+\.exe", "HyperEZ-Setup-%s.exe" % v, count=0)
        sub(doc, r"HyperEZ-[\d.]+-(linux|macos)", r"HyperEZ-%s-\1" % v, count=0)
        sub(doc, r"hyperez-[\d.]+\.vsix", "hyperez-%s.vsix" % v, count=0)
    sub("editors/vscode/PUBLICAR.md", r"hyperez-[\d.]+\.vsix", "hyperez-%s.vsix" % v, count=0)
    print("Recuerda añadir la sección " + v + " a CHANGELOG.md.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
