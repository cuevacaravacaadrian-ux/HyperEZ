# Gráficos y sonido

## Imágenes: `Image`

`Image` es una imagen en memoria (píxeles RGBA) con la que se generan gráficas, mapas, texturas para juegos, miniaturas o banners. Todo se dibuja con transparencia y bordes suaves, y se guarda como PNG, JPG, BMP o TGA.

```hz
var im = Image(300, 150, "#1e2030")                 // ancho, alto y fondo
im.rect(20, 20, 120, 60, "#2f81f7")                 // rectángulo relleno
im.rect(160, 20, 120, 60, "#ffffff", false)         // solo el borde
im.circle(80, 115, 25, Color(255, 200, 0, 180))     // color semitransparente
im.line(160, 100, 280, 130, "#7ee787", 3)           // línea de 3 píxeles
var ancho = im.text(170, 105, "HyperEZ", "white", 20)
print(im, ancho > 0, im.get(30, 30))
im.save(Path.join(Path.temp, "dibujo.png"))
```

| Método | Qué hace |
|---|---|
| `Image(ancho, alto, fondo?)`, `Image.load(ruta)` | crear o leer (PNG, JPG, BMP, GIF, TGA) |
| `get(x, y)`, `set(x, y, color)` | leer y cambiar un píxel |
| `fill`, `rect`, `line`, `circle` | dibujar |
| `text(x, y, texto, color, tamaño, fuente?)` | texto con DejaVu Sans (incluida) o un archivo `.ttf`; devuelve el ancho |
| `Image.textWidth(texto, tamaño)` | cuánto ocupará un texto (para centrar) |
| `paste(otra, x, y)` | pegar otra imagen (respeta su transparencia) |
| `crop`, `resize`, `flipH`, `flipV`, `grayscale`, `invert`, `copy` | transformar |
| `pixels()`, `Image.fromBytes(ancho, alto, bytes)` | píxeles como `Bytes` RGBA |
| `save(ruta, calidad?)` | guardar según la extensión |

Los colores se pueden escribir como `Color(r, g, b, a)`, `"#rrggbb"`, `"#rrggbbaa"` o por nombre (`"red"`, `"white"`, `"gold"`...: la paleta de HyperEZ).

Ejemplos completos: [una gráfica de barras](../../examples/graficos/grafica.hz) y [el conjunto de Mandelbrot](../../examples/graficos/fractal.hz). Las imágenes de presentación del repositorio también están hechas así: [tools/banner.hz](../../tools/banner.hz).

## Sonido: `import audio`

La biblioteca `audio` crea sonidos y música muestra a muestra y los guarda como WAV (el formato que abre cualquier reproductor y cualquier motor de juegos).

```hz
import audio
var s = audio.Sound()
s.melody("C4 E4 G4 C5", 0.2)               // notas inglesas: C do, D re, E mi, F fa, G sol, A la, B si
s.silence(0.1)
s.tone(440, 0.3, 0.3, "square")            // frecuencia, segundos, volumen, forma de onda
s.note("F#4", 0.2).note("-", 0.1)          // "-" es un silencio
print(round(s.duration, 2), "segundos", audio.frequency("A4"), "Hz")
s.save(Path.join(Path.temp, "efecto.wav"))
```

- Formas de onda: `"sine"` (suave), `"square"` (de consola antigua), `"triangle"`, `"saw"` y `"noise"` (explosiones, viento).
- `mix(otro, desde)` suma otro sonido encima: acordes, bajo y melodía, efectos.
- `toBytes()` da el WAV como `Bytes` (por ejemplo para enviarlo con `Http`).

El ejemplo [examples/audio/melodia.hz](../../examples/audio/melodia.hz) compone una melodía con bajo.
