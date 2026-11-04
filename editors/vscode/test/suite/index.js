// Prueba de integración: se ejecuta dentro de VS Code (ver test/runTest.js).
'use strict';
const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const os = require('os');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function waitFor(fn, ms, what) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const v = await fn();
    if (v) return v;
    await sleep(200);
  }
  throw new Error('Tiempo agotado esperando: ' + what);
}

async function run() {
  const results = [];
  const check = (name, ok, detail) => results.push({ name, ok: !!ok, detail: detail || '' });
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'hz-vscode-'));
  fs.writeFileSync(path.join(dir, 'modulo.hz'), '/// Suma dos números.\nfunction sumar(a: number, b: number): number { return a + b }\n');
  const file = path.join(dir, 'main.hz');
  fs.writeFileSync(file, [
    'from "./modulo.hz" import sumar',
    'class Jugador {',
    '    var nombre: text',
    '    var vida = 3',
    '    constructor(nombre: text) { this.nombre = nombre }',
    '}',
    'var j = Jugador("Ana")',
    'pirnt(sumar(1, 2))',
    'Console.',
    '',
  ].join('\n'));

  const pkg = require('../../package.json');
  const ext = vscode.extensions.getExtension(`${pkg.publisher}.${pkg.name}`);
  check('la extensión está instalada', ext);
  const doc = await vscode.workspace.openTextDocument(file);
  await vscode.window.showTextDocument(doc);
  check('lenguaje detectado como hyperez', doc.languageId === 'hyperez', doc.languageId);
  await waitFor(() => ext.isActive, 15000, 'activación');
  check('la extensión se activa', ext.isActive);

  const diags = await waitFor(() => {
    const d = vscode.languages.getDiagnostics(doc.uri);
    return d.some((x) => x.message.includes('pirnt')) ? d : null;
  }, 20000, 'diagnósticos');
  const pirnt = diags.find((x) => x.message.includes('pirnt'));
  check('diagnóstico de nombre desconocido', pirnt, pirnt && pirnt.message.split('\n')[0]);
  check('el diagnóstico está en la línea 8', pirnt && pirnt.range.start.line === 7, pirnt && JSON.stringify(pirnt.range.start));

  const actions = await vscode.commands.executeCommand('vscode.executeCodeActionProvider', doc.uri, pirnt.range);
  check('arreglo rápido pirnt -> print', actions && actions.some((a) => a.title.includes("'print'")), actions && actions.map((a) => a.title).join(', '));

  const comp = await vscode.commands.executeCommand('vscode.executeCompletionItemProvider', doc.uri, new vscode.Position(8, 8), '.');
  const labels = comp.items.map((i) => (typeof i.label === 'string' ? i.label : i.label.label));
  check('autocompletado de Console.', labels.includes('readKey') && labels.includes('clear'), labels.slice(0, 6).join(', '));

  const hover = await vscode.commands.executeCommand('vscode.executeHoverProvider', doc.uri, new vscode.Position(7, 8));
  const htext = hover.map((h) => h.contents.map((c) => c.value || c).join(' ')).join(' ');
  check('hover con la documentación de otro archivo', htext.includes('Suma dos números'), htext.slice(0, 80));

  const defs = await vscode.commands.executeCommand('vscode.executeDefinitionProvider', doc.uri, new vscode.Position(7, 8));
  check('ir a la definición en modulo.hz', defs.length && (defs[0].uri || defs[0].targetUri).fsPath.endsWith('modulo.hz'));

  const sig = await vscode.commands.executeCommand('vscode.executeSignatureHelpProvider', doc.uri, new vscode.Position(7, 15), '(');
  check('ayuda de parámetros', sig && sig.signatures.length && sig.signatures[0].label.includes('a: number'), sig && sig.signatures[0] && sig.signatures[0].label);

  const syms = await vscode.commands.executeCommand('vscode.executeDocumentSymbolProvider', doc.uri);
  const names = (syms || []).map((s) => s.name);
  check('esquema del documento', names.includes('Jugador') && names.includes('j'), names.join(', '));

  const edit = await vscode.commands.executeCommand('vscode.executeDocumentRenameProvider', doc.uri, new vscode.Position(6, 4), 'ana');
  check('renombrar variable', edit && edit.size >= 1);

  const colors = await vscode.commands.executeCommand('vscode.executeDocumentColorProvider', doc.uri);
  check('proveedor de colores registrado', Array.isArray(colors));

  const lenses = await vscode.commands.executeCommand('vscode.executeCodeLensProvider', doc.uri);
  const lensTitles = (lenses || []).map((l) => l.command && l.command.title);
  check('botones ▶ Ejecutar / ⟳ / ✓ encima del código', lensTitles.includes('▶ Ejecutar') && lensTitles.includes('✓ Comprobar'), lensTitles.join(', '));

  // Formato con el servidor de lenguaje (hyper fmt)
  const messy = path.join(dir, 'desordenado.hez');
  fs.writeFileSync(messy, 'if true {\nprint(1)   \n}\n');
  const mdoc = await vscode.workspace.openTextDocument(messy);
  check('los .hez también son hyperez', mdoc.languageId === 'hyperez', mdoc.languageId);
  const fmtEdits = await waitFor(async () => {
    const e = await vscode.commands.executeCommand('vscode.executeFormatDocumentProvider', mdoc.uri, { tabSize: 4, insertSpaces: true });
    return e && e.length ? e : null;
  }, 15000, 'formato');
  // VS Code reduce el cambio a lo mínimo: se aplica y se compara el texto final
  const we = new vscode.WorkspaceEdit();
  we.set(mdoc.uri, fmtEdits);
  await vscode.workspace.applyEdit(we);
  check('dar formato (sangría y espacios)', mdoc.getText() === 'if true {\n    print(1)\n}\n', JSON.stringify(mdoc.getText()));

  const lang = ext.packageJSON.contributes.languages[0];
  check('.hez es la extensión por defecto', lang.extensions[0] === '.hez', lang.extensions.join(' '));

  const cmds = await vscode.commands.getCommands(true);
  for (const c of ['hyperez.run', 'hyperez.watch', 'hyperez.repl', 'hyperez.newFile', 'hyperez.openGuide', 'hyperez.openWalkthrough', 'hyperez.check', 'hyperez.test', 'hyperez.newProject', 'hyperez.showDocs', 'hyperez.restartServer',
                   'hyperez.format', 'hyperez.showBytecode', 'hyperez.openInEditor', 'hyperez.runTests'])
    check('comando ' + c, cmds.includes(c));

  const tasks = await vscode.tasks.fetchTasks({ type: 'hyperez' });
  check('proveedor de tareas', Array.isArray(tasks));

  const out = process.env.HZ_TEST_RESULTS;
  if (out) fs.writeFileSync(out, JSON.stringify(results, null, 2));
  const failed = results.filter((r) => !r.ok);
  if (failed.length) throw new Error(failed.length + ' comprobaciones fallaron: ' + failed.map((f) => f.name).join('; '));
}

module.exports = { run };
