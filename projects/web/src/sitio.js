// La web de HyperEZ: el botón de descarga según el sistema, los archivos de la última versión (de GitHub),
// el menú del móvil, las pestañas de ejemplos de código, los filtros y el buscador de la documentación.
(function () {
  'use strict';
  const REPO = 'cuevacaravacaadrian/HyperEZ';
  const $$ = (s, r) => Array.from((r || document).querySelectorAll(s));

  // ---- ¿qué sistema usa quien visita? ----
  const plataforma = ((navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || navigator.userAgent || '').toLowerCase();
  const so = /win/.test(plataforma) ? 'windows' : /mac|iphone|ipad/.test(plataforma) ? 'macos' : /linux|x11|cros|android/.test(plataforma) ? 'linux' : 'windows';
  const NOMBRE = { windows: 'Windows', macos: 'macOS', linux: 'Linux' };
  for (const b of $$('[data-descarga-principal]')) {
    const t = b.querySelector('[data-so]');
    if (t) t.textContent = NOMBRE[so];
    b.href = b.getAttribute('href').replace(/#.*$/, '') + '#' + so;
  }
  const tarjeta = document.getElementById(so);
  if (tarjeta && tarjeta.classList.contains('sistema')) tarjeta.classList.add('recomendado');

  // ---- archivos de la última versión publicada ----
  const enlaces = $$('[data-archivo]');
  if (enlaces.length || $$('[data-ultima-version]').length) {
    const cache = sessionStorage.getItem('hz-release');
    const pedir = cache ? Promise.resolve(JSON.parse(cache)) :
      fetch('https://api.github.com/repos/' + REPO + '/releases/latest', { headers: { Accept: 'application/vnd.github+json' } })
        .then((r) => (r.ok ? r.json() : Promise.reject(r.status)))
        .then((d) => { sessionStorage.setItem('hz-release', JSON.stringify({ tag_name: d.tag_name, html_url: d.html_url, published_at: d.published_at,
          assets: d.assets.map((a) => ({ name: a.name, size: a.size, url: a.browser_download_url, downloads: a.download_count })) })); return JSON.parse(sessionStorage.getItem('hz-release')); });
    pedir.then((rel) => {
      const version = rel.tag_name.replace(/^v/, '');
      for (const el of $$('[data-ultima-version]')) el.textContent = version;
      for (const el of $$('[data-fecha-version]')) el.textContent = new Date(rel.published_at).toLocaleDateString('es', { day: 'numeric', month: 'long', year: 'numeric' });
      for (const a of enlaces) {
        const re = new RegExp(a.dataset.archivo, 'i');
        const asset = rel.assets.find((x) => re.test(x.name));
        if (!asset) { if (a.dataset.siFalta === 'ocultar') (a.closest('li') || a).hidden = true; continue; }
        a.href = asset.url;
        const tam = a.querySelector('[data-tam]') || (a.closest('li') && a.closest('li').querySelector('[data-tam]'));
        if (tam) tam.textContent = (asset.size / 1048576).toLocaleString('es', { maximumFractionDigits: 1 }) + ' MB';
        const nombre = a.querySelector('[data-nombre]');
        if (nombre) nombre.textContent = asset.name;
      }
      const aviso = document.getElementById('aviso-version');
      if (aviso && aviso.dataset.web && aviso.dataset.web !== version) aviso.hidden = false;
    }).catch(() => { /* sin conexión con GitHub: los enlaces llevan a la página de versiones */ });
  }

  // ---- menú del móvil ----
  const boton = document.querySelector('.menu-movil');
  if (boton) boton.addEventListener('click', () => document.querySelector('.nav').classList.toggle('abierta'));

  // ---- pestañas de código de la portada ----
  for (const grupo of $$('.pestanas-codigo')) {
    grupo.addEventListener('click', (e) => {
      const b = e.target.closest('button');
      if (!b) return;
      $$('button', grupo).forEach((x) => x.classList.toggle('activa', x === b));
      $$('.muestra', grupo.parentElement).forEach((m) => { m.hidden = m.dataset.muestra !== b.dataset.muestra; });
    });
  }

  // ---- filtros de ejemplos ----
  const filtros = document.querySelector('.filtros');
  if (filtros) filtros.addEventListener('click', (e) => {
    const b = e.target.closest('button');
    if (!b) return;
    $$('button', filtros).forEach((x) => x.classList.toggle('activa', x === b));
    for (const t of $$('.ejemplo')) t.hidden = b.dataset.cat !== '*' && t.dataset.cat !== b.dataset.cat;
  });

  // ---- buscar en el menú de la documentación ----
  const buscar = document.querySelector('.docs-buscar');
  if (buscar) buscar.addEventListener('input', () => {
    const q = buscar.value.trim().toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '');
    for (const li of $$('.docs-menu li')) {
      if (li.classList.contains('grupo')) { li.hidden = !!q; continue; }
      const t = li.textContent.toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '');
      li.hidden = q && !t.includes(q);
    }
  });
})();

