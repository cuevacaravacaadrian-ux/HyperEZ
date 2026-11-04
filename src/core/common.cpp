#include "core/common.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <io.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <climits>
#include <cstdlib>
#endif
#endif

namespace fs = std::filesystem;

namespace hz {

// ---- Texto ---------------------------------------------------------------

std::string formatNumber(double n) {
    if (std::isnan(n)) return "nan";
    if (std::isinf(n)) return n > 0 ? "inf" : "-inf";
    if (n == 0) return "0"; // también -0
    if (std::fabs(n) < 1e16 && n == std::floor(n)) {
        char buf[32];
        auto r = std::to_chars(buf, buf + sizeof buf, (long long)n);
        return std::string(buf, r.ptr);
    }
    char buf[64];
    auto r = std::to_chars(buf, buf + sizeof buf, n);
    return std::string(buf, r.ptr);
}

std::string escapeString(std::string_view s, char quote) {
    std::string out;
    out.reserve(s.size() + 2);
    out += quote;
    for (char c : s) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\\': out += "\\\\"; break;
            case '\0': out += "\\0"; break;
            default:
                if (c == quote) { out += '\\'; out += c; }
                else out += c;
        }
    }
    out += quote;
    return out;
}

std::string toLower(std::string_view s) {
    std::string r(s);
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return std::string(s.substr(a, b - a));
}

std::vector<std::string> splitLines(std::string_view s) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == '\n') {
            size_t end = i;
            if (end > start && s[end - 1] == '\r') end--;
            lines.emplace_back(s.substr(start, end - start));
            start = i + 1;
        }
    }
    return lines;
}

bool startsWith(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.substr(0, p.size()) == p; }
bool endsWith(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.substr(s.size() - p.size()) == p; }

// Distancia de edición con transposiciones (Damerau, alineación óptima): "pirnt" -> "print" = 1.
int levenshtein(std::string_view a, std::string_view b) {
    size_t n = a.size(), m = b.size();
    std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1));
    auto eq = [&](size_t i, size_t j) { return std::tolower((unsigned char)a[i]) == std::tolower((unsigned char)b[j]); };
    for (size_t i = 0; i <= n; i++) d[i][0] = (int)i;
    for (size_t j = 0; j <= m; j++) d[0][j] = (int)j;
    for (size_t i = 1; i <= n; i++)
        for (size_t j = 1; j <= m; j++) {
            d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (eq(i - 1, j - 1) ? 0 : 1)});
            if (i > 1 && j > 1 && eq(i - 1, j - 2) && eq(i - 2, j - 1)) d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
        }
    return d[n][m];
}

std::string closestMatch(std::string_view word, const std::vector<std::string>& candidates) {
    std::string best;
    int bestD = 1 << 30;
    int limit = word.size() <= 3 ? 1 : std::max(2, (int)word.size() / 3);
    for (auto& c : candidates) {
        if (c.empty() || c[0] == '_' || c[0] == ' ') continue;
        int d = levenshtein(word, c);
        if (d < bestD) { bestD = d; best = c; }
    }
    return bestD <= limit ? best : "";
}

// ---- UTF-8 ---------------------------------------------------------------

uint32_t utf8Decode(std::string_view s, size_t& i) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { i++; return c; }
    int n = 0;
    uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
    else { i++; return 0xFFFD; }
    for (int k = 1; k <= n; k++) {
        if (i + k >= s.size()) { i = s.size(); return 0xFFFD; }
        unsigned char cc = (unsigned char)s[i + k];
        if ((cc & 0xC0) != 0x80) { i += k; return 0xFFFD; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    i += n + 1;
    return cp;
}

void utf8Append(std::string& out, uint32_t cp) {
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

size_t utf8Length(std::string_view s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
    return n;
}

size_t utf8Offset(std::string_view s, size_t charIndex) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {
            if (n == charIndex) return i;
            n++;
        }
    }
    return s.size();
}

// ---- Sistema de archivos --------------------------------------------------

static fs::path P(const std::string& s) { return fs::path((const char8_t*)s.c_str()); }
static std::string S(const fs::path& p) {
    auto u = p.u8string();
    std::string r((const char*)u.data(), u.size());
    for (auto& c : r) if (c == '\\') c = '/';
    return r;
}

