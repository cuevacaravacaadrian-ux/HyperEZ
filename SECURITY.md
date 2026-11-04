# Seguridad

## Versiones con soporte

Solo la última versión publicada recibe correcciones de seguridad.

## Avisar de un problema de seguridad

Si encuentras un fallo que pueda poner en riesgo a quien usa HyperEZ (por ejemplo, que un programa o una petición web pueda leer archivos que no debería), **no abras una incidencia pública**. Usa *Security → Report a vulnerability* en la página del repositorio en GitHub, con:

- qué ocurre y cómo reproducirlo (el programa `.hz` más pequeño posible);
- la salida de `hyper version`;
- el sistema operativo.

Responderemos lo antes posible y publicaremos la corrección en una versión nueva.

## Ten en cuenta

Un programa HyperEZ puede hacer lo mismo que cualquier otro programa de tu usuario: leer y escribir archivos, ejecutar órdenes (`Process`) o conectarse a internet (`Http`). Ejecuta solo programas de quien confíes, igual que harías con un `.exe` o un script de Python. El servidor web (`Http.serve`) escucha por defecto solo en tu equipo (`127.0.0.1`).
