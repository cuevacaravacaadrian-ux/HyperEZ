// El certificado de HyperEZ (Black Edition): el mismo diseño del PDF original (A4 apaisado, 841,89 × 595,28 pt),
// dibujado en SVG con los datos de quien lo obtiene, su código de identificación y un QR que lleva a la página de
// verificación. Lo usan aprender.html (para emitirlo) y verificar.html (para comprobarlo).
//
// Qué se comprueba: el código (HZ-AAAA-XXXX-XXXX) es un resumen SHA-256 de los datos del certificado; la página de
// verificación lo recalcula a partir del enlace del QR, así que si alguien cambia el nombre o la nota, no coincide.
// La web no tiene servidor ni base de datos: no guarda una lista de certificados emitidos.
(function (global) {
  'use strict';
  const W = 841.8898, H = 595.2756;
  const NEGRO = '#101010', GRIS = '#737373', GRIS_OSC = '#343434', LINEA = '#d9d9d9';
  const LETRA = "Helvetica, 'Helvetica Neue', Arial, sans-serif";
  const VERSION = 1;

  const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

  function b64url(texto) {
    const bytes = new TextEncoder().encode(texto);
    let bin = '';
    for (const b of bytes) bin += String.fromCharCode(b);
    return btoa(bin).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
  }
  function desdeB64url(s) {
    const bin = atob(s.replace(/-/g, '+').replace(/_/g, '/'));
    return new TextDecoder().decode(Uint8Array.from(bin, (c) => c.charCodeAt(0)));
  }

  /** El nombre tal como va en el certificado: sin espacios de más. */
  function limpiarNombre(n) { return String(n || '').replace(/\s+/g, ' ').trim().slice(0, 80); }

  async function sha256Hex(texto) {
    const d = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(texto));
    return Array.from(new Uint8Array(d), (b) => b.toString(16).padStart(2, '0')).join('');
  }

  /** El código de identificación: HZ-AAAA-XXXX-XXXX (año de emisión y 8 cifras del resumen de los datos). */
  async function calcularId(d) {
    const h = (await sha256Hex(['hyperez-certificado', d.v, d.n, d.p, d.f, d.m, d.r].join('|'))).toUpperCase();
    return 'HZ-' + d.f.slice(0, 4) + '-' + h.slice(0, 4) + '-' + h.slice(4, 8);
  }

  /** Crea los datos de un certificado nuevo: nombre, nota (0-100), misiones y la fecha de hoy. */
  async function emitir(nombre, nota, misiones) {
    const r = Array.from(crypto.getRandomValues(new Uint8Array(3)), (b) => b.toString(16).padStart(2, '0')).join('');
    const hoy = new Date();
    const f = hoy.getFullYear() + '-' + String(hoy.getMonth() + 1).padStart(2, '0') + '-' + String(hoy.getDate()).padStart(2, '0');
    const d = { v: VERSION, n: limpiarNombre(nombre), p: Math.max(0, Math.min(100, Math.round(nota))), f, m: misiones, r };
    d.id = await calcularId(d);
    return d;
  }

  /** El enlace de verificación (el que lleva el QR). */
  function enlace(d, base) {
    const u = new URL('verificar.html', base || location.href);
    u.search = '?c=' + b64url(JSON.stringify({ v: d.v, n: d.n, p: d.p, f: d.f, m: d.m, r: d.r }));
    return u.toString();
  }

  /** Lee un certificado de un enlace o código y comprueba que su identificación coincide. */
  async function verificar(textoOEnlace) {
    let c = String(textoOEnlace || '').trim();
    const m = /[?&]c=([A-Za-z0-9_-]+)/.exec(c);
    if (m) c = m[1];
    if (!/^[A-Za-z0-9_-]{10,}$/.test(c)) return { ok: false, motivo: 'formato' };
    let d;
    try { d = JSON.parse(desdeB64url(c)); } catch (e) { return { ok: false, motivo: 'formato' }; }
    if (!d || d.v !== VERSION || typeof d.n !== 'string' || typeof d.p !== 'number' || !/^\d{4}-\d{2}-\d{2}$/.test(d.f || '')) return { ok: false, motivo: 'formato' };
    d.id = await calcularId(d);
    return { ok: true, datos: d };
  }

  function fechaLarga(f, idioma) {
    const [a, m, dia] = f.split('-').map(Number);
    return new Date(a, m - 1, dia).toLocaleDateString(idioma || 'es', { day: 'numeric', month: 'long', year: 'numeric' });
  }

  /** El QR como cuadrados SVG (qrcode-generator, MIT), dentro de la caja del certificado. */
  function qrSvg(texto, x, y, lado) {
    if (typeof qrcode !== 'function') return '';
    const qr = qrcode(0, 'L'); // L: menos denso (cabe mejor en la caja de 18 mm y se lee mejor con el móvil)
    qr.addData(texto);
    qr.make();
    const n = qr.getModuleCount(), m = lado / (n + 2); // un módulo de margen a cada lado
    let p = '';
    for (let r = 0; r < n; r++)
      for (let c = 0; c < n; c++)
        if (qr.isDark(r, c)) p += 'M' + (x + (c + 1) * m).toFixed(3) + ' ' + (y + (r + 1) * m).toFixed(3) + 'h' + m.toFixed(3) + 'v' + m.toFixed(3) + 'h-' + m.toFixed(3) + 'z';
    return `<rect x="${x}" y="${y}" width="${lado}" height="${lado}" fill="#fff"/><path d="${p}" fill="${NEGRO}" shape-rendering="crispEdges"/>`;
  }

  /**
   * El certificado en SVG. datos = {n, p, id}; qr = el enlace de verificación; imagenes = {logo, firma} (rutas o data:).
   * Sin datos (vista previa) deja las líneas vacías, como el PDF original.
   */
  function svg(datos, qr, imagenes) {
    const img = imagenes || {};
    const t = (x, y, txt, tam, color, peso, ancla, extra) =>
      `<text x="${x}" y="${y}" font-family="${LETRA}" font-size="${tam}" fill="${color}"${peso ? ' font-weight="700"' : ''} text-anchor="${ancla || 'middle'}"${extra || ''}>${esc(txt)}</text>`;
    const l = (x1, y, x2, color, ancho) => `<line x1="${x1}" y1="${y}" x2="${x2}" y2="${y}" stroke="${color}" stroke-width="${ancho}"/>`;
    const cx = W / 2;
    const d = datos || {};
    let nombre = '';
    if (d.n) {
      // los nombres largos se ajustan al ancho de la línea
      const tam = d.n.length > 40 ? 17 : d.n.length > 30 ? 20 : 24;
      const ajuste = d.n.length > 46 ? ' textLength="520" lengthAdjust="spacingAndGlyphs"' : '';
      nombre = t(cx, 298, d.n, tam, NEGRO, true, 'middle', ajuste + ' letter-spacing=".2"');
    }
    return `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 ${W} ${H}" width="${W}" height="${H}" role="img" aria-label="Certificado de HyperEZ${d.n ? ' de ' + esc(d.n) : ''}">
<rect width="${W}" height="${H}" fill="#fff"/>
<rect x="23" y="23" width="795.8898" height="549.2756" fill="none" stroke="${NEGRO}" stroke-width="1.65"/>
<rect x="30" y="30" width="781.8898" height="535.2756" fill="none" stroke="${LINEA}" stroke-width=".65"/>
${img.logo ? `<image href="${esc(img.logo)}" xlink:href="${esc(img.logo)}" x="388.4449" y="46" width="65" height="65"/>` : ''}
${t(cx, 132, 'HyperEZ', 24, NEGRO, true)}
${t(cx, 146, 'PROGRAMMING LANGUAGE', 8, GRIS)}
${l(58, 160, 783.8898, LINEA, .75)}
${t(cx, 183, 'A RECOGNITION OF DEDICATION AND ACHIEVEMENT', 8, GRIS, true)}
${t(cx, 218, 'CERTIFICATE OF COMPLETION', 27, NEGRO, true)}
${t(cx, 239, 'WITH HONORS AND RECOGNITION', 11, GRIS_OSC)}
${t(cx, 270, 'This certificate is proudly and sincerely presented to', 10, GRIS)}
${nombre}
${l(155, 305, 686.8898, NEGRO, .9)}
${t(cx, 318, 'RECIPIENT NAME', 7.5, GRIS)}
${t(cx, 345, 'For demonstrating consistency, patience, and determination throughout the learning journey,', 9.5, GRIS_OSC)}
${t(cx, 361, 'and for turning every challenge into progress through the successful completion of HyperEZ.', 9.5, GRIS_OSC)}
${t(cx, 385, 'FINAL ASSESSMENT — AVERAGE SCORE', 9, NEGRO, true)}
${d.p !== undefined ? t(cx, 404, d.p + ' / 100', 15, NEGRO, true) : ''}
${l(358.9449, 410, 482.9449, NEGRO, .85)}
${l(58, 476.2756, 783.8898, LINEA, .75)}
${img.firma ? `<image href="${esc(img.firma)}" xlink:href="${esc(img.firma)}" x="80.6667" y="484.2756" width="116.6667" height="28"/>` : ''}
${l(69, 516.2756, 235, NEGRO, .65)}
${t(69, 530.2756, 'Adrián Cueva Caravaca', 8.8, NEGRO, true, 'start')}
${t(69, 542.2756, 'Founder, HyperEZ', 7.5, GRIS, false, 'start')}
${t(cx, 504.2756, 'CERTIFICATE IDENTIFICATION NUMBER', 7, GRIS, true)}
${d.id ? `<text x="${cx}" y="519.5" font-family="Consolas, 'Courier New', monospace" font-size="10.5" font-weight="700" fill="${NEGRO}" text-anchor="middle" letter-spacing="1">${esc(d.id)}</text>` : ''}
${l(342.9449, 523.2756, 498.9449, LINEA, .75)}
${qr ? qrSvg(qr, 715.8898, 467.2756, 52) : ''}
<rect x="715.8898" y="467.2756" width="52" height="52" fill="none" stroke="${NEGRO}" stroke-width=".8"/>
${t(741.8898, 530.2756, 'QR VALIDATION', 6.5, GRIS, true)}
${t(741.8898, 540.2756, 'SCAN TO VERIFY', 6, GRIS)}
${t(cx, 556.2756, 'HYPEREZ  |  OFFICIAL CERTIFICATION', 5.8, GRIS, false, 'middle', ' xml:space="preserve"')}
</svg>`;
  }

  /** Una imagen como data: (para que el SVG se pueda pasar a PNG sin depender de archivos externos). */
  async function comoDataUrl(ruta) {
    const blob = await (await fetch(ruta)).blob();
    return await new Promise((ok) => { const r = new FileReader(); r.onload = () => ok(r.result); r.readAsDataURL(blob); });
  }

  /** El certificado en PNG (escala 3: unas 2500 × 1800 px). */
  async function png(datos, qr, imagenes, escala) {
    const e = escala || 3;
    const img = { logo: await comoDataUrl(imagenes.logo), firma: await comoDataUrl(imagenes.firma) };
    const url = URL.createObjectURL(new Blob([svg(datos, qr, img)], { type: 'image/svg+xml' }));
    try {
      const im = new Image();
      await new Promise((ok, mal) => { im.onload = ok; im.onerror = mal; im.src = url; });
      const c = document.createElement('canvas');
      c.width = Math.round(W * e); c.height = Math.round(H * e);
      const g = c.getContext('2d');
      g.fillStyle = '#fff'; g.fillRect(0, 0, c.width, c.height);
      g.drawImage(im, 0, 0, c.width, c.height);
      return await new Promise((ok) => c.toBlob(ok, 'image/png'));
    } finally { URL.revokeObjectURL(url); }
  }

  global.HZCertificado = { svg, emitir, verificar, enlace, calcularId, fechaLarga, png, limpiarNombre, b64url, desdeB64url, ANCHO: W, ALTO: H };
})(typeof window !== 'undefined' ? window : globalThis);
