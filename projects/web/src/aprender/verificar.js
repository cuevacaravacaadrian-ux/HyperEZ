// verificar.html: lee el certificado del enlace (?c=...) o del formulario, recalcula su código y lo muestra.
(function () {
  'use strict';
  const C = window.HZCertificado;
  const $ = (id) => document.getElementById(id);
  const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const IMAGENES = { logo: 'aprender/cert-logo.png', firma: 'aprender/cert-firma.png' };

  async function comprobar(texto) {
    const v = await C.verificar(texto);
    const caja = $('veredicto'), cert = $('verificar-cert');
    caja.hidden = false;
    caja.classList.remove('entra'); void caja.offsetWidth; caja.classList.add('entra');
    if (!v.ok) {
      caja.className = 'veredicto mal entra';
      caja.innerHTML = '<div class="veredicto-icono">✗</div><div><h2>No es un certificado válido</h2><p>El enlace está incompleto o dañado. Escanea de nuevo el QR o copia el enlace entero.</p></div>';
      cert.hidden = true;
      return;
    }
    const d = v.datos;
    caja.className = 'veredicto bien entra';
    caja.innerHTML = `<div class="veredicto-icono">✓</div><div><h2>Certificado auténtico</h2>
      <dl><dt>Titular</dt><dd>${esc(d.n)}</dd><dt>Curso</dt><dd>Academia HyperEZ · ${d.m} misiones y examen final</dd>
      <dt>Nota media</dt><dd>${d.p} / 100</dd><dt>Emitido</dt><dd>${C.fechaLarga(d.f)}</dd><dt>Código</dt><dd><code>${esc(d.id)}</code></dd></dl>
      <p class="pequeno">El código coincide con los datos: compruébalo con el impreso en el certificado.</p></div>`;
    cert.hidden = false;
    cert.innerHTML = C.svg(d, C.enlace(d, location.href), IMAGENES);
    document.title = 'Certificado de ' + d.n + ' · HyperEZ';
  }

  $('verificar-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const t = $('verificar-entrada').value.trim();
    if (t) comprobar(t);
  });
  const c = new URLSearchParams(location.search).get('c');
  if (c) { $('verificar-entrada').value = location.href; comprobar(location.href); }
})();