static FILE* openFile(const std::string& path, const char* mode) {
#ifdef _WIN32
    std::wstring wm(mode, mode + std::strlen(mode));
    return _wfopen(P(path).wstring().c_str(), wm.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

bool readFile(const std::string& path, std::string& out) {
    FILE* f = openFile(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(size > 0 ? (size_t)size : 0);
    size_t got = size > 0 ? std::fread(out.data(), 1, (size_t)size, f) : 0;
    std::fclose(f);
    out.resize(got);
    return true;
}

bool writeFile(const std::string& path, std::string_view data) {
    FILE* f = openFile(path, "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

bool writeFileAtomic(const std::string& path, std::string_view data) {
    std::string tmp = path + ".tmp";
    if (!writeFile(tmp, data)) return false;
    std::error_code ec;
    fs::rename(P(tmp), P(path), ec);
    if (ec) { fs::remove(P(tmp), ec); return false; }
    return true;
}

bool fileExists(const std::string& path) { std::error_code ec; return fs::is_regular_file(P(path), ec); }
bool dirExists(const std::string& path) { std::error_code ec; return fs::is_directory(P(path), ec); }
bool makeDirs(const std::string& path) { std::error_code ec; fs::create_directories(P(path), ec); return dirExists(path); }
std::string pathJoin(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return S(P(a) / P(b));
}
std::string pathDir(const std::string& p) { auto r = S(P(p).parent_path()); return r.empty() ? "." : r; }
std::string pathFilename(const std::string& p) { return S(P(p).filename()); }
std::string pathStem(const std::string& p) { return S(P(p).stem()); }
std::string pathExt(const std::string& p) { return toLower(S(P(p).extension())); }
std::string pathNormalize(const std::string& p) { return S(P(p).lexically_normal()); }
std::string pathAbsolute(const std::string& p) { std::error_code ec; auto a = fs::absolute(P(p), ec); return S(a.lexically_normal()); }
std::string currentDir() { std::error_code ec; return S(fs::current_path(ec)); }

std::string exePath() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof buf / sizeof buf[0]));
    return S(fs::path(std::wstring(buf, n)));
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0) return "";
    char real[PATH_MAX];
    return realpath(buf, real) ? std::string(real) : std::string(buf);
#else
    std::error_code ec;
    return S(fs::read_symlink("/proc/self/exe", ec));
#endif
}

std::string userDataDir(const std::string& app) {
#ifdef _WIN32
    PWSTR w = nullptr;
    std::string base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &w))) {
        base = S(fs::path(w));
        CoTaskMemFree(w);
    } else {
        base = currentDir();
    }
#else
    const char* h = std::getenv("HOME");
    std::string base = h ? std::string(h) + "/.local/share" : currentDir();
#endif
    std::string dir = pathJoin(pathJoin(base, "HyperEZ"), app);
    makeDirs(dir);
    return dir;
}

std::vector<std::string> listFilesRecursive(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!dirExists(dir)) return out;
    for (auto it = fs::recursive_directory_iterator(P(dir), ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec)) out.push_back(S(it->path()));
    }
    std::sort(out.begin(), out.end());
    return out;
}

// ---- Consola --------------------------------------------------------------

static bool g_colors = false;

void consoleInit() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        if (SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) g_colors = true;
    }
    HANDLE ho = GetStdHandle(STD_OUTPUT_HANDLE);
    if (ho != INVALID_HANDLE_VALUE && GetConsoleMode(ho, &mode)) SetConsoleMode(ho, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    g_colors = isatty(2);
#endif
    if (std::getenv("NO_COLOR")) g_colors = false;
    if (std::getenv("HYPEREZ_COLOR")) g_colors = true;
}

bool consoleColors() { return g_colors; }

bool readLine(std::string& out) {
    out.clear();
#ifdef _WIN32
    // En una consola, ReadConsoleW lee bien los acentos y la ñ (la entrada UTF-8 por ReadFile no es fiable).
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (in != INVALID_HANDLE_VALUE && GetConsoleMode(in, &mode)) {
        std::wstring w;
        wchar_t buf[512];
        for (;;) {
            DWORD n = 0;
            if (!ReadConsoleW(in, buf, 512, &n, nullptr)) return false;
            if (n == 0) return !w.empty();
            w.append(buf, n);
            if (w.back() == L'\n') break;
        }
        while (!w.empty() && (w.back() == L'\n' || w.back() == L'\r')) w.pop_back();
        if (w.size() == 1 && w[0] == 0x1A) return false; // Ctrl+Z: fin de la entrada
        int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        out.resize(sz);
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), sz, nullptr, nullptr);
        return true;
    }
#endif
    int c;
    bool any = false;
    while ((c = std::fgetc(stdin)) != EOF) {
        any = true;
        if (c == '\n') break;
        out += (char)c;
    }
    if (!out.empty() && out.back() == '\r') out.pop_back();
    return any;
}

bool stdinIsConsole() {
#ifdef _WIN32
    DWORD mode = 0;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
#else
    return isatty(0);
#endif
}

namespace color {
const char* red() { return g_colors ? "\x1b[91m" : ""; }
const char* yellow() { return g_colors ? "\x1b[93m" : ""; }
const char* blue() { return g_colors ? "\x1b[94m" : ""; }
const char* cyan() { return g_colors ? "\x1b[96m" : ""; }
const char* green() { return g_colors ? "\x1b[92m" : ""; }
const char* magenta() { return g_colors ? "\x1b[95m" : ""; }
const char* gray() { return g_colors ? "\x1b[90m" : ""; }
const char* bold() { return g_colors ? "\x1b[1m" : ""; }
const char* reset() { return g_colors ? "\x1b[0m" : ""; }
}

double nowSeconds() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return duration<double>(steady_clock::now() - start).count();
}

} // namespace hz
