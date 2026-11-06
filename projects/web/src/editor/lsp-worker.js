// Servidor de lenguaje de HyperEZ (el mismo de VS Code) en un Worker: errores en vivo, autocompletado, ayuda,
// firmas, ir a la definición y formato. Recibe {wasm: WebAssembly.Module} una vez y después mensajes:
//   {id, lsp: "<mensaje JSON-RPC>"}  → {id, out: [respuestas y notificaciones]}
//   {id, format: "<código>"}        → {id, text} o {id, error}
//   {id, nuevoProyecto: {tipo, nombre}} → {id, archivos: [{path, content}]} (los de hyper new)
//   {carpetas: [...]}                 las carpetas del proyecto (así se encuentra hypersettings/ y se importa de hypercode/)
'use strict';
importScripts('hyperez.js');

let hz = null;
const cola = [];

function atender(m) {
  if (m.lsp !== undefined) {
    let out = [];
    try { out = JSON.parse(hz.lsp(m.lsp)); } catch (e) { out = []; }
    postMessage({ id: m.id, out });
  } else if (m.carpetas) {
    const FS = hz.FS;
    const borrar = (d) => { for (const n of FS.readdir(d)) if (n !== '.' && n !== '..') { borrar(d + '/' + n); FS.rmdir(d + '/' + n); } };
    try { FS.mkdir('/proyecto'); } catch (e) { /* ya está */ }
    try { borrar('/proyecto'); } catch (e) { /* vacía */ }
    for (const c of m.carpetas) {
      let p = '/proyecto';
      for (const s of c.split('/')) { if (!s) continue; p += '/' + s; try { FS.mkdir(p); } catch (e) { /* ya existe */ } }
    }
  } else if (m.nuevoProyecto) {
    const r = hz.nuevo(m.nuevoProyecto.tipo, m.nuevoProyecto.nombre);
    postMessage({ id: m.id, archivos: r ? JSON.parse(r) : null });
  } else if (m.format !== undefined) {
    const r = hz.format(m.format);
    if (r.charCodeAt(0) === 1) postMessage({ id: m.id, error: r.slice(1) });
    else postMessage({ id: m.id, text: r });
  }
}

onmessage = (e) => {
  const m = e.data;
  if (m.wasm) {
    createHyperEZ({
      instantiateWasm: (imports, listo) => { WebAssembly.instantiate(m.wasm, imports).then((inst) => listo(inst)); return {}; },
      print: () => {}, printErr: () => {},
    }).then((mod) => {
      mod.ENV.NO_COLOR = '1';
      hz = { lsp: mod.cwrap('hz_lsp', 'string', ['string']), format: mod.cwrap('hz_format', 'string', ['string']), FS: mod.FS,
        nuevo: mod.cwrap('hz_new_project', 'string', ['string', 'string']),
        version: mod.ccall('hz_version', 'string', [], []) };
      postMessage({ listo: true, version: hz.version });
      while (cola.length) atender(cola.shift());
    }, (err) => postMessage({ fallo: String(err) }));
    return;
  }
  if (hz) atender(m); else cola.push(m);
};
