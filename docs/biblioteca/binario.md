# Datos binarios: `Bytes`, `Hash` y `Base64`

## `Bytes`

Un bloque de bytes (números de 0 a 255). Sirve para leer y escribir formatos de archivo (imágenes, sonidos, partidas guardadas), protocolos de red, emuladores o imágenes de disco.

```hz
var b = Bytes([72, 111, 108, 97])     // también Bytes(tamaño), Bytes("texto") o Bytes(otro)
print(b, b.length, b[0], b.toText())
b[0] = 104
b.push(33)                            // añadir al final (números, listas, textos u otros Bytes)
print(b.toText(), b.toHex(" "), b.toList())
```

### Números de varios bytes

Los métodos `read*` y `write*` leen y escriben enteros y decimales en una posición. Por defecto en *little endian* (el orden de x86 y ARM); con `false` al final, en *big endian* (el de las redes).

| Método | Tamaño |
|---|---|
| `readU8` `readI8` / `writeU8` | 1 byte (sin y con signo) |
| `readU16` `readI16` / `writeU16` / `pushU16` | 2 bytes |
| `readU32` `readI32` / `writeU32` / `pushU32` | 4 bytes |
| `readI64` / `writeI64` / `pushI64` | 8 bytes |
| `readF32` / `writeF32` / `pushF32` | decimal de 4 bytes (float) |
| `readF64` / `writeF64` / `pushF64` | decimal de 8 bytes (double) |

```hz
// Cabecera de un formato inventado: firma "HZS1", versión (u16) y número de niveles (u32)
var cab = Bytes("HZS1").pushU16(2).pushU32(15)
print(cab.toHex(" "))
print(cab.slice(0, 4).toText(), cab.readU16(4), cab.readU32(6))
print(cab.readU16(4, false)) // el mismo dato leído en big endian
```

Más métodos: `slice(desde, hasta)`, `concat(otro)`, `indexOf(buscado)`, `fill(valor)`, `resize(n)`, `copy()`, `equals(otro)` (y `==`). `File.readBytes(ruta)` y `File.writeBytes(ruta, bytes)` leen y escriben archivos binarios.

## `Hash`

Huellas para comprobar que unos datos no han cambiado, comparar archivos o generar identificadores.

```hz
print(Hash.sha256("hola"))     // SHA-256 en hexadecimal
print(Hash.crc32("hola"))      // CRC-32 (el de ZIP y PNG)
print(Hash.fnv1a("hola"))      // rápido, no criptográfico
print(Hash.uuid().length)      // identificador único aleatorio
```

## `Base64`

Para meter datos binarios dentro de texto (JSON, correos, URLs):

```hz
var codificado = Base64.encode("¡Hola!")
print(codificado, Base64.decode(codificado))
print(Bytes([0, 255]).toBase64(), Base64.decodeBytes("AP8=").toList())
```

## Compresión

`compress(nivel)` comprime con zlib (el algoritmo de ZIP y PNG) y `decompress()` lo deshace:

```hz
var datos = Bytes("HyperEZ " * 500)
var z = datos.compress(9)
print(datos.length, "->", z.length, "bytes")
print(z.decompress().toText() == datos.toText())
```

## Números en otras bases

```hz
print((255).toHex(), (255).toHex(4), (5).toBinary(8), (35).toBase(36))
print(parseInt("ff", 16), parseInt("0x1F", 16), parseInt("1010", 2))
print(0xFF & 0x0F, 1 << 8, ~0)
```
