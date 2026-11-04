# Códigos de error y aviso

Cada mensaje del compilador lleva un código (`error[E0205]`, `aviso[W0303]`). Aquí están todos, agrupados por tipo.
Donde pone «…» va el nombre o el valor concreto de tu programa.

> Este archivo se genera con `python tools/gen_docs.py`: no lo edites a mano.

## Léxico: caracteres, números y textos mal escritos

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `E0001` | Comentario de bloque sin cerrar<br>No se puede leer el archivo '…' | añade */ al final del comentario |
| `E0002` | Número mal formado<br>…Dígito '…' no válido en este número<br>Un número no puede ir seguido de letras | separa el número del identificador con un espacio u operador |
| `E0003` | Escape \u{...} sin cerrar<br>Escape \u sin dígitos hexadecimales<br>…Secuencia de escape desconocida '\…' | escapes válidos: \n \t \r \0 \\ \" \' \$ \u{XXXX} |
| `E0004` | Interpolación ${...} sin cerrar<br>Interpolación vacía ${} | añade '}' |
| `E0005` | Texto sin cerrar | …falta la comilla de cierre … |
| `E0006` | Carácter inesperado '…' |  |

## Sintaxis: la estructura del código

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `E0100` | … |  |
| `E0101` | La constante '…' necesita un valor inicial | const … = valor |
| `E0102` | El parámetro '…' necesita un valor por defecto porque va después de otro que lo tiene<br>Un parámetro '...' no puede tener valor por defecto<br>El parámetro '...' debe ser el último |  |
| `E0103` | Solo puede haber un '...' y debe ir al final<br>El '...' debe ser el último de la lista<br>La desestructuración necesita al menos un nombre |  |
| `E0104` | 'step' solo se puede usar con rangos (a..b) |  |
| `E0105` | La clase '…' ya tiene un constructor |  |
| `E0106` | No se puede asignar a esta expresión | solo se puede asignar a variables, propiedades (obj.x) o índices (lista[i]) |
| `E0107` | Los argumentos posicionales deben ir antes que los argumentos con nombre |  |

## Nombres y declaraciones

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `E0201` | Ya existe una variable llamada '…' en este bloque | usa otro nombre o asigna sin 'var' |
| `E0202` | No se puede usar el miembro de instancia '…' en un método estático | los métodos 'static' no tienen 'this' |
| `E0203` | La variable '…' se usa antes de declararla (línea …) | mueve la declaración 'var …' más arriba |
| `E0204` | '…' ya está declarado… en la línea … | cada nombre global debe ser único en el archivo |
| `E0205` | No existe ninguna variable llamada '…'<br>No existe nada llamado '…' | …declárala primero con: var … = ...…¿quisiste decir '…'? Para crearla usa: var … = ... |
| `E0206` | …No se encontró el módulo '…'… |  |
| `E0207` | El módulo '…' no exporta '…' |  |
| `E0208` | Valor repetido '…' en el enum |  |
| `E0209` | El valor de un enum debe ser un número o un texto literal |  |
| `E0210` | 'import' solo puede usarse en el nivel superior del archivo |  |
| `E0212` | No se puede reasignar la constante '…'<br>El campo '…' es constante | usa 'var' si necesitas cambiar su valor |
| `E0213` | '…' se declaró con 'let' y solo puede asignarse una vez | usa 'var' si necesitas cambiar su valor |
| `E0214` | No se puede asignar a '…' (es …una función…un módulo…un tipo)<br>No se puede reasignar el integrado '…'<br>No se puede asignar al método '…' |  |
| `E0215` | '….…' es de solo lectura<br>'…' es de solo lectura<br>La propiedad '…' solo tiene 'get' (es de solo lectura) | añade: set …(valor) { ... } |
| `E0216` | Los textos no se pueden modificar por índice | crea un texto nuevo, p. ej. con replace() |
| `E0217` | '…' se usa antes de asignarle un valor |  |
| `E0218` | 'this' solo puede usarse dentro de los métodos de una clase |  |
| `E0219` | 'await' solo puede usarse dentro de funciones 'async' | …declara la función como: async function …(...)…marca la función como async |

