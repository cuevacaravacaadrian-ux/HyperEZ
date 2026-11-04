# Sistema operativo: procesos, archivos, rutas y tiempo

Con estos módulos HyperEZ sirve como lenguaje de *scripts*: automatizar tareas, organizar archivos, lanzar otros programas o construir herramientas.

## Archivos: `File`

Las rutas relativas parten de la carpeta del programa (no de la carpeta desde la que se lanza).

```hz
var carpeta = Path.join(Path.temp, "hz_doc_archivos")
File.write(Path.join(carpeta, "notas.txt"), "primera línea\n")   // crea las carpetas que falten
File.append(Path.join(carpeta, "notas.txt"), "segunda línea\n")
print(File.read(Path.join(carpeta, "notas.txt")))
print(File.readLines(Path.join(carpeta, "notas.txt")).length)
File.copy(Path.join(carpeta, "notas.txt"), Path.join(carpeta, "copia/notas.txt"))
print(File.listAll(carpeta), File.size(Path.join(carpeta, "notas.txt")))
File.deleteDir(carpeta)   // borra la carpeta y todo su contenido
print(File.exists(carpeta))
```

| Función | Qué hace |
|---|---|
| `read`, `readLines`, `readBytes` | leer como texto, como lista de líneas o como `Bytes` |
| `write`, `append`, `writeBytes` | escribir (crea o reemplaza), añadir al final, escribir binario |
| `exists`, `isFile`, `isDir`, `size`, `modified` | información |
| `copy`, `move`, `delete`, `deleteDir`, `makeDir` | copiar, mover o renombrar, borrar |
| `list`, `listAll` | contenido de una carpeta (`listAll` entra en subcarpetas y filtra por extensión) |
| `tempFile` | una ruta nueva en la carpeta temporal |

`File.deleteDir` se niega a borrar la raíz del disco o la carpeta del propio proyecto, por seguridad.

## Rutas: `Path`

`Path` trabaja con el texto de las rutas sin tocar el disco:

```hz
var r = Path.join("datos", "niveles", "nivel1.json")
print(r, Path.dir(r), Path.name(r), Path.stem(r), Path.ext(r))
print(Path.withExt(r, ".bak"), Path.normalize("a/./b/../c"))
print(Path.isAbsolute(r), Path.home.length > 0, Path.separator)
```

## Otros programas: `Process`

`Process.run` ejecuta un programa con sus argumentos (sin pasar por la consola del sistema, así que no hay problemas con espacios o comillas). `Process.shell` ejecuta una línea de órdenes tal cual (cmd en Windows, sh en Linux y macOS). Los dos esperan a que termine y devuelven `{code, output, error, ok, timedOut}`.

```hz
var r = Process.run(System.exe, ["version"])
print(r.ok, r.code, r.output.trim())

print(Process.shell("echo hola desde el sistema").output.trim())
```

Opciones (último argumento): `cwd` (carpeta de trabajo), `input` (texto que recibe por la entrada), `timeout` (segundos) y `env` (mapa de variables de entorno):

```text
Process.run("git", ["status"], { cwd: "mi-proyecto", timeout: 10 })
Process.run("python", ["script.py"], { input: "datos\n", env: { MODO: "prueba" } })
```

### En segundo plano

`Process.start` lanza un programa sin esperar y devuelve un control con `wait(segundos)`, `kill()` y `running`:

```text
var servidor = Process.start(System.exe, ["servidor.hz"])
// ... hacer peticiones al servidor ...
servidor.kill()
```

`Process.open(destino)` abre un archivo, una carpeta o una web con la aplicación predeterminada; `Process.exit(codigo)` termina el programa; `Process.pid` es el número del proceso.

## Información del sistema: `System`

```hz
print(System.platform, System.arch, System.cpuCount > 0)
print(System.version, System.args)         // argumentos: hyper programa.hz uno dos
print(System.env("PATH") != null)
System.setEnv("MI_VARIABLE", "valor")      // también la ven los programas que lances
print(System.env("MI_VARIABLE"))
```

Más propiedades: `System.user`, `System.hostname`, `System.cwd`, `System.scriptDir` (carpeta del programa) y `System.exe` (ruta de `hyper`). `System.sleep(segundos)` detiene el programa; dentro de funciones `async` usa `await wait(segundos)`.

## Fechas y horas: `Time`

```hz
var ahora = Time.now()                          // segundos desde 1970 (con decimales)
print(Time.format(ahora, "%d/%m/%Y %H:%M") != "")
var cumple = Time.make(2026, 12, 25, 18, 30)    // fecha local -> segundos
var d = Time.date(cumple)
print(d.year, d.month, d.day, d.hour, d.dayName)
print(round((cumple - Time.make(2026, 12, 24)) / 3600), "horas de diferencia")
print(Time.measure(() => System.sleep(0.01)) > 0) // cuánto tarda una función
```

Formatos de `Time.format` (los de `strftime`): `%Y` año, `%m` mes, `%d` día, `%H` hora, `%M` minuto, `%S` segundo, `%A` nombre del día, `%B` nombre del mes, `%j` día del año.

## Un script completo

El ejemplo [examples/sistema/organizar.hz](../../examples/sistema/organizar.hz) ordena los archivos de una carpeta en subcarpetas según su tipo.
