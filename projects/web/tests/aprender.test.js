// Prueba la Academia HyperEZ con el módulo WebAssembly de verdad:
//   node projects/web/tests/aprender.test.js build-web/hyperez.js
// - cada solución supera su reto y el código inicial no (si no, el reto sería trivial);
// - cada ejemplo de las lecciones se ejecuta sin errores;
// - las preguntas están bien formadas;
// - el certificado: el código de identificación se recalcula igual y cambia si alguien toca los datos.
'use strict';
const path = require('path');
require(path.join(__dirname, '../src/aprender/misiones.js'));
require(path.join(__dirname, '../src/aprender/reto.js'));
if (!globalThis.crypto) globalThis.crypto = require('crypto').webcrypto;
require(path.join(__dirname, '../src/aprender/certificado.js'));
const { MISIONES, EXAMEN } = globalThis.HZ_ACADEMIA;

const modPath = path.resolve(process.argv[2] || 'build-web/hyperez.js');
let ok = 0, fail = 0;
const check = (n, c, extra) => { if (c) ok++; else { fail++; console.log('✗ ' + n + (extra ? '\n    ' + String(extra).slice(0, 800).replace(/\n/g, '\n    ') : '')); } };

(async () => {
  let out = '';
  const hz = await require(modPath)({ print: (t) => { out += t + '\n'; }, printErr: (t) => { out += t + '\n'; }, stdin: () => null });
  hz.ENV.NO_COLOR = '1';
  hz.FS.mkdirTree('/proyecto');
  const run = hz.cwrap('hz_run', 'number', ['string']);
  let n = 0;
  const ejecutar = async (codigo) => {
    const ruta = '/proyecto/reto' + (n++) + '.hez';
    hz.FS.writeFile(ruta, codigo);
    out = '';
    const codigoSalida = run(ruta);
    return { salida: out, codigo: codigoSalida, pasadas: hz.ccall('hz_tests_passed', 'number', [], []), falladas: hz.ccall('hz_tests_failed', 'number', [], []) };
  };

  const ids = new Set();
  for (const m of MISIONES) {
    check(m.id + ': id único', !ids.has(m.id)); ids.add(m.id);
    const bien = await globalThis.HZReto.comprobar(m.reto, m.reto.solucion, ejecutar);
    check(m.id + ': la solución supera el reto', bien.ok, JSON.stringify(bien).slice(0, 300) + '\n' + (bien.r ? bien.r.salida : ''));
    const mal = await globalThis.HZReto.comprobar(m.reto, m.reto.inicial, ejecutar);
    check(m.id + ': el código inicial no supera el reto', !mal.ok);
    if (m.reto.esperado && m.reto.comprobar.variantes) {
      const r = await ejecutar(m.reto.solucion);
      check(m.id + ': la salida esperada del enunciado coincide con la solución', globalThis.HZReto.normalizar(r.salida) === m.reto.esperado, r.salida);
    }
    for (const [i, b] of m.leccion.entries()) {
      if (!b.codigo) continue;
      const r = await ejecutar(b.codigo);
      check(m.id + ': el ejemplo ' + i + ' funciona', r.codigo === 0 && r.salida.trim() !== '', b.codigo + '\n→ ' + r.salida);
    }
    for (const q of m.preguntas) check(m.id + ': pregunta bien formada', q.o.length >= 3 && q.ok >= 0 && q.ok < q.o.length && q.porque, q.p);
  }
  // una trampa: escribir la salida a mano no vale en los retos con variantes
  const trampa = await globalThis.HZReto.comprobar(MISIONES.find((m) => m.id === 'numeros').reto,
    'var monedas = 250\nvar pociones = 3\nconst PRECIO = 40\nprint("Te quedan 130 monedas")\n', ejecutar);
  check('escribir la salida a mano no supera el reto', !trampa.ok && /cambiamos/.test(trampa.mensaje), JSON.stringify(trampa));
  for (const q of EXAMEN) check('examen: pregunta bien formada', q.o.length >= 3 && q.ok >= 0 && q.ok < q.o.length, q.p);

  // ---- certificado ----
  const C = globalThis.HZCertificado;
  const d = await C.emitir('  Ana   García López ', 91.6, 10);
  check('el nombre se limpia', d.n === 'Ana García López', d.n);
  check('la nota se redondea', d.p === 92);
  check('formato del código', /^HZ-\d{4}-[0-9A-F]{4}-[0-9A-F]{4}$/.test(d.id), d.id);
  const url = C.enlace(d, 'https://cuevacaravacaadrian.github.io/HyperEZ/aprender.html');
  check('el enlace va a verificar.html', url.startsWith('https://cuevacaravacaadrian.github.io/HyperEZ/verificar.html?c='), url);
  const v = await C.verificar(url);
  check('verificar un certificado auténtico', v.ok && v.datos.id === d.id && v.datos.n === d.n && v.datos.p === 92, JSON.stringify(v));
  const datos = JSON.parse(C.desdeB64url(url.split('?c=')[1]));
  datos.p = 100;
  const falso = await C.verificar('?c=' + C.b64url(JSON.stringify(datos)));
  check('un certificado retocado cambia de código', falso.ok && falso.datos.id !== d.id);
  check('un enlace roto no se verifica', !(await C.verificar('?c=%%%')).ok && !(await C.verificar('hola')).ok);
  check('el SVG lleva el nombre, la nota y el código', (() => { const s = C.svg(d, url, {}); return s.includes('Ana García López') && s.includes('92 / 100') && s.includes(d.id); })());

  // el QR se lee (si está jsQR, solo en las pruebas locales)
  try {
    const jsQR = require(process.env.JSQR || 'jsqr');
    global.qrcode = require(path.join(__dirname, '../src/aprender/qrcode.js'));
    const qr = global.qrcode(0, 'L'); qr.addData(url); qr.make();
    const k = qr.getModuleCount(), esc = 6, lado = (k + 8) * esc;
    const px = new Uint8ClampedArray(lado * lado * 4).fill(255);
    for (let r = 0; r < k; r++) for (let c = 0; c < k; c++) if (qr.isDark(r, c))
      for (let y = 0; y < esc; y++) for (let x = 0; x < esc; x++) { const i = (((r + 4) * esc + y) * lado + (c + 4) * esc + x) * 4; px[i] = px[i + 1] = px[i + 2] = 0; }
    const leido = jsQR(px, lado, lado);
    check('el QR lleva el enlace de verificación', leido && leido.data === url, leido && leido.data);
  } catch (e) { if (!/Cannot find module/.test(e.message)) throw e; console.log('(sin jsQR: no se lee el QR)'); }

  console.log(`${ok} comprobaciones correctas, ${fail} fallidas`);
  process.exit(fail ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(1); });
