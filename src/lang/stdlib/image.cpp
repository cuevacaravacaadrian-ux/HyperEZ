// Image: imágenes en memoria (RGBA de 8 bits) para generar gráficos, gráficas, mapas, texturas,
// miniaturas o banners. Dibuja con transparencia, escribe texto con fuentes TrueType y lee y guarda
// PNG, JPG, BMP y TGA. Usa las bibliotecas stb (third_party/stb).
#include "lang/stdlib/native.h"
#include "lang/stdlib/mathtypes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBIW_WINDOWS_UTF8
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace hz {

extern const unsigned char kDefaultFont[];
extern const unsigned long kDefaultFont_size;

struct Img {
    int w = 0, h = 0;
    std::vector<uint8_t> px; // RGBA
    Img() = default;
    Img(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_ * 4, 0) {}
    uint8_t* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
};

static void freeImg(void* p) { delete static_cast<Img*>(p); }
static ObjClass* imageClass(VM& vm) { return static_cast<ObjClass*>(vm.builtins[vm.builtinSlot("Image")].o); }
static Img& I(Value* args) { return *static_cast<Img*>(static_cast<ObjForeign*>(args[-1].o)->ptr); }
static Value wrap(VM& vm, Img img) { return Value::object(vm.newForeign(imageClass(vm), new Img(std::move(img)), freeImg)); }
static Img& asImg(VM& vm, const Value& v, const char* fname) {
    if (!isObjType(v, OT::Foreign) || static_cast<ObjForeign*>(v.o)->cls != imageClass(vm))
        vm.raise(std::string(fname) + ": se esperaba una Image, no " + vm.typeName(v));
    return *static_cast<Img*>(static_cast<ObjForeign*>(v.o)->ptr);
}

struct C8 { uint8_t r, g, b, a; };
static C8 colorArg(VM& vm, Value* args, int argc, int i, const char* fname, C8 def = {255, 255, 255, 255}) {
    Value v = optArg(args, argc, i);
    if (v.isNull()) return def;
    RGBA c = toColor(vm, v, fname);
    auto q = [](float f) { return (uint8_t)std::clamp((int)std::lround(f * 255.0f), 0, 255); };
    return {q(c.r), q(c.g), q(c.b), q(c.a)};
}

// Mezcla un color sobre un píxel con una cobertura extra (0..255) para el texto y los bordes suaves.
static inline void blend(Img& im, int x, int y, C8 c, int cover = 255) {
    if (x < 0 || y < 0 || x >= im.w || y >= im.h) return;
    uint8_t* d = im.at(x, y);
    int a = c.a * cover / 255;
    if (a >= 255) { d[0] = c.r; d[1] = c.g; d[2] = c.b; d[3] = 255; return; }
    if (a <= 0) return;
    int da = d[3];
    int outA = a + da * (255 - a) / 255;
    if (outA == 0) return;
    for (int k = 0; k < 3; k++) {
        int src = k == 0 ? c.r : k == 1 ? c.g : c.b;
        d[k] = (uint8_t)((src * a + d[k] * da * (255 - a) / 255) / outA);
    }
    d[3] = (uint8_t)outA;
}

static int dimArg(VM& vm, Value* args, int argc, int i, const char* fname) {
    double d = argNum(vm, args, argc, i, fname);
    if (d < 1 || d > 16384 || std::floor(d) != d) vm.raise(std::string(fname) + ": " + formatNumber(d) + " no es un tamaño válido (1 a 16384)");
    return (int)d;
}

NATIVE(c_image) {
    int w = dimArg(vm, args, argc, 0, "Image"), h = dimArg(vm, args, argc, 1, "Image");
    Img im(w, h);
    C8 c = colorArg(vm, args, argc, 2, "Image", {0, 0, 0, 0});
    for (int i = 0; i < w * h; i++) std::memcpy(&im.px[(size_t)i * 4], &c, 4);
    return Value::object(vm.newForeign(static_cast<ObjClass*>(args[-1].o), new Img(std::move(im)), freeImg));
}
NATIVE(im_load) {
    std::string p = argStr(vm, args, argc, 0, "Image.load");
    int w, h, n;
    unsigned char* data = stbi_load(resolvePath(vm, p).c_str(), &w, &h, &n, 4);
    if (!data) vm.raise("Image.load: no se puede leer la imagen '" + p + "' (" + (stbi_failure_reason() ? stbi_failure_reason() : "?") + ")");
    Img im(w, h);
    std::memcpy(im.px.data(), data, im.px.size());
    stbi_image_free(data);
    return wrap(vm, std::move(im));
}
NATIVE(im_fromBytes) {
    int w = dimArg(vm, args, argc, 0, "Image.fromBytes"), h = dimArg(vm, args, argc, 1, "Image.fromBytes");
    auto* data = bytesData(vm, optArg(args, argc, 2));
    if (!data) vm.raise("Image.fromBytes: se esperaban Bytes con los píxeles RGBA");
    if (data->size() != (size_t)w * h * 4) vm.raise("Image.fromBytes: hacen falta " + std::to_string((size_t)w * h * 4) + " bytes (ancho × alto × 4)");
    Img im(w, h);
    im.px = *data;
    return wrap(vm, std::move(im));
}
NATIVE(im_width) { UNUSED; return Value(I(args).w); }
NATIVE(im_height) { UNUSED; return Value(I(args).h); }
NATIVE(im_get) {
    Img& im = I(args);
    int x = (int)argNum(vm, args, argc, 0, "Image.get"), y = (int)argNum(vm, args, argc, 1, "Image.get");
    if (x < 0 || y < 0 || x >= im.w || y >= im.h) vm.raise("Image.get: (" + std::to_string(x) + ", " + std::to_string(y) + ") está fuera de la imagen");
    uint8_t* p = im.at(x, y);
    return makeColor(vm, {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, p[3] / 255.0f});
}
NATIVE(im_set) {
    Img& im = I(args);
    int x = (int)argNum(vm, args, argc, 0, "Image.set"), y = (int)argNum(vm, args, argc, 1, "Image.set");
    C8 c = colorArg(vm, args, argc, 2, "Image.set");
    if (x >= 0 && y >= 0 && x < im.w && y < im.h) std::memcpy(im.at(x, y), &c, 4);
    return Value();
}
NATIVE(im_fill) {
    Img& im = I(args);
    C8 c = colorArg(vm, args, argc, 0, "Image.fill");
    for (int i = 0; i < im.w * im.h; i++) std::memcpy(&im.px[(size_t)i * 4], &c, 4);
    return args[-1];
}
NATIVE(im_rect) {
    Img& im = I(args);
    int x = (int)argNum(vm, args, argc, 0, "Image.rect"), y = (int)argNum(vm, args, argc, 1, "Image.rect");
    int w = (int)argNum(vm, args, argc, 2, "Image.rect"), h = (int)argNum(vm, args, argc, 3, "Image.rect");
    C8 c = colorArg(vm, args, argc, 4, "Image.rect");
    bool filled = argBool(args, argc, 5, true);
    for (int j = std::max(0, y); j < std::min(im.h, y + h); j++)
        for (int i = std::max(0, x); i < std::min(im.w, x + w); i++)
            if (filled || i == x || j == y || i == x + w - 1 || j == y + h - 1) blend(im, i, j, c);
    return args[-1];
}
NATIVE(im_line) {
    Img& im = I(args);
    double x0 = argNum(vm, args, argc, 0, "Image.line"), y0 = argNum(vm, args, argc, 1, "Image.line");
    double x1 = argNum(vm, args, argc, 2, "Image.line"), y1 = argNum(vm, args, argc, 3, "Image.line");
    C8 c = colorArg(vm, args, argc, 4, "Image.line");
    double width = std::max(1.0, argNum(vm, args, argc, 5, "Image.line", 1));
    double r = width / 2;
    // Distancia de cada píxel al segmento, con borde suavizado
    int minx = (int)std::floor(std::min(x0, x1) - r - 1), maxx = (int)std::ceil(std::max(x0, x1) + r + 1);
    int miny = (int)std::floor(std::min(y0, y1) - r - 1), maxy = (int)std::ceil(std::max(y0, y1) + r + 1);
    double dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int y = std::max(0, miny); y <= std::min(im.h - 1, maxy); y++)
        for (int x = std::max(0, minx); x <= std::min(im.w - 1, maxx); x++) {
            double px = x + 0.5, py = y + 0.5;
            double t = len2 == 0 ? 0 : std::clamp(((px - x0) * dx + (py - y0) * dy) / len2, 0.0, 1.0);
            double ex = x0 + t * dx - px, ey = y0 + t * dy - py;
            double d = std::sqrt(ex * ex + ey * ey);
            double cover = std::clamp(r + 0.5 - d, 0.0, 1.0);
            if (cover > 0) blend(im, x, y, c, (int)(cover * 255));
        }
    return args[-1];
}
NATIVE(im_circle) {
    Img& im = I(args);
    double cx = argNum(vm, args, argc, 0, "Image.circle"), cy = argNum(vm, args, argc, 1, "Image.circle");
    double rad = argNum(vm, args, argc, 2, "Image.circle");
    C8 c = colorArg(vm, args, argc, 3, "Image.circle");
    bool filled = argBool(args, argc, 4, true);
    for (int y = std::max(0, (int)(cy - rad - 2)); y <= std::min(im.h - 1, (int)(cy + rad + 2)); y++)
        for (int x = std::max(0, (int)(cx - rad - 2)); x <= std::min(im.w - 1, (int)(cx + rad + 2)); x++) {
            double d = std::hypot(x + 0.5 - cx, y + 0.5 - cy);
            double cover = filled ? std::clamp(rad + 0.5 - d, 0.0, 1.0) : std::clamp(1.0 - std::fabs(d - rad), 0.0, 1.0);
            if (cover > 0) blend(im, x, y, c, (int)(cover * 255));
        }
    return args[-1];
}

