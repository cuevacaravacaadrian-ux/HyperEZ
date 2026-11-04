# Inteligencia artificial y ciencia de datos

La base es `Matrix`, álgebra lineal programada en C++. Encima, en HyperEZ, están las bibliotecas `ai` (aprendizaje automático), `stats` (estadística) y `csv` (tablas).

## `Matrix`

```hz
var A = Matrix([[1, 2], [3, 4]])
var B = Matrix.identity(2)
print(A + B)                      // + - * / elemento a elemento
print(A * 10, 2 * A == A + A)     // con números
print(A.dot(A))                   // producto de matrices
print(A.transpose().toList(), A.sum(), A.mean(), A.argmax())
```

- Crear: `Matrix(filas, columnas, valor)`, `Matrix([[...], [...]])`, `Matrix.zeros`, `Matrix.ones`, `Matrix.identity`, `Matrix.random(filas, columnas, min, max)` (usa `randomSeed`).
- Leer: `rows`, `cols`, `get(i, j)`, `row(i)`, `col(j)`, `toList()`, `flatten()`.
- Calcular: `dot`, `transpose`, `sum(eje)`, `mean`, `max`, `min`, `argmax`, `pow`, `exp`, `log`, `sqrt`, `abs`, `clip`, `map(f)`.
- Redes neuronales: `sigmoid`, `relu`, `reluDeriv`, `tanh`, `softmax`.
- **Expansión** (*broadcasting*): una fila `1×n` o una columna `n×1` se repite para operar con una matriz entera: `X.dot(W) + sesgo`.

## `ai`: aprendizaje automático

### Redes neuronales

`NeuralNetwork([entradas, ocultas..., salidas], activacionOculta, activacionSalida)` crea un perceptrón multicapa que se entrena con descenso del gradiente.

```hz
import ai
randomSeed(1)
var red = ai.NeuralNetwork([2, 8, 1], "tanh", "sigmoid")
var X = [[0, 0], [0, 1], [1, 0], [1, 1]]
var Y = [[0], [1], [1], [0]]                // XOR
var error = red.train(X, Y, 2000, 0.5)      // épocas y tasa de aprendizaje
print(error < 0.05, red.classify(X), red)
```

- `train(entradas, esperado, epocas, tasa, lote, verbose)`: `lote` > 0 entrena por grupos (más rápido con muchos datos); `verbose` imprime el progreso. Devuelve el error final; `red.history` guarda el de cada época.
- `predict(entrada)` devuelve la salida (`Matrix`); `classify(entrada)` la clase de cada fila.
- Activaciones: `"sigmoid"`, `"relu"`, `"tanh"`, `"softmax"` (clasificación en varias clases, con entropía cruzada) y `"linear"`.

Clasificación en varias clases con `softmax` y `oneHot`:

```hz
import ai
randomSeed(2)
var puntos = [[0.1, 0.2], [0.2, 0.1], [0.9, 0.8], [0.8, 0.9], [0.1, 0.9], [0.2, 0.8]]
var clases = [0, 0, 1, 1, 2, 2]
var red = ai.NeuralNetwork([2, 8, 3], "relu", "softmax")
red.train(puntos, ai.oneHot(clases, 3), 1500, 0.3)
print(ai.accuracy(red.classify(puntos), clases))
```

### Otros modelos

```hz
import ai
var rl = ai.LinearRegression().fit([1, 2, 3, 4], [2.1, 3.9, 6.2, 7.8])
print(rl, rl.predict(5))

print(ai.knn([[1, 1], [2, 2], [9, 9]], ["bajo", "bajo", "alto"], [8, 8], 1))

randomSeed(3)
var g = ai.kmeans([[0, 0], [0, 1], [10, 10], [10, 11]], 2)
print(g.labels[0] == g.labels[1], g.labels[0] != g.labels[2])
```

Utilidades: `oneHot(etiquetas, clases)`, `normalize(datos)` (escala cada columna a 0..1), `accuracy(predichas, reales)`, `split(datos, 0.8)` (entrenamiento y prueba), `euclidean(a, b)`.

## `stats`: estadística

```hz
import stats
var notas = [6.5, 8, 9.2, 5, 7.5, 8]
print(stats.mean(notas), stats.median(notas), stats.mode(notas))
print(round(stats.stdev(notas), 3), stats.percentile(notas, 90))
print(stats.correlation([1, 2, 3, 4], [2, 4, 5, 8]))
print(stats.describe(notas))
print(stats.histogram(notas, 3).map(h => h.count))
```

## Datos

### JSON

```hz
var partida = { jugador: "Ana", nivel: 3, objetos: ["espada", "llave"] }
var texto = JSON.stringify(partida, 2)   // 2 = sangría
print(JSON.parse(texto).objetos[1])
```

### CSV

```hz
import csv
var filas = csv.parse("nombre,edad\nAna,30\nLuis,25\n")
print(filas[0].nombre, filas[1].edad + 1)       // los números se convierten solos
print(csv.stringify([{ a: 1, b: "x, y" }]))     // escapa comas y comillas
```

`csv.read(ruta)` y `csv.write(ruta, filas)` trabajan con archivos. Mira [examples/datos/ventas.hz](../../examples/datos/ventas.hz) para un análisis completo.

## Álgebra lineal con `Matrix`

```hz
var a = Matrix([[4, 7], [2, 6]])
print(a.det())                              // determinante: 10
print(a.inverse().toList())                 // inversa
print(a.dot(a.inverse()).equals(Matrix.identity(2), 0.000001))

// Sistema de ecuaciones: 2x + y = 3, x + 3y = 5
var x = Matrix([[2, 1], [1, 3]]).solve([3, 5])
print(x.toList())                           // [[0.8], [1.4]]

print(a.trace(), a.norm(), a.std(), a.diag())
print(Matrix([[1, 2, 3], [4, 5, 6]]).slice(0, 2, 1, 3).toList())   // filas 0..2, columnas 1..3
print(Matrix.concat([[1, 2]], [[3, 4]]).toList(), Matrix.concat([[1], [2]], [[3], [4]], 1).toList())
```

`det`, `inverse` y `solve` usan descomposición LU con pivoteo parcial (estable y rápida, en C++).
