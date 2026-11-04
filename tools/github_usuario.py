"""Pone tu usuario de GitHub en los enlaces del proyecto (README, extensión de VS Code, plantillas...).

    python tools/github_usuario.py <usuario> [repositorio]

Sustituye el marcador TU_USUARIO (y, si das otro nombre de repositorio, "HyperEZ" en las URL
de GitHub) en todos los archivos de texto del proyecto.
"""
import os
import re
import sys

# La consola de Windows (y la de GitHub Actions) puede no ser UTF-8: los ✓ y las tildes no deben romper el script
for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP_DIRS = {".git", "build", "dist", "node_modules", ".vscode-test", "third_party", "bin"}
EXTS = {".md", ".json", ".h", ".cpp", ".yml", ".yaml", ".txt", ".ps1", ".py", ".js", ".hz", ".cmake"}


def main():
    if len(sys.argv) not in (2, 3) or not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,38})", sys.argv[1]):
        print(__doc__)
        return 2
    user = sys.argv[1]
    repo = sys.argv[2] if len(sys.argv) == 3 else "HyperEZ"
    changed = 0
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if os.path.splitext(name)[1].lower() not in EXTS or name == os.path.basename(__file__):
                continue
            path = os.path.join(dirpath, name)
            with open(path, encoding="utf-8", errors="surrogateescape", newline="") as f:
                text = f.read()
            if "TU_USUARIO" not in text:
                continue
            new = text.replace("github.com/TU_USUARIO/HyperEZ", "github.com/%s/%s" % (user, repo))
            new = new.replace("TU_USUARIO/HyperEZ", "%s/%s" % (user, repo)).replace("TU_USUARIO", user)
            with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as f:
                f.write(new)
            print("  " + os.path.relpath(path, ROOT))
            changed += 1
    print("%d archivo(s) actualizados con github.com/%s/%s" % (changed, user, repo))
    return 0


if __name__ == "__main__":
    sys.exit(main())