// ---- texto ----------------------------------------------------------------

struct Font {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
};
static Font* loadFont(VM& vm, const std::string& path) {
    static std::map<std::string, std::unique_ptr<Font>> cache;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second.get();
    auto f = std::make_unique<Font>();
    if (path.empty()) {
        f->data.assign(kDefaultFont, kDefaultFont + kDefaultFont_size);
    } else {
        std::string s;
        if (!readFile(resolvePath(vm, path), s)) vm.raise("Image.text: no se puede leer la fuente '" + path + "'");
        f->data.assign(s.begin(), s.end());
    }
    if (!stbtt_InitFont(&f->info, f->data.data(), stbtt_GetFontOffsetForIndex(f->data.data(), 0)))
        vm.raise("Image.text: '" + path + "' no es una fuente TrueType válida");
    Font* raw = f.get();
    cache[path] = std::move(f);
    return raw;
}
// Dibuja (si im no es nulo) y devuelve el ancho en píxeles.
static double drawText(Img* im, Font* font, const std::string& text, double x, double y, double size, C8 c) {
    float scale = stbtt_ScaleForPixelHeight(&font->info, (float)size);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &gap);
    double baseline = y + ascent * scale;
    double pen = x, maxPen = x;
    size_t i = 0;
    uint32_t prev = 0;
    while (i < text.size()) {
        uint32_t cp = utf8Decode(text, i);
        if (cp == '\n') {
            maxPen = std::max(maxPen, pen);
            pen = x;
            baseline += (ascent - descent + gap) * scale;
            prev = 0;
            continue;
        }
        if (prev) pen += stbtt_GetCodepointKernAdvance(&font->info, (int)prev, (int)cp) * scale;
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font->info, (int)cp, &adv, &lsb);
        if (im) {
            int x0, y0, x1, y1;
            float sx = (float)(pen - std::floor(pen));
            stbtt_GetCodepointBitmapBoxSubpixel(&font->info, (int)cp, scale, scale, sx, 0, &x0, &y0, &x1, &y1);
            int gw = x1 - x0, gh = y1 - y0;
            if (gw > 0 && gh > 0) {
                std::vector<unsigned char> bmp((size_t)gw * gh);
                stbtt_MakeCodepointBitmapSubpixel(&font->info, bmp.data(), gw, gh, gw, scale, scale, sx, 0, (int)cp);
                int ox = (int)std::floor(pen) + x0, oy = (int)std::lround(baseline) + y0;
                for (int yy = 0; yy < gh; yy++)
                    for (int xx = 0; xx < gw; xx++)
                        if (int cov = bmp[(size_t)yy * gw + xx]) blend(*im, ox + xx, oy + yy, c, cov);
            }
        }
        pen += adv * scale;
        prev = cp;
    }
    return std::max(maxPen, pen) - x;
}
NATIVE(im_text) {
    Img& im = I(args);
    double x = argNum(vm, args, argc, 0, "Image.text"), y = argNum(vm, args, argc, 1, "Image.text");
    std::string t = vm.toString(optArg(args, argc, 2));
    C8 c = colorArg(vm, args, argc, 3, "Image.text");
    double size = argNum(vm, args, argc, 4, "Image.text", 16);
    Font* f = loadFont(vm, argStr(vm, args, argc, 5, "Image.text", ""));
    return Value(drawText(&im, f, t, x, y, size, c));
}
NATIVE(im_textWidth) {
    std::string t = vm.toString(optArg(args, argc, 0));
    double size = argNum(vm, args, argc, 1, "Image.textWidth", 16);
    Font* f = loadFont(vm, argStr(vm, args, argc, 2, "Image.textWidth", ""));
    return Value(drawText(nullptr, f, t, 0, 0, size, {}));
}

