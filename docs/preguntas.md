# Preguntas frecuentes

**¿Para quién es HyperEZ?**
Para quien aprende a programar en español y para quien quiere hacer proyectos reales sin pelearse con el lenguaje: juegos, herramientas, análisis de datos, IA, webs. Los mensajes de error explican qué pasa y cómo arreglarlo.

**¿Por qué las palabras clave están en inglés (`if`, `function`, `class`)?**
Para que lo aprendido sirva en cualquier otro lenguaje y la documentación del mundo sea aplicable. Todo lo demás está en español: los mensajes, la documentación, los ejemplos y los nombres que tú elijas (`var puntuación`, `function mover()`).

**¿Es compilado o interpretado?**
Se compila a bytecode y lo ejecuta una máquina virtual, como Python o Lua. La compilación es tan rápida (milisegundos) que no se nota.

**¿Es rápido?**
Bastante para un lenguaje de este tipo: está optimizado con PGO, cachés en línea e instrucciones fusionadas, y lo pesado (matrices, imágenes, expresiones regulares, compresión) se hace en C++. Mira [rendimiento](interno/rendimiento.md).

**¿Hay que poner tipos?**
No. Si los pones, HyperEZ encuentra más errores antes de ejecutar y el editor autocompleta mejor; si no, también deduce muchos solo.

**¿Puedo hacer un juego con gráficos?**
Hoy, juegos de consola (con colores, teclado en tiempo real y sonido WAV) e imágenes generadas. Los gráficos en ventana están en la [hoja de ruta](hoja-de-ruta.md) como biblioteca opcional.

**¿Puedo escribir un sistema operativo?**
Puedes escribir sus herramientas (sectores de arranque, imágenes de disco, scripts de compilación) — mira [sistemas operativos](biblioteca/sistemas-operativos.md). Para escribir el kernel en sí hace falta compilar a código nativo, que está en la hoja de ruta.

**¿Funciona sin internet?**
Sí. Todo está dentro de `hyper`; solo `Http` necesita conexión.

**¿Dónde se guarda lo que escribo con `File.write("datos.txt", ...)`?**
Las rutas relativas parten de la carpeta del programa, no de la carpeta desde la que lo lanzas. Así un juego encuentra sus archivos lo abras como lo abras.

**El doble clic en un `.hz` abre otro programa.**
Vuelve a ejecutar el instalador (o `hyper register`): borra la elección anterior de Windows y vuelve a asociar los archivos.

**Windows dice "Windows protegió su PC".**
El instalador aún no está firmado digitalmente. Pulsa *Más información → Ejecutar de todas formas*.

**¿Cómo informo de un error?**
En las [incidencias de GitHub](https://github.com/cuevacaravacaadrian/HyperEZ/issues/new/choose), con el programa más pequeño que lo reproduzca y la salida de `hyper version`.

## ¿Por qué GitHub no muestra HyperEZ en la barra de lenguajes?

GitHub reconoce los lenguajes con [Linguist](https://github.com/github-linguist/linguist), y solo añade uno nuevo cuando ya se usa de verdad en GitHub: al menos **2000 archivos** con su extensión indexados en el último año, sin contar forks y repartidos entre muchas personas (no cuentan los repositorios del autor). Todo lo técnico para proponerlo ya está preparado en [editors/linguist](../editors/linguist/README.md), con un [plan por fases](../editors/linguist/OBJETIVOS.md) para llegar a esa cifra. **Tú puedes ayudar**: sube tus programas a GitHub con archivos `.hez`. Mientras tanto, el `.gitattributes` del repositorio hace que GitHub coloree los `.hez` y `.hz` con un lenguaje parecido.

## ¿Puedo usar HyperEZ en otro editor?

Sí: además de HyperEZ Editor y VS Code hay colores para Notepad++, Sublime Text, Vim y highlight.js, y cualquier editor con LSP (Neovim, Helix, Zed...) puede usar `hyper lsp`. Ver [otros editores](../editors/README.md).

## ¿Cómo hago una página web?

`hyper new mi-web --tipo web` (aplicación con plantillas, formularios y API) o `hyper new mi-blog --tipo sitio` (páginas desde Markdown), y `hyper serve` para verla con recarga automática. Todo en [hacer webs](biblioteca/web.md).
