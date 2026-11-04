"""Comprueba que los ejemplos de la documentación funcionan.

Extrae cada bloque ```hz de los archivos Markdown indicados y lo ejecuta con hyper.
Un bloque falla si tiene errores de compilación o termina con un error.

    python tools/check_docs.py <hyper.exe> docs/GUIA.md README.md ...
"""
import os
import re
import subprocess
import sys
import tempfile

# La consola de Windows (y la de GitHub Actions) puede no ser UTF-8: los ✓ y las tildes no deben romper el script
for _s in (sys.stdout, sys.stderr):
    if hasattr(_s, "reconfigure"):
        _s.reconfigure(encoding="utf-8", errors="replace")


def blocks(path):
    text = open(path, encoding="utf-8").read()
    for m in re.finditer(r"^```(?:hz|hez|hyperez)\s*\n(.*?)^```", text, re.S | re.M):
        line = text.count("\n", 0, m.start()) + 1
        yield line, m.group(1)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    hyper, files = os.path.abspath(sys.argv[1]), sys.argv[2:]
    failed = total = 0
    env = dict(os.environ, NO_COLOR="1")
    with tempfile.TemporaryDirectory() as tmp:
        for f in files:
            for line, code in blocks(f):
                total += 1
                src = os.path.join(tmp, "bloque_%d.hz" % total)
                with open(src, "w", encoding="utf-8") as out:
                    out.write(code)
                r = subprocess.run([hyper, "run", src], capture_output=True, text=True, encoding="utf-8",
                                   errors="replace", timeout=30, env=env, cwd=tmp, stdin=subprocess.DEVNULL)
                name = "%s:%d" % (os.path.basename(f), line)
                if r.returncode != 0:
                    failed += 1
                    print("✗ " + name)
                    for l in (r.stdout + r.stderr).strip().splitlines()[:12]:
                        print("    " + l)
                else:
                    print("✓ " + name)
    print("\n%d bloques de código, %d con errores" % (total, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
