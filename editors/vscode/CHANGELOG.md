# Cambios

## 0.8.3

- **Tema de iconos de HyperEZ**: las carpetas de un proyecto (`hypersettings`, `hypercode`, `hypertests`, `hyperlibs`, `hypertools`, `hyperdata`, `hyperassets`, `hyperdocs`, `hyperbuild`) y los archivos `.hez` con su propio icono. Se ofrece una vez al abrir un proyecto con `hypersettings/`, y está en el comando *HyperEZ: Usar los iconos de HyperEZ*.
- Los proyectos nuevos usan las carpetas de HyperEZ; F5 y *Ejecutar pruebas* reconocen la raíz por `hypersettings/` o `hypertests/`.
- Enlaces a [la web de HyperEZ](https://cuevacaravacaadrian.github.io/HyperEZ/) y al editor en el navegador.
- Herramientas de publicación actualizadas (`@vscode/vsce` 4.1, sin la vulnerabilidad de `minimatch`).

## 0.8.2

- El identificador de la extensión es ahora **`hyperez.hyperezprogramminglanguagevscodeextension`**. El instalador de HyperEZ quita las anteriores (`hyperez.hyperez` y `hyperez.hyperezprogramminglanguage`).
- **Ejecutar sin guardar**: F5 en un archivo sin título lo ejecuta tal cual, sin pedir dónde guardarlo; con `hyperez.run.saveBeforeRun` desactivado, también los archivos con cambios (necesita HyperEZ 0.8.2).
- HyperEZ también se puede usar sin instalar nada en el [editor del navegador](https://cuevacaravacaadrian.github.io/HyperEZ/editor/index.html).

## 0.8.1

- **Nuevo identificador para el Marketplace: `hyperez.hyperezprogramminglanguage`.** Si tenías la versión anterior (`hyperez.hyperez`, la del instalador), desinstálala para no tener dos; el instalador de HyperEZ 0.8.1 ya lo hace solo.
- **F5 deja ver el resultado**: VS Code cerraba la terminal en cuanto el programa terminaba bien. Ahora *Ejecutar*, *Comprobar*, *Ejecutar pruebas* y **▶ Probar** esperan una tecla al terminar.
- Página de la extensión renovada: capturas, primeros pasos, ejemplos e insignias.
- Colores para el formato de las interpolaciones (`"${precio:.2f}"`) y para `do { } while`; fragmentos `hacer` y `formato` (54 en total).
- Los proyectos nuevos se reconocen también por `main.hez`.

## 0.8.0

- Colores para la sintaxis nueva del lenguaje: tubería `|>`, `**=`, comprensiones `[x for x in lista if ...]` y propagación `{...mapa}`.
- Fragmentos nuevos: comprensión, tubería, aplicación web con plantillas y sitio desde Markdown (52 en total).
- El servidor de lenguaje entiende todo lo nuevo de HyperEZ 0.8 (webs con `import web` y `import site`, `hyper serve`).

## 0.7.0

- Botones encima del código (CodeLens): **▶ Ejecutar · ⟳ Repetir al guardar · ✓ Comprobar**, y **▶ Probar** sobre cada `test(...)` (ajuste `hyperez.codeLens`).
- **Dar formato** (Mayús+Alt+F) con el formateador del lenguaje, también desde el menú contextual; ajuste `hyperez.formatOnSave`.
- Comandos nuevos: *Ver el bytecode del archivo*, *Abrir en HyperEZ Editor* (Windows) y *Ejecutar las pruebas de este archivo*.
- `.hez` es la extensión por defecto: ejecutar un archivo sin guardar pregunta dónde guardarlo como `.hez` (en la carpeta abierta o en `Documentos\HyperEZ`) y lo ejecuta.
- Resaltado de las funciones nuevas (`gcd`, `noise`, `primes`, `chr`, `ord`...) y 8 fragmentos nuevos: grafo, cola de prioridad, camino A*, plantilla, Markdown, zip, gráfica SVG y registro.

## 0.6.0

- Página de bienvenida **Empezar con HyperEZ**, con un tutorial en cinco pasos (se abre la primera vez).
- Tema de color **HyperEZ Noche**.
- *Nuevo proyecto* pregunta el tipo: juego, aplicación, inteligencia artificial o herramienta.
- Comandos nuevos: *Nuevo archivo HyperEZ*, *Abrir la guía del lenguaje*, *Abrir los ejemplos*, *Empezar con HyperEZ*.
- 14 fragmentos nuevos: servidor web, red neuronal, imagen, sonido, base de datos, HTTP, expresiones regulares, procesos, CSV, argumentos, operadores y sector de arranque.
- Resaltado de los textos `r"..."` y de los módulos nuevos (`Http`, `Image`, `Matrix`, `Regex`, `Bytes`, `Process`...).

## 0.5.0

- Comando *Ejecutar y repetir al guardar* (Ctrl+Alt+F5): usa `hyper watch`.
- Comando *Abrir consola interactiva*: usa `hyper repl`.
- Licencia MIT y enlaces al repositorio.

## 0.4.0

- Primera versión de la extensión.
- Servidor de lenguaje (`hyper lsp`): diagnósticos, autocompletado, hover, ayuda de firmas, definición, referencias, renombrar, resaltado, esquema y arreglos rápidos.
- Gramática de resaltado para `.hz`/`.hez` y bloques de código en Markdown.
- Ejecutar (F5), comprobar y probar desde el editor; nuevo proyecto; referencia de la API.
- Fragmentos en español, muestras de color y tareas.
