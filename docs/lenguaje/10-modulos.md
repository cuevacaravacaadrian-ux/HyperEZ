# 10. Módulos

Cada archivo `.hz` es un módulo. Todo lo que se declara en él (funciones, clases, variables, enums...) se puede usar desde otros archivos, salvo lo marcado como `private`.

```text
// enemigos.hz
class Orco { var vida = 30 }
const VELOCIDAD = 2
private var contador = 0          // solo se ve dentro de enemigos.hz

// main.hz
from "./enemigos.hz" import Orco, VELOCIDAD   // trae nombres concretos
import "./enemigos.hz" as enemigos            // trae el módulo entero con un nombre

var o = Orco()
var o2 = enemigos.Orco()
print(VELOCIDAD, enemigos.VELOCIDAD)
```

- Las rutas entre comillas son relativas al archivo que importa.
- Un módulo se ejecuta una sola vez, la primera vez que se importa.
- Los nombres importados conservan sus tipos: el editor autocompleta y avisa de errores igual que en el propio archivo.

## Importar por nombre

Sin comillas, `import nombre` busca, por este orden:

1. `nombre.hz` junto al archivo que importa;
2. en la carpeta del proyecto: `src/nombre.hz`, `nombre.hz` y `hz_packages/nombre/`;
3. las **bibliotecas incluidas** con HyperEZ.

Un punto separa carpetas: `import juego.enemigos` busca `juego/enemigos.hz`.

## Bibliotecas incluidas

Vienen dentro de `hyper` (no hay que instalar nada) y están escritas en HyperEZ:

| Biblioteca | Para qué sirve |
|---|---|
| `ai` | redes neuronales, regresión lineal, k vecinos, k-medias, normalización |
| `stats` | media, mediana, desviación típica, percentiles, correlación, histogramas |
| `csv` | leer y escribir tablas CSV |
| `cli` | argumentos de línea de órdenes, preguntas, barras de progreso, tablas |
| `x86` | ensamblador x86 de 16 bits para crear sectores de arranque |
| `web` | aplicaciones web con rutas (`/jugadores/:id`) sobre `Http.serve` |
| `html` | generar HTML con escape automático |
| `db` | base de datos de documentos guardada en JSON |
| `audio` | sonidos y música en archivos WAV |

```hz
import stats
print(stats.median([3, 1, 4, 1, 5]), stats.stdev([2, 4, 4, 4, 5, 5, 7, 9]))

from stats import mean, percentile
print(mean([1, 2, 3]), percentile([1, 2, 3, 4, 5], 90))
```

Los módulos nativos (`File`, `Http`, `Matrix`, `Console`...) están siempre disponibles sin importar nada. La lista completa está en la [biblioteca estándar](../biblioteca/README.md).
