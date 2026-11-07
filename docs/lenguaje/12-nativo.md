# HyperEZ nativo: código máquina para sistemas

`hyper build` compila HyperEZ a **código máquina x86-64**, sin máquina virtual ni sistema operativo debajo. Es lo que se necesita para escribir un **kernel**, un cargador de arranque o un programa UEFI. El sistema operativo [HyperEZX](https://github.com/cuevacaravacaadrian/HyperEZX) está hecho así.

```text
hyper build kernel.hez -o BOOTX64.EFI              aplicación UEFI (arranca desde un USB o una ISO)
hyper build kernel.hez -o kernel.bin --formato bin  binario plano, independiente de la posición
hyper build kernel.hez --mapa                       además, kernel.efi.map con la dirección de cada función
```

## El subconjunto nativo

En modo nativo **todos los valores son enteros de 64 bits** (las direcciones de memoria también). No hay recolector de basura, ni listas, mapas o textos dinámicos: la memoria la gestiona tu programa.

| Se puede | Cómo |
|---|---|
| funciones | `function suma(a: int, b: int): int { return a + b }` (parámetros con valor por defecto constante) |
| variables y constantes | locales y globales; `const ANCHO = 1024` se calcula al compilar |
| control | `if/else`, `while`, `do { } while`, `for i in a..b`, `for i in a..=b step -2`, `repeat n`, `break`, `continue`, `return`, ternario |
| operadores | `+ - * / %` (con signo), `& | ^ ~ << >>`, comparaciones, `and`, `or`, `not`, `+=`, `++`... |
| `struct` | campos de tamaño fijo (`u8`, `i8`, `u16`, `i16`, `u32`, `i32`, `int`/`u64`/`i64`, o un puntero a otro struct) y métodos |
| `enum` | constantes enteras: `Color.Rojo` |
| textos literales | `"hola"` es la dirección de un texto UTF-8 terminado en 0; `u16("hola")`, de uno UTF-16 (el que usa UEFI) |
| datos globales | `var tabla = [1, 2, 3]`, `var memoria = buffer(4096)` (a cero), `var fuente = embed("fuente.bin")` y `embedSize("fuente.bin")` |
| varios archivos | `import "./video.hez"`: todos forman un solo programa |

### Structs

Una variable de tipo struct guarda la **dirección** del struct. `Ventana(dir)` ve una dirección como un struct y `sizeof(Ventana)` da su tamaño:

```hz
struct Punto {
    var x: i32
    var y: i32
    function distancia(): int { return abs(x) + abs(y) }   // un método: x e y son los campos de este punto
}

var memoria = buffer(64)

function main(imagen: int, sistema: int): int {
    var p: Punto = Punto(memoria)        // el struct vive en `memoria`
    p.x = -3
    p.y = 4
    return p.distancia()                 // 7
}

function abs(a: int): int { return a < 0 ? -a : a }
```

### El procesador y la memoria

| Función | Qué hace |
|---|---|
| `peek8/16/32/64(dir)`, `peeks8/16/32(dir)` | lee de la memoria (sin signo / con signo) |
| `poke8/16/32/64(dir, valor)` | escribe en la memoria |
| `copy(destino, origen, n)`, `copy32(...)` | copia n bytes (o n enteros de 32 bits) |
| `fill8(destino, valor, n)`, `fill32(...)` | rellena (muy rápido: `rep stos`) |
| `inb/inw/inl(puerto)`, `outb/outw/outl(puerto, valor)` | puertos de E/S (controladores de dispositivos) |
| `cli()`, `sti()`, `hlt()`, `pause()` | interrupciones y espera |
| `rdtsc()`, `cpuid(hoja, subhoja, destino)` | contador de ciclos e identificación del procesador |
| `udiv`, `umod`, `ult` | división, resto y comparación sin signo |
| `callptr(funcion, args...)` | llama a una dirección (por ejemplo, a los servicios de UEFI) |

Las llamadas siguen la convención de Microsoft x64 (`rcx`, `rdx`, `r8`, `r9` y la pila), la misma de UEFI, así que `callptr` llama directamente al firmware.

## Un programa UEFI completo

```hz
// Escribe en la pantalla con los servicios de UEFI y pinta un degradado con GOP
function main(imagen: int, sistema: int): int {
    var conOut = peek64(sistema + 64)
    callptr(peek64(conOut + 8), conOut, u16("Hola desde HyperEZ nativo\r\n"))
    while true { hlt() }
    return 0
}
```

```text
hyper build hola.hez -o disco/EFI/BOOT/BOOTX64.EFI
qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=fat:rw:disco
```

`main(imagen, sistema)` es el punto de entrada: el firmware la llama con su `ImageHandle` y su `SystemTable`.

## Lo que no hay en modo nativo

Listas, mapas y textos dinámicos, clases con herencia, excepciones (`try`), `foreach`, números decimales, cierres (lambdas) y la biblioteca de HyperEZ: el compilador explica cada caso con un mensaje en español. Para ellos, el HyperEZ de siempre (`hyper run`) sigue siendo el mismo.
