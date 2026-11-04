// Extensión de VS Code para HyperEZ.
// - Cliente del servidor de lenguaje (`hyper lsp`): errores, autocompletado, hover, firmas, renombrar...
// - Ejecutar (F5), modo watch (se repite al guardar), consola interactiva, comprobar y probar.
// - Nuevo proyecto, referencia de la API, tareas, barra de estado y muestras de color.
'use strict';

const vscode = require('vscode');
const path = require('path');
const fs = require('fs');
const os = require('os');
const cp = require('child_process');

let client = null;
let statusItem = null;
let runTerminal = null;
let replTerminal = null;
let output = null;

// ---------------------------------------------------------------------------
// Localizar hyper.exe
// ---------------------------------------------------------------------------

function exeName() {
  return process.platform === 'win32' ? 'hyper.exe' : 'hyper';
}

function isFile(p) {
  try { return fs.statSync(p).isFile(); } catch { return false; }
}

function findOnPath() {
  const dirs = (process.env.PATH || '').split(path.delimiter);
  for (const d of dirs) {
    if (!d) continue;
    const p = path.join(d.replace(/^"|"$/g, ''), exeName());
    if (isFile(p)) return p;
  }
  return null;
}

function hyperPath(context) {
  const configured = vscode.workspace.getConfiguration('hyperez').get('path');
  if (configured && isFile(configured)) return configured;
  const candidates = [];
  if (process.platform === 'win32' && process.env.LOCALAPPDATA) {
    candidates.push(path.join(process.env.LOCALAPPDATA, 'Programs', 'HyperEZ', 'bin', 'hyper.exe'));
  }
  for (const c of candidates) if (isFile(c)) return c;
  const onPath = findOnPath();
  if (onPath) return onPath;
  const bundled = path.join(context.extensionPath, 'bin', exeName());
  if (isFile(bundled)) return bundled;
  return null;
}

function requireHyper(context) {
  const p = hyperPath(context);
  if (!p) {
    vscode.window.showErrorMessage(
      'No se encontró hyper.exe. Instala HyperEZ o indica su ruta en el ajuste "hyperez.path".',
      'Abrir ajustes'
    ).then((choice) => {
      if (choice) vscode.commands.executeCommand('workbench.action.openSettings', 'hyperez.path');
    });
  }
  return p;
}

function hyperVersion(exe) {
  try {
    return cp.execFileSync(exe, ['version'], { encoding: 'utf8', timeout: 5000 }).trim();
  } catch {
    return null;
  }
}

// ---------------------------------------------------------------------------
// Servidor de lenguaje
// ---------------------------------------------------------------------------

async function startClient(context) {
  if (!vscode.workspace.getConfiguration('hyperez').get('languageServer.enabled')) return;
  const exe = hyperPath(context);
  if (!exe) {
    output.appendLine('hyper.exe no encontrado: el servidor de lenguaje está desactivado.');
    return;
  }
  let lc;
  try {
    lc = require('vscode-languageclient/node');
  } catch (e) {
    output.appendLine('No se pudo cargar vscode-languageclient: ' + e.message);
    return;
  }
  const serverOptions = { command: exe, args: ['lsp'], transport: lc.TransportKind.stdio };
  const clientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'hyperez' },
      { scheme: 'untitled', language: 'hyperez' },
    ],
    outputChannel: output,
    synchronize: { fileEvents: vscode.workspace.createFileSystemWatcher('**/*.{hz,hez}') },
  };
  client = new lc.LanguageClient('hyperez', 'HyperEZ', serverOptions, clientOptions);
  try {
    await client.start();
    output.appendLine('Servidor de lenguaje iniciado: ' + exe);
  } catch (e) {
    output.appendLine('Error al iniciar el servidor de lenguaje: ' + e.message);
    client = null;
  }
}

async function stopClient() {
  if (client) {
    const c = client;
    client = null;
    try { await c.stop(); } catch { /* ya estaba parado */ }
  }
}

// ---------------------------------------------------------------------------
// Ejecución
// ---------------------------------------------------------------------------

function targetFile(uri) {
  if (uri && uri.fsPath) return uri.fsPath;
  const ed = vscode.window.activeTextEditor;
  if (ed && ed.document.languageId === 'hyperez') return ed.document.uri.fsPath;
  return null;
}

