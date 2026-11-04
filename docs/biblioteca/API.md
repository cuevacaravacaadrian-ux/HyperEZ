# Referencia de la API de HyperEZ 0.6.0

> Generada automáticamente con `hyper docs` a partir del catálogo de la máquina virtual.

## Contenido

- [Funciones globales](#funciones-globales)
- [Constantes](#constantes)
- Tipos y clases: [Bytes](#clase-bytes), [Color](#clase-color), [Enum](#clase-enum), [EnumValue](#clase-enumvalue), [Error](#clase-error), [Event](#clase-event), [EventData](#clase-eventdata), [Image](#clase-image), [Matrix](#clase-matrix), [Module](#clase-module), [ProcessHandle](#clase-processhandle), [Range](#clase-range), [Regex](#clase-regex), [Subscription](#clase-subscription), [Task](#clase-task), [Vec2](#clase-vec2), [Vec3](#clase-vec3), [bool](#clase-bool), [function](#clase-function), [list](#clase-list), [map](#clase-map), [number](#clase-number), [set](#clase-set), [text](#clase-text)
- Módulos: [Base64](#modulo-base64), [Console](#modulo-console), [File](#modulo-file), [Hash](#modulo-hash), [Http](#modulo-http), [JSON](#modulo-json), [Path](#modulo-path), [Process](#modulo-process), [System](#modulo-system), [Time](#modulo-time)
- [Bibliotecas para import](#bibliotecas)

## Funciones globales

#### `abs(x: number): number`

Valor absoluto.

#### `acos(x: number): number`

Arcocoseno (radianes).

#### `approach(actual: number, objetivo: number, paso: number): number`

Acerca 'actual' a 'objetivo' como mucho 'paso' sin pasarse.

#### `asin(x: number): number`

Arcoseno (radianes).

#### `assert(condicion: any, mensaje?: text): void`

Lanza un error si la condición es falsa.

#### `assertEqual(actual: any, esperado: any, mensaje?: text): void`

Lanza un error si los valores no son iguales (los números se comparan con una pequeña tolerancia).

#### `assertThrows(funcion: function, mensaje?: text): Error`

Comprueba que la función lanza un error y lo devuelve.

#### `atan(x: number): number`

Arcotangente (radianes).

#### `atan2(y: number, x: number): number`

Ángulo (radianes) del vector (x, y).

#### `ceil(x: number): number`

Redondea hacia arriba.

#### `chance(probabilidad: number): bool`

true con la probabilidad indicada (0 a 1). chance(0.25) es true 1 de cada 4 veces.

#### `clamp(x: number, minimo: number, maximo: number): number`

Limita x al intervalo [minimo, maximo].

#### `cos(radianes: number): number`

Coseno.

#### `degrees(radianes: number): number`

Convierte radianes a grados.

#### `distance(x1: number, y1: number, x2: number, y2: number): number`

Distancia entre dos puntos.

#### `exit(codigo?: number): void`

Termina el programa inmediatamente.

#### `exp(x: number): number`

e elevado a x.

#### `floor(x: number): number`

Redondea hacia abajo.

#### `hypot(x: number, y: number): number`

Longitud de (x, y).

#### `input(mensaje?: text): text?`

Lee una línea escrita por el usuario en la consola.

```hz
var nombre = input("¿Cómo te llamas? ")
```

#### `isNaN(x: any): bool`

true si x no es un número válido (NaN).

#### `len(valor: any): number`

Longitud de un texto, lista, mapa, conjunto o rango.

#### `lerp(a: number, b: number, t: number): number`

Interpolación lineal entre a y b (t de 0 a 1).

#### `log(x: number, base?: number): number`

Logaritmo natural (o en la base indicada).

#### `log10(x: number): number`

Logaritmo en base 10.

#### `max(...valores: number|list): number`

El mayor de los valores (o de una lista).

#### `min(...valores: number|list): number`

El menor de los valores (o de una lista).

#### `nextFrame(): Task`

Tarea que se completa en el siguiente fotograma.

```hz
await nextFrame()
```

#### `now(): number`

Hora actual del sistema en segundos desde 1970 (con milisegundos).

#### `off(suscripcion: any): void`

Cancela una suscripción de 'on', o todas las de un evento.

#### `parseInt(texto: text, base: number = 10): number?`

Número entero escrito en una base: parseInt("ff", 16) es 255; admite 0x, 0b y 0o. null si no es válido.

#### `pow(base: number, exponente: number): number`

Potencia.

#### `print(...valores: any): void`

Muestra valores en la consola separados por espacios, con salto de línea.

```hz
print("Hola", nombre)
```

#### `println(...valores: any): void`

Igual que print.

#### `radians(grados: number): number`

Convierte grados a radianes.

#### `random(min?: number, max?: number): number`

Número aleatorio: random() en [0,1), random(max) o random(min, max).

#### `randomChoice(lista: list): any`

Elemento aleatorio de una lista.

#### `randomInt(min: number, max: number): int`

Entero aleatorio entre min y max (ambos incluidos).

#### `randomSeed(semilla: number): void`

Fija la semilla del generador aleatorio (resultados repetibles).

#### `range(inicio: number, fin?: number, paso: number = 1): Range`

Crea un rango de números (fin excluido).

#### `remap(x: number, desdeMin: number, desdeMax: number, hastaMin: number, hastaMax: number): number`

Convierte x de un intervalo a otro (p. ej. de 0..100 a 0..1).

#### `round(x: number, decimales: number = 0): number`

Redondea al entero (o a N decimales) más cercano.

#### `set(...valores: any): set`

Crea un conjunto (valores sin repetir).

```hz
var vistos = set(1, 2, 3)
```

#### `sign(x: number): number`

-1, 0 o 1 según el signo.

#### `sin(radianes: number): number`

Seno.

#### `smoothstep(desde: number, hasta: number, x: number): number`

Transición suave de 0 a 1 entre desde y hasta.

#### `spawn(funcion: function, ...args: any): Task`

Ejecuta una función (normalmente async) en segundo plano y devuelve su tarea.

#### `sqrt(x: number): number`

Raíz cuadrada.

#### `str(valor: any): text`

Convierte cualquier valor en texto.

#### `tan(radianes: number): number`

Tangente.

#### `test(nombre: text, cuerpo: function): bool`

Ejecuta una prueba con nombre. Muestra ✓ o ✗ sin detener el programa; 'hyper test' falla si alguna prueba falla.

```hz
test("sumar", () => assertEqual(2 + 2, 4))
```

#### `time(): number`

Segundos transcurridos desde que empezó el programa (o el juego).

#### `toInt(valor: number): int`

Quita los decimales de un número.

#### `toNumber(valor: any, porDefecto?: any): number?`

Convierte texto o bool en número. Devuelve porDefecto (o null) si no es posible.

#### `typeof(valor: any): text`

Nombre del tipo de un valor ("number", "text", "list", nombre de clase...).

#### `wait(segundos: number): Task`

Tarea que se completa tras N segundos. Úsala con await dentro de funciones async.

```hz
await wait(1.5)
```

#### `wrap(x: number, minimo: number, maximo: number): number`

Da la vuelta: si x se sale por un lado, entra por el otro (útil para pantallas que se repiten).

#### `write(...valores: any): void`

Muestra valores sin salto de línea.

#### `async function awaitAll(tareas: list): Task<list>`

Espera a que terminen todas las tareas y devuelve sus resultados en el mismo orden.

#### `async function awaitAny(tareas: list)`

Devuelve el resultado de la primera tarea que termine.

## Constantes

| Nombre | Tipo | Descripción |
|---|---|---|
| `PI` | `number` | π ≈ 3.14159 |
| `TAU` | `number` | 2π ≈ 6.28318 (una vuelta completa) |
| `E` | `number` | Número e ≈ 2.71828 |
| `INFINITY` | `number` | Infinito |

## Clase Bytes

Bloque de bytes (números de 0 a 255) para datos binarios: formatos de archivo, protocolos, imágenes de disco... Se lee y escribe con b[i], y con readU16/writeU32... para números de varios bytes.

Constructor: `Bytes(contenido: number|text|list = 0, relleno: number = 0)`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.length` | `number` | Número de bytes. *(solo lectura)* |

#### `.compress(nivel: number = 8): Bytes`

Comprime con zlib (deflate); nivel de 1 (rápido) a 9 (pequeño).

#### `.concat(otro: Bytes|text): Bytes`

Nuevos Bytes con los dos bloques seguidos.

#### `.copy(): Bytes`

Copia independiente.

#### `.decompress(): Bytes`

Descomprime datos zlib.

#### `.equals(otro: Bytes): bool`

¿Tienen exactamente los mismos bytes?

#### `.fill(valor: number): Bytes`

Pone el mismo byte en todas las posiciones.

#### `Bytes.fromBase64(texto: text): Bytes`

Bytes a partir de texto Base64.

#### `Bytes.fromHex(hex: text): Bytes`

Bytes a partir de texto hexadecimal ("de ad be ef").

#### `.get(i: number): number`

Byte en la posición i (también b[i]).

#### `.indexOf(buscado: number|text|Bytes, desde: number = 0): number`

Posición de un byte o secuencia, o -1.

#### `.push(...valores: any): Bytes`

Añade al final números (bytes), listas, textos u otros Bytes.

#### `.pushF32(valor: number, littleEndian: bool = true): Bytes`

Añade un decimal de 32 bits.

#### `.pushF64(valor: number, littleEndian: bool = true): Bytes`

Añade un decimal de 64 bits.

#### `.pushI64(valor: number, littleEndian: bool = true): Bytes`

Añade un entero de 64 bits.

#### `.pushU16(valor: number, littleEndian: bool = true): Bytes`

Añade un entero de 16 bits.

#### `.pushU32(valor: number, littleEndian: bool = true): Bytes`

Añade un entero de 32 bits.

#### `.readF32(posicion: number, littleEndian: bool = true): number`

Lee un decimal de 32 bits (float).

#### `.readF64(posicion: number, littleEndian: bool = true): number`

Lee un decimal de 64 bits (double).

#### `.readI16(posicion: number, littleEndian: bool = true): number`

Lee un entero de 16 bits con signo.

#### `.readI32(posicion: number, littleEndian: bool = true): number`

Lee un entero de 32 bits con signo.

#### `.readI64(posicion: number, littleEndian: bool = true): number`

Lee un entero de 64 bits con signo.

#### `.readI8(posicion: number, littleEndian: bool = true): number`

Lee un byte con signo (-128 a 127).

#### `.readU16(posicion: number, littleEndian: bool = true): number`

Lee un entero de 16 bits sin signo.

#### `.readU32(posicion: number, littleEndian: bool = true): number`

Lee un entero de 32 bits sin signo.

#### `.readU8(posicion: number, littleEndian: bool = true): number`

Lee un byte sin signo.

#### `.resize(tamano: number): Bytes`

Cambia el tamaño (los bytes nuevos valen 0).

#### `.set(i: number, valor: number): void`

Cambia el byte de la posición i (también b[i] = valor).

#### `.slice(desde: number = 0, hasta?: number): Bytes`

Copia de una parte (hasta no incluido; admite negativos).

#### `.toBase64(): text`

Codifica en Base64.

#### `.toHex(separador: text = ""): text`

Representación hexadecimal: "48656c6c6f".

#### `.toList(): list<number>`

Los bytes como lista de números.

#### `.toString(): text`

Descripción corta.

#### `.toText(): text`

Interpreta los bytes como texto UTF-8.

#### `.writeF32(posicion: number, valor: number, littleEndian: bool = true): void`

Escribe un decimal de 32 bits.

#### `.writeF64(posicion: number, valor: number, littleEndian: bool = true): void`

Escribe un decimal de 64 bits.

#### `.writeI64(posicion: number, valor: number, littleEndian: bool = true): void`

Escribe un entero de 64 bits.

#### `.writeU16(posicion: number, valor: number, littleEndian: bool = true): void`

Escribe un entero de 16 bits.

#### `.writeU32(posicion: number, valor: number, littleEndian: bool = true): void`

Escribe un entero de 32 bits.

#### `.writeU8(posicion: number, valor: number, littleEndian: bool = true): void`

Escribe un byte.

## Clase Color

Color RGBA con componentes de 0 a 255. También acepta Color("red") o Color("#ff8800").

Constructor: `Color(r: number|text|list = 255, g: number = 255, b: number = 255, a: number = 255)`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.BEIGE` | `Color` | Color beige. *(estática)* *(solo lectura)* |
| `.BLACK` | `Color` | Color black. *(estática)* *(solo lectura)* |
| `.BLUE` | `Color` | Color blue. *(estática)* *(solo lectura)* |
| `.BROWN` | `Color` | Color brown. *(estática)* *(solo lectura)* |
| `.CYAN` | `Color` | Color cyan. *(estática)* *(solo lectura)* |
| `.DARKBLUE` | `Color` | Color darkblue. *(estática)* *(solo lectura)* |
| `.DARKGRAY` | `Color` | Color darkgray. *(estática)* *(solo lectura)* |
| `.DARKGREEN` | `Color` | Color darkgreen. *(estática)* *(solo lectura)* |
| `.GOLD` | `Color` | Color gold. *(estática)* *(solo lectura)* |
| `.GRAY` | `Color` | Color gray. *(estática)* *(solo lectura)* |
| `.GREEN` | `Color` | Color green. *(estática)* *(solo lectura)* |
| `.GREY` | `Color` | Color grey. *(estática)* *(solo lectura)* |
| `.LIGHTGRAY` | `Color` | Color lightgray. *(estática)* *(solo lectura)* |
| `.LIME` | `Color` | Color lime. *(estática)* *(solo lectura)* |
| `.MAGENTA` | `Color` | Color magenta. *(estática)* *(solo lectura)* |
| `.MAROON` | `Color` | Color maroon. *(estática)* *(solo lectura)* |
| `.ORANGE` | `Color` | Color orange. *(estática)* *(solo lectura)* |
| `.PINK` | `Color` | Color pink. *(estática)* *(solo lectura)* |
| `.PURPLE` | `Color` | Color purple. *(estática)* *(solo lectura)* |
| `.RED` | `Color` | Color red. *(estática)* *(solo lectura)* |
| `.SKYBLUE` | `Color` | Color skyblue. *(estática)* *(solo lectura)* |
| `.TRANSPARENT` | `Color` | Color transparent. *(estática)* *(solo lectura)* |
| `.VIOLET` | `Color` | Color violet. *(estática)* *(solo lectura)* |
| `.WHITE` | `Color` | Color white. *(estática)* *(solo lectura)* |
| `.YELLOW` | `Color` | Color yellow. *(estática)* *(solo lectura)* |
| `.a` | `number` | Componente a (0-255). |
| `.b` | `number` | Componente b (0-255). |
| `.g` | `number` | Componente g (0-255). |
| `.r` | `number` | Componente r (0-255). |

#### `.brighten(factor: number): Color`

Más claro (>1) o más oscuro (<1).

#### `.copy(): Color`

Copia independiente.

#### `Color.hex(codigo: text): Color`

Color desde "#rrggbb" o "#rrggbbaa".

#### `Color.hsv(tono: number, saturacion: number = 1, brillo: number = 1, alfa: number = 1): Color`

Color desde tono (0-360), saturación y brillo (0-1). Ideal para arcoíris.

#### `.lerp(otro: Color, t: number): Color`

Mezcla con otro color.

#### `Color.rgb(r: number, g: number, b: number, a: number = 255): Color`

Color desde componentes 0-255.

#### `.toHex(): text`

Texto "#rrggbb".

#### `.withAlpha(a: number): Color`

Copia con otra transparencia (0-255).

## Clase Enum

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.count` | `number` | Cantidad de valores. *(solo lectura)* |
| `.name` | `text` | Nombre del enum. *(solo lectura)* |

#### `.fromName(nombre: text): any`

Valor con ese nombre (o null).

#### `.fromValue(valor: any): any`

Valor con ese valor asociado (o null).

#### `.values(): list`

Todos los valores del enum en orden.

## Clase EnumValue

## Clase Error

Error lanzable con throw. Campos: message, name, stack, file, line, column.

Constructor: `Error(mensaje?: text, tipo?: text)`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.column` | `number` |  |
| `.file` | `text` |  |
| `.line` | `number` |  |
| `.message` | `text` |  |
| `.name` | `text` |  |
| `.stack` | `list<text>` | Pila de llamadas cuando se creó el error. |

#### `.toString(): text`

Texto 'Tipo: mensaje'.

## Clase Event

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.listenerCount` | `number` | Número de manejadores activos. *(solo lectura)* |
| `.name` | `text` | Nombre del evento. *(solo lectura)* |

#### `.clear(): void`

Quita todos los manejadores.

## Clase EventData

Datos que recibe un manejador 'on'. Además de estos campos tiene uno por cada parámetro del evento.

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.data` | `list` | Todos los valores emitidos, en orden. |
| `.defaultPrevented` | `bool` | true si se llamó a preventDefault(). |
| `.name` | `text` | Nombre del evento. |
| `.target` | `any` | Objeto que emitió el evento (si aplica). |
| `.timestamp` | `number` | Momento de la emisión (segundos). |

#### `.preventDefault(): void`

Marca el evento para que el emisor no haga su acción por defecto.

#### `.stopPropagation(): void`

Detiene el despacho: los manejadores siguientes no se ejecutan.

## Clase Image

Imagen en memoria (píxeles RGBA) para generar gráficos, mapas, texturas o banners: se dibuja con transparencia, se escribe texto y se guarda como PNG, JPG, BMP o TGA.

Constructor: `Image(ancho: number, alto: number, fondo?: Color)`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.height` | `number` | Alto en píxeles. *(solo lectura)* |
| `.width` | `number` | Ancho en píxeles. *(solo lectura)* |

#### `.circle(x: number, y: number, radio: number, color?: Color, relleno: bool = true): Image`

Círculo relleno o solo la circunferencia.

#### `.copy(): Image`

Copia independiente.

#### `.crop(x: number, y: number, ancho: number, alto: number): Image`

Recorte (imagen nueva).

#### `.fill(color: Color): Image`

Pinta toda la imagen de un color.

#### `.flipH(): Image`

Voltea horizontalmente.

#### `.flipV(): Image`

Voltea verticalmente.

#### `Image.fromBytes(ancho: number, alto: number, pixeles: Bytes): Image`

Imagen a partir de píxeles RGBA.

#### `.get(x: number, y: number): Color`

Color de un píxel.

#### `.grayscale(): Image`

Pasa a escala de grises.

#### `.invert(): Image`

Invierte los colores.

#### `.line(x0: number, y0: number, x1: number, y1: number, color?: Color, grosor: number = 1): Image`

Línea con bordes suaves.

#### `Image.load(ruta: text): Image`

Lee una imagen PNG, JPG, BMP, GIF o TGA.

#### `.paste(otra: Image, x: number = 0, y: number = 0): Image`

Dibuja otra imagen encima (con su transparencia).

#### `.pixels(): Bytes`

Los píxeles como Bytes (R, G, B, A por píxel, fila a fila).

#### `.rect(x: number, y: number, ancho: number, alto: number, color?: Color, relleno: bool = true): Image`

Rectángulo relleno o solo el borde.

#### `.resize(ancho: number, alto: number, suave: bool = true): Image`

Imagen nueva con otro tamaño.

#### `.save(ruta: text, calidad: number = 90): void`

Guarda según la extensión: .png, .jpg, .bmp o .tga.

#### `.set(x: number, y: number, color: Color): void`

Cambia un píxel (sin mezclar).

#### `.text(x: number, y: number, texto: any, color?: Color, tamano: number = 16, fuente?: text): number`

Escribe texto (fuente DejaVu Sans incluida, o un archivo .ttf) y devuelve su ancho en píxeles.

#### `Image.textWidth(texto: any, tamano: number = 16, fuente?: text): number`

Ancho que ocupará un texto.

#### `.toString(): text`

Descripción.

## Clase Matrix

Matriz de números (filas × columnas) calculada en C++: la base para redes neuronales, estadística y gráficos. Se opera con +, -, *, / (elemento a elemento o con un número) y dot() para el producto de matrices.

Constructor: `Matrix(filas: number|list, columnas: number = 1, valor: number = 0)`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.cols` | `number` | Número de columnas. *(solo lectura)* |
| `.rows` | `number` | Número de filas. *(solo lectura)* |
| `.size` | `number` | Número total de valores (filas × columnas). *(solo lectura)* |

#### `.abs(): Matrix`

Valor absoluto en cada valor.

#### `.add(otra: Matrix|number): Matrix`

Suma elemento a elemento (también con +).

#### `.argmax(): list<number>`

Para cada fila, la columna del valor máximo (la clase predicha en clasificación).

#### `.clip(minimo: number, maximo: number): Matrix`

Limita cada valor al intervalo.

#### `.col(columna: number): list<number>`

Una columna como lista.

#### `.copy(): Matrix`

Copia independiente.

#### `.div(otra: Matrix|number): Matrix`

Divide elemento a elemento (también con /).

#### `.dot(otra: Matrix): Matrix`

Producto de matrices (filas por columnas).

#### `.equals(otra: Matrix, tolerancia: number = 0.000000001): bool`

¿Misma forma y valores (con tolerancia)?

#### `.exp(): Matrix`

e^x en cada valor.

#### `.flatten(): list<number>`

Todos los valores en una sola lista, fila tras fila.

#### `Matrix.fromList(filas: list): Matrix`

Matriz a partir de [[1, 2], [3, 4]] (o una fila [1, 2, 3]).

#### `.get(fila: number, columna: number = 0): number`

Valor en (fila, columna).

#### `Matrix.identity(n: number): Matrix`

Matriz identidad n×n.

#### `.log(): Matrix`

Logaritmo natural en cada valor.

#### `.map(f: function): Matrix`

Aplica f a cada valor (más lento que las funciones nativas).

#### `.max(): number`

Valor máximo.

#### `.mean(): number`

Media de todos los valores.

#### `.min(): number`

Valor mínimo.

#### `.mul(otra: Matrix|number): Matrix`

Multiplica elemento a elemento (también con *). Para el producto de matrices usa dot().

#### `Matrix.ones(filas: number, columnas: number = 1): Matrix`

Matriz de unos.

#### `.pow(exponente: number): Matrix`

Eleva cada valor a un exponente.

#### `Matrix.random(filas: number, columnas: number, minimo: number = -1, maximo: number = 1): Matrix`

Valores aleatorios uniformes (usa la semilla de randomSeed).

#### `.relu(): Matrix`

ReLU: max(0, x) en cada valor.

#### `.reluDeriv(): Matrix`

Derivada de ReLU: 1 si x > 0, si no 0.

#### `.reshape(filas: number, columnas: number): Matrix`

Los mismos valores con otra forma.

#### `.row(fila: number): list<number>`

Una fila como lista.

#### `.set(fila: number, columna: number, valor: number): void`

Cambia el valor en (fila, columna).

#### `.sigmoid(): Matrix`

Función sigmoide 1 / (1 + e^-x) en cada valor.

#### `.softmax(): Matrix`

Softmax por filas: cada fila pasa a ser una distribución de probabilidad.

#### `.sqrt(): Matrix`

Raíz cuadrada en cada valor.

#### `.sub(otra: Matrix|number): Matrix`

Resta elemento a elemento (también con -).

#### `.sum(eje?: number): any`

Suma de todo (número), de cada columna (eje 0, fila 1×n) o de cada fila (eje 1, columna n×1).

#### `.tanh(): Matrix`

Tangente hiperbólica en cada valor.

#### `.toList(): list`

Lista de filas [[...], [...]].

#### `.toString(): text`

Descripción con los primeros valores.

#### `.transpose(): Matrix`

Transpuesta: las filas pasan a ser columnas.

#### `Matrix.zeros(filas: number, columnas: number = 1): Matrix`

Matriz de ceros.

## Clase Module

## Clase ProcessHandle

Programa lanzado con Process.start.

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.running` | `bool` | ¿Sigue en marcha? *(solo lectura)* |

#### `.kill(): bool`

Termina el programa.

#### `.wait(segundos?: number): number?`

Espera a que termine (como mucho esos segundos) y devuelve su código, o null si sigue.

## Clase Range

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.end` | `number` | Fin. *(solo lectura)* |
| `.length` | `number` | Cantidad de números. *(solo lectura)* |
| `.start` | `number` | Inicio. *(solo lectura)* |
| `.step` | `number` | Paso. *(solo lectura)* |

#### `.contains(x: number): bool`

¿Está x en el rango?

#### `.toList(): list<number>`

Lista con todos los números.

## Clase Regex

Expresión regular (sintaxis de JavaScript) para buscar patrones en textos. Usa textos r"..." para no duplicar las barras: Regex(r"\d+").

Constructor: `Regex(patron: text, opciones: text = "")`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.pattern` | `text` | El patrón original. *(solo lectura)* |

#### `Regex.escape(texto: text): text`

Escapa los caracteres especiales para buscar el texto tal cual.

#### `.find(texto: text, desde: number = 0): map?`

Primera coincidencia: {text, start, end, groups} (posiciones en caracteres) o null.

#### `.findAll(texto: text): list<map>`

Todas las coincidencias.

#### `.matches(texto: text): bool`

¿El texto completo encaja con el patrón?

#### `.replace(texto: text, reemplazo: text|function, todas: bool = true): text`

Reemplaza las coincidencias. En el texto de reemplazo, $1, $2... son los grupos; también puede ser una función que recibe la coincidencia.

#### `.split(texto: text): list<text>`

Divide el texto por las coincidencias.

#### `.test(texto: text): bool`

¿Aparece el patrón en el texto?

#### `.toString(): text`

Descripción.

## Clase Subscription

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.active` | `bool` | ¿Sigue activa? *(solo lectura)* |

#### `.cancel(): void`

Deja de escuchar el evento.

## Clase Task

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.done` | `bool` | ¿Ha terminado (bien, con error o cancelada)? *(solo lectura)* |
| `.error` | `any` | Error (si falló). *(solo lectura)* |
| `.failed` | `bool` | ¿Terminó con error? *(solo lectura)* |
| `.result` | `any` | Resultado (si terminó bien). *(solo lectura)* |

#### `.cancel(): bool`

Cancela la tarea (cooperativo: se detiene en su siguiente await).

#### `.then(f: function): Task`

Llama a f(resultado) cuando termine bien.

## Clase Vec2

Vector 2D (x, y). Se suma, resta y multiplica con +, -, *, /.

Constructor: `Vec2(x: number = 0, y: number = 0)`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.down` | `Vec2` | (0, 1): abajo en pantalla. *(estática)* *(solo lectura)* |
| `.left` | `Vec2` | (-1, 0) *(estática)* *(solo lectura)* |
| `.one` | `Vec2` | (1, 1) *(estática)* *(solo lectura)* |
| `.right` | `Vec2` | (1, 0) *(estática)* *(solo lectura)* |
| `.up` | `Vec2` | (0, -1): arriba en pantalla. *(estática)* *(solo lectura)* |
| `.x` | `number` | Componente x. |
| `.y` | `number` | Componente y. |
| `.zero` | `Vec2` | (0, 0) *(estática)* *(solo lectura)* |

#### `.angle(): number`

Ángulo en radianes respecto al eje X.

#### `.copy(): Vec2`

Copia independiente.

#### `.cross(otro: Vec2): number`

Producto cruzado 2D (escalar).

#### `.distance(otro: Vec2): number`

Distancia hasta otro punto.

#### `.dot(otro: Vec2): number`

Producto escalar.

#### `Vec2.fromAngle(radianes: number, longitud: number = 1): Vec2`

Vector a partir de un ángulo.

#### `.length(): number`

Longitud (módulo) del vector.

#### `.lengthSquared(): number`

Longitud al cuadrado (más rápida para comparar).

#### `.lerp(otro: Vec2, t: number): Vec2`

Interpolación lineal hacia otro vector.

#### `.limit(maximo: number): Vec2`

Copia con la longitud limitada a 'maximo'.

#### `.moveTowards(destino: Vec2, maxDistancia: number): Vec2`

Avanza hacia destino como mucho maxDistancia.

#### `.normalized(): Vec2`

Vector de longitud 1 con la misma dirección.

#### `.rotate(radianes: number): Vec2`

Copia rotada.

#### `.set(x: number, y: number): Vec2`

Cambia los componentes (en el sitio).

#### `.toList(): list<number>`

Componentes en una lista.

## Clase Vec3

Vector 3D (x, y, z) para posiciones, direcciones y escalas en 3D.

Constructor: `Vec3(x: number = 0, y: number = 0, z: number = 0)`

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.back` | `Vec3` | (0, 0, 1) *(estática)* *(solo lectura)* |
| `.down` | `Vec3` | (0, -1, 0) *(estática)* *(solo lectura)* |
| `.forward` | `Vec3` | (0, 0, -1): hacia delante (OpenGL). *(estática)* *(solo lectura)* |
| `.left` | `Vec3` | (-1, 0, 0) *(estática)* *(solo lectura)* |
| `.one` | `Vec3` | (1, 1, 1) *(estática)* *(solo lectura)* |
| `.right` | `Vec3` | (1, 0, 0) *(estática)* *(solo lectura)* |
| `.up` | `Vec3` | (0, 1, 0) *(estática)* *(solo lectura)* |
| `.x` | `number` | Componente x. |
| `.y` | `number` | Componente y. |
| `.z` | `number` | Componente z. |
| `.zero` | `Vec3` | (0, 0, 0) *(estática)* *(solo lectura)* |

#### `.copy(): Vec3`

Copia independiente.

#### `.cross(otro: Vec3): Vec3`

Producto vectorial.

#### `.distance(otro: Vec3): number`

Distancia hasta otro punto.

#### `.dot(otro: Vec3): number`

Producto escalar.

#### `.length(): number`

Longitud (módulo) del vector.

#### `.lengthSquared(): number`

Longitud al cuadrado (más rápida para comparar).

#### `.lerp(otro: Vec3, t: number): Vec3`

Interpolación lineal hacia otro vector.

#### `.limit(maximo: number): Vec3`

Copia con la longitud limitada a 'maximo'.

#### `.moveTowards(destino: Vec3, maxDistancia: number): Vec3`

Avanza hacia destino como mucho maxDistancia.

#### `.normalized(): Vec3`

Vector de longitud 1 con la misma dirección.

#### `.set(x: number, y: number, z: number): Vec3`

Cambia los componentes (en el sitio).

#### `.toList(): list<number>`

Componentes en una lista.

## Clase bool

## Clase function

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.name` | `text` | Nombre de la función. *(solo lectura)* |

#### `.call(...args: any): any`

Llama a la función con los argumentos.

## Clase list

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.length` | `number` | Número de elementos. *(solo lectura)* |

#### `.add(...elementos: any): list`

Añade elementos al final.

#### `.all(f: function): bool`

¿Todos cumplen f?

#### `.any(f: function): bool`

¿Alguno cumple f?

#### `.average(): number`

Media de los números de la lista.

#### `.chunk(tamano: number): list`

Divide en grupos de 'tamano' elementos.

#### `.clear(): list`

Vacía la lista.

#### `.contains(valor: any): bool`

¿Contiene el valor?

#### `.copy(): list`

Copia superficial.

#### `.count(f?: function): number`

Cuántos cumplen f (o el total).

#### `.fill(valor: any): list`

Pone el mismo valor en todas las posiciones (en el sitio).

#### `.filter(f: function): list`

Nueva lista con los elementos donde f(x) es verdadero.

#### `.find(f: function): T`

Primer elemento que cumple f (o null).

#### `.findIndex(f: function): number`

Posición del primero que cumple f (o -1).

#### `.first(): T`

Primer elemento (o null).

#### `.flat(profundidad: number = 1): list`

Aplana listas anidadas.

#### `.flatMap(f: function): list`

map + flat: f puede devolver una lista.

#### `.forEach(f: function): void`

Llama a f(x) para cada elemento.

#### `.groupBy(f: function): map`

Agrupa en un mapa según la clave que devuelve f.

#### `.indexOf(valor: any): number`

Posición del valor (o -1).

#### `.insert(indice: number, valor: any): list`

Inserta en una posición.

#### `.isEmpty(): bool`

¿Está vacía?

#### `.join(separador: text = ", "): text`

Une los elementos en un texto.

#### `.last(): T`

Último elemento (o null).

#### `.map(f: function): list`

Nueva lista con f(x) para cada elemento.

#### `.max(): T`

Mayor elemento.

#### `.maxBy(f: function): any`

Elemento con el mayor valor de f(elemento).

#### `.min(): T`

Menor elemento.

#### `.minBy(f: function): any`

Elemento con el menor valor de f(elemento).

#### `.partition(f: function): list`

[los que cumplen f, los que no].

#### `.pop(): T`

Quita y devuelve el último elemento (o null).

#### `.push(...elementos: any): list`

Añade elementos al final.

#### `.random(): T`

Elemento aleatorio (o null).

#### `.reduce(f: function, inicial?: any): any`

Acumula: f(acumulado, x).

#### `.remove(valor: any): bool`

Quita la primera aparición de un valor.

#### `.removeAt(indice: number): T`

Quita el elemento de una posición y lo devuelve.

#### `.reverse(): list`

Invierte el orden (en el sitio).

#### `.reversed(): list`

Copia invertida.

#### `.shift(): T`

Quita y devuelve el primer elemento (o null).

#### `.shuffle(): list`

Mezcla al azar (en el sitio).

#### `.skip(n: number): list`

Todos menos los n primeros.

#### `.slice(inicio?: number, fin?: number): list`

Sublista entre inicio y fin (excluido).

#### `.sort(comparar?: function): list`

Ordena en el sitio (opcional: comparar(a, b) < 0).

#### `.sortBy(clave: function): list`

Ordena en el sitio por clave(x).

#### `.sum(): number`

Suma de los elementos numéricos.

#### `.swap(i: number, j: number): list`

Intercambia dos elementos (en el sitio).

#### `.take(n: number): list`

Los n primeros elementos.

#### `.unique(): list`

Copia sin elementos repetidos.

#### `.zip(otra: list): list`

Pares [a, b] con los elementos de ambas listas.

## Clase map

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.length` | `number` | Número de entradas. *(solo lectura)* |

#### `.clear(): map`

Vacía el mapa.

#### `.contains(clave: any): bool`

¿Existe la clave?

#### `.copy(): map`

Copia superficial.

#### `.entries(): list`

Lista de pares [clave, valor].

#### `.filter(f: function): map`

Nuevo mapa con las entradas para las que f(clave, valor) es verdadero.

#### `.forEach(f: function): void`

Llama a f(clave, valor).

#### `.get(clave: any, porDefecto?: any): any`

Valor de la clave (o porDefecto).

#### `.has(clave: any): bool`

¿Existe la clave?

#### `.isEmpty(): bool`

¿Está vacío?

#### `.keyOf(valor: any): any`

Primera clave que tiene ese valor (o null).

#### `.keys(): list`

Lista de claves.

#### `.mapValues(f: function): map`

Nuevo mapa con f(valor, clave) como valores.

#### `.merge(otro: map): map`

Copia las entradas de otro mapa.

#### `.remove(clave: any): bool`

Elimina una clave.

#### `.set(clave: any, valor: any): map`

Asigna un valor.

#### `.values(): list`

Lista de valores.

## Clase number

#### `.isInteger(): bool`

¿No tiene decimales?

#### `.toBase(base: number, digitos: number = 0): text`

Entero en cualquier base de 2 a 36.

#### `.toBinary(digitos: number = 0): text`

Entero en binario: (5).toBinary(8) es "00000101".

#### `.toFixed(decimales: number = 0): text`

Texto con N decimales fijos.

#### `.toHex(digitos: number = 0): text`

Entero en hexadecimal: (255).toHex() es "ff"; con dígitos rellena con ceros.

#### `.toText(): text`

Convierte en texto.

## Clase set

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.length` | `number` | Número de elementos. *(solo lectura)* |

#### `.add(...valores: any): set`

Añade valores.

#### `.clear(): set`

Vacía el conjunto.

#### `.contains(valor: any): bool`

¿Contiene el valor?

#### `.copy(): set`

Copia.

#### `.forEach(f: function): void`

Llama a f(valor).

#### `.has(valor: any): bool`

¿Contiene el valor?

#### `.isEmpty(): bool`

¿Está vacío?

#### `.remove(valor: any): bool`

Quita un valor.

#### `.toList(): list`

Convierte en lista.

## Clase text

| Propiedad | Tipo | Descripción |
|---|---|---|
| `.length` | `number` | Número de caracteres. *(solo lectura)* |

#### `.capitalize(): text`

Primera letra en mayúscula.

#### `.center(ancho: number, relleno: text = " "): text`

Centra el texto en 'ancho' caracteres.

#### `.chars(): list<text>`

Lista de caracteres.

#### `.contains(sub: text): bool`

¿Contiene el subtexto?

#### `.count(sub: text): number`

Cuántas veces aparece sub.

#### `.endsWith(sufijo: text): bool`

¿Termina en...?

#### `.format(...valores: any): text`

Sustituye {} (o {0}, {1}...) por los valores: "{} tiene {} puntos".format(nombre, 10)

#### `.indexOf(sub: text): number`

Posición de la primera aparición (o -1).

#### `.isEmpty(): bool`

¿Está vacío?

#### `.isNumeric(): bool`

¿El texto es un número válido?

#### `.lastIndexOf(sub: text): number`

Posición de la última aparición (o -1).

#### `.lines(): list<text>`

Lista de líneas.

#### `.lower(): text`

Texto en minúsculas.

#### `.matches(patron: text, opciones: text = ""): bool`

¿Aparece la expresión regular en el texto?

#### `.padEnd(longitud: number, relleno: text = " "): text`

Rellena por la derecha hasta la longitud.

#### `.padStart(longitud: number, relleno: text = " "): text`

Rellena por la izquierda hasta la longitud.

#### `.repeat(veces: number): text`

Repite el texto.

#### `.replace(buscar: text, poner: text): text`

Reemplaza todas las apariciones.

#### `.replaceRegex(patron: text, reemplazo: text): text`

Reemplaza todas las coincidencias de una expresión regular ($1, $2... son los grupos).

#### `.reverse(): text`

Texto al revés.

#### `.slice(inicio: number, fin?: number): text`

Igual que substring; admite índices negativos.

#### `.split(separador?: text): list<text>`

Divide el texto (por espacios si no se indica separador).

#### `.startsWith(prefijo: text): bool`

¿Empieza por...?

#### `.substring(inicio: number, fin?: number): text`

Parte del texto entre inicio y fin (excluido).

#### `.toNumber(porDefecto?: any): number?`

Convierte a número.

#### `.trim(): text`

Quita espacios al principio y al final.

#### `.trimEnd(): text`

Quita espacios al final.

#### `.trimStart(): text`

Quita espacios al principio.

#### `.upper(): text`

Texto en mayúsculas.

#### `.words(): list<text>`

Palabras separadas por espacios.

## Módulo Base64

Codificar datos binarios como texto (Base64) y al revés.

#### `Base64.decode(texto: text): text`

Decodifica Base64 a texto.

#### `Base64.decodeBytes(texto: text): Bytes`

Decodifica Base64 a Bytes.

#### `Base64.encode(datos: text|Bytes): text`

Codifica en Base64.

## Módulo Console

Control de la consola para juegos de texto: colores, cursor y teclas sin pulsar Intro.

| Propiedad | Tipo | Descripción |
|---|---|---|
| `Console.height` | `number` | Filas visibles de la consola. *(solo lectura)* |
| `Console.interactive` | `bool` | true si el programa corre en una consola con teclado (false en pruebas o con la salida redirigida). *(solo lectura)* |
| `Console.width` | `number` | Columnas visibles de la consola. *(solo lectura)* |

#### `Console.clear(): void`

Borra la pantalla de la consola.

#### `Console.keyAvailable(): bool`

¿Hay una tecla pulsada esperando? (no bloquea)

#### `Console.moveTo(columna: number, fila: number): void`

Mueve el cursor (0, 0 es la esquina superior izquierda).

#### `Console.pollKey(): text?`

Tecla pulsada o null si no hay ninguna (no bloquea). Ideal para bucles de juego.

#### `Console.print(texto?: any, color?: Color, fondo?: Color): void`

Escribe una línea, opcionalmente en color.

#### `Console.readKey(): text`

Espera a que se pulse una tecla y devuelve su nombre: "a", "up", "enter", "space"...

```hz
var tecla = Console.readKey()
```

#### `Console.showCursor(visible: bool = true): void`

Muestra u oculta el cursor.

#### `Console.title(texto: text): void`

Cambia el título de la ventana de la consola.

#### `Console.write(texto: any, color?: Color, fondo?: Color): void`

Escribe sin salto de línea, opcionalmente en color.

```hz
Console.write("¡Peligro!", "red")
```

## Módulo File

Leer y escribir archivos. Las rutas relativas parten de la carpeta del programa.

#### `File.append(ruta: text, contenido: any): void`

Añade texto al final de un archivo.

#### `File.copy(origen: text, destino: text): bool`

Copia un archivo (o una carpeta entera) reemplazando el destino.

#### `File.delete(ruta: text): bool`

Borra un archivo (o una carpeta vacía).

#### `File.deleteDir(carpeta: text): number`

Borra una carpeta con todo su contenido y devuelve cuántos elementos borró. ¡Cuidado: no se puede deshacer!

#### `File.exists(ruta: text): bool`

¿Existe el archivo o la carpeta?

#### `File.isDir(ruta: text): bool`

¿Es una carpeta?

#### `File.isFile(ruta: text): bool`

¿Existe y es un archivo (no una carpeta)?

#### `File.list(carpeta: text = "."): list<text>`

Nombres de los archivos y carpetas que contiene una carpeta.

#### `File.listAll(carpeta: text = ".", extension?: text): list<text>`

Todos los archivos de la carpeta y sus subcarpetas (rutas relativas), opcionalmente solo con una extensión.

#### `File.makeDir(ruta: text): bool`

Crea una carpeta (y las intermedias).

#### `File.modified(ruta: text): number`

Fecha de la última modificación (segundos desde 1970, como Time.now()).

#### `File.move(origen: text, destino: text): bool`

Mueve o renombra un archivo o carpeta.

#### `File.read(ruta: text): text`

Contenido completo de un archivo de texto.

```hz
var datos = File.read("nivel1.txt")
```

#### `File.readBytes(ruta: text): Bytes`

Lee un archivo binario completo.

#### `File.readLines(ruta: text): list<text>`

Líneas de un archivo de texto.

#### `File.size(ruta: text): number`

Tamaño del archivo en bytes.

#### `File.tempFile(extension: text = ".tmp"): text`

Ruta nueva en la carpeta temporal (no crea el archivo).

#### `File.write(ruta: text, contenido: any): void`

Escribe (o reemplaza) un archivo. Crea las carpetas que falten.

#### `File.writeBytes(ruta: text, datos: Bytes|text): void`

Escribe (o reemplaza) un archivo binario.

## Módulo Hash

Huellas de datos: comprobar integridad, comparar archivos, identificadores únicos.

#### `Hash.crc32(datos: text|Bytes): number`

Suma de comprobación CRC-32 (la de ZIP y PNG).

#### `Hash.fnv1a(datos: text|Bytes): number`

Hash FNV-1a de 32 bits (rápido; no criptográfico).

#### `Hash.sha256(datos: text|Bytes): text`

Huella SHA-256 en hexadecimal (64 caracteres).

```hz
print(Hash.sha256("hola"))
```

#### `Hash.sha256Bytes(datos: text|Bytes): Bytes`

Huella SHA-256 como 32 Bytes.

#### `Hash.uuid(): text`

Identificador único aleatorio (UUID versión 4).

## Módulo Http

Peticiones a servidores web (HTTP y HTTPS): APIs, descargas y envío de datos.

#### `Http.download(url: text, ruta: text, opciones?: map): number`

Descarga a un archivo y devuelve los bytes descargados.

#### `Http.encode(texto: text): text`

Codifica un texto para ponerlo en una URL (espacios, acentos...).

#### `Http.get(url: text, opciones?: map): map`

Pide una página o recurso. Devuelve {status, ok, body, headers}. Opciones: headers (mapa) y timeout (segundos, 30 por defecto).

```hz
var r = Http.get("https://example.com")
print(r.status, r.body.length)
```

#### `Http.getJson(url: text, opciones?: map): any`

Pide JSON y lo devuelve ya convertido (mapas, listas...).

#### `Http.post(url: text, cuerpo?: any, opciones?: map): map`

Envía datos. Un mapa o lista se envía como JSON. Devuelve {status, ok, body, headers}. Opciones: headers (mapa) y timeout (segundos, 30 por defecto).

#### `Http.postJson(url: text, datos: any, opciones?: map): any`

Envía datos como JSON y devuelve la respuesta convertida.

#### `Http.query(parametros: map): text`

Mapa a parámetros de URL: {q: "hola mundo", n: 2} -> "q=hola%20mundo&n=2".

#### `Http.request(metodo: text, url: text, cuerpo?: any, opciones?: map): map`

Petición con cualquier método (PUT, DELETE, PATCH...). Devuelve {status, ok, body, headers}. Opciones: headers (mapa) y timeout (segundos, 30 por defecto).

#### `Http.serve(puerto: number, manejador?: function, opciones?: map): number`

Servidor web: llama a manejador(peticion) por cada petición y envía lo que devuelva. La petición tiene {method, path, url, query, headers, body, json}. La respuesta puede ser un texto (HTML), un mapa o lista (JSON) o {status, body, json, file, type, headers}. Opciones: host ("127.0.0.1"; "0.0.0.0" para la red local), static (carpeta de archivos), cors, quiet y maxRequests. Devuelve cuántas peticiones atendió.

```hz
Http.serve(8080, req => "<h1>Hola desde ${req.path}</h1>")
```

## Módulo JSON

Convertir datos a texto JSON y viceversa.

#### `JSON.parse(texto: text): any`

Convierte texto JSON en mapas, listas, números...

#### `JSON.stringify(valor: any, sangria: number = 0): text`

Convierte un valor en texto JSON.

```hz
JSON.stringify({vida: 3}, 2)
```

## Módulo Path

Trabajar con rutas de archivos sin tocar el disco: unir, separar, extensiones...

| Propiedad | Tipo | Descripción |
|---|---|---|
| `Path.home` | `text` | Carpeta del usuario. *(solo lectura)* |
| `Path.separator` | `text` | Separador de carpetas del sistema (\\ en Windows, / en el resto). *(solo lectura)* |
| `Path.temp` | `text` | Carpeta de archivos temporales. *(solo lectura)* |

#### `Path.absolute(ruta: text): text`

Ruta completa (las relativas parten de la carpeta del programa).

#### `Path.dir(ruta: text): text`

Carpeta que contiene la ruta.

#### `Path.ext(ruta: text): text`

Extensión con el punto (".json") o "".

#### `Path.isAbsolute(ruta: text): bool`

¿Es una ruta completa?

#### `Path.join(...partes: text): text`

Une partes de una ruta: Path.join("datos", "nivel1.json").

#### `Path.name(ruta: text): text`

Nombre del archivo con extensión.

#### `Path.normalize(ruta: text): text`

Quita . y .. y unifica las barras.

#### `Path.relative(ruta: text, desde: text = "."): text`

Ruta relativa a otra carpeta.

#### `Path.stem(ruta: text): text`

Nombre del archivo sin extensión.

#### `Path.withExt(ruta: text, extension: text): text`

La misma ruta con otra extensión.

## Módulo Process

Ejecutar otros programas y órdenes del sistema, y terminar el programa.

| Propiedad | Tipo | Descripción |
|---|---|---|
| `Process.pid` | `number` | Identificador del proceso actual. *(solo lectura)* |

#### `Process.exit(codigo: number = 0): void`

Termina el programa con ese código de salida.

#### `Process.open(destino: text): bool`

Abre un archivo, carpeta o dirección web con la aplicación predeterminada.

#### `Process.run(programa: text, argumentos?: list<text>, opciones?: map): map`

Ejecuta un programa y espera a que termine. Devuelve {code, output, error, ok, timedOut}. Opciones: cwd (carpeta), input (texto de entrada), timeout (segundos), env (mapa de variables).

```hz
var r = Process.run("git", ["--version"])
print(r.output)
```

#### `Process.shell(orden: text, opciones?: map): map`

Ejecuta una orden con el intérprete del sistema (cmd en Windows, sh en Linux y macOS). Devuelve lo mismo que run.

```hz
print(Process.shell("echo hola").output)
```

#### `Process.start(programa: text, argumentos?: list<text>): ProcessHandle`

Lanza un programa en segundo plano (comparte la consola) y devuelve un control para esperarlo o terminarlo.

## Módulo System

Información del sistema y del programa.

| Propiedad | Tipo | Descripción |
|---|---|---|
| `System.arch` | `text` | Arquitectura del procesador: "x64", "arm64"... *(solo lectura)* |
| `System.args` | `list<text>` | Argumentos pasados al programa: hyper juego.hz uno dos *(solo lectura)* |
| `System.cpuCount` | `number` | Número de núcleos del procesador. *(solo lectura)* |
| `System.cwd` | `text` | Carpeta de trabajo actual. *(solo lectura)* |
| `System.exe` | `text` | Ruta del ejecutable de HyperEZ. *(solo lectura)* |
| `System.hostname` | `text` | Nombre del equipo. *(solo lectura)* |
| `System.platform` | `text` | "windows", "linux" o "macos". *(solo lectura)* |
| `System.scriptDir` | `text` | Carpeta del programa que se está ejecutando. *(solo lectura)* |
| `System.user` | `text` | Nombre del usuario. *(solo lectura)* |
| `System.version` | `text` | Versión de HyperEZ. *(solo lectura)* |

#### `System.date(): map`

Fecha y hora actuales: {year, month, day, hour, minute, second, weekday, text}.

#### `System.env(nombre: text): text?`

Valor de una variable de entorno (o null).

#### `System.setEnv(nombre: text, valor?: any): void`

Cambia (o con null quita) una variable de entorno para este programa y los que lance.

#### `System.sleep(segundos: number): void`

Detiene el programa unos segundos (bloquea; dentro de async usa await wait()).

## Módulo Time

Fechas, horas y medir cuánto tarda algo.

#### `Time.clock(): number`

Segundos desde que empezó el programa (reloj que nunca va hacia atrás).

#### `Time.date(momento?: number): map`

Partes de una fecha: {year, month, day, hour, minute, second, millisecond, weekday, dayName, yearDay}.

#### `Time.format(momento?: number, formato: text = "%Y-%m-%d %H:%M:%S"): text`

Fecha como texto. Formato de strftime: %Y año, %m mes, %d día, %H hora, %M minutos, %S segundos.

```hz
print(Time.format(Time.now(), "%d/%m/%Y"))
```

#### `Time.make(anio: number, mes: number = 1, dia: number = 1, hora: number = 0, minuto: number = 0, segundo: number = 0): number`

Momento (segundos desde 1970) de una fecha y hora locales.

#### `Time.measure(funcion: function): number`

Ejecuta la función y devuelve cuántos segundos tardó.

#### `Time.now(): number`

Momento actual en segundos desde el 1 de enero de 1970 (con decimales).

## Bibliotecas

Se pueden importar por nombre: `import HyperCore`.

| Biblioteca | Contiene | Descripción |
|---|---|---|
| `HyperCore` | JSON, File, System, Console | Núcleo: tipos, texto, colecciones, JSON, archivos, sistema, consola, tareas y eventos. |
| `HyperMath` | Vec2, Vec3, Color | Vectores, colores y funciones matemáticas. |