## Tipos y expresiones

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `E0301` | Tipo desconocido '…' | …¿quisiste decir '…'? |
| `E0302` | Tipo incorrecto en …: se esperaba … pero es … |  |
| `E0303` | Esta expresión no devuelve ningún valor (void) |  |
| `E0304` | '…' es int pero el valor tiene decimales | usa number o round(...) |
| `E0305` | Asignación dentro de una condición | para comparar usa '==' |
| `E0306` | Los límites de un rango deben ser números, no …<br>repeat necesita un número, no …<br>Los límites de un rango deben ser números |  |
| `E0307` | No se puede recorrer un valor de tipo … | …para repetir N veces usa: for i in 0..n { }… |
| `E0308` | 'break' fuera de un bucle<br>'continue' fuera de un bucle |  |
| `E0309` | 'return' fuera de una función |  |
| `E0310` | Un constructor no puede devolver un valor<br>La función '…' es void y no puede devolver un valor<br>El tipo … no tiene '…' | quita el valor o cambia el tipo de retorno<br>…¿quisiste decir '…'? |
| `E0311` | La función '…' debe devolver un valor de tipo …<br>No se puede acceder a '…' porque el valor es null | usa ?. para acceso seguro: obj?.… |
| `E0312` | Un valor de enum solo tiene 'name', 'value' y 'ordinal' |  |
| `E0313` | El enum '…' no tiene el valor '…' | …¿quisiste decir '…'? |
| `E0314` | '…' no tiene ningún miembro llamado '…' | …¿quisiste decir '…'? |
| `E0315` | … no tiene ningún miembro llamado '…' | …¿quisiste decir '…'? |
| `E0316` | '…' no tiene ningún miembro estático llamado '…' | …¿quisiste decir '…'? |
| `E0317` | '…' es un miembro de instancia; necesitas un objeto de '…' | crea uno con …(...) |
| `E0318` | '…' es …privado…protegido en '…' |  |
| `E0320` | …El operador '…-…~…+' necesita un número, no …<br>…'…++…--' necesita un número y '…' es … |  |
| `E0321` | …No se puede … … y …<br>No se puede usar '+=' con … y …<br>Este operador necesita números (…, …) | …para convertir texto a número usa toNumber(...)… |
| `E0322` | No se puede comparar … con … |  |
| `E0323` | 'in' necesita una lista, mapa, conjunto, texto o rango a la derecha |  |
| `E0324` | Los operadores de bits necesitan números | para lógica usa 'and' / 'or' |
| `E0325` | El índice de una lista debe ser un número, no …<br>No se puede indexar un valor de tipo … |  |
| `E0326` | No se pueden declarar variables dentro de un caso de 'match' usado en medio de una expresión | guarda el resultado en una variable: var r = match x { ... } |

## Clases, objetos y miembros

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `E0401` | 'emit' necesita un evento, pero esto es … | declara el evento con: event Nombre(param: tipo) |
| `E0402` | Demasiados argumentos para …: acepta … y se pasaron …<br>La clase '…' no tiene constructor y no acepta argumentos | firma: …<br>añade: constructor(...) { ... } |
| `E0403` | … no tiene ningún parámetro llamado '…' | …firma: …¿quisiste decir '…'? |
| `E0404` | El parámetro '…' ya recibió un valor |  |
| `E0405` | Faltan argumentos para …: … | firma: … |
| `E0406` | No se puede llamar a null<br>Un módulo no es una función<br>… no es una función (es …) |  |
| `E0407` | Los eventos no se llaman: usa 'emit …Evento(...)' |  |
| `E0408` | 'on' necesita un evento, pero esto es … | declara el evento con: event Nombre(param: tipo) |
| `E0409` | Demasiados argumentos (máximo 255) |  |

## Funciones, módulos y control de flujo

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `E0501` | La clase base '…' no existe |  |
| `E0502` | '…' es una interfaz: usa 'implements', no 'extends' |  |
| `E0503` | No se puede heredar de la clase integrada '…' |  |
| `E0504` | Herencia circular: '…' no puede heredar de sí misma |  |
| `E0505` | '…' no es una interfaz conocida |  |
| `E0506` | El miembro '…' está repetido |  |
| `E0507` | La clase '…' no implementa '…' de la interfaz '…' | añade: function …() { ... } |
| `E0508` | Las clases solo pueden declararse en el nivel superior del archivo |  |
| `E0509` | super(...) solo puede llamarse dentro de un constructor |  |
| `E0510` | La clase '…' no hereda de ninguna otra<br>'super' solo puede usarse en clases que heredan de otra<br>'super' no está disponible aquí |  |
| `E0511` | No se puede crear un objeto de la interfaz '…' |  |

## Límites del compilador

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `E0900` | Función demasiado grande (más de 65535 constantes o saltos)<br>Salto demasiado largo en el bytecode<br>Bucle demasiado largo | divide el código en funciones más pequeñas |
| `E0999` | Error interno del compilador: desajuste de pila (slot … / …) | por favor, reporta este fallo con el código que lo produce |

## Avisos (el programa se ejecuta igualmente)

| Código | Mensaje | Cómo arreglarlo |
|---|---|---|
| `W0301` | Código inalcanzable: nunca se ejecuta | está después de un return/throw/break/continue |
| `W0302` | La función '…' declara que devuelve … pero puede terminar sin 'return' | añade 'return valor' al final |
| `W0303` | Esta comparación no tiene efecto<br>Esta expresión no tiene efecto | ¿querías asignar con '=' en vez de comparar con '=='? |
| `W0304` | La clase '…' no declara '…' | …decláralo en la clase: var … ... o function …() { ... }…¿quisiste decir '…'? |
| `W0305` | '…' es estático; accede con ….… |  |
| `W0306` | Comparar … con … siempre da …false…true |  |