async function saveIfNeeded(file) {
  if (!vscode.workspace.getConfiguration('hyperez').get('run.saveBeforeRun')) return true;
  const doc = vscode.workspace.textDocuments.find((d) => d.uri.fsPath === file);
  if (doc && doc.isUntitled) {
    vscode.window.showWarningMessage('Guarda el archivo (con extensión .hz) antes de ejecutarlo.');
    return false;
  }
  if (doc && doc.isDirty) return doc.save();
  return true;
}

function runInTerminal(context, args, cwd, title) {
  const exe = requireHyper(context);
  if (!exe) return;
  if (runTerminal && vscode.workspace.getConfiguration('hyperez').get('run.reuseTerminal')) {
    runTerminal.dispose();
    runTerminal = null;
  }
  runTerminal = vscode.window.createTerminal({
    name: title,
    shellPath: exe,
    shellArgs: args,
    cwd,
    iconPath: vscode.Uri.file(path.join(context.extensionPath, 'icons', 'hz-file.svg')),
    env: { HYPEREZ_COLOR: '1' },
  });
  runTerminal.show(false);
}

async function cmdRun(context, uri) {
  const file = targetFile(uri);
  if (!file) { vscode.window.showWarningMessage('Abre un archivo .hz para ejecutarlo.'); return; }
  if (!(await saveIfNeeded(file))) return;
  if (!vscode.workspace.isTrusted) { vscode.window.showWarningMessage('Este espacio de trabajo no es de confianza: no se ejecutan programas.'); return; }
  runInTerminal(context, ['run', file], path.dirname(file), 'HyperEZ: ' + path.basename(file));
}

// Modo watch: ejecuta y vuelve a ejecutar cada vez que se guarda un .hz de la carpeta.
async function cmdWatch(context, uri) {
  const file = targetFile(uri);
  if (!file) { vscode.window.showWarningMessage('Abre un archivo .hz para ejecutarlo en modo watch.'); return; }
  if (!(await saveIfNeeded(file))) return;
  if (!vscode.workspace.isTrusted) { vscode.window.showWarningMessage('Este espacio de trabajo no es de confianza: no se ejecutan programas.'); return; }
  runInTerminal(context, ['watch', file], path.dirname(file), 'HyperEZ watch: ' + path.basename(file));
}

// Consola interactiva: una terminal propia que se reutiliza mientras siga abierta.
function cmdRepl(context) {
  if (replTerminal) { replTerminal.show(false); return; }
  const exe = requireHyper(context);
  if (!exe) return;
  const file = targetFile();
  const ws = vscode.workspace.workspaceFolders;
  const cwd = file ? path.dirname(file) : ws && ws.length ? ws[0].uri.fsPath : os.homedir();
  replTerminal = vscode.window.createTerminal({
    name: 'HyperEZ: consola',
    shellPath: exe,
    shellArgs: ['repl'],
    cwd,
    iconPath: new vscode.ThemeIcon('terminal'),
    env: { HYPEREZ_COLOR: '1' },
  });
  replTerminal.show(false);
}

async function cmdCheck(context, uri) {
  const file = targetFile(uri);
  if (!file) { vscode.window.showWarningMessage('Abre un archivo .hz para comprobarlo.'); return; }
  if (!(await saveIfNeeded(file))) return;
  runInTerminal(context, ['check', file], path.dirname(file), 'HyperEZ: comprobar');
}

function projectRootFor(file) {
  let dir = file ? path.dirname(file) : null;
  for (let i = 0; dir && i < 8; i++) {
    if (fs.existsSync(path.join(dir, 'tests')) || fs.existsSync(path.join(dir, 'main.hz'))) return dir;
    const up = path.dirname(dir);
    if (up === dir) break;
    dir = up;
  }
  const ws = vscode.workspace.workspaceFolders;
  return ws && ws.length ? ws[0].uri.fsPath : file ? path.dirname(file) : os.homedir();
}

async function cmdTest(context, uri) {
  const file = targetFile(uri);
  const root = projectRootFor(file);
  await vscode.workspace.saveAll(false);
  runInTerminal(context, ['test'], root, 'HyperEZ: pruebas');
}

