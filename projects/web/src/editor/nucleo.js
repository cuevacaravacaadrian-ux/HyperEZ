// Núcleo del editor web de HyperEZ: todo lo que no depende del navegador (se prueba con Node).
//   tokenize        colores del código (las mismas categorías que HyperEZ Editor y VS Code)
//   autoPair        cerrar paréntesis, corchetes, llaves y comillas; saltar el cierre; borrar la pareja
//   enterIndent     sangría al pulsar Intro (y entre llaves)
//   toggleComment   comentar o descomentar líneas con //
//   matchBracket    la pareja de un paréntesis, corchete o llave
//   VFS             el sistema de archivos del Explorador (carpetas y archivos, en el navegador)
//   makeZip         descargar el proyecto como .zip
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.HZNucleo = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const KEYWORDS = new Set(('if else match for foreach in while repeat break continue step do return await throw try catch finally ' +
    'var let const function class struct interface enum import from as constructor extends implements get set static private ' +
    'public protected async event on once emit priority new and or not is').split(' '));
  const CONTEXTUAL = new Set(['from', 'as', 'get', 'set', 'step', 'do', 'event', 'on', 'once', 'emit', 'priority']);
  const CONSTANTS = new Set(['true', 'false', 'null', 'this', 'super']);
  const TYPES = new Set('number int text bool list map set any void function object never'.split(' '));
  const BUILTINS = new Set(('print input len range str toNumber round floor ceil abs min max sqrt pow random randomInt randomSeed ' +
    'format typeof assert assertEqual assertThrows test exit wait spawn awaitAll clamp lerp sin cos tan sign gcd chr ord').split(' '));

  const ID_START = /[\p{L}_]/u;
  const ID_PART = /[\p{L}\p{N}_]/u;

  // Lista de tramos {s, e, t} (inicio, fin y tipo) de los trozos con color. El resto del texto es normal.
  function tokenize(src, base, out, depth) {
    base = base || 0;
    out = out || [];
    depth = depth || 0;
    const n = src.length;
    let i = 0;
    let prevSig = ''; // último símbolo significativo (para saber si un nombre es un tipo)
    const push = (s, e, t) => { if (e > s) out.push({ s: base + s, e: base + e, t }); };
    while (i < n) {
      const c = src[i];
      // comentarios
      if (c === '/' && src[i + 1] === '/') {
        let j = src.indexOf('\n', i);
        if (j < 0) j = n;
        push(i, j, src[i + 2] === '/' ? 'doc' : 'com');
        i = j;
        continue;
      }
      if (c === '/' && src[i + 1] === '*') {
        let j = src.indexOf('*/', i + 2);
        j = j < 0 ? n : j + 2;
        push(i, j, 'com');
        i = j;
        continue;
      }
      // textos
      if (c === '"' || c === "'" || ((c === 'r') && (src[i + 1] === '"' || src[i + 1] === "'"))) {
        const raw = c === 'r';
        const start = i;
        if (raw) i++;
        const q = src[i];
        const triple = !raw && src[i + 1] === q && src[i + 2] === q;
        i += triple ? 3 : 1;
        let segStart = start;
        while (i < n) {
          const d = src[i];
          if (triple ? (d === q && src[i + 1] === q && src[i + 2] === q) : d === q) { i += triple ? 3 : 1; break; }
          if (!triple && d === '\n') break;
          if (!raw && d === '\\') { push(segStart, i, 'str'); push(i, Math.min(n, i + 2), 'esc'); i += 2; segStart = i; continue; }
          if (!raw && d === '$' && src[i + 1] === '{' && depth < 4) {
            push(segStart, i, 'str');
            // buscar la } que cierra (con llaves y textos dentro)
            let j = i + 2, lv = 1;
            while (j < n && lv > 0) {
              const e = src[j];
              if (e === '"' || e === "'") { j++; while (j < n && src[j] !== e && src[j] !== '\n') { if (src[j] === '\\') j++; j++; } j++; continue; }
              if (e === '{') lv++;
              else if (e === '}') { lv--; if (lv === 0) break; }
              else if (e === '\n' && !triple) break;
              j++;
            }
            push(i, i + 2, 'interp');
            const code = src.slice(i + 2, j);
            const spec = splitFormatSpec(code);
            const exprLen = spec === null ? code.length : code.length - spec.length - 1;
            tokenize(code.slice(0, exprLen), base + i + 2, out, depth + 1);
            if (spec !== null) push(i + 2 + exprLen, j, 'fmt');
            if (j < n && src[j] === '}') push(j, j + 1, 'interp');
            i = j < n ? j + 1 : j;
            segStart = i;
            continue;
          }
          i++;
        }
        push(segStart, i, 'str');
        prevSig = 'str';
        continue;
      }
      // números
      if (/[0-9]/.test(c) || (c === '.' && /[0-9]/.test(src[i + 1] || '') && prevSig !== 'id')) {
        const m = /^(?:0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|[0-9][0-9_]*(?:\.(?!\.)[0-9_]*)?(?:[eE][+-]?[0-9]+)?|\.[0-9]+)/.exec(src.slice(i, i + 64));
        const len = m ? m[0].length : 1;
        push(i, i + len, 'num');
        i += len;
        prevSig = 'num';
        continue;
      }
      // nombres
      if (ID_START.test(c)) {
        let j = i + 1;
        while (j < n && ID_PART.test(src[j])) j++;
        const w = src.slice(i, j);
        let k = j;
        while (k < n && (src[k] === ' ' || src[k] === '\t')) k++;
        const call = src[k] === '(';
        const afterDot = prevSig === '.';
        let t = null;
        if (afterDot) t = call ? 'fn' : 'prop';
        else if (CONSTANTS.has(w)) t = 'const';
        else if (KEYWORDS.has(w) && !(CONTEXTUAL.has(w) && (call || w === 'do' && src[k] !== '{'))) t = 'kw';
        else if (TYPES.has(w) && (prevSig === ':' || prevSig === 'is' || prevSig === '<' || prevSig === '->')) t = 'type';
        else if (/^\p{Lu}/u.test(w)) t = 'type';
        else if (BUILTINS.has(w) && call) t = 'builtin';
        else if (call) t = 'fn';
        if (t) push(i, j, t);
        prevSig = w === 'is' ? 'is' : 'id';
        i = j;
        continue;
      }
      if (c === '-' && src[i + 1] === '>') { prevSig = '->'; i += 2; continue; }
      if (!/\s/.test(c)) prevSig = c;
      i++;
    }
    return out;
  }

  // "${precio:.2f}": el formato del final (como en el lexer de HyperEZ). Devuelve el formato o null.
  function splitFormatSpec(code) {
    let depth = 0, ternary = false, colon = -1;
    for (let i = 0; i < code.length; i++) {
      const c = code[i];
      if (c === '"' || c === "'") { i++; while (i < code.length && code[i] !== c) { if (code[i] === '\\') i++; i++; } continue; }
      if ('([{'.includes(c)) depth++;
      else if (')]}'.includes(c)) depth--;
      else if (depth === 0 && c === '?') { if (code[i + 1] === '?' || code[i + 1] === '.') { i++; continue; } ternary = true; }
      else if (depth === 0 && c === ':' && !ternary) colon = i;
    }
    if (colon < 0) return null;
    const spec = code.slice(colon + 1);
    return /^(?:[^\s{}]?[<>^])?\+?0?\d*[,_]?(?:\.\d+)?[feE%dxXbos]?$/u.test(spec) && spec.length > 0 ? spec : null;
  }

  // HTML con colores. diags: [{s, e, sev}] (sev 1 = error, 2 = aviso) para subrayar.
  function highlightHtml(src, diags) {
    const toks = tokenize(src);
    const cuts = new Set([0, src.length]);
    for (const t of toks) { cuts.add(t.s); cuts.add(t.e); }
    const ds = (diags || []).map((d) => ({ s: Math.max(0, d.s), e: Math.max(d.s + 1, d.e), sev: d.sev }));
    for (const d of ds) { cuts.add(d.s); cuts.add(Math.min(src.length, d.e)); }
    const pts = [...cuts].filter((x) => x >= 0 && x <= src.length).sort((a, b) => a - b);
    // los tramos no se solapan (los textos se cortan alrededor de cada ${...}): basta recorrerlos en orden
    const sorted = toks.slice().sort((a, b) => a.s - b.s);
    let html = '';
    let ti = 0;
    for (let p = 0; p < pts.length - 1; p++) {
      const a = pts[p], b = pts[p + 1];
      if (b <= a) continue;
      while (ti < sorted.length && sorted[ti].e <= a) ti++;
      const t = ti < sorted.length && sorted[ti].s <= a ? sorted[ti].t : '';
      // marcas: 1 error, 2 aviso, 3 pareja de paréntesis (si se juntan, gana el error)
      let rank = 0;
      for (const d of ds) if (d.s <= a && d.e > a) { const r = d.sev === 1 ? 3 : d.sev === 2 ? 2 : 1; if (r > rank) rank = r; }
      const extra = rank === 3 ? ' err' : rank === 2 ? ' warn' : rank === 1 ? ' br' : '';
      const text = esc(src.slice(a, b));
      const c = (t ? 't-' + t : '') + extra;
      html += c ? `<span class="${c.trim()}">${text}</span>` : text;
    }
    return html + '\n';
  }

  function esc(s) { return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

  // ---------------------------------------------------------------------------
  // Posiciones (las del servidor de lenguaje: línea y carácter UTF-16, que son los de JavaScript)
  // ---------------------------------------------------------------------------
  function lineStarts(text) {
    const r = [0];
    for (let i = 0; i < text.length; i++) if (text.charCodeAt(i) === 10) r.push(i + 1);
    return r;
  }
  function offsetToPos(starts, off) {
    let lo = 0, hi = starts.length - 1;
    while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (starts[mid] <= off) lo = mid; else hi = mid - 1; }
    return { line: lo, character: off - starts[lo] };
  }
  function posToOffset(starts, pos, textLength) {
    const line = Math.max(0, Math.min(pos.line, starts.length - 1));
    return Math.min(textLength, starts[line] + Math.max(0, pos.character));
  }

  // ---------------------------------------------------------------------------
  // Escribir: parejas, sangría, comentarios
  // ---------------------------------------------------------------------------
  const OPEN = { '(': ')', '[': ']', '{': '}', '"': '"', "'": "'" };
  const CLOSE = new Set([')', ']', '}']);

  // ¿Qué hacer al pulsar key con la selección [a, b)? Devuelve null (lo normal) o un cambio
  // {from, to, insert, selStart, selEnd}: sustituir text[from, to) por insert y dejar esa selección.
  function autoPair(text, a, b, key) {
    const next = text[b] || '';
    const prev = text[a - 1] || '';
    if (key === 'Backspace') {
      if (a === b && a > 0 && OPEN[prev] && OPEN[prev] === next && (prev !== '"' && prev !== "'" || text[a - 2] !== '\\'))
        return { from: a - 1, to: a + 1, insert: '', selStart: a - 1, selEnd: a - 1 };
      return null;
    }
    if (key.length !== 1) return null;
    // saltar el cierre que ya está escrito
    if ((CLOSE.has(key) || key === '"' || key === "'") && a === b && next === key) {
      if (key === '"' || key === "'") {
        // solo si es la comilla que cierra un texto abierto en esta línea
        const ls = text.lastIndexOf('\n', a - 1) + 1;
        const count = text.slice(ls, a).split(key).length - 1;
        if (count % 2 === 1) return { from: a, to: a, insert: '', selStart: a + 1, selEnd: a + 1 };
      } else return { from: a, to: a, insert: '', selStart: a + 1, selEnd: a + 1 };
    }
    if (!OPEN[key]) return null;
    const close = OPEN[key];
    if (a !== b) { // rodear la selección
      const inner = text.slice(a, b);
      return { from: a, to: b, insert: key + inner + close, selStart: a + 1, selEnd: b + 1 };
    }
    if (key === '"' || key === "'") {
      if (/[\p{L}\p{N}_\\]/u.test(prev) || /[\p{L}\p{N}_]/u.test(next)) return null;
      if (key === "'" && /[\p{L}]/u.test(prev)) return null;
    } else if (next && !/[\s)\]},;:]/.test(next)) return null;
    return { from: a, to: a, insert: key + close, selStart: a + 1, selEnd: a + 1 };
  }

  // Intro: la sangría de la línea, más 4 espacios si acaba abriendo { [ (; entre {} deja el cierre debajo.
  function enterIndent(text, a, b, unit) {
    unit = unit || '    ';
    const ls = text.lastIndexOf('\n', a - 1) + 1;
    const line = text.slice(ls, a);
    const indent = /^[ \t]*/.exec(line)[0];
    const before = line.trimEnd();
    const opens = /[{[(]$/.test(before);
    const next = text[b] || '';
    if (opens && OPEN[before[before.length - 1]] === next) {
      const ins = '\n' + indent + unit + '\n' + indent;
      const caret = a + 1 + indent.length + unit.length;
      return { from: a, to: b, insert: ins, selStart: caret, selEnd: caret };
    }
    const ins = '\n' + indent + (opens ? unit : '');
    return { from: a, to: b, insert: ins, selStart: a + ins.length, selEnd: a + ins.length };
  }

  // '}' al principio de una línea con solo espacios: quitar un nivel de sangría
  function outdentOnClose(text, a, key, unit) {
    unit = unit || '    ';
    if (!CLOSE.has(key)) return null;
    const ls = text.lastIndexOf('\n', a - 1) + 1;
    const line = text.slice(ls, a);
    if (!/^[ \t]+$/.test(line)) return null;
    const cut = line.endsWith(unit) ? unit.length : line.endsWith('\t') ? 1 : 0;
    if (!cut) return null;
    if (text[a] === key) return null; // ya está el cierre: autoPair lo salta
    return { from: a - cut, to: a, insert: key, selStart: a - cut + 1, selEnd: a - cut + 1 };
  }

  // Comentar o descomentar las líneas que toca la selección
  function toggleComment(text, a, b) {
    const ls = text.lastIndexOf('\n', a - 1) + 1;
    let le = text.indexOf('\n', b > a && text[b - 1] === '\n' ? b - 1 : b);
    if (le < 0) le = text.length;
    const lines = text.slice(ls, le).split('\n');
    const all = lines.filter((l) => l.trim()).every((l) => /^\s*\/\//.test(l));
    const minIndent = Math.min(...lines.filter((l) => l.trim()).map((l) => /^\s*/.exec(l)[0].length), 1e9);
    const out = lines.map((l) => {
      if (!l.trim()) return l;
      if (all) return l.replace(/^(\s*)\/\/ ?/, '$1');
      return l.slice(0, minIndent) + '// ' + l.slice(minIndent);
    }).join('\n');
    return { from: ls, to: le, insert: out, selStart: ls, selEnd: ls + out.length };
  }

  // Tab / Mayús+Tab con varias líneas seleccionadas
  function indentLines(text, a, b, outdent, unit) {
    unit = unit || '    ';
    const ls = text.lastIndexOf('\n', a - 1) + 1;
    let le = text.indexOf('\n', b > a && text[b - 1] === '\n' ? b - 1 : b);
    if (le < 0) le = text.length;
    const out = text.slice(ls, le).split('\n').map((l) => {
      if (!outdent) return l.length ? unit + l : l;
      if (l.startsWith(unit)) return l.slice(unit.length);
      return l.replace(/^[ \t]{1,4}/, '');
    }).join('\n');
    return { from: ls, to: le, insert: out, selStart: ls, selEnd: ls + out.length };
  }

  // La pareja de un paréntesis/corchete/llave en la posición off (o la de antes): [a, b] o null
  function matchBracket(text, off) {
    const pairs = { '(': ')', '[': ']', '{': '}' };
    const rev = { ')': '(', ']': '[', '}': '{' };
    let at = -1;
    if (pairs[text[off]] || rev[text[off]]) at = off;
    else if (pairs[text[off - 1]] || rev[text[off - 1]]) at = off - 1;
    if (at < 0) return null;
    const c = text[at];
    const toks = tokenize(text).filter((t) => t.t === 'str' || t.t === 'com' || t.t === 'doc');
    const inSkip = (i) => toks.some((t) => i >= t.s && i < t.e);
    if (inSkip(at)) return null;
    let depth = 0;
    if (pairs[c]) {
      for (let i = at; i < text.length; i++) {
        if (inSkip(i)) continue;
        if (text[i] === c) depth++;
        else if (text[i] === pairs[c] && --depth === 0) return [at, i];
      }
    } else {
      for (let i = at; i >= 0; i--) {
        if (inSkip(i)) continue;
        if (text[i] === c) depth++;
        else if (text[i] === rev[c] && --depth === 0) return [i, at];
      }
    }
    return null;
  }

  // La palabra que se está escribiendo justo antes de la posición (para filtrar el autocompletado)
  function wordBefore(text, off) {
    let i = off;
    while (i > 0 && ID_PART.test(text[i - 1])) i--;
    return { start: i, word: text.slice(i, off) };
  }

  // ---------------------------------------------------------------------------
  // Sistema de archivos del Explorador
  // ---------------------------------------------------------------------------
  class VFS {
    constructor(data) {
      this.files = new Map();   // ruta → contenido
      this.folders = new Set(); // rutas de carpetas
      if (data) this.load(data);
    }
    static norm(p) {
      p = ('/' + p).replace(/\\/g, '/').replace(/\/+/g, '/');
      const parts = [];
      for (const s of p.split('/')) { if (!s || s === '.') continue; if (s === '..') parts.pop(); else parts.push(s); }
      return '/' + parts.join('/');
    }
    static dir(p) { const i = p.lastIndexOf('/'); return i <= 0 ? '/' : p.slice(0, i); }
    static name(p) { return p.slice(p.lastIndexOf('/') + 1); }
    static validName(n) { return !!n && !/[\\/:*?"<>|]/.test(n) && n !== '.' && n !== '..' && n.trim() === n; }
    exists(p) { p = VFS.norm(p); return p === '/' || this.files.has(p) || this.folders.has(p); }
    isDir(p) { p = VFS.norm(p); return p === '/' || this.folders.has(p); }
    ensureDir(p) {
      p = VFS.norm(p);
      while (p !== '/' && !this.folders.has(p)) { this.folders.add(p); p = VFS.dir(p); }
    }
    write(p, content) {
      p = VFS.norm(p);
      if (this.folders.has(p)) throw new Error('Ya hay una carpeta llamada ' + VFS.name(p));
      this.ensureDir(VFS.dir(p));
      this.files.set(p, content);
    }
    read(p) { return this.files.get(VFS.norm(p)); }
    mkdir(p) {
      p = VFS.norm(p);
      if (this.files.has(p)) throw new Error('Ya hay un archivo llamado ' + VFS.name(p));
      this.ensureDir(p);
    }
    // Hijos directos de una carpeta: carpetas primero, luego archivos, por nombre
    list(dir) {
      dir = VFS.norm(dir);
      const pre = dir === '/' ? '/' : dir + '/';
      const kids = [];
      for (const f of this.folders) if (f.startsWith(pre) && !f.slice(pre.length).includes('/')) kids.push({ path: f, dir: true });
      for (const f of this.files.keys()) if (f.startsWith(pre) && !f.slice(pre.length).includes('/')) kids.push({ path: f, dir: false });
      const coll = (a, b) => VFS.name(a.path).localeCompare(VFS.name(b.path), 'es', { sensitivity: 'base', numeric: true });
      return kids.sort((a, b) => (a.dir === b.dir ? coll(a, b) : a.dir ? -1 : 1));
    }
    remove(p) {
      p = VFS.norm(p);
      const pre = p + '/';
      this.files.delete(p);
      this.folders.delete(p);
      for (const f of [...this.files.keys()]) if (f.startsWith(pre)) this.files.delete(f);
      for (const f of [...this.folders]) if (f.startsWith(pre)) this.folders.delete(f);
    }
    // Mueve o renombra un archivo o una carpeta (con todo lo que tiene dentro). Devuelve [viejo, nuevo] de cada archivo.
    move(from, to) {
      from = VFS.norm(from);
      to = VFS.norm(to);
      if (from === to) return [];
      if (this.exists(to)) throw new Error('Ya existe ' + VFS.name(to));
      if (to.startsWith(from + '/')) throw new Error('No se puede mover una carpeta dentro de sí misma');
      const moved = [];
      if (this.files.has(from)) {
        this.ensureDir(VFS.dir(to));
        this.files.set(to, this.files.get(from));
        this.files.delete(from);
        moved.push([from, to]);
        return moved;
      }
      if (!this.folders.has(from)) throw new Error('No existe ' + from);
      this.ensureDir(to);
      const pre = from + '/';
      for (const f of [...this.folders]) if (f.startsWith(pre)) { this.folders.delete(f); this.folders.add(to + f.slice(from.length)); }
      for (const [f, c] of [...this.files]) if (f.startsWith(pre)) { this.files.delete(f); this.files.set(to + f.slice(from.length), c); moved.push([f, to + f.slice(from.length)]); }
      this.folders.delete(from);
      return moved;
    }
    // Un nombre libre en la carpeta: "nuevo.hez", "nuevo 2.hez"...
    freeName(dir, base, ext) {
      dir = VFS.norm(dir);
      for (let i = 1; ; i++) {
        const n = base + (i > 1 ? ' ' + i : '') + (ext || '');
        if (!this.exists((dir === '/' ? '' : dir) + '/' + n)) return n;
      }
    }
    toJSON() { return { files: Object.fromEntries(this.files), folders: [...this.folders] }; }
    load(d) {
      this.files = new Map(Object.entries(d.files || {}));
      this.folders = new Set(d.folders || []);
      for (const f of this.files.keys()) this.ensureDir(VFS.dir(f));
    }
  }

  // ---------------------------------------------------------------------------
  // .zip (sin comprimir: lo abre cualquier sistema)
  // ---------------------------------------------------------------------------
  let crcTable = null;
  function crc32(bytes) {
    if (!crcTable) {
      crcTable = new Uint32Array(256);
      for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crcTable[n] = c >>> 0; }
    }
    let c = 0xffffffff;
    for (let i = 0; i < bytes.length; i++) c = crcTable[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  }
  function makeZip(entries) { // entries: [{name, data: Uint8Array | string}]
    const enc = new TextEncoder();
    const chunks = [], central = [];
    let offset = 0;
    const u16 = (v) => [v & 255, (v >>> 8) & 255];
    const u32 = (v) => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];
    for (const e of entries) {
      const name = enc.encode(e.name);
      const data = typeof e.data === 'string' ? enc.encode(e.data) : e.data;
      const crc = crc32(data);
      const local = [0x50, 0x4b, 0x03, 0x04, ...u16(20), ...u16(0x0800), ...u16(0), ...u16(0), ...u16(0x21),
        ...u32(crc), ...u32(data.length), ...u32(data.length), ...u16(name.length), ...u16(0)];
      chunks.push(new Uint8Array(local), name, data);
      central.push(new Uint8Array([0x50, 0x4b, 0x01, 0x02, ...u16(20), ...u16(20), ...u16(0x0800), ...u16(0), ...u16(0), ...u16(0x21),
        ...u32(crc), ...u32(data.length), ...u32(data.length), ...u16(name.length), ...u16(0), ...u16(0), ...u16(0), ...u16(0),
        ...u32(0), ...u32(offset)]), name);
      offset += local.length + name.length + data.length;
    }
    const cdSize = central.reduce((s, c) => s + c.length, 0);
    const end = new Uint8Array([0x50, 0x4b, 0x05, 0x06, ...u16(0), ...u16(0), ...u16(entries.length), ...u16(entries.length),
      ...u32(cdSize), ...u32(offset), ...u16(0)]);
    const all = [...chunks, ...central, end];
    const out = new Uint8Array(all.reduce((s, c) => s + c.length, 0));
    let p = 0;
    for (const c of all) { out.set(c, p); p += c.length; }
    return out;
  }

  return { tokenize, splitFormatSpec, highlightHtml, esc, lineStarts, offsetToPos, posToOffset, autoPair, enterIndent,
    outdentOnClose, toggleComment, indentLines, matchBracket, wordBefore, VFS, makeZip, crc32, KEYWORDS };
});
