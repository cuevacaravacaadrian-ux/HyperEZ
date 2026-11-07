# Componentes de terceros

HyperEZ incluye estos componentes de otros autores. Sus licencias completas están en las carpetas indicadas.

| Componente | Para qué se usa | Licencia | Archivo |
|---|---|---|---|
| [stb_image, stb_image_write, stb_truetype](https://github.com/nothings/stb) (Sean Barrett) | leer y guardar imágenes, texto en `Image`, compresión zlib | MIT o dominio público | `third_party/stb/LICENSE` |
| [nlohmann/json](https://github.com/nlohmann/json) (Niels Lohmann) | protocolo del servidor de lenguaje | MIT | `third_party/nlohmann/LICENSE.MIT` |
| [qrcode-generator](https://github.com/kazuhikoarase/qrcode-generator) (Kazuhiko Arase) | el QR de verificación de los certificados de la Academia (web) | MIT | `projects/web/src/aprender/qrcode.js` (cabecera) |
| [DejaVu Sans](https://dejavu-fonts.github.io/) | fuente por defecto de `Image.text` (incrustada en `hyper`) | Bitstream Vera / dominio público | `assets/fonts/LICENSE-DejaVu.txt` |

En el repositorio también están `third_party/cgltf`, `third_party/dr_libs` y `stb_vorbis`, que HyperEZ no compila ni distribuye por ahora.
