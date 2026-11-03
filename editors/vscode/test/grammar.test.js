// Prueba de la gramática: tokeniza código HyperEZ con el mismo motor que VS Code
// (vscode-textmate + Oniguruma) y comprueba los ámbitos de los tokens clave.
//   node test/grammar.test.js
'use strict';
const fs = require('fs');
const path = require('path');
const vsctm = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

const root = path.join(__dirname, '..');
const wasm = fs.readFileSync(path.join(root, 'node_modules', 'vscode-oniguruma', 'release', 'onig.wasm')).buffer;

const registry = new vsctm.Registry({
  onigLib: oniguruma.loadWASM(wasm).then(() => ({
    createOnigScanner: (s) => new oniguruma.OnigScanner(s),
    createOnigString: (s) => new oniguruma.OnigString(s),
  })),
  loadGrammar: async (scope) => {
    if (scope !== 'source.hyperez') return null;
    const file = path.join(root, 'syntaxes', 'hyperez.tmLanguage.json');
    return vsctm.parseRawGrammar(fs.readFileSync(file, 'utf8'), file);
  },
});

// [línea, texto del token, fragmento de ámbito que debe tener]
const cases = [
  ['/// Documentación', ' Documentación', 'comment.line.triple-slash.documentation'],
  ['// comentario', ' comentario', 'comment.line.double-slash'],
  ['var vida: number = 100', 'var', 'storage.type.var'],
  ['var vida: number = 100', 'vida', 'variable.other.readwrite'],
  ['var vida: number = 100', 'number', 'support.type.primitive'],
  ['var vida: number = 100', '100', 'constant.numeric.decimal'],
  ['const MAX_VIDA = 0xFF_FF', 'MAX_VIDA', 'variable.other.constant'],
  ['const MAX_VIDA = 0xFF_FF', '0xFF_FF', 'constant.numeric.hex'],
  ['async function cargar(nombre: text, ...resto: any): Task {', 'async', 'storage.modifier.async'],
  ['async function cargar(nombre: text, ...resto: any): Task {', 'cargar', 'entity.name.function'],
  ['async function cargar(nombre: text, ...resto: any): Task {', 'nombre', 'variable.parameter'],
  ['async function cargar(nombre: text, ...resto: any): Task {', 'resto', 'variable.parameter'],
  ['async function cargar(nombre: text, ...resto: any): Task {', 'Task', 'support.type.primitive'],
  ['class Jefe extends Actor implements Danable {', 'Jefe', 'entity.name.type.class'],
  ['class Jefe extends Actor implements Danable {', 'Actor', 'entity.other.inherited-class'],
  ['class Jefe extends Actor implements Danable {', 'Danable', 'entity.other.inherited-class'],
  ['print("¡Hola, ${nombre}!")', 'print', 'support.function.builtin'],
  ['print("¡Hola, ${nombre}!")', '¡Hola, ', 'string.quoted.double'],
  ['print("¡Hola, ${nombre}!")', '${', 'punctuation.section.interpolation.begin'],
  ['print("¡Hola, ${nombre}!")', 'nombre', 'meta.interpolation'],
  ['print("total: ${ {a: 1}.a }")', '}', 'meta.interpolation'],
  ['for i in 10..0 step -2 {', 'step', 'keyword.control.loop'],
  ['for i in 10..0 step -2 {', '..', 'keyword.operator.range'],
  ['on Golpe priority 10 as e {', 'on', 'keyword.control.event'],
  ['on Golpe priority 10 as e {', 'Golpe', 'entity.name.function.event'],
  ['on Golpe priority 10 as e {', 'priority', 'keyword.control.event'],
  ['emit Golpe("Ana", 5)', 'emit', 'keyword.control.event'],
  ['event Golpe(quien: text)', 'event', 'storage.type.event'],
  ['get vivo(): bool { return vida > 0 }', 'get', 'storage.type.property'],
  ['get vivo(): bool { return vida > 0 }', 'vivo', 'entity.name.function.accessor'],
  ['jugador.mover(Direccion.Arriba, 10)', 'mover', 'entity.name.function.member'],
  ['jugador.mover(Direccion.Arriba, 10)', 'Arriba', 'variable.other.property'],
  ['Console.print("x", "red")', 'Console', 'support.class.builtin'],
  ['var c = Color.RED', 'RED', 'variable.other.constant.property'],
  ['if a and not b or c is Jefe { }', 'and', 'keyword.operator.word'],
  ['if a and not b or c is Jefe { }', 'Jefe', 'entity.name.type'],
  ['var x = m?.nada?.x ?? "por defecto"', '??', 'keyword.operator.nullish'],
  ['var f = x => x * 2', '=>', 'storage.type.function.arrow'],
  ['from "./jugador.hz" import Jugador, Direccion', 'from', 'keyword.control.import'],
  ['from "./jugador.hz" import Jugador, Direccion', '"./jugador.hz"', 'entity.name.namespace'],
  ['var t = """varias', '"""', 'string.quoted.triple.double'],
  ['try { } catch e { print(e.message) } finally { }', 'catch', 'keyword.control.trycatch'],
  ['try { } catch e { print(e.message) } finally { }', 'e', 'variable.parameter'],
  ['var r = await wait(0.5)', 'await', 'keyword.control.flow'],
  ['var b = true', 'true', 'constant.language.boolean'],
  ['this.vida = null', 'this', 'variable.language.this'],
  ['this.vida = null', 'null', 'constant.language.null'],
  ['var a = PI * 2', 'PI', 'support.constant.math'],
];

(async () => {
  const grammar = await registry.loadGrammar('source.hyperez');
  let failed = 0;
  for (const [line, token, scope] of cases) {
    const r = grammar.tokenizeLine(line, vsctm.INITIAL);
    const toks = r.tokens.map((t) => ({ text: line.substring(t.startIndex, t.endIndex), scopes: t.scopes }));
    const hit = toks.find((t) => t.text === token && t.scopes.some((s) => s.includes(scope)));
    if (!hit) {
      failed++;
      const found = toks.filter((t) => t.text.includes(token) || token.includes(t.text.trim()) && t.text.trim());
      console.log('✗ ' + JSON.stringify(token) + ' en ' + JSON.stringify(line) + ' debería ser ' + scope);
      for (const t of found) console.log('    ' + JSON.stringify(t.text) + ' -> ' + t.scopes.slice(1).join(' '));
    }
  }
  // Una línea con texto multilínea: el estado debe continuar a la siguiente
  const l1 = grammar.tokenizeLine('var t = """primera', vsctm.INITIAL);
  const l2 = grammar.tokenizeLine('segunda """ + x', l1.ruleStack);
  const seg = l2.tokens.find((t) => 'segunda """ + x'.substring(t.startIndex, t.endIndex).startsWith('segunda'));
  if (!seg || !seg.scopes.some((s) => s.includes('string.quoted.triple'))) { failed++; console.log('✗ el texto triple no continúa en la línea siguiente'); }
  console.log(failed ? `\n${failed} de ${cases.length + 1} comprobaciones fallaron` : `✓ ${cases.length + 1} comprobaciones de la gramática correctas`);
  process.exit(failed ? 1 : 0);
})();
