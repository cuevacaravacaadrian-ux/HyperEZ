// La Academia HyperEZ: las misiones del curso (lección, reto y preguntas) y el examen final.
// Cada reto se comprueba ejecutando HyperEZ de verdad (WebAssembly), de una de estas formas:
//   salida:    lo que tiene que mostrar el programa, exactamente
//   variantes: se cambia una línea del programa por otra y se comprueba la salida (para que no valga escribirla a mano)
//   pruebas:   código oculto con test(...) que se añade al final del programa (funciones y clases)
//   usa:       expresiones regulares que el código tiene que cumplir, con su mensaje
// projects/web/tests/aprender.test.js comprueba que cada solución pasa, que el código inicial no, y que los ejemplos funcionan.
(function (global) {
  'use strict';

  const MUNDOS = [
    { id: 1, nombre: 'La Puerta del Código', color: '#62d962', descripcion: 'Tus primeras líneas: mostrar, guardar y calcular.' },
    { id: 2, nombre: 'El Bosque de las Decisiones', color: '#58a6ff', descripcion: 'Textos, decisiones y bucles: el programa empieza a pensar.' },
    { id: 3, nombre: 'Las Minas de los Datos', color: '#ffcb6b', descripcion: 'Listas y funciones: guardar muchas cosas y reutilizar tu código.' },
    { id: 4, nombre: 'La Torre del Maestro', color: '#c792ea', descripcion: 'Mapas y clases: modelar el mundo de tu juego.' },
  ];

  const MISIONES = [
    // ------------------------------------------------------------------------------------------- 1
    {
      id: 'hola', mundo: 1, icono: '👋', titulo: '¡Hola, mundo!', xp: 100,
      historia: 'Llegas a la Academia HyperEZ. Un guardián de piedra bloquea la puerta: solo deja pasar a quien sabe hablarle en código.',
      leccion: [
        { texto: '<p>Un programa es una lista de órdenes que el ordenador ejecuta <b>de arriba abajo</b>. La primera orden que vas a aprender es <code>print</code>: muestra en la pantalla lo que le pongas entre paréntesis.</p>' },
        { codigo: 'print("¡Hola, mundo!")' },
        { texto: '<p>Los <b>textos</b> van entre comillas <code>"..."</code>. Los números, sin comillas. Si le das varias cosas a <code>print</code> separadas por comas, las muestra en la misma línea con un espacio entre ellas.</p>' },
        { codigo: 'print("Tengo", 3, "vidas")\nprint(2 + 2)' },
        { texto: '<p>Lo que va detrás de <code>//</code> es un <b>comentario</b>: HyperEZ no lo ejecuta. Sirve para explicar tu código a quien lo lea (también a ti dentro de un mes).</p>' },
        { codigo: '// Esto no hace nada: es una nota para las personas\nprint("Esto sí se ejecuta")' },
      ],
      reto: {
        enunciado: 'El guardián solo abre la puerta si le saludas con <b>estas dos líneas exactas</b>:',
        esperado: '¡Hola, HyperEZ!\nSoy aprendiz de programación',
        inicial: '// Escribe aquí las dos órdenes print\n',
        solucion: 'print("¡Hola, HyperEZ!")\nprint("Soy aprendiz de programación")\n',
        pistas: ['Necesitas dos órdenes <code>print</code>, una por línea.', 'Copia los textos tal cual, con sus signos: <code>print("¡Hola, HyperEZ!")</code>.'],
        comprobar: { salida: '¡Hola, HyperEZ!\nSoy aprendiz de programación' },
      },
      preguntas: [
        { p: '¿Qué muestra <code>print("Hola", 2, "veces")</code>?', o: ['Hola 2 veces', 'Hola2veces', 'Hola, 2, veces', 'Un error'], ok: 0,
          porque: '<code>print</code> separa con un espacio cada cosa que le das.' },
        { p: '¿Cuál de estas líneas es un comentario en HyperEZ?', o: ['# una nota', '// una nota', '<!-- una nota -->', "' una nota"], ok: 1,
          porque: 'Los comentarios empiezan por <code>//</code>.' },
        { p: '¿Para qué sirven las comillas en <code>"Hola"</code>?', o: ['Para hacer una cuenta', 'Para que sea un texto', 'Para crear una variable', 'Para comentar'], ok: 1,
          porque: 'Todo lo que va entre comillas es un texto.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 2
    {
      id: 'variables', mundo: 1, icono: '📦', titulo: 'Cajas con nombre', xp: 100,
      historia: 'Dentro de la academia te dan una mochila mágica con compartimentos. Cada compartimento tiene un nombre y guarda una cosa.',
      leccion: [
        { texto: '<p>Una <b>variable</b> es una caja con nombre donde guardas un valor. Se crea con <code>var</code>, y luego puedes usarla y cambiarla.</p>' },
        { codigo: 'var heroe = "Luna"\nvar vidas = 3\nprint(heroe, "tiene", vidas, "vidas")\nvidas = vidas - 1\nprint("Ahora le quedan", vidas)' },
        { texto: '<p>Para meter variables dentro de un texto, escribe <code>${...}</code>: HyperEZ pone ahí su valor. Es la forma más cómoda de componer frases.</p>' },
        { codigo: 'var heroe = "Luna"\nvar nivel = 7\nprint("${heroe} está en el nivel ${nivel}")\nprint("El nivel siguiente es el ${nivel + 1}")' },
        { texto: '<p><code>nivel += 1</code> es lo mismo que <code>nivel = nivel + 1</code>. Y si un valor no debe cambiar nunca, usa <code>const</code>: HyperEZ te avisará si intentas cambiarlo.</p>' },
        { codigo: 'const MAX_VIDAS = 5\nvar vidas = 2\nvidas += 1\nprint("${vidas} de ${MAX_VIDAS}")' },
      ],
      reto: {
        enunciado: 'Tu héroe <b>Orion</b> empieza en el nivel 1. Súbele el nivel <b>dos veces</b> (sumando 1 cada vez) y muestra, usando las variables:',
        esperado: 'Orion ha llegado al nivel 3',
        inicial: 'var nombre = "Orion"\nvar nivel = 1\n\n// sube el nivel dos veces\n\n// y muéstralo con print("${...}")\n',
        solucion: 'var nombre = "Orion"\nvar nivel = 1\nnivel = nivel + 1\nnivel += 1\nprint("${nombre} ha llegado al nivel ${nivel}")\n',
        pistas: ['Para subir el nivel: <code>nivel += 1</code> (dos veces).', 'Para mostrarlo: <code>print("${nombre} ha llegado al nivel ${nivel}")</code>.'],
        comprobar: {
          variantes: [
            { cambia: ['var nivel = 1', 'var nivel = 1'], salida: 'Orion ha llegado al nivel 3' },
            { cambia: ['var nivel = 1', 'var nivel = 5'], salida: 'Orion ha llegado al nivel 7' },
            { cambia: ['var nombre = "Orion"', 'var nombre = "Vega"'], salida: 'Vega ha llegado al nivel 3' },
          ],
          usa: [[/\$\{\s*nivel\s*\}/, 'Muestra el nivel con <code>${nivel}</code> dentro del texto.']],
        },
      },
      preguntas: [
        { p: 'Después de <code>var x = 5</code> y <code>x = x + 2</code>, ¿cuánto vale <code>x</code>?', o: ['5', '2', '7', '"x + 2"'], ok: 2, porque: 'Primero vale 5 y luego se le suma 2.' },
        { p: '¿Qué diferencia hay entre <code>var</code> y <code>const</code>?', o: ['Ninguna', 'const no se puede cambiar después', 'var solo guarda números', 'const solo guarda textos'], ok: 1,
          porque: 'Una constante guarda un valor fijo.' },
        { p: 'Con <code>var n = "Ana"</code>, ¿qué muestra <code>print("Hola ${n}")</code>?', o: ['Hola ${n}', 'Hola n', 'Hola Ana', 'Un error'], ok: 2,
          porque: '<code>${n}</code> se cambia por el valor de <code>n</code>.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 3
    {
      id: 'numeros', mundo: 1, icono: '🪙', titulo: 'La tienda del gremio', xp: 100,
      historia: 'El gremio de aventureros tiene una tienda. Antes de comprar, tienes que saber hacer cuentas con tus monedas.',
      leccion: [
        { texto: '<p>HyperEZ sabe hacer cuentas: <code>+</code> suma, <code>-</code> resta, <code>*</code> multiplica, <code>/</code> divide, <code>%</code> da el resto de una división y <code>**</code> es la potencia. Los paréntesis deciden qué va primero.</p>' },
        { codigo: 'print(7 + 2, 7 - 2, 7 * 2, 7 / 2)\nprint(7 % 2, 2 ** 10)\nprint((2 + 3) * 4, 2 + 3 * 4)' },
        { texto: '<p>Las cuentas se pueden guardar en variables y usar con <code>+=</code>, <code>-=</code> y <code>*=</code>. Para redondear, <code>round(número, decimales)</code>.</p>' },
        { codigo: 'var oro = 120\noro -= 45       // compras una espada\noro += 30       // vendes una poción\nprint("Oro:", oro)\nprint(round(10 / 3, 2))' },
      ],
      reto: {
        enunciado: 'Tienes <b>250 monedas</b> y compras <b>3 pociones</b> de <b>40 monedas</b>. Calcula con las variables cuánto te queda y muéstralo así:',
        esperado: 'Te quedan 130 monedas',
        inicial: 'var monedas = 250\nvar pociones = 3\nconst PRECIO = 40\n\n// resta lo que cuestan las pociones y muestra el resultado\n',
        solucion: 'var monedas = 250\nvar pociones = 3\nconst PRECIO = 40\nmonedas -= pociones * PRECIO\nprint("Te quedan ${monedas} monedas")\n',
        pistas: ['Las pociones cuestan <code>pociones * PRECIO</code>.', '<code>monedas -= pociones * PRECIO</code> y después <code>print("Te quedan ${monedas} monedas")</code>.'],
        comprobar: {
          variantes: [
            { cambia: ['var pociones = 3', 'var pociones = 3'], salida: 'Te quedan 130 monedas' },
            { cambia: ['var pociones = 3', 'var pociones = 5'], salida: 'Te quedan 50 monedas' },
            { cambia: ['var monedas = 250', 'var monedas = 1000'], salida: 'Te quedan 880 monedas' },
          ],
        },
      },
      preguntas: [
        { p: '¿Cuánto es <code>7 % 3</code>?', o: ['2,33', '1', '21', '4'], ok: 1, porque: '7 entre 3 da 2 y sobra 1: <code>%</code> es el resto.' },
        { p: '¿Cuánto es <code>2 + 3 * 4</code>?', o: ['20', '14', '24', '9'], ok: 1, porque: 'La multiplicación va antes que la suma: 2 + 12.' },
        { p: '¿Qué hace <code>vida -= 10</code>?', o: ['Pone la vida a -10', 'Resta 10 a la vida', 'Compara la vida con 10', 'Nada'], ok: 1, porque: 'Es lo mismo que <code>vida = vida - 10</code>.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 4
    {
      id: 'textos', mundo: 2, icono: '📜', titulo: 'El pergamino secreto', xp: 100,
      historia: 'En la entrada del bosque encuentras un pergamino con un mensaje desordenado. Para leerlo tendrás que transformarlo.',
      leccion: [
        { texto: '<p>Los textos tienen <b>funciones propias</b> que se escriben con un punto: <code>.upper()</code> lo pone en mayúsculas, <code>.lower()</code> en minúsculas, <code>.trim()</code> quita los espacios de los lados y <code>.length</code> dice cuántos caracteres tiene.</p>' },
        { codigo: 'var hechizo = "  bola de fuego  "\nprint(hechizo.trim().upper())\nprint(hechizo.trim().length)\nprint(hechizo.contains("fuego"))' },
        { texto: '<p>Los textos se pueden unir con <code>+</code>, y dentro de <code>${...}</code> puedes darles formato: <code>:.2f</code> deja dos decimales.</p>' },
        { codigo: 'var nombre = "Luna"\nprint("¡" + nombre + "!")\nvar precio = 9.5\nprint("Precio: ${precio:.2f} monedas")\nprint("${nombre.upper()} tiene ${nombre.length} letras")' },
      ],
      reto: {
        enunciado: 'El mensaje del pergamino tiene espacios de más. Muéstralo <b>sin los espacios de los lados y en mayúsculas</b>, y en la línea siguiente <b>cuántos caracteres tiene</b> (ya sin esos espacios):',
        esperado: 'EL TESORO ESTA EN LA CUEVA\nTiene 26 caracteres',
        inicial: 'var mensaje = "   el tesoro esta en la cueva   "\n\n// muestra el mensaje limpio y en mayúsculas\n// y cuántos caracteres tiene\n',
        solucion: 'var mensaje = "   el tesoro esta en la cueva   "\nvar limpio = mensaje.trim()\nprint(limpio.upper())\nprint("Tiene ${limpio.length} caracteres")\n',
        pistas: ['Primero <code>mensaje.trim()</code>, y sobre eso <code>.upper()</code>.', 'Guarda el texto limpio: <code>var limpio = mensaje.trim()</code> y usa <code>limpio.length</code>.'],
        comprobar: {
          variantes: [
            { cambia: ['var mensaje = "   el tesoro esta en la cueva   "', 'var mensaje = "   el tesoro esta en la cueva   "'], salida: 'EL TESORO ESTA EN LA CUEVA\nTiene 26 caracteres' },
            { cambia: ['var mensaje = "   el tesoro esta en la cueva   "', 'var mensaje = "  sigue al norte "'], salida: 'SIGUE AL NORTE\nTiene 14 caracteres' },
          ],
        },
      },
      preguntas: [
        { p: '¿Qué da <code>"hola".upper()</code>?', o: ['hola', 'HOLA', 'Hola', '4'], ok: 1, porque: '<code>upper</code> pasa todo a mayúsculas.' },
        { p: '¿Cuánto vale <code>"abc".length</code>?', o: ['2', '3', '"abc"', 'abc3'], ok: 1, porque: 'Tiene tres caracteres.' },
        { p: '¿Qué muestra <code>print("${2 + 3}")</code>?', o: ['2 + 3', '${2 + 3}', '5', '23'], ok: 2, porque: 'Dentro de <code>${...}</code> se calcula la expresión.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 5
    {
      id: 'decisiones', mundo: 2, icono: '🔀', titulo: 'El cruce de caminos', xp: 120,
      historia: 'El camino se divide en tres. Tu programa tendrá que decidir qué hacer según la vida que le quede a tu héroe.',
      leccion: [
        { texto: '<p><code>if</code> ejecuta un bloque <code>{ ... }</code> solo si se cumple una condición. Con <code>else if</code> añades más casos y con <code>else</code>, lo que pasa si no se cumple ninguno.</p>' },
        { codigo: 'var vida = 2\nif vida <= 0 {\n    print("Fin de la partida")\n} else if vida == 1 {\n    print("¡Cuidado!")\n} else {\n    print("Todo bien")\n}' },
        { texto: '<p>Para comparar: <code>==</code> (igual), <code>!=</code> (distinto), <code>&lt;</code>, <code>&lt;=</code>, <code>&gt;</code>, <code>&gt;=</code>. Y para juntar condiciones: <code>and</code>, <code>or</code> y <code>not</code>.</p>' },
        { codigo: 'var llave = true\nvar nivel = 4\nif llave and nivel >= 3 {\n    print("Abres la puerta")\n}\nprint(5 > 3, 5 == 3, not (5 == 3))' },
      ],
      reto: {
        enunciado: 'Según la <code>vida</code> del héroe, muestra <b>una</b> de estas tres frases:<ul><li>si la vida es <b>0 o menos</b>: <code>Fin de la partida</code></li><li>si es <b>menos de 50</b>: <code>Herido</code></li><li>si no: <code>Todo bien</code></li></ul>Lo comprobaremos cambiando la vida a otros valores.',
        esperado: 'Herido',
        inicial: 'var vida = 35\n\n// usa if, else if y else\n',
        solucion: 'var vida = 35\nif vida <= 0 {\n    print("Fin de la partida")\n} else if vida < 50 {\n    print("Herido")\n} else {\n    print("Todo bien")\n}\n',
        pistas: ['Empieza por el caso más extremo: <code>if vida &lt;= 0 { ... }</code>.', 'Luego <code>else if vida &lt; 50 { ... }</code> y por último <code>else { ... }</code>.'],
        comprobar: {
          variantes: [
            { cambia: ['var vida = 35', 'var vida = 35'], salida: 'Herido' },
            { cambia: ['var vida = 35', 'var vida = 0'], salida: 'Fin de la partida' },
            { cambia: ['var vida = 35', 'var vida = -8'], salida: 'Fin de la partida' },
            { cambia: ['var vida = 35', 'var vida = 50'], salida: 'Todo bien' },
            { cambia: ['var vida = 35', 'var vida = 100'], salida: 'Todo bien' },
          ],
          usa: [[/\bif\b/, 'Usa <code>if</code> para decidir.'], [/\belse\b/, 'Usa <code>else</code> para el resto de casos.']],
        },
      },
      preguntas: [
        { p: '¿Qué da <code>5 &gt; 3 and 2 &gt; 4</code>?', o: ['true', 'false', '5', 'Un error'], ok: 1, porque: 'Con <code>and</code> tienen que cumplirse las dos, y 2 no es mayor que 4.' },
        { p: 'Con <code>var x = 10</code>, ¿qué muestra <code>if x &gt; 5 { print("A") } else { print("B") }</code>?', o: ['A', 'B', 'A y B', 'Nada'], ok: 0, porque: '10 es mayor que 5.' },
        { p: '¿Qué operador comprueba si dos valores son iguales?', o: ['=', '==', '===', 'equals'], ok: 1, porque: '<code>=</code> guarda un valor; <code>==</code> compara.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 6
    {
      id: 'bucles', mundo: 2, icono: '🔁', titulo: 'La escalera mágica', xp: 120,
      historia: 'Una escalera de piedra sube hacia la copa del árbol más alto. Cada escalón tiene un número… y repetir es la clave.',
      leccion: [
        { texto: '<p>Un <b>bucle</b> repite un bloque. <code>repeat 3 { ... }</code> lo repite 3 veces. <code>for i in 1..=5 { ... }</code> lo repite con <code>i</code> valiendo 1, 2, 3, 4 y 5 (con <code>..</code> en vez de <code>..=</code>, el último no se incluye).</p>' },
        { codigo: 'repeat 3 {\n    print("¡Ataque!")\n}\nfor i in 1..=5 {\n    print("Escalón", i)\n}' },
        { texto: '<p><code>foreach</code> recorre los elementos de una lista, y <code>while</code> repite mientras se cumpla una condición.</p>' },
        { codigo: 'foreach fruta in ["manzana", "pera", "uva"] {\n    print("Comes una", fruta)\n}\nvar energia = 3\nwhile energia > 0 {\n    print("Energía:", energia)\n    energia -= 1\n}' },
      ],
      reto: {
        enunciado: 'Cada escalón muestra su <b>cuadrado</b>. Usa un bucle para mostrar del 1 al 6, uno por línea, con este formato:',
        esperado: '1 x 1 = 1\n2 x 2 = 4\n3 x 3 = 9\n4 x 4 = 16\n5 x 5 = 25\n6 x 6 = 36',
        inicial: '// usa un bucle: for i in 1..=6 { ... }\n',
        solucion: 'for i in 1..=6 {\n    print("${i} x ${i} = ${i * i}")\n}\n',
        pistas: ['<code>for i in 1..=6 { ... }</code> repite con i = 1, 2... 6.', 'Dentro: <code>print("${i} x ${i} = ${i * i}")</code>.'],
        comprobar: {
          salida: '1 x 1 = 1\n2 x 2 = 4\n3 x 3 = 9\n4 x 4 = 16\n5 x 5 = 25\n6 x 6 = 36',
          usa: [[/\b(for|foreach|while|repeat)\b/, 'Usa un bucle (<code>for</code>, <code>while</code>...) en vez de escribir cada línea.'],
            [/^(?![\s\S]*print[\s\S]*print[\s\S]*print)/, 'Con un solo <code>print</code> dentro del bucle es suficiente.']],
        },
      },
      preguntas: [
        { p: '¿Cuántas veces se repite <code>for i in 0..3 { ... }</code>?', o: ['2', '3', '4', 'Ninguna'], ok: 1, porque: '<code>0..3</code> va de 0 a 2: 0, 1 y 2.' },
        { p: '¿Y <code>for i in 1..=3 { ... }</code>?', o: ['2', '3', '4', '1'], ok: 1, porque: '<code>..=</code> incluye el último: 1, 2 y 3.' },
        { p: 'Si la condición de un <code>while</code> es falsa desde el principio…', o: ['se ejecuta una vez', 'no se ejecuta nunca', 'da un error', 'se repite para siempre'], ok: 1, porque: 'El <code>while</code> mira la condición antes de cada vuelta.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 7
    {
      id: 'listas', mundo: 3, icono: '💎', titulo: 'El botín de la mina', xp: 120,
      historia: 'En las minas encuentras gemas de distintos valores. Necesitas organizarlas para saber cuánto vale tu botín.',
      leccion: [
        { texto: '<p>Una <b>lista</b> guarda muchos valores en orden: <code>[30, 12, 50]</code>. Se cuentan desde 0: <code>botin[0]</code> es el primero y <code>botin[-1]</code> el último. <code>.add(x)</code> añade, <code>.length</code> cuenta y <code>.sum()</code> suma.</p>' },
        { codigo: 'var botin = [30, 12, 50, 8]\nbotin.add(25)\nprint(botin)\nprint("Primero:", botin[0], "Último:", botin[-1])\nprint("Total:", botin.sum(), "Gemas:", botin.length, "Mejor:", max(botin))' },
        { texto: '<p>Para quedarte con algunos elementos, <code>.filter(x =&gt; condición)</code>; para transformarlos todos, <code>.map(x =&gt; ...)</code> o una <b>comprensión</b>: <code>[x * 2 for x in lista]</code>.</p>' },
        { codigo: 'var botin = [30, 12, 50, 8, 25]\nprint(botin.filter(x => x >= 25))\nprint(botin.map(x => x * 2))\nprint([x for x in botin if x < 20])' },
      ],
      reto: {
        enunciado: 'Tienes las puntuaciones de tu equipo en <code>puntos</code>. Muestra el <b>total</b>, la <b>mejor</b> y la lista de las que son de <b>40 o más</b>:',
        esperado: 'Total: 236\nMejor: 88\nAprobados: [45, 88, 61]',
        inicial: 'var puntos = [12, 45, 7, 88, 23, 61]\n\n// Total, Mejor y Aprobados (40 o más)\n',
        solucion: 'var puntos = [12, 45, 7, 88, 23, 61]\nprint("Total: ${puntos.sum()}")\nprint("Mejor: ${max(puntos)}")\nprint("Aprobados: ${puntos.filter(p => p >= 40)}")\n',
        pistas: ['<code>puntos.sum()</code> y <code>max(puntos)</code> te dan los dos primeros.', 'Para los aprobados: <code>puntos.filter(p =&gt; p &gt;= 40)</code>.'],
        comprobar: {
          variantes: [
            { cambia: ['var puntos = [12, 45, 7, 88, 23, 61]', 'var puntos = [12, 45, 7, 88, 23, 61]'], salida: 'Total: 236\nMejor: 88\nAprobados: [45, 88, 61]' },
            { cambia: ['var puntos = [12, 45, 7, 88, 23, 61]', 'var puntos = [10, 50, 40]'], salida: 'Total: 100\nMejor: 50\nAprobados: [50, 40]' },
          ],
        },
      },
      preguntas: [
        { p: '¿Qué es <code>[5, 6, 7][1]</code>?', o: ['5', '6', '7', 'Un error'], ok: 1, porque: 'Se cuenta desde 0: la posición 1 es el segundo.' },
        { p: 'Tras <code>var l = [1, 2, 3]</code> y <code>l.add(4)</code>, ¿cuánto vale <code>l.length</code>?', o: ['3', '4', '5', '10'], ok: 1, porque: 'Ahora tiene cuatro elementos.' },
        { p: '¿Qué da <code>[1, 2, 3, 4].filter(x =&gt; x &gt; 2)</code>?', o: ['[1, 2]', '[3, 4]', '[2, 3, 4]', '2'], ok: 1, porque: 'Se quedan los mayores que 2.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 8
    {
      id: 'funciones', mundo: 3, icono: '⚒️', titulo: 'La forja del herrero', xp: 150,
      historia: 'El herrero de la mina te enseña su secreto: no repetir el trabajo. Una buena herramienta se forja una vez y se usa siempre.',
      leccion: [
        { texto: '<p>Una <b>función</b> es un trozo de código con nombre que puedes usar muchas veces. Recibe <b>parámetros</b> y con <code>return</code> devuelve un resultado.</p>' },
        { codigo: 'function doble(x: number): number {\n    return x * 2\n}\nprint(doble(4), doble(doble(3)))' },
        { texto: '<p>Los tipos (<code>: number</code>, <code>: text</code>, <code>: bool</code>) son opcionales, pero ayudan: HyperEZ te avisa si usas mal la función. Un parámetro puede tener <b>valor por defecto</b>.</p>' },
        { codigo: 'function saludo(nombre: text, signo: text = "!"): text {\n    return "Hola, ${nombre}${signo}"\n}\nprint(saludo("Luna"))\nprint(saludo("Orion", "?"))' },
        { texto: '<p>Las funciones cortas se pueden escribir con una flecha: <code>var triple = x =&gt; x * 3</code>. Es lo que usabas en <code>.filter(...)</code>.</p>' },
      ],
      reto: {
        enunciado: 'El herrero necesita dos funciones:<ul><li><code>danio(fuerza, arma)</code> devuelve <b>fuerza × arma</b>.</li><li><code>esCritico(d)</code> devuelve <code>true</code> si el daño <code>d</code> es <b>50 o más</b>, y <code>false</code> si no.</li></ul>Las comprobaremos con pruebas ocultas.',
        inicial: 'function danio(fuerza: number, arma: number): number {\n    return 0 // cámbialo\n}\n\nfunction esCritico(d: number): bool {\n    return false // cámbialo\n}\n\nprint(danio(12, 5), esCritico(danio(12, 5)))\n',
        solucion: 'function danio(fuerza: number, arma: number): number {\n    return fuerza * arma\n}\n\nfunction esCritico(d: number): bool {\n    return d >= 50\n}\n\nprint(danio(12, 5), esCritico(danio(12, 5)))\n',
        pistas: ['<code>return fuerza * arma</code>.', 'Una comparación ya es <code>true</code> o <code>false</code>: <code>return d &gt;= 50</code>.'],
        comprobar: {
          pruebas: 'test("danio multiplica fuerza y arma", () => {\n    assertEqual(danio(10, 3), 30)\n    assertEqual(danio(7, 0), 0)\n    assertEqual(danio(12, 5), 60)\n})\ntest("esCritico desde 50", () => {\n    assert(esCritico(50))\n    assert(esCritico(80))\n    assert(not esCritico(49))\n})',
        },
      },
      preguntas: [
        { p: '¿Qué hace <code>return</code>?', o: ['Muestra un valor', 'Devuelve el resultado de la función', 'Repite la función', 'Borra una variable'], ok: 1, porque: 'La función termina y su llamada vale lo que devuelve.' },
        { p: 'Con <code>function doble(x) { return x * 2 }</code>, ¿cuánto es <code>doble(doble(3))</code>?', o: ['6', '9', '12', '3'], ok: 2, porque: 'doble(3) = 6 y doble(6) = 12.' },
        { p: 'Con <code>function f(n = 10) { return n }</code>, ¿qué da <code>f()</code>?', o: ['0', 'null', '10', 'Un error'], ok: 2, porque: 'Sin argumento se usa el valor por defecto.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 9
    {
      id: 'mapas', mundo: 4, icono: '🗺️', titulo: 'El inventario del mago', xp: 150,
      historia: 'En la torre, el mago guarda cientos de objetos. Para no perderse, cada uno tiene una etiqueta con su nombre.',
      leccion: [
        { texto: '<p>Un <b>mapa</b> guarda valores con <b>clave</b>: <code>{ nombre: "Ana", vida: 3 }</code>. Se lee con un punto o con corchetes, y se pueden añadir claves nuevas.</p>' },
        { codigo: 'var jugador = { nombre: "Ana", vida: 3 }\njugador.oro = 10\njugador["nivel"] = 2\nprint(jugador.nombre, jugador["vida"], jugador.keys())' },
        { texto: '<p><code>.get(clave, porDefecto)</code> da el valor o, si la clave no está, el que tú digas. Y <code>for clave, valor in mapa</code> recorre todo el mapa.</p>' },
        { codigo: 'var precios = { espada: 120, escudo: 80 }\nprint(precios.get("arco", 0))\nfor objeto, precio in precios {\n    print(objeto, "cuesta", precio)\n}' },
      ],
      reto: {
        enunciado: 'Escribe la función <code>inventario(objetos)</code>: recibe una lista de nombres y devuelve un mapa con <b>cuántas veces aparece cada uno</b>. Por ejemplo, <code>inventario(["poción", "espada", "poción"])</code> devuelve <code>{poción: 2, espada: 1}</code>.',
        inicial: 'function inventario(objetos: list): map {\n    var cuenta = {}\n    // recorre los objetos y suma 1 a cada uno\n    return cuenta\n}\n\nprint(inventario(["poción", "espada", "poción"]))\n',
        solucion: 'function inventario(objetos: list): map {\n    var cuenta = {}\n    foreach o in objetos {\n        cuenta[o] = cuenta.get(o, 0) + 1\n    }\n    return cuenta\n}\n\nprint(inventario(["poción", "espada", "poción"]))\n',
        pistas: ['Recorre la lista con <code>foreach o in objetos { ... }</code>.', 'Dentro: <code>cuenta[o] = cuenta.get(o, 0) + 1</code>.'],
        comprobar: {
          pruebas: 'test("cuenta los objetos repetidos", () => {\n    var inv = inventario(["poción", "espada", "poción"])\n    assertEqual(inv["poción"], 2)\n    assertEqual(inv["espada"], 1)\n})\ntest("muchos objetos", () => {\n    var inv = inventario(["a", "b", "a", "a", "c"])\n    assertEqual(inv["a"], 3)\n    assertEqual(inv.keys().length, 3)\n})\ntest("una lista vacía", () => {\n    assertEqual(inventario([]).keys().length, 0)\n})',
        },
      },
      preguntas: [
        { p: 'Con <code>var m = { oro: 5 }</code>, ¿qué da <code>m.oro</code>?', o: ['"oro"', '5', 'null', 'Un error'], ok: 1, porque: 'Se lee el valor de la clave <code>oro</code>.' },
        { p: '¿Qué da <code>{ a: 1 }.get("b", 0)</code>?', o: ['1', '0', 'null', 'Un error'], ok: 1, porque: 'No existe la clave <code>b</code>: devuelve el valor por defecto.' },
        { p: '¿Cómo añades la clave <code>nivel</code> a <code>jugador</code>?', o: ['jugador.add("nivel")', 'jugador.nivel = 2', 'nivel in jugador', 'var jugador.nivel'], ok: 1, porque: 'Basta con darle un valor.' },
      ],
    },
    // ------------------------------------------------------------------------------------------- 10
    {
      id: 'clases', mundo: 4, icono: '🛡️', titulo: 'El héroe definitivo', xp: 200,
      historia: 'Última prueba: el maestro de la torre te pide que crees tu propio héroe, con su vida, su nombre y sus reglas.',
      leccion: [
        { texto: '<p>Una <b>clase</b> es un molde para crear objetos con sus propios datos (<b>campos</b>) y acciones (<b>métodos</b>). El <code>constructor</code> se ejecuta al crear cada objeto, y <code>this</code> es el objeto.</p>' },
        { codigo: 'class Monstruo {\n    var nombre: text\n    var vida: number = 30\n\n    constructor(nombre: text) {\n        this.nombre = nombre\n    }\n\n    function rugir() {\n        print("${nombre}: ¡GRRR!")\n    }\n}\n\nvar m = Monstruo("Trasgo")\nm.rugir()\nprint(m.vida)' },
        { texto: '<p>Un <code>get</code> es un dato que se calcula al leerlo, sin paréntesis. Y <code>max(a, b)</code> te da el mayor de dos valores, útil para que algo no baje de cero.</p>' },
        { codigo: 'class Cofre {\n    var monedas: number = 0\n    get lleno(): bool { return monedas >= 100 }\n    function sacar(n: number) { monedas = max(0, monedas - n) }\n}\n\nvar c = Cofre()\nc.monedas = 120\nprint(c.lleno)\nc.sacar(500)\nprint(c.monedas, c.lleno)' },
      ],
      reto: {
        enunciado: 'Completa la clase <code>Heroe</code>:<ul><li><code>recibir(danio)</code> resta el daño a la <code>vida</code>, <b>sin bajar de 0</b>.</li><li><code>vivo</code> es <code>true</code> mientras la vida sea mayor que 0.</li></ul>Lo comprobaremos con pruebas ocultas.',
        inicial: 'class Heroe {\n    var nombre: text\n    var vida: number = 100\n\n    constructor(nombre: text) {\n        this.nombre = nombre\n    }\n\n    function recibir(danio: number) {\n        // resta el daño a la vida (sin bajar de 0)\n    }\n\n    get vivo(): bool {\n        return true // cámbialo\n    }\n}\n\nvar h = Heroe("Ana")\nh.recibir(30)\nprint(h.nombre, h.vida, h.vivo)\n',
        solucion: 'class Heroe {\n    var nombre: text\n    var vida: number = 100\n\n    constructor(nombre: text) {\n        this.nombre = nombre\n    }\n\n    function recibir(danio: number) {\n        vida = max(0, vida - danio)\n    }\n\n    get vivo(): bool {\n        return vida > 0\n    }\n}\n\nvar h = Heroe("Ana")\nh.recibir(30)\nprint(h.nombre, h.vida, h.vivo)\n',
        pistas: ['En <code>recibir</code>: <code>vida = max(0, vida - danio)</code>.', 'En <code>vivo</code>: <code>return vida &gt; 0</code>.'],
        comprobar: {
          pruebas: 'test("un héroe nuevo", () => {\n    var h = Heroe("Ana")\n    assertEqual(h.nombre, "Ana")\n    assertEqual(h.vida, 100)\n    assert(h.vivo)\n})\ntest("recibe daño", () => {\n    var h = Heroe("Ana")\n    h.recibir(30)\n    assertEqual(h.vida, 70)\n    h.recibir(20)\n    assertEqual(h.vida, 50)\n})\ntest("la vida no baja de 0", () => {\n    var h = Heroe("Ana")\n    h.recibir(250)\n    assertEqual(h.vida, 0)\n    assert(not h.vivo)\n})',
        },
      },
      preguntas: [
        { p: '¿Cuándo se ejecuta el <code>constructor</code>?', o: ['Al llamar a un método', 'Al crear un objeto de la clase', 'Al terminar el programa', 'Nunca'], ok: 1, porque: '<code>Heroe("Ana")</code> crea el objeto y ejecuta su constructor.' },
        { p: 'Dentro de una clase, ¿qué es <code>this</code>?', o: ['La clase', 'El objeto actual', 'El programa', 'La última variable'], ok: 1, porque: '<code>this.nombre</code> es el nombre de ese objeto.' },
        { p: '¿Qué da <code>max(0, 20 - 50)</code>?', o: ['-30', '0', '20', '50'], ok: 1, porque: '20 - 50 es -30, y el mayor entre 0 y -30 es 0.' },
      ],
    },
  ];

  // ---------------------------------------------------------------------------------- examen final
  const EXAMEN = [
    { p: '¿Qué muestra <code>print("a", "b")</code>?', o: ['ab', 'a b', 'a, b', '"a" "b"'], ok: 1 },
    { p: '¿Cuál de estas líneas crea una variable que se puede cambiar?', o: ['const x = 1', 'var x = 1', 'x == 1', 'print(x = 1)'], ok: 1 },
    { p: '¿Cuánto vale <code>10 % 4</code>?', o: ['2,5', '2', '40', '6'], ok: 1 },
    { p: 'Con <code>var n = 3</code>, ¿qué muestra <code>print("n vale ${n * 2}")</code>?', o: ['n vale ${n * 2}', 'n vale 6', 'n vale n * 2', 'n vale 32'], ok: 1 },
    { p: '¿Qué da <code>"  Hola  ".trim().length</code>?', o: ['8', '4', '6', '2'], ok: 1 },
    { p: '¿Qué muestra este código? <code>var x = 7</code> · <code>if x &lt; 5 { print("A") } else if x &lt; 10 { print("B") } else { print("C") }</code>', o: ['A', 'B', 'C', 'B y C'], ok: 1 },
    { p: '¿Cuántas líneas muestra <code>for i in 2..=6 { print(i) }</code>?', o: ['4', '5', '6', '3'], ok: 1 },
    { p: '¿Cuál es el último elemento de una lista <code>l</code>?', o: ['l[0]', 'l[-1]', 'l.last', 'l[l]'], ok: 1 },
    { p: '¿Qué da <code>[3, 8, 1].map(x =&gt; x * 10)</code>?', o: ['[30, 80, 10]', '[3, 8, 1, 10]', '120', '[38, 1]'], ok: 0 },
    { p: '¿Qué hace una función sin <code>return</code>?', o: ['Da un error siempre', 'Ejecuta su código y no devuelve ningún valor', 'Devuelve 0', 'Se repite'], ok: 1 },
    { p: 'Con <code>var m = { a: 1 }</code>, ¿qué hace <code>m["b"] = 2</code>?', o: ['Da un error', 'Añade la clave b con el valor 2', 'Cambia a por b', 'Nada'], ok: 1 },
    { p: '¿Para qué sirve una clase?', o: ['Para comentar código', 'Para crear objetos con sus datos y métodos', 'Solo para hacer cuentas', 'Para repetir código'], ok: 1 },
  ];

  const NIVELES = [
    { xp: 0, nombre: 'Aprendiz' }, { xp: 300, nombre: 'Explorador' }, { xp: 700, nombre: 'Programador' },
    { xp: 1100, nombre: 'Hechicero del código' }, { xp: 1500, nombre: 'Maestro HyperEZ' },
  ];

  const INSIGNIAS = [
    { id: 'primer-programa', icono: '🚀', nombre: 'Primer programa', texto: 'Completa tu primera misión.' },
    { id: 'sin-pistas', icono: '🧠', nombre: 'Sin ayuda', texto: 'Supera 3 retos sin pedir pistas.' },
    { id: 'pleno', icono: '🎯', nombre: 'Pleno', texto: 'Acierta todas las preguntas de una misión a la primera.' },
    { id: 'mitad', icono: '⛰️', nombre: 'Mitad del camino', texto: 'Completa 5 misiones.' },
    { id: 'todas', icono: '🏰', nombre: 'Leyenda de la academia', texto: 'Completa las 10 misiones.' },
    { id: 'graduado', icono: '🎓', nombre: 'Graduado', texto: 'Aprueba el examen final y consigue tu certificado.' },
  ];

  global.HZ_ACADEMIA = { MUNDOS, MISIONES, EXAMEN, NIVELES, INSIGNIAS, APROBADO: 60 };
})(typeof window !== 'undefined' ? window : globalThis);
