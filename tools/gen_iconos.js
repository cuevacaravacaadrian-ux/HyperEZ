// Iconos de las carpetas y archivos de HyperEZ (los mismos en VS Code, HyperEZ Editor y el editor web).
//
//   node tools/gen_iconos.js           escribe los SVG (assets/icons/tema) y el tema de iconos de VS Code
//   node tools/gen_iconos.js --ico     además, los .ico de HyperEZ Editor (los dibuja Chrome: el mismo resultado)
//
// Cada carpeta de un proyecto HyperEZ (docs/herramientas/proyectos.md) tiene su color y su símbolo.
'use strict';
const fs = require('fs');
const path = require('path');

const RAIZ = path.join(__dirname, '..');
const SALIDA = path.join(RAIZ, 'assets', 'icons', 'tema');
const VSCODE = path.join(RAIZ, 'editors', 'vscode', 'icons');

// ---- carpetas: nombre → [color, color oscuro, símbolo (en blanco, en una rejilla de 32), descripción] ----
const GEAR = (cx, cy, fondo) => `<g transform="translate(${cx} ${cy})"><path fill="#fff" d="M-1.1-5.6h2.2l.4 1.6 1.2.5 1.4-.9 1.6 1.6-.9 1.4.5 1.2 1.6.4v2.2l-1.6.4-.5 1.2.9 1.4-1.6 1.6-1.4-.9-1.2.5-.4 1.6h-2.2l-.4-1.6-1.2-.5-1.4.9-1.6-1.6.9-1.4-.5-1.2-1.6-.4v-2.2l1.6-.4.5-1.2-.9-1.4 1.6-1.6 1.4.9 1.2-.5z"/><circle r="1.8" fill="${fondo}"/></g>`;
const L = 'fill="none" stroke="#fff" stroke-linecap="round" stroke-linejoin="round"';
const CARPETAS = {
  carpeta: ['#e8b339', '#c08f1f', () => '', 'una carpeta cualquiera'],
  hypersettings: ['#7c8aa5', '#596580', (c) => GEAR(19, 19.8, c), 'configuración del proyecto (proyecto.json)'],
  hypercode: ['#2f81f7', '#1f5fbf', () => `<path ${L} stroke-width="2" d="M15.6 16.2 12.4 19.8l3.2 3.6M22.4 16.2l3.2 3.6-3.2 3.6M20.3 15.4l-2.6 8.8"/>`, 'los módulos del programa'],
  hypertests: ['#2da44e', '#1f7a39', () => `<path ${L} stroke-width="2.6" d="M14.2 19.9l3.3 3.3 6.6-6.8"/>`, 'las pruebas (hyper test)'],
  hyperlibs: ['#8957e5', '#6639ba', () => `<g ${L} stroke-width="1.6"><path d="M19 14.6l5.4 2.7v6l-5.4 2.7-5.4-2.7v-6z"/><path d="M13.6 17.3l5.4 2.7 5.4-2.7M19 20v6"/></g>`, 'bibliotecas de otros (import por nombre)'],
  hypertools: ['#db8b0b', '#a96a05', () => `<path fill="#fff" d="M23.8 14.9a3.4 3.4 0 0 0-4.5 4.1l-5.1 5.1a1.3 1.3 0 0 0 1.8 1.8l5.1-5.1a3.4 3.4 0 0 0 4.1-4.5l-2 2-1.9-.5-.5-1.9z"/>`, 'herramientas del proyecto (hyper tool)'],
  hyperassets: ['#d6409f', '#a8317b', () => `<rect x="13.2" y="15" width="11.6" height="9.6" rx="1.4" ${L} stroke-width="1.6"/><circle cx="16.6" cy="18" r="1.3" fill="#fff"/><path ${L} stroke-width="1.5" d="M13.9 23.8l3.6-3.4 2.4 2.2 1.8-1.6 2.6 2.6"/>`, 'imágenes, sonidos y fuentes'],
  hyperdata: ['#1b9bb0', '#157687', () => `<g ${L} stroke-width="1.6"><ellipse cx="19" cy="15.9" rx="5" ry="1.9"/><path d="M14 15.9v7.6c0 1 2.2 1.9 5 1.9s5-.9 5-1.9v-7.6M14 19.7c0 1 2.2 1.9 5 1.9s5-.9 5-1.9"/></g>`, 'datos del programa (CSV, JSON...)'],
  hyperdocs: ['#5865c9', '#414ca0', () => `<path fill="#fff" d="M13.8 15.2h4.4c.5 0 .8.3.8.8v9.2c0-.6-.4-1-1-1h-4.2zM24.2 15.2h-4.4c-.5 0-.8.3-.8.8v9.2c0-.6.4-1 1-1h4.2z"/>`, 'la documentación del proyecto'],
  hyperbuild: ['#6e7781', '#4f5760', () => `<path ${L} stroke-width="2" d="M19 14.4v7M15.9 18.5l3.1 3.1 3.1-3.1M14.2 24.9h9.6"/>`, 'lo que se genera (no se sube a git)'],
};

