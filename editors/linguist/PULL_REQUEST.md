Add HyperEZ

<!-- Texto preparado para la pull request a github-linguist/linguist (en inglés, como pide Linguist).
     Generado por tools/linguist_kit.py: antes de abrirla, comprueba la búsqueda y pon las cifras de hoy. -->

## Description

[HyperEZ](https://github.com/cuevacaravacaadrian-ux/HyperEZ) is a general-purpose programming language with Spanish documentation and error
messages, aimed at learners and used for games, web applications, data analysis, AI and tooling. It ships an
installer for Windows, packages for Linux and macOS, a language server, a formatter and a VS Code extension
([Marketplace](https://marketplace.visualstudio.com/items?itemName=hyperez.hyperezprogramminglanguage)).
Files use the `.hez` extension (not used by any other language in Linguist); scripts may start with
`#!/usr/bin/env hyper`.

## Checklist:

- [x] **I am adding a new language.**
  - [ ] The extension of the new language is used in hundreds of repositories on GitHub.com.
    - Search results for each extension:
      - https://github.com/search?type=code&q=NOT+is%3Afork+path%3A*.hez+-user%3Acuevacaravacaadrian-ux
  - [x] I have included a real-world usage sample for all extensions added in this PR:
    - Sample source(s):
      - https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/examples/juegos/tres_en_raya.hz
      - https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/examples/algoritmos/rutas.hz
      - https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/examples/web/pagina_markdown.hz
      - https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/examples/ia/red_neuronal.hz
      - https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/examples/sistema/organizar.hz
      - https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/src/lib/collections.hz
      - https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/src/lib/validate.hz
    - Sample license(s): MIT (https://github.com/cuevacaravacaadrian-ux/HyperEZ/blob/main/LICENSE)
  - [x] I have included a syntax highlighting grammar: https://github.com/cuevacaravacaadrian-ux/hyperez-grammar
  - [x] I have added a color
    - Hex value: `#62d962`
    - Rationale: the green of the code in HyperEZ's logo and banner; among the brand's greens, blues and indigos it is the one farthest from every existing Linguist color (nearest: Elvish #55bb55, CIELAB ΔE 13.1).
  - [x] I have updated the heuristics to distinguish my language from others using the same extension.
    - Not needed: no other language uses `.hez`.
