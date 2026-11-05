// Ejecuta un programa de HyperEZ en un Worker (así ■ Detener puede pararlo aunque sea un bucle infinito).
// Recibe {wasm, files: {ruta: texto}, entry: ruta, stdin: texto}. Envía la salida al momento ({out} y {err}),
// los archivos que el programa crea o cambia ({files}) y, al terminar, {done, code, ms}.
'use strict';
importScripts('hyperez.js');

const RAIZ = '/proyecto';

function crearCarpetas(FS, ruta) {
  const partes = ruta.split('/').slice(0, -1);
  let p = '';
  for (const s of partes) { if (!s) continue; p += '/' + s; try { FS.mkdir(p); } catch (e) { /* ya existe */ } }
}

function recorrer(FS, dir, out) {
  for (const n of FS.readdir(dir)) {
    if (n === '.' || n === '..') continue;
    const p = dir + '/' + n;
    const st = FS.stat(p);
    if (FS.isDir(st.mode)) recorrer(FS, p, out);
    else out.push(p);
  }
  return out;
}

const TEXTO = /\.(hez|hz|txt|md|json|csv|html|htm|css|js|svg|xml|ini|cfg|log|yml|yaml|toml)$/i;

onmessage = (e) => {
  const { wasm, files, entry, stdin } = e.data;
  const entrada = new TextEncoder().encode(stdin || '');
  let pos = 0;
  // la salida se agrupa un poco para no saturar la página con miles de mensajes
  let pendiente = '', pendienteErr = '', ultimo = 0;
  const vaciar = () => {
    if (pendiente) { postMessage({ out: pendiente }); pendiente = ''; }
    if (pendienteErr) { postMessage({ err: pendienteErr }); pendienteErr = ''; }
    ultimo = performance.now();
  };
  const escribir = (t, err) => {
    if (err) { if (pendiente) vaciar(); pendienteErr += t + '\n'; } else { if (pendienteErr) vaciar(); pendiente += t + '\n'; }
    if (performance.now() - ultimo > 30 || pendiente.length + pendienteErr.length > 8192) vaciar();
  };
  createHyperEZ({
    instantiateWasm: (imports, listo) => { WebAssembly.instantiate(wasm, imports).then((inst) => listo(inst)); return {}; },
    print: (t) => escribir(t, false),
    printErr: (t) => escribir(t, true),
    stdin: () => (pos < entrada.length ? entrada[pos++] : null),
  }).then((hz) => {
    hz.ENV.NO_COLOR = '1';
    const FS = hz.FS;
    const antes = new Map();
    for (const [ruta, texto] of Object.entries(files)) {
      const p = RAIZ + ruta;
      crearCarpetas(FS, p);
      if (typeof texto === 'string' && texto.startsWith('data:')) {
        const b64 = texto.slice(texto.indexOf(',') + 1);
        FS.writeFile(p, Uint8Array.from(atob(b64), (c) => c.charCodeAt(0)));
      } else FS.writeFile(p, texto);
      antes.set(p, texto);
    }
    FS.chdir(RAIZ);
    const t0 = performance.now();
    let code = 1;
    try { code = hz.ccall('hz_run', 'number', ['string'], [RAIZ + entry]); }
    catch (err) { escribir('error interno: ' + err, true); }
    const ms = performance.now() - t0;
    vaciar();
    // archivos nuevos o cambiados por el programa (imágenes, CSV, HTML...): vuelven al explorador
    const cambiados = {};
    for (const p of recorrer(FS, RAIZ, [])) {
      const datos = FS.readFile(p);
      if (datos.length > 4 * 1024 * 1024) continue;
      let valor;
      if (TEXTO.test(p)) valor = new TextDecoder().decode(datos);
      else {
        let bin = '';
        for (let i = 0; i < datos.length; i += 0x8000) bin += String.fromCharCode.apply(null, datos.subarray(i, i + 0x8000));
        const tipo = /\.png$/i.test(p) ? 'image/png' : /\.jpe?g$/i.test(p) ? 'image/jpeg' : /\.wav$/i.test(p) ? 'audio/wav' : 'application/octet-stream';
        valor = 'data:' + tipo + ';base64,' + btoa(bin);
      }
      if (antes.get(p) !== valor) cambiados[p.slice(RAIZ.length)] = valor;
    }
    if (Object.keys(cambiados).length) postMessage({ files: cambiados });
    postMessage({ done: true, code, ms });
  }, (err) => postMessage({ err: 'No se pudo cargar HyperEZ: ' + err + '\n', done: true, code: 1, ms: 0 }));
};