function carpetaSvg(nombre, abierta) {
  const [c, oscuro, simbolo] = CARPETAS[nombre];
  const fondo = '<path fill="' + oscuro + '" d="M3 8.5C3 7.1 4.1 6 5.5 6h7.3c.7 0 1.3.3 1.8.8l1.9 2.2h10c1.4 0 2.5 1.1 2.5 2.5v13c0 1.4-1.1 2.5-2.5 2.5h-21C4.1 27 3 25.9 3 24.5z"/>';
  const frente = abierta
    ? '<path fill="' + c + '" d="M6.2 13.2c.3-.8 1-1.2 1.9-1.2H29c1.1 0 1.8 1 1.5 2l-3.2 11c-.3 1.2-1.4 2-2.6 2H5.3c-1.3 0-2.2-1.2-1.8-2.4z"/>'
    : '<path fill="' + c + '" d="M3 12.5c0-1.1.9-2 2-2h22c1.1 0 2 .9 2 2v12c0 1.4-1.1 2.5-2.5 2.5h-21C4.1 27 3 25.9 3 24.5z"/>';
  const s = simbolo(c);
  const dentro = abierta && s ? `<g transform="translate(1.2 .3)">${s}</g>` : s;
  return fondo + frente + dentro;
}

// ---- archivos ----
const ARCHIVOS = {
  archivo: '<path fill="#f3f4f6" stroke="#8c959f" stroke-width="1.2" d="M8 3.5h10.5L25 10v17.5c0 .6-.4 1-1 1H8c-.6 0-1-.4-1-1v-23c0-.6.4-1 1-1z"/><path fill="none" stroke="#8c959f" stroke-width="1.2" d="M18.5 3.5V10H25"/>',
  hez: '<rect x="3" y="3" width="26" height="26" rx="6" fill="#111"/><path fill="#62d962" d="M9.5 8.5h3.6v5.6h5.8V8.5h3.6v15h-3.6v-5.8h-5.8v5.8H9.5z"/>',
  proyecto: '<rect x="5" y="3" width="22" height="26" rx="3" fill="#7c8aa5"/>' + GEAR(16, 16.5, '#7c8aa5').replace('translate(16 16.5)', 'translate(16 16.5) scale(1.15)'),
  md: '<rect x="2.5" y="7" width="27" height="18" rx="3" fill="none" stroke="#2f81f7" stroke-width="1.8"/><path fill="none" stroke="#2f81f7" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" d="M7 21V11l3.5 4.3L14 11v10M21 11v7.5M17.8 16l3.2 3.6 3.2-3.6"/>',
  json: '<path fill="none" stroke="#d4a72c" stroke-width="2.2" stroke-linecap="round" d="M12 6c-3 0-2.5 3-2.5 5.5S8 15 6.5 16c1.5 1 3 2 3 4.5S9 26 12 26M20 6c3 0 2.5 3 2.5 5.5S24 15 25.5 16c-1.5 1-3 2-3 4.5S23 26 20 26"/>',
  imagen: '<rect x="3.5" y="6" width="25" height="20" rx="2.5" fill="#dbeafe" stroke="#2f81f7" stroke-width="1.6"/><circle cx="11" cy="12.5" r="2.4" fill="#f2b600"/><path fill="#2da44e" d="M4.6 24.6l7.4-7.4 5 5 3.5-3.5 7 6.9z"/>',
  datos: '<rect x="4" y="6" width="24" height="20" rx="2" fill="#e6f6f4" stroke="#1b9bb0" stroke-width="1.6"/><path stroke="#1b9bb0" stroke-width="1.4" d="M4 12h24M4 18.5h24M12 6v20M20 6v20"/>',
  html: '<path fill="#e34c26" d="M5 4h22l-2 22.5L16 29l-9-2.5z"/><path fill="none" stroke="#fff" stroke-width="1.7" stroke-linejoin="round" d="M21 10.5H11.3l.4 4.5h8.9l-.6 6.2L16 22.4l-4-1.2-.3-2.6"/>',
  css: '<path fill="#2965f1" d="M5 4h22l-2 22.5L16 29l-9-2.5z"/><path fill="none" stroke="#fff" stroke-width="1.7" stroke-linejoin="round" d="M11.2 10.5h9.8l-.4 4.5h-8.9M20.6 15l-.6 6.2L16 22.4l-4-1.2-.3-2.6"/>',
  js: '<rect x="3" y="3" width="26" height="26" rx="3" fill="#f7df1e"/><path fill="none" stroke="#222" stroke-width="2.4" d="M15.2 13v9.6c0 2-2.6 2.7-4.2 1.4M24.4 15c-1-1.8-5.4-1.9-5.4.8 0 3 5.6 1.9 5.6 5.1 0 2.8-4.7 3-6.1.8"/>',
  texto: '<path fill="#f3f4f6" stroke="#8c959f" stroke-width="1.2" d="M8 3.5h10.5L25 10v17.5c0 .6-.4 1-1 1H8c-.6 0-1-.4-1-1v-23c0-.6.4-1 1-1z"/><path stroke="#8c959f" stroke-width="1.5" d="M10.5 14h11M10.5 18h11M10.5 22h7"/>',
};
const EXTENSIONES = { hez: 'hez', hz: 'hez', md: 'md', markdown: 'md', json: 'json', png: 'imagen', jpg: 'imagen', jpeg: 'imagen', gif: 'imagen',
  svg: 'imagen', webp: 'imagen', ico: 'imagen', bmp: 'imagen', csv: 'datos', tsv: 'datos', db: 'datos', sqlite: 'datos', html: 'html', htm: 'html',
  css: 'css', js: 'js', mjs: 'js', txt: 'texto', log: 'texto', ini: 'texto', out: 'texto' };