// ---- transformaciones -----------------------------------------------------

NATIVE(im_paste) {
    Img& dst = I(args);
    Img& src = asImg(vm, optArg(args, argc, 0), "Image.paste");
    int ox = (int)argNum(vm, args, argc, 1, "Image.paste", 0), oy = (int)argNum(vm, args, argc, 2, "Image.paste", 0);
    for (int y = 0; y < src.h; y++)
        for (int x = 0; x < src.w; x++) {
            uint8_t* p = src.at(x, y);
            blend(dst, ox + x, oy + y, {p[0], p[1], p[2], p[3]});
        }
    return args[-1];
}
NATIVE(im_crop) {
    Img& im = I(args);
    int x = (int)argNum(vm, args, argc, 0, "Image.crop"), y = (int)argNum(vm, args, argc, 1, "Image.crop");
    int w = dimArg(vm, args, argc, 2, "Image.crop"), h = dimArg(vm, args, argc, 3, "Image.crop");
    Img out(w, h);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (x + i >= 0 && y + j >= 0 && x + i < im.w && y + j < im.h) std::memcpy(out.at(i, j), im.at(x + i, y + j), 4);
    return wrap(vm, std::move(out));
}
NATIVE(im_resize) {
    Img& im = I(args);
    int w = dimArg(vm, args, argc, 0, "Image.resize"), h = dimArg(vm, args, argc, 1, "Image.resize");
    bool smooth = argBool(args, argc, 2, true);
    Img out(w, h);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            double sx = (i + 0.5) * im.w / w - 0.5, sy = (j + 0.5) * im.h / h - 0.5;
            if (!smooth) {
                int px = std::clamp((int)std::lround(sx), 0, im.w - 1), py = std::clamp((int)std::lround(sy), 0, im.h - 1);
                std::memcpy(out.at(i, j), im.at(px, py), 4);
                continue;
            }
            int x0 = std::clamp((int)std::floor(sx), 0, im.w - 1), y0 = std::clamp((int)std::floor(sy), 0, im.h - 1);
            int x1 = std::min(x0 + 1, im.w - 1), y1 = std::min(y0 + 1, im.h - 1);
            double fx = std::clamp(sx - x0, 0.0, 1.0), fy = std::clamp(sy - y0, 0.0, 1.0);
            for (int k = 0; k < 4; k++) {
                double v = im.at(x0, y0)[k] * (1 - fx) * (1 - fy) + im.at(x1, y0)[k] * fx * (1 - fy) + im.at(x0, y1)[k] * (1 - fx) * fy +
                           im.at(x1, y1)[k] * fx * fy;
                out.at(i, j)[k] = (uint8_t)std::lround(v);
            }
        }
    return wrap(vm, std::move(out));
}
NATIVE(im_flipH) {
    UNUSED;
    Img& im = I(args);
    for (int y = 0; y < im.h; y++)
        for (int x = 0; x < im.w / 2; x++) std::swap_ranges(im.at(x, y), im.at(x, y) + 4, im.at(im.w - 1 - x, y));
    return args[-1];
}
NATIVE(im_flipV) {
    UNUSED;
    Img& im = I(args);
    for (int y = 0; y < im.h / 2; y++) std::swap_ranges(im.at(0, y), im.at(0, y) + (size_t)im.w * 4, im.at(0, im.h - 1 - y));
    return args[-1];
}
NATIVE(im_grayscale) {
    UNUSED;
    Img& im = I(args);
    for (size_t i = 0; i < im.px.size(); i += 4) {
        uint8_t g = (uint8_t)std::lround(0.299 * im.px[i] + 0.587 * im.px[i + 1] + 0.114 * im.px[i + 2]);
        im.px[i] = im.px[i + 1] = im.px[i + 2] = g;
    }
    return args[-1];
}
NATIVE(im_invert) {
    UNUSED;
    Img& im = I(args);
    for (size_t i = 0; i < im.px.size(); i += 4)
        for (int k = 0; k < 3; k++) im.px[i + k] = (uint8_t)(255 - im.px[i + k]);
    return args[-1];
}
NATIVE(im_copy) { UNUSED; return wrap(vm, I(args)); }
NATIVE(im_pixels) {
    UNUSED;
    return newBytes(vm, I(args).px);
}
NATIVE(im_save) {
    Img& im = I(args);
    std::string p = argStr(vm, args, argc, 0, "Image.save");
    int quality = (int)argNum(vm, args, argc, 1, "Image.save", 90);
    std::string full = resolvePath(vm, p);
    if (!pathDir(full).empty()) makeDirs(pathDir(full));
    std::string ext = toLower(pathExt(p));
    int ok = 0;
    if (ext == ".png") ok = stbi_write_png(full.c_str(), im.w, im.h, 4, im.px.data(), im.w * 4);
    else if (ext == ".jpg" || ext == ".jpeg") ok = stbi_write_jpg(full.c_str(), im.w, im.h, 4, im.px.data(), std::clamp(quality, 1, 100));
    else if (ext == ".bmp") ok = stbi_write_bmp(full.c_str(), im.w, im.h, 4, im.px.data());
    else if (ext == ".tga") ok = stbi_write_tga(full.c_str(), im.w, im.h, 4, im.px.data());
    else vm.raise("Image.save: formato desconocido '" + ext + "' (usa .png, .jpg, .bmp o .tga)");
    if (!ok) vm.raise("Image.save: no se puede escribir '" + p + "'");
    return Value();
}
NATIVE(im_toString) { UNUSED; return vm.str("Image(" + std::to_string(I(args).w) + "×" + std::to_string(I(args).h) + ")"); }

