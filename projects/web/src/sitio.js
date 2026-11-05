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