// ---- animaciones de la web: aparecer al hacer scroll, contadores, brillo de las tarjetas, barra de progreso ----
(function () {
  'use strict';
  const reducido = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const $$ = (s, r) => Array.from((r || document).querySelectorAll(s));
  const yo = document.querySelector('script[src$="sitio.js"]');
  const raiz = yo ? yo.getAttribute('src').replace(/sitio\.js$/, '') : '';

  // auroras detrás de las portadas que no la tienen
  for (const el of $$('.portada, .titular, .llamada')) {
    if (el.querySelector('.aurora')) continue;
    const a = document.createElement('div');
    a.className = 'aurora'; a.setAttribute('aria-hidden', 'true'); a.innerHTML = '<i></i><i></i><i></i>';
    el.insertBefore(a, el.firstChild);
  }

  // barra de progreso de lectura
  const barra = document.querySelector('.progreso-scroll');
  if (barra) {
    let pendiente = false;
    const pintar = () => { pendiente = false; const h = document.documentElement; barra.style.transform = 'scaleX(' + (h.scrollTop / Math.max(1, h.scrollHeight - h.clientHeight)) + ')'; };
    addEventListener('scroll', () => { if (!pendiente) { pendiente = true; requestAnimationFrame(pintar); } }, { passive: true });
    pintar();
  }

  // aparecer al hacer scroll, uno detrás de otro dentro de cada grupo
  const candidatos = $$('.seccion h2, .seccion .sub, .tarjeta, .paso, .dos > *, .ventana, .cifras > div, .sistema, .version, .faq details, .llamada, .persona, .ejemplo, .promo-ruta li, .promo-cert, .instrucciones > div')
    .filter((el) => !el.closest('.mision, .revelar, .portada'));
  if (!reducido && 'IntersectionObserver' in window) {
    const io = new IntersectionObserver((es) => {
      for (const e of es) if (e.isIntersecting) { e.target.classList.add('visible'); io.unobserve(e.target); }
    }, { threshold: .1, rootMargin: '0px 0px -40px 0px' });
    const grupos = new Map();
    for (const el of candidatos) {
      const k = el.parentElement;
      const n = grupos.get(k) || 0;
      grupos.set(k, n + 1);
      el.classList.add('revelar');
      el.style.setProperty('--retardo', Math.min(n, 6) * 80 + 'ms');
      io.observe(el);
    }
  }

  // los números de la portada cuentan hacia arriba
  const cifras = $$('.cifras b');
  if (cifras.length && !reducido && 'IntersectionObserver' in window) {
    const io = new IntersectionObserver((es) => {
      for (const e of es) {
        if (!e.isIntersecting) continue;
        io.unobserve(e.target);
        const el = e.target, fin = parseInt(el.textContent.replace(/\D/g, ''), 10);
        if (!fin) continue;
        const t0 = performance.now(), dur = 1400;
        (function paso(t) {
          const x = Math.min(1, (t - t0) / dur), v = Math.round(fin * (1 - Math.pow(1 - x, 3)));
          el.textContent = v.toLocaleString('es');
          if (x < 1) requestAnimationFrame(paso);
        })(t0);
      }
    }, { threshold: .5 });
    cifras.forEach((c) => io.observe(c));
  }

  // brillo que sigue al ratón en las tarjetas
  document.addEventListener('pointermove', (e) => {
    const t = e.target.closest && e.target.closest('.tarjeta, .sistema');
    if (!t) return;
    const r = t.getBoundingClientRect();
    t.style.setProperty('--mx', (e.clientX - r.left) + 'px');
    t.style.setProperty('--my', (e.clientY - r.top) + 'px');
  }, { passive: true });

  // la muestra del certificado de la Academia (portada)
  const muestra = document.querySelector('[data-certificado-muestra]');
  if (muestra) {
    const cargar = (src) => new Promise((ok, mal) => { const s = document.createElement('script'); s.src = raiz + src; s.onload = ok; s.onerror = mal; document.head.appendChild(s); });
    cargar('aprender/qrcode.js').then(() => cargar('aprender/certificado.js')).then(() => {
      const C = window.HZCertificado;
      muestra.innerHTML = C.svg({ n: 'Tu nombre aquí', p: 95, id: 'HZ-' + new Date().getFullYear() + '-0000-0000' }, new URL(raiz + 'verificar.html', location.href).href,
        { logo: raiz + 'aprender/cert-logo.png', firma: raiz + 'aprender/cert-firma.png' });
    }).catch(() => {});
  }
})();
