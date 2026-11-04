# HyperEZ en tu editor

| Editor | Qué tiene | Cómo instalarlo |
|---|---|---|
| **HyperEZ Editor** (Windows) | todo: colores, errores en vivo, F5, pestañas, panel de salida, autocompletado, formato | viene con [HyperEZ-Setup.exe](https://github.com/cuevacaravacaadrian-ux/HyperEZ/releases/latest) · [guía](../docs/herramientas/editor.md) |
| **Visual Studio Code** | todo: colores, errores en vivo, autocompletado, F12, F2, formato, botones ▶ | [`vscode/`](vscode) · se instala con el instalador o con el `.vsix` · [guía](../docs/herramientas/vscode.md) |
| **Cualquier editor con LSP** (Neovim, Helix, Zed, Emacs, Sublime con LSP…) | errores, autocompletado, hover, ir a la definición, renombrar, formato | configura el servidor de lenguaje `hyper lsp` para los archivos `.hez` y `.hz` |
| **Sublime Text / TextMate** | colores | copia [`sublime/HyperEZ.tmLanguage`](sublime/HyperEZ.tmLanguage) en tu carpeta `Packages/User` |
| **Notepad++** | colores, plegado de llaves | *Lenguaje → Definir tu idioma… → Importar…* y elige [`notepad++/HyperEZ.xml`](notepad++/HyperEZ.xml) |
| **Vim / Neovim** | colores | copia [`vim/syntax`](vim/syntax) y [`vim/ftdetect`](vim/ftdetect) en `~/.vim` (o `~/.config/nvim`) |
| **Páginas web** (highlight.js) | colores en blogs y documentación | [`highlightjs/hyperez.js`](highlightjs/hyperez.js) junto a highlight.js: `<code class="language-hz">` |
| **GitHub** | reconocimiento oficial del lenguaje | [`linguist/`](linguist): el kit para proponerlo a GitHub Linguist |

Los colores de Sublime, Notepad++, Vim y highlight.js se generan a partir de la gramática de VS Code con `python tools/gen_editors.py`.

## Servidor de lenguaje en otros editores

`hyper lsp` habla el protocolo LSP 3.17 por la entrada y la salida estándar. Por ejemplo, en Neovim (con `nvim-lspconfig` o a mano):

```text
vim.filetype.add({ extension = { hez = "hyperez", hz = "hyperez" } })
vim.lsp.start({ name = "hyperez", cmd = { "hyper", "lsp" }, root_dir = vim.fn.getcwd() })
```

y en Helix (`languages.toml`):

```text
[[language]]
name = "hyperez"
scope = "source.hyperez"
file-types = ["hez", "hz"]
language-servers = ["hyper"]
comment-token = "//"
indent = { tab-width = 4, unit = "    " }

[language-server.hyper]
command = "hyper"
args = ["lsp"]
```
