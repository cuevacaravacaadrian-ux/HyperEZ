# Tutorial: una red neuronal

Una red neuronal aprende a partir de ejemplos. Aquí enseñaremos a una a decidir si un punto está **dentro o fuera de un círculo**, sin decirle nunca qué es un círculo.

## 1. Los datos

Cada ejemplo es una entrada (`[x, y]`) y su respuesta correcta (1 dentro, 0 fuera):

```hz
randomSeed(1)
var puntos = []
var dentro = []
repeat 200 {
    var x = random(-1, 1)
    var y = random(-1, 1)
    puntos.add([x, y])
    dentro.add(x * x + y * y < 0.36 ? 1 : 0)   // círculo de radio 0,6
}
print(puntos.length, "ejemplos;", dentro.count(d => d == 1), "dentro")
```

## 2. La red

`ai.NeuralNetwork([2, 12, 2], "relu", "softmax")` tiene:

- 2 entradas (x e y);
- una capa oculta de 12 neuronas con activación ReLU;
- 2 salidas con *softmax*: la probabilidad de "fuera" y la de "dentro".

Para entrenarla, las respuestas se escriben en formato *one-hot*: la clase 1 es `[0, 1]`.

## 3. Entrenar y comprobar

Separamos 160 ejemplos para aprender y 40 para comprobar con puntos que la red no ha visto:

```hz
import ai
randomSeed(1)
var puntos = []
var dentro = []
repeat 200 {
    var x = random(-1, 1)
    var y = random(-1, 1)
    puntos.add([x, y])
    dentro.add(x * x + y * y < 0.36 ? 1 : 0)
}

var red = ai.NeuralNetwork([2, 12, 2], "relu", "softmax")
var error = red.train(puntos.take(160), ai.oneHot(dentro.take(160), 2), 300, 0.3, 32)
var acierto = ai.accuracy(red.classify(puntos.skip(160)), dentro.skip(160))
print("Error final:", round(error, 3))
print("Aciertos con puntos nuevos: ${round(acierto * 100)} %")
assert(acierto > 0.8, "la red debería aprender el círculo")

// Lo que ha aprendido: # dentro, . fuera
for fila in 0..9 {
    var linea = ""
    for col in 0..19 { linea += red.classify([[col / 9 - 1, fila / 4 - 1]])[0] == 1 ? "#" : "." }
    print(linea)
}
```

## 4. Qué está pasando

- `train` repite muchas veces (**épocas**): calcula la salida, mide el error y ajusta un poco los pesos en la dirección que lo reduce (**descenso del gradiente**). La **tasa de aprendizaje** (`0.3`) es el tamaño de cada ajuste.
- Con `lote` (`32`) los ejemplos se procesan en grupos mezclados: aprende más rápido y generaliza mejor.
- Todo el cálculo se hace con `Matrix` (en C++); la red en sí son unas 150 líneas de HyperEZ en [src/lib/ai.hz](../../src/lib/ai.hz). Léelas: es la mejor forma de entender cómo funciona.

## Para seguir

- Cambia el número de neuronas, las épocas o la tasa y observa el error (`red.history` guarda el de cada época).
- Prueba con datos reales en CSV: `hyper new mi-ia --tipo ia` crea un proyecto que clasifica flores.
- [Guía de IA y datos](../biblioteca/ia-datos.md) con regresión lineal, k vecinos y k-medias.
