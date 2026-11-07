// Prueba de la Academia HyperEZ en un Chrome de verdad (sin ventana), por el protocolo de DevTools:
//   node projects/web/tests/aprender-navegador.test.js http://localhost:8123/aprender.html [captura.png]
// Juega la misión 1 (ejemplo, reto mal y bien, preguntas), llega al examen, emite el certificado y lo verifica.
'use strict';
const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const URL_EDITOR = process.argv[2] || 'http://localhost:8123/aprender.html';
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
    if (process.env.SIN_ANIMACIONES) await cdp('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
    await cdp('Page.navigate', { url: URL_EDITOR });
    await esperar(500);
    await evaluar('localStorage.clear()');
    await cdp('Page.reload', { ignoreCache: true });

    check('la Academia carga HyperEZ en WebAssembly', await hasta(`document.getElementById('estado-motor').classList.contains('listo')`, 30000),
      await evaluar(`document.getElementById('estado-motor').textContent`) + '\n    ' + consola.join('\n    '));
    check('el mapa tiene 10 misiones y solo la primera abierta', await evaluar(`document.querySelectorAll('.nodo').length === 10 && document.querySelectorAll('.nodo.abierta').length === 1 && document.querySelectorAll('.nodo.cerrada').length === 9`));

    // misión 1: lección (ejemplo ejecutable), reto y preguntas
    await evaluar(`document.querySelector('.nodo.abierta').click()`);
    check('se abre la misión', await hasta(`!document.getElementById('mision').hidden && document.getElementById('mision-titulo').textContent === '¡Hola, mundo!'`, 3000));
    await evaluar(`document.querySelector('[data-ejemplo]').click()`);
    check('el ejemplo de la lección se ejecuta', await hasta(`document.querySelector('.ejemplo-vivo .consola').textContent.includes('¡Hola, mundo!')`, 20000),
      await evaluar(`document.querySelector('.ejemplo-vivo .consola').textContent`));
    await evaluar(`document.getElementById('ir-reto').click()`);
    await hasta(`!!document.getElementById('b-comprobar')`, 3000);
    // primero mal: debe explicar la diferencia
    await evaluar(`(() => { const t = document.querySelector('.ed-texto'); t.value = 'print("Hola")'; t.dispatchEvent(new Event('input')); document.getElementById('b-comprobar').click(); })()`);
    check('un reto mal resuelto explica la diferencia', await hasta(`document.getElementById('resultado').classList.contains('mal') && !!document.querySelector('#resultado .dif')`, 20000),
      await evaluar(`document.getElementById('resultado').textContent`));
    // ahora bien
    const sol = await evaluar(`HZ_ACADEMIA.MISIONES[0].reto.solucion`);
    await evaluar(`(() => { const t = document.querySelector('.ed-texto'); t.value = ${JSON.stringify(sol)}; t.dispatchEvent(new Event('input')); document.getElementById('b-comprobar').click(); })()`);
    check('el reto se supera con HyperEZ de verdad', await hasta(`document.getElementById('resultado').classList.contains('bien')`, 20000),
      await evaluar(`document.getElementById('resultado').textContent + ' | ' + document.getElementById('consola').textContent`));
    check('da experiencia', await evaluar(`JSON.parse(localStorage.getItem('hyperez-academia-v1')).xp >= 100`));
    await evaluar(`document.getElementById('ir-preguntas').click()`);
    for (let k = 0; k < 3; k++) {
      await hasta(`!!document.querySelector('.opcion')`, 3000);
      const ok = await evaluar(`HZ_ACADEMIA.MISIONES[0].preguntas[${k}].ok`);
      await evaluar(`document.querySelector('.opcion[data-o="${ok}"]').click()`);
      await hasta(`!document.getElementById('siguiente').hidden`, 2000);
      await evaluar(`document.getElementById('siguiente').click()`);
      await esperar(200);
    }
    check('misión completada con tres estrellas', await hasta(`!!document.querySelector('.fin') && document.querySelectorAll('.fin-estrellas .si').length === 3`, 3000),
      await evaluar(`document.getElementById('mision-cuerpo').textContent.slice(0, 200)`));
    if (CAPTURA) fs.writeFileSync(CAPTURA.replace(/\.png$/, '-mision.png'), Buffer.from((await cdp('Page.captureScreenshot', { format: 'png' })).result.data, 'base64'));
    await evaluar(`document.getElementById('fin-mapa').click()`);
    check('la misión 2 queda desbloqueada', await hasta(`document.querySelectorAll('.nodo.hecha').length === 1 && document.querySelector('.nodo[data-mision="1"]').classList.contains('abierta')`, 3000));

    // las otras 9 misiones, ya hechas (para llegar al examen)
    await evaluar(`(() => { const e = JSON.parse(localStorage.getItem('hyperez-academia-v1'));
      for (const m of HZ_ACADEMIA.MISIONES) e.misiones[m.id] = e.misiones[m.id] && e.misiones[m.id].reto ? e.misiones[m.id] : { reto: true, pistas: 0, solucion: false, quiz: 100, estrellas: 3, codigo: null };
      localStorage.setItem('hyperez-academia-v1', JSON.stringify(e)); })()`);
    await cdp('Page.reload', {});
    check('con las 10 misiones se abre el examen', await hasta(`!document.getElementById('boton-examen').disabled`, 10000));
    await evaluar(`document.getElementById('boton-examen').click()`);
    for (let k = 0; k < 12; k++) {
      await hasta(`!!document.querySelector('.opcion:not(.elegida)')`, 3000);
      await evaluar(`(() => { const t = document.querySelector('.quiz h3').innerHTML; const q = HZ_ACADEMIA.EXAMEN.find((x) => x.p === t); document.querySelector('.opcion[data-o="' + q.ok + '"]').click(); })()`);
      await esperar(380);
    }
    check('examen aprobado con un 100%', await hasta(`document.querySelector('.nota-grande') && document.querySelector('.nota-grande').textContent.includes('100%')`, 4000),
      await evaluar(`document.getElementById('mision-cuerpo').textContent.slice(0, 300)`));
    await evaluar(`document.getElementById('fin-cert').click()`);
    check('aparece el formulario del certificado', await hasta(`!document.getElementById('cert-form').hidden`, 3000));
    await evaluar(`(() => { document.getElementById('cert-nombre').value = 'Adrián Cueva Caravaca'; document.getElementById('cert-form').requestSubmit(); })()`);
    check('se emite el certificado con el nombre, la nota y el código', await hasta(`(() => { const s = document.querySelector('#cert-marco svg'); return s && s.textContent.includes('Adrián Cueva Caravaca') && s.textContent.includes('100 / 100') && /HZ-\\d{4}-[0-9A-F]{4}-[0-9A-F]{4}/.test(s.textContent); })()`, 5000),
      await evaluar(`(document.querySelector('#cert-marco svg') || {}).textContent`));
    check('el certificado lleva el QR', await evaluar(`document.querySelectorAll('#cert-marco svg path[shape-rendering]').length === 1`));
    check('medalla de graduado', await evaluar(`JSON.parse(localStorage.getItem('hyperez-academia-v1')).insignias.includes('graduado')`));
    if (CAPTURA) {
      await evaluar(`document.getElementById('certificado-seccion').scrollIntoView()`);
      await esperar(1200);
      fs.writeFileSync(CAPTURA.replace(/\.png$/, '-certificado.png'), Buffer.from((await cdp('Page.captureScreenshot', { format: 'png' })).result.data, 'base64'));
    }
    const idCert = await evaluar(`JSON.parse(localStorage.getItem('hyperez-academia-v1')).certificado.id`);
    const enlace = await evaluar(`document.getElementById('cert-verificar').href`);
    check('el enlace de verificación va a verificar.html', /verificar\.html\?c=/.test(enlace), enlace);

    // verificar
    await cdp('Page.navigate', { url: enlace });
    check('la página de verificación confirma el certificado', await hasta(`document.querySelector('.veredicto.bien') && document.querySelector('.veredicto').textContent.includes('Adrián Cueva Caravaca') && document.querySelector('.veredicto').textContent.includes('${idCert}')`, 8000),
      await evaluar(`(document.querySelector('.veredicto') || {}).textContent`).catch(() => ''));
    if (CAPTURA) fs.writeFileSync(CAPTURA.replace(/\.png$/, '-verificar.png'), Buffer.from((await cdp('Page.captureScreenshot', { format: 'png' })).result.data, 'base64'));
    // retocado: otra nota con el mismo código impreso → el código que se muestra cambia
    const retocado = await evaluar(`(() => { const d = JSON.parse(HZCertificado.desdeB64url(new URLSearchParams(location.search).get('c'))); d.p = 55; return location.pathname + '?c=' + HZCertificado.b64url(JSON.stringify(d)); })()`);
    await cdp('Page.navigate', { url: new URL(retocado, enlace).href });
    check('un certificado retocado muestra otro código', await hasta(`document.querySelector('.veredicto') && !document.querySelector('.veredicto').textContent.includes('${idCert}')`, 8000));
    await cdp('Page.navigate', { url: new URL('verificar.html?c=roto', enlace).href });
    check('un enlace roto no es válido', await hasta(`!!document.querySelector('.veredicto.mal')`, 8000));

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
