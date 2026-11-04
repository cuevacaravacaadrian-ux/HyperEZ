# 9. Eventos y tareas asíncronas

## Eventos

Un evento separa "lo que pasa" de "quién reacciona". Se declara con sus datos, se escucha con `on` y se lanza con `emit`.

```hz
event Golpe(objetivo: text, danio: number)

on Golpe as e { print("${e.objetivo} pierde ${e.danio} de vida") }
once Golpe priority 10 as e { print("¡Primer golpe de la partida!") }  // una sola vez, antes que los demás

emit Golpe("Ana", 5)
emit Golpe(objetivo: "Rey", danio: 7)
```

- `on` devuelve una **suscripción**; `off(suscripcion)` la cancela y `off(Golpe)` cancela todas las del evento.
- `priority N`: los manejadores con más prioridad se ejecutan antes.
- Dentro de un manejador, `e.stopPropagation()` impide que se ejecuten los siguientes.

```hz
event Puntos(cantidad: number)
var total = 0
var sub = on Puntos as e { total += e.cantidad }
emit Puntos(10)
off(sub)
emit Puntos(99) // ya nadie escucha
print(total)    // 10
```

## Tareas asíncronas

Una función `async` puede **esperar** (`await`) sin bloquear el resto del programa: temporizadores, animaciones, oleadas de enemigos, descargas...

```hz
async function cuentaAtras(desde: number) {
    for i in desde..0 step -1 {
        print(i)
        await wait(0.1) // espera 0,1 segundos
    }
    print("¡Ya!")
}
await cuentaAtras(3)
```

| Función | Qué hace |
|---|---|
| `wait(segundos)` | tarea que termina pasado ese tiempo |
| `nextFrame()` | espera a la siguiente vuelta del planificador |
| `spawn(funcionAsync)` | lanza una tarea en segundo plano y la devuelve: `.cancel()`, `.done`, `.result`, `.failed`, `.error`, `.then(f)` |
| `awaitAll(tareas)` | espera a todas y devuelve sus resultados en orden |
| `awaitAny(tareas)` | devuelve el resultado de la primera que termine |

```hz
async function cargar(nombre: text): text {
    await wait(0.05)
    return "nivel ${nombre} listo"
}
print(await awaitAll([cargar("1"), cargar("2"), cargar("3")]))

var reloj = spawn(async () => {
    while true {
        await wait(1)
        print("tic")
    }
})
reloj.cancel()
print("reloj detenido:", reloj.done)
```

Un programa no termina mientras queden tareas pendientes. Las tareas avanzan por turnos en un solo hilo: no hace falta preocuparse de bloqueos ni de que dos tareas cambien lo mismo a la vez.
