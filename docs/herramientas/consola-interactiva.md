# Consola interactiva

`hyper` (o `hyper repl`) abre una consola donde cada línea se ejecuta al pulsar Intro. Es la forma más rápida de probar una idea, aprender una función o hacer cálculos.

```text
HyperEZ 0.7.0 · consola interactiva
Escribe código y pulsa Intro. Lo que definas se conserva entre líneas.
.ayuda muestra los comandos · .salir para terminar

hz> var precios = [3.5, 12, 7.25]
hz> precios.sum()
22.75
hz> precios.map(p => p * 1.21).map(p => round(p, 2))
[4.24, 14.52, 8.77]
```

## Cómo funciona

- Si escribes una **expresión**, se muestra su valor (los textos, entre comillas).
- Las **variables, funciones y clases** que defines siguen disponibles en las líneas siguientes.
- Puedes **redefinir** algo escribiéndolo otra vez: las funciones que lo usaban pasan a usar la versión nueva.
- Si una línea deja abierto un `{`, `(` o `[`, termina en un operador o abre un `"""`, la entrada **continúa** (`...`); una línea vacía la termina.
- Los **errores** se muestran con la línea de lo que escribiste y no borran nada de lo anterior.
- **Ctrl+C** detiene un bucle infinito sin cerrar la consola.
- Puedes **importar** módulos y bibliotecas (`import ai`, `from "./mio.hz" import X`) relativos a la carpeta actual.

## Comandos

| Comando | Qué hace |
|---|---|
| `.ayuda` | lista los comandos |
| `.vars` | variables, funciones y clases definidas, con sus valores |
| `.codigo` | todo el código de la sesión |
| `.guardar [archivo]` | convierte la sesión en un programa (por defecto `sesion.hz`): las expresiones sueltas se escriben como `print(...)` y se quitan las líneas que fallaron |
| `.cargar archivo` | ejecuta un archivo dentro de la sesión (sus definiciones quedan disponibles) |
| `.limpiar` | limpia la pantalla |
| `.reiniciar` | empieza una sesión nueva |
| `.salir` | termina (también `salir`, `exit` o Ctrl+Z e Intro en Windows / Ctrl+D en Linux y macOS) |

## Desde otros sitios

- **Windows**: menú Inicio → *HyperEZ (consola interactiva)*, o clic derecho en el fondo de una carpeta → *Abrir HyperEZ aquí*.
- **VS Code**: *HyperEZ: Abrir consola interactiva*.
- **Con una tubería**: `echo "2 ** 10" | hyper repl` imprime `1024` (útil en scripts).
