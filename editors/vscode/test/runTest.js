// Lanza VS Code con la extensión en desarrollo y ejecuta test/suite/index.js.
//   node test/runTest.js [ruta de Code.exe]   (sin ruta: descarga VS Code Insiders en .vscode-test; su instancia no choca con un VS Code abierto o actualizándose)
// Usa un perfil y una carpeta de extensiones temporales: no toca la configuración del usuario.
'use strict';
const path = require('path');
const fs = require('fs');
const os = require('os');
const { runTests } = require('@vscode/test-electron');

// Por defecto se usa una copia aislada descargada en .vscode-test; se puede indicar otra.
function findCode() {
  const p = process.argv[2] || process.env.HZ_VSCODE_EXE;
  return p && fs.existsSync(p) ? p : undefined;
}

(async () => {
  // Si este script se ejecuta desde una terminal de VS Code, sus variables harían que Code.exe
  // se comportara como Node o se conectara a la ventana ya abierta.
  for (const k of Object.keys(process.env)) if (k === 'ELECTRON_RUN_AS_NODE' || k.startsWith('VSCODE_')) delete process.env[k];
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'hz-vscode-profile-'));
  const results = path.join(tmp, 'resultados.json');
  process.env.HZ_TEST_RESULTS = results;
  let code = 0;
  try {
    await runTests({
      vscodeExecutablePath: findCode(),
      version: process.env.HZ_VSCODE_VERSION || 'insiders',
      extensionDevelopmentPath: path.join(__dirname, '..'),
      extensionTestsPath: path.join(__dirname, 'suite', 'index.js'),
      launchArgs: ['--user-data-dir', path.join(tmp, 'datos'), '--extensions-dir', path.join(tmp, 'ext'), '--disable-workspace-trust', '--skip-welcome', '--skip-release-notes'],
      extensionTestsEnv: { HZ_TEST_RESULTS: results },
    });
  } catch (e) {
    console.error('Las pruebas de integración fallaron:', e.message || e);
    code = 1;
  }
  if (fs.existsSync(results)) {
    for (const r of JSON.parse(fs.readFileSync(results, 'utf8'))) console.log((r.ok ? '✓ ' : '✗ ') + r.name + (r.detail ? '  — ' + r.detail : ''));
  }
  process.exit(code);
})();
