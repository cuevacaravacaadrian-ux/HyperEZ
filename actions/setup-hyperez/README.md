# Acción `setup-hyperez`

Instala HyperEZ (la orden `hyper`) en GitHub Actions, en Windows, Linux o macOS, para que las pruebas de un proyecto se ejecuten solas cada vez que se sube un cambio.

```yaml
# .github/workflows/pruebas.yml
name: Pruebas
on: [push, pull_request]
jobs:
  pruebas:
    runs-on: ubuntu-latest        # o windows-latest, macos-latest
    steps:
      - uses: actions/checkout@v4
      - uses: cuevacaravacaadrian-ux/HyperEZ/actions/setup-hyperez@main
        with:
          version: latest           # o una versión concreta: 0.8.1
      - run: hyper check main.hez
      - run: hyper test
```

Los proyectos que crea `hyper new` ya traen este archivo. La versión instalada queda en `steps.<id>.outputs.version`.

| Entrada | Por defecto | |
|---|---|---|
| `version` | `latest` | la versión de HyperEZ: `latest` o un número (`0.8.1`) |

Descarga los archivos de la [página de versiones](https://github.com/cuevacaravacaadrian-ux/HyperEZ/releases): `hyper.exe` en Windows y `HyperEZ-<versión>-<sistema>-<arquitectura>.tar.gz` en Linux y macOS.
