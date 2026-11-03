// mkicon — genera los iconos de HyperEZ a partir de assets/branding/mark.svg.
// El logotipo es una "H" pixelada formada por rectángulos; se rasteriza con supermuestreo.
//
//   mkicon <mark.svg> <carpeta-salida>
//
// Produce: hyperez.ico (aplicación), hz-file.ico (documentos .hz/.hez), setup.ico,
//          hyperez-128.png (extensión de VS Code) y hz-file-32.png.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct Rect { float x0, y0, x1, y1; };

static std::vector<Rect> g_rects;
static float g_tx = 0, g_ty = 0, g_scale = 1;

static bool loadMark(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    size_t t = s.find("translate(");
    if (t != std::string::npos) std::sscanf(s.c_str() + t, "translate(%f %f) scale(%f)", &g_tx, &g_ty, &g_scale);
    size_t d = s.find(" d=\"");
    if (d == std::string::npos) return false;
    d += 4;
    size_t e = s.find('"', d);
    std::string p = s.substr(d, e - d);
    // Subtrazados M x y h w v h h -w z: se toma la caja de los puntos visitados.
    float cx = 0, cy = 0;
    Rect cur{1e9f, 1e9f, -1e9f, -1e9f};
    auto touch = [&] { cur.x0 = std::min(cur.x0, cx); cur.y0 = std::min(cur.y0, cy); cur.x1 = std::max(cur.x1, cx); cur.y1 = std::max(cur.y1, cy); };
    const char* q = p.c_str();
    while (*q) {
        char c = *q++;
        if (c == ' ' || c == ',') continue;
        char* end;
        if (c == 'M') { cx = std::strtof(q, &end); q = end; cy = std::strtof(q, &end); q = end; cur = {1e9f, 1e9f, -1e9f, -1e9f}; touch(); }
        else if (c == 'h') { cx += std::strtof(q, &end); q = end; touch(); }
        else if (c == 'v') { cy += std::strtof(q, &end); q = end; touch(); }
        else if (c == 'H') { cx = std::strtof(q, &end); q = end; touch(); }
        else if (c == 'V') { cy = std::strtof(q, &end); q = end; touch(); }
        else if (c == 'z' || c == 'Z') { if (cur.x1 > cur.x0) g_rects.push_back(cur); }
    }
    return !g_rects.empty();
}

// ¿El punto (u, v) del lienzo 64x64 cae dentro de la H? (centrada en cx, cy con escala extra k)
static bool inMark(float u, float v, float k, float ox, float oy) {
    // la H original ocupa aprox. 44x37 unidades antes de la escala del SVG
    float gx = (u - ox) / (g_scale * k), gy = (v - oy) / (g_scale * k);
    for (auto& r : g_rects) if (gx >= r.x0 && gx < r.x1 && gy >= r.y0 && gy < r.y1) return true;
    return false;
}

static bool inRoundRect(float u, float v, float x0, float y0, float x1, float y1, float r) {
    if (u < x0 || u > x1 || v < y0 || v > y1) return false;
    float cx = std::clamp(u, x0 + r, x1 - r), cy = std::clamp(v, y0 + r, y1 - r);
    float dx = u - cx, dy = v - cy;
    return dx * dx + dy * dy <= r * r;
}

struct RGBA { float r, g, b, a; };

// Composición "encima" en espacio lineal simple.
static RGBA over(RGBA dst, RGBA src) {
    float a = src.a + dst.a * (1 - src.a);
    if (a <= 0) return {0, 0, 0, 0};
    return {(src.r * src.a + dst.r * dst.a * (1 - src.a)) / a, (src.g * src.a + dst.g * dst.a * (1 - src.a)) / a,
            (src.b * src.a + dst.b * dst.a * (1 - src.a)) / a, a};
}

enum Style { App, File, Setup };

static RGBA sample(Style st, float u, float v) {
    RGBA c{0, 0, 0, 0};
    const RGBA white{1, 1, 1, 1}, ink{0.067f, 0.067f, 0.067f, 1}, edge{0.62f, 0.64f, 0.68f, 1}, accent{0.30f, 0.56f, 1.0f, 1};
    // centro de la H en coordenadas del glifo (aprox. 22 x 18.5) para recentrarla
    const float gw = 44, gh = 37;
    if (st == App || st == Setup) {
        if (inRoundRect(u, v, 1, 1, 63, 63, 12)) c = over(c, edge);
        if (inRoundRect(u, v, 2.2f, 2.2f, 61.8f, 61.8f, 11)) c = over(c, white);
        float k = 1.0f;
        float ox = 32 - gw * g_scale * k / 2, oy = 32 - gh * g_scale * k / 2;
        if (inMark(u, v, k, ox, oy)) c = over(c, ink);
        if (st == Setup) {
            // flecha de descarga en la esquina
            if (inRoundRect(u, v, 38, 38, 62, 62, 7)) c = over(c, accent);
            bool arrow = (u > 47.5f && u < 52.5f && v > 42 && v < 52) || (v >= 51 && v < 58 && std::abs(u - 50) < (58 - v) * 1.0f);
            if (arrow) c = over(c, white);
        }
        return c;
    }
    // Documento con esquina doblada
    const float x0 = 9, x1 = 55, y0 = 2, y1 = 62, fold = 15;
    auto inPage = [&](float inset) {
        if (u < x0 + inset || u > x1 - inset || v < y0 + inset || v > y1 - inset) return false;
        return (u - (x1 - fold)) - (v - y0) <= -inset * 0.4f; // corte diagonal de la esquina
    };
    if (inPage(0)) c = over(c, edge);
    if (inPage(1.6f)) c = over(c, white);
    // pliegue
    if (u > x1 - fold && v < y0 + fold && (u - (x1 - fold)) - (v - y0) > 0 && u < x1 && v > y0) {
        bool inFold = (u - (x1 - fold)) - (v - y0) <= fold && u <= x1 && v >= y0;
        if (inFold && v <= y0 + fold && u >= x1 - fold) c = over(c, edge);
    }
    float k = 0.68f;
    float ox = 32 - gw * g_scale * k / 2, oy = 38 - gh * g_scale * k / 2;
    if (inMark(u, v, k, ox, oy)) c = over(c, ink);
    // franja de color bajo la H
    if (u > x0 + 7 && u < x1 - 7 && v > 53 && v < 56.5f) c = over(c, accent);
    return c;
}

