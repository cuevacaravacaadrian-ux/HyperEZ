# HyperEZ Editor

Un editor de código sencillo y clásico, con fondo blanco, que se instala con HyperEZ en Windows. Es la forma más rápida de empezar: lo abres, escribes y pulsas **F5**.

![HyperEZ Editor con un juego de tres en raya abierto](../img/editor.png)

## Abrirlo

- Al terminar la instalación se abre solo (casilla *Abrir HyperEZ Editor al terminar*).
- Menú Inicio → **HyperEZ Editor**, o el acceso directo del escritorio.
- Clic derecho en un archivo `.hez` o `.hz` → **Editar con HyperEZ Editor** (o *Abrir con → HyperEZ Editor*).
- Clic derecho en una carpeta → *Nuevo → Programa HyperEZ* crea un `.hez` vacío.
- Desde VS Code: *HyperEZ: Abrir en HyperEZ Editor*.

Los programas nuevos se guardan como **`.hez`** en `Documentos\HyperEZ` (puedes elegir otra carpeta).

## Lo que hace

| | |
|---|---|
| **Colores** | palabras clave, textos (y lo que va dentro de `${...}`), números, comentarios, tipos y llamadas. Usa el analizador del propio lenguaje, así que colorea igual que el compilador entiende el código. |
| **Errores mientras escribes** | el panel de abajo lista los errores y avisos con su línea; doble clic lleva a cada uno. La parte del código con el error se marca en rojo claro y su número de línea también. |
| **Ejecutar (F5)** | guarda y ejecuta el programa en una consola nueva, que espera una tecla al terminar. Los juegos de consola funcionan con el teclado. |
| **Repetir al guardar (Ctrl+F5)** | ejecuta en modo *watch*: cada vez que guardas, vuelve a ejecutarse. |
| **Consola interactiva (F6)** | abre `hyper repl` en la carpeta del archivo. |
| **Comprobar (F7)** | busca errores sin ejecutar. |
| **Autocompletado (Ctrl+Espacio)** | palabras clave, todas las funciones, módulos y clases, y los nombres de tu archivo. Tras `Console.` o `Time.` muestra sus funciones; la firma aparece en la barra de estado. |
| **Firmas** | al escribir `(` tras una función, la barra de estado enseña sus parámetros. |
| **Sangría automática** | Intro mantiene la sangría y añade un nivel tras `{`, `[` o `(`; al escribir `}` se quita un nivel. Tab y Mayús+Tab sangran varias líneas. |
| **Formatear (Ctrl+Mayús+F)** | ordena la sangría y los espacios como `hyper fmt`. Se deshace con Ctrl+Z. |
| **Buscar y reemplazar** | Ctrl+F, F3 (siguiente), Ctrl+H; Ctrl+G va a una línea. |
| **Comentar (Ctrl+/)** y **duplicar línea (Ctrl+D)** | |
| **Zoom** | Ctrl+rueda, Ctrl++ y Ctrl+-; Ctrl+0 vuelve al tamaño normal. Se recuerda para la próxima vez. |
| **Plantillas** | *Archivo → Nuevo desde plantilla*: juego de consola, clase con pruebas, inteligencia artificial, servidor web, dibujar una imagen, leer un CSV. |
| **Ejemplos** | *Archivo → Abrir ejemplo* (o el botón *Ejemplos*) con todos los ejemplos instalados, por áreas. |
| **Recientes, arrastrar y soltar** | los últimos 10 archivos; también puedes soltar un archivo en la ventana. |
| **Ayuda (F1)** | la guía del lenguaje, la documentación completa y la referencia de todas las funciones. |

Si cierras con cambios sin guardar, pregunta antes. El título muestra `•` mientras hay cambios (si deshaces hasta lo guardado, desaparece).

## Atajos

| Tecla | Acción |
|---|---|
| F5 | Ejecutar |
| Ctrl+F5 | Ejecutar y repetir al guardar |
| F6 | Consola interactiva |
| F7 | Comprobar errores |
| Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Mayús+S | Nuevo / Abrir / Guardar / Guardar como |
| Ctrl+F / F3 / Ctrl+H | Buscar / siguiente / reemplazar |
| Ctrl+G | Ir a la línea |
| Ctrl+Espacio | Completar nombre |
| Ctrl+/ | Comentar o descomentar |
| Ctrl+D | Duplicar la línea |
| Ctrl+Mayús+F | Formatear documento |
| Tab / Mayús+Tab | Sangrar / quitar sangría |
| Ctrl++ / Ctrl+- / Ctrl+0 | Tamaño del texto |
| F1 | Guía del lenguaje |

## Cómo está hecho

Es un programa de Windows nativo (`bin\hyperez-editor.exe`, en `src/editor/editor.cpp`) escrito en C++ con la API de Win32 y el control RichEdit. Enlaza la misma biblioteca `hzlang` que `hyper.exe`:

- los colores salen del **lexer** de HyperEZ (también dentro de las interpolaciones `${...}`);
- los errores salen del **analizador** (`compileSource` sin generar código), en una máquina virtual nueva cada vez que dejas de escribir 0,7 segundos;
- el autocompletado usa el **catálogo de la API** que registran las funciones nativas y las bibliotecas incluidas;
- *Formatear* llama al mismo **formateador** que `hyper fmt`;
- *Ejecutar* lanza el `hyper.exe` que está junto al editor.

Los ajustes (tamaño de letra, ajuste de línea, paneles, archivos recientes y posición de la ventana) se guardan en `HKEY_CURRENT_USER\Software\HyperEZ\Editor`; el desinstalador los borra.
