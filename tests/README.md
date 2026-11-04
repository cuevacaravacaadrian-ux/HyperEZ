# Pruebas

`hyper test tests` ejecuta cada `.hz` de estas carpetas (o `ctest --test-dir build` para todas las baterías). Los archivos que empiezan por `_` son auxiliares y los `.out` son la salida esperada.

| Carpeta | Qué prueba |
|---|---|
| [lang](lang) | el lenguaje: sintaxis, clases, control del flujo, async y eventos, módulos, biblioteca básica, novedades |
| [lib](lib) | la biblioteca: sistema y procesos, binario, expresiones regulares, matrices, gráficos, servidor web y las bibliotecas `ai`, `stats`, `csv`, `cli`, `x86`, `web`, `html`, `db` y `audio` |
| [juegos](juegos) | lógica típica de juegos: colisiones, paso fijo, IA con estados, caminos, guardado, rendimiento |
| [errors](errors) | los mensajes de error de compilación y de ejecución (salida exacta) |
| [repl](repl) | una sesión grabada de la consola interactiva |
| [bench](bench) | benchmarks de rendimiento (no forman parte de las pruebas): `sh tests/bench/minbench.sh build/hyper.exe 7` |