// ---- compresión (zlib) para Bytes --------------------------------------

static std::vector<uint8_t> bytesOf(VM& vm, Value v, const char* fname) {
    if (auto* d = bytesData(vm, v)) return *d;
    vm.raise(std::string(fname) + ": se esperaban Bytes");
}
static Value toBytes(VM& vm, const unsigned char* data, size_t n) { return newBytes(vm, std::vector<uint8_t>(data, data + n)); }
NATIVE(b_compress) {
    auto in = bytesOf(vm, args[-1], "Bytes.compress");
    int level = std::clamp((int)argNum(vm, args, argc, 0, "Bytes.compress", 8), 1, 9);
    int outLen = 0;
    unsigned char* z = stbi_zlib_compress(in.data(), (int)in.size(), &outLen, level);
    if (!z) vm.raise("Bytes.compress: no se pudo comprimir");
    Value r = toBytes(vm, z, (size_t)outLen);
    STBIW_FREE(z);
    return r;
}
NATIVE(b_decompress) {
    auto in = bytesOf(vm, args[-1], "Bytes.decompress");
    int outLen = 0;
    char* d = stbi_zlib_decode_malloc((const char*)in.data(), (int)in.size(), &outLen);
    if (!d) vm.raise("Bytes.decompress: los datos no están comprimidos con zlib (Bytes.compress)");
    Value r = toBytes(vm, (const unsigned char*)d, (size_t)outLen);
    STBI_FREE(d);
    return r;
}

