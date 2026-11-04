# La máquina virtual

## Valores

Un `Value` ocupa 16 bytes: una etiqueta (`null`, `bool`, número, objeto, "sin valor") y el dato. Los números son `double`. Todo lo demás es un **objeto** en el montón (`Obj`): textos, listas, mapas, funciones, clausuras, clases, instancias, vectores, tareas, fibras, módulos, eventos y objetos nativos (`Bytes`, `Matrix`, `Regex`...).

Los **textos están internados**: cada texto distinto existe una sola vez, así que comparar textos es comparar punteros y las claves de los mapas se buscan muy rápido.

## Intérprete

Un bucle `switch` sobre el bytecode (`src/lang/vm/interp.cpp`) con una pila de valores por fibra y un marco (`CallFrame`) por llamada. Detalles que lo hacen rápido:

- **Caminos rápidos** para números en la aritmética y las comparaciones (sin llamadas).
- **Llamadas directas**: si se llama a una función del script con exactamente sus parámetros, se apila el marco sin pasar por la lógica general de llamadas.
- **Cachés en línea** para campos (`GET_PROP`/`SET_PROP`) y métodos (`INVOKE`).
- **Asignador de objetos pequeños**: listas libres por tamaño (múltiplos de 16 bytes hasta 256) que reutilizan la memoria; crear un `Vec2` cuesta unos nanosegundos en vez de una llamada a `malloc`.
- **Sobrecarga de operadores** solo en el camino lento: si los dos operandos son números, nunca se busca un método.

## Recolector de basura

Marcado y barrido (*mark & sweep*) no incremental. Las raíces son la pila y los marcos de todas las fibras, los globales de cada módulo, los integrados, las tareas pendientes, los manejadores de eventos y los valores que el código nativo retiene (`TempRoot`). Se dispara cuando la memoria asignada supera el doble de la que quedó viva en la recolección anterior (mínimo 8 MB), y se comprueba en los puntos seguros: llamadas y saltos hacia atrás.

## Fibras, tareas y `await`

Cada función `async` se ejecuta en su propia **fibra** (pila y marcos propios). `await tarea` suspende la fibra actual hasta que la tarea termina; el **planificador** reanuda las fibras listas por turnos, en un solo hilo del sistema operativo. `wait(s)` crea un temporizador; `nextFrame()` espera a la siguiente vuelta. Al terminar el programa principal, la VM sigue procesando tareas hasta que no queda ninguna.

## Eventos

`event` crea un objeto evento con la lista de sus parámetros. `on`/`once` añaden suscripciones ordenadas por prioridad; `emit` crea un objeto con los datos (`EventData`) y llama a cada manejador en orden hasta que alguno llama a `stopPropagation()`.

## Errores en ejecución

Un error es una instancia de `Error` (o de una clase que hereda de ella) con mensaje, tipo, archivo, línea y pila de llamadas. `throw` busca el manejador `try` más cercano en la fibra actual; si no lo hay, el error termina la fibra (y el programa, si es la principal) y se muestra con la línea de código señalada. Las funciones nativas lanzan errores con `vm.raise(...)`, que se convierten en errores normales del script.

## Código nativo

Las funciones nativas tienen la firma `Value f(VM&, int argc, Value* args)`; `args[-1]` es el receptor en los métodos. Se registran con una firma textual (`"(ruta: text): text"`) que sirve a la vez para la comprobación de argumentos, para los tipos del analizador, para el autocompletado del editor y para la referencia de la API. Ver [cómo ampliar HyperEZ](extender.md).
