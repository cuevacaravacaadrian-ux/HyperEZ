// Preludio: parte de la biblioteca estándar escrita en el propio HyperEZ.
// Se compila al arrancar la VM y sus nombres públicos quedan disponibles en todos los archivos.
namespace hz {

extern const char* kPreludeName;
extern const char* kPreludeSource;

const char* kPreludeName = "<preludio>";

const char* kPreludeSource = R"HZ(
/// Espera a que terminen todas las tareas y devuelve sus resultados en el mismo orden.
async function awaitAll(tareas: list): list {
    var resultados = []
    foreach t in tareas {
        resultados.add(await t)
    }
    return resultados
}

/// Devuelve el resultado de la primera tarea que termine.
async function awaitAny(tareas: list): any {
    while true {
        foreach t in tareas {
            if t.done {
                return await t
            }
        }
        await nextFrame()
    }
}
)HZ";

} // namespace hz