void registerImage(VM& vm) {
    ObjClass* c = vm.nativeClass("Image",
        "Imagen en memoria (píxeles RGBA) para generar gráficos, mapas, texturas o banners: se dibuja con transparencia, "
        "se escribe texto y se guarda como PNG, JPG, BMP o TGA.",
        c_image, "(ancho: number, alto: number, fondo?: Color)");
    vm.defineClassProp(c, "width", "number", "Ancho en píxeles.", im_width);
    vm.defineClassProp(c, "height", "number", "Alto en píxeles.", im_height);
    vm.defineMethod(c, "get", "(x: number, y: number): Color", "Color de un píxel.", im_get);
    vm.defineMethod(c, "set", "(x: number, y: number, color: Color): void", "Cambia un píxel (sin mezclar).", im_set);
    vm.defineMethod(c, "fill", "(color: Color): Image", "Pinta toda la imagen de un color.", im_fill);
    vm.defineMethod(c, "rect", "(x: number, y: number, ancho: number, alto: number, color?: Color, relleno?: bool = true): Image",
                    "Rectángulo relleno o solo el borde.", im_rect);
    vm.defineMethod(c, "line", "(x0: number, y0: number, x1: number, y1: number, color?: Color, grosor?: number = 1): Image",
                    "Línea con bordes suaves.", im_line);
    vm.defineMethod(c, "circle", "(x: number, y: number, radio: number, color?: Color, relleno?: bool = true): Image",
                    "Círculo relleno o solo la circunferencia.", im_circle);
    vm.defineMethod(c, "text", "(x: number, y: number, texto: any, color?: Color, tamano?: number = 16, fuente?: text): number",
                    "Escribe texto (fuente DejaVu Sans incluida, o un archivo .ttf) y devuelve su ancho en píxeles.", im_text);
    vm.defineStatic(c, "textWidth", "(texto: any, tamano?: number = 16, fuente?: text): number", "Ancho que ocupará un texto.", im_textWidth);
    vm.defineMethod(c, "paste", "(otra: Image, x?: number = 0, y?: number = 0): Image", "Dibuja otra imagen encima (con su transparencia).", im_paste);
    vm.defineMethod(c, "crop", "(x: number, y: number, ancho: number, alto: number): Image", "Recorte (imagen nueva).", im_crop);
    vm.defineMethod(c, "resize", "(ancho: number, alto: number, suave?: bool = true): Image", "Imagen nueva con otro tamaño.", im_resize);
    vm.defineMethod(c, "flipH", "(): Image", "Voltea horizontalmente.", im_flipH);
    vm.defineMethod(c, "flipV", "(): Image", "Voltea verticalmente.", im_flipV);
    vm.defineMethod(c, "grayscale", "(): Image", "Pasa a escala de grises.", im_grayscale);
    vm.defineMethod(c, "invert", "(): Image", "Invierte los colores.", im_invert);
    vm.defineMethod(c, "copy", "(): Image", "Copia independiente.", im_copy);
    vm.defineMethod(c, "pixels", "(): Bytes", "Los píxeles como Bytes (R, G, B, A por píxel, fila a fila).", im_pixels);
    vm.defineMethod(c, "save", "(ruta: text, calidad?: number = 90): void", "Guarda según la extensión: .png, .jpg, .bmp o .tga.", im_save);
    vm.defineMethod(c, "toString", "(): text", "Descripción.", im_toString);
    vm.defineStatic(c, "load", "(ruta: text): Image", "Lee una imagen PNG, JPG, BMP, GIF o TGA.", im_load);
    vm.defineStatic(c, "fromBytes", "(ancho: number, alto: number, pixeles: Bytes): Image", "Imagen a partir de píxeles RGBA.", im_fromBytes);

    ObjClass* bytes = static_cast<ObjClass*>(vm.builtins[vm.builtinSlot("Bytes")].o);
    vm.defineMethod(bytes, "compress", "(nivel?: number = 8): Bytes", "Comprime con zlib (deflate); nivel de 1 (rápido) a 9 (pequeño).", b_compress);
    vm.defineMethod(bytes, "decompress", "(): Bytes", "Descomprime datos zlib.", b_decompress);
}

} // namespace hz
