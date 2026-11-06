// HyperEZ Editor en el navegador. El lenguaje es el de verdad, compilado a WebAssembly (hyperez.wasm):
// lsp-worker.js da los errores en vivo, el autocompletado y la ayuda (el servidor de lenguaje de VS Code)
// y run-worker.js ejecuta los programas sin guardar nada. El proyecto se guarda en este navegador.
(function () {
  'use strict';
  const N = window.HZNucleo;
  const VFS = N.VFS;
  const $ = (id) => document.getElementById(id);
  const CLAVE = 'hyperez-editor-web-v1';
  const RAIZ_URI = 'file:///proyecto';
  const ES_CODIGO = /\.(hez|hz)$/i;
  const ES_TEXTO = /\.(hez|hz|txt|md|json|csv|html|htm|css|js|svg|xml|ini|cfg|log|yml|yaml|toml|out)$/i;

  const BIENVENIDA = `// ¡Bienvenido a HyperEZ en el navegador!
// Pulsa F5 (o ▶ Ejecutar) y mira el resultado abajo, en "Salida". No hace falta guardar:
// tu proyecto se queda en este navegador. A la izquierda, el Explorador con las carpetas de HyperEZ:
// hypercode (tus módulos, se importan por su nombre), hypertests (las pruebas) e hypersettings.
from tienda import Objeto, conDescuento

var tienda = [
    Objeto("Espada", 120, "arma"),
    Objeto("Escudo", 80, "defensa"),
    Objeto("Poción", 15, "magia"),
    Objeto("Arco", 95, "arma"),
]

var armas = [o.nombre for o in tienda if o.tipo == "arma"]
print("⚔  Armas: \${armas.join(", ")}")
foreach o in tienda {
    print("   \${o.nombre:<8} \${o.precio |> conDescuento(20):>4} monedas")
}

// Prueba a escribir "tienda[0]." para ver el autocompletado, o a equivocarte en un nombre:
// los errores salen al momento, en español. Ctrl+Mayús+T ejecuta las pruebas de hypertests/.
`;
  const PRUEBAS = `// Pruebas de la tienda: Ejecutar → Ejecutar las pruebas (Ctrl+Mayús+T), o hyper test en tu equipo
from tienda import Objeto, conDescuento

test("el descuento se redondea", () => {
    assertEqual(conDescuento(120, 20), 96)
    assertEqual(conDescuento(15, 20), 12)
})

test("los objetos de 100 monedas o más son caros", () => {
    assert(Objeto("Espada", 120, "arma").caro)
    assert(not Objeto("Poción", 15, "magia").caro)
})
`;
  const PROYECTO_JSON = JSON.stringify({ nombre: 'mi-tienda', version: '0.1.0', descripcion: 'Mi primer proyecto de HyperEZ',
    autor: '', principal: 'main.hez', tipo: 'app' }, null, 2) + '\n';
  const TIENDA = `/// Un objeto de la tienda del juego
class Objeto {
    var nombre: text
    var precio: number
    var tipo: text

    constructor(nombre: text, precio: number, tipo: text) {
        this.nombre = nombre
        this.precio = precio
        this.tipo = tipo
    }

    get caro(): bool { return precio >= 100 }
}

/// El precio con un descuento (en %), redondeado
function conDescuento(precio: number, porcentaje: number): number {
    return round(precio * (1 - porcentaje / 100))
}
`;
  function proyectoInicial() {
    const fs = new VFS();
    fs.write('/main.hez', BIENVENIDA);
    fs.write('/hypercode/tienda.hez', TIENDA);
    fs.write('/hypertests/test_tienda.hez', PRUEBAS);
    fs.write('/hypersettings/proyecto.json', PROYECTO_JSON);
    fs.write('/LEEME.md', '# Mi proyecto\n\nCreado con HyperEZ en el navegador. Descárgalo con *Archivo → Descargar el proyecto (.zip)*\n' +
      'y ejecútalo en tu equipo con `hyper run` (y sus pruebas con `hyper test`).\n\n' +
      '| Carpeta | Para qué |\n|---|---|\n| `hypersettings/` | `proyecto.json`: nombre, versión y programa principal |\n' +
      '| `hypercode/` | Los módulos: se importan por su nombre (`from tienda import Objeto`) |\n| `hypertests/` | Las pruebas |\n');
    return fs;
  }

  // ---------------------------------------------------------------------------
  // Estado (se guarda en localStorage)
  // ---------------------------------------------------------------------------
  let guardado = null;
  try { guardado = JSON.parse(localStorage.getItem(CLAVE) || 'null'); } catch (e) { guardado = null; }
  const estado = Object.assign({
    abiertos: ['/main.hez'], activo: '/main.hez', tema: 'clasico', explorador: true, panel: true, numeros: true,
    tam: 14, anchoExp: 230, altoPanel: 190, carpetasAbiertas: ['/hypercode', '/hypertests'], entrada: '', repetir: false, fuente: '',
  }, guardado || {});
  const fs = guardado && guardado.vfs ? new VFS(guardado.vfs) : proyectoInicial();
  const vistas = new Map(); // ruta → {selStart, selEnd, scrollTop, scrollLeft}
  let temporizadorGuardar = 0;
  function guardar() {
    if (typeof lspCarpetas === 'function') lspCarpetas();
    clearTimeout(temporizadorGuardar);
    temporizadorGuardar = setTimeout(() => {
      try {
        localStorage.setItem(CLAVE, JSON.stringify(Object.assign({}, estado, { vfs: fs.toJSON() })));
      } catch (e) {
        avisoEstado('No queda espacio en el navegador para guardar el proyecto: descárgalo (.zip) y borra archivos grandes.');
      }
    }, 300);
  }

  // ---------------------------------------------------------------------------
  // Elementos
  // ---------------------------------------------------------------------------
  const ta = $('fuente'), pre = $('resaltado'), numeros = $('numeros'), lineaActual = $('linea-actual');
  const sugerencias = $('sugerencias'), firma = $('firma'), info = $('info');
  const salida = $('salida'), problemas = $('problemas');
  let actual = null;      // ruta del archivo abierto en el editor
  let diagnosticos = new Map(); // ruta → [{range, severity, message, code}]

  // ---------------------------------------------------------------------------
  // El motor: WebAssembly en dos Workers
  // ---------------------------------------------------------------------------
  let modulo = null;
  const moduloListo = (async () => {
    try {
      if (WebAssembly.compileStreaming) return await WebAssembly.compileStreaming(fetch('hyperez.wasm'));
    } catch (e) { /* el servidor no da application/wasm: se compila a partir de los bytes */ }
    const r = await fetch('hyperez.wasm');
    return WebAssembly.compile(await r.arrayBuffer());
  })().then((m) => (modulo = m));

  const lsp = { worker: null, id: 0, pendientes: new Map(), listo: false, version: 0, versiones: new Map() };
  function lspEnviar(obj) { // devuelve una promesa con las respuestas
    return new Promise((resolver) => {
      const id = ++lsp.id;
      lsp.pendientes.set(id, resolver);
      lsp.worker.postMessage(Object.assign({ id }, obj));
    });
  }
  let rpcId = 0;
  async function lspPedir(method, params) {
    const id = ++rpcId;
    const out = await lspEnviar({ lsp: JSON.stringify({ jsonrpc: '2.0', id, method, params }) });
    procesarSalidaLsp(out);
    const r = out.find((m) => m.id === id);
    return r ? r.result : null;
  }
  async function lspAvisar(method, params) {
    procesarSalidaLsp(await lspEnviar({ lsp: JSON.stringify({ jsonrpc: '2.0', method, params }) }));
  }
  function procesarSalidaLsp(out) {
    for (const m of out) {
      if (m.method === 'textDocument/publishDiagnostics') {
        const ruta = decodeURIComponent(m.params.uri.slice(RAIZ_URI.length));
        diagnosticos.set(ruta, m.params.diagnostics || []);
        programarDiagnosticos();
      }
    }
  }
  const uri = (ruta) => RAIZ_URI + ruta.split('/').map(encodeURIComponent).join('/');

  async function iniciarLsp() {
    lsp.worker = new Worker('lsp-worker.js');
    lsp.worker.onmessage = (e) => {
      const m = e.data;
      if (m.listo) { estadoMotor('HyperEZ ' + m.version + ' · en el navegador'); return; }
      if (m.fallo) { estadoMotor('No se pudo cargar HyperEZ'); return; }
      const r = lsp.pendientes.get(m.id);
      if (r) { lsp.pendientes.delete(m.id); r(m.out || m); }
    };
    lsp.worker.postMessage({ wasm: await moduloListo });
    await lspPedir('initialize', { processId: null, rootUri: RAIZ_URI, capabilities: {} });
    await lspAvisar('initialized', {});
    lspCarpetas();
    for (const ruta of fs.files.keys()) if (ES_CODIGO.test(ruta)) await lspAbrir(ruta);
    lsp.listo = true;
  }
  // el servidor de lenguaje necesita las carpetas (hypersettings/ marca la raíz del proyecto)
  let carpetasLsp = '';
  function lspCarpetas() {
    if (!lsp.worker) return;
    const lista = [...fs.folders].sort();
    const clave = lista.join('|');
    if (clave === carpetasLsp) return;
    carpetasLsp = clave;
    lsp.worker.postMessage({ carpetas: lista });
  }
  async function lspAbrir(ruta) {
    if (!lsp.worker || lsp.versiones.has(ruta)) return;
    lsp.versiones.set(ruta, 1);
    await lspAvisar('textDocument/didOpen', { textDocument: { uri: uri(ruta), languageId: 'hyperez', version: 1, text: fs.read(ruta) || '' } });
  }
  async function lspCerrar(ruta) {
    if (!lsp.versiones.has(ruta)) return;
    lsp.versiones.delete(ruta);
    diagnosticos.delete(ruta);
    await lspAvisar('textDocument/didClose', { textDocument: { uri: uri(ruta) } });
    programarDiagnosticos();
  }
  let temporizadorCambio = 0;
  function lspCambio(ruta) {
    if (!ES_CODIGO.test(ruta)) return;
    clearTimeout(temporizadorCambio);
    temporizadorCambio = setTimeout(async () => {
      if (!lsp.worker) return;
      if (!lsp.versiones.has(ruta)) { await lspAbrir(ruta); return; }
      const v = lsp.versiones.get(ruta) + 1;
      lsp.versiones.set(ruta, v);
      await lspAvisar('textDocument/didChange', { textDocument: { uri: uri(ruta), version: v }, contentChanges: [{ text: fs.read(ruta) }] });
    }, 220);
  }

  // ---------------------------------------------------------------------------
  // Pintar el código
  // ---------------------------------------------------------------------------
  let medida = { ancho: 7.7, alto: 20, padX: 10, padY: 4 };
  function medir() {
    const cs = getComputedStyle(ta);
    const c = document.createElement('canvas').getContext('2d');
    c.font = cs.fontSize + ' ' + cs.fontFamily;
    medida = { ancho: c.measureText('M'.repeat(50)).width / 50, alto: parseFloat(cs.lineHeight) || 20,
      padX: parseFloat(cs.paddingLeft) || 10, padY: parseFloat(cs.paddingTop) || 4 };
  }
  let inicios = [0];
  let pendientePintar = false;
  function programarPintar() {
    if (pendientePintar) return;
    pendientePintar = true;
    requestAnimationFrame(() => { pendientePintar = false; pintar(); });
  }
  function marcasDe(ruta, texto) {
    const st = N.lineStarts(texto);
    return (diagnosticos.get(ruta) || []).map((d) => {
      let s = N.posToOffset(st, d.range.start, texto.length);
      let e = N.posToOffset(st, d.range.end, texto.length);
      if (e <= s) e = Math.min(texto.length, s + 1);
      return { s, e, sev: d.severity === 1 ? 1 : 2, d };
    });
  }
  function pintar() {
    if (actual === null) return;
    const texto = ta.value;
    inicios = N.lineStarts(texto);
    const marcas = ES_CODIGO.test(actual) ? marcasDe(actual, texto) : [];
    if (ES_CODIGO.test(actual)) {
      const par = ta.selectionStart === ta.selectionEnd ? N.matchBracket(texto, ta.selectionStart) : null;
      if (par) marcas.push({ s: par[0], e: par[0] + 1, sev: 3 }, { s: par[1], e: par[1] + 1, sev: 3 });
      pre.innerHTML = N.highlightHtml(texto, marcas);
    } else pre.textContent = texto + '\n';
    // números de línea (con los de las líneas con errores en rojo, como en la aplicación)
    const conError = new Map();
    for (const m of marcas) if (m.sev !== 3) {
      const l = N.offsetToPos(inicios, m.s).line;
      if (!conError.has(l) || m.sev === 1) conError.set(l, m.sev);
    }
    const lineaCursor = N.offsetToPos(inicios, ta.selectionStart).line;
    let html = '';
    for (let i = 0; i < inicios.length; i++) {
      const c = (i === lineaCursor ? 'actual' : '') + (conError.has(i) ? (conError.get(i) === 1 ? ' con-error' : ' con-aviso') : '');
      html += c ? `<div class="${c.trim()}">${i + 1}</div>` : `<div>${i + 1}</div>`;
    }
    numeros.innerHTML = html;
    sincronizarScroll();
    actualizarEstado();
  }
  function sincronizarScroll() {
    pre.scrollTop = ta.scrollTop;
    pre.scrollLeft = ta.scrollLeft;
    numeros.style.transform = `translateY(${-ta.scrollTop}px)`;
    const l = N.offsetToPos(inicios, ta.selectionStart).line;
    lineaActual.style.top = (medida.padY + l * medida.alto - ta.scrollTop) + 'px';
  }
  ta.addEventListener('scroll', () => { sincronizarScroll(); ocultarEmergentes(); });

  // coordenadas (en el área de código) de una posición del texto
  function columnaVisual(off) {
    const p = N.offsetToPos(inicios, off);
    const linea = ta.value.slice(inicios[p.line], off);
    let col = 0;
    for (const ch of linea) col = ch === '\t' ? col + 4 - (col % 4) : col + 1;
    return { line: p.line, col };
  }
  function xyDe(off) {
    const v = columnaVisual(off);
    return { x: medida.padX + v.col * medida.ancho - ta.scrollLeft, y: medida.padY + v.line * medida.alto - ta.scrollTop };
  }
  function posicionDelRaton(ev) {
    const r = ta.getBoundingClientRect();
    const line = Math.floor((ev.clientY - r.top - medida.padY + ta.scrollTop) / medida.alto);
    if (line < 0 || line >= inicios.length) return -1;
    const colV = Math.floor((ev.clientX - r.left - medida.padX + ta.scrollLeft) / medida.ancho);
    if (colV < 0) return -1;
    const ini = inicios[line], fin = line + 1 < inicios.length ? inicios[line + 1] - 1 : ta.value.length;
    let col = 0;
    for (let i = ini; i < fin; i++) {
      const ch = ta.value[i];
      const w = ch === '\t' ? 4 - (col % 4) : 1;
      if (col + w > colV) return i;
      col += w;
    }
    return -1;
  }

  // ---------------------------------------------------------------------------
  // Archivos abiertos y pestañas
  // ---------------------------------------------------------------------------
  function abrir(ruta, seleccion) {
    ruta = VFS.norm(ruta);
    if (!fs.files.has(ruta)) return;
    if (actual !== null) guardarVista();
    if (!estado.abiertos.includes(ruta)) estado.abiertos.push(ruta);
    estado.activo = ruta;
    actual = ruta;
    // las carpetas que lo contienen, abiertas
    for (let d = VFS.dir(ruta); d !== '/'; d = VFS.dir(d)) if (!estado.carpetasAbiertas.includes(d)) estado.carpetasAbiertas.push(d);
    const contenido = fs.read(ruta) || '';
    const imagen = contenido.startsWith('data:image/');
    $('vista-imagen').hidden = !imagen;
    $('codigo').hidden = imagen;
    $('medianil').hidden = imagen;
    if (imagen) {
      $('imagen').src = contenido;
    } else {
      ta.value = contenido.startsWith('data:') ? '(archivo binario)' : contenido;
      ta.readOnly = contenido.startsWith('data:');
      const v = vistas.get(ruta);
      if (seleccion) ta.setSelectionRange(seleccion[0], seleccion[1]);
      else if (v) ta.setSelectionRange(v.selStart, v.selEnd);
      else ta.setSelectionRange(0, 0);
      pintar();
      if (v && !seleccion) { ta.scrollTop = v.scrollTop; ta.scrollLeft = v.scrollLeft; }
      if (seleccion) mostrarSeleccion();
      sincronizarScroll();
      ta.focus();
    }
    if (ES_CODIGO.test(ruta)) lspAbrir(ruta);
    $('titulo-archivo').textContent = ruta.slice(1);
    document.title = VFS.name(ruta) + ' — HyperEZ Editor en el navegador';
    pintarPestanas();
    pintarArbol();
    guardar();
  }
  function guardarVista() {
    if (actual === null || $('codigo').hidden) return;
    vistas.set(actual, { selStart: ta.selectionStart, selEnd: ta.selectionEnd, scrollTop: ta.scrollTop, scrollLeft: ta.scrollLeft });
  }
  function cerrarPestana(ruta) {
    const i = estado.abiertos.indexOf(ruta);
    if (i < 0) return;
    estado.abiertos.splice(i, 1);
    if (actual === ruta) {
      const otro = estado.abiertos[Math.min(i, estado.abiertos.length - 1)];
      if (otro) abrir(otro);
      else { actual = null; ta.value = ''; pre.textContent = ''; numeros.innerHTML = ''; $('titulo-archivo').textContent = ''; pintarPestanas(); pintarArbol(); }
    } else pintarPestanas();
    guardar();
  }
  // los iconos de HyperEZ (los mete la web en index.html: assets/icons/tema); si no están, los de siempre
  const TEMA = window.HZ_ICONOS || null;
  function icono(ruta, esCarpeta, abierta) {
    if (TEMA) {
      const nombre = VFS.name(ruta).toLowerCase();
      let t;
      if (esCarpeta) t = (TEMA.carpetas.includes(nombre) ? nombre : 'carpeta') + (abierta ? '-abierta' : '');
      else if (TEMA.nombres[nombre]) t = 'archivo-' + TEMA.nombres[nombre];
      else {
        const ext = (/\.([^.]+)$/.exec(nombre) || [])[1];
        t = ext && TEMA.extensiones[ext] ? 'archivo-' + TEMA.extensiones[ext] : 'archivo';
      }
      return `<svg class="ico tema"><use href="#t-${t}"/></svg>`;
    }
    let id = 'i-archivo';
    if (esCarpeta) id = abierta ? 'i-carpeta-abierta' : 'i-carpeta';
    else if (ES_CODIGO.test(ruta)) id = 'i-hez';
    else if (/\.md$/i.test(ruta)) id = 'i-md';
    else if (/\.json$/i.test(ruta)) id = 'i-json';
    else if (/\.(png|jpe?g|gif|svg|webp)$/i.test(ruta)) id = 'i-imagen';
    return `<svg class="ico"><use href="#${id}"/></svg>`;
  }
  const escHtml = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  function erroresDe(ruta) { return (diagnosticos.get(ruta) || []).filter((d) => d.severity === 1).length; }
  function pintarPestanas() {
    const cont = $('pestanas');
    cont.innerHTML = estado.abiertos.map((r) =>
      `<div class="pestana${r === actual ? ' activa' : ''}${erroresDe(r) ? ' con-error' : ''}" data-ruta="${escHtml(r)}" title="${escHtml(r.slice(1))}" role="tab">` +
      `${icono(r)}<span class="nombre">${escHtml(VFS.name(r))}</span><button class="cerrar" title="Cerrar (Ctrl+W)">✕</button></div>`).join('');
  }
  $('pestanas').addEventListener('mousedown', (e) => {
    const p = e.target.closest('.pestana');
    if (!p) return;
    if (e.button === 1 || e.target.classList.contains('cerrar')) { e.preventDefault(); cerrarPestana(p.dataset.ruta); return; }
    if (e.button === 0) abrir(p.dataset.ruta);
  });

  // ---------------------------------------------------------------------------
  // Explorador
  // ---------------------------------------------------------------------------
  const arbol = $('arbol');
  let seleccionArbol = null;
  let nombrando = false; // hay un nombre a medio escribir en el árbol: no repintarlo (lo borraría)
  function pintarArbol() {
    if (nombrando) return;
    const filas = [];
    const recorrer = (dir, nivel) => {
      for (const k of fs.list(dir)) {
        const abierta = k.dir && estado.carpetasAbiertas.includes(k.path);
        const errores = k.dir ? 0 : erroresDe(k.path);
        filas.push(`<div class="nodo${k.path === (seleccionArbol || actual) ? ' sel' : ''}${abierta ? ' abierta' : ''}" data-ruta="${escHtml(k.path)}"` +
          ` data-dir="${k.dir ? 1 : 0}" style="padding-left:${8 + nivel * 14}px" draggable="true" role="treeitem">` +
          `<svg class="flecha${k.dir ? '' : ' oculta'}"><use href="#i-flecha"/></svg>${icono(k.path, k.dir, abierta)}` +
          `<span class="nombre">${escHtml(VFS.name(k.path))}</span>${errores ? `<span class="marca-error">${errores}</span>` : ''}</div>`);
        if (abierta) recorrer(k.path, nivel + 1);
      }
    };
    recorrer('/', 0);
    arbol.innerHTML = filas.join('') || '<div class="exp-pie">El proyecto está vacío: crea un archivo.</div>';
  }
  function carpetaDestino() {
    const sel = seleccionArbol || actual;
    if (!sel) return '/';
    return fs.isDir(sel) ? sel : VFS.dir(sel);
  }
  arbol.addEventListener('click', (e) => {
    const n = e.target.closest('.nodo');
    if (!n || n.querySelector('input')) return;
    const ruta = n.dataset.ruta;
    seleccionArbol = ruta;
    if (n.dataset.dir === '1') {
      const i = estado.carpetasAbiertas.indexOf(ruta);
      if (i >= 0) estado.carpetasAbiertas.splice(i, 1); else estado.carpetasAbiertas.push(ruta);
      pintarArbol();
      guardar();
    } else { seleccionArbol = null; abrir(ruta); }
  });
  arbol.addEventListener('keydown', (e) => {
    const sel = seleccionArbol || actual;
    if (!sel) return;
    if (e.key === 'F2') { e.preventDefault(); renombrar(sel); }
    else if (e.key === 'Delete') { e.preventDefault(); eliminar(sel); }
  });
  arbol.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const n = e.target.closest('.nodo');
    const ruta = n ? n.dataset.ruta : null;
    seleccionArbol = ruta;
    pintarArbol();
    const opciones = [
      ['Nuevo archivo', () => nuevoArchivo(ruta ? (fs.isDir(ruta) ? ruta : VFS.dir(ruta)) : '/')],
      ['Nueva carpeta', () => nuevaCarpeta(ruta ? (fs.isDir(ruta) ? ruta : VFS.dir(ruta)) : '/')],
    ];
    if (ruta) {
      opciones.push(null);
      if (!fs.isDir(ruta)) {
        opciones.push(['Abrir', () => abrir(ruta)]);
        if (ES_CODIGO.test(ruta)) opciones.push(['Ejecutar', () => { abrir(ruta); ejecutar(); }]);
        opciones.push(['Duplicar', () => duplicar(ruta)], ['Descargar', () => descargarArchivo(ruta)]);
      }
      opciones.push(['Renombrar…', () => renombrar(ruta), 'F2'], ['Eliminar', () => eliminar(ruta), 'Supr'], null,
        ['Copiar la ruta', () => navigator.clipboard && navigator.clipboard.writeText(ruta.slice(1))]);
    }
    menuContextual(e.clientX, e.clientY, opciones);
  });
  // escribir un nombre dentro del árbol (archivo nuevo, carpeta nueva o renombrar)
  function pedirNombre(dir, inicial, alAceptar, rutaExistente) {
    pintarArbol();
    let fila;
    if (rutaExistente) fila = arbol.querySelector(`.nodo[data-ruta="${CSS.escape(rutaExistente)}"]`);
    if (!fila) {
      fila = document.createElement('div');
      fila.className = 'nodo';
      const nivel = dir === '/' ? 0 : dir.split('/').length - 1;
      fila.style.paddingLeft = (8 + nivel * 14) + 'px';
      const padre = arbol.querySelector(`.nodo[data-ruta="${CSS.escape(dir)}"]`);
      if (padre) padre.after(fila); else arbol.prepend(fila);
    }
    fila.innerHTML = '';
    nombrando = true;
    const entrada = document.createElement('input');
    entrada.value = inicial;
    fila.appendChild(entrada);
    entrada.focus();
    const punto = inicial.lastIndexOf('.');
    entrada.setSelectionRange(0, punto > 0 ? punto : inicial.length);
    let hecho = false;
    const terminar = (aceptar) => {
      if (hecho) return;
      hecho = true;
      nombrando = false;
      const nombre = entrada.value.trim();
      if (aceptar && nombre && nombre !== inicial || aceptar && !rutaExistente && nombre) {
        if (!VFS.validName(nombre)) { avisoEstado('Nombre no válido: ' + nombre); pintarArbol(); return; }
        try { alAceptar(nombre); } catch (err) { avisoEstado(err.message); }
      }
      pintarArbol();
    };
    entrada.addEventListener('keydown', (e) => {
      e.stopPropagation();
      if (e.key === 'Enter') terminar(true);
      else if (e.key === 'Escape') terminar(false);
    });
    entrada.addEventListener('blur', () => terminar(true));
  }
  function unir(dir, nombre) { return VFS.norm((dir === '/' ? '' : dir) + '/' + nombre); }
  function nuevoArchivo(dir) {
    dir = dir || carpetaDestino();
    if (!estado.explorador) { estado.explorador = true; aplicarVista(); }
    if (dir !== '/' && !estado.carpetasAbiertas.includes(dir)) estado.carpetasAbiertas.push(dir);
    pedirNombre(dir, fs.freeName(dir, 'nuevo', '.hez'), (nombre) => {
      if (!/\.[a-z0-9]+$/i.test(nombre)) nombre += '.hez';
      const ruta = unir(dir, nombre);
      if (fs.exists(ruta)) throw new Error('Ya existe ' + nombre);
      fs.write(ruta, ES_CODIGO.test(ruta) ? '// ' + nombre + '\nprint("¡Hola desde ' + nombre + '!")\n' : '');
      abrir(ruta);
    });
  }
  function nuevaCarpeta(dir) {
    dir = dir || carpetaDestino();
    if (!estado.explorador) { estado.explorador = true; aplicarVista(); }
    if (dir !== '/' && !estado.carpetasAbiertas.includes(dir)) estado.carpetasAbiertas.push(dir);
    pedirNombre(dir, fs.freeName(dir, 'carpeta', ''), (nombre) => {
      const ruta = unir(dir, nombre);
      if (fs.exists(ruta)) throw new Error('Ya existe ' + nombre);
      fs.mkdir(ruta);
      estado.carpetasAbiertas.push(ruta);
      seleccionArbol = ruta;
      guardar();
    });
  }
  function mover(desde, hasta) {
    const movidos = fs.move(desde, hasta);
    for (const [a, b] of movidos) {
      const i = estado.abiertos.indexOf(a);
      if (i >= 0) estado.abiertos[i] = b;
      if (actual === a) actual = b;
      if (vistas.has(a)) { vistas.set(b, vistas.get(a)); vistas.delete(a); }
      if (ES_CODIGO.test(a)) lspCerrar(a);
      if (ES_CODIGO.test(b)) lspAbrir(b);
    }
    estado.carpetasAbiertas = estado.carpetasAbiertas.map((c) => (c === desde || c.startsWith(desde + '/') ? hasta + c.slice(desde.length) : c));
    if (actual) estado.activo = actual;
    pintarPestanas();
    pintarArbol();
    if (actual) $('titulo-archivo').textContent = actual.slice(1);
    guardar();
  }
  function renombrar(ruta) {
    pedirNombre(VFS.dir(ruta), VFS.name(ruta), (nombre) => mover(ruta, unir(VFS.dir(ruta), nombre)), ruta);
  }
  function eliminar(ruta) {
    const esCarpeta = fs.isDir(ruta);
    confirmar(`¿Eliminar ${esCarpeta ? 'la carpeta' : ''} «${VFS.name(ruta)}»${esCarpeta ? ' y todo lo que tiene dentro' : ''}?`, 'Eliminar', () => {
      const afectados = [...fs.files.keys()].filter((f) => f === ruta || f.startsWith(ruta + '/'));
      fs.remove(ruta);
      for (const f of afectados) { lspCerrar(f); if (estado.abiertos.includes(f)) cerrarPestana(f); }
      seleccionArbol = null;
      pintarArbol();
      guardar();
    });
  }
  function duplicar(ruta) {
    const ext = (/\.[^.]+$/.exec(ruta) || [''])[0];
    const base = VFS.name(ruta).slice(0, VFS.name(ruta).length - ext.length) + ' copia';
    const nueva = unir(VFS.dir(ruta), fs.freeName(VFS.dir(ruta), base, ext));
    fs.write(nueva, fs.read(ruta));
    abrir(nueva);
  }
  // arrastrar dentro del árbol (mover) y desde el equipo (añadir archivos)
  let arrastrando = null;
  arbol.addEventListener('dragstart', (e) => {
    const n = e.target.closest('.nodo');
    if (!n) return;
    arrastrando = n.dataset.ruta;
    e.dataTransfer.effectAllowed = 'move';
    e.dataTransfer.setData('text/plain', arrastrando);
  });
  arbol.addEventListener('dragover', (e) => {
    e.preventDefault();
    arbol.querySelectorAll('.soltar').forEach((x) => x.classList.remove('soltar'));
    const n = e.target.closest('.nodo');
    (n && n.dataset.dir === '1' ? n : arbol).classList.add('soltar');
  });
  arbol.addEventListener('dragleave', () => arbol.querySelectorAll('.soltar').forEach((x) => x.classList.remove('soltar')));
  arbol.addEventListener('drop', (e) => {
    e.preventDefault();
    arbol.classList.remove('soltar');
    arbol.querySelectorAll('.soltar').forEach((x) => x.classList.remove('soltar'));
    const n = e.target.closest('.nodo');
    const dir = n ? (n.dataset.dir === '1' ? n.dataset.ruta : VFS.dir(n.dataset.ruta)) : '/';
    if (e.dataTransfer.files && e.dataTransfer.files.length) { anadirArchivos(e.dataTransfer.files, dir); return; }
    if (arrastrando && arrastrando !== dir && VFS.dir(arrastrando) !== dir) {
      try { mover(arrastrando, unir(dir, VFS.name(arrastrando))); } catch (err) { avisoEstado(err.message); }
    }
    arrastrando = null;
  });
  function anadirArchivos(lista, dir) {
    let ultimo = null;
    const pendientes = [...lista].map((f) => new Promise((resolver) => {
      const lector = new FileReader();
      lector.onload = () => {
        const ruta = unir(dir, f.name);
        fs.write(ruta, lector.result);
        if (ES_CODIGO.test(ruta)) { lspCerrar(ruta).then(() => lspAbrir(ruta)); ultimo = ruta; }
        resolver();
      };
      if (ES_TEXTO.test(f.name)) lector.readAsText(f); else lector.readAsDataURL(f);
    }));
    Promise.all(pendientes).then(() => { pintarArbol(); guardar(); if (ultimo) abrir(ultimo); });
  }

  // ---------------------------------------------------------------------------
  // Escribir: parejas, sangría, comentarios, autocompletado
  // ---------------------------------------------------------------------------
  function aplicar(c) {
    ta.focus();
    ta.setSelectionRange(c.from, c.to);
    let hecho = false;
    try {
      hecho = c.insert === '' ? (c.from === c.to || document.execCommand('delete')) : document.execCommand('insertText', false, c.insert);
    } catch (e) { hecho = false; }
    if (!hecho) { // sin execCommand (se pierde el deshacer de este cambio)
      ta.setRangeText(c.insert, c.from, c.to, 'end');
      ta.dispatchEvent(new Event('input'));
    }
    ta.setSelectionRange(c.selStart, c.selEnd);
  }
  ta.addEventListener('input', (e) => {
    if (actual === null || ta.readOnly) return;
    fs.write(actual, ta.value);
    programarPintar();
    lspCambio(actual);
    guardar();
    repetirSiToca();
    if (e.inputType === 'insertText' && e.data) {
      if (e.data === '.' || /[\p{L}_]/u.test(e.data)) programarSugerencias(e.data === '.');
      else ocultarSugerencias();
      if (e.data === '(' || e.data === ',') pedirFirma();
      else if (e.data === ')') firma.hidden = true;
    } else if (e.inputType && e.inputType.startsWith('delete')) {
      if (!sugerencias.hidden) programarSugerencias(false);
    }
  });
  let sobrescribir = false;
  $('estado-modo').title = 'Insert: insertar (INS) o sobrescribir (SOB)';
  $('estado-modo').addEventListener('click', () => { sobrescribir = !sobrescribir; $('estado-modo').textContent = sobrescribir ? 'SOB' : 'INS'; ta.focus(); });
  // Ctrl+rueda: tamaño de la letra (como en la aplicación)
  $('editor').addEventListener('wheel', (e) => {
    if (!e.ctrlKey) return;
    e.preventDefault();
    comando(e.deltaY < 0 ? 'zoom-mas' : 'zoom-menos');
  }, { passive: false });
  ta.addEventListener('keydown', (e) => {
    if (!sugerencias.hidden && teclaSugerencias(e)) return;
    const a = ta.selectionStart, b = ta.selectionEnd, texto = ta.value;
    const ctrl = e.ctrlKey || e.metaKey;
    if (ctrl && e.key === ' ') { e.preventDefault(); mostrarSugerencias(true); return; }
    if (e.key === 'Escape') { ocultarEmergentes(); return; }
    if (ta.readOnly) return;
    if (e.key === 'Tab' && !ctrl) {
      e.preventDefault();
      const varias = texto.slice(a, b).includes('\n');
      if (e.shiftKey || varias) aplicar(N.indentLines(texto, a, b, e.shiftKey));
      else {
        const col = columnaVisual(a).col;
        const espacios = ' '.repeat(4 - (col % 4));
        aplicar({ from: a, to: b, insert: espacios, selStart: a + espacios.length, selEnd: a + espacios.length });
      }
      return;
    }
    if (e.key === 'Enter' && !ctrl && !e.altKey) {
      e.preventDefault();
      aplicar(N.enterIndent(texto, a, b));
      firma.hidden = true;
      return;
    }
    if (e.key === 'Backspace' && !ctrl) {
      const c = N.autoPair(texto, a, b, 'Backspace');
      if (c) { e.preventDefault(); aplicar(c); return; }
      // borrar una sangría de 4 espacios de una vez
      if (a === b) {
        const ls = texto.lastIndexOf('\n', a - 1) + 1;
        const antes = texto.slice(ls, a);
        if (antes.length >= 4 && /^ +$/.test(antes) && antes.length % 4 === 0) {
          e.preventDefault();
          aplicar({ from: a - 4, to: a, insert: '', selStart: a - 4, selEnd: a - 4 });
          return;
        }
      }
    }
    if (e.key === 'Insert' && !ctrl && !e.shiftKey) { e.preventDefault(); sobrescribir = !sobrescribir; $('estado-modo').textContent = sobrescribir ? 'SOB' : 'INS'; return; }
    if (sobrescribir && e.key.length === 1 && !ctrl && !e.altKey && a === b && a < texto.length && texto[a] !== '\n') {
      e.preventDefault();
      aplicar({ from: a, to: a + 1, insert: e.key, selStart: a + 1, selEnd: a + 1 });
      return;
    }
    if (e.key.length === 1 && !ctrl && !e.altKey) {
      const c = N.outdentOnClose(texto, a, e.key) || N.autoPair(texto, a, b, e.key);
      if (c) {
        e.preventDefault();
        aplicar(c);
        if (e.key === '(') pedirFirma();
        if (e.key === ')') firma.hidden = true;
      }
    }
  });
  ta.addEventListener('keyup', (e) => { if (/^Arrow|Home|End|Page/.test(e.key)) { programarPintar(); firma.hidden = firma.hidden || /^Arrow(Up|Down)/.test(e.key); } });
  ta.addEventListener('mouseup', () => programarPintar());
  ta.addEventListener('select', () => programarPintar());
  document.addEventListener('selectionchange', () => { if (document.activeElement === ta) programarPintar(); });
  // Ctrl+clic: ir a la definición
  ta.addEventListener('mousedown', (e) => { if ((e.ctrlKey || e.metaKey) && e.button === 0) { const off = posicionDelRaton(e); if (off >= 0) { e.preventDefault(); irADefinicion(off); } } });

  // ---- autocompletado ----
  let listaSug = [], indiceSug = 0, temporizadorSug = 0, inicioPalabra = 0, peticionSug = 0;
  function programarSugerencias(inmediato) {
    clearTimeout(temporizadorSug);
    temporizadorSug = setTimeout(() => mostrarSugerencias(false), inmediato ? 30 : 120);
  }
  const ICONO_TIPO = { 2: 'm', 3: 'ƒ', 5: '▪', 6: 'x', 7: 'C', 9: '{}', 10: 'p', 13: 'E', 14: 'k', 15: '✂', 21: 'c', 22: 'S', 25: 'T' };
  async function mostrarSugerencias(forzar) {
    if (!lsp.listo || actual === null || !ES_CODIGO.test(actual)) return;
    const off = ta.selectionStart;
    const { start, word } = N.wordBefore(ta.value, off);
    const trasPunto = ta.value[start - 1] === '.';
    if (!forzar && !word && !trasPunto) { ocultarSugerencias(); return; }
    const yo = ++peticionSug;
    // el servidor tiene que ver el texto de ahora mismo
    clearTimeout(temporizadorCambio);
    const v = (lsp.versiones.get(actual) || 1) + 1;
    lsp.versiones.set(actual, v);
    await lspAvisar('textDocument/didChange', { textDocument: { uri: uri(actual), version: v }, contentChanges: [{ text: ta.value }] });
    const pos = N.offsetToPos(N.lineStarts(ta.value), off);
    const r = await lspPedir('textDocument/completion', { textDocument: { uri: uri(actual) }, position: pos });
    if (yo !== peticionSug || ta.selectionStart !== off) return;
    let items = (r && (r.items || r)) || [];
    const w = word.toLowerCase();
    if (w) {
      items = items.filter((i) => i.label.toLowerCase().includes(w));
      items.sort((x, y) => (y.label.toLowerCase().startsWith(w) - x.label.toLowerCase().startsWith(w)) ||
        (x.sortText || x.label).localeCompare(y.sortText || y.label));
    }
    if (!items.length || (items.length === 1 && items[0].label === word)) { ocultarSugerencias(); return; }
    listaSug = items.slice(0, 200);
    indiceSug = 0;
    inicioPalabra = start;
    sugerencias.innerHTML = listaSug.map((i, k) =>
      `<div class="sugerencia${k === 0 ? ' sel' : ''}" data-k="${k}" role="option"><span class="tipo k-${i.kind || 0}">${ICONO_TIPO[i.kind] || '·'}</span>` +
      `<span>${escHtml(i.label)}</span><span class="det">${escHtml(i.detail || '')}</span></div>`).join('');
    const p = xyDe(start);
    sugerencias.style.left = Math.max(0, p.x - 22) + 'px';
    sugerencias.style.top = (p.y + medida.alto + 2) + 'px';
    sugerencias.hidden = false;
    const caja = $('codigo').getBoundingClientRect();
    if (p.y + medida.alto + 230 > caja.height) sugerencias.style.top = Math.max(0, p.y - Math.min(220, listaSug.length * 20) - 4) + 'px';
  }
  function ocultarSugerencias() { sugerencias.hidden = true; listaSug = []; }
  function marcarSugerencia(k) {
    indiceSug = (k + listaSug.length) % listaSug.length;
    sugerencias.querySelectorAll('.sugerencia').forEach((el, i) => el.classList.toggle('sel', i === indiceSug));
    const el = sugerencias.children[indiceSug];
    if (el) el.scrollIntoView({ block: 'nearest' });
  }
  function teclaSugerencias(e) {
    if (e.key === 'ArrowDown') { e.preventDefault(); marcarSugerencia(indiceSug + 1); return true; }
    if (e.key === 'ArrowUp') { e.preventDefault(); marcarSugerencia(indiceSug - 1); return true; }
    if (e.key === 'PageDown') { e.preventDefault(); marcarSugerencia(Math.min(listaSug.length - 1, indiceSug + 8)); return true; }
    if (e.key === 'Enter' || e.key === 'Tab') { e.preventDefault(); aceptarSugerencia(listaSug[indiceSug]); return true; }
    if (e.key === 'Escape') { e.preventDefault(); ocultarSugerencias(); return true; }
    return false;
  }
  function aceptarSugerencia(item) {
    if (!item) return;
    let texto = item.insertText || item.label;
    let cursor = -1;
    if (item.insertTextFormat === 2) { // fragmento: ${1:nombre} → nombre, $0 → cursor
      texto = texto.replace(/\$\{(\d+):([^}]*)\}/g, '$2').replace(/\$\{\d+\}/g, '');
      cursor = texto.indexOf('$0');
      texto = texto.replace(/\$\d+/g, '');
    }
    const fin = ta.selectionStart;
    const nuevoCursor = inicioPalabra + (cursor >= 0 ? cursor : texto.length);
    aplicar({ from: inicioPalabra, to: fin, insert: texto, selStart: nuevoCursor, selEnd: nuevoCursor });
    ocultarSugerencias();
    if (texto.endsWith('(') || texto.endsWith('()')) pedirFirma();
  }
  sugerencias.addEventListener('mousedown', (e) => {
    const el = e.target.closest('.sugerencia');
    if (el) { e.preventDefault(); aceptarSugerencia(listaSug[+el.dataset.k]); }
  });

  // ---- ayuda de parámetros ----
  async function pedirFirma() {
    if (!lsp.listo || !ES_CODIGO.test(actual || '')) return;
    clearTimeout(temporizadorCambio);
    const v = (lsp.versiones.get(actual) || 1) + 1;
    lsp.versiones.set(actual, v);
    await lspAvisar('textDocument/didChange', { textDocument: { uri: uri(actual), version: v }, contentChanges: [{ text: ta.value }] });
    const off = ta.selectionStart;
    const r = await lspPedir('textDocument/signatureHelp', { textDocument: { uri: uri(actual) }, position: N.offsetToPos(N.lineStarts(ta.value), off) });
    if (!r || !r.signatures || !r.signatures.length) { firma.hidden = true; return; }
    const s = r.signatures[r.activeSignature || 0];
    const activo = r.activeParameter !== undefined ? r.activeParameter : s.activeParameter;
    let etiqueta = escHtml(s.label);
    const par = s.parameters && s.parameters[activo];
    if (par) {
      const t = typeof par.label === 'string' ? par.label : s.label.slice(par.label[0], par.label[1]);
      const i = s.label.indexOf(t);
      if (i >= 0) etiqueta = escHtml(s.label.slice(0, i)) + '<b>' + escHtml(t) + '</b>' + escHtml(s.label.slice(i + t.length));
    }
    const doc = s.documentation ? '\n' + escHtml(typeof s.documentation === 'string' ? s.documentation : s.documentation.value) : '';
    firma.innerHTML = `<code>${etiqueta}</code>${doc}`;
    const p = xyDe(off);
    firma.style.left = Math.max(0, p.x - 20) + 'px';
    firma.style.top = Math.max(0, p.y - firma.offsetHeight - 4) + 'px';
    firma.hidden = false;
    firma.style.top = Math.max(0, p.y - firma.offsetHeight - 4) + 'px';
  }

  // ---- información al pasar el ratón ----
  let temporizadorInfo = 0, ultimaInfo = -1;
  ta.addEventListener('mousemove', (e) => {
    clearTimeout(temporizadorInfo);
    const off = posicionDelRaton(e);
    if (off !== ultimaInfo) info.hidden = true;
    if (off < 0 || actual === null || !ES_CODIGO.test(actual)) return;
    temporizadorInfo = setTimeout(() => mostrarInfo(off, e), 420);
  });
  ta.addEventListener('mouseleave', () => { clearTimeout(temporizadorInfo); info.hidden = true; });
  async function mostrarInfo(off, ev) {
    ultimaInfo = off;
    const marcas = marcasDe(actual, ta.value).filter((m) => m.s <= off && m.e > off);
    let html = marcas.map((m) => `<span class="${m.sev === 1 ? 'info-error' : ''}">${m.sev === 1 ? 'Error' : 'Aviso'}${m.d.code ? ' ' + escHtml(m.d.code) : ''}: </span>${escHtml(m.d.message)}`).join('\n');
    if (lsp.listo) {
      const r = await lspPedir('textDocument/hover', { textDocument: { uri: uri(actual) }, position: N.offsetToPos(inicios, off) });
      if (ultimaInfo !== off) return;
      if (r && r.contents) {
        let md = typeof r.contents === 'string' ? r.contents : r.contents.value || (Array.isArray(r.contents) ? r.contents.map((c) => c.value || c).join('\n') : '');
        md = escHtml(md).replace(/```[a-z]*\n?([\s\S]*?)```/g, '<code>$1</code>').replace(/`([^`]+)`/g, '<code>$1</code>').trim();
        if (md) html = (html ? html + '\n\n' : '') + md;
      }
    }
    if (!html) return;
    info.innerHTML = html;
    const r = $('codigo').getBoundingClientRect();
    info.style.left = Math.max(0, Math.min(ev.clientX - r.left, r.width - 380)) + 'px';
    info.hidden = false;
    const y = xyDe(off).y;
    info.style.top = (y > info.offsetHeight + 6 ? y - info.offsetHeight - 4 : y + medida.alto + 4) + 'px';
  }
  function ocultarEmergentes() { ocultarSugerencias(); firma.hidden = true; info.hidden = true; }

  async function irADefinicion(off) {
    if (!lsp.listo || !ES_CODIGO.test(actual || '')) return;
    const r = await lspPedir('textDocument/definition', { textDocument: { uri: uri(actual) }, position: N.offsetToPos(N.lineStarts(ta.value), off) });
    const loc = Array.isArray(r) ? r[0] : r;
    if (!loc || !loc.uri || !loc.uri.startsWith(RAIZ_URI)) { avisoEstado('No se encontró la definición (las de la biblioteca están en la documentación)'); return; }
    const ruta = decodeURIComponent(loc.uri.slice(RAIZ_URI.length));
    const texto = fs.read(ruta) || '';
    const st = N.lineStarts(texto);
    const s = N.posToOffset(st, loc.range.start, texto.length), e = N.posToOffset(st, loc.range.end, texto.length);
    abrir(ruta, [s, e]);
  }
  function mostrarSeleccion() {
    const p = xyDe(ta.selectionStart);
    const alto = ta.clientHeight;
    if (p.y < 0 || p.y > alto - medida.alto * 2) ta.scrollTop = Math.max(0, ta.scrollTop + p.y - alto / 3);
    if (p.x < 0 || p.x > ta.clientWidth - 40) ta.scrollLeft = Math.max(0, ta.scrollLeft + p.x - ta.clientWidth / 2);
    sincronizarScroll();
  }
  function irA(ruta, linea, columna) {
    if (!fs.files.has(ruta)) return;
    const texto = fs.read(ruta);
    const st = N.lineStarts(texto);
    const off = N.posToOffset(st, { line: linea, character: columna || 0 }, texto.length);
    abrir(ruta, [off, off]);
  }

  // ---------------------------------------------------------------------------
  // Diagnósticos: subrayados, panel Problemas, pestañas y explorador
  // ---------------------------------------------------------------------------
  let temporizadorDiag = 0;
  function programarDiagnosticos() {
    clearTimeout(temporizadorDiag);
    temporizadorDiag = setTimeout(() => { programarPintar(); pintarProblemas(); pintarPestanas(); pintarArbol(); }, 30);
  }
  function pintarProblemas() {
    const filas = [];
    let errores = 0;
    for (const [ruta, lista] of diagnosticos) {
      if (!fs.files.has(ruta)) continue;
      for (const d of lista) {
        if (d.severity === 1) errores++;
        filas.push(`<li data-ruta="${escHtml(ruta)}" data-l="${d.range.start.line}" data-c="${d.range.start.character}">` +
          `<span class="sev sev-${d.severity === 1 ? 1 : 2}">${d.severity === 1 ? '✗' : '⚠'}</span><span>${escHtml(d.message)}</span>` +
          `<span class="donde">${escHtml(ruta.slice(1))}:${d.range.start.line + 1}${d.code ? ' · ' + escHtml(d.code) : ''}</span></li>`);
      }
    }
    problemas.innerHTML = filas.join('') || '<li class="vacio">✓ Sin problemas. F5 ejecuta el archivo abierto; Mayús+F5 lo detiene.</li>';
    const c = $('cuenta-problemas');
    c.textContent = filas.length;
    c.classList.toggle('hay', errores > 0);
  }
  problemas.addEventListener('click', (e) => {
    const li = e.target.closest('li[data-ruta]');
    if (li) irA(li.dataset.ruta, +li.dataset.l, +li.dataset.c);
  });
  function actualizarEstado() {
    if (actual === null) return;
    const v = columnaVisual(ta.selectionStart);
    $('estado-pos').textContent = `Línea ${v.line + 1}, columna ${v.col + 1}` + (ta.selectionEnd > ta.selectionStart ? ` (${ta.selectionEnd - ta.selectionStart} sel.)` : '');
    const ds = diagnosticos.get(actual) || [];
    const err = ds.filter((d) => d.severity === 1).length, av = ds.length - err;
    const el = $('estado-errores');
    el.textContent = err ? `✗ ${err} error${err > 1 ? 'es' : ''}` + (av ? `, ${av} aviso${av > 1 ? 's' : ''}` : '') : av ? `⚠ ${av} aviso${av > 1 ? 's' : ''}` : '✓ Sin errores';
    el.className = err ? 'mal' : '';
    const ext = (/\.[^.]+$/.exec(actual) || ['(sin extensión)'])[0];
    $('estado-tipo').textContent = `UTF-8 · ${ext} · ${estado.tam} px`;
  }
  let temporizadorAviso = 0;
  function avisoEstado(texto) {
    const el = $('estado-motor');
    const antes = el.dataset.normal || el.textContent;
    el.dataset.normal = antes;
    el.textContent = texto;
    el.classList.add('mal');
    clearTimeout(temporizadorAviso);
    temporizadorAviso = setTimeout(() => { el.textContent = el.dataset.normal; el.classList.remove('mal'); }, 5000);
  }
  function estadoMotor(t) { const el = $('estado-motor'); el.dataset.normal = t; el.textContent = t; }

  // ---------------------------------------------------------------------------
  // Ejecutar (sin guardar: el programa ve los archivos tal como están en el editor)
  // ---------------------------------------------------------------------------
  let ejecucion = null;
  function paginaPanel(nombre) {
    if (!estado.panel) { estado.panel = true; aplicarVista(); }
    document.querySelectorAll('.panel-pestanas [data-pagina]').forEach((b) => b.classList.toggle('activa', b.dataset.pagina === nombre));
    for (const p of ['problemas', 'salida', 'entrada']) $('pagina-' + p).hidden = p !== nombre;
  }
  document.querySelector('.panel-pestanas').addEventListener('click', (e) => {
    const b = e.target.closest('[data-pagina]');
    if (b) paginaPanel(b.dataset.pagina);
  });
  function escribirSalida(texto, clase) {
    const abajo = salida.parentElement.scrollTop + salida.parentElement.clientHeight >= salida.parentElement.scrollHeight - 4;
    const span = document.createElement('span');
    if (clase) span.className = clase;
    // "archivo.hez:12" → enlace que lleva a esa línea
    const partes = texto.split(/((?:[\w.\-/]+\/)?[\w.\-]+\.(?:hez|hz):\d+(?::\d+)?)/);
    for (let i = 0; i < partes.length; i++) {
      if (i % 2 === 1) {
        const a = document.createElement('a');
        a.className = 'ir';
        a.textContent = partes[i];
        a.addEventListener('click', () => irAMensaje(partes[i]));
        span.appendChild(a);
      } else if (partes[i]) span.appendChild(document.createTextNode(partes[i]));
    }
    salida.appendChild(span);
    if (salida.childNodes.length > 4000) salida.removeChild(salida.firstChild);
    if (abajo) salida.parentElement.scrollTop = salida.parentElement.scrollHeight;
  }
  function irAMensaje(t) {
    const m = /^(.*?):(\d+)(?::(\d+))?$/.exec(t);
    if (!m) return;
    let ruta = m[1].startsWith('/proyecto/') ? m[1].slice(9) : '/' + m[1].replace(/^\.?\//, '');
    if (!fs.files.has(ruta)) ruta = [...fs.files.keys()].find((f) => f.endsWith('/' + VFS.name(m[1]))) || ruta;
    irA(ruta, +m[2] - 1, m[3] ? +m[3] - 1 : 0);
  }
  // La raíz del proyecto de una ruta: la carpeta con hypersettings/ (como hyper run), o '/'
  function raizProyecto(ruta) {
    for (let d = ruta && fs.isDir(ruta) ? ruta : ruta ? VFS.dir(ruta) : '/'; ; d = VFS.dir(d)) {
      if (fs.isDir(unir(d, 'hypersettings'))) return d;
      if (d === '/') return '/';
    }
  }
  // El programa principal del proyecto: "principal" de hypersettings/proyecto.json, o main.hez
  function principalDe(raiz) {
    let p = 'main.hez';
    try { p = JSON.parse(fs.read(unir(raiz, 'hypersettings/proyecto.json')) || '{}').principal || p; } catch (e) { /* proyecto.json mal escrito */ }
    const r = unir(raiz, p);
    return fs.files.has(r) ? r : null;
  }
  async function ejecutar(entradas) {
    let entrada = actual;
    if (!entradas && (entrada === null || !ES_CODIGO.test(entrada))) {
      entrada = principalDe(raizProyecto(actual)) || (fs.files.has('/main.hez') ? '/main.hez' : [...fs.files.keys()].find((f) => ES_CODIGO.test(f)));
      if (!entrada) { avisoEstado('No hay ningún archivo .hez que ejecutar'); return; }
    }
    if (ejecucion) detener(true);
    paginaPanel('salida');
    salida.innerHTML = '';
    if (entradas) escribirSalida(`▶ Pruebas: ${entradas.length} archivo${entradas.length === 1 ? '' : 's'}   (sin guardar: lo que ves en el editor)\n\n`, 'fin');
    else escribirSalida(`▶ ${entrada.slice(1)}   (sin guardar: se ejecuta lo que ves en el editor)\n\n`, 'fin');
    const wasm = modulo || await moduloListo;
    const w = new Worker('run-worker.js');
    const yo = { worker: w, inicio: performance.now() };
    ejecucion = yo;
    $('btn-detener').disabled = false;
    w.onmessage = (e) => {
      if (ejecucion !== yo) return;
      const m = e.data;
      if (m.out) escribirSalida(m.out);
      if (m.err) escribirSalida(m.err, 'err');
      if (m.files) {
        const nuevos = [];
        for (const [ruta, valor] of Object.entries(m.files)) { if (!fs.files.has(ruta)) nuevos.push(ruta); fs.write(ruta, valor); }
        if (nuevos.length) escribirSalida(`\n📁 El programa creó ${nuevos.map((r) => r.slice(1)).join(', ')} (en el Explorador)\n`, 'fin');
        pintarArbol();
        guardar();
      }
      if (m.done) {
        const s = (m.ms / 1000).toLocaleString('es', { maximumFractionDigits: 2, minimumFractionDigits: 2 });
        if (entradas && m.tests) {
          const t = m.tests, total = t.passed + t.failed;
          escribirSalida(t.failed ? `\n■ ${total} prueba${total === 1 ? '' : 's'}: ${t.passed} correcta${t.passed === 1 ? '' : 's'}, ${t.failed} fallida${t.failed === 1 ? '' : 's'} (${s} s)\n`
            : `\n■ ✓ Las ${total} prueba${total === 1 ? '' : 's'} pasan (${t.files} archivo${t.files === 1 ? '' : 's'}, ${s} s)\n`, t.failed ? 'fin err' : 'fin bien');
        } else escribirSalida(m.code === 0 ? `\n■ Terminó bien (código 0) en ${s} s\n` : `\n■ Terminó con errores (código ${m.code}) en ${s} s\n`,
          m.code === 0 ? 'fin bien' : 'fin err');
        w.terminate();
        ejecucion = null;
        $('btn-detener').disabled = true;
      }
    };
    const archivos = {};
    for (const [ruta, valor] of fs.files) archivos[ruta] = valor;
    if (actual && ES_CODIGO.test(actual)) archivos[actual] = ta.value;
    w.postMessage({ wasm, files: archivos, folders: [...fs.folders], entry: entrada, entries: entradas || null, stdin: $('entrada').value });
  }
  // Ejecutar → Ejecutar las pruebas: los .hez de hypertests/ (o tests/) del proyecto, uno tras otro (como hyper test)
  function ejecutarPruebas() {
    const raiz = raizProyecto(actual);
    const dir = fs.isDir(unir(raiz, 'hypertests')) ? unir(raiz, 'hypertests') : unir(raiz, 'tests');
    const lista = [...fs.files.keys()].filter((f) => f.startsWith(dir + '/') && ES_CODIGO.test(f)).sort();
    if (!lista.length) { avisoEstado('Este proyecto no tiene pruebas: créalas en hypertests/'); return; }
    ejecutar(lista);
  }
  // Ejecutar → Repetir al cambiar: vuelve a ejecutar el programa cuando dejas de escribir
  let temporizadorRepetir = 0;
  function repetirSiToca() {
    if (!estado.repetir) return;
    clearTimeout(temporizadorRepetir);
    temporizadorRepetir = setTimeout(() => ejecutar(), 900);
  }

  // Archivo → Nuevo proyecto: los mismos de hyper new (juego, app, ia...), en una carpeta nueva
  const TIPOS = [['juego', '🎮 Juego de consola', 'con teclado (para jugarlo, en tu equipo)'], ['app', '📋 Aplicación', 'menú y datos guardados'],
    ['ia', '🧠 Inteligencia artificial', 'una red neuronal que aprende de un CSV'], ['script', '🛠 Herramienta', 'de línea de órdenes, con argumentos'],
    ['web', '🌐 Aplicación web', 'plantillas, formulario y API (hyper serve)'], ['sitio', '📰 Sitio web', 'escrito en Markdown']];
  function nuevoProyecto() {
    dialogo(`<h2>Nuevo proyecto</h2><p>Con las carpetas de HyperEZ: <b>hypersettings</b> (el proyecto), <b>hypercode</b> (los módulos) y <b>hypertests</b> (las pruebas).</p>
      <div class="tipos">${TIPOS.map(([t, n, d], i) => `<label class="tipo-proyecto"><input type="radio" name="tipo" value="${t}"${i === 1 ? ' checked' : ''}><span><b>${n}</b><small>${d}</small></span></label>`).join('')}</div>
      <label class="campo">Nombre <input type="text" id="d-nombre" value="${escHtml(fs.freeName('/', 'mi-proyecto', ''))}"></label>
      <p class="error" id="d-error" hidden></p>
      <div class="botones"><button data-cerrar>Cancelar</button><button class="principal" id="d-crear">Crear</button></div>`, (d, cerrar) => {
      const nombre = d.querySelector('#d-nombre');
      const crear = async () => {
        const n = nombre.value.trim(), tipo = d.querySelector('input[name=tipo]:checked').value;
        const error = (t) => { const e = d.querySelector('#d-error'); e.textContent = t; e.hidden = false; };
        if (!VFS.validName(n)) { error('Escribe un nombre sin \\ / : * ? " < > |'); return; }
        if (fs.exists('/' + n)) { error('Ya hay algo llamado ' + n); return; }
        if (!lsp.worker) { error('HyperEZ aún se está cargando'); return; }
        const r = await lspEnviar({ nuevoProyecto: { tipo, nombre: n } });
        if (!r.archivos) { error('No se pudo crear el proyecto'); return; }
        cerrar();
        for (const f of r.archivos) fs.write('/' + n + '/' + f.path, f.content);
        for (const c of ['/' + n, '/' + n + '/hypercode']) if (!estado.carpetasAbiertas.includes(c)) estado.carpetasAbiertas.push(c);
        lspCarpetas();
        for (const f of r.archivos) if (ES_CODIGO.test(f.path)) await lspAbrir('/' + n + '/' + f.path);
        abrir('/' + n + '/main.hez');
        avisoEstado(`Proyecto ${n} creado: F5 lo ejecuta, Ctrl+Mayús+T sus pruebas`);
      };
      d.querySelector('#d-crear').onclick = crear;
      nombre.onkeydown = (e) => { if (e.key === 'Enter') crear(); };
      nombre.select();
    });
  }

  // Archivo → Imprimir: el código con sus colores, en una ventana aparte
  function imprimir() {
    if (actual === null || $('codigo').hidden) { avisoEstado('Abre un archivo para imprimirlo'); return; }
    const w = open('', '_blank');
    if (!w) { avisoEstado('El navegador no dejó abrir la ventana para imprimir'); return; }
    const cuerpo = ES_CODIGO.test(actual) ? N.highlightHtml(ta.value, []) : escHtml(ta.value);
    w.document.write(`<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>${escHtml(VFS.name(actual))}</title><style>
      body{margin:14mm;font:10.5pt/1.45 Consolas,"Cascadia Mono",monospace;color:#1f2328}h1{font:600 10pt "Segoe UI",sans-serif;color:#666;margin:0 0 8px}
      pre{margin:0;white-space:pre-wrap;word-break:break-word;font:inherit}.t-kw,.t-const,.t-interp{color:#0000dc}.t-str{color:#a31515}.t-num,.t-fmt{color:#098658}
      .t-com{color:#008000;font-style:italic}.t-doc{color:#3b7f3b;font-style:italic}.t-type{color:#267f99}.t-fn,.t-builtin{color:#795e26}.t-esc{color:#e00}</style></head>
      <body><h1>${escHtml(actual.slice(1))} · HyperEZ</h1><pre>${cuerpo}</pre></body></html>`);
    w.document.close();
    w.focus();
    setTimeout(() => w.print(), 200);
  }

  // Ver → Fuente
  const FUENTES = [['', 'Consolas (la de siempre)'], ['"Cascadia Code", "Cascadia Mono"', 'Cascadia Code'], ['"Courier New"', 'Courier New'],
    ['"Fira Code", "Fira Mono"', 'Fira Code'], ['"JetBrains Mono"', 'JetBrains Mono'], ['"Source Code Pro"', 'Source Code Pro'], ['monospace', 'La del sistema']];
  function elegirFuente() {
    dialogo(`<h2>Fuente del código</h2><label class="campo">Fuente <select id="d-fuente">${FUENTES.map(([v, n]) =>
      `<option value="${escHtml(v)}"${v === estado.fuente ? ' selected' : ''}>${escHtml(n)}</option>`).join('')}</select></label>
      <label class="campo">Tamaño <input type="number" id="d-tam" min="9" max="28" value="${estado.tam}"></label>
      <div class="botones"><button data-cerrar>Cancelar</button><button class="principal" id="d-ok">Aceptar</button></div>`, (d, cerrar) => {
      d.querySelector('#d-ok').onclick = () => {
        estado.fuente = d.querySelector('#d-fuente').value;
        estado.tam = Math.max(9, Math.min(28, parseInt(d.querySelector('#d-tam').value, 10) || 14));
        cerrar();
        aplicarVista();
      };
    });
  }
  function detener(silencio) {
    if (!ejecucion) return;
    ejecucion.worker.terminate();
    ejecucion = null;
    $('btn-detener').disabled = true;
    if (!silencio) escribirSalida('\n■ Detenido\n', 'fin err');
  }

  // ---------------------------------------------------------------------------
  // Buscar y reemplazar
  // ---------------------------------------------------------------------------
  const barra = $('buscar-barra'), bTexto = $('buscar-texto'), rTexto = $('reemplazar-texto');
  function abrirBuscar(reemplazar) {
    barra.hidden = false;
    rTexto.hidden = !reemplazar;
    $('reemplazar-uno').hidden = $('reemplazar-todo').hidden = !reemplazar;
    const sel = ta.value.slice(ta.selectionStart, ta.selectionEnd);
    if (sel && !sel.includes('\n')) bTexto.value = sel;
    bTexto.focus();
    bTexto.select();
    contarCoincidencias();
  }
  function contarCoincidencias() {
    const q = bTexto.value;
    if (!q) { $('buscar-cuenta').textContent = ''; return 0; }
    const n = ta.value.toLowerCase().split(q.toLowerCase()).length - 1;
    $('buscar-cuenta').textContent = n ? `${n} coincidencia${n > 1 ? 's' : ''}` : 'Sin resultados';
    return n;
  }
  function buscar(atras) {
    const q = bTexto.value.toLowerCase();
    if (!q) return;
    const t = ta.value.toLowerCase();
    let i = atras ? t.lastIndexOf(q, ta.selectionStart - 1) : t.indexOf(q, ta.selectionEnd);
    if (i < 0) i = atras ? t.lastIndexOf(q) : t.indexOf(q);
    if (i < 0) return;
    ta.focus();
    ta.setSelectionRange(i, i + q.length);
    mostrarSeleccion();
    programarPintar();
    bTexto.focus();
  }
  bTexto.addEventListener('input', contarCoincidencias);
  bTexto.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') { e.preventDefault(); buscar(e.shiftKey); }
    if (e.key === 'Escape') { barra.hidden = true; ta.focus(); }
  });
  rTexto.addEventListener('keydown', (e) => { if (e.key === 'Escape') { barra.hidden = true; ta.focus(); } });
  $('buscar-sig').onclick = () => buscar(false);
  $('buscar-ant').onclick = () => buscar(true);
  $('buscar-cerrar').onclick = () => { barra.hidden = true; ta.focus(); };
  $('reemplazar-uno').onclick = () => {
    const sel = ta.value.slice(ta.selectionStart, ta.selectionEnd);
    if (sel && sel.toLowerCase() === bTexto.value.toLowerCase()) {
      const a = ta.selectionStart;
      aplicar({ from: a, to: ta.selectionEnd, insert: rTexto.value, selStart: a + rTexto.value.length, selEnd: a + rTexto.value.length });
    }
    buscar(false);
    contarCoincidencias();
  };
  $('reemplazar-todo').onclick = () => {
    const q = bTexto.value;
    if (!q) return;
    const re = new RegExp(q.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi');
    const nuevo = ta.value.replace(re, () => rTexto.value);
    aplicar({ from: 0, to: ta.value.length, insert: nuevo, selStart: 0, selEnd: 0 });
    contarCoincidencias();
  };

  // ---------------------------------------------------------------------------
  // Menús, diálogos y comandos
  // ---------------------------------------------------------------------------
  const menu = $('menu');
  menu.addEventListener('click', (e) => {
    const item = e.target.closest('.menu-item');
    const boton = e.target.closest('button[data-cmd]');
    if (boton) { cerrarMenus(); comando(boton.dataset.cmd, boton.dataset.arg); return; }
    if (item && !e.target.closest('.desplegable')) {
      const abierto = item.classList.contains('abierto');
      cerrarMenus();
      if (!abierto) { item.classList.add('abierto'); marcarMenus(); }
    }
  });
  menu.addEventListener('mouseover', (e) => {
    const item = e.target.closest('.menu-item');
    if (item && menu.querySelector('.abierto') && !item.classList.contains('abierto')) { cerrarMenus(); item.classList.add('abierto'); marcarMenus(); }
  });
  document.addEventListener('mousedown', (e) => {
    if (!e.target.closest('.menu')) cerrarMenus();
    if (!e.target.closest('.menu-contextual')) $('contextual').hidden = true;
    if (!e.target.closest('.sugerencias') && e.target !== ta) ocultarSugerencias();
  });
  function cerrarMenus() { menu.querySelectorAll('.abierto').forEach((m) => m.classList.remove('abierto')); }
  function marcarMenus() {
    const marca = (cmd, si) => { const b = menu.querySelector(`[data-cmd="${cmd}"]`); if (b) b.classList.toggle('marcado', si); };
    marca('ver-explorador', estado.explorador);
    marca('ver-panel', estado.panel);
    marca('ver-numeros', estado.numeros);
    marca('tema-clasico', estado.tema === 'clasico');
    marca('tema-noche', estado.tema === 'noche');
    marca('repetir', estado.repetir);
  }
  document.querySelector('.herramientas').addEventListener('click', (e) => {
    const b = e.target.closest('button[data-cmd]');
    if (b) comando(b.dataset.cmd);
  });
  document.querySelector('.exp-botones').addEventListener('click', (e) => {
    const b = e.target.closest('button[data-cmd]');
    if (b) comando(b.dataset.cmd);
  });
  document.querySelector('.panel-pestanas [data-cmd]').addEventListener('click', () => comando('limpiar-salida'));

  function menuContextual(x, y, opciones) {
    const m = $('contextual');
    m.innerHTML = '';
    for (const o of opciones) {
      if (!o) { m.appendChild(document.createElement('hr')); continue; }
      const b = document.createElement('button');
      b.innerHTML = `<span>${escHtml(o[0])}</span>${o[2] ? `<kbd>${o[2]}</kbd>` : ''}`;
      b.onclick = () => { m.hidden = true; o[1](); };
      m.appendChild(b);
    }
    m.hidden = false;
    m.style.left = Math.min(x, innerWidth - m.offsetWidth - 4) + 'px';
    m.style.top = Math.min(y, innerHeight - m.offsetHeight - 4) + 'px';
  }
  function dialogo(html, alAbrir) {
    const fondo = $('dialogo');
    fondo.firstElementChild.innerHTML = html;
    fondo.hidden = false;
    const cerrar = () => { fondo.hidden = true; ta.focus(); };
    fondo.onclick = (e) => { if (e.target === fondo || e.target.closest('[data-cerrar]')) cerrar(); };
    fondo.onkeydown = (e) => { if (e.key === 'Escape') cerrar(); };
    if (alAbrir) alAbrir(fondo.firstElementChild, cerrar);
    const foco = fondo.querySelector('input, .principal, button');
    if (foco) foco.focus();
  }
  function confirmar(texto, boton, alAceptar) {
    dialogo(`<p>${escHtml(texto)}</p><div class="botones"><button data-cerrar>Cancelar</button><button class="principal" id="d-si">${escHtml(boton)}</button></div>`,
      (d, cerrar) => { d.querySelector('#d-si').onclick = () => { cerrar(); alAceptar(); }; });
  }
  function descargar(nombre, datos, tipo) {
    const blob = datos instanceof Blob ? datos : new Blob([datos], { type: tipo || 'text/plain;charset=utf-8' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = nombre;
    document.body.appendChild(a);
    a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
  }
  function bytesDe(valor) {
    if (typeof valor === 'string' && valor.startsWith('data:')) {
      const b = atob(valor.slice(valor.indexOf(',') + 1));
      return Uint8Array.from(b, (c) => c.charCodeAt(0));
    }
    return valor;
  }
  function descargarArchivo(ruta) { descargar(VFS.name(ruta), new Blob([bytesDe(fs.read(ruta))])); }

  async function compartir() {
    if (actual === null || !ES_CODIGO.test(actual)) { avisoEstado('Abre un archivo .hez para compartirlo'); return; }
    let datos = new TextEncoder().encode(ta.value);
    let pref = 'c';
    if (window.CompressionStream) {
      const s = new Blob([datos]).stream().pipeThrough(new CompressionStream('deflate-raw'));
      datos = new Uint8Array(await new Response(s).arrayBuffer());
      pref = 'z';
    }
    let bin = '';
    for (const b of datos) bin += String.fromCharCode(b);
    const b64 = btoa(bin).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
    const enlace = location.href.split('#')[0].split('?')[0] + '#' + pref + '=' + b64 + '&n=' + encodeURIComponent(VFS.name(actual));
    try { await navigator.clipboard.writeText(enlace); } catch (e) { /* sin permiso: se muestra para copiarlo a mano */ }
    dialogo(`<h2>Enlace para compartir</h2><p>Quien lo abra verá este programa en su navegador y podrá ejecutarlo. Ya está copiado.</p>
      <input type="text" readonly value="${escHtml(enlace)}"><div class="botones"><button class="principal" data-cerrar>Aceptar</button></div>`,
    (d) => { const i = d.querySelector('input'); i.focus(); i.select(); });
  }
  async function leerCompartido() {
    const m = /#([cz])=([^&]+)(?:&n=([^&]*))?/.exec(location.hash);
    if (!m) return false;
    try {
      const bin = atob(m[2].replace(/-/g, '+').replace(/_/g, '/'));
      let datos = Uint8Array.from(bin, (c) => c.charCodeAt(0));
      if (m[1] === 'z') datos = new Uint8Array(await new Response(new Blob([datos]).stream().pipeThrough(new DecompressionStream('deflate-raw'))).arrayBuffer());
      const nombre = VFS.validName(decodeURIComponent(m[3] || '')) ? decodeURIComponent(m[3]) : 'compartido.hez';
      const ruta = unir('/compartidos', fs.freeName('/compartidos', nombre.replace(/\.(hez|hz)$/i, ''), '.hez'));
      fs.write(ruta, new TextDecoder().decode(datos));
      history.replaceState(null, '', location.pathname + location.search);
      abrir(ruta);
      return true;
    } catch (e) { avisoEstado('El enlace compartido está dañado'); return false; }
  }

  // ejemplos (los genera la web a partir de la carpeta examples del repositorio)
  let ejemplos = [];
  fetch('ejemplos.json').then((r) => r.json()).then((lista) => {
    ejemplos = lista;
    const grupos = {};
    for (const e of lista) (grupos[e.categoria] = grupos[e.categoria] || []).push(e);
    $('menu-ejemplos').innerHTML = Object.keys(grupos).map((g) => `<div class="grupo">${escHtml(g)}</div>` +
      grupos[g].map((e) => `<button data-cmd="ejemplo" data-arg="${escHtml(e.ruta)}" title="${escHtml(e.descripcion)}">${escHtml(e.titulo)}</button>`).join('')).join('');
    const q = new URLSearchParams(location.search).get('ejemplo');
    if (q) abrirEjemplo(q);
  }).catch(() => { $('menu-ejemplos').innerHTML = '<span class="vacio">No se pudieron cargar los ejemplos</span>'; });
  function abrirEjemplo(ruta) {
    const e = ejemplos.find((x) => x.ruta === ruta || x.ruta.replace(/\.(hez|hz)$/, '') === ruta);
    if (!e) return;
    const dir = '/ejemplos/' + e.ruta.split('/').slice(0, -1).join('/');
    const destino = VFS.norm(dir + '/' + VFS.name(e.ruta).replace(/\.hz$/, '.hez'));
    if (!fs.files.has(destino)) fs.write(destino, e.codigo);
    for (const [n, c] of Object.entries(e.extras || {})) if (!fs.files.has(unir(dir, n))) fs.write(unir(dir, n), c);
    abrir(destino);
    if (e.teclado) {
      paginaPanel('salida');
      salida.innerHTML = '';
      escribirSalida(`ℹ «${e.titulo}» usa el teclado en tiempo real (un juego de consola): en el navegador puedes leerlo y modificarlo,\n` +
        `pero para jugar descarga HyperEZ y ejecútalo con F5 en HyperEZ Editor.\n`, 'fin');
    }
  }

  function aplicarVista() {
    document.body.className = estado.tema === 'noche' ? 'tema-noche' : 'tema-clasico';
    $('cuerpo').classList.toggle('sin-explorador', !estado.explorador);
    $('cuerpo').style.setProperty('--ancho-explorador', estado.anchoExp + 'px');
    document.querySelector('.centro').classList.toggle('sin-panel', !estado.panel);
    document.querySelector('.centro').style.setProperty('--alto-panel', estado.altoPanel + 'px');
    $('editor').classList.toggle('sin-numeros', !estado.numeros);
    document.documentElement.style.setProperty('--tam', estado.tam + 'px');
    if (estado.fuente) document.documentElement.style.setProperty('--letra-codigo', estado.fuente + ', Consolas, monospace');
    else document.documentElement.style.removeProperty('--letra-codigo');
    document.documentElement.style.setProperty('--alto-linea', Math.round(estado.tam * 1.45) + 'px');
    medir();
    programarPintar();
    marcarMenus();
    guardar();
  }

  async function darFormato() {
    if (actual === null || !ES_CODIGO.test(actual) || !lsp.worker) return;
    const r = await lspEnviar({ format: ta.value });
    if (r.error) { avisoEstado('No se puede dar formato: ' + r.error); return; }
    if (r.text === ta.value) { avisoEstado('El código ya tenía buen formato'); return; }
    const pos = N.offsetToPos(N.lineStarts(ta.value), ta.selectionStart);
    const off = N.posToOffset(N.lineStarts(r.text), pos, r.text.length);
    aplicar({ from: 0, to: ta.value.length, insert: r.text, selStart: off, selEnd: off });
  }

  function comando(cmd, arg) {
    switch (cmd) {
      case 'nuevo': nuevoArchivo(); break;
      case 'nueva-carpeta': nuevaCarpeta(); break;
      case 'nuevo-proyecto': nuevoProyecto(); break;
      case 'imprimir': imprimir(); break;
      case 'pruebas': ejecutarPruebas(); break;
      case 'repetir':
        estado.repetir = !estado.repetir;
        marcarMenus();
        guardar();
        avisoEstado(estado.repetir ? '⟳ Repetir al cambiar: el programa se ejecuta cada vez que dejas de escribir' : 'Repetir al cambiar: desactivado');
        if (estado.repetir) ejecutar();
        break;
      case 'fuente': elegirFuente(); break;
      case 'abrir': $('abrir-archivos').click(); break;
      case 'guardar': if (actual) descargarArchivo(actual); break;
      case 'zip': {
        const nombre = (estado.nombreProyecto || 'mi-proyecto') + '.zip';
        descargar(nombre, new Blob([N.makeZip([...fs.files].map(([r, v]) => ({ name: r.slice(1), data: bytesDe(v) })))], { type: 'application/zip' }));
        break;
      }
      case 'compartir': compartir(); break;
      case 'cerrar-pestana': if (actual) cerrarPestana(actual); break;
      case 'reiniciar': confirmar('Se borrarán todos los archivos de este navegador y se volverá al proyecto de bienvenida.', 'Empezar de cero', () => {
        localStorage.removeItem(CLAVE);
        location.reload();
      }); break;
      case 'deshacer': ta.focus(); document.execCommand('undo'); break;
      case 'rehacer': ta.focus(); document.execCommand('redo'); break;
      case 'cortar': ta.focus(); document.execCommand('cut'); break;
      case 'copiar': ta.focus(); document.execCommand('copy'); break;
      case 'pegar':
        ta.focus();
        if (navigator.clipboard && navigator.clipboard.readText) navigator.clipboard.readText().then((t) => aplicar({ from: ta.selectionStart, to: ta.selectionEnd, insert: t, selStart: ta.selectionStart + t.length, selEnd: ta.selectionStart + t.length })).catch(() => avisoEstado('Usa Ctrl+V para pegar'));
        break;
      case 'buscar': abrirBuscar(false); break;
      case 'reemplazar': abrirBuscar(true); break;
      case 'ir-linea':
        dialogo(`<h2>Ir a la línea</h2><input type="text" id="d-linea" placeholder="1 – ${inicios.length}"><div class="botones"><button data-cerrar>Cancelar</button><button class="principal" id="d-ir">Ir</button></div>`,
          (d, cerrar) => {
            const ir = () => { const n = parseInt(d.querySelector('#d-linea').value, 10); cerrar(); if (n > 0) irA(actual, n - 1, 0); };
            d.querySelector('#d-ir').onclick = ir;
            d.querySelector('#d-linea').onkeydown = (e) => { if (e.key === 'Enter') ir(); };
          });
        break;
      case 'comentar': aplicar(N.toggleComment(ta.value, ta.selectionStart, ta.selectionEnd)); break;
      case 'duplicar': {
        const t = ta.value, a = ta.selectionStart;
        const ls = t.lastIndexOf('\n', a - 1) + 1;
        let le = t.indexOf('\n', a);
        if (le < 0) le = t.length;
        const linea = t.slice(ls, le);
        aplicar({ from: le, to: le, insert: '\n' + linea, selStart: a + linea.length + 1, selEnd: a + linea.length + 1 });
        break;
      }
      case 'autocompletar': ta.focus(); mostrarSugerencias(true); break;
      case 'formato': darFormato(); break;
      case 'seleccionar-todo': ta.focus(); ta.select(); break;
      case 'ejecutar': ejecutar(); break;
      case 'detener': detener(false); break;
      case 'comprobar': paginaPanel('problemas'); lspCambio(actual || ''); avisoEstado(erroresDe(actual || '') ? 'Hay errores: míralos en Problemas' : '✓ Sin errores'); break;
      case 'entrada': paginaPanel('entrada'); $('entrada').focus(); break;
      case 'limpiar-salida': salida.innerHTML = ''; break;
      case 'ver-explorador': estado.explorador = !estado.explorador; aplicarVista(); break;
      case 'ver-panel': estado.panel = !estado.panel; aplicarVista(); break;
      case 'ver-numeros': estado.numeros = !estado.numeros; aplicarVista(); break;
      case 'tema-clasico': estado.tema = 'clasico'; aplicarVista(); break;
      case 'tema-noche': estado.tema = 'noche'; aplicarVista(); break;
      case 'zoom-mas': estado.tam = Math.min(28, estado.tam + 1); aplicarVista(); break;
      case 'zoom-menos': estado.tam = Math.max(9, estado.tam - 1); aplicarVista(); break;
      case 'zoom-normal': estado.tam = 14; aplicarVista(); break;
      case 'plegar': estado.carpetasAbiertas = []; pintarArbol(); guardar(); break;
      case 'menu-ejemplos': { const m = menu.querySelector('[data-menu="ejemplos"]'); cerrarMenus(); m.classList.add('abierto'); break; }
      case 'ejemplo': abrirEjemplo(arg); break;
      case 'ayuda-guia': open('../docs/GUIA.html', '_blank'); break;
      case 'ayuda-api': open('../docs/biblioteca/API.html', '_blank'); break;
      case 'ayuda-descargar': location.href = '../descargar.html'; break;
      case 'ayuda-atajos': dialogo(`<h2>Atajos de teclado</h2><table>${[
        ['F5', 'Ejecutar (sin guardar)'], ['Mayús+F5', 'Detener'], ['Ctrl+Mayús+F5', 'Comprobar errores'], ['Ctrl+F5', 'Repetir al cambiar'],
        ['Ctrl+Mayús+T', 'Ejecutar las pruebas'], ['Ctrl+Mayús+N', 'Nuevo proyecto'], ['Ctrl+P', 'Imprimir'], ['Insert', 'Insertar o sobrescribir'],
        ['Ctrl+rueda', 'Tamaño de la letra'], ['Ctrl+Espacio', 'Autocompletar'],
        ['Ctrl+clic', 'Ir a la definición'], ['Mayús+Alt+F', 'Dar formato'], ['Ctrl+/', 'Comentar o descomentar'], ['Ctrl+D', 'Duplicar la línea'],
        ['Tab / Mayús+Tab', 'Sangrar / quitar sangría'], ['Ctrl+F / Ctrl+H', 'Buscar / reemplazar'], ['Ctrl+G', 'Ir a la línea'],
        ['Ctrl+N / Ctrl+O', 'Nuevo archivo / abrir del equipo'], ['Ctrl+S', 'Descargar el archivo'], ['Ctrl+W', 'Cerrar la pestaña'],
        ['Ctrl+Mayús+E / Ctrl+J', 'Mostrar el Explorador / el panel'], ['Ctrl++ / Ctrl+- / Ctrl+0', 'Tamaño de la letra'],
        ['F2 / Supr (en el Explorador)', 'Renombrar / eliminar']].map((f) => `<tr><td><kbd>${f[0]}</kbd></td><td>${f[1]}</td></tr>`).join('')}</table>
        <div class="botones"><button class="principal" data-cerrar>Cerrar</button></div>`); break;
      case 'acerca': dialogo(`<h2>HyperEZ Editor en el navegador</h2><p>${escHtml($('estado-motor').dataset.normal || '')}</p>
        <p>El mismo lenguaje que instalas en tu equipo, compilado a WebAssembly: el compilador, la máquina virtual, las bibliotecas y el
        servidor de lenguaje de VS Code funcionan dentro de esta página, sin enviar tu código a ningún servidor.</p>
        <p>Para juegos con teclado, archivos de tu equipo, webs y todo lo demás, descarga HyperEZ: incluye HyperEZ Editor para Windows.</p>
        <div class="botones"><a href="../creditos.html">Créditos</a><a class="principal" href="../descargar.html">Descargar HyperEZ</a><button data-cerrar>Cerrar</button></div>`); break;
      default: break;
    }
  }
  $('abrir-archivos').addEventListener('change', (e) => { anadirArchivos(e.target.files, carpetaDestino()); e.target.value = ''; });
  $('entrada').addEventListener('input', () => { estado.entrada = $('entrada').value; guardar(); });

  // atajos de toda la página
  document.addEventListener('keydown', (e) => {
    const ctrl = e.ctrlKey || e.metaKey;
    const k = e.key.toLowerCase();
    let cmd = null;
    if (e.key === 'F5' && ctrl && e.shiftKey) cmd = 'comprobar';
    else if (e.key === 'F5' && ctrl) cmd = 'repetir';
    else if (ctrl && e.shiftKey && k === 'n') cmd = 'nuevo-proyecto';
    else if (ctrl && e.shiftKey && k === 't') cmd = 'pruebas';
    else if (e.key === 'F5' && e.shiftKey) cmd = 'detener';
    else if (e.key === 'F5') cmd = 'ejecutar';
    else if (e.key === 'F12') { e.preventDefault(); irADefinicion(ta.selectionStart); return; }
    else if (ctrl && e.shiftKey && k === 'e') cmd = 'ver-explorador';
    else if (ctrl && !e.shiftKey && !e.altKey) {
      cmd = { n: 'nuevo', o: 'abrir', s: 'guardar', w: 'cerrar-pestana', f: 'buscar', h: 'reemplazar', g: 'ir-linea', '/': 'comentar', d: 'duplicar',
        j: 'ver-panel', '+': 'zoom-mas', '=': 'zoom-mas', '-': 'zoom-menos', '0': 'zoom-normal', y: 'rehacer', p: 'imprimir' }[k] || null;
      if ((k === 'y') && document.activeElement !== ta) cmd = null;
    } else if (e.altKey && e.shiftKey && k === 'f') cmd = 'formato';
    if (cmd) { e.preventDefault(); comando(cmd); }
  });

  // separadores arrastrables
  function arrastrar(el, alMover) {
    el.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      el.setPointerCapture(e.pointerId);
      el.classList.add('arrastrando');
      const mover = (ev) => alMover(ev);
      const soltar = () => { el.classList.remove('arrastrando'); el.removeEventListener('pointermove', mover); el.removeEventListener('pointerup', soltar); guardar(); medir(); programarPintar(); };
      el.addEventListener('pointermove', mover);
      el.addEventListener('pointerup', soltar);
    });
  }
  arrastrar($('sep-explorador'), (e) => {
    estado.anchoExp = Math.max(140, Math.min(520, e.clientX - $('cuerpo').getBoundingClientRect().left));
    $('cuerpo').style.setProperty('--ancho-explorador', estado.anchoExp + 'px');
  });
  arrastrar($('sep-panel'), (e) => {
    const c = document.querySelector('.centro').getBoundingClientRect();
    estado.altoPanel = Math.max(70, Math.min(c.height - 120, c.bottom - e.clientY));
    document.querySelector('.centro').style.setProperty('--alto-panel', estado.altoPanel + 'px');
  });
  addEventListener('resize', () => { medir(); programarPintar(); });

  // botón de descarga: directo al instalador si es Windows
  const so = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || navigator.userAgent;
  if (/win/i.test(so)) $('boton-descargar').href = '../descargar.html#windows';
  else if (/mac/i.test(so)) $('boton-descargar').href = '../descargar.html#macos';
  else if (/linux/i.test(so)) $('boton-descargar').href = '../descargar.html#linux';

  // ---------------------------------------------------------------------------
  // Arranque
  // ---------------------------------------------------------------------------
  $('entrada').value = estado.entrada || '';
  aplicarVista();
  estado.abiertos = estado.abiertos.filter((r) => fs.files.has(r));
  if (!estado.abiertos.length && fs.files.size) estado.abiertos.push(fs.files.has('/main.hez') ? '/main.hez' : [...fs.files.keys()][0]);
  pintarProblemas();
  leerCompartido().then((hecho) => {
    if (!hecho) abrir(fs.files.has(estado.activo) ? estado.activo : estado.abiertos[0]);
  });
  iniciarLsp().catch((err) => {
    estadoMotor('No se pudo cargar HyperEZ en este navegador');
    console.error(err);
  });
})();
