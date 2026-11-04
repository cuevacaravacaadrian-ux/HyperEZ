# La herramienta `hyper`

Todo HyperEZ está en un solo ejecutable: `hyper` (`hyper.exe` en Windows).

| Orden | Qué hace |
|---|---|
| `hyper` | Consola interactiva (si se ejecuta en una terminal; si no, muestra la ayuda) |
| `hyper repl` | Consola interactiva |
| `hyper archivo.hz [args...]` | Ejecuta un programa; `System.args` recibe los argumentos |
| `hyper run [archivo.hz] [args...]` | Igual; sin archivo ejecuta `main.hz` de la carpeta actual |
| `hyper watch [archivo.hz] [args...]` | Ejecuta y vuelve a ejecutar cada vez que se guarda un `.hz` de la carpeta (o subcarpetas) |
| `hyper check [archivos o carpetas]` | Busca errores sin ejecutar |
| `hyper test [carpeta o archivos] [--update]` | Ejecuta las pruebas (por defecto `tests/`); `--update` regraba los `.out` |
| `hyper new <nombre> [--tipo juego\|app\|ia\|script]` | Crea un proyecto |
| `hyper docs [salida.md]` | Genera la referencia de toda la API en Markdown |
| `hyper lsp` | Servidor de lenguaje (lo usan los editores) |
| `hyper register` / `hyper unregister` | Asocia (o desasocia) los `.hz`/`.hez` con este `hyper` |
| `hyper version` | Versión |
| `hyper help` | Ayuda |

## Opciones de `run`

| Opción | |
|---|---|
| `--pause` | espera una tecla al terminar (lo usa el doble clic en Windows) |
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
