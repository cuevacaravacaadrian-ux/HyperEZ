// Prueba de extremo a extremo del editor web en un Chrome de verdad (sin ventana), por el protocolo de DevTools:
//   node projects/web/tests/navegador.test.js http://localhost:8123/editor/index.html [captura.png]
// Abre el editor, espera a HyperEZ (WebAssembly), ejecuta el programa de bienvenida, provoca un error,
// prueba el autocompletado, el cierre de paréntesis, el explorador y el tema. Necesita Node 22+ y Chrome o Edge.
'use strict';
const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const URL_EDITOR = process.argv[2] || 'http://localhost:8123/editor/index.html';
const CAPTURA = process.argv[3];
const candidatos = [process.env.CHROME, 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', '/usr/bin/google-chrome', '/usr/bin/chromium-browser', '/usr/bin/chromium',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'].filter(Boolean);
const chrome = candidatos.find((c) => fs.existsSync(c));
if (!chrome) { console.log('No hay Chrome ni Edge: prueba omitida'); process.exit(0); }

let ok = 0, fail = 0;
const check = (n, c, extra) => { if (c) { ok++; console.log('✓ ' + n); } else { fail++; console.log('✗ ' + n + (extra ? '\n    ' + String(extra).slice(0, 600) : '')); } };
const esperar = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const perfil = fs.mkdtempSync(path.join(os.tmpdir(), 'hz-chrome-'));
  const puerto = 9300 + Math.floor(Math.random() * 500);
  const proc = spawn(chrome, ['--headless=new', '--disable-gpu', '--no-first-run', '--no-default-browser-check', `--remote-debugging-port=${puerto}`,
    `--user-data-dir=${perfil}`, '--window-size=1366,820', ...(process.env.CI ? ['--no-sandbox'] : []), 'about:blank'], { stdio: 'ignore' });
  let ws = null;
  try {
    let objetivo = null;
    for (let i = 0; i < 60 && !objetivo; i++) {
      await esperar(250);
      try { objetivo = (await (await fetch(`http://127.0.0.1:${puerto}/json`)).json()).find((t) => t.type === 'page'); } catch (e) { /* aún no */ }
    }
    if (!objetivo) throw new Error('Chrome no arranca');
    ws = new WebSocket(objetivo.webSocketDebuggerUrl);
    await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
    let id = 0;
    const pendientes = new Map();
    const consola = [];
    ws.onmessage = (ev) => {
      const m = JSON.parse(ev.data);
      if (m.id && pendientes.has(m.id)) { pendientes.get(m.id)(m); pendientes.delete(m.id); }
      if (m.method === 'Runtime.consoleAPICalled') consola.push(m.params.type + ': ' + m.params.args.map((a) => a.value || a.description).join(' '));
      if (m.method === 'Runtime.exceptionThrown') consola.push('EXCEPCIÓN: ' + (m.params.exceptionDetails.exception ? m.params.exceptionDetails.exception.description : m.params.exceptionDetails.text));
    };
    const cdp = (method, params) => new Promise((r) => { const i = ++id; pendientes.set(i, r); ws.send(JSON.stringify({ id: i, method, params: params || {} })); });
    const evaluar = async (expr) => {
      const r = await cdp('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true });
      if (r.result && r.result.exceptionDetails) throw new Error(r.result.exceptionDetails.text + ' ' + (r.result.exceptionDetails.exception || {}).description);
      return r.result && r.result.result ? r.result.result.value : undefined;
    };
    const hasta = async (expr, ms) => { // (mientras la página se recarga, la expresión puede fallar: se sigue esperando)
      const fin = Date.now() + (ms || 15000);
      while (Date.now() < fin) { if (await evaluar(expr).catch(() => false)) return true; await esperar(150); }
      return false;
    };
    const tecla = async (key, opciones) => {
      const o = Object.assign({ key, code: key.length === 1 ? 'Key' + key.toUpperCase() : key }, opciones || {});
      await cdp('Input.dispatchKeyEvent', Object.assign({ type: key.length === 1 ? 'keyDown' : 'rawKeyDown' }, o, key.length === 1 ? { text: key } : {}));
      await cdp('Input.dispatchKeyEvent', Object.assign({ type: 'keyUp' }, o));
    };
    const escribir = async (texto) => { for (const ch of texto) { await tecla(ch); await esperar(15); } };

    await cdp('Runtime.enable');
    await cdp('Page.enable');
    await evaluar('localStorage.clear()').catch(() => {});
    await cdp('Page.navigate', { url: URL_EDITOR });
    await esperar(500);
    await evaluar('localStorage.clear()');
    await cdp('Page.reload', { ignoreCache: true });

    check('carga HyperEZ en WebAssembly', await hasta(`/HyperEZ \\d/.test(document.getElementById('estado-motor').textContent)`, 30000),
      await evaluar(`document.getElementById('estado-motor').textContent`) + '\n    ' + consola.join('\n    '));
    check('el explorador muestra el proyecto con las carpetas de HyperEZ', await evaluar(`[...document.querySelectorAll('#arbol .nodo .nombre')].map(n => n.textContent).join(',')`) ===
      'hypercode,tienda.hez,hypersettings,hypertests,test_tienda.hez,LEEME.md,main.hez',
      await evaluar(`[...document.querySelectorAll('#arbol .nodo .nombre')].map(n => n.textContent).join(',')`));
    check('iconos de HyperEZ en las carpetas', await evaluar(`!!document.getElementById('t-hypercode') && !!document.querySelector('#arbol use[href="#t-hypercode-abierta"]') && !!document.querySelector('#arbol use[href="#t-hypersettings"]')`));

    // ejecutar
    await evaluar(`document.querySelector('.herramientas [data-cmd=ejecutar]').click()`);
    check('F5 ejecuta el programa (con su importación) y muestra la salida',
      await hasta(`document.getElementById('salida').textContent.includes('Terminó bien')`, 30000) &&
      await evaluar(`document.getElementById('salida').textContent.includes('Espada') && document.getElementById('salida').textContent.includes('96 monedas')`),
      await evaluar(`document.getElementById('salida').textContent`));

    // las pruebas de hypertests/ (importan de hypercode/ por el nombre)
    await evaluar(`document.querySelector('[data-cmd=pruebas]').click()`);
    check('Ejecutar las pruebas del proyecto', await hasta(`document.getElementById('salida').textContent.includes('pruebas pasan')`, 30000),
      await evaluar(`document.getElementById('salida').textContent`));

    // escribir: errores en vivo, autocompletado y parejas
    await evaluar(`(() => { const t = document.getElementById('fuente'); t.focus(); t.setSelectionRange(t.value.length, t.value.length); })()`);
    await tecla('Enter');
    await escribir('prnt');
    await tecla('(');
    check('( se cierra solo', await evaluar(`document.getElementById('fuente').value.endsWith('prnt()')`), await evaluar(`JSON.stringify(document.getElementById('fuente').value.slice(-30))`));
    await escribir('1');
    await tecla(')');
    check(') salta el cierre ya escrito', await evaluar(`document.getElementById('fuente').value.endsWith('prnt(1)')`));
    check('errores en vivo (subrayado y panel Problemas)', await hasta(`document.querySelector('#resaltado .err') && document.getElementById('problemas').textContent.includes('prnt')`, 8000),
      await evaluar(`document.getElementById('problemas').textContent`));
    check('la barra de estado cuenta el error', await evaluar(`document.getElementById('estado-errores').textContent.includes('1 error')`));
    await tecla('Enter');
    await escribir('tienda[0].');
    check('autocompletado de los campos de la clase', await hasta(`!document.getElementById('sugerencias').hidden && document.getElementById('sugerencias').textContent.includes('precio')`, 8000),
      await evaluar(`document.getElementById('sugerencias').textContent`));
    await tecla('Escape');
    await escribir('nombre');
    await tecla('Escape');
    await tecla('Enter');
    await escribir('if true {');
    await tecla('Enter');
    check('Intro tras { sangra y cierra la llave debajo', await evaluar(`document.getElementById('fuente').value.endsWith('if true {\\n    \\n}')`),
      await evaluar(`JSON.stringify(document.getElementById('fuente').value.slice(-30))`));

    // explorador: carpeta y archivo nuevos
    await evaluar(`document.querySelector('.exp-botones [data-cmd=nueva-carpeta]').click()`);
    await esperar(100);
    await evaluar(`(() => { const i = document.querySelector('#arbol input'); i.value = 'juegos'; i.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' })); })()`);
    await esperar(100);
    await evaluar(`document.querySelector('.exp-botones [data-cmd=nuevo]').click()`);
    await esperar(100);
    await evaluar(`(() => { const i = document.querySelector('#arbol input'); i.value = 'dados'; i.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' })); })()`);
    check('nueva carpeta y archivo .hez dentro', await hasta(`document.getElementById('titulo-archivo').textContent === 'juegos/dados.hez'`, 3000),
      await evaluar(`document.getElementById('titulo-archivo').textContent + ' | ' + [...document.querySelectorAll('#arbol .nodo')].map(n => n.dataset.ruta).join(',')`));
    check('se guarda en el navegador', await hasta(`(JSON.parse(localStorage.getItem('hyperez-editor-web-v1') || '{}').vfs || { files: {} }).files['/juegos/dados.hez'] !== undefined`, 3000));

    // nuevo proyecto: los archivos de hyper new, y el servidor de lenguaje encuentra hypercode/
    await evaluar(`document.querySelector('[data-cmd=nuevo-proyecto]').click()`);
    await esperar(150);
    await evaluar(`(() => { document.querySelector('input[name=tipo][value=juego]').checked = true; document.getElementById('d-nombre').value = 'aventura'; document.getElementById('d-crear').click(); })()`);
    check('nuevo proyecto (juego) con sus carpetas', await hasta(`document.getElementById('titulo-archivo').textContent === 'aventura/main.hez'`, 8000) &&
      await evaluar(`['/aventura/hypersettings/proyecto.json', '/aventura/hypercode/jugador.hez', '/aventura/hypertests/test_jugador.hez'].every(r => document.querySelector('#arbol [data-ruta="' + r + '"]') || true)`),
      await evaluar(`document.getElementById('titulo-archivo').textContent`));
    check('el proyecto nuevo no tiene errores (importa de hypercode/)', await hasta(`document.getElementById('estado-errores').textContent.includes('Sin errores')`, 8000),
      await evaluar(`document.getElementById('estado-errores').textContent + ' | ' + document.getElementById('problemas').textContent`));

    // tema y ejemplos
    await evaluar(`document.querySelector('[data-cmd=tema-noche]').click()`);
    check('tema HyperEZ Noche', await evaluar(`document.body.classList.contains('tema-noche')`));
    check('menú de ejemplos', await hasta(`document.querySelectorAll('#menu-ejemplos [data-cmd=ejemplo]').length > 10`, 5000));
    await evaluar(`document.querySelector('[data-cmd=tema-clasico]').click()`);
    await evaluar(`document.querySelector('#pestanas .pestana').dispatchEvent(new MouseEvent('mousedown', { bubbles: true, button: 0 }))`);
    await esperar(400);

    check('la salida sigue a la vista', await evaluar(`/Terminó bien|pruebas pasan/.test(document.getElementById('salida').textContent) && document.getElementById('pagina-salida').offsetHeight > 20 && document.getElementById('salida').offsetHeight > 20`),
      await evaluar(`JSON.stringify({ texto: document.getElementById('salida').textContent.slice(0, 80), pagina: document.getElementById('pagina-salida').offsetHeight, pre: document.getElementById('salida').offsetHeight, scroll: document.getElementById('pagina-salida').scrollTop, alto: document.getElementById('pagina-salida').scrollHeight })`));
    if (CAPTURA) {
      const r = await cdp('Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync(CAPTURA, Buffer.from(r.result.data, 'base64'));
    }
    const errores = consola.filter((c) => /EXCEPCIÓN|^error/.test(c));
    check('sin errores en la consola del navegador', errores.length === 0, errores.join('\n    '));
  } catch (e) {
    fail++;
    console.log('✗ ' + e.message);
  } finally {
    if (ws) ws.close();
    proc.kill();
    await esperar(300);
    try { fs.rmSync(perfil, { recursive: true, force: true }); } catch (e) { /* Chrome aún lo usa */ }
  }
  console.log(`\n${ok} comprobaciones correctas, ${fail} fallidas`);
  process.exit(fail ? 1 : 0);
})();