async function cmdNewProject(context) {
  const exe = requireHyper(context);
  if (!exe) return;
  const kind = await vscode.window.showQuickPick([
    { label: '$(game) Juego', description: 'juego de consola con teclado', id: 'juego', name: 'mi-juego' },
    { label: '$(window) Aplicación', description: 'menú y datos guardados en JSON', id: 'app', name: 'mi-app' },
    { label: '$(sparkle) Inteligencia artificial', description: 'red neuronal que aprende de un CSV', id: 'ia', name: 'mi-ia' },
    { label: '$(terminal) Herramienta', description: 'script de línea de órdenes con argumentos', id: 'script', name: 'mi-herramienta' },
  ], { title: 'Nuevo proyecto de HyperEZ', placeHolder: '¿Qué quieres crear?' });
  if (!kind) return;
  const name = await vscode.window.showInputBox({
    title: 'Nuevo proyecto de HyperEZ',
    prompt: 'Nombre del proyecto (será el nombre de la carpeta)',
    value: kind.name,
    validateInput: (v) => (/^[^\\/:*?"<>|]+$/.test(v.trim()) ? null : 'Nombre no válido para una carpeta'),
  });
  if (!name) return;
  const parent = await vscode.window.showOpenDialog({
    canSelectFolders: true, canSelectFiles: false, canSelectMany: false, openLabel: 'Crear aquí',
    title: '¿Dónde se crea el proyecto?',
  });
  if (!parent || !parent.length) return;
  const dest = path.join(parent[0].fsPath, name.trim());
  try {
    cp.execFileSync(exe, ['new', dest, '--tipo', kind.id], { encoding: 'utf8', env: { ...process.env, NO_COLOR: '1' } });
  } catch (e) {
    vscode.window.showErrorMessage('No se pudo crear el proyecto: ' + ((e.stderr || e.stdout || e.message) + '').trim());
    return;
  }
  const choice = await vscode.window.showInformationMessage('Proyecto "' + name + '" creado.', 'Abrir', 'Abrir en otra ventana');
  if (choice === 'Abrir') await vscode.commands.executeCommand('vscode.openFolder', vscode.Uri.file(dest), false);
  else if (choice === 'Abrir en otra ventana') await vscode.commands.executeCommand('vscode.openFolder', vscode.Uri.file(dest), true);
}

async function cmdShowDocs(context) {
  const exe = requireHyper(context);
  if (!exe) return;
  const dir = context.globalStorageUri.fsPath;
  fs.mkdirSync(dir, { recursive: true });
  const out = path.join(dir, 'API-HyperEZ.md');
  try {
    cp.execFileSync(exe, ['docs', out], { encoding: 'utf8', env: { ...process.env, NO_COLOR: '1' } });
  } catch (e) {
    vscode.window.showErrorMessage('No se pudo generar la documentación: ' + e.message);
    return;
  }
  await vscode.commands.executeCommand('markdown.showPreview', vscode.Uri.file(out));
}

// Nuevo archivo .hz sin guardar, con un programa de ejemplo
async function cmdNewFile() {
  const doc = await vscode.workspace.openTextDocument({
    language: 'hyperez',
    content: '// Mi programa en HyperEZ: pulsa F5 para ejecutarlo (guárdalo antes con extensión .hz)\n' +
      'var nombre = "mundo"\nprint("¡Hola, ${nombre}!")\n',
  });
  await vscode.window.showTextDocument(doc);
}

// Carpeta de la instalación de HyperEZ (docs/ y examples/), si existe
function installDir(context) {
  const exe = hyperPath(context);
  if (!exe) return null;
  const dir = path.dirname(path.dirname(exe));
  return fs.existsSync(path.join(dir, 'docs')) ? dir : null;
}

const REPO = 'https://github.com/cuevacaravacaadrian-ux/HyperEZ';

async function cmdOpenGuide(context) {
  const dir = installDir(context);
  const local = dir && path.join(dir, 'docs', 'GUIA.md');
  if (local && fs.existsSync(local)) {
    await vscode.commands.executeCommand('markdown.showPreview', vscode.Uri.file(local));
  } else {
    await vscode.env.openExternal(vscode.Uri.parse(REPO + '/blob/main/docs/README.md'));
  }
}

async function cmdOpenExamples(context) {
  const dir = installDir(context);
  if (dir && fs.existsSync(path.join(dir, 'examples'))) {
    await vscode.commands.executeCommand('vscode.openFolder', vscode.Uri.file(path.join(dir, 'examples')), true);
  } else {
    await vscode.env.openExternal(vscode.Uri.parse(REPO + '/tree/main/examples'));
  }
}

// ---------------------------------------------------------------------------
// Tareas: { "type": "hyperez", "command": "test" }
// ---------------------------------------------------------------------------

function taskProvider(context) {
  const make = (def, folder) => {
    const exe = hyperPath(context) || 'hyper';
    const args = [def.command];
    if (def.file) args.push(def.file);
    const task = new vscode.Task(def, folder || vscode.TaskScope.Workspace, 'hyper ' + args.join(' '), 'hyperez',
      new vscode.ProcessExecution(exe, args, { env: { NO_COLOR: '1' } }), ['$hyperez']);
    if (def.command === 'test') task.group = vscode.TaskGroup.Test;
    if (def.command === 'check') task.group = vscode.TaskGroup.Build;
    return task;
  };
  return {
    provideTasks() {
      const folders = vscode.workspace.workspaceFolders || [];
      const tasks = [];
      for (const f of folders) {
        tasks.push(make({ type: 'hyperez', command: 'check', file: '.' }, f));
        tasks.push(make({ type: 'hyperez', command: 'test' }, f));
      }
      return tasks;
    },
    resolveTask(task) {
      if (task.definition && task.definition.command) return make(task.definition, task.scope);
      return undefined;
    },
  };
}

// ---------------------------------------------------------------------------
// Muestras de color: "#ff8800", Color(255, 136, 0) y Color.rgb(...)
// ---------------------------------------------------------------------------

const NAMED = {
  white: [255, 255, 255], black: [0, 0, 0], red: [230, 41, 55], green: [0, 228, 48], blue: [0, 121, 241], yellow: [253, 249, 0],
  orange: [255, 161, 0], purple: [200, 122, 255], pink: [255, 109, 194], cyan: [0, 220, 230], magenta: [255, 0, 255],
  gray: [130, 130, 130], lime: [50, 205, 50], gold: [255, 203, 0], skyblue: [102, 191, 255],
};

function colorProvider() {
  const hexRe = /(["'])#([0-9a-fA-F]{3,8})\1/g;
  const ctorRe = /\bColor(?:\.rgb)?\(\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)\s*,\s*(\d+(?:\.\d+)?)\s*(?:,\s*(\d+(?:\.\d+)?)\s*)?\)/g;
  const namedRe = /\bColor\.([A-Z]+)\b/g;
  const hex2 = (n) => Math.round(Math.max(0, Math.min(1, n)) * 255).toString(16).padStart(2, '0');
  return {
    provideDocumentColors(doc) {
      if (!vscode.workspace.getConfiguration('hyperez').get('colorDecorators')) return [];
      const text = doc.getText();
      const out = [];
      let m;
      while ((m = hexRe.exec(text))) {
        let h = m[2];
        if (h.length === 3 || h.length === 4) h = h.split('').map((c) => c + c).join('');
        if (h.length !== 6 && h.length !== 8) continue;
        const v = (i) => parseInt(h.substr(i, 2), 16) / 255;
        const start = doc.positionAt(m.index + 1);
        const end = doc.positionAt(m.index + m[0].length - 1);
        out.push(new vscode.ColorInformation(new vscode.Range(start, end), new vscode.Color(v(0), v(2), v(4), h.length === 8 ? v(6) : 1)));
      }
      while ((m = ctorRe.exec(text))) {
        const a = m[4] !== undefined ? +m[4] / 255 : 1;
        const range = new vscode.Range(doc.positionAt(m.index), doc.positionAt(m.index + m[0].length));
        out.push(new vscode.ColorInformation(range, new vscode.Color(+m[1] / 255, +m[2] / 255, +m[3] / 255, a)));
      }
      while ((m = namedRe.exec(text))) {
        const c = NAMED[m[1].toLowerCase()];
        if (!c) continue;
        const range = new vscode.Range(doc.positionAt(m.index), doc.positionAt(m.index + m[0].length));
        out.push(new vscode.ColorInformation(range, new vscode.Color(c[0] / 255, c[1] / 255, c[2] / 255, 1)));
      }
      return out;
    },
    provideColorPresentations(color, ctx) {
      const current = ctx.document.getText(ctx.range);
      const r = Math.round(color.red * 255), g = Math.round(color.green * 255), b = Math.round(color.blue * 255);
      const a = Math.round(color.alpha * 255);
      const hex = '#' + hex2(color.red) + hex2(color.green) + hex2(color.blue) + (a < 255 ? hex2(color.alpha) : '');
      if (current.startsWith('#')) return [new vscode.ColorPresentation(hex)];
      return [
        new vscode.ColorPresentation(a < 255 ? `Color(${r}, ${g}, ${b}, ${a})` : `Color(${r}, ${g}, ${b})`),
        new vscode.ColorPresentation(`Color.hex("${hex}")`),
      ];
    },
  };
}

// ---------------------------------------------------------------------------
// Activación
// ---------------------------------------------------------------------------

function updateStatus(context) {
  const ed = vscode.window.activeTextEditor;
  if (ed && ed.document.languageId === 'hyperez') statusItem.show();
  else statusItem.hide();
}

async function activate(context) {
  output = vscode.window.createOutputChannel('HyperEZ');
  context.subscriptions.push(output);

  const reg = (id, fn) => context.subscriptions.push(vscode.commands.registerCommand(id, (...a) => fn(context, ...a)));
  reg('hyperez.run', cmdRun);
  reg('hyperez.newFile', cmdNewFile);
  reg('hyperez.openGuide', cmdOpenGuide);
  reg('hyperez.openExamples', cmdOpenExamples);
  reg('hyperez.openWalkthrough', () =>
    vscode.commands.executeCommand('workbench.action.openWalkthrough', context.extension.id + '#empezar', false));
  reg('hyperez.watch', cmdWatch);
  reg('hyperez.repl', cmdRepl);
  reg('hyperez.check', cmdCheck);
  reg('hyperez.test', cmdTest);
  reg('hyperez.newProject', cmdNewProject);
  reg('hyperez.showDocs', cmdShowDocs);
  reg('hyperez.restartServer', async (ctx) => {
    await stopClient();
    await startClient(ctx);
    vscode.window.setStatusBarMessage('HyperEZ: servidor de lenguaje reiniciado', 3000);
  });
  reg('hyperez.showVersion', (ctx) => {
    const exe = requireHyper(ctx);
    if (!exe) return;
    vscode.window.showInformationMessage((hyperVersion(exe) || 'HyperEZ (versión desconocida)') + ' — ' + exe);
  });

  statusItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 50);
  statusItem.text = '$(play) HyperEZ';
  statusItem.tooltip = 'Ejecutar este archivo con HyperEZ (F5)';
  statusItem.command = 'hyperez.run';
  context.subscriptions.push(statusItem);
  context.subscriptions.push(vscode.window.onDidChangeActiveTextEditor(() => updateStatus(context)));
  updateStatus(context);

  context.subscriptions.push(vscode.tasks.registerTaskProvider('hyperez', taskProvider(context)));
  context.subscriptions.push(vscode.languages.registerColorProvider({ language: 'hyperez' }, colorProvider()));
  context.subscriptions.push(vscode.window.onDidCloseTerminal((t) => {
    if (t === runTerminal) runTerminal = null;
    if (t === replTerminal) replTerminal = null;
  }));
  context.subscriptions.push(vscode.workspace.onDidChangeConfiguration(async (e) => {
    if (e.affectsConfiguration('hyperez.path') || e.affectsConfiguration('hyperez.languageServer')) {
      await stopClient();
      await startClient(context);
    }
  }));

  await startClient(context);

  // La primera vez, la bienvenida con el tutorial
  if (!context.globalState.get('hyperez.welcomed')) {
    await context.globalState.update('hyperez.welcomed', true);
    if (!process.env.HYPEREZ_NO_WELCOME) {
      vscode.commands.executeCommand('workbench.action.openWalkthrough', context.extension.id + '#empezar', false);
    }
  }
}

async function deactivate() {
  await stopClient();
}

module.exports = { activate, deactivate };