static std::vector<uint8_t> render(Style st, int size) {
    std::vector<uint8_t> px(size * size * 4);
    const int ss = size <= 32 ? 8 : 4;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int sy = 0; sy < ss; sy++)
                for (int sx = 0; sx < ss; sx++) {
                    float u = (x + (sx + 0.5f) / ss) / size * 64, v = (y + (sy + 0.5f) / ss) / size * 64;
                    RGBA c = sample(st, u, v);
                    r += c.r * c.a; g += c.g * c.a; b += c.b * c.a; a += c.a;
                }
            float n = (float)(ss * ss);
            uint8_t* p = &px[(y * size + x) * 4];
            float al = a / n;
            p[0] = (uint8_t)std::lround(a > 0 ? r / a * 255 : 0);
            p[1] = (uint8_t)std::lround(a > 0 ? g / a * 255 : 0);
            p[2] = (uint8_t)std::lround(a > 0 ? b / a * 255 : 0);
            p[3] = (uint8_t)std::lround(al * 255);
        }
    return px;
}

static void appendBytes(void* ctx, void* data, int size) {
    auto* v = static_cast<std::vector<uint8_t>*>(ctx);
    v->insert(v->end(), (uint8_t*)data, (uint8_t*)data + size);
}

static std::vector<uint8_t> png(const std::vector<uint8_t>& rgba, int size) {
    std::vector<uint8_t> out;
    stbi_write_png_to_func(appendBytes, &out, size, size, 4, rgba.data(), size * 4);
    return out;
}

static bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return true;
}

static void put16(std::vector<uint8_t>& v, int x) { v.push_back(x & 255); v.push_back((x >> 8) & 255); }
static void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((x >> (8 * i)) & 255); }

// ICO con una imagen por tamaño. Las pequeñas en BMP de 32 bits (compatibilidad), 256 en PNG.
static bool writeIco(const std::string& path, Style st) {
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 96, 128, 256};
    std::vector<std::vector<uint8_t>> blobs;
    for (int s : sizes) {
        auto rgba = render(st, s);
        if (s >= 128) { blobs.push_back(png(rgba, s)); continue; }
        std::vector<uint8_t> b;
        put32(b, 40); put32(b, s); put32(b, s * 2); put16(b, 1); put16(b, 32); put32(b, 0); put32(b, 0);
        put32(b, 0); put32(b, 0); put32(b, 0); put32(b, 0);
        for (int y = s - 1; y >= 0; y--)
            for (int x = 0; x < s; x++) {
                uint8_t* p = &rgba[(y * s + x) * 4];
                b.push_back(p[2]); b.push_back(p[1]); b.push_back(p[0]); b.push_back(p[3]);
            }
        int maskRow = ((s + 31) / 32) * 4;
        b.insert(b.end(), maskRow * s, 0);
        blobs.push_back(b);
    }
    std::vector<uint8_t> ico;
    int n = (int)blobs.size();
    put16(ico, 0); put16(ico, 1); put16(ico, n);
    uint32_t offset = 6 + 16 * n;
    for (int i = 0; i < n; i++) {
        int s = sizes[i];
        ico.push_back(s >= 256 ? 0 : s);
        ico.push_back(s >= 256 ? 0 : s);
        ico.push_back(0); ico.push_back(0);
        put16(ico, 1); put16(ico, 32);
        put32(ico, (uint32_t)blobs[i].size());
        put32(ico, offset);
        offset += (uint32_t)blobs[i].size();
    }
    for (auto& b : blobs) ico.insert(ico.end(), b.begin(), b.end());
    return writeFile(path, ico);
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "Uso: mkicon <mark.svg> <carpeta-salida>\n"); return 2; }
    if (!loadMark(argv[1])) { std::fprintf(stderr, "No se pudo leer el logotipo de %s\n", argv[1]); return 1; }
    std::string out = argv[2];
    bool ok = writeIco(out + "/hyperez.ico", App) && writeIco(out + "/hz-file.ico", File) && writeIco(out + "/setup.ico", Setup);
    ok = ok && writeFile(out + "/hyperez-128.png", png(render(App, 128), 128));
    ok = ok && writeFile(out + "/hyperez-256.png", png(render(App, 256), 256));
    ok = ok && writeFile(out + "/hz-file-32.png", png(render(File, 32), 32));
    ok = ok && writeFile(out + "/hz-file-256.png", png(render(File, 256), 256));
    std::printf(ok ? "Iconos generados en %s (%zu rectángulos)\n" : "Error al escribir en %s\n", out.c_str(), g_rects.size());
    return ok ? 0 : 1;
}
