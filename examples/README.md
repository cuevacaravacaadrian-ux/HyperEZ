# Ejemplos

Programas completos, organizados por áreas. Ejecútalos con `hyper <archivo.hz>` (o con F5 en VS Code). Los juegos y las aplicaciones se manejan con el teclado y, cuando no hay teclado (en las pruebas), se juegan solos.

| Área | Ejemplo | Qué enseña |
|---|---|---|
| **Básico** | [basico/hola.hz](basico/hola.hz) | variables, bucles y textos |
| | [basico/adivina.hz](basico/adivina.hz) | `input`, números aleatorios y `while` |
| **Juegos** | [juegos/serpiente.hz](juegos/serpiente.hz) | juego en tiempo real con `Console.pollKey` |
| | [juegos/tres_en_raya.hz](juegos/tres_en_raya.hz) | IA con minimax y poda alfa-beta: nunca pierde |
| | [juegos/mazmorra.hz](juegos/mazmorra.hz) | clases, eventos, `enum`, `async` y combate por turnos |
| **Inteligencia artificial** | [ia/red_neuronal.hz](ia/red_neuronal.hz) | una red neuronal aprende XOR y a reconocer un círculo |
| | [ia/regresion.hz](ia/regresion.hz) | regresión lineal, k vecinos y k-medias |
| **Datos** | [datos/ventas.hz](datos/ventas.hz) | leer un CSV, estadísticas, agrupar y guardar un informe |
| **Web** | [web/puntuaciones.hz](web/puntuaciones.hz) | página HTML + API JSON + base de datos |
| **Red** | [red/api_web.hz](red/api_web.hz) | consultar APIs web reales (funciona también sin conexión) |
| **Gráficos** | [graficos/grafica.hz](graficos/grafica.hz) | gráfica de barras en PNG con texto |
| | [graficos/fractal.hz](graficos/fractal.hz) | el conjunto de Mandelbrot píxel a píxel |
| **Sonido** | [audio/melodia.hz](audio/melodia.hz) | componer música y guardarla en WAV |
| **Aplicaciones** | [apps/agenda.hz](apps/agenda.hz) | aplicación de consola con menú y datos en JSON |
| **Herramientas** | [herramientas/contar_palabras.hz](herramientas/contar_palabras.hz) | argumentos, expresiones regulares y tablas |
| **Sistema** | [sistema/organizar.hz](sistema/organizar.hz) | ordenar archivos en carpetas según su tipo |
| | [sistema/info.hz](sistema/info.hz) | información del sistema y otros programas |
| **Sistemas operativos** | [sistemas/sector_arranque.hz](sistemas/sector_arranque.hz) | un sector de arranque x86 que arranca en QEMU |

Los archivos `.out` junto a cada ejemplo son su salida esperada: `hyper test examples` comprueba que todos siguen funcionando.
