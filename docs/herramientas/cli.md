# La herramienta `hyper`

Todo HyperEZ está en un solo ejecutable: `hyper` (`hyper.exe` en Windows).

| Orden | Qué hace |
|---|---|
| `hyper` | Consola interactiva (si se ejecuta en una terminal; si no, muestra la ayuda) |
| `hyper repl` | Consola interactiva |
| `hyper archivo.hz [args...]` | Ejecuta un programa; `System.args` recibe los argumentos |
| `hyper run [archivo.hez] [args...]` | Igual; sin archivo ejecuta `main.hez` (o `main.hz`) de la carpeta actual |
| `hyper watch [archivo.hz] [args...]` | Ejecuta y vuelve a ejecutar cada vez que se guarda un `.hz` de la carpeta (o subcarpetas) |
| `hyper check [archivos o carpetas]` | Busca errores sin ejecutar |
| `hyper test [carpeta o archivos] [--update]` | Ejecuta las pruebas (por defecto `tests/`); `--update` regraba los `.out` |
| `hyper new <nombre> [--tipo juego\|app\|ia\|script\|web\|sitio]` | Crea un proyecto |
| `hyper serve [carpeta] [--puerto N] [--abrir] [--red]` | Sirve una web y recarga el navegador al guardar ([webs](../biblioteca/web.md#hyper-serve-servidor-para-desarrollar)) |
| `hyper fmt [archivos o carpetas] [--check]` | Ordena la sangría y los espacios (`--check`: solo dice qué archivos lo necesitan) |
| `hyper docs [salida.md]` | Genera la referencia de toda la API en Markdown |
| `hyper bytecode <archivo.hz>` | Muestra las instrucciones que genera el compilador |
| `hyper lsp` | Servidor de lenguaje (lo usan los editores) |
| `hyper register` / `hyper unregister` | Asocia (o desasocia) los `.hz`/`.hez` con este `hyper` |
| `hyper version` | Versión |
| `hyper help` | Ayuda |

## Opciones de `run`

| Opción | |
|---|---|
| `--as <ruta>` | ejecuta el archivo como si estuviera en `<ruta>`: las importaciones, `System.scriptDir` y los errores usan esa ruta. Lo usan HyperEZ Editor, VS Code y el editor web para **ejecutar sin guardar** |
| `--pause` | espera una tecla al terminar (lo usan el doble clic en Windows y F5 en VS Code); también vale para `hyper check` y `hyper test` |
| `--` | todo lo que sigue se pasa al programa aunque empiece por `-` |

## Códigos de salida

| Código | Significado |
|---|---|
| 0 | todo bien |
| 1 | errores de compilación, un error en ejecución sin capturar, o pruebas fallidas |
| 2 | uso incorrecto (falta un argumento, orden desconocida) |
| `n` | lo que indique `Process.exit(n)` |

## Variables de entorno

| Variable | Efecto |
|---|---|
| `NO_COLOR` | sin colores en los mensajes |
| `HYPEREZ_COLOR` | colores aunque la salida no sea una consola |

## Scripts ejecutables (Linux y macOS)

```text
#!/usr/bin/env hyper
print("hola desde un script")
```

Con `chmod +x saludo.hz` se ejecuta como `./saludo.hz`. También funciona con archivos sin extensión si la primera línea es `#!`.

## Formato: `hyper fmt`

Deja el código con la sangría de 4 espacios que marcan las llaves, quita los espacios al final de las líneas y las líneas en blanco de sobra, y termina el archivo con un salto de línea. **No cambia nada de lo que hace el programa**: antes de escribir comprueba que los tokens son exactamente los mismos. Respeta la alineación a mano de las líneas que continúan una expresión y no toca el interior de los textos `"""..."""` ni de los comentarios `/* */`.

```text
hyper fmt                    formatea todos los .hz y .hez de la carpeta (y subcarpetas)
hyper fmt juego.hez          un archivo
hyper fmt --check src        no cambia nada; termina con código 1 si algún archivo necesita formato
```

Lo mismo hacen *Formatear* (Ctrl+Mayús+F) en HyperEZ Editor y *Dar formato al documento* (Mayús+Alt+F) en VS Code.

## El bytecode: `hyper bytecode`

Enseña cómo traduce el compilador tu programa a instrucciones de la máquina virtual (útil para aprender cómo funciona por dentro o para ver qué optimizaciones aplica):

```text
> hyper bytecode suma.hz
== <principal> ==  29 bytes · 4 constantes · 5 huecos · 0 parámetro(s)
     0     1  CONST                  #0                ; 0
     3     |  DEFINE_GLOBAL          g0                ; suma
     ...
    13     2  FOR_CHECK_V            s1 0 s4  → 28
    21     |  GET_GLOBAL             g0                ; suma
    24     |  GET_LOCAL              s4
    ...
```

Cada línea: posición, línea del código fuente, instrucción, operandos (`#` constante, `s` hueco local, `g` variable global, `→`/`↑` destino de un salto) y, tras `;`, lo que significan. Las instrucciones se explican en [bytecode.md](../interno/bytecode.md).

## Tipos de proyecto

| `--tipo` | Qué crea |
|---|---|
| `juego` (por defecto) | juego de consola con teclado |
| `app` | aplicación de consola con menú y datos guardados en JSON |
| `ia` | red neuronal que aprende de un CSV |
| `script` | herramienta de línea de órdenes con argumentos |
| `web` | aplicación web: plantillas HTML, un formulario, una API JSON y CSS |
| `sitio` | sitio web estático escrito en Markdown (blog, documentación, portfolio) |

Todos tienen `main.hez`, `src/` (o las carpetas de la web), `tests/` con pruebas que pasan desde el primer momento y un `README.md`, y `.github/workflows/pruebas.yml`, que ejecuta las pruebas en GitHub con cada cambio (con la [acción `setup-hyperez`](../../actions/setup-hyperez/README.md)).
