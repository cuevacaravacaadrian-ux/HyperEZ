// Comprueba la solución de un reto de la Academia HyperEZ. Lo usan la página (con HyperEZ en un Worker) y las
// pruebas (con HyperEZ en Node). ejecutar(código) → Promise<{ salida, codigo, pasadas, falladas }>.
(function (global) {
  'use strict';

  const normalizar = (t) => String(t || '').replace(/\r\n?/g, '\n').split('\n').map((l) => l.replace(/\s+$/, '')).join('\n').replace(/\n+$/, '');
  // el código sin comentarios (para las reglas "usa": un comentario no cuenta)
  const sinComentarios = (c) => c.replace(/\/\/[^\n]*/g, '');

  /** Primera línea en la que dos salidas se diferencian (para explicar el fallo). */
  function diferencia(obtenida, esperada) {
    const a = obtenida.split('\n'), b = esperada.split('\n');
    for (let i = 0; i < Math.max(a.length, b.length); i++)
      if (a[i] !== b[i]) return { linea: i + 1, obtenida: a[i] === undefined ? '(nada)' : a[i], esperada: b[i] === undefined ? '(nada)' : b[i] };
    return null;
  }

  async function comprobar(reto, codigo, ejecutar) {
    const c = reto.comprobar || {};
    for (const [re, msg] of c.usa || []) {
      if (!re.test(sinComentarios(codigo))) return { ok: false, motivo: 'usa', mensaje: msg };
    }
    if (c.variantes) {
      for (let i = 0; i < c.variantes.length; i++) {
        const v = c.variantes[i];
        if (!codigo.includes(v.cambia[0]))
          return { ok: false, motivo: 'linea', mensaje: 'No cambies la línea <code>' + v.cambia[0].replace(/</g, '&lt;') + '</code>: la usamos para probar tu programa con otros valores.' };
        const r = await ejecutar(codigo.replace(v.cambia[0], v.cambia[1]));
        if (r.codigo !== 0) return { ok: false, motivo: 'error', mensaje: 'Tu programa termina con un error.', r };
        const ob = normalizar(r.salida), es = normalizar(v.salida);
        if (ob !== es) {
          const otra = v.cambia[0] !== v.cambia[1];
          return { ok: false, motivo: 'salida', r, dif: diferencia(ob, es), mensaje: otra
            ? 'Funciona con el valor del enunciado, pero no si cambiamos <code>' + v.cambia[0] + '</code> por <code>' + v.cambia[1] + '</code>: calcula el resultado con las variables en vez de escribirlo.'
            : 'La salida no es la que se pide.' };
        }
      }
      return { ok: true };
    }
    const programa = c.pruebas ? codigo + '\n\n// ---- pruebas de la Academia ----\n' + c.pruebas + '\n' : codigo;
    const r = await ejecutar(programa);
    if (c.pruebas) {
      if (r.falladas > 0 || r.pasadas === 0 || r.codigo !== 0)
        return { ok: false, motivo: 'pruebas', r, mensaje: r.pasadas + r.falladas > 0
          ? 'Fallan ' + r.falladas + ' de ' + (r.pasadas + r.falladas) + ' pruebas: mira cuáles en la salida.' : 'Tu programa tiene un error: corrígelo y vuelve a probar.' };
      return { ok: true, r };
    }
    if (r.codigo !== 0) return { ok: false, motivo: 'error', mensaje: 'Tu programa termina con un error.', r };
    if (c.salida !== undefined) {
      const ob = normalizar(r.salida), es = normalizar(c.salida);
      if (ob !== es) return { ok: false, motivo: 'salida', r, dif: diferencia(ob, es), mensaje: 'La salida no es la que se pide.' };
    }
    return { ok: true, r };
  }

  global.HZReto = { comprobar, normalizar };
})(typeof window !== 'undefined' ? window : globalThis);
