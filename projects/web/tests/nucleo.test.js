// Pruebas del núcleo del editor web:  node projects/web/tests/nucleo.test.js
'use strict';
const N = require('../src/editor/nucleo.js');

let ok = 0, fail = 0;
function eq(name, got, want) {
  const g = JSON.stringify(got), w = JSON.stringify(want);
  if (g === w) { ok++; console.log('✓ ' + name); } else { fail++; console.log('✗ ' + name + '\n    obtenido: ' + g + '\n    esperado: ' + w); }
}
const kinds = (src) => N.tokenize(src).sort((a, b) => a.s - b.s).map((t) => [src.slice(t.s, t.e), t.t]);
// Aplica un cambio de autoPair/enterIndent y devuelve el texto con | donde queda el cursor
function apply(text, a, b, ch) {
  if (!ch) return null;
  const t = text.slice(0, ch.from) + ch.insert + text.slice(ch.to);
  return t.slice(0, ch.selStart) + '|' + t.slice(ch.selStart);
}
function key(textWithCaret, k) {
  const a = textWithCaret.indexOf('|');
  const text = textWithCaret.replace('|', '');
  return apply(text, a, a, N.autoPair(text, a, a, k));
}

// ---- colores ----
eq('palabras clave, nombres y números', kinds('var x = 42\nif x > 1 { print(x) }'),
  [['var', 'kw'], ['42', 'num'], ['if', 'kw'], ['1', 'num'], ['print', 'builtin']]);
eq('comentarios', kinds('// hola\n/// doc\n/* a */ x'), [['// hola', 'com'], ['/// doc', 'doc'], ['/* a */', 'com']]);
eq('textos con interpolación y formato', kinds('"a ${precio:.2f} b"'),
  [['"a ', 'str'], ['${', 'interp'], [':.2f', 'fmt'], ['}', 'interp'], [' b"', 'str']]);
eq('tipos y clases', kinds('var n: number = Jugador()'), [['var', 'kw'], ['number', 'type'], ['Jugador', 'type']]);
eq('métodos y propiedades', kinds('lista.add(1).length'), [['add', 'fn'], ['1', 'num'], ['length', 'prop']]);
eq('do solo es palabra clave delante de {', kinds('do { } while x\nvar do = 1'), [['do', 'kw'], ['while', 'kw'], ['var', 'kw'], ['1', 'num']]);
eq('escapes y textos r"..."', kinds('"a\\n" r"\\d+"'), [['"a', 'str'], ['\\n', 'esc'], ['"', 'str'], ['r"\\d+"', 'str']]);
eq('true, null y this', kinds('x = true ?? null'), [['true', 'const'], ['null', 'const']]);
eq('el formato no confunde la condición', N.splitFormatSpec('ok ? 1 : 2'), null);
eq('formato con ancho y relleno', N.splitFormatSpec('nombre:*^10'), '*^10');
eq('HTML escapado y subrayado del error', N.highlightHtml('prnt(1 < 2)', [{ s: 0, e: 4, sev: 1 }]),
  '<span class="t-fn err">prnt</span>(<span class="t-num">1</span> &lt; <span class="t-num">2</span>)\n');

// ---- parejas ----
eq('( cierra solo', key('print|', '('), 'print(|)');
eq('" cierra solo', key('print(|)', '"'), 'print("|")');
eq('salta el cierre ya escrito', key('print("hola"|)', ')'), 'print("hola")|');
eq('salta la comilla que cierra', key('print("hola|")', '"'), 'print("hola"|)');
eq('no cierra la comilla pegada a una palabra', key('l|', "'"), null);
eq('no cierra delante de una palabra', key('|x', '('), null);
eq('borra la pareja vacía', key('f(|)', 'Backspace'), 'f|');
eq('rodea la selección', (() => { const t = 'hola'; return apply(t, 0, 4, N.autoPair(t, 0, 4, '"')); })(), '"|hola"');

// ---- sangría ----
eq('Intro mantiene la sangría', (() => { const t = '    x = 1'; return apply(t, 9, 9, N.enterIndent(t, 9, 9)); })(), '    x = 1\n    |');
eq('Intro tras { añade un nivel', (() => { const t = 'if x {'; return apply(t, 6, 6, N.enterIndent(t, 6, 6)); })(), 'if x {\n    |');
eq('Intro entre {} deja el cierre debajo', (() => { const t = 'if x {}'; return apply(t, 6, 6, N.enterIndent(t, 6, 6)); })(), 'if x {\n    |\n}');
eq('} al principio quita un nivel', (() => { const t = 'if x {\n    a\n    '; const a = t.length; return apply(t, a, a, N.outdentOnClose(t, a, '}')); })(), 'if x {\n    a\n}|');
eq('comentar líneas', N.toggleComment('a\n  b', 0, 5).insert, '// a\n//   b');
eq('descomentar líneas', N.toggleComment('// a\n// b', 0, 9).insert, 'a\nb');
eq('Tab con varias líneas', N.indentLines('a\nb', 0, 3, false).insert, '    a\n    b');
eq('Mayús+Tab', N.indentLines('    a\n  b', 0, 9, true).insert, 'a\nb');
eq('pareja de llaves', N.matchBracket('f(a, [1, "(", 2])', 1), [1, 16]);
eq('pareja hacia atrás', N.matchBracket('{ x }', 5), [0, 4]);
eq('palabra antes del cursor', N.wordBefore('var jugador.vi', 14), { start: 12, word: 'vi' });

// ---- posiciones ----
const st = N.lineStarts('ab\ncde\nf');
eq('posición a línea y carácter', N.offsetToPos(st, 5), { line: 1, character: 2 });
eq('línea y carácter a posición', N.posToOffset(st, { line: 2, character: 0 }, 8), 7);

// ---- explorador ----
const fs = new N.VFS();
fs.write('/main.hez', 'print(1)');
fs.write('/src/util.hez', 'x');
fs.mkdir('/datos');
eq('lista: carpetas primero', fs.list('/').map((k) => k.path), ['/datos', '/src', '/main.hez']);
eq('renombrar una carpeta mueve lo de dentro', fs.move('/src', '/lib'), [['/src/util.hez', '/lib/util.hez']]);
eq('el archivo movido conserva el contenido', fs.read('/lib/util.hez'), 'x');
eq('nombre libre', fs.freeName('/', 'main', '.hez'), 'main 2.hez');
fs.remove('/lib');
eq('borrar una carpeta borra lo de dentro', [fs.exists('/lib'), fs.exists('/lib/util.hez')], [false, false]);
eq('no se mueve una carpeta dentro de sí misma', (() => { fs.mkdir('/a/b'); try { fs.move('/a', '/a/b/c'); return 'movida'; } catch (e) { return 'error'; } })(), 'error');
eq('guardar y cargar', new N.VFS(JSON.parse(JSON.stringify(fs.toJSON()))).list('/').map((k) => k.path), fs.list('/').map((k) => k.path));
eq('rutas normalizadas', N.VFS.norm('a//b/../c/'), '/a/c');

// ---- zip ----
const zip = N.makeZip([{ name: 'main.hez', data: 'print("hola")\n' }, { name: 'src/a.hez', data: 'x' }]);
eq('zip: firma y número de archivos', [zip[0], zip[1], zip[zip.length - 22], zip[zip.length - 12]], [0x50, 0x4b, 0x50, 2]);
eq('crc32 conocido', N.crc32(new TextEncoder().encode('123456789')).toString(16), 'cbf43926');

console.log(`\n${ok} comprobaciones correctas, ${fail} fallidas`);
process.exit(fail ? 1 : 0);
