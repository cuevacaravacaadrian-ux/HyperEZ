/*
 * HyperEZ para highlight.js (https://highlightjs.org): colorea código HyperEZ en páginas web.
 *   <script src="highlight.min.js"></script>
 *   <script src="hyperez.js"></script>
 *   <script>hljs.highlightAll()</script>      ...  <pre><code class="language-hz">print("hola")</code></pre>
 * Generado por tools/gen_editors.py
 */
(function () {
  function hyperez(hljs) {
    const INTERP = { className: 'subst', begin: /\$\{/, end: /\}/, keywords: {} };
    const STRING = {
      className: 'string',
      variants: [
        { begin: /r"/, end: /"/ }, { begin: /r'/, end: /'/ },
        { begin: /"""/, end: /"""/, contains: [INTERP] },
        { begin: /"/, end: /"/, contains: [hljs.BACKSLASH_ESCAPE, INTERP] },
        { begin: /'/, end: /'/, contains: [hljs.BACKSLASH_ESCAPE, INTERP] }
      ]
    };
    const KEYWORDS = {
      keyword: "var let const function return if else for foreach in while repeat break continue class struct constructor this super extends implements interface enum import from as public private protected static new try catch finally throw async await match event on once emit step get set and or not is",
      literal: "true false null",
      type: "number int text bool list map set any void function Task Range Event",
      built_in: "print println write input len typeof str toNumber toInt assert assertEqual assertThrows test exit range time wait spawn abs sqrt pow floor ceil round min max clamp random randomInt randomChoice randomSeed Console File System JSON Path Time Process Http Regex Bytes Hash Matrix Image Vec2 Vec3 Color Error"
    };
    INTERP.keywords = KEYWORDS;
    INTERP.contains = [STRING, hljs.C_NUMBER_MODE];
    return {
      name: 'HyperEZ',
      aliases: ['hz', 'hez', 'hyperez'],
      keywords: KEYWORDS,
      contains: [
        hljs.COMMENT('///', '$', { className: 'doctag' }),
        hljs.C_LINE_COMMENT_MODE,
        hljs.C_BLOCK_COMMENT_MODE,
        STRING,
        { className: 'number', begin: /\b(0[xX][0-9a-fA-F_]+|0[bB][01_]+|\d[\d_]*(\.\d[\d_]*)?([eE][+-]?\d+)?)\b/ },
        { className: 'title.class', begin: /\b[A-Z][A-Za-z0-9_]*\b/ },
        { className: 'title.function', begin: /\b[a-z_][A-Za-z0-9_]*(?=\s*\()/ },
        { className: 'operator', begin: /\|>|=>|->|\.\.=?|\?\?|\?\./ }
      ]
    };
  }
  if (typeof module !== 'undefined' && module.exports) module.exports = hyperez;
  if (typeof hljs !== 'undefined') hljs.registerLanguage('hyperez', hyperez);
})();
