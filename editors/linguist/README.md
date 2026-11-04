# Que GitHub reconozca HyperEZ como lenguaje

GitHub decide el lenguaje de cada archivo, los colores del código y la barra de lenguajes de los repositorios con [Linguist](https://github.com/github-linguist/linguist). Para que **HyperEZ** aparezca ahí como un lenguaje más hay que proponerlo a Linguist con una *pull request*.

## El requisito

Linguist solo acepta lenguajes que ya se usan en GitHub: piden que la extensión aparezca en **al menos 200 repositorios públicos distintos** (sin contar los del propio autor) y lo comprueban con una búsqueda. Mientras HyperEZ no llegue a esa cifra, la propuesta no se puede aceptar: este kit deja todo preparado para cuando llegue.

Cómo acercarse a esa cifra:

- Publicar el repositorio de HyperEZ (ahora es privado) y los proyectos hechos con HyperEZ como repositorios públicos, con archivos `.hez`.
- Animar a quienes lo usen a subir sus programas a GitHub.
- Comprobar el número con la búsqueda de GitHub: `extension:hez NOT is:fork` y `extension:hz NOT is:fork`.

Mientras tanto, el [`.gitattributes`](../../.gitattributes) del repositorio hace que GitHub coloree los `.hez` y `.hz` con un lenguaje de sintaxis parecida, sin contarlos en las estadísticas. Puedes copiar esas líneas en tus proyectos.

## Qué hay en esta carpeta

| Archivo | Para qué |
|---|---|
| [`languages.yml`](languages.yml) | la entrada para `lib/linguist/languages.yml` (Linguist asigna el `language_id`) |
| [`samples/`](samples) | programas de ejemplo reales para `samples/HyperEZ/` (Linguist los usa para reconocer el lenguaje) |

La gramática de colores es la de la extensión de VS Code: [`editors/vscode/syntaxes/hyperez.tmLanguage.json`](../vscode/syntaxes/hyperez.tmLanguage.json) (alcance `source.hyperez`, licencia MIT). Linguist la incorpora como submódulo desde un repositorio público.

## Los pasos (cuando se cumpla el requisito)

1. Haz público el repositorio de HyperEZ (Linguist necesita acceder a la gramática).
2. Haz un *fork* de [github-linguist/linguist](https://github.com/github-linguist/linguist) y sigue su guía [CONTRIBUTING.md](https://github.com/github-linguist/linguist/blob/main/CONTRIBUTING.md#adding-a-language).
3. Añade la gramática: `script/add-grammar https://github.com/cuevacaravacaadrian-ux/HyperEZ`.
4. Copia la entrada de `languages.yml` en orden alfabético y ejecuta `script/update-ids`.
5. Copia los archivos de [`samples/`](samples) en `samples/HyperEZ/`.
6. Ejecuta las pruebas de Linguist (`bundle exec rake test`) y abre la *pull request* con la búsqueda que demuestra los 200 repositorios.
