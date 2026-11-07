// La Academia HyperEZ (aprender.html): el mapa de misiones, cada misión (lección, reto y preguntas), la experiencia,
// las medallas, el examen final y el certificado. Los programas se ejecutan con HyperEZ de verdad (WebAssembly) en un
// Worker, el mismo del editor del navegador. El progreso se guarda en este navegador (localStorage).
(function () {
  'use strict';
  const A = window.HZ_ACADEMIA, C = window.HZCertificado, N = window.HZNucleo;
  const $ = (id) => document.getElementById(id);
  const CLAVE = 'hyperez-academia-v1';
  const IMAGENES = { logo: 'aprender/cert-logo.png', firma: 'aprender/cert-firma.png' };
  const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  const reducido = matchMedia('(prefers-reduced-motion: reduce)').matches;

  // ------------------------------------------------------------------------------------------- progreso
  let estado;
  try { estado = JSON.parse(localStorage.getItem(CLAVE) || 'null'); } catch (e) { estado = null; }
  estado = Object.assign({ misiones: {}, xp: 0, insignias: [], examen: null, certificado: null }, estado || {});
  const guardar = () => { try { localStorage.setItem(CLAVE, JSON.stringify(estado)); } catch (e) { /* sin espacio */ } };
  const progreso = (id) => estado.misiones[id] || (estado.misiones[id] = { reto: false, pistas: 0, solucion: false, quiz: null, estrellas: 0, codigo: null, xp: 0 });
  const completada = (m) => { const p = estado.misiones[m.id]; return !!(p && p.reto && p.quiz !== null); };
  const desbloqueada = (i) => i === 0 || completada(A.MISIONES[i - 1]);
  const hechas = () => A.MISIONES.filter(completada).length;
  function nivel() {
    let n = A.NIVELES[0], sig = null;
    for (let i = 0; i < A.NIVELES.length; i++) if (estado.xp >= A.NIVELES[i].xp) { n = A.NIVELES[i]; sig = A.NIVELES[i + 1] || null; }
    return { actual: n, siguiente: sig };
  }
  function notaMedia() {
    const qs = A.MISIONES.map((m) => (estado.misiones[m.id] || {}).quiz).filter((x) => x !== null && x !== undefined);
    const media = qs.length ? qs.reduce((a, b) => a + b, 0) / qs.length : 0;
    const examen = estado.examen ? estado.examen.mejor : 0;
    return Math.round(media * 0.5 + examen * 0.5);
  }
  function sumarXp(n, motivo) {
    const antes = nivel().actual;
    estado.xp += n;
    guardar();
    toast('✨', '+' + n + ' XP', motivo);
    const ahora = nivel().actual;
    if (ahora !== antes) setTimeout(() => toast('⬆️', '¡Subes de nivel!', 'Ahora eres <b>' + esc(ahora.nombre) + '</b>'), 700);
    pintarFicha();
  }
  function insignia(id) {
    if (estado.insignias.includes(id)) return;
    estado.insignias.push(id);
    guardar();
    const i = A.INSIGNIAS.find((x) => x.id === id);
    setTimeout(() => toast(i.icono, 'Medalla: ' + i.nombre, i.texto, true), 900);
    pintarInsignias();
  }
  function revisarInsignias() {
    const n = hechas();
    if (n >= 1) insignia('primer-programa');
    if (n >= 5) insignia('mitad');
    if (n >= A.MISIONES.length) insignia('todas');
    if (A.MISIONES.filter((m) => { const p = estado.misiones[m.id]; return p && p.reto && !p.pistas && !p.solucion; }).length >= 3) insignia('sin-pistas');
    if (A.MISIONES.some((m) => (estado.misiones[m.id] || {}).quiz === 100)) insignia('pleno');
    if (estado.certificado) insignia('graduado');
  }

  // ------------------------------------------------------------------------------------------- avisos y confeti
  function toast(icono, titulo, texto, grande) {
    const t = document.createElement('div');
    t.className = 'toast' + (grande ? ' grande' : '');
    t.innerHTML = `<span class="toast-icono">${icono}</span><div><b>${titulo}</b>${texto ? '<small>' + texto + '</small>' : ''}</div>`;
    $('toasts').appendChild(t);
    setTimeout(() => t.classList.add('fuera'), grande ? 4200 : 2800);
    setTimeout(() => t.remove(), grande ? 4800 : 3400);
  }
  function confeti() {
    if (reducido) return;
    const c = $('confeti'), g = c.getContext('2d');
    c.width = innerWidth; c.height = innerHeight;
    const colores = ['#62d962', '#58a6ff', '#ffcb6b', '#c792ea', '#ff9e64', '#ffffff'];
    const piezas = Array.from({ length: 160 }, () => ({ x: innerWidth / 2 + (Math.random() - .5) * 200, y: innerHeight * .35, vx: (Math.random() - .5) * 16,
      vy: -Math.random() * 15 - 4, r: Math.random() * 6 + 4, a: Math.random() * 6, va: (Math.random() - .5) * .3, color: colores[Math.floor(Math.random() * colores.length)] }));
    const t0 = performance.now();
    c.hidden = false;
    (function paso(t) {
      g.clearRect(0, 0, c.width, c.height);
      for (const p of piezas) {
        p.vy += .38; p.vx *= .99; p.x += p.vx; p.y += p.vy; p.a += p.va;
        g.save(); g.translate(p.x, p.y); g.rotate(p.a); g.fillStyle = p.color; g.fillRect(-p.r / 2, -p.r / 4, p.r, p.r / 2); g.restore();
      }
      if (t - t0 < 3200) requestAnimationFrame(paso); else { g.clearRect(0, 0, c.width, c.height); c.hidden = true; }
    })(t0);
  }

  // ------------------------------------------------------------------------------------------- HyperEZ (WebAssembly)
  const modulo = (async () => {
    const r = await fetch('editor/hyperez.wasm');
    if (!r.ok) throw new Error('sin motor');
    if (WebAssembly.compileStreaming && /wasm/.test(r.headers.get("content-type") || "")) return WebAssembly.compileStreaming(r);
    return WebAssembly.compile(await r.arrayBuffer());
  })();
  modulo.then(() => { $('estado-motor').textContent = '● HyperEZ listo en tu navegador'; $('estado-motor').classList.add('listo'); })
    .catch(() => { $('estado-motor').textContent = 'No se pudo cargar HyperEZ en este navegador'; });

  /** Ejecuta un programa y devuelve {salida, codigo, pasadas, falladas}; alSalir(texto, esError) va mostrando la salida. */
  async function ejecutar(codigo, alSalir) {
    const wasm = await modulo;
    return await new Promise((ok) => {
      const w = new Worker('editor/run-worker.js');
      let salida = '';
      const limite = setTimeout(() => {
        w.terminate();
        const m = '\n■ Tu programa tarda demasiado: ¿hay un bucle que no termina nunca?\n';
        if (alSalir) alSalir(m, true);
        ok({ salida, codigo: -1, pasadas: 0, falladas: 0 });
      }, 10000);
      w.onerror = (e) => {
        clearTimeout(limite);
        w.terminate();
        const m = 'No se pudo ejecutar HyperEZ: ' + (e.message || 'error del Worker') + '\n';
        if (alSalir) alSalir(m, true);
        ok({ salida: salida + m, codigo: -1, pasadas: 0, falladas: 0 });
      };
      w.onmessage = (e) => {
        const m = e.data;
        if (m.out) { salida += m.out; if (alSalir) alSalir(m.out, false); }
        if (m.err) { salida += m.err; if (alSalir) alSalir(m.err, true); }
        if (m.done) {
          clearTimeout(limite);
          w.terminate();
          ok({ salida, codigo: m.code, pasadas: (m.tests || {}).passed || 0, falladas: (m.tests || {}).failed || 0 });
        }
      };
      w.postMessage({ wasm, files: { '/reto.hez': codigo }, folders: [], entry: '/reto.hez', stdin: '' });
    });
  }

  // ------------------------------------------------------------------------------------------- ficha y mapa
  function pintarFicha() {
    const { actual, siguiente } = nivel();
    $('ficha-nivel').textContent = actual.nombre;
    $('ficha-xp').textContent = estado.xp + ' XP' + (siguiente ? ' · ' + (siguiente.xp - estado.xp) + ' para ' + siguiente.nombre : '');
    $('ficha-barra').style.width = (siguiente ? Math.min(100, 100 * (estado.xp - actual.xp) / (siguiente.xp - actual.xp)) : 100) + '%';
    $('ficha-misiones').textContent = hechas() + '/' + A.MISIONES.length;
    $('ficha-estrellas').textContent = A.MISIONES.reduce((s, m) => s + ((estado.misiones[m.id] || {}).estrellas || 0), 0);
    $('ficha-insignias').textContent = estado.insignias.length;
    $('ficha-avatar').textContent = ['🧙', '🧭', '💻', '🔮', '👑'][A.NIVELES.indexOf(actual)] || '🧙';
    const sig = A.MISIONES.findIndex((m) => !completada(m));
    $('boton-continuar').textContent = estado.certificado ? '🎓 Ver mi certificado' : sig < 0 ? '📝 Hacer el examen final' : hechas() ? '▶ Continuar: misión ' + (sig + 1) : '▶ Empezar la aventura';
  }
  function pintarMapa() {
    let html = '';
    for (const mundo of A.MUNDOS) {
      const ms = A.MISIONES.map((m, i) => ({ m, i })).filter((x) => x.m.mundo === mundo.id);
      html += `<div class="mundo revelar" style="--color:${mundo.color}"><div class="mundo-cabeza"><span>Mundo ${mundo.id}</span><h3>${esc(mundo.nombre)}</h3><p>${esc(mundo.descripcion)}</p></div><div class="nodos">`;
      for (const { m, i } of ms) {
        const p = estado.misiones[m.id] || {};
        const cls = completada(m) ? 'hecha' : desbloqueada(i) ? 'abierta' : 'cerrada';
        const estrellas = completada(m) ? '<span class="estrellas">' + '★'.repeat(p.estrellas || 0) + '<i>' + '★'.repeat(3 - (p.estrellas || 0)) + '</i></span>' : '';
        html += `<button class="nodo ${cls}" data-mision="${i}" ${cls === 'cerrada' ? 'aria-disabled="true"' : ''}>
          <span class="nodo-circulo">${cls === 'cerrada' ? '🔒' : m.icono}</span>
          <span class="nodo-texto"><small>Misión ${i + 1} · ${m.xp} XP</small><b>${esc(m.titulo)}</b>${estrellas}</span></button>`;
      }
      html += '</div></div>';
    }
    $('mapa').innerHTML = html;
    observar($('mapa'));
    const todas = hechas() === A.MISIONES.length;
    $('boton-examen').disabled = !todas;
    $('examen-tarjeta').classList.toggle('abierta', todas);
    if (estado.examen) $('examen-texto').innerHTML = `Tu mejor nota: <b>${estado.examen.mejor}%</b> (${estado.examen.intentos} intento${estado.examen.intentos === 1 ? '' : 's'}). ` +
      (estado.examen.mejor >= A.APROBADO ? '¡Aprobado! Puedes repetirlo para subir la nota.' : 'Necesitas un ' + A.APROBADO + ' o más: ¡inténtalo otra vez!');
  }
  $('mapa').addEventListener('click', (e) => {
    const b = e.target.closest('.nodo');
    if (!b) return;
    const i = +b.dataset.mision;
    if (!desbloqueada(i)) { b.classList.remove('temblar'); void b.offsetWidth; b.classList.add('temblar'); toast('🔒', 'Misión bloqueada', 'Completa antes la misión ' + i + '.'); return; }
    abrirMision(i);
  });
  function pintarInsignias() {
    $('insignias').innerHTML = A.INSIGNIAS.map((i) => `<div class="insignia ${estado.insignias.includes(i.id) ? 'ganada' : ''} revelar"><span>${i.icono}</span><b>${esc(i.nombre)}</b><small>${esc(i.texto)}</small></div>`).join('');
    observar($('insignias'));
  }

  // ------------------------------------------------------------------------------------------- editor de código pequeño
  function crearEditor(contenedor, codigo, alCambiar) {
    contenedor.innerHTML = '<pre class="ed-color" aria-hidden="true"></pre><textarea class="ed-texto" spellcheck="false" autocapitalize="off" autocomplete="off" wrap="off" aria-label="Tu código"></textarea>';
    const ta = contenedor.querySelector('textarea'), pre = contenedor.querySelector('pre');
    const pintar = () => { pre.innerHTML = N.highlightHtml(ta.value, []) + '\n'; pre.scrollTop = ta.scrollTop; pre.scrollLeft = ta.scrollLeft; };
    const aplicar = (c) => {
      ta.setSelectionRange(c.from, c.to);
      if (!document.execCommand('insertText', false, c.insert)) ta.setRangeText(c.insert, c.from, c.to, 'end');
      ta.setSelectionRange(c.selStart, c.selEnd);
      pintar();
      if (alCambiar) alCambiar(ta.value);
    };
    ta.value = codigo;
    ta.addEventListener('input', () => { pintar(); if (alCambiar) alCambiar(ta.value); });
    ta.addEventListener('scroll', () => { pre.scrollTop = ta.scrollTop; pre.scrollLeft = ta.scrollLeft; });
    ta.addEventListener('keydown', (e) => {
      const a = ta.selectionStart, b = ta.selectionEnd, t = ta.value;
      if (e.key === 'Tab') { e.preventDefault(); const varias = t.slice(a, b).includes('\n'); aplicar(e.shiftKey || varias ? N.indentLines(t, a, b, e.shiftKey) : { from: a, to: b, insert: '    ', selStart: a + 4, selEnd: a + 4 }); return; }
      if (e.key === 'Enter' && !e.ctrlKey) { e.preventDefault(); aplicar(N.enterIndent(t, a, b)); return; }
      if (e.key === 'Backspace') { const c = N.autoPair(t, a, b, 'Backspace'); if (c) { e.preventDefault(); aplicar(c); } return; }
      if (e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey) {
        const c = N.outdentOnClose(t, a, e.key) || N.autoPair(t, a, b, e.key);
        if (c) { e.preventDefault(); aplicar(c); }
      }
    });
    pintar();
    return { get valor() { return ta.value; }, set valor(v) { ta.value = v; pintar(); if (alCambiar) alCambiar(v); }, foco() { ta.focus(); }, ta };
  }

  // ------------------------------------------------------------------------------------------- la misión
  let actual = null; // { i, m, paso, xpGanado, preguntas... }
  function abrirMision(i) {
    const m = A.MISIONES[i];
    actual = { i, m, paso: 0, xpGanado: 0 };
    const mundo = A.MUNDOS.find((x) => x.id === m.mundo);
    $('mision').style.setProperty('--color', mundo.color);
    $('mision-icono').textContent = m.icono;
    $('mision-mundo').textContent = 'Mundo ' + mundo.id + ' · ' + mundo.nombre + ' · Misión ' + (i + 1);
    $('mision-titulo').textContent = m.titulo;
    $('mision').hidden = false;
    document.body.classList.add('con-mision');
    requestAnimationFrame(() => $('mision').classList.add('visible'));
    history.replaceState(null, '', '#mision-' + m.id);
    pintarXpMision();
    irAPaso(0);
  }
  function cerrarMision() {
    $('mision').classList.remove('visible');
    document.body.classList.remove('con-mision');
    setTimeout(() => { $('mision').hidden = true; $('mision-cuerpo').innerHTML = ''; }, reducido ? 0 : 280);
    history.replaceState(null, '', location.pathname);
    actual = null;
    pintarMapa(); pintarFicha();
  }
  $('mision-cerrar').addEventListener('click', cerrarMision);
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape' && actual && !document.activeElement.matches('textarea')) cerrarMision(); });
  function pintarXpMision() { $('mision-xp').textContent = '+' + actual.xpGanado + ' XP'; }
  function ganar(n, motivo) { actual.xpGanado += n; pintarXpMision(); sumarXp(n, motivo); }
  function irAPaso(p) {
    actual.paso = p;
    for (const li of $('mision-pasos').children) {
      const k = +li.dataset.paso;
      li.className = k < p ? 'hecho' : k === p ? 'actual' : '';
    }
    const cuerpo = $('mision-cuerpo');
    cuerpo.classList.remove('entra'); void cuerpo.offsetWidth; cuerpo.classList.add('entra');
    cuerpo.scrollTop = 0;
    if (p === 0) pasoLeccion(); else if (p === 1) pasoReto(); else if (p === 2) pasoPreguntas(); else pasoFin();
  }

  // -- 1. lección
  function pasoLeccion() {
    const m = actual.m;
    let html = `<div class="historia"><span>📖</span><p>${esc(m.historia)}</p></div><div class="leccion">`;
    m.leccion.forEach((b, k) => {
      if (b.texto) html += b.texto;
      else html += `<div class="ejemplo-vivo"><pre class="codigo">${N.highlightHtml(b.codigo, [])}</pre><div class="ejemplo-barra"><button class="btn-mini" data-ejemplo="${k}">▶ Ejecutar</button><button class="btn-mini claro" data-copiar="${k}">Copiar</button></div><pre class="consola" hidden></pre></div>`;
    });
    html += '</div><div class="mision-pie"><span></span><button class="btn btn-verde btn-brillo" id="ir-reto">Al reto →</button></div>';
    $('mision-cuerpo').innerHTML = html;
    $('mision-cuerpo').querySelectorAll('[data-ejemplo]').forEach((b) => b.addEventListener('click', async () => {
      const consola = b.closest('.ejemplo-vivo').querySelector('.consola');
      consola.hidden = false; consola.textContent = ''; consola.classList.add('cargando');
      b.disabled = true;
      await ejecutar(m.leccion[+b.dataset.ejemplo].codigo, (t, err) => { consola.classList.remove('cargando'); const s = document.createElement('span'); if (err) s.className = 'err'; s.textContent = t; consola.appendChild(s); });
      consola.classList.remove('cargando');
      b.disabled = false;
    }));
    $('mision-cuerpo').querySelectorAll('[data-copiar]').forEach((b) => b.addEventListener('click', () => {
      navigator.clipboard.writeText(m.leccion[+b.dataset.copiar].codigo).then(() => toast('📋', 'Copiado', 'Pégalo en el reto para probarlo'));
    }));
    $('ir-reto').addEventListener('click', () => irAPaso(1));
  }

  // -- 2. reto
  function pasoReto() {
    const m = actual.m, r = m.reto, p = progreso(m.id);
    const esperado = r.esperado ? `<div class="esperado"><small>Salida esperada</small><pre>${esc(r.esperado)}</pre></div>` : '';
    $('mision-cuerpo').innerHTML = `<div class="reto">
      <div class="reto-enunciado"><h3>🎯 Tu reto</h3><div>${r.enunciado}</div>${esperado}
        <div class="pistas" id="pistas"></div></div>
      <div class="reto-trabajo">
        <div class="ed" id="ed"></div>
        <div class="reto-botones">
          <button class="btn btn-oscuro" id="b-ejecutar">▶ Ejecutar</button>
          <button class="btn btn-verde btn-brillo" id="b-comprobar">✓ Comprobar</button>
          <button class="btn-mini claro" id="b-pista">💡 Pista</button>
          <button class="btn-mini claro" id="b-reiniciar">↺ Reiniciar</button>
          <button class="btn-mini claro" id="b-solucion" hidden>👁 Ver la solución</button>
        </div>
        <div class="resultado" id="resultado" hidden></div>
        <pre class="consola" id="consola">La salida de tu programa aparecerá aquí.</pre>
      </div></div>
      <div class="mision-pie"><button class="btn btn-claro" id="volver-leccion">← Lección</button><button class="btn btn-verde" id="ir-preguntas" ${p.reto ? '' : 'disabled'}>Preguntas →</button></div>`;
    const ed = crearEditor($('ed'), p.codigo || r.inicial, (v) => { p.codigo = v; guardar(); });
    const consola = $('consola');
    let fallos = 0;
    const pintarPistas = () => {
      $('pistas').innerHTML = r.pistas.slice(0, p.pistas).map((t, k) => `<div class="pista"><b>Pista ${k + 1}</b> ${t}</div>`).join('');
      $('b-pista').textContent = p.pistas < r.pistas.length ? '💡 Pista (' + (r.pistas.length - p.pistas) + ')' : '💡 Sin más pistas';
      $('b-pista').disabled = p.pistas >= r.pistas.length;
      $('b-solucion').hidden = !(fallos >= 3 || p.pistas >= r.pistas.length) || p.reto;
    };
    pintarPistas();
    const escribir = (t, err) => { const s = document.createElement('span'); if (err) s.className = 'err'; s.textContent = t; consola.appendChild(s); consola.scrollTop = consola.scrollHeight; };
    const ocupado = (si) => { $('b-ejecutar').disabled = $('b-comprobar').disabled = si; };
    $('b-ejecutar').addEventListener('click', async () => {
      ocupado(true); consola.textContent = ''; consola.classList.add('cargando');
      const res = await ejecutar(ed.valor, (t, err) => { consola.classList.remove('cargando'); escribir(t, err); });
      consola.classList.remove('cargando');
      if (!res.salida) escribir('(tu programa no ha mostrado nada)', false);
      ocupado(false);
    });
    $('b-comprobar').addEventListener('click', async () => {
      ocupado(true); consola.textContent = ''; consola.classList.add('cargando');
      const res = await window.HZReto.comprobar(r, ed.valor, (codigo) => ejecutar(codigo));
      consola.classList.remove('cargando');
      if (res.r) escribir(res.r.salida || '(tu programa no ha mostrado nada)', res.r.codigo !== 0);
      const caja = $('resultado');
      caja.hidden = false;
      if (res.ok) {
        caja.className = 'resultado bien';
        caja.innerHTML = '<b>✓ ¡Reto superado!</b> HyperEZ ha comprobado tu programa.';
        if (!p.reto) {
          p.reto = true; guardar();
          const xp = p.solucion ? Math.round(m.xp * .3) : Math.max(Math.round(m.xp * .4), m.xp - p.pistas * 20);
          ganar(xp, p.solucion ? 'Reto (con la solución)' : p.pistas ? 'Reto superado con ' + p.pistas + ' pista' + (p.pistas > 1 ? 's' : '') : '¡Reto superado sin pistas!');
          confeti();
        }
        $('ir-preguntas').disabled = false;
        $('b-solucion').hidden = true;
      } else {
        fallos++;
        caja.className = 'resultado mal';
        let extra = '';
        if (res.dif) extra = `<div class="dif"><span>Línea ${res.dif.linea}</span><div><small>Tu programa</small><code>${esc(res.dif.obtenida)}</code></div><div><small>Se esperaba</small><code>${esc(res.dif.esperada)}</code></div></div>`;
        caja.innerHTML = '<b>Casi…</b> ' + res.mensaje + extra;
        caja.classList.remove('temblar'); void caja.offsetWidth; caja.classList.add('temblar');
        pintarPistas();
      }
      ocupado(false);
    });
    $('b-pista').addEventListener('click', () => { if (p.pistas < r.pistas.length) { p.pistas++; guardar(); pintarPistas(); } });
    $('b-reiniciar').addEventListener('click', () => { ed.valor = r.inicial; ed.foco(); });
    $('b-solucion').addEventListener('click', () => { p.solucion = true; guardar(); ed.valor = r.solucion; toast('👁', 'Esta es una solución', 'Léela con calma y pulsa ✓ Comprobar'); });
    $('volver-leccion').addEventListener('click', () => irAPaso(0));
    $('ir-preguntas').addEventListener('click', () => irAPaso(2));
    ed.foco();
  }

  // -- 3. preguntas
  function pasoPreguntas() {
    const m = actual.m, p = progreso(m.id);
    let k = 0, aciertos = 0;
    const primera = p.quiz === null; // solo cuenta para la nota la primera vez
    const pintar = () => {
      const q = m.preguntas[k];
      $('mision-cuerpo').innerHTML = `<div class="quiz">
        <div class="quiz-progreso">${m.preguntas.map((_, j) => `<i class="${j < k ? 'hecha' : j === k ? 'actual' : ''}"></i>`).join('')}</div>
        <p class="quiz-num">Pregunta ${k + 1} de ${m.preguntas.length}${primera ? '' : ' · repaso (la nota ya está guardada)'}</p>
        <h3>${q.p}</h3>
        <div class="opciones">${q.o.map((o, j) => `<button class="opcion" data-o="${j}"><span>${'ABCD'[j]}</span>${o.includes('<') ? o : esc(o)}</button>`).join('')}</div>
        <div class="explicacion" id="explicacion" hidden></div>
        <div class="mision-pie"><span></span><button class="btn btn-verde" id="siguiente" hidden>${k + 1 < m.preguntas.length ? 'Siguiente →' : 'Terminar la misión →'}</button></div></div>`;
      $('mision-cuerpo').querySelectorAll('.opcion').forEach((b) => b.addEventListener('click', () => {
        if ($('mision-cuerpo').querySelector('.opcion.elegida')) return;
        const j = +b.dataset.o, bien = j === q.ok;
        b.classList.add('elegida', bien ? 'bien' : 'mal');
        if (!bien) $('mision-cuerpo').querySelector(`[data-o="${q.ok}"]`).classList.add('bien');
        $('mision-cuerpo').querySelectorAll('.opcion').forEach((x) => { x.disabled = true; });
        if (bien) { aciertos++; if (primera) ganar(10, 'Respuesta correcta'); }
        const ex = $('explicacion');
        ex.hidden = false;
        ex.className = 'explicacion ' + (bien ? 'bien' : 'mal');
        ex.innerHTML = (bien ? '<b>¡Correcto!</b> ' : '<b>No exactamente.</b> ') + q.porque;
        $('siguiente').hidden = false;
        $('siguiente').focus();
      }));
      $('siguiente').addEventListener('click', () => {
        k++;
        if (k < m.preguntas.length) { const c = $('mision-cuerpo'); c.classList.remove('entra'); void c.offsetWidth; c.classList.add('entra'); pintar(); return; }
        if (primera) {
          p.quiz = Math.round(100 * aciertos / m.preguntas.length);
          p.estrellas = 1 + (p.quiz === 100 ? 1 : 0) + (!p.pistas && !p.solucion ? 1 : 0);
          guardar();
        }
        irAPaso(3);
      });
    };
    pintar();
  }

  // -- 4. fin de la misión
  function pasoFin() {
    const m = actual.m, p = progreso(m.id), sig = actual.i + 1 < A.MISIONES.length ? A.MISIONES[actual.i + 1] : null;
    revisarInsignias();
    confeti();
    $('mision-cuerpo').innerHTML = `<div class="fin">
      <div class="fin-icono">${m.icono}</div>
      <h2>¡Misión completada!</h2>
      <div class="fin-estrellas">${[0, 1, 2].map((j) => `<span class="${j < p.estrellas ? 'si' : ''}" style="--d:${j * 0.25}s">★</span>`).join('')}</div>
      <p>Preguntas: <b>${p.quiz}%</b> a la primera · ${p.pistas ? p.pistas + ' pista' + (p.pistas > 1 ? 's' : '') : 'sin pistas'} · <b>+${actual.xpGanado} XP</b> en esta visita</p>
      <p class="fin-truco">⭐ Una estrella por completarla, otra por acertar todas las preguntas y otra por no pedir pistas.</p>
      <div class="acciones">${sig ? `<button class="btn btn-verde btn-brillo" id="fin-siguiente">Siguiente: ${esc(sig.titulo)} →</button>` : `<button class="btn btn-verde btn-brillo" id="fin-examen">📝 Al examen final</button>`}<button class="btn btn-claro" id="fin-mapa">Volver al mapa</button></div></div>`;
    $('fin-mapa').addEventListener('click', cerrarMision);
    if (sig) $('fin-siguiente').addEventListener('click', () => abrirMision(actual.i + 1));
    else $('fin-examen').addEventListener('click', () => { cerrarMision(); abrirExamen(); });
    pintarFicha();
  }

  // ------------------------------------------------------------------------------------------- examen
  function abrirExamen() {
    if (hechas() < A.MISIONES.length) return;
    const orden = A.EXAMEN.map((q, j) => j).sort(() => Math.random() - .5);
    let k = 0, aciertos = 0;
    actual = { i: -1, m: { id: 'examen' }, paso: 0, xpGanado: 0 };
    $('mision').style.setProperty('--color', '#62d962');
    $('mision-icono').textContent = '📝';
    $('mision-mundo').textContent = 'Academia HyperEZ · Prueba final';
    $('mision-titulo').textContent = 'Examen final';
    $('mision-pasos').hidden = true;
    $('mision').hidden = false;
    document.body.classList.add('con-mision');
    requestAnimationFrame(() => $('mision').classList.add('visible'));
    pintarXpMision();
    const pintar = () => {
      const q = A.EXAMEN[orden[k]];
      $('mision-cuerpo').innerHTML = `<div class="quiz">
        <div class="quiz-progreso">${orden.map((_, j) => `<i class="${j < k ? 'hecha' : j === k ? 'actual' : ''}"></i>`).join('')}</div>
        <p class="quiz-num">Pregunta ${k + 1} de ${orden.length}</p><h3>${q.p}</h3>
        <div class="opciones">${q.o.map((o, j) => `<button class="opcion" data-o="${j}"><span>${'ABCD'[j]}</span>${o.includes('<') ? o : esc(o)}</button>`).join('')}</div></div>`;
      $('mision-cuerpo').querySelectorAll('.opcion').forEach((b) => b.addEventListener('click', () => {
        if (+b.dataset.o === q.ok) aciertos++;
        b.classList.add('elegida');
        setTimeout(() => { k++; if (k < orden.length) { const c = $('mision-cuerpo'); c.classList.remove('entra'); void c.offsetWidth; c.classList.add('entra'); pintar(); } else terminar(); }, 260);
      }));
    };
    const terminar = () => {
      const nota = Math.round(100 * aciertos / orden.length);
      const ex = estado.examen || { mejor: 0, intentos: 0 };
      ex.intentos++; ex.mejor = Math.max(ex.mejor, nota); estado.examen = ex; guardar();
      const aprobado = nota >= A.APROBADO;
      if (aprobado) { ganar(200, 'Examen aprobado'); confeti(); }
      $('mision-cuerpo').innerHTML = `<div class="fin">
        <div class="fin-icono">${aprobado ? '🎓' : '📚'}</div>
        <h2>${aprobado ? '¡Aprobado!' : 'Casi lo tienes'}</h2>
        <div class="nota-grande" style="--nota:${nota}"><span>${nota}%</span></div>
        <p>${aciertos} de ${orden.length} respuestas correctas. ${aprobado ? 'Tu nota media para el certificado: <b>' + notaMedia() + ' / 100</b> (la mitad, las preguntas de las misiones; la otra mitad, tu mejor examen).' : 'Necesitas un ' + A.APROBADO + '%. Repasa las lecciones y vuelve a intentarlo.'}</p>
        <div class="acciones">${aprobado ? '<button class="btn btn-verde btn-brillo" id="fin-cert">🎓 Conseguir mi certificado</button>' : '<button class="btn btn-verde" id="fin-otra">Intentarlo otra vez</button>'}<button class="btn btn-claro" id="fin-mapa">Volver</button></div></div>`;
      $('fin-mapa').addEventListener('click', () => { $('mision-pasos').hidden = false; cerrarMision(); });
      if (aprobado) $('fin-cert').addEventListener('click', () => { $('mision-pasos').hidden = false; cerrarMision(); pintarCertificado(); $('certificado-seccion').scrollIntoView({ behavior: reducido ? 'auto' : 'smooth' }); setTimeout(() => $('cert-nombre') && $('cert-nombre').focus(), 600); });
      else $('fin-otra').addEventListener('click', abrirExamen);
    };
    pintar();
  }
  $('boton-examen').addEventListener('click', abrirExamen);
  $('boton-continuar').addEventListener('click', () => {
    const sig = A.MISIONES.findIndex((m) => !completada(m));
    if (estado.certificado || (sig < 0 && estado.examen && estado.examen.mejor >= A.APROBADO)) $('certificado-seccion').scrollIntoView({ behavior: 'smooth' });
    else if (sig < 0) abrirExamen();
    else abrirMision(sig);
  });

  // ------------------------------------------------------------------------------------------- certificado
  const base = new URL('.', location.href).href;
  function pintarCertificado() {
    const c = estado.certificado;
    const puede = estado.examen && estado.examen.mejor >= A.APROBADO;
    const marco = $('cert-marco');
    if (c) {
      const url = C.enlace(c, base);
      marco.innerHTML = C.svg(c, url, IMAGENES);
      marco.classList.add('emitido');
      $('cert-form').hidden = true;
      $('cert-acciones').hidden = false;
      $('cert-verificar').href = url;
      $('cert-linkedin').href = 'https://www.linkedin.com/sharing/share-offsite/?url=' + encodeURIComponent(url);
      $('cert-sub').innerHTML = `Emitido a <b>${esc(c.n)}</b> el ${C.fechaLarga(c.f)} · nota <b>${c.p} / 100</b> · código <code>${esc(c.id)}</code>`;
    } else {
      marco.innerHTML = C.svg(null, null, IMAGENES);
      marco.classList.toggle('bloqueado', !puede);
      $('cert-form').hidden = !puede;
      $('cert-acciones').hidden = true;
      if (puede) $('cert-sub').innerHTML = `¡Lo has conseguido! Tu nota media es <b>${notaMedia()} / 100</b>. Escribe tu nombre para emitir el certificado.`;
    }
  }
  $('cert-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const nombre = C.limpiarNombre($('cert-nombre').value);
    if (nombre.length < 3) { toast('✍️', 'Escribe tu nombre completo', ''); return; }
    estado.certificado = await C.emitir(nombre, notaMedia(), hechas());
    guardar();
    revisarInsignias();
    pintarCertificado();
    confeti();
    toast('🎓', '¡Certificado emitido!', 'Descárgalo en PDF o comparte su enlace de verificación', true);
  });
  $('cert-pdf').addEventListener('click', () => {
    const c = estado.certificado;
    $('cert-impresion').innerHTML = C.svg(c, C.enlace(c, base), IMAGENES);
    const titulo = document.title;
    document.title = 'Certificado HyperEZ - ' + c.n;
    setTimeout(() => { window.print(); document.title = titulo; }, 150);
  });
  $('cert-png').addEventListener('click', async () => {
    const c = estado.certificado;
    const blob = await C.png(c, C.enlace(c, base), IMAGENES, 3);
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'Certificado HyperEZ - ' + c.n + '.png';
    document.body.appendChild(a); a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
  });
  $('cert-copiar').addEventListener('click', () => {
    navigator.clipboard.writeText(C.enlace(estado.certificado, base)).then(() => toast('🔗', 'Enlace copiado', 'Quien lo abra verá que tu certificado es auténtico'));
  });
  // inclinación 3D del certificado al pasar el ratón
  $('cert-marco').addEventListener('pointermove', (e) => {
    if (reducido) return;
    const r = e.currentTarget.getBoundingClientRect();
    e.currentTarget.style.setProperty('--rx', ((e.clientY - r.top) / r.height - .5) * -6 + 'deg');
    e.currentTarget.style.setProperty('--ry', ((e.clientX - r.left) / r.width - .5) * 8 + 'deg');
  });
  $('cert-marco').addEventListener('pointerleave', (e) => { e.currentTarget.style.setProperty('--rx', '0deg'); e.currentTarget.style.setProperty('--ry', '0deg'); });

  $('boton-reiniciar').addEventListener('click', () => {
    if (!confirm('¿Borrar todo tu progreso de la Academia (misiones, experiencia, medallas y certificado) en este navegador?')) return;
    localStorage.removeItem(CLAVE);
    location.reload();
  });

  // ------------------------------------------------------------------------------------------- aparecer al hacer scroll
  const io = 'IntersectionObserver' in window && !reducido ? new IntersectionObserver((es) => {
    for (const e of es) if (e.isIntersecting) { e.target.classList.add('visible'); io.unobserve(e.target); }
  }, { threshold: .12 }) : null;
  function observar(raiz) {
    raiz.querySelectorAll('.revelar:not(.visible)').forEach((el, k) => {
      el.style.setProperty('--retardo', (k % 6) * 70 + 'ms');
      if (io) io.observe(el); else el.classList.add('visible');
    });
  }

  // ------------------------------------------------------------------------------------------- arranque
  pintarFicha();
  pintarMapa();
  pintarInsignias();
  pintarCertificado();
  revisarInsignias();
  const h = /^#mision-(.+)$/.exec(location.hash);
  if (h) { const i = A.MISIONES.findIndex((m) => m.id === h[1]); if (i >= 0 && desbloqueada(i)) abrirMision(i); }
  window.HZAcademia = { estado, abrirMision, abrirExamen, ejecutar }; // para las pruebas
})();