const svg = (cuerpo) => `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">${cuerpo}</svg>\n`;
function iconos() {
  const lista = {};
  for (const n of Object.keys(CARPETAS)) { lista[n] = carpetaSvg(n, false); lista[n + '-abierta'] = carpetaSvg(n, true); }
  for (const [n, cuerpo] of Object.entries(ARCHIVOS)) lista['archivo-' + n === 'archivo-archivo' ? 'archivo' : 'archivo-' + n] = cuerpo;
  return lista;
}

function escribirSvg(lista) {
  fs.mkdirSync(SALIDA, { recursive: true });
  for (const [n, cuerpo] of Object.entries(lista)) fs.writeFileSync(path.join(SALIDA, n + '.svg'), svg(cuerpo));
  // el tema de iconos de VS Code ("Tema de iconos de archivo: HyperEZ")
  const temaDir = path.join(VSCODE, 'tema');
  fs.mkdirSync(temaDir, { recursive: true });
  for (const f of fs.readdirSync(temaDir)) if (f.endsWith('.svg')) fs.unlinkSync(path.join(temaDir, f));
  const defs = {};
  for (const n of Object.keys(lista)) { fs.copyFileSync(path.join(SALIDA, n + '.svg'), path.join(temaDir, n + '.svg')); defs['_' + n] = { iconPath: './tema/' + n + '.svg' }; }
  const especiales = Object.keys(CARPETAS).filter((n) => n !== 'carpeta');
  const tema = {
    iconDefinitions: defs,
    folder: '_carpeta', folderExpanded: '_carpeta-abierta', rootFolder: '_carpeta', rootFolderExpanded: '_carpeta-abierta',
    file: '_archivo',
    folderNames: Object.fromEntries(especiales.map((n) => [n, '_' + n])),
    folderNamesExpanded: Object.fromEntries(especiales.map((n) => [n, '_' + n + '-abierta'])),
    fileExtensions: Object.fromEntries(Object.entries(EXTENSIONES).map(([e, n]) => [e, '_archivo-' + n])),
    fileNames: { 'proyecto.json': '_archivo-proyecto' },
    languageIds: { hyperez: '_archivo-hez', markdown: '_archivo-md', json: '_archivo-json', jsonc: '_archivo-json', html: '_archivo-html', css: '_archivo-css', javascript: '_archivo-js', plaintext: '_archivo-texto' },
    hidesExplorerArrows: false,
  };
  fs.writeFileSync(path.join(VSCODE, 'hyperez-iconos.json'), JSON.stringify(tema, null, 2) + '\n');
  // lo que necesitan el editor web y HyperEZ Editor: qué carpeta y qué archivo lleva cada icono
  const mapa = { carpetas: especiales, extensiones: EXTENSIONES, nombres: { 'proyecto.json': 'proyecto' }, descripciones: Object.fromEntries(Object.entries(CARPETAS).map(([n, v]) => [n, v[3]])) };
  fs.writeFileSync(path.join(SALIDA, 'tema.json'), JSON.stringify(mapa, null, 2) + '\n');
  // el editor del navegador: todos en un solo SVG (<symbol id="t-...">), que la web mete en editor/index.html
  const sprite = '<svg xmlns="http://www.w3.org/2000/svg" width="0" height="0" style="position:absolute" aria-hidden="true"><defs>' +
    Object.entries(lista).map(([n, c]) => `<symbol id="t-${n}" viewBox="0 0 32 32">${c}</symbol>`).join('') + '</defs></svg>\n';
  fs.writeFileSync(path.join(SALIDA, 'sprite.svg'), sprite);
  console.log(`✓ ${Object.keys(lista).length} iconos en ${path.relative(RAIZ, SALIDA)} y el tema de VS Code (${path.relative(RAIZ, path.join(VSCODE, 'hyperez-iconos.json'))})`);
}

