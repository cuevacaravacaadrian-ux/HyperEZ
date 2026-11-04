# Primeros pasos

Cuatro formas de escribir HyperEZ y ver el resultado al momento. La más sencilla es la primera.

## 1. HyperEZ Editor (Windows)

Se instala con HyperEZ y se abre al terminar la instalación (también está en el menú Inicio y en el escritorio).

1. Escribe tu programa (el editor trae uno de ejemplo).
2. Pulsa **F5**: te pide dónde guardarlo (como `.hez`, en `Documentos\HyperEZ`) y lo ejecuta en una ventana nueva.
3. Los errores aparecen abajo mientras escribes, con su línea. **Ctrl+Espacio** completa nombres y **F1** abre la guía.

En *Archivo → Abrir ejemplo* están todos los ejemplos y en *Archivo → Nuevo desde plantilla*, programas para empezar (juego, IA, web...). Todo lo que hace está en [HyperEZ Editor](../herramientas/editor.md).

## 2. La consola interactiva

Escribe `hyper` en una terminal (o abre *HyperEZ (consola interactiva)* en el menú Inicio, o *Abrir HyperEZ aquí* con clic derecho en una carpeta):

```text
hz> 2 + 3
5
hz> var nombre = "Ana"
hz> "Hola, " + nombre
"Hola, Ana"
hz> function doble(n: number): number {
...     return n * 2
... }
hz> doble(21)
42
hz> .guardar prueba.hz
✓ Guardado en prueba.hz (ejecútalo con: hyper prueba.hz)
```

- El valor de una expresión se muestra solo; lo que defines se conserva.
- Si dejas algo abierto (`{`, `(`, `[`) la entrada continúa en la línea siguiente; una línea vacía la termina.
- Puedes redefinir una función o variable escribiéndola otra vez.
- Ctrl+C detiene un bucle que no termina sin cerrar la consola.
- Comandos: `.ayuda`, `.vars`, `.codigo`, `.guardar archivo`, `.cargar archivo`, `.limpiar`, `.reiniciar`, `.salir`.

## 3. Visual Studio Code

1. Abre una carpeta (*Archivo → Abrir carpeta*).
2. Crea `hola.hz`:

```hz
var nombre = "mundo"
print("¡Hola, ${nombre}!")
for i in 1..=3 {
    print("Cuenta:", i)
}
```

3. Pulsa **F5**: el programa se ejecuta en una terminal.
4. Pulsa **Ctrl+Alt+F5** para el **modo watch**: cada vez que guardes (Ctrl+S) se vuelve a ejecutar solo.

Mientras escribes, VS Code subraya los errores, autocompleta (`nombre.` muestra los métodos de los textos) y enseña la documentación al pasar el ratón.

## 4. La terminal

```text
hyper hola.hz           ejecuta
hyper watch hola.hz     ejecuta y repite cada vez que guardas un .hz de la carpeta
hyper check hola.hz     busca errores sin ejecutar
```

También puedes hacer **doble clic** en un `.hz`: se ejecuta y la ventana espera a que pulses una tecla.

## Crear un proyecto

```text
hyper new mi-juego                 juego de consola (por defecto)
hyper new mi-app --tipo app        aplicación con menú y datos guardados
hyper new mi-ia --tipo ia          red neuronal que aprende de un CSV
hyper new mi-script --tipo script  herramienta de línea de órdenes
```

Todos los proyectos tienen la misma estructura:

```text
mi-juego/
  main.hez       punto de entrada (hyper run lo ejecuta)
  src/           módulos del proyecto
  tests/         pruebas (hyper test)
  datos/         archivos que usa el programa (si hacen falta)
  README.md
```

## Y ahora

- [Tutorial: un juego paso a paso](tutorial-juego.md)
- [Tutorial: una red neuronal](tutorial-ia.md)
- [La guía del lenguaje](../GUIA.md) y la [referencia completa](../lenguaje/README.md)
