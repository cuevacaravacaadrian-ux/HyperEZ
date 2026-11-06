# Proyectos y carpetas de HyperEZ

Un proyecto de HyperEZ tiene **sus propias carpetas**, cada una con un nombre que empieza por `hyper` y su propio icono en HyperEZ Editor, en el editor del navegador y en VS Code (con los iconos de HyperEZ). No hay que configurar nada: `hyper run`, `hyper test`, las importaciones y los editores las reconocen solos.

```text
mi-juego/
  main.hez              el programa principal (F5 o hyper run)
  hypersettings/
    proyecto.json       nombre, versión y programa principal
  hypercode/            los módulos del programa
    jugador.hez
  hypertests/           las pruebas (hyper test)
    test_jugador.hez
  hyperlibs/            bibliotecas de otros (si hacen falta)
  hypertools/           herramientas del proyecto (hyper tool)
  hyperdata/            datos: CSV, JSON...
  hyperassets/          imágenes, sonidos y fuentes
  hyperdocs/            la documentación del proyecto
  hyperbuild/           lo que se genera (no se sube a git)
  README.md
```

Solo `hypersettings/` es imprescindible: marca la **raíz del proyecto**. Las demás se crean cuando hacen falta.

## Crear un proyecto

- **Terminal**: `hyper new mi-juego` (o `--tipo app`, `ia`, `script`, `web`, `sitio`).
- **HyperEZ Editor**: *Archivo → Nuevo proyecto* (Ctrl+Mayús+N). Se crea en `Documentos\HyperEZ` y se abre en el Explorador.
- **Editor del navegador**: *Archivo → Nuevo proyecto* (Ctrl+Mayús+N), con los mismos tipos.
- **VS Code**: *HyperEZ: Nuevo proyecto*.

## Para qué sirve cada carpeta

| Carpeta | Para qué | Quién la usa |
|---|---|---|
| `hypersettings/` | `proyecto.json` con el nombre, la versión, la descripción, el autor y el **programa principal** | `hyper run` sin archivo ejecuta el principal; los editores muestran el nombre del proyecto |
| `hypercode/` | los **módulos**: se importan **por su nombre**, desde cualquier carpeta del proyecto | `from jugador import Jugador` |
| `hypertests/` | las **pruebas** (`test("...", () => { ... })`) | `hyper test` sin carpeta las ejecuta todas; en el navegador, *Ejecutar → Ejecutar las pruebas* |
| `hyperlibs/` | **bibliotecas de otros**, una carpeta por biblioteca con su `main.hez` | `import saludos` busca `hyperlibs/saludos/main.hez` |
| `hypertools/` | **herramientas** del proyecto: scripts para generar datos, publicar... | `hyper tool` las lista (con su primer comentario) y `hyper tool <nombre> [args]` ejecuta una |
| `hyperdata/` | los **datos** del programa | `Path.join(System.projectDir, "hyperdata/tareas.json")` |
| `hyperassets/` | imágenes, sonidos, fuentes | igual, con `System.projectDir` |
| `hyperdocs/` | la documentación | `import site` la convierte en una web |
| `hyperbuild/` | lo que se genera al compilar o publicar | el `.gitignore` de `hyper new` ya lo excluye |

## proyecto.json

```json
{
  "nombre": "mi-juego",
  "version": "0.1.0",
  "descripcion": "Un juego de laberintos",
  "autor": "Ana",
  "principal": "main.hez",
  "tipo": "juego",
  "hyperez": "0.8.3"
}
```

`principal` es el archivo que ejecutan `hyper run` sin argumentos, F5 sobre un archivo que no es código (en el navegador) y los botones de ejecutar del proyecto.

## Importar por nombre

Sin comillas, `from jugador import Jugador` busca, por este orden:

1. `jugador.hez` junto al archivo que importa;
2. `hypercode/jugador.hez` y `hyperlibs/jugador/main.hez` de la raíz del proyecto (la carpeta con `hypersettings/`);
3. `src/`, la carpeta del proyecto y `hz_packages/` (proyectos de antes);
4. las bibliotecas incluidas con HyperEZ.

Así, una prueba de `hypertests/` importa los módulos de `hypercode/` igual que `main.hez`, sin rutas como `"../src/jugador.hez"`.

## La raíz del proyecto en el código

`System.projectDir` es la carpeta con `hypersettings/` (la primera hacia arriba desde el programa; si no hay, la del programa). Es la forma segura de llegar a los datos, se ejecute el programa desde donde se ejecute:

```hz
var ruta = Path.join(System.projectDir, "hyperdata", "puntuaciones.json")
var puntos = File.exists(ruta) ? JSON.parse(File.read(ruta)) : []
```

## Herramientas del proyecto

Un archivo en `hypertools/` es una herramienta. Su primer comentario es la descripción:

```hz
// Genera 100 niveles aleatorios en hyperdata/niveles.json
var niveles = [{ numero: i, semilla: randomInt(1, 99999) } for i in 1..=100]
File.write(Path.join(System.projectDir, "hyperdata", "niveles.json"), JSON.stringify(niveles))
print("✓ ${niveles.length} niveles")
```

```text
hyper tool                     lista las herramientas del proyecto
hyper tool niveles             ejecuta hypertools/niveles.hez
hyper tool publicar --final    con argumentos (System.args)
```

## Los iconos

Cada carpeta tiene su icono, el mismo en todas partes:

| | |
|---|---|
| **HyperEZ Editor** | el Explorador los muestra siempre |
| **Editor del navegador** | igual |
| **VS Code** | *HyperEZ: Usar los iconos de HyperEZ* (se ofrece al abrir un proyecto con `hypersettings/`), o *Preferencias → Tema de iconos de archivo → HyperEZ* |

Los dibuja `tools/gen_iconos.js` (SVG para VS Code y la web, `.ico` para HyperEZ Editor) y están en `assets/icons/tema`.

## Proyectos de antes

Los proyectos con `src/` y `tests/` siguen funcionando: `hyper test` usa `tests/` si no hay `hypertests/`, y las importaciones por nombre siguen buscando en `src/`. Para pasarlos a las carpetas de HyperEZ basta con renombrar `src` → `hypercode`, `tests` → `hypertests` y crear `hypersettings/proyecto.json`.
