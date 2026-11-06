# Que GitHub reconozca HyperEZ como lenguaje

GitHub decide el lenguaje de cada archivo con [Linguist](https://github.com/github-linguist/linguist): la barra de lenguajes de cada repositorio, los colores del código, las estadísticas y la búsqueda `language:HyperEZ` salen de ahí. Para que **HyperEZ** aparezca como un lenguaje más hay que proponerlo a Linguist con una *pull request*, y la aceptan o no las personas que mantienen Linguist.

**Este kit deja preparado todo lo que depende del proyecto.** Lo que falta, el uso real en GitHub, depende de la comunidad: los pasos y las metas están en [OBJETIVOS.md](OBJETIVOS.md).

## Lo que pide Linguist

| Requisito | Estado |
|---|---|
| Una entrada en `languages.yml` (tipo, color, extensiones, intérprete, gramática) | ✅ [`languages.yml`](languages.yml) |
| Una gramática de colores en un repositorio público con licencia | ✅ preparada en [`grammar/`](grammar); falta crear el repositorio `hyperez-grammar` (abajo) |
| Muestras de código real (no "hola mundo") para cada extensión | ✅ 7 en [`samples/`](samples), con licencia MIT |
| Que ninguna otra extensión choque | ✅ `.hez` no la usa ningún lenguaje de Linguist |
| **Uso en GitHub: al menos 2000 archivos `.hez` indexados en el último año, sin forks y repartidos entre muchos usuarios y repositorios** | ❌ hoy hay unos 18, y no son de HyperEZ (`python tools/linguist_kit.py --progress`) |
| No ser un lenguaje "muy nuevo o de hobby" (cierran esas propuestas) | ❌ se demuestra con lo anterior: usuarios, proyectos y tiempo |

Linguist no cuenta los repositorios del autor del lenguaje y quita de la búsqueda a un usuario si acapara los resultados. **No sirve crear cuentas o repositorios para inflar la cifra**: lo comprueban, rechazan la propuesta y va contra las normas de GitHub. La única forma es que haya gente programando en HyperEZ y subiendo sus proyectos.

### Por qué solo `.hez`

HyperEZ acepta `.hez` y `.hz`, pero en GitHub ya hay miles de archivos `.hz` que no son HyperEZ (configuración del framework Hertz de Go, firmware...). Linguist nunca le daría `.hz` a HyperEZ, así que la propuesta pide solo **`.hez`**, la extensión por defecto: los proyectos nuevos de `hyper new` y los archivos nuevos del editor y de VS Code ya la usan. Cuando HyperEZ esté en Linguist, un proyecto con archivos `.hz` puede añadir a su `.gitattributes`:

```text
*.hz linguist-language=HyperEZ
```

## Qué hay en esta carpeta

| Archivo | Para qué |
|---|---|
| [`languages.yml`](languages.yml) | la entrada para `lib/linguist/languages.yml` (Linguist asigna el `language_id`) |
| [`samples/`](samples) | programas reales para `samples/HyperEZ/` (uno empieza por `#!/usr/bin/env hyper`, para el intérprete) |
| [`grammar/`](grammar) | el contenido del repositorio de la gramática: solo la gramática, su licencia y un README |
| [`PULL_REQUEST.md`](PULL_REQUEST.md) | el texto de la *pull request*, en inglés y con la plantilla de Linguist rellenada |
| [`OBJETIVOS.md`](OBJETIVOS.md) | el plan por fases para llegar al requisito de uso |

Todo menos este README y OBJETIVOS.md lo genera `python tools/linguist_kit.py` a partir del repositorio (la gramática de VS Code, los ejemplos y las bibliotecas), y `ctest -R linguist` comprueba que está al día y que cumple lo que miran las pruebas de Linguist: campos válidos, una muestra por extensión, muestras que compilan, el intérprete declarado, una sola gramática con el alcance `source.hyperez` y licencia.

## El repositorio de la gramática

Linguist añade cada gramática como submódulo y rechaza los alcances repetidos. Este repositorio tiene la gramática dos veces con el mismo alcance (la de VS Code y la de Sublime Text), así que hace falta un repositorio aparte que solo la contenga:

1. Crea en GitHub el repositorio público **`hyperez-grammar`**, vacío.
2. Copia en él lo que hay en `editors/linguist/grammar/` y súbelo:

```text
git clone https://github.com/cuevacaravacaadrian/hyperez-grammar.git
copy editors\linguist\grammar\* hyperez-grammar\
cd hyperez-grammar
git add .
git commit -m "HyperEZ grammar"
git push
```

Cada vez que cambie la gramática, `python tools/linguist_kit.py` actualiza `grammar/`: vuelve a copiarla y súbela.

## La pull request (cuando se cumpla el requisito de uso)

`tools/linguist_pr.py` hace los pasos del [CONTRIBUTING.md de Linguist](https://github.com/github-linguist/linguist/blob/main/CONTRIBUTING.md#adding-a-language) sin Ruby ni Docker: clona Linguist, crea la rama `hyperez`, añade la entrada a `lib/linguist/languages.yml` en su sitio y con su `language_id` (el cálculo de `script/update-ids`), copia las muestras a `samples/HyperEZ/`, añade la gramática como submódulo con su entrada en `grammars.yml`, `vendor/README.md` y `vendor/licenses` (lo de `script/add-grammar`) y lo guarda en un commit.

1. En GitHub, crea el repositorio público **`hyperez-grammar`** con lo que hay en [`grammar/`](grammar) (arriba) y haz un **fork** de [github-linguist/linguist](https://github.com/github-linguist/linguist).
2. `python tools/linguist_pr.py` lo prepara en `build-linguist/` (sin subir nada) y `python tools/linguist_pr.py --push` lo sube a tu fork.
3. Comprueba la cifra con `python tools/linguist_kit.py --progress` y con la búsqueda de GitHub `NOT is:fork path:*.hez`.
4. Abre la *pull request* desde el enlace que muestra `--push`, con el texto de [`PULL_REQUEST.md`](PULL_REQUEST.md) y las cifras del día. Las pruebas de Linguist (`bundle exec rake test`) se ejecutan solas en la pull request; las que dependen de HyperEZ (campos, muestras que compilan, intérprete, gramática con licencia) ya las comprueba `ctest -R linguist`.

## Mientras tanto

- El [`.gitattributes`](../../.gitattributes) de este repositorio colorea los `.hez` y `.hz` con un lenguaje de sintaxis parecida, sin contarlos en las estadísticas.
- La extensión de VS Code, HyperEZ Editor y los colores para Notepad++, Sublime, Vim y highlight.js ya reconocen HyperEZ.
