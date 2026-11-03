// pack — empaqueta una carpeta en un único archivo para incrustarlo en el instalador.
//
//   pack <salida.bin> <carpeta>
//
// Formato (little endian):
//   "HZPACK01"  u32 n  { u32 lenRuta, ruta UTF-8 con '/', u64 tamaño, datos }*n
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

static void put32(std::ofstream& o, uint32_t v) { o.write((const char*)&v, 4); }
static void put64(std::ofstream& o, uint64_t v) { o.write((const char*)&v, 8); }

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "Uso: pack <salida.bin> <carpeta>\n"); return 2; }
    fs::path root((const char8_t*)argv[2]);
    std::vector<fs::path> files;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->is_regular_file()) files.push_back(it->path());
    if (ec) { std::fprintf(stderr, "No se puede leer la carpeta %s\n", argv[2]); return 1; }
    std::sort(files.begin(), files.end());
    std::ofstream out(fs::path((const char8_t*)argv[1]), std::ios::binary);
    if (!out) { std::fprintf(stderr, "No se puede escribir %s\n", argv[1]); return 1; }
    out.write("HZPACK01", 8);
    put32(out, (uint32_t)files.size());
    uint64_t total = 0;
    for (auto& f : files) {
        auto rel = fs::relative(f, root).generic_u8string();
        std::string r((const char*)rel.data(), rel.size());
        std::ifstream in(f, std::ios::binary);
        std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        put32(out, (uint32_t)r.size());
        out.write(r.data(), (std::streamsize)r.size());
        put64(out, data.size());
        out.write(data.data(), (std::streamsize)data.size());
        total += data.size();
    }
    std::printf("pack: %zu archivos, %.1f MB\n", files.size(), total / 1048576.0);
    return out ? 0 : 1;
}
