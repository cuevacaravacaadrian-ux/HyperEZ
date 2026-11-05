// Prueba del módulo WebAssembly de HyperEZ (el que usa el editor web) con Node:
//   node projects/web/tests/wasm.test.js build-web/hyperez.js
// Comprueba que ejecuta programas (con importaciones), que el servidor de lenguaje da errores,
// autocompletado y ayuda, y que el formateador funciona.
'use strict';
const path = require('path');

const modPath = path.resolve(process.argv[2] || 'build-web/hyperez.js');
let ok = 0, fail = 0;
function check(name, cond, extra) {
  if (cond) { ok++; console.log('✓ ' + name); } else { fail++; console.log('✗ ' + name + (extra ? '\n    ' + extra : '')); }
}

(async () => {
  let out = '';
  const createHyperEZ = require(modPath);
  const hz = await createHyperEZ({ print: (t) => { out += t + '\n'; }, printErr: (t) => { out += t + '\n'; } });
  hz.ENV.NO_COLOR = '1';
  const run = hz.cwrap('hz_run', 'number', ['string']);
  const lsp = hz.cwrap('hz_lsp', 'string', ['string']);
  const fmt = hz.cwrap('hz_format', 'string', ['string']);

  check('versión', /^\d+\.\d+\.\d+$/.test(hz.ccall('hz_version', 'string', [], [])));

  // ---- ejecutar, con un módulo importado ----
  hz.FS.mkdirTree('/proyecto/src');
  hz.FS.writeFile('/proyecto/src/util.hez', 'function doble(x: number): number { return x * 2 }\n');
  hz.FS.writeFile('/proyecto/main.hez',
    'from "./src/util.hez" import doble\nprint("hola ${doble(21)}")\nprint("${3.14159:.2f}", [x * x for x in 1..=3])\n');
  out = '';
  let code = run('/proyecto/main.hez');
  check('ejecuta un programa con importaciones', code === 0 && out.includes('hola 42') && out.includes('3.14 [1, 4, 9]'), out);

  hz.FS.writeFile('/proyecto/mal.hez', 'prnt("x")\n');
  out = '';
  code = run('/proyecto/mal.hez');
  check('un error de compilación devuelve 1 y lo explica', code === 1 && out.includes("prnt"), out);

  hz.FS.writeFile('/proyecto/lanza.hez', 'throw Error("fallo a propósito")\n');
  out = '';
  code = run('/proyecto/lanza.hez');
  check('un error en ejecución devuelve 1', code === 1 && out.includes('fallo a propósito'), out);

  hz.FS.writeFile('/proyecto/libs.hez',
    'import validate\nimport collections\nprint(validate.dni("12345678Z"))\nvar p = collections.Stack()\np.push(7)\nprint("cima", p.pop())\n');
  out = '';
  code = run('/proyecto/libs.hez');
  check('las bibliotecas incluidas funcionan', code === 0 && out.includes('true') && out.includes('cima 7'), out);

  // ---- servidor de lenguaje ----
  const send = (msg) => JSON.parse(lsp(JSON.stringify(Object.assign({ jsonrpc: '2.0' }, msg))));
  const init = send({ id: 1, method: 'initialize', params: { capabilities: {} } });
  check('initialize', init.length === 1 && init[0].result && init[0].result.capabilities, JSON.stringify(init).slice(0, 300));
  send({ method: 'initialized', params: {} });
  const uri = 'file:///proyecto/demo.hez';
  const text = 'class Objeto {\n    var nombre: text\n    var precio: number\n}\nvar o = Objeto()\nprnt(o.nombre)\no.\n';
  const opened = send({ method: 'textDocument/didOpen', params: { textDocument: { uri, languageId: 'hyperez', version: 1, text } } });
  const diag = opened.find((m) => m.method === 'textDocument/publishDiagnostics');
  check('errores en vivo', diag && diag.params.diagnostics.some((d) => d.message.includes('prnt')), JSON.stringify(opened).slice(0, 400));
  const comp = send({ id: 2, method: 'textDocument/completion', params: { textDocument: { uri }, position: { line: 6, character: 2 } } });
  const items = comp[0] && comp[0].result ? (comp[0].result.items || comp[0].result) : [];
  check('autocompletado de los campos de una clase', items.some((i) => i.label === 'nombre') && items.some((i) => i.label === 'precio'),
    JSON.stringify(items.map((i) => i.label)).slice(0, 300));
  const hover = send({ id: 3, method: 'textDocument/hover', params: { textDocument: { uri }, position: { line: 4, character: 9 } } });
  check('ayuda al pasar el ratón', hover[0] && hover[0].result && JSON.stringify(hover[0].result).includes('Objeto'), JSON.stringify(hover).slice(0, 300));

  // ---- formateador ----
  const f = fmt('if true {\nprint(1)\n}\n');
  check('formato', f === 'if true {\n    print(1)\n}\n', JSON.stringify(f));
  check('no formatea un texto sin cerrar', fmt('print("hola\n').charCodeAt(0) === 1);

  console.log(`\n${ok} comprobaciones correctas, ${fail} fallidas`);
  process.exit(fail ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(1); });
