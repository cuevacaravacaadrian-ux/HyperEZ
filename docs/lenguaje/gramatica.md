# Gramática

La sintaxis completa de HyperEZ en notación EBNF. `{ x }` significa "cero o más veces", `[ x ]` "opcional" y `|` "o". Los textos entre comillas son literales. `NL` es un salto de línea (o `;`), que termina las sentencias.

Es una referencia para quien quiera escribir herramientas o entender el parser (`src/lang/syntax/parser.cpp`); para aprender, es mejor la [referencia por temas](README.md).

## Programa y sentencias

```ebnf
programa      = { sentencia NL } ;
bloque        = "{" { sentencia NL } "}" ;

sentencia     = declaracion | if | while | for | foreach | repeat | try | match
              | "return" [ expresion ] | "break" | "continue" | "throw" expresion
              | emit | on | expresion ;

declaracion   = [ visibilidad ] ( variable | funcion | clase | interfaz | enum | evento )
              | importacion ;
visibilidad   = "private" | "public" | "protected" ;
```

## Declaraciones

```ebnf
variable      = ( "var" | "let" | "const" ) ( NOMBRE [ ":" tipo ] [ "=" expresion ]
                                            | patron "=" expresion ) ;
patron        = "[" [ elemento { "," elemento } ] "]"
              | "{" [ campo { "," campo } ] "}" ;
elemento      = [ "..." ] NOMBRE ;
campo         = NOMBRE [ ":" NOMBRE ] ;

funcion       = [ "async" ] "function" NOMBRE parametros [ ":" tipo ] bloque ;
parametros    = "(" [ parametro { "," parametro } ] ")" ;
parametro     = [ "..." ] NOMBRE [ ":" tipo ] [ "=" expresion ] ;

clase         = ( "class" | "struct" ) NOMBRE [ "extends" NOMBRE ] [ "implements" NOMBRE { "," NOMBRE } ]
                "{" { miembro NL } "}" ;
miembro       = [ visibilidad ] [ "static" ] ( variable | metodo | acceso | constructor ) ;
metodo        = [ "async" ] "function" NOMBRE parametros [ ":" tipo ] bloque ;
acceso        = ( "get" NOMBRE "(" ")" | "set" NOMBRE "(" parametro ")" ) [ ":" tipo ] bloque ;
constructor   = "constructor" parametros bloque ;

interfaz      = "interface" NOMBRE [ "extends" NOMBRE { "," NOMBRE } ]
                "{" { "function" NOMBRE parametros [ ":" tipo ] NL } "}" ;
enum          = "enum" NOMBRE "{" [ valorEnum { "," valorEnum } ] "}" ;
valorEnum     = NOMBRE [ "=" ( NUMERO | TEXTO ) ] ;
evento        = "event" NOMBRE parametros ;

importacion   = "import" ( TEXTO | ruta ) [ "as" NOMBRE ]
              | "from" ( TEXTO | ruta ) "import" NOMBRE [ "as" NOMBRE ] { "," NOMBRE [ "as" NOMBRE ] } ;
ruta          = NOMBRE { "." NOMBRE } ;

tipo          = ( NOMBRE [ "<" tipo { "," tipo } ">" ] | "(" [ tipo { "," tipo } ] ")" "=>" tipo ) [ "?" ] ;
```

## Control del flujo

```ebnf
if            = "if" expresion bloque [ "else" ( if | bloque ) ] ;
while         = "while" expresion bloque ;
for           = "for" NOMBRE [ "," NOMBRE ] "in" expresion [ "step" expresion ] bloque ;
foreach       = "foreach" NOMBRE [ "," NOMBRE ] "in" expresion bloque ;
repeat        = "repeat" expresion bloque ;
try           = "try" bloque [ "catch" [ NOMBRE ] bloque ] [ "finally" bloque ] ;

match         = "match" expresion "{" { caso NL } "}" ;
caso          = ( patrones [ "if" expresion ] | "else" [ "if" expresion ] ) "=>" ( expresion | bloque ) ;
patrones      = expresion { "," expresion } ;

emit          = "emit" expresion argumentos ;
on            = ( "on" | "once" ) expresion [ "priority" expresion ] [ "as" NOMBRE ] bloque ;
```

## Expresiones (de menor a mayor precedencia)

```ebnf
expresion     = asignacion ;
asignacion    = condicional [ ( "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "??=" ) asignacion ] ;
condicional   = nulo [ "?" expresion ":" expresion ] ;
nulo          = o { "??" o } ;
o             = y { "or" y } ;
y             = no { "and" no } ;
no            = "not" no | igualdad ;
igualdad      = comparacion { ( "==" | "!=" ) comparacion } ;
comparacion   = bitOr { ( "<" | "<=" | ">" | ">=" | "in" | "not" "in" ) bitOr | "is" [ "not" ] tipo } ;
bitOr         = bitXor { "|" bitXor } ;
bitXor        = bitAnd { "^" bitAnd } ;
bitAnd        = desplaz { "&" desplaz } ;
desplaz       = rango { ( "<<" | ">>" ) rango } ;
rango         = suma [ ( ".." | "..=" ) suma ] ;
suma          = producto { ( "+" | "-" ) producto } ;
producto      = unario { ( "*" | "/" | "%" ) unario } ;
unario        = ( "-" | "+" | "!" | "~" | "++" | "--" | "await" ) unario | potencia ;
potencia      = postfijo [ "**" unario ] ;
postfijo      = primario { "." NOMBRE | "?." NOMBRE | "[" expresion "]" | "?.[" expresion "]"
                         | argumentos | "++" | "--" } ;
argumentos    = "(" [ argumento { "," argumento } ] ")" ;
argumento     = [ NOMBRE ":" ] expresion | "..." expresion ;

primario      = NUMERO | TEXTO | "true" | "false" | "null" | "this" | "super" "." NOMBRE
              | NOMBRE | "(" expresion ")" | lista | mapa | lambda | match ;
lista         = "[" [ [ "..." ] expresion { "," [ "..." ] expresion } [ "," ] ] "]" ;
mapa          = "{" [ clave ":" expresion { "," clave ":" expresion } [ "," ] ] "}" ;
clave         = NOMBRE | TEXTO | "[" expresion "]" ;
lambda        = [ "async" ] ( NOMBRE | parametros [ ":" tipo ] ) "=>" ( expresion | bloque ) ;
```

## Elementos léxicos

```ebnf
NOMBRE        = letra { letra | digito } ;          (* letra incluye "_", acentos y ñ *)
NUMERO        = digitos [ "." digitos ] [ ( "e" | "E" ) [ "+" | "-" ] digitos ]
              | "0x" hex { hex } | "0b" ( "0" | "1" ) { "0" | "1" } | "0o" octal { octal } ;
digitos       = digito { digito | "_" } ;
TEXTO         = '"' { caracter | escape | "${" expresion "}" } '"'
              | "'" { caracter | escape | "${" expresion "}" } "'"
              | '"""' { cualquiera } '"""'
              | "r" '"' { cualquiera } '"' | "r" "'" { cualquiera } "'" ;
escape        = "\n" | "\t" | "\r" | "\0" | "\\" | '\"' | "\'" | "\$" | "\u{" hex { hex } "}" ;
comentario    = "//" { cualquiera } fin_de_linea | "/*" { cualquiera } "*/" ;
```
