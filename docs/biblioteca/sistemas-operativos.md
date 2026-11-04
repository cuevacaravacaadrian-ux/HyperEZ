# Sistemas operativos y bajo nivel

HyperEZ todavía no compila a código máquina nativo, así que **no puede ser el lenguaje en el que está escrito un kernel** (eso exige ejecutarse sin sistema operativo debajo). Lo que sí puede hacer desde ya es todo el trabajo de **herramientas** que rodea a un sistema operativo:

- generar código máquina y sectores de arranque (`import x86`);
- crear y analizar imágenes de disco, sistemas de archivos y formatos binarios (`Bytes`);
- automatizar la compilación, el empaquetado y las pruebas en emuladores (`Process`, `File`);
- scripts de instalación y configuración para una distribución (`Process.shell`, `Path`, `File`).

La compilación a código nativo para poder escribir kernels está en la [hoja de ruta](../hoja-de-ruta.md).

## Un sector de arranque

La BIOS de un PC lee el primer sector del disco (512 bytes) en la dirección `0x7C00` y lo ejecuta si acaba en `55 AA`. Con `x86.Assembler` se escribe en HyperEZ:

```hz
import x86

var a = x86.Assembler(0x7C00)
a.xorReg16("ax", "ax").movSeg("ds", "ax")   // ds = 0
a.printString("mensaje")                     // imprime con la BIOS (int 0x10)
a.label("fin").cli().hlt().jmp("fin")        // para el procesador
a.label("mensaje").db("Hola desde HyperEZ!", 0)

var sector = x86.bootSector(a)
print(sector.length, sector.slice(0, 8).toHex(" "), sector.slice(510).toHex(" "))
```

Guárdalo con `File.writeBytes("arranque.img", sector)` y arráncalo en un emulador:

```text
qemu-system-x86_64 -drive format=raw,file=arranque.img
```

Instrucciones disponibles: `movReg8`, `movReg16`, `movRegReg16`, `movSeg`, `xorReg16`, `orReg8`, `cmpAl`, `addReg16`, `subReg16`, `inc16`, `dec16`, `push16`, `pop16`, `jmp`, `jmpNear`, `jz`/`je`, `jnz`/`jne`, `jc`, `jnc`, `call`, `ret`, `interrupt`, `lodsb`, `stosb`, `cli`, `sti`, `hlt`, `nop`, y datos con `db` y `dw`. Las etiquetas se pueden usar antes de definirlas: se resuelven en `build()`.

Lee el código de la biblioteca en [src/lib/x86.hz](../../src/lib/x86.hz): cada instrucción son una o dos líneas, y es fácil añadir las que falten.

## Formatos binarios

```hz
// Cabecera de un archivo BMP de 2×2 píxeles (formato real de Windows)
var ancho = 2
var alto = 2
var bmp = Bytes("BM").pushU32(54 + ancho * alto * 4).pushU32(0).pushU32(54)
bmp.pushU32(40).pushU32(ancho).pushU32(alto).pushU16(1).pushU16(32).pushU32(0)
bmp.pushU32(ancho * alto * 4).pushU32(2835).pushU32(2835).pushU32(0).pushU32(0)
foreach color in [[255, 0, 0], [0, 255, 0], [0, 0, 255], [255, 255, 255]] {
    bmp.push(color[2], color[1], color[0], 255) // azul, verde, rojo, alfa
}
print(bmp.length, bmp.slice(0, 2).toText(), bmp.readU32(18), bmp.readU32(22))
```

## Bits

```hz
var flags = 0
flags = flags | (1 << 3)          // activar el bit 3
print((flags & (1 << 3)) != 0)    // ¿está activo?
flags = flags & ~(1 << 3)         // desactivarlo
print(flags, (0b1011).toBinary(8), (0xCAFE).toHex())
```
