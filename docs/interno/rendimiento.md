# Rendimiento

## Cómo se compila `hyper`

- **C++20 con `-O2`/`-O3`** (GCC en Windows con MSYS2 UCRT64; GCC o Clang en Linux y macOS).
- **LTO** (optimización en tiempo de enlace): todo el programa se optimiza junto.
- **PGO** (optimización guiada por perfil, solo con GCC): `build.ps1` compila una versión instrumentada, la entrena ejecutando las pruebas, los ejemplos y los benchmarks, y recompila con esos datos. El compilador sabe así qué ramas son las habituales y cómo ordenar el código del intérprete.

```text
./build.ps1          con PGO (como las versiones publicadas)
./build.ps1 -NoPGO   la mitad de tiempo de compilación
```

## Benchmarks

`tests/bench/bench.hz` mide seis casos típicos; `tests/bench/minbench.sh` lo ejecuta varias veces y se queda con el mínimo:

```text
sh tests/bench/minbench.sh build/hyper.exe 7
```

| Caso | Qué hace | antes de 0.4 | 0.4.0 | 0.5.0 | 0.6.0 |
|---|---|---|---|---|---|
| `fib` | `fib(27)` recursivo con parámetros `: number` (≈630.000 llamadas) | 44 ms | 42 ms | 25 ms | **18 ms** |
| `bucle` | 3 millones de vueltas con `%` y una variable global | 118 ms | 80 ms | 72 ms | **72 ms** |
| `objetos` | 300.000 llamadas a un método que modifica dos campos | 52 ms | 31 ms | 21 ms | **17 ms** |
| `vec` | 300.000 sumas de `Vec2` (crea dos objetos por vuelta) | 69 ms | 61 ms | 62 ms | **18 ms** |
| `listas` | `map` + `filter` + `sum` sobre 200.000 elementos | 31 ms | 26 ms | 25 ms | **25 ms** |
| `texto` | concatenar 20.000 veces a un texto creciente | 282 ms | 238 ms | 151 ms | **148 ms** |

Medido en el mismo equipo (Windows 11, versiones compiladas con `build.ps1`). Los números absolutos dependen del equipo; lo importante es la evolución.

## Qué mejoró en cada versión

**0.5.0**: PGO; los textos se procesan una sola vez al internarlos (antes se calculaba su *hash* tres veces).

**0.6.0**:

- llamadas directas a funciones y métodos del script cuando los argumentos encajan;
- comprobación de tipos de parámetros sin copiar ni comparar textos (y una instrucción propia para `: number`);
- comparación y salto fusionados en una instrucción;
- bucle `for` numérico con una sola instrucción por vuelta;
- asignador de objetos pequeños con listas libres (`vec` es 3,4 veces más rápido).

En total, de la primera versión a la 0.6.0: llamadas 2,4 veces más rápidas, objetos 3 veces, vectores 3,8 veces, bucles 1,6 veces y textos 1,9 veces.

## Consejos para programas rápidos

- Dentro de funciones, las variables locales son más rápidas que las globales.
- Anotar los parámetros con tipos no ralentiza (y ayuda a encontrar errores).
- Para cálculos numéricos grandes usa `Matrix`: opera en C++ sobre bloques de memoria.
- Construir un texto muy largo con `+=` copia el texto cada vez; acumula las partes en una lista y usa `join("")` al final.
- `Time.measure(f)` dice cuánto tarda una función.