// ---- .ico para HyperEZ Editor: Chrome dibuja cada SVG a cada tamaño (PNG dentro del .ico) ----
async function escribirIco(lista) {
  const { spawn } = require('child_process');
  const os = require('os');
  const chrome = [process.env.CHROME, 'C:/Program Files/Google/Chrome/Application/chrome.exe', 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    '/usr/bin/google-chrome', '/usr/bin/chromium'].filter(Boolean).find((c) => fs.existsSync(c));
  if (!chrome) throw new Error('Hace falta Chrome o Edge para dibujar los .ico');
  const TAMANOS = [16, 20, 24, 32, 40, 48];
  const perfil = fs.mkdtempSync(path.join(os.tmpdir(), 'hz-iconos-'));
  const puerto = 9800 + Math.floor(Math.random() * 100);
  const proc = spawn(chrome, ['--headless=new', '--disable-gpu', `--remote-debugging-port=${puerto}`, `--user-data-dir=${perfil}`, '--window-size=800,800', 'about:blank'], { stdio: 'ignore' });
  const esperar = (ms) => new Promise((r) => setTimeout(r, ms));
  let objetivo = null;
  for (let i = 0; i < 60 && !objetivo; i++) { await esperar(250); try { objetivo = (await (await fetch(`http://127.0.0.1:${puerto}/json`)).json()).find((t) => t.type === 'page'); } catch (e) { /* aún no */ } }
  const ws = new WebSocket(objetivo.webSocketDebuggerUrl);
  await new Promise((r) => { ws.onopen = r; });
  let id = 0;
  const pend = new Map();
  ws.onmessage = (ev) => { const m = JSON.parse(ev.data); if (pend.has(m.id)) { pend.get(m.id)(m.result); pend.delete(m.id); } };
  const cdp = (method, params) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params: params || {} })); });
  await cdp('Emulation.setDefaultBackgroundColorOverride', { color: { r: 0, g: 0, b: 0, a: 0 } });
  await cdp('Emulation.setDeviceMetricsOverride', { width: 800, height: 800, deviceScaleFactor: 1, mobile: false });
  let hechos = 0;
  for (const [n, cuerpo] of Object.entries(lista)) {
    const html = '<!DOCTYPE html><html><body style="margin:0;background:transparent">' +
      TAMANOS.map((t, k) => `<img style="position:absolute;left:${k * 60}px;top:0;width:${t}px;height:${t}px" src="data:image/svg+xml;base64,${Buffer.from(svg(cuerpo)).toString('base64')}">`).join('') + '</body></html>';
    await cdp('Page.navigate', { url: 'data:text/html;base64,' + Buffer.from(html).toString('base64') });
    await esperar(120);
    const pngs = [];
    for (let k = 0; k < TAMANOS.length; k++) {
      const t = TAMANOS[k];
      const r = await cdp('Page.captureScreenshot', { format: 'png', clip: { x: k * 60, y: 0, width: t, height: t, scale: 1 } });
      pngs.push({ t, data: Buffer.from(r.data, 'base64') });
    }
    // .ico con imágenes PNG (Windows Vista o posterior)
    const cab = Buffer.alloc(6 + 16 * pngs.length);
    cab.writeUInt16LE(0, 0); cab.writeUInt16LE(1, 2); cab.writeUInt16LE(pngs.length, 4);
    let off = cab.length;
    pngs.forEach((p, k) => {
      const e = 6 + 16 * k;
      cab.writeUInt8(p.t >= 256 ? 0 : p.t, e); cab.writeUInt8(p.t >= 256 ? 0 : p.t, e + 1);
      cab.writeUInt16LE(1, e + 4); cab.writeUInt16LE(32, e + 6); cab.writeUInt32LE(p.data.length, e + 8); cab.writeUInt32LE(off, e + 12);
      off += p.data.length;
    });
    fs.writeFileSync(path.join(SALIDA, n + '.ico'), Buffer.concat([cab, ...pngs.map((p) => p.data)]));
    hechos++;
  }
  ws.close();
  proc.kill();
  console.log(`✓ ${hechos} archivos .ico (${TAMANOS.join(', ')} px) en ${path.relative(RAIZ, SALIDA)}`);
}

(async () => {
  const lista = iconos();
  escribirSvg(lista);
  if (process.argv.includes('--ico')) await escribirIco(lista);
})().catch((e) => { console.error(e); process.exit(1); });
