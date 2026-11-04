// HyperEZ Editor: editor de código clásico para Windows, con fondo blanco y todo lo necesario para
// programar en HyperEZ sin instalar nada más:
//   barra de herramientas clásica, pestañas (varios archivos a la vez), colores de sintaxis, números de
//   línea, errores mientras escribes (el mismo analizador que el compilador), ejecutar con F5 (en una
//   consola) o con Mayús+F5 (en el panel de salida), modo watch, consola interactiva, buscar y
//   reemplazar, ir a la línea, sangría automática, comentar líneas, autocompletado (Ctrl+Espacio),
//   formato, imprimir, elegir la fuente, zoom, archivos recientes, plantillas, ejemplos y arrastrar y
//   soltar. Los archivos nuevos se guardan como .hez.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <richole.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tom.h>

#include "lang/vm/vm.h"
#include "lang/sema/analyzer.h"
#include "lang/syntax/lexer.h"
#include "lang/syntax/format.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace hz;
namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------

enum : int {
    ID_EDIT = 100, ID_GUTTER, ID_PANEL, ID_STATUS, ID_COMPLETE, ID_TABS, ID_TOOLBAR, ID_BOTTOMTABS, ID_OUTPUT,
    ID_TB_NEW = 200, ID_TB_OPEN, ID_TB_SAVE, ID_TB_RUN, ID_TB_CHECK, ID_TB_WATCH, ID_TB_REPL, ID_TB_EXAMPLES, ID_TB_HELP,
    // Menú
    CMD_NEW = 1000, CMD_OPEN, CMD_SAVE, CMD_SAVEAS, CMD_EXIT, CMD_OPEN_FOLDER,
    CMD_UNDO, CMD_REDO, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_DELETE, CMD_SELECTALL, CMD_FIND, CMD_FINDNEXT, CMD_REPLACE,
    CMD_GOTO, CMD_COMMENT, CMD_INDENT, CMD_OUTDENT, CMD_COMPLETE, CMD_DUPLICATE, CMD_FORMAT,
    CMD_RUN, CMD_CHECK, CMD_WATCH, CMD_REPL, CMD_TESTS,
    CMD_ZOOMIN, CMD_ZOOMOUT, CMD_ZOOMRESET, CMD_WRAP, CMD_PANEL, CMD_LINENUMBERS,
    CMD_HELP_GUIDE, CMD_HELP_API, CMD_HELP_DOCS, CMD_HELP_KEYS, CMD_ABOUT,
    CMD_CLOSETAB, CMD_NEXTTAB, CMD_PREVTAB, CMD_PRINT, CMD_FONT, CMD_RUNPANEL, CMD_STOP, CMD_TOOLBAR, CMD_STATUSBAR,
    CMD_CLEAROUTPUT, CMD_EXAMPLESMENU, CMD_CLOSEOTHERS,
    CMD_RECENT = 2000,  // 2000..2009
    CMD_EXAMPLE = 3000, // 3000..3999
    CMD_TEMPLATE = 4000 // 4000..4099
};

enum : UINT_PTR { TIMER_HIGHLIGHT = 1, TIMER_CHECK = 2, TIMER_GUTTER = 3 };

HINSTANCE g_inst;
HWND g_wnd, g_edit, g_gutter, g_panel, g_status, g_complete, g_findDlg;
HFONT g_font, g_uiFont, g_gutterFont, g_boldGutterFont;
ITextDocument* g_doc = nullptr;
int g_dpi = 96;
int g_fontPt = 11;
bool g_wrap = false, g_showPanel = true, g_showNumbers = true;
int g_panelHeight = 170;            // alto del panel de abajo (en píxeles a 96 ppp)
int g_splitTop = -1, g_splitBottom = -1; // franja del separador arrastrable (en la ventana)
bool g_splitDrag = false;
std::wstring g_file;   // vacío = sin título
bool g_dirty = false;
std::wstring g_savedText; // texto tal como está guardado (para saber si hay cambios)
bool g_formatting = false;
int g_lastFirstLine = -1;
UINT g_findMsg;
FINDREPLACEW g_fr{};
wchar_t g_findWhat[256] = L"", g_replaceWith[256] = L"";
std::vector<std::wstring> g_recent;
std::vector<std::wstring> g_examples;
WNDPROC g_editProc = nullptr;

struct Problem { int line = 0, col = 0; LONG cpStart = 0, cpEnd = 0; std::wstring text; bool error = true; };
std::vector<Problem> g_problems;

// Un documento abierto en una pestaña
struct Doc {
    HWND edit = nullptr;
    ITextDocument* tom = nullptr;
    std::wstring file, saved;
    bool dirty = false, overwrite = false;
    std::vector<Problem> problems;
};
std::vector<Doc> g_docs;
int g_cur = -1;
HWND g_tabs = nullptr, g_toolbar = nullptr, g_bottomTabs = nullptr, g_output = nullptr;
HFONT g_outFont = nullptr;
bool g_overwrite = false, g_showToolbar = true, g_showStatus = true;
std::wstring g_fontFace = L"Consolas";
int g_bottomPage = 0; // 0: problemas, 1: salida
HMENU g_examplesMenu = nullptr;

// Catálogo para el autocompletado: nombres globales y miembros de módulos/clases
std::vector<std::wstring> g_globals;
std::map<std::wstring, std::vector<std::wstring>> g_members;
std::map<std::wstring, std::wstring> g_signatures;

int S(int v) { return MulDiv(v, g_dpi, 96); }

// ---------------------------------------------------------------------------
// Texto y conversiones
// ---------------------------------------------------------------------------

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

// Texto del editor tal como lo ve RichEdit (los párrafos terminan en \r; cada posición es un carácter UTF-16).
std::wstring editorText() {
    GETTEXTLENGTHEX gl{GTL_DEFAULT | GTL_NUMCHARS, 1200};
    LRESULT n = SendMessageW(g_edit, EM_GETTEXTLENGTHEX, (WPARAM)&gl, 0);
    std::wstring w((size_t)n + 1, L'\0');
    GETTEXTEX gt{(DWORD)((n + 1) * sizeof(wchar_t)), GT_DEFAULT, 1200, nullptr, nullptr};
    LRESULT got = SendMessageW(g_edit, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)w.data());
    w.resize((size_t)got);
    return w;
}

// UTF-8 para el compilador (con \n) y la tabla byte -> posición del editor.
struct Utf8Text {
    std::string text;
    std::vector<LONG> cp; // cp[byte] = posición en el editor
};
Utf8Text toUtf8(const std::wstring& w) {
    Utf8Text r;
    r.text.reserve(w.size() + w.size() / 4);
    r.cp.reserve(w.size() + w.size() / 4 + 1);
    for (size_t i = 0; i < w.size(); i++) {
        uint32_t c = w[i];
        LONG at = (LONG)i;
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < w.size()) { c = 0x10000 + ((c - 0xD800) << 10) + (w[i + 1] - 0xDC00); i++; }
        if (c == L'\r') c = L'\n';
        char buf[4];
        int n;
        if (c < 0x80) { buf[0] = (char)c; n = 1; }
        else if (c < 0x800) { buf[0] = (char)(0xC0 | c >> 6); buf[1] = (char)(0x80 | (c & 63)); n = 2; }
        else if (c < 0x10000) { buf[0] = (char)(0xE0 | c >> 12); buf[1] = (char)(0x80 | (c >> 6 & 63)); buf[2] = (char)(0x80 | (c & 63)); n = 3; }
        else { buf[0] = (char)(0xF0 | c >> 18); buf[1] = (char)(0x80 | (c >> 12 & 63)); buf[2] = (char)(0x80 | (c >> 6 & 63)); buf[3] = (char)(0x80 | (c & 63)); n = 4; }
        for (int k = 0; k < n; k++) { r.text += buf[k]; r.cp.push_back(at); }
    }
    r.cp.push_back((LONG)w.size());
    return r;
}

std::wstring exeDir() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
    return fs::path(std::wstring(buf, n)).parent_path().wstring();
}
std::wstring hyperExe() { return exeDir() + L"\\hyper.exe"; }
std::wstring installDir() { return fs::path(exeDir()).parent_path().wstring(); }

// ---------------------------------------------------------------------------
// Ajustes (registro: HKCU\Software\HyperEZ\Editor)
// ---------------------------------------------------------------------------

const wchar_t* kKey = L"Software\\HyperEZ\\Editor";

DWORD readDword(const wchar_t* name, DWORD def) {
    DWORD v = def, sz = sizeof v;
    RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v;
}
void writeDword(const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof v);
        RegCloseKey(k);
    }
}
void loadSettings() {
    g_fontPt = (int)std::clamp<DWORD>(readDword(L"FontSize", 11), 7, 40);
    g_wrap = readDword(L"WordWrap", 0) != 0;
    g_showPanel = readDword(L"ShowPanel", 1) != 0;
    g_showNumbers = readDword(L"LineNumbers", 1) != 0;
    g_showToolbar = readDword(L"Toolbar", 1) != 0;
    g_showStatus = readDword(L"StatusBar", 1) != 0;
    g_panelHeight = (int)std::clamp<DWORD>(readDword(L"PanelHeight", 170), 60, 2000);
    {
        wchar_t face[LF_FACESIZE];
        DWORD fsz = sizeof face;
        if (RegGetValueW(HKEY_CURRENT_USER, kKey, L"FontFace", RRF_RT_REG_SZ, nullptr, face, &fsz) == ERROR_SUCCESS && face[0]) g_fontFace = face;
    }
    for (int i = 0; i < 10; i++) {
        wchar_t name[16], buf[MAX_PATH * 2];
        wsprintfW(name, L"Recent%d", i);
        DWORD sz = sizeof buf;
        if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS && fs::exists(buf))
            g_recent.push_back(buf);
    }
}
void saveSettings() {
    writeDword(L"FontSize", (DWORD)g_fontPt);
    writeDword(L"WordWrap", g_wrap);
    writeDword(L"ShowPanel", g_showPanel);
    writeDword(L"LineNumbers", g_showNumbers);
    writeDword(L"Toolbar", g_showToolbar);
    writeDword(L"StatusBar", g_showStatus);
    writeDword(L"PanelHeight", (DWORD)g_panelHeight);
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        for (int i = 0; i < 10; i++) {
            wchar_t name[16];
            wsprintfW(name, L"Recent%d", i);
            if (i < (int)g_recent.size())
                RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)g_recent[i].c_str(), (DWORD)((g_recent[i].size() + 1) * sizeof(wchar_t)));
            else RegDeleteValueW(k, name);
        }
        RegSetValueExW(k, L"FontFace", 0, REG_SZ, (const BYTE*)g_fontFace.c_str(), (DWORD)((g_fontFace.size() + 1) * sizeof(wchar_t)));
        WINDOWPLACEMENT wp{sizeof wp};
        GetWindowPlacement(g_wnd, &wp);
        RegSetValueExW(k, L"Placement", 0, REG_BINARY, (const BYTE*)&wp, sizeof wp);
        RegCloseKey(k);
    }
}

// ---------------------------------------------------------------------------
// Catálogo de la API (autocompletado y firmas)
// ---------------------------------------------------------------------------

void buildCatalog() {
    VM vm;
    registerStdlib(vm);
    registerMathTypes(vm);
    buildWorld(vm);
    vm.loadPrelude();
    std::set<std::wstring> globals;
    for (const char* kw : {"var", "let", "const", "function", "return", "if", "else", "for", "foreach", "in", "while", "repeat", "break",
                           "continue", "class", "struct", "constructor", "this", "super", "extends", "implements", "interface", "enum",
                           "import", "from", "as", "private", "static", "true", "false", "null", "and", "or", "not", "is", "try",
                           "catch", "finally", "throw", "async", "await", "match", "event", "on", "once", "emit", "step", "get", "set"})
        globals.insert(widen(kw));
    for (auto& e : vm.catalog) {
        std::wstring name = widen(e.name);
        if (e.owner.empty()) {
            globals.insert(name);
            if (e.kind == ApiKind::Function) g_signatures[name] = name + widen(e.signature());
        } else {
            std::wstring owner = widen(e.owner);
            g_members[owner].push_back(name);
            if (e.kind == ApiKind::Function || e.kind == ApiKind::Method) g_signatures[owner + L"." + name] = owner + L"." + name + widen(e.signature());
        }
    }
    for (auto& lib : vm.bundledLibraryNames()) globals.insert(widen(lib)); // import ai, import zip...
    g_globals.assign(globals.begin(), globals.end());
    for (auto& [k, v] : g_members) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }
}

// ---------------------------------------------------------------------------
// Colores de sintaxis
// ---------------------------------------------------------------------------

const COLORREF C_TEXT = RGB(31, 35, 40), C_KEYWORD = RGB(0, 0, 220), C_STRING = RGB(163, 21, 21), C_NUMBER = RGB(9, 134, 88),
               C_COMMENT = RGB(0, 128, 0), C_TYPE = RGB(38, 127, 153), C_BUILTIN = RGB(121, 94, 38), C_ERRORBG = RGB(255, 222, 222),
               C_GUTTER_BG = RGB(246, 248, 250), C_GUTTER_FG = RGB(140, 149, 159);

const std::set<std::string> kContextual = {"event", "on", "once", "emit", "from", "as", "step", "priority"};
// Tipos básicos (en minúscula) que se colorean cuando aparecen como anotación de tipo
const std::set<std::string> kBasicTypes = {"number", "int", "text", "bool", "list", "map", "set", "any", "void", "function", "object", "never"};

struct Span { LONG a, b; COLORREF color; bool italic; };

// Recoge los tramos de color de una lista de tokens; las interpolaciones ${...} se analizan de nuevo
void collectSpans(const std::vector<Token>& toks, const Utf8Text& u, std::vector<Span>& out, int depth = 0) {
    auto cpAt = [&](int byte) { return u.cp[std::min((size_t)std::max(0, byte), u.cp.size() - 1)]; };
    for (size_t i = 0; i < toks.size(); i++) {
        const Token& t = toks[i];
        if (t.kind == Tok::Eof) break;
        LONG a = cpAt(t.loc.offset), b = cpAt(t.loc.offset + t.loc.length);
        Tok prev = i > 0 ? toks[i - 1].kind : Tok::Eof;
        COLORREF c = C_TEXT;
        if (isKeyword(t.kind)) c = C_KEYWORD;
        else if (t.kind == Tok::Number) c = C_NUMBER;
        else if (t.kind == Tok::String) {
            out.push_back({a, b, C_STRING, false});
            if (t.interpolated && depth < 4) {
                for (auto& part : t.parts) {
                    if (!part.isExpr) continue;
                    LONG ea = cpAt(part.loc.offset), eb = cpAt(part.loc.offset + (int)part.text.size());
                    out.push_back({cpAt(part.loc.offset - 2), ea, C_KEYWORD, false}); // ${
                    out.push_back({ea, eb, C_TEXT, false});
                    out.push_back({eb, cpAt(part.loc.offset + (int)part.text.size() + 1), C_KEYWORD, false}); // }
                    DiagnosticBag bag;
                    Lexer sub(part.text, bag, part.loc);
                    collectSpans(sub.tokenize(), u, out, depth + 1);
                }
            }
            continue;
        } else if (t.kind == Tok::Ident) {
            bool call = i + 1 < toks.size() && toks[i + 1].kind == Tok::LParen;
            bool typePos = prev == Tok::Colon || prev == Tok::ThinArrow || prev == Tok::KwIs || prev == Tok::Less ||
                           (prev == Tok::Ident && toks[i - 1].text == "as");
            if (kContextual.count(t.text) && !call) c = C_KEYWORD;
            else if (!t.text.empty() && std::isupper((unsigned char)t.text[0])) c = C_TYPE;
            else if (typePos && kBasicTypes.count(t.text)) c = C_TYPE;
            else if (call) c = C_BUILTIN;
        }
        if (c != C_TEXT) out.push_back({a, b, c, false});
    }
}

void highlight() {
    if (!g_doc) return;
    std::wstring w = editorText();
    Utf8Text u = toUtf8(w);
    DiagnosticBag bag;
    Lexer lx(u.text, bag);
    std::vector<Token> toks = lx.tokenize();
    auto cpAt = [&](size_t byte) { return u.cp[std::min(byte, u.cp.size() - 1)]; };

    std::vector<Span> spans;
    spans.reserve(toks.size());
    collectSpans(toks, u, spans);
    for (auto& cm : lx.comments) { // los comentarios el lexer los guarda aparte
        size_t start = (size_t)cm.loc.offset, end;
        if (cm.block) { end = u.text.find("*/", start + 2); end = end == std::string::npos ? u.text.size() : end + 2; }
        else { end = u.text.find('\n', start); if (end == std::string::npos) end = u.text.size(); }
        spans.push_back({cpAt(start), cpAt(end), C_COMMENT, true});
    }
    std::stable_sort(spans.begin(), spans.end(), [](const Span& x, const Span& y) { return x.a < y.a; });
    // Unir tramos del mismo color separados solo por espacios: menos llamadas a RichEdit
    std::vector<Span> merged;
    merged.reserve(spans.size());
    for (auto& s : spans) {
        if (s.b <= s.a) continue;
        if (!merged.empty()) {
            Span& m = merged.back();
            bool gapBlank = s.a >= m.b && s.a - m.b < 64 &&
                            std::all_of(w.begin() + m.b, w.begin() + s.a, [](wchar_t ch) { return ch == L' ' || ch == L'\t'; });
            if (m.color == s.color && m.italic == s.italic && gapBlank) { m.b = std::max(m.b, s.b); continue; }
        }
        merged.push_back(s);
    }

    g_formatting = true;
    long frozen = 0;
    g_doc->Undo(tomSuspend, nullptr);
    g_doc->Freeze(&frozen);
    ITextRange* r = nullptr;
    g_doc->Range(0, (long)w.size() + 1, &r);
    if (r) {
        ITextFont* f = nullptr;
        if (r->GetFont(&f) == S_OK && f) {
            f->SetForeColor((long)C_TEXT);
            f->SetItalic(tomFalse);
            f->SetBold(tomFalse);
            f->SetBackColor(tomAutoColor);
            f->Release();
        }
        for (auto& s : merged) {
            r->SetRange(s.a, s.b);
            ITextFont* sf = nullptr;
            if (r->GetFont(&sf) == S_OK && sf) {
                sf->SetForeColor((long)s.color);
                if (s.italic) sf->SetItalic(tomTrue);
                sf->Release();
            }
        }
        // fondo rojo claro donde hay errores
        for (auto& p : g_problems) {
            if (!p.error || p.cpEnd <= p.cpStart) continue;
            r->SetRange(p.cpStart, p.cpEnd);
            ITextFont* ef = nullptr;
            if (r->GetFont(&ef) == S_OK && ef) { ef->SetBackColor((long)C_ERRORBG); ef->Release(); }
        }
        r->Release();
    }
    g_doc->Unfreeze(&frozen);
    g_doc->Undo(tomResume, nullptr);
    g_formatting = false;
}

// ---------------------------------------------------------------------------
// Comprobación de errores (el mismo analizador que el compilador)
// ---------------------------------------------------------------------------

void updateStatus();
void showProblems();

void checkProgram() {
    std::wstring w = editorText();
    Utf8Text u = toUtf8(w);
    std::vector<Problem> found;
    {
        VM vm;
        registerStdlib(vm);
        registerMathTypes(vm);
        buildWorld(vm);
        vm.loadPrelude();
        std::string path = g_file.empty() ? narrow((fs::current_path() / L"sin-titulo.hez").wstring()) : narrow(g_file);
        vm.projectRoot = narrow(fs::path(widen(path)).parent_path().wstring());
        ModuleUnit* unit = vm.compileSource(path, u.text, true, false);
        for (auto& d : unit->diags.items) {
            if (d.severity != Severity::Error && d.severity != Severity::Warning) continue;
            Problem p;
            p.error = d.severity == Severity::Error;
            p.line = d.loc.line;
            p.col = d.loc.col;
            size_t a = (size_t)std::max(0, d.loc.offset), b = a + (size_t)std::max(1, d.loc.length);
            p.cpStart = u.cp[std::min(a, u.cp.size() - 1)];
            p.cpEnd = u.cp[std::min(b, u.cp.size() - 1)];
            p.text = widen(d.message) + (d.hint.empty() ? L"" : L"  (" + widen(d.hint) + L")");
            found.push_back(p);
        }
        for (auto* imp : unit->imports) {
            if (imp->diags.hasErrors()) {
                Problem p;
                p.text = L"El módulo importado " + widen(pathFilename(imp->path)) + L" tiene errores";
                found.push_back(p);
            }
        }
    }
    g_problems = found;
    showProblems();
    highlight();
    updateStatus();
}

void showProblems() {
    SendMessageW(g_panel, LB_RESETCONTENT, 0, 0);
    if (g_problems.empty()) {
        SendMessageW(g_panel, LB_ADDSTRING, 0, (LPARAM)L"✓ Sin errores. F5 ejecuta en una consola; Mayús+F5, aquí en el panel de salida.");
    } else {
        for (auto& p : g_problems) {
            wchar_t head[64];
            wsprintfW(head, p.line > 0 ? L"%s  línea %d, columna %d:  " : L"%s  ", p.error ? L"✗ Error" : L"⚠ Aviso", p.line, p.col);
            std::wstring item = head + p.text;
            SendMessageW(g_panel, LB_ADDSTRING, 0, (LPARAM)item.c_str());
        }
    }
    if (g_bottomTabs) {
        std::wstring label = g_problems.empty() ? L"Problemas" : L"Problemas (" + std::to_wstring(g_problems.size()) + L")";
        TCITEMW it{};
        it.mask = TCIF_TEXT;
        it.pszText = label.data();
        SendMessageW(g_bottomTabs, TCM_SETITEMW, 0, (LPARAM)&it);
    }
}

// ---------------------------------------------------------------------------
// Barra de estado, título y números de línea
// ---------------------------------------------------------------------------

void updateTabLabel(int i);

void updateTitle() {
    std::wstring name = g_file.empty() ? L"sin título.hez" : fs::path(g_file).filename().wstring();
    std::wstring t = (g_dirty ? L"• " : L"") + name + L" — HyperEZ Editor";
    SetWindowTextW(g_wnd, t.c_str());
    updateTabLabel(g_cur);
}

void updateStatus() {
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    LONG line = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax);
    LONG start = (LONG)SendMessageW(g_edit, EM_LINEINDEX, line, 0);
    wchar_t pos[64];
    wsprintfW(pos, L"  Línea %ld, columna %ld", line + 1, sel.cpMax - start + 1);
    SendMessageW(g_status, SB_SETTEXTW, 0, (LPARAM)pos);
    int errors = 0, warns = 0;
    for (auto& p : g_problems) (p.error ? errors : warns)++;
    wchar_t pr[64];
    if (errors) wsprintfW(pr, L"  ✗ %d error%s", errors, errors == 1 ? L"" : L"es");
    else if (warns) wsprintfW(pr, L"  ⚠ %d aviso%s", warns, warns == 1 ? L"" : L"s");
    else wsprintfW(pr, L"  ✓ Sin errores");
    SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)pr);
    std::wstring ext = g_file.empty() ? L".hez" : fs::path(g_file).extension().wstring();
    std::wstring fmt = L"  UTF-8 · " + ext + L" · " + g_fontFace + L" " + std::to_wstring(g_fontPt) + L" pt";
    SendMessageW(g_status, SB_SETTEXTW, 2, (LPARAM)fmt.c_str());
    SendMessageW(g_status, SB_SETTEXTW, 3, (LPARAM)(g_overwrite ? L"  SOB" : L"  INS"));
    std::wstring ver = L"  HyperEZ " + widen(HYPEREZ_VERSION);
    SendMessageW(g_status, SB_SETTEXTW, 4, (LPARAM)ver.c_str());
    InvalidateRect(g_gutter, nullptr, FALSE);
}

int gutterWidth() {
    if (!g_showNumbers) return 0;
    LONG lines = (LONG)SendMessageW(g_edit, EM_GETLINECOUNT, 0, 0);
    int digits = std::max(3, (int)std::to_wstring(lines).size());
    HDC dc = GetDC(g_gutter);
    HGDIOBJ old = SelectObject(dc, g_gutterFont);
    SIZE sz;
    GetTextExtentPoint32W(dc, L"0", 1, &sz);
    SelectObject(dc, old);
    ReleaseDC(g_gutter, dc);
    return sz.cx * digits + S(18);
}

LRESULT CALLBACK gutterProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);
        HBRUSH bg = CreateSolidBrush(C_GUTTER_BG);
        FillRect(mem, &rc, bg);
        DeleteObject(bg);
        RECT edge = {rc.right - 1, 0, rc.right, rc.bottom};
        HBRUSH eb = CreateSolidBrush(RGB(222, 226, 230));
        FillRect(mem, &edge, eb);
        DeleteObject(eb);
        SetBkMode(mem, TRANSPARENT);
        RECT er;
        GetWindowRect(g_edit, &er);
        POINT origin = {er.left, er.top};
        ScreenToClient(h, &origin);
        LONG first = (LONG)SendMessageW(g_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
        LONG count = (LONG)SendMessageW(g_edit, EM_GETLINECOUNT, 0, 0);
        CHARRANGE sel;
        SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
        LONG cur = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax);
        std::set<int> errLines;
        for (auto& p : g_problems) if (p.error) errLines.insert(p.line - 1);
        for (LONG i = first; i < count; i++) {
            LONG cp = (LONG)SendMessageW(g_edit, EM_LINEINDEX, i, 0);
            POINTL pt{};
            SendMessageW(g_edit, EM_POSFROMCHAR, (WPARAM)&pt, cp);
            int y = pt.y + origin.y;
            if (y > rc.bottom) break;
            std::wstring num = std::to_wstring(i + 1);
            bool isCur = i == cur;
            SelectObject(mem, isCur ? g_boldGutterFont : g_gutterFont);
            SetTextColor(mem, errLines.count(i) ? RGB(207, 34, 46) : isCur ? RGB(31, 35, 40) : C_GUTTER_FG);
            RECT tr = {0, y, rc.right - S(8), y + S(40)};
            DrawTextW(mem, num.c_str(), (int)num.size(), &tr, DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOCLIP);
            if (errLines.count(i)) {
                HBRUSH dot = CreateSolidBrush(RGB(207, 34, 46));
                RECT d = {S(3), y + S(6), S(7), y + S(10)};
                FillRect(mem, &d, dot);
                DeleteObject(dot);
            }
        }
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) { SetFocus(g_edit); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Disposición de la ventana
// ---------------------------------------------------------------------------

// Alto de la tira de pestañas de un control de pestañas (sin su área de contenido)
int tabStripHeight(HWND tabs, int width) {
    RECT r{0, 0, width, S(300)};
    SendMessageW(tabs, TCM_ADJUSTRECT, FALSE, (LPARAM)&r);
    return std::max(S(20), (int)r.top - S(2));
}

void layout() {
    if (!g_wnd) return;
    RECT rc;
    GetClientRect(g_wnd, &rc);
    int statusH = 0;
    if (g_status) {
        ShowWindow(g_status, g_showStatus ? SW_SHOW : SW_HIDE);
        if (g_showStatus) {
            SendMessageW(g_status, WM_SIZE, 0, 0);
            RECT sr;
            GetWindowRect(g_status, &sr);
            statusH = sr.bottom - sr.top;
            int parts[5] = {S(190), S(370), S(640), S(700), -1};
            SendMessageW(g_status, SB_SETPARTS, 5, (LPARAM)parts);
        }
    }
    int top = 0;
    if (g_toolbar) {
        ShowWindow(g_toolbar, g_showToolbar ? SW_SHOW : SW_HIDE);
        if (g_showToolbar) {
            DWORD bs = (DWORD)SendMessageW(g_toolbar, TB_GETBUTTONSIZE, 0, 0);
            int th = HIWORD(bs) + S(6);
            MoveWindow(g_toolbar, 0, 0, rc.right, th, TRUE);
            top = th;
        }
    }
    if (g_tabs) {
        int th = tabStripHeight(g_tabs, rc.right);
        MoveWindow(g_tabs, 0, top, rc.right, th, TRUE);
        top += th;
    }
    // El panel de abajo se puede agrandar arrastrando el separador; el código conserva al menos S(80)
    int gap = g_showPanel ? S(5) : 0;
    int panelH = 0;
    if (g_showPanel) panelH = std::max(S(60), std::min(S(g_panelHeight), (int)rc.bottom - statusH - top - gap - S(80)));
    int bottom = rc.bottom - statusH - panelH;
    int editBottom = std::max(top, bottom - gap);
    g_splitTop = g_showPanel ? editBottom : -1;
    g_splitBottom = g_showPanel ? bottom : -1;
    int gw = gutterWidth();
    MoveWindow(g_gutter, 0, top, gw, editBottom - top, TRUE);
    ShowWindow(g_gutter, g_showNumbers ? SW_SHOW : SW_HIDE);
    if (g_edit) MoveWindow(g_edit, gw, top, rc.right - gw, editBottom - top, TRUE);
    if (g_showPanel) { RECT gr{0, editBottom, rc.right, bottom}; InvalidateRect(g_wnd, &gr, TRUE); } // la franja del separador
    if (g_bottomTabs) {
        ShowWindow(g_bottomTabs, g_showPanel ? SW_SHOW : SW_HIDE);
        int bh = tabStripHeight(g_bottomTabs, rc.right);
        MoveWindow(g_bottomTabs, 0, bottom, rc.right, bh, TRUE);
        int ch = std::max(0, panelH - bh);
        MoveWindow(g_panel, 0, bottom + bh, rc.right, ch, TRUE);
        MoveWindow(g_output, 0, bottom + bh, rc.right, ch, TRUE);
        ShowWindow(g_panel, g_showPanel && g_bottomPage == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_output, g_showPanel && g_bottomPage == 1 ? SW_SHOW : SW_HIDE);
    }
}

void applyFontTo(HWND edit, ITextDocument* tom) {
    if (!edit) return;
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof cf;
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_CHARSET;
    wcsncpy_s(cf.szFaceName, g_fontFace.c_str(), _TRUNCATE);
    cf.yHeight = g_fontPt * 20;
    cf.bCharSet = DEFAULT_CHARSET;
    g_formatting = true;
    if (tom) tom->Undo(tomSuspend, nullptr);
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    // tabuladores de 4 espacios
    PARAFORMAT2 pf{};
    pf.cbSize = sizeof pf;
    pf.dwMask = PFM_TABSTOPS;
    HDC dc = GetDC(edit);
    HGDIOBJ old = SelectObject(dc, g_font);
    SIZE sz;
    GetTextExtentPoint32W(dc, L"0000", 4, &sz);
    SelectObject(dc, old);
    ReleaseDC(edit, dc);
    pf.cTabCount = 32;
    for (int i = 0; i < 32; i++) pf.rgxTabs[i] = MulDiv(sz.cx * (i + 1), 1440, g_dpi);
    SendMessageW(edit, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
    if (tom) tom->Undo(tomResume, nullptr);
    g_formatting = false;
}

void applyFont() {
    if (g_font) DeleteObject(g_font);
    if (g_gutterFont) DeleteObject(g_gutterFont);
    if (g_boldGutterFont) DeleteObject(g_boldGutterFont);
    int px = -MulDiv(g_fontPt, g_dpi, 72);
    const wchar_t* face = g_fontFace.c_str();
    g_font = CreateFontW(px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, face);
    g_gutterFont = CreateFontW(px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, face);
    g_boldGutterFont = CreateFontW(px, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, face);
    for (auto& d : g_docs) applyFontTo(d.edit, d.tom);
    layout();
    updateStatus();
}

// ---------------------------------------------------------------------------
// Archivos
// ---------------------------------------------------------------------------

void setText(const std::wstring& w) {
    g_formatting = true;
    SETTEXTEX st{ST_DEFAULT, 1200};
    SendMessageW(g_edit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)w.c_str());
    SendMessageW(g_edit, EM_EMPTYUNDOBUFFER, 0, 0);
    g_formatting = false;
    g_dirty = false;
    g_savedText = editorText();
    applyFontTo(g_edit, g_doc);
    layout();
    checkProgram();
    updateTitle();
}

void addRecent(const std::wstring& f) {
    g_recent.erase(std::remove_if(g_recent.begin(), g_recent.end(), [&](const std::wstring& x) { return _wcsicmp(x.c_str(), f.c_str()) == 0; }),
                   g_recent.end());
    g_recent.insert(g_recent.begin(), f);
    if (g_recent.size() > 10) g_recent.resize(10);
}

void rebuildMenus();

int addTab();
void activate(int i);
void storeCurrent();

bool openFile(const std::wstring& path) {
    std::wstring abs = fs::absolute(path).wstring();
    storeCurrent();
    for (size_t i = 0; i < g_docs.size(); i++) // ya abierto: ir a su pestaña
        if (!g_docs[i].file.empty() && _wcsicmp(g_docs[i].file.c_str(), abs.c_str()) == 0) { activate((int)i); return true; }
    std::string data;
    if (!readFile(narrow(path), data)) {
        MessageBoxW(g_wnd, (L"No se puede abrir el archivo:\n" + path).c_str(), L"HyperEZ Editor", MB_ICONERROR);
        return false;
    }
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) data.erase(0, 3);
    std::wstring w = widen(data);
    std::wstring clean;
    clean.reserve(w.size());
    for (size_t i = 0; i < w.size(); i++) {
        if (w[i] == L'\r') { if (i + 1 < w.size() && w[i + 1] == L'\n') continue; clean += L'\r'; }
        else if (w[i] == L'\n') clean += L'\r';
        else clean += w[i];
    }
    // se reutiliza la pestaña activa si es un documento nuevo que no se ha tocado
    bool reuse = g_cur >= 0 && g_file.empty() && !g_dirty;
    if (!reuse) addTab();
    g_file = abs;
    setText(clean);
    addRecent(g_file);
    rebuildMenus();
    SetCurrentDirectoryW(fs::path(g_file).parent_path().c_str());
    return true;
}

bool writeTo(const std::wstring& path) {
    std::wstring w = editorText();
    std::wstring out;
    out.reserve(w.size() + w.size() / 20);
    for (wchar_t c : w) { if (c == L'\r') out += L"\r\n"; else out += c; }
    if (!writeFileAtomic(narrow(path), narrow(out))) {
        MessageBoxW(g_wnd, (L"No se puede guardar en:\n" + path).c_str(), L"HyperEZ Editor", MB_ICONERROR);
        return false;
    }
    g_file = path;
    g_dirty = false;
    g_savedText = w;
    addRecent(path);
    rebuildMenus();
    updateTitle();
    updateStatus();
    return true;
}

// Documentos\HyperEZ: la carpeta de "mis programas" (la crea el instalador; si no existe, se crea aquí)
std::wstring projectsDir() {
    PWSTR p = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p))) dir = std::wstring(p) + L"\\HyperEZ";
    CoTaskMemFree(p);
    std::error_code ec;
    if (!dir.empty()) fs::create_directories(dir, ec);
    return dir;
}

bool saveAs(const wchar_t* title = L"Guardar programa HyperEZ") {
    wchar_t buf[MAX_PATH * 2] = L"programa.hez";
    if (!g_file.empty()) wcscpy_s(buf, fs::path(g_file).filename().c_str());
    std::wstring initial = g_file.empty() ? projectsDir() : fs::path(g_file).parent_path().wstring();
    OPENFILENAMEW of{sizeof of};
    of.hwndOwner = g_wnd;
    of.lpstrInitialDir = initial.empty() ? nullptr : initial.c_str();
    of.lpstrFilter = L"Programa HyperEZ (*.hez)\0*.hez\0Programa HyperEZ (*.hz)\0*.hz\0Todos los archivos\0*.*\0";
    of.lpstrFile = buf;
    of.nMaxFile = (DWORD)std::size(buf);
    of.lpstrDefExt = L"hez";
    of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOREADONLYRETURN;
    of.lpstrTitle = title;
    if (!GetSaveFileNameW(&of)) return false;
    return writeTo(buf);
}

bool save() { return g_file.empty() ? saveAs() : writeTo(g_file); }

// ¿Se puede descartar el texto actual? (pregunta si hay cambios sin guardar)
bool confirmDiscard() {
    if (!g_dirty) return true;
    std::wstring name = g_file.empty() ? L"sin título.hez" : fs::path(g_file).filename().wstring();
    int r = MessageBoxW(g_wnd, (L"¿Quieres guardar los cambios de " + name + L"?").c_str(), L"HyperEZ Editor", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return false;
    if (r == IDYES) return save();
    return true;
}

const wchar_t* kWelcome =
    L"// ¡Bienvenido a HyperEZ!  Pulsa F5 para ejecutar este programa.\r"
    L"// F7 comprueba los errores · Ctrl+Espacio completa nombres · Ayuda tiene la guía y ejemplos.\r"
    L"\r"
    L"var nombre = \"mundo\"\r"
    L"print(\"¡Hola, ${nombre}!\")\r"
    L"\r"
    L"for i in 1..=3 {\r"
    L"    print(\"Cuenta:\", i)\r"
    L"}\r";

struct Template { const wchar_t* name; const wchar_t* code; };
const Template kTemplates[] = {
    {L"Programa vacío", L"// Mi programa HyperEZ\r\r"},
    {L"Hola, mundo", nullptr},
    {L"Juego de consola", L"// Juego: mueve la @ con las flechas; Esc para salir\r"
                          L"var x = 10\rvar jugando = Console.interactive\r"
                          L"while jugando {\r    Console.clear()\r    Console.print(\" \".repeat(x) + \"@\", \"cyan\")\r"
                          L"    Console.print(\"← → para moverte · Esc para salir\", \"gray\")\r"
                          L"    match Console.readKey() {\r        \"left\" => x = max(0, x - 1)\r        \"right\" => x = min(Console.width - 2, x + 1)\r"
                          L"        \"escape\" => jugando = false\r        else => null\r    }\r}\r"},
    {L"Clase con pruebas", L"class Contador {\r    var valor = 0\r    function sumar(n: number = 1) { valor += n }\r}\r\r"
                           L"test(\"el contador suma\", () => {\r    var c = Contador()\r    c.sumar()\r    c.sumar(5)\r"
                           L"    assertEqual(c.valor, 6)\r})\r"},
    {L"Inteligencia artificial", L"import ai\r\rrandomSeed(1)\rvar red = ai.NeuralNetwork([2, 8, 1], \"tanh\")\r"
                                 L"var X = [[0, 0], [0, 1], [1, 0], [1, 1]]\rred.train(X, [[0], [1], [1], [0]], 2000, 0.5)\r"
                                 L"print(\"XOR:\", red.classify(X))\r"},
    {L"Servidor web", L"import web\r\rvar app = web.App()\rapp.get(\"/\", req => \"<h1>¡Hola desde HyperEZ!</h1>\")\r"
                      L"app.get(\"/api/hora\", req => ({ hora: Time.format(Time.now(), \"%H:%M:%S\") }))\r"
                      L"print(\"Abre http://localhost:8080 en el navegador\")\rapp.listen(8080)\r"},
    {L"Dibujar una imagen", L"var im = Image(400, 200, \"#ffffff\")\rim.rect(20, 20, 160, 100, \"#2f81f7\")\r"
                            L"im.circle(280, 80, 50, \"#ffc857\")\rim.text(20, 150, \"Hecho con HyperEZ\", \"#1f2328\", 24)\r"
                            L"im.save(\"dibujo.png\")\rProcess.open(Path.absolute(\"dibujo.png\"))\r"},
    {L"Leer un archivo CSV", L"import csv\rimport stats\r\rvar filas = csv.parse(\"mes,ventas\\nene,120\\nfeb,95\\nmar,160\")\r"
                             L"var ventas = filas.map(f => f.ventas)\rprint(\"Media:\", stats.mean(ventas), \"Máximo:\", ventas.max())\r"},
};

void newFile(const wchar_t* code = nullptr) {
    bool reuse = g_cur >= 0 && g_file.empty() && !g_dirty && editorText().empty();
    if (!reuse) addTab();
    g_file.clear();
    setText(code ? code : L"");
    LONG end = (LONG)editorText().size();
    CHARRANGE cr{end, end};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
}

void openDialog() {
    wchar_t buf[MAX_PATH * 2] = L"";
    std::wstring initial = g_file.empty() ? projectsDir() : fs::path(g_file).parent_path().wstring();
    OPENFILENAMEW of{sizeof of};
    of.hwndOwner = g_wnd;
    of.lpstrInitialDir = initial.empty() ? nullptr : initial.c_str();
    of.lpstrFilter = L"Programas HyperEZ (*.hez, *.hz)\0*.hez;*.hz\0Todos los archivos\0*.*\0";
    of.lpstrFile = buf;
    of.nMaxFile = (DWORD)std::size(buf);
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    of.lpstrTitle = L"Abrir programa HyperEZ";
    if (GetOpenFileNameW(&of)) openFile(buf);
}

// ---------------------------------------------------------------------------
// Ejecutar
// ---------------------------------------------------------------------------

void launch(const std::wstring& args, const std::wstring& cwd, bool viaCmd = false) {
    std::wstring exe = hyperExe();
    if (!fs::exists(exe)) {
        MessageBoxW(g_wnd, L"No se encuentra hyper.exe junto al editor. Reinstala HyperEZ.", L"HyperEZ Editor", MB_ICONERROR);
        return;
    }
    std::wstring cmd = viaCmd ? L"cmd.exe /c \"\"" + exe + L"\" " + args + L" & echo. & pause\""
                              : L"\"" + exe + L"\" " + args;
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        MessageBoxW(g_wnd, L"No se pudo ejecutar hyper.exe.", L"HyperEZ Editor", MB_ICONERROR);
    }
}

// Guarda (pidiendo nombre si hace falta) antes de ejecutar
bool ensureSaved() {
    if (g_file.empty()) return saveAs(L"Guarda tu programa para ejecutarlo (se guarda como .hez)");
    return !g_dirty || save();
}

void runProgram() {
    if (!ensureSaved()) return;
    checkProgram();
    int errors = (int)std::count_if(g_problems.begin(), g_problems.end(), [](const Problem& p) { return p.error; });
    if (errors) {
        if (!g_showPanel) { g_showPanel = true; layout(); }
        MessageBeep(MB_ICONWARNING);
        SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)L"  ✗ Corrige los errores para ejecutar");
        return;
    }
    launch(L"run --pause \"" + g_file + L"\"", fs::path(g_file).parent_path().wstring());
}

// ---------------------------------------------------------------------------
// Edición: sangría, comentarios, duplicar, ir a línea, autocompletado
// ---------------------------------------------------------------------------

std::wstring lineText(LONG line) {
    LONG start = (LONG)SendMessageW(g_edit, EM_LINEINDEX, line, 0);
    LONG len = (LONG)SendMessageW(g_edit, EM_LINELENGTH, start, 0);
    std::wstring buf((size_t)len + 2, L'\0');
    *(WORD*)buf.data() = (WORD)(len + 1);
    LRESULT n = SendMessageW(g_edit, EM_GETLINE, line, (LPARAM)buf.data());
    buf.resize((size_t)n);
    while (!buf.empty() && (buf.back() == L'\r' || buf.back() == L'\n')) buf.pop_back();
    return buf;
}

void replaceSel(const std::wstring& s) { SendMessageW(g_edit, EM_REPLACESEL, TRUE, (LPARAM)s.c_str()); }

// Aplica f a cada línea seleccionada (sangrar, comentar...)
template <class F> void forSelectedLines(F f) {
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    LONG l0 = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMin);
    LONG l1 = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax);
    if (l1 > l0 && sel.cpMax == (LONG)SendMessageW(g_edit, EM_LINEINDEX, l1, 0)) l1--;
    std::vector<std::wstring> lines;
    for (LONG l = l0; l <= l1; l++) lines.push_back(lineText(l));
    f(lines);
    std::wstring joined;
    for (size_t i = 0; i < lines.size(); i++) { if (i) joined += L'\r'; joined += lines[i]; }
    LONG a = (LONG)SendMessageW(g_edit, EM_LINEINDEX, l0, 0);
    LONG bStart = (LONG)SendMessageW(g_edit, EM_LINEINDEX, l1, 0);
    LONG b = bStart + (LONG)SendMessageW(g_edit, EM_LINELENGTH, bStart, 0);
    CHARRANGE cr{a, b};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
    replaceSel(joined);
    cr = {a, a + (LONG)joined.size()};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
}

void indentLines(bool in) {
    forSelectedLines([&](std::vector<std::wstring>& ls) {
        for (auto& l : ls) {
            if (in) l = L"    " + l;
            else { size_t n = 0; while (n < 4 && n < l.size() && l[n] == L' ') n++; if (n == 0 && !l.empty() && l[0] == L'\t') n = 1; l.erase(0, n); }
        }
    });
}

void toggleComment() {
    forSelectedLines([](std::vector<std::wstring>& ls) {
        bool all = std::all_of(ls.begin(), ls.end(), [](const std::wstring& l) {
            size_t p = l.find_first_not_of(L" \t");
            return p == std::wstring::npos || l.compare(p, 2, L"//") == 0;
        });
        for (auto& l : ls) {
            size_t p = l.find_first_not_of(L" \t");
            if (p == std::wstring::npos) continue;
            if (all) { l.erase(p, l.compare(p, 3, L"// ") == 0 ? 3 : 2); }
            else l.insert(p, L"// ");
        }
    });
}

void duplicateLine() {
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    LONG line = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax);
    std::wstring t = lineText(line);
    LONG start = (LONG)SendMessageW(g_edit, EM_LINEINDEX, line, 0);
    LONG end = start + (LONG)t.size();
    CHARRANGE cr{end, end};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
    replaceSel(L"\r" + t);
}

// Formatear documento (lo mismo que "hyper fmt"): se puede deshacer con Ctrl+Z
void formatDocument() {
    std::wstring w = editorText();
    FormatResult r = formatSource(toUtf8(w).text);
    if (!r.ok) {
        SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)(L"  No se puede formatear: " + widen(r.error)).c_str());
        MessageBeep(MB_ICONWARNING);
        return;
    }
    std::wstring out;
    for (wchar_t c : widen(r.text)) out += c == L'\n' ? L'\r' : c;
    if (!out.empty() && out.back() == L'\r' && (w.empty() || w.back() != L'\r')) out.pop_back(); // RichEdit ya tiene el último párrafo
    if (out == w) { SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)L"  El formato ya era correcto"); return; }
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    LONG line = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax);
    LONG first = (LONG)SendMessageW(g_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
    CHARRANGE all{0, -1};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&all);
    replaceSel(out);
    // volver a la misma línea (al final de su sangría) y a la misma zona visible
    LONG cp = (LONG)SendMessageW(g_edit, EM_LINEINDEX, line, 0);
    if (cp < 0) cp = 0;
    std::wstring lt = lineText(line);
    cp += (LONG)std::min(lt.size(), lt.find_first_not_of(L' ') == std::wstring::npos ? lt.size() : lt.find_first_not_of(L' '));
    CHARRANGE cr{cp, cp};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
    LONG nowFirst = (LONG)SendMessageW(g_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
    SendMessageW(g_edit, EM_LINESCROLL, 0, first - nowFirst);
    SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)L"  ✓ Documento formateado (Ctrl+Z lo deshace)");
}

// Al escribir "(" tras una función, su firma aparece en la barra de estado
void showCallSignature() {
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    LONG line = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax);
    LONG ls = (LONG)SendMessageW(g_edit, EM_LINEINDEX, line, 0);
    std::wstring t = lineText(line).substr(0, (size_t)std::max(0L, sel.cpMax - ls));
    if (t.empty() || t.back() != L'(') return;
    t.pop_back();
    size_t i = t.size();
    auto isId = [](wchar_t c) { return iswalnum(c) || c == L'_' || c == L'.' || c > 127; };
    while (i > 0 && isId(t[i - 1])) i--;
    std::wstring name = t.substr(i);
    auto it = g_signatures.find(name);
    if (it == g_signatures.end() && name.find(L'.') != std::wstring::npos) it = g_signatures.find(name.substr(name.rfind(L'.') + 1));
    if (it != g_signatures.end()) SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)(L"  " + it->second).c_str());
}

// Ventana sencilla para pedir un texto (ir a la línea)
std::wstring g_inputResult;
bool g_inputDone = false, g_inputOk = false;
LRESULT CALLBACK inputProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
                g_inputOk = LOWORD(wp) == IDOK;
                wchar_t buf[256];
                GetDlgItemTextW(h, 10, buf, 256);
                g_inputResult = buf;
                g_inputDone = true;
                return 0;
            }
            break;
        case WM_CLOSE: g_inputDone = true; g_inputOk = false; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
bool askText(const wchar_t* title, const wchar_t* label, std::wstring& value) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = inputProc;
        wc.hInstance = g_inst;
        wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wc.lpszClassName = L"HzInput";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        reg = true;
    }
    RECT pr;
    GetWindowRect(g_wnd, &pr);
    int w = S(320), hgt = S(140);
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"HzInput", title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             pr.left + (pr.right - pr.left - w) / 2, pr.top + S(120), w, hgt, g_wnd, nullptr, g_inst, nullptr);
    auto ctl = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int cw, int ch, int id) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(cw), S(ch), h, (HMENU)(INT_PTR)id, g_inst, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        return c;
    };
    ctl(L"STATIC", label, 0, 12, 12, 290, 18, 0);
    HWND edit = ctl(L"EDIT", value.c_str(), WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 12, 34, 290, 24, 10);
    ctl(L"BUTTON", L"Aceptar", WS_TABSTOP | BS_DEFPUSHBUTTON, 128, 70, 84, 28, IDOK);
    ctl(L"BUTTON", L"Cancelar", WS_TABSTOP, 218, 70, 84, 28, IDCANCEL);
    EnableWindow(g_wnd, FALSE);
    ShowWindow(h, SW_SHOW);
    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, 0, -1);
    g_inputDone = false;
    MSG m;
    while (!g_inputDone && GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_KEYDOWN && m.wParam == VK_RETURN) { SendMessageW(h, WM_COMMAND, IDOK, 0); continue; }
        if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE) { SendMessageW(h, WM_COMMAND, IDCANCEL, 0); continue; }
        if (!IsDialogMessageW(h, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    EnableWindow(g_wnd, TRUE);
    DestroyWindow(h);
    SetForegroundWindow(g_wnd);
    value = g_inputResult;
    return g_inputOk;
}

void gotoLine(int line, int col = 1) {
    LONG count = (LONG)SendMessageW(g_edit, EM_GETLINECOUNT, 0, 0);
    line = std::clamp(line, 1, (int)count);
    LONG cp = (LONG)SendMessageW(g_edit, EM_LINEINDEX, line - 1, 0) + std::max(0, col - 1);
    CHARRANGE cr{cp, cp};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessageW(g_edit, EM_SCROLLCARET, 0, 0);
    SetFocus(g_edit);
}

// --- autocompletado ---
LONG g_completeStart = 0;
std::wstring currentWord(LONG& start, std::wstring* owner = nullptr) {
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    LONG line = (LONG)SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax);
    LONG ls = (LONG)SendMessageW(g_edit, EM_LINEINDEX, line, 0);
    std::wstring t = lineText(line).substr(0, (size_t)(sel.cpMax - ls));
    size_t i = t.size();
    auto isId = [](wchar_t c) { return iswalnum(c) || c == L'_' || c > 127; };
    while (i > 0 && isId(t[i - 1])) i--;
    start = ls + (LONG)i;
    if (owner) {
        owner->clear();
        if (i > 0 && t[i - 1] == L'.') {
            size_t j = i - 1;
            while (j > 0 && isId(t[j - 1])) j--;
            *owner = t.substr(j, i - 1 - j);
        }
    }
    return t.substr(i);
}

void hideComplete() { ShowWindow(g_complete, SW_HIDE); }

// En la barra de estado, la firma de lo que está seleccionado en la lista ("Console.print(...)")
std::wstring g_completeOwner;
void showCompleteSignature() {
    int i = (int)SendMessageW(g_complete, LB_GETCURSEL, 0, 0);
    if (i < 0) return;
    wchar_t buf[256];
    SendMessageW(g_complete, LB_GETTEXT, i, (LPARAM)buf);
    std::wstring key = g_completeOwner.empty() ? std::wstring(buf) : g_completeOwner + L"." + buf;
    auto it = g_signatures.find(key);
    SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)(it != g_signatures.end() ? L"  " + it->second : std::wstring(L"  ") + buf).c_str());
}

void showComplete() {
    LONG start;
    std::wstring owner;
    std::wstring prefix = currentWord(start, &owner);
    g_completeStart = start;
    std::vector<std::wstring> cands;
    g_completeOwner.clear();
    if (!owner.empty() && g_members.count(owner)) { cands = g_members[owner]; g_completeOwner = owner; }
    else {
        cands = g_globals;
        // nombres del propio documento
        std::wstring all = editorText();
        std::set<std::wstring> seen(cands.begin(), cands.end());
        std::wstring cur;
        for (wchar_t c : all + L" ") {
            if (iswalnum(c) || c == L'_' || c > 127) cur += c;
            else { if (cur.size() > 2 && !iswdigit(cur[0]) && !seen.count(cur)) { seen.insert(cur); cands.push_back(cur); } cur.clear(); }
        }
    }
    SendMessageW(g_complete, LB_RESETCONTENT, 0, 0);
    int n = 0;
    for (auto& c : cands) {
        if (c.size() < prefix.size() || _wcsnicmp(c.c_str(), prefix.c_str(), prefix.size()) != 0 || c == prefix) continue;
        SendMessageW(g_complete, LB_ADDSTRING, 0, (LPARAM)c.c_str());
        if (++n > 400) break;
    }
    if (n == 0) { hideComplete(); return; }
    SendMessageW(g_complete, LB_SETCURSEL, 0, 0);
    POINTL pt{};
    SendMessageW(g_edit, EM_POSFROMCHAR, (WPARAM)&pt, start);
    POINT p{pt.x, pt.y};
    ClientToScreen(g_edit, &p);
    int lineH = MulDiv(g_fontPt, g_dpi, 72) + S(6);
    int visible = std::min(n, 10);
    int itemH = (int)SendMessageW(g_complete, LB_GETITEMHEIGHT, 0, 0);
    SetWindowPos(g_complete, HWND_TOP, p.x, p.y + lineH, S(280), itemH * visible + S(4), SWP_SHOWWINDOW | SWP_NOACTIVATE);
    showCompleteSignature();
}

void acceptComplete() {
    int i = (int)SendMessageW(g_complete, LB_GETCURSEL, 0, 0);
    if (i < 0) { hideComplete(); return; }
    wchar_t buf[256];
    SendMessageW(g_complete, LB_GETTEXT, i, (LPARAM)buf);
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    CHARRANGE cr{g_completeStart, sel.cpMax};
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
    replaceSel(buf);
    hideComplete();
    std::wstring key = g_completeOwner.empty() ? std::wstring(buf) : g_completeOwner + L"." + buf;
    if (g_signatures.count(key)) SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)(L"  " + g_signatures[key]).c_str());
}

// Teclas especiales dentro del editor
LRESULT CALLBACK editProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    bool completing = IsWindowVisible(g_complete);
    if (msg == WM_KEYDOWN) {
        bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
        if (completing) {
            int cur = (int)SendMessageW(g_complete, LB_GETCURSEL, 0, 0), count = (int)SendMessageW(g_complete, LB_GETCOUNT, 0, 0);
            if (wp == VK_DOWN || wp == VK_UP) {
                cur = std::clamp(cur + (wp == VK_DOWN ? 1 : -1), 0, count - 1);
                SendMessageW(g_complete, LB_SETCURSEL, cur, 0);
                showCompleteSignature();
                return 0;
            }
            if (wp == VK_RETURN || wp == VK_TAB) { acceptComplete(); return 0; }
            if (wp == VK_ESCAPE) { hideComplete(); return 0; }
        }
        if (wp == VK_TAB && !ctrl) {
            CHARRANGE sel;
            SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&sel);
            LONG l0 = (LONG)SendMessageW(h, EM_EXLINEFROMCHAR, 0, sel.cpMin), l1 = (LONG)SendMessageW(h, EM_EXLINEFROMCHAR, 0, sel.cpMax);
            if (shift || l1 > l0) indentLines(!shift);
            else replaceSel(L"    ");
            return 0;
        }
        if (wp == VK_RETURN && !ctrl) {
            // Sangría automática: la misma que la línea actual, y 4 más tras una llave abierta
            CHARRANGE sel;
            SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&sel);
            LONG line = (LONG)SendMessageW(h, EM_EXLINEFROMCHAR, 0, sel.cpMin);
            LONG ls = (LONG)SendMessageW(h, EM_LINEINDEX, line, 0);
            std::wstring before = lineText(line).substr(0, (size_t)std::max(0L, sel.cpMin - ls));
            std::wstring indent;
            for (wchar_t c : before) { if (c == L' ' || c == L'\t') indent += c; else break; }
            std::wstring trimmed = before;
            while (!trimmed.empty() && (trimmed.back() == L' ' || trimmed.back() == L'\t')) trimmed.pop_back();
            if (!trimmed.empty() && (trimmed.back() == L'{' || trimmed.back() == L'[' || trimmed.back() == L'(')) indent += L"    ";
            replaceSel(L"\r" + indent);
            return 0;
        }
        if (ctrl && wp == VK_SPACE) { showComplete(); return 0; }
        if (wp == VK_INSERT && !ctrl && !shift) {
            g_overwrite = !g_overwrite; // RichEdit cambia de modo con Insert: lo mostramos en la barra de estado
            LRESULT r = CallWindowProcW(g_editProc, h, msg, wp, lp);
            updateStatus();
            return r;
        }
    }
    if (msg == WM_CHAR) {
        if (wp == L' ' && GetKeyState(VK_CONTROL) < 0) return 0; // Ctrl+Espacio ya gestionado
        if (wp == L'}') {
            // al cerrar un bloque en una línea con solo espacios, quitar un nivel de sangría
            CHARRANGE sel;
            SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&sel);
            LONG line = (LONG)SendMessageW(h, EM_EXLINEFROMCHAR, 0, sel.cpMin);
            LONG ls = (LONG)SendMessageW(h, EM_LINEINDEX, line, 0);
            std::wstring before = lineText(line).substr(0, (size_t)std::max(0L, sel.cpMin - ls));
            if (before.size() >= 4 && before.find_first_not_of(L' ') == std::wstring::npos) {
                CHARRANGE cr{sel.cpMin - 4, sel.cpMin};
                SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&cr);
                replaceSel(L"}");
                return 0;
            }
        }
        LRESULT r = CallWindowProcW(g_editProc, h, msg, wp, lp);
        if (wp == L'(') showCallSignature();
        if (completing) {
            if (iswalnum((wint_t)wp) || wp == L'_' || wp == L'\b') showComplete();
            else hideComplete();
        } else if (wp == L'.') {
            LONG s;
            std::wstring owner;
            currentWord(s, &owner);
            if (g_members.count(owner)) showComplete();
        }
        return r;
    }
    if (msg == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wp) & MK_CONTROL)) {
        SendMessageW(g_wnd, WM_COMMAND, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? CMD_ZOOMIN : CMD_ZOOMOUT, 0);
        return 0;
    }
    if (msg == WM_KILLFOCUS && (HWND)wp != g_complete) hideComplete();
    return CallWindowProcW(g_editProc, h, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Buscar y reemplazar
// ---------------------------------------------------------------------------

bool findNext(const std::wstring& what, DWORD flags, bool wrap = true) {
    if (what.empty()) return false;
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    FINDTEXTEXW ft{};
    bool down = flags & FR_DOWN;
    ft.chrg = down ? CHARRANGE{sel.cpMax, -1} : CHARRANGE{sel.cpMin, 0};
    ft.lpstrText = what.c_str();
    DWORD f = (flags & (FR_MATCHCASE | FR_WHOLEWORD)) | (down ? FR_DOWN : 0);
    LRESULT r = SendMessageW(g_edit, EM_FINDTEXTEXW, f, (LPARAM)&ft);
    if (r < 0 && wrap) {
        ft.chrg = down ? CHARRANGE{0, -1} : CHARRANGE{(LONG)editorText().size(), 0};
        r = SendMessageW(g_edit, EM_FINDTEXTEXW, f, (LPARAM)&ft);
    }
    if (r < 0) { MessageBeep(MB_ICONINFORMATION); return false; }
    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&ft.chrgText);
    SendMessageW(g_edit, EM_SCROLLCARET, 0, 0);
    return true;
}

void openFind(bool replace) {
    if (g_findDlg) { SetFocus(g_findDlg); return; }
    CHARRANGE sel;
    SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
    if (sel.cpMax > sel.cpMin && sel.cpMax - sel.cpMin < 200) {
        std::wstring w = editorText().substr((size_t)sel.cpMin, (size_t)(sel.cpMax - sel.cpMin));
        if (w.find(L'\r') == std::wstring::npos) wcscpy_s(g_findWhat, w.c_str());
    }
    g_fr = FINDREPLACEW{};
    g_fr.lStructSize = sizeof g_fr;
    g_fr.hwndOwner = g_wnd;
    g_fr.Flags = FR_DOWN;
    g_fr.lpstrFindWhat = g_findWhat;
    g_fr.wFindWhatLen = (WORD)std::size(g_findWhat);
    g_fr.lpstrReplaceWith = g_replaceWith;
    g_fr.wReplaceWithLen = (WORD)std::size(g_replaceWith);
    g_findDlg = replace ? ReplaceTextW(&g_fr) : FindTextW(&g_fr);
}

void onFindMessage(FINDREPLACEW* fr) {
    if (fr->Flags & FR_DIALOGTERM) { g_findDlg = nullptr; return; }
    if (fr->Flags & FR_FINDNEXT) findNext(fr->lpstrFindWhat, fr->Flags);
    if (fr->Flags & FR_REPLACE) {
        CHARRANGE sel;
        SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
        std::wstring cur = editorText().substr((size_t)sel.cpMin, (size_t)(sel.cpMax - sel.cpMin));
        bool match = (fr->Flags & FR_MATCHCASE) ? cur == fr->lpstrFindWhat : _wcsicmp(cur.c_str(), fr->lpstrFindWhat) == 0;
        if (match) replaceSel(fr->lpstrReplaceWith);
        findNext(fr->lpstrFindWhat, fr->Flags | FR_DOWN);
    }
    if (fr->Flags & FR_REPLACEALL) {
        CHARRANGE start{0, 0};
        SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&start);
        int n = 0;
        while (findNext(fr->lpstrFindWhat, (fr->Flags & (FR_MATCHCASE | FR_WHOLEWORD)) | FR_DOWN, false)) {
            replaceSel(fr->lpstrReplaceWith);
            n++;
        }
        wchar_t msg[96];
        wsprintfW(msg, L"  %d reemplazo%s", n, n == 1 ? L"" : L"s");
        SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)msg);
    }
}

// ---------------------------------------------------------------------------
// Menús y barra de herramientas
// ---------------------------------------------------------------------------

void setCheck(int cmd, bool on);

// ---------------------------------------------------------------------------
// Pestañas: cada documento tiene su propio control de texto. Los globales g_edit, g_doc, g_file,
// g_dirty, g_savedText y g_problems son siempre los del documento activo (se guardan al cambiar).
// ---------------------------------------------------------------------------

void showProblems();
void applyFontTo(HWND edit, ITextDocument* tom);

Doc makeEditor() {
    Doc d;
    d.edit = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
                             WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_WANTRETURN,
                             0, 0, 10, 10, g_wnd, (HMENU)(INT_PTR)ID_EDIT, g_inst, nullptr);
    SendMessageW(d.edit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_SCROLL);
    SendMessageW(d.edit, EM_EXLIMITTEXT, 0, 64 * 1024 * 1024);
    SendMessageW(d.edit, EM_SETTEXTMODE, TM_RICHTEXT | TM_MULTILEVELUNDO | TM_MULTICODEPAGE, 0);
    SendMessageW(d.edit, EM_SETUNDOLIMIT, 500, 0);
    SendMessageW(d.edit, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));
    SendMessageW(d.edit, EM_SETMARGINS, EC_LEFTMARGIN, S(6));
    SendMessageW(d.edit, EM_SETTARGETDEVICE, 0, g_wrap ? 0 : 1);
    SendMessageW(d.edit, EM_SETLANGOPTIONS, 0, SendMessageW(d.edit, EM_GETLANGOPTIONS, 0, 0) & ~IMF_AUTOFONT);
    IRichEditOle* ole = nullptr;
    SendMessageW(d.edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    if (ole) {
        static const GUID IID_ITextDocument_ = {0x8CC497C0, 0xA1DF, 0x11CE, {0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D}};
        ole->QueryInterface(IID_ITextDocument_, (void**)&d.tom);
        ole->Release();
    }
    WNDPROC prev = (WNDPROC)SetWindowLongPtrW(d.edit, GWLP_WNDPROC, (LONG_PTR)editProc);
    if (!g_editProc) g_editProc = prev;
    return d;
}

// Guarda en su Doc el estado del documento activo
void storeCurrent() {
    if (g_cur < 0 || g_cur >= (int)g_docs.size()) return;
    Doc& d = g_docs[(size_t)g_cur];
    d.file = g_file;
    d.dirty = g_dirty;
    d.saved = g_savedText;
    d.problems = g_problems;
    d.overwrite = g_overwrite;
}

std::wstring docName(const std::wstring& file) { return file.empty() ? L"sin título.hez" : fs::path(file).filename().wstring(); }

void updateTabLabel(int i) {
    if (!g_tabs || i < 0 || i >= (int)g_docs.size()) return;
    bool dirty = i == g_cur ? g_dirty : g_docs[(size_t)i].dirty;
    std::wstring file = i == g_cur ? g_file : g_docs[(size_t)i].file;
    std::wstring label = (dirty ? L"• " : L"") + docName(file) + L"   ";
    TCITEMW it{};
    it.mask = TCIF_TEXT;
    it.pszText = label.data();
    SendMessageW(g_tabs, TCM_SETITEMW, (WPARAM)i, (LPARAM)&it);
}

void activate(int i) {
    if (i < 0 || i >= (int)g_docs.size()) return;
    if (i != g_cur) {
        storeCurrent();
        if (g_cur >= 0 && g_cur < (int)g_docs.size()) ShowWindow(g_docs[(size_t)g_cur].edit, SW_HIDE);
        g_cur = i;
        Doc& d = g_docs[(size_t)i];
        g_edit = d.edit;
        g_doc = d.tom;
        g_file = d.file;
        g_dirty = d.dirty;
        g_savedText = d.saved;
        g_problems = d.problems;
        g_overwrite = d.overwrite;
        g_lastFirstLine = -1;
        if (!g_file.empty()) SetCurrentDirectoryW(fs::path(g_file).parent_path().c_str());
    }
    SendMessageW(g_tabs, TCM_SETCURSEL, (WPARAM)i, 0);
    ShowWindow(g_edit, SW_SHOW);
    layout();
    showProblems();
    updateTitle();
    updateStatus();
    SetFocus(g_edit);
}

int addTab() {
    storeCurrent();
    Doc d = makeEditor();
    g_docs.push_back(d);
    int i = (int)g_docs.size() - 1;
    std::wstring label = L"sin título.hez   ";
    TCITEMW it{};
    it.mask = TCIF_TEXT;
    it.pszText = label.data();
    SendMessageW(g_tabs, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&it);
    applyFontTo(d.edit, d.tom);
    activate(i);
    return i;
}

bool confirmDiscard();

// Cierra una pestaña (pregunta si tiene cambios). Siempre queda al menos una.
bool closeTab(int i) {
    if (i < 0 || i >= (int)g_docs.size()) return false;
    activate(i);
    if (!confirmDiscard()) return false;
    if (g_doc) g_doc->Release();
    DestroyWindow(g_edit);
    g_docs.erase(g_docs.begin() + i);
    SendMessageW(g_tabs, TCM_DELETEITEM, (WPARAM)i, 0);
    g_cur = -1;
    g_edit = nullptr;
    g_doc = nullptr;
    g_problems.clear();
    if (g_docs.empty()) {
        addTab();
        g_file.clear();
        g_dirty = false;
        g_savedText = editorText();
        updateTitle();
    } else {
        activate(std::min(i, (int)g_docs.size() - 1));
    }
    return true;
}

// Pestaña de documentos: el botón central del ratón cierra la pestaña
WNDPROC g_tabsProc = nullptr;
LRESULT CALLBACK tabsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_MBUTTONUP) {
        TCHITTESTINFO hit{};
        hit.pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int i = (int)SendMessageW(h, TCM_HITTEST, 0, (LPARAM)&hit);
        if (i >= 0) { closeTab(i); return 0; }
    }
    return CallWindowProcW(g_tabsProc, h, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Panel de salida: ejecutar el programa dentro del editor (Mayús+F5)
// ---------------------------------------------------------------------------

enum : UINT { WM_APP_OUTPUT = WM_APP + 20, WM_APP_DONE = WM_APP + 21 };
HANDLE g_runProc = nullptr;
double g_runStart = 0;
std::string g_outCarry; // trozo de un carácter UTF-8 que llegó partido

void setBottomPage(int page) {
    g_bottomPage = page;
    SendMessageW(g_bottomTabs, TCM_SETCURSEL, (WPARAM)page, 0);
    if (!g_showPanel) { g_showPanel = true; setCheck(CMD_PANEL, true); }
    layout();
}

void appendOutput(const std::wstring& w) {
    std::wstring t;
    t.reserve(w.size() + 16);
    for (size_t i = 0; i < w.size(); i++) {
        if (w[i] == L'\n' && (i == 0 || w[i - 1] != L'\r')) t += L"\r\n";
        else t += w[i];
    }
    int len = GetWindowTextLengthW(g_output);
    if (len > 2000000) { SetWindowTextW(g_output, L"(salida anterior recortada)\r\n"); len = GetWindowTextLengthW(g_output); }
    SendMessageW(g_output, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_output, EM_REPLACESEL, FALSE, (LPARAM)t.c_str());
    SendMessageW(g_output, EM_SCROLLCARET, 0, 0);
}

void appendOutputUtf8(const std::string& chunk) {
    std::string s = g_outCarry + chunk;
    g_outCarry.clear();
    // no cortar un carácter UTF-8 por la mitad
    size_t end = s.size(), i = s.size();
    int back = 0;
    while (i > 0 && back < 4 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) { i--; back++; }
    if (i > 0) {
        unsigned char lead = (unsigned char)s[i - 1];
        int need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (need > 1 && back + 1 < need) { end = i - 1; g_outCarry = s.substr(end); }
    }
    appendOutput(widen(s.substr(0, end)));
}

double nowSeconds() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

// Entorno del proceso actual con NO_COLOR=1 (sin códigos de color en el panel)
std::vector<wchar_t> environmentWithNoColor() {
    std::vector<wchar_t> env;
    LPWCH cur = GetEnvironmentStringsW();
    for (LPWCH p = cur; p && *p; p += wcslen(p) + 1) {
        if (_wcsnicmp(p, L"NO_COLOR=", 9) == 0 || _wcsnicmp(p, L"HYPEREZ_COLOR=", 14) == 0) continue;
        env.insert(env.end(), p, p + wcslen(p) + 1);
    }
    if (cur) FreeEnvironmentStringsW(cur);
    const wchar_t* extra = L"NO_COLOR=1";
    env.insert(env.end(), extra, extra + wcslen(extra) + 1);
    env.push_back(L'\0');
    return env;
}

void stopRun() {
    if (g_runProc) TerminateProcess(g_runProc, 1);
}

bool ensureSaved();

void runInPanel() {
    if (!ensureSaved()) return;
    checkProgram();
    if (std::any_of(g_problems.begin(), g_problems.end(), [](const Problem& p) { return p.error; })) {
        setBottomPage(0);
        MessageBeep(MB_ICONWARNING);
        SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)L"  ✗ Corrige los errores para ejecutar");
        return;
    }
    if (g_runProc) {
        MessageBeep(MB_ICONINFORMATION);
        SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)L"  Ya hay un programa en marcha (Ejecutar → Detener)");
        return;
    }
    std::wstring exe = hyperExe();
    if (!fs::exists(exe)) {
        MessageBoxW(g_wnd, L"No se encuentra hyper.exe junto al editor. Reinstala HyperEZ.", L"HyperEZ Editor", MB_ICONERROR);
        return;
    }
    setBottomPage(1);
    SetWindowTextW(g_output, L"");
    g_outCarry.clear();
    appendOutput(L"▶ " + docName(g_file) + L"   (sin teclado: para juegos e input() usa F5)\r\n\r\n");
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{sizeof si};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = wr;
    si.hStdError = wr;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exe + L"\" run \"" + g_file + L"\"";
    std::vector<wchar_t> env = environmentWithNoColor();
    std::wstring cwd = fs::path(g_file).parent_path().wstring();
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, env.data(), cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        CloseHandle(rd);
        appendOutput(L"No se pudo ejecutar hyper.exe\r\n");
        return;
    }
    CloseHandle(pi.hThread);
    g_runProc = pi.hProcess;
    g_runStart = nowSeconds();
    HWND target = g_wnd;
    HANDLE proc = pi.hProcess;
    std::thread([rd, proc, target] {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(rd, buf, sizeof buf, &n, nullptr) && n > 0)
            PostMessageW(target, WM_APP_OUTPUT, 0, (LPARAM)new std::string(buf, n));
        CloseHandle(rd);
        WaitForSingleObject(proc, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(proc, &code);
        PostMessageW(target, WM_APP_DONE, code, 0);
    }).detach();
    SendMessageW(g_toolbar, TB_ENABLEBUTTON, CMD_STOP, TRUE);
}

void runFinished(DWORD code) {
    double secs = nowSeconds() - g_runStart;
    if (!g_outCarry.empty()) { appendOutput(widen(g_outCarry)); g_outCarry.clear(); }
    wchar_t buf[160];
    swprintf(buf, std::size(buf), L"\r\n%ls Terminó %ls (código %lu) en %.2f s\r\n", code == 0 ? L"■" : L"✗",
             code == 0 ? L"bien" : L"con errores", (unsigned long)code, secs);
    std::wstring msg = buf;
    for (auto& c : msg) if (c == L'.') { c = L','; break; }
    appendOutput(msg);
    if (g_runProc) { CloseHandle(g_runProc); g_runProc = nullptr; }
    SendMessageW(g_toolbar, TB_ENABLEBUTTON, CMD_STOP, FALSE);
}

// Doble clic en una línea de la salida con "archivo.hez:12:5" (o "(archivo.hez:12)") lleva a esa línea
WNDPROC g_outputProc = nullptr;
LRESULT CALLBACK outputProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT r = CallWindowProcW(g_outputProc, h, msg, wp, lp);
    if (msg == WM_LBUTTONDBLCLK) {
        DWORD a = 0;
        SendMessageW(h, EM_GETSEL, (WPARAM)&a, 0);
        int line = (int)SendMessageW(h, EM_LINEFROMCHAR, a, 0);
        wchar_t buf[1024];
        *(WORD*)buf = (WORD)(std::size(buf) - 1);
        int n = (int)SendMessageW(h, EM_GETLINE, (WPARAM)line, (LPARAM)buf);
        std::wstring t(buf, (size_t)std::max(0, n));
        for (const wchar_t* ext : {L".hez:", L".hz:"}) {
            size_t p = t.find(ext);
            if (p == std::wstring::npos) continue;
            size_t q = p + wcslen(ext);
            int ln = _wtoi(t.c_str() + q);
            int col = 1;
            size_t c2 = t.find(L':', q);
            if (c2 != std::wstring::npos && c2 < q + 8) col = std::max(1, _wtoi(t.c_str() + c2 + 1));
            if (ln > 0) { gotoLine(ln, col); return r; }
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// Imprimir (Ctrl+P): con los colores del código, el nombre del archivo y el número de página
// ---------------------------------------------------------------------------

void printDocument() {
    PRINTDLGW pd{};
    pd.lStructSize = sizeof pd;
    pd.hwndOwner = g_wnd;
    pd.Flags = PD_RETURNDC | PD_NOSELECTION | PD_NOPAGENUMS | PD_USEDEVMODECOPIESANDCOLLATE;
    if (!PrintDlgW(&pd)) return;
    HDC dc = pd.hDC;
    std::wstring name = docName(g_file);
    DOCINFOW di{sizeof di};
    di.lpszDocName = name.c_str();
    if (StartDocW(dc, &di) > 0) {
        int dpiX = GetDeviceCaps(dc, LOGPIXELSX), dpiY = GetDeviceCaps(dc, LOGPIXELSY);
        int pw = GetDeviceCaps(dc, PHYSICALWIDTH), ph = GetDeviceCaps(dc, PHYSICALHEIGHT);
        int ox = GetDeviceCaps(dc, PHYSICALOFFSETX), oy = GetDeviceCaps(dc, PHYSICALOFFSETY);
        auto twX = [&](int px) { return MulDiv(px, 1440, dpiX); };
        auto twY = [&](int px) { return MulDiv(px, 1440, dpiY); };
        int margin = 1134; // 2 cm
        FORMATRANGE fr{};
        fr.hdc = fr.hdcTarget = dc;
        fr.rcPage = {0, 0, twX(pw), twY(ph)};
        fr.rc = {margin - twX(ox), margin + 360 - twY(oy), twX(pw) - margin - twX(ox), twY(ph) - margin - twY(oy)};
        GETTEXTLENGTHEX gl{GTL_DEFAULT | GTL_NUMCHARS, 1200};
        LONG len = (LONG)SendMessageW(g_edit, EM_GETTEXTLENGTHEX, (WPARAM)&gl, 0);
        fr.chrg = {0, -1};
        HFONT hf = CreateFontW(-MulDiv(9, dpiY, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
        int page = 1;
        do {
            StartPage(dc);
            HGDIOBJ old = SelectObject(dc, hf);
            SetTextColor(dc, RGB(110, 110, 110));
            std::wstring head = name + L"   ·   página " + std::to_wstring(page);
            TextOutW(dc, MulDiv(margin, dpiX, 1440) - ox, MulDiv(margin, dpiY, 1440) - oy, head.c_str(), (int)head.size());
            SelectObject(dc, old);
            LONG next = (LONG)SendMessageW(g_edit, EM_FORMATRANGE, TRUE, (LPARAM)&fr);
            EndPage(dc);
            if (next <= fr.chrg.cpMin) break;
            fr.chrg.cpMin = next;
            page++;
        } while (fr.chrg.cpMin < len);
        SendMessageW(g_edit, EM_FORMATRANGE, FALSE, 0);
        DeleteObject(hf);
        EndDoc(dc);
        SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)(L"  Impreso: " + std::to_wstring(page) + L" página(s)").c_str());
    }
    DeleteDC(dc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
}

// Ver → Fuente: solo fuentes de ancho fijo
void chooseFont() {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(g_fontPt, g_dpi, 72);
    wcsncpy_s(lf.lfFaceName, g_fontFace.c_str(), _TRUNCATE);
    CHOOSEFONTW cf{};
    cf.lStructSize = sizeof cf;
    cf.hwndOwner = g_wnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_NOVERTFONTS;
    if (!ChooseFontW(&cf)) return;
    g_fontFace = lf.lfFaceName;
    g_fontPt = std::clamp(cf.iPointSize / 10, 7, 40);
    applyFont();
    highlight();
}

// ---------------------------------------------------------------------------
// Menús
// ---------------------------------------------------------------------------

void collectExamples() {
    g_examples.clear();
    std::error_code ec;
    fs::path dir = fs::path(installDir()) / L"examples";
    if (!fs::exists(dir, ec)) dir = fs::path(exeDir()) / L".." / L".." / L"examples"; // desde el repositorio (build/)
    if (!fs::exists(dir, ec)) return;
    for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->path().extension() == L".hz" || it->path().extension() == L".hez") g_examples.push_back(it->path().wstring());
    std::sort(g_examples.begin(), g_examples.end());
}

HMENU g_recentMenu = nullptr;

void rebuildMenus() {
    if (!g_recentMenu) return;
    while (GetMenuItemCount(g_recentMenu) > 0) DeleteMenu(g_recentMenu, 0, MF_BYPOSITION);
    if (g_recent.empty()) AppendMenuW(g_recentMenu, MF_STRING | MF_GRAYED, 0, L"(ninguno)");
    for (size_t i = 0; i < g_recent.size(); i++) {
        std::wstring label = L"&" + std::to_wstring((i + 1) % 10) + L"  " + g_recent[i];
        AppendMenuW(g_recentMenu, MF_STRING, CMD_RECENT + i, label.c_str());
    }
}

HMENU buildMenu() {
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu(), edit = CreatePopupMenu(), run = CreatePopupMenu(), view = CreatePopupMenu(), help = CreatePopupMenu();
    HMENU templates = CreatePopupMenu(), examples = CreatePopupMenu();
    g_examplesMenu = examples;
    g_recentMenu = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, CMD_NEW, L"&Nuevo (pestaña nueva)\tCtrl+N");
    for (size_t i = 0; i < std::size(kTemplates); i++) AppendMenuW(templates, MF_STRING, CMD_TEMPLATE + i, kTemplates[i].name);
    AppendMenuW(file, MF_POPUP, (UINT_PTR)templates, L"Nuevo desde &plantilla");
    AppendMenuW(file, MF_STRING, CMD_OPEN, L"&Abrir...\tCtrl+O");
    AppendMenuW(file, MF_POPUP, (UINT_PTR)g_recentMenu, L"Abrir &reciente");
    std::map<std::wstring, HMENU> groups;
    for (size_t i = 0; i < g_examples.size() && i < 900; i++) {
        fs::path p(g_examples[i]);
        std::wstring group = p.parent_path().filename().wstring();
        if (!groups.count(group)) {
            groups[group] = CreatePopupMenu();
            std::wstring label = group;
            if (!label.empty()) label[0] = (wchar_t)towupper(label[0]);
            AppendMenuW(examples, MF_POPUP, (UINT_PTR)groups[group], label.c_str());
        }
        AppendMenuW(groups[group], MF_STRING, CMD_EXAMPLE + i, p.stem().c_str());
    }
    if (g_examples.empty()) AppendMenuW(examples, MF_STRING | MF_GRAYED, 0, L"(no se encontraron ejemplos)");
    AppendMenuW(file, MF_POPUP, (UINT_PTR)examples, L"Abrir &ejemplo");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, CMD_SAVE, L"&Guardar\tCtrl+S");
    AppendMenuW(file, MF_STRING, CMD_SAVEAS, L"Guardar &como...\tCtrl+Mayús+S");
    AppendMenuW(file, MF_STRING, CMD_OPEN_FOLDER, L"Abrir la &carpeta del archivo");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, CMD_CLOSETAB, L"Ce&rrar la pestaña\tCtrl+W");
    AppendMenuW(file, MF_STRING, CMD_CLOSEOTHERS, L"Cerrar las &demás pestañas");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, CMD_PRINT, L"&Imprimir...\tCtrl+P");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, CMD_EXIT, L"&Salir\tAlt+F4");

    AppendMenuW(edit, MF_STRING, CMD_UNDO, L"&Deshacer\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, CMD_REDO, L"&Rehacer\tCtrl+Y");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, CMD_CUT, L"Cor&tar\tCtrl+X");
    AppendMenuW(edit, MF_STRING, CMD_COPY, L"&Copiar\tCtrl+C");
    AppendMenuW(edit, MF_STRING, CMD_PASTE, L"&Pegar\tCtrl+V");
    AppendMenuW(edit, MF_STRING, CMD_DELETE, L"&Eliminar\tSupr");
    AppendMenuW(edit, MF_STRING, CMD_SELECTALL, L"&Seleccionar todo\tCtrl+A");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, CMD_FIND, L"&Buscar...\tCtrl+F");
    AppendMenuW(edit, MF_STRING, CMD_FINDNEXT, L"Buscar &siguiente\tF3");
    AppendMenuW(edit, MF_STRING, CMD_REPLACE, L"Reempla&zar...\tCtrl+H");
    AppendMenuW(edit, MF_STRING, CMD_GOTO, L"&Ir a la línea...\tCtrl+G");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, CMD_COMPLETE, L"Completar &nombre\tCtrl+Espacio");
    AppendMenuW(edit, MF_STRING, CMD_COMMENT, L"Comentar o descomentar\tCtrl+/");
    AppendMenuW(edit, MF_STRING, CMD_DUPLICATE, L"Duplicar la línea\tCtrl+D");
    AppendMenuW(edit, MF_STRING, CMD_FORMAT, L"&Formatear documento\tCtrl+Mayús+F");
    AppendMenuW(edit, MF_STRING, CMD_INDENT, L"Aumentar sangría\tTab");
    AppendMenuW(edit, MF_STRING, CMD_OUTDENT, L"Reducir sangría\tMayús+Tab");

    AppendMenuW(run, MF_STRING, CMD_RUN, L"&Ejecutar en una consola\tF5");
    AppendMenuW(run, MF_STRING, CMD_RUNPANEL, L"Ejecutar en el &panel de salida\tMayús+F5");
    AppendMenuW(run, MF_STRING, CMD_STOP, L"&Detener el programa del panel\tCtrl+Pausa");
    AppendMenuW(run, MF_STRING, CMD_CHECK, L"&Comprobar errores\tF7");
    AppendMenuW(run, MF_STRING, CMD_WATCH, L"Ejecutar y repetir al &guardar\tCtrl+F5");
    AppendMenuW(run, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(run, MF_STRING, CMD_REPL, L"Abrir la consola &interactiva\tF6");
    AppendMenuW(run, MF_STRING, CMD_TESTS, L"Ejecutar las &pruebas de la carpeta");
    AppendMenuW(run, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(run, MF_STRING, CMD_CLEAROUTPUT, L"&Borrar el panel de salida");

    AppendMenuW(view, MF_STRING, CMD_ZOOMIN, L"&Aumentar texto\tCtrl++");
    AppendMenuW(view, MF_STRING, CMD_ZOOMOUT, L"&Reducir texto\tCtrl+-");
    AppendMenuW(view, MF_STRING, CMD_ZOOMRESET, L"&Tamaño normal\tCtrl+0");
    AppendMenuW(view, MF_STRING, CMD_FONT, L"&Fuente...");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING, CMD_NEXTTAB, L"Pestaña &siguiente\tCtrl+Tab");
    AppendMenuW(view, MF_STRING, CMD_PREVTAB, L"Pestaña &anterior\tCtrl+Mayús+Tab");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING | (g_showToolbar ? MF_CHECKED : 0), CMD_TOOLBAR, L"&Barra de herramientas");
    AppendMenuW(view, MF_STRING | (g_showStatus ? MF_CHECKED : 0), CMD_STATUSBAR, L"Barra de &estado");
    AppendMenuW(view, MF_STRING | (g_showNumbers ? MF_CHECKED : 0), CMD_LINENUMBERS, L"&Números de línea");
    AppendMenuW(view, MF_STRING | (g_wrap ? MF_CHECKED : 0), CMD_WRAP, L"A&juste de línea");
    AppendMenuW(view, MF_STRING | (g_showPanel ? MF_CHECKED : 0), CMD_PANEL, L"&Panel de problemas y salida");

    AppendMenuW(help, MF_STRING, CMD_HELP_GUIDE, L"&Guía del lenguaje\tF1");
    AppendMenuW(help, MF_STRING, CMD_HELP_DOCS, L"&Documentación completa");
    AppendMenuW(help, MF_STRING, CMD_HELP_API, L"&Referencia de todas las funciones");
    AppendMenuW(help, MF_STRING, CMD_HELP_KEYS, L"&Atajos de teclado");
    AppendMenuW(help, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(help, MF_STRING, CMD_ABOUT, L"Acerca de &HyperEZ");

    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"&Archivo");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)edit, L"&Editar");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)run, L"E&jecutar");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)view, L"&Ver");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, L"A&yuda");
    rebuildMenus();
    return bar;
}

void setCheck(int cmd, bool on) { CheckMenuItem(GetMenu(g_wnd), cmd, on ? MF_CHECKED : MF_UNCHECKED); }

// Documentación: la versión HTML instalada (docs-html) o, si no está, la carpeta docs
void openDoc(const wchar_t* html, const wchar_t* md) {
    std::error_code ec;
    for (const wchar_t* candidate : {html, md}) {
        fs::path p = fs::path(installDir()) / candidate;
        if (fs::exists(p, ec)) { ShellExecuteW(g_wnd, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return; }
    }
    ShellExecuteW(g_wnd, L"open", widen(std::string(HYPEREZ_REPO_URL) + "/tree/main/docs").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void onCommand(int id) {
    if (id >= CMD_RECENT && id < CMD_RECENT + 10) {
        size_t i = (size_t)(id - CMD_RECENT);
        if (i < g_recent.size()) { std::wstring f = g_recent[i]; openFile(f); }
        return;
    }
    if (id >= CMD_EXAMPLE && id < CMD_EXAMPLE + 1000) {
        size_t i = (size_t)(id - CMD_EXAMPLE);
        if (i < g_examples.size()) openFile(g_examples[i]);
        return;
    }
    if (id >= CMD_TEMPLATE && id < CMD_TEMPLATE + 100) {
        size_t i = (size_t)(id - CMD_TEMPLATE);
        if (i < std::size(kTemplates)) newFile(kTemplates[i].code ? kTemplates[i].code : kWelcome);
        return;
    }
    switch (id) {
        case CMD_NEW: case ID_TB_NEW: newFile(); break;
        case CMD_OPEN: case ID_TB_OPEN: openDialog(); break;
        case CMD_SAVE: case ID_TB_SAVE: save(); break;
        case CMD_SAVEAS: saveAs(); break;
        case CMD_OPEN_FOLDER:
            if (!g_file.empty()) ShellExecuteW(g_wnd, L"open", L"explorer.exe", (L"/select,\"" + g_file + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
            break;
        case CMD_EXIT: PostMessageW(g_wnd, WM_CLOSE, 0, 0); break;
        case CMD_UNDO: SendMessageW(g_edit, EM_UNDO, 0, 0); break;
        case CMD_REDO: SendMessageW(g_edit, EM_REDO, 0, 0); break;
        case CMD_CUT: SendMessageW(g_edit, WM_CUT, 0, 0); break;
        case CMD_COPY: SendMessageW(g_edit, WM_COPY, 0, 0); break;
        case CMD_PASTE: SendMessageW(g_edit, EM_PASTESPECIAL, CF_UNICODETEXT, 0); break;
        case CMD_DELETE: SendMessageW(g_edit, WM_CLEAR, 0, 0); break;
        case CMD_SELECTALL: { CHARRANGE all{0, -1}; SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&all); break; }
        case CMD_FIND: openFind(false); break;
        case CMD_REPLACE: openFind(true); break;
        case CMD_FINDNEXT: if (g_findWhat[0]) findNext(g_findWhat, FR_DOWN | (g_fr.Flags & (FR_MATCHCASE | FR_WHOLEWORD))); else openFind(false); break;
        case CMD_GOTO: {
            std::wstring v;
            CHARRANGE sel;
            SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);
            v = std::to_wstring(SendMessageW(g_edit, EM_EXLINEFROMCHAR, 0, sel.cpMax) + 1);
            if (askText(L"Ir a la línea", L"Número de línea:", v)) gotoLine(_wtoi(v.c_str()));
            break;
        }
        case CMD_COMMENT: toggleComment(); break;
        case CMD_DUPLICATE: duplicateLine(); break;
        case CMD_FORMAT: formatDocument(); break;
        case CMD_INDENT: indentLines(true); break;
        case CMD_OUTDENT: indentLines(false); break;
        case CMD_COMPLETE: SetFocus(g_edit); showComplete(); break;
        case CMD_RUN: case ID_TB_RUN: runProgram(); break;
        case CMD_CHECK: case ID_TB_CHECK:
            checkProgram();
            if (!g_showPanel) { g_showPanel = true; layout(); setCheck(CMD_PANEL, true); }
            break;
        case CMD_WATCH: case ID_TB_WATCH:
            if (ensureSaved()) launch(L"watch \"" + g_file + L"\"", fs::path(g_file).parent_path().wstring());
            break;
        case CMD_REPL: case ID_TB_REPL: {
            std::wstring cwd = g_file.empty() ? fs::current_path().wstring() : fs::path(g_file).parent_path().wstring();
            launch(L"repl", cwd);
            break;
        }
        case CMD_TESTS:
            if (ensureSaved()) launch(L"test", fs::path(g_file).parent_path().wstring(), true);
            break;
        case CMD_ZOOMIN: g_fontPt = std::min(40, g_fontPt + 1); applyFont(); highlight(); break;
        case CMD_ZOOMOUT: g_fontPt = std::max(7, g_fontPt - 1); applyFont(); highlight(); break;
        case CMD_ZOOMRESET: g_fontPt = 11; applyFont(); highlight(); break;
        case CMD_WRAP:
            g_wrap = !g_wrap;
            for (auto& d : g_docs) SendMessageW(d.edit, EM_SETTARGETDEVICE, 0, g_wrap ? 0 : 1);
            setCheck(CMD_WRAP, g_wrap);
            break;
        case CMD_CLOSETAB: closeTab(g_cur); break;
        case CMD_CLOSEOTHERS: {
            storeCurrent();
            HWND keep = g_edit;
            for (int i = (int)g_docs.size() - 1; i >= 0; i--)
                if (g_docs[(size_t)i].edit != keep && !closeTab(i)) break;
            for (size_t i = 0; i < g_docs.size(); i++) if (g_docs[i].edit == keep) activate((int)i);
            break;
        }
        case CMD_NEXTTAB: if (!g_docs.empty()) activate((g_cur + 1) % (int)g_docs.size()); break;
        case CMD_PREVTAB: if (!g_docs.empty()) activate((g_cur + (int)g_docs.size() - 1) % (int)g_docs.size()); break;
        case CMD_PRINT: printDocument(); break;
        case CMD_FONT: chooseFont(); break;
        case CMD_RUNPANEL: runInPanel(); break;
        case CMD_STOP: stopRun(); break;
        case CMD_CLEAROUTPUT: SetWindowTextW(g_output, L""); setBottomPage(1); break;
        case CMD_TOOLBAR: g_showToolbar = !g_showToolbar; setCheck(CMD_TOOLBAR, g_showToolbar); layout(); break;
        case CMD_STATUSBAR: g_showStatus = !g_showStatus; setCheck(CMD_STATUSBAR, g_showStatus); layout(); break;
        case CMD_PANEL: g_showPanel = !g_showPanel; setCheck(CMD_PANEL, g_showPanel); layout(); break;
        case CMD_LINENUMBERS: g_showNumbers = !g_showNumbers; setCheck(CMD_LINENUMBERS, g_showNumbers); layout(); break;
        case CMD_HELP_GUIDE: case ID_TB_HELP: openDoc(L"docs-html\\GUIA.html", L"docs\\GUIA.md"); break;
        case CMD_HELP_DOCS: openDoc(L"docs-html\\index.html", L"docs\\README.md"); break;
        case CMD_HELP_API: openDoc(L"docs-html\\biblioteca\\API.html", L"docs\\biblioteca\\API.md"); break;
        case CMD_EXAMPLESMENU: {
            RECT r{};
            SendMessageW(g_toolbar, TB_GETRECT, CMD_EXAMPLESMENU, (LPARAM)&r);
            MapWindowPoints(g_toolbar, nullptr, (POINT*)&r, 2);
            TrackPopupMenu(g_examplesMenu, TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, g_wnd, nullptr);
            break;
        }
        case CMD_HELP_KEYS:
            MessageBoxW(g_wnd,
                        L"F5\tEjecutar en una consola\nMayús+F5\tEjecutar en el panel de salida\nCtrl+Pausa\tDetener el programa del panel\n"
                        L"Ctrl+F5\tEjecutar y repetir al guardar\nF6\tConsola interactiva\nF7\tComprobar errores\n\n"
                        L"Ctrl+N\tPestaña nueva\nCtrl+W\tCerrar la pestaña\nCtrl+Tab\tPestaña siguiente\nCtrl+P\tImprimir\n"
                        L"Insert\tSobrescribir (SOB) o insertar (INS)\n\n"
                        L"Ctrl+N / O / S\tNuevo / Abrir / Guardar\nCtrl+F / H\tBuscar / Reemplazar\nF3\tBuscar siguiente\n"
                        L"Ctrl+G\tIr a la línea\nCtrl+Espacio\tCompletar nombre\nCtrl+/\tComentar líneas\nCtrl+D\tDuplicar línea\n"
                        L"Ctrl+Mayús+F\tFormatear documento\n"
                        L"Tab / Mayús+Tab\tSangría\nCtrl+rueda, Ctrl++ / Ctrl+-\tTamaño del texto\nF1\tGuía del lenguaje",
                        L"Atajos de teclado", MB_ICONINFORMATION);
            break;
        case CMD_ABOUT: {
            std::wstring msg = L"HyperEZ Editor " + widen(HYPEREZ_VERSION) +
                               L"\n\nUn editor sencillo y clásico para programar en HyperEZ, el lenguaje de programación en español."
                               L"\n\nIncluye el lenguaje completo: el editor usa su mismo analizador, formateador y catálogo de funciones."
                               L"\n\n" + widen(HYPEREZ_REPO_URL) + L"\n\nLicencia MIT © 2026 Adrián Cueva Caravaca";
            MSGBOXPARAMSW mb{sizeof mb};
            mb.hwndOwner = g_wnd;
            mb.hInstance = g_inst;
            mb.lpszText = msg.c_str();
            mb.lpszCaption = L"Acerca de HyperEZ Editor";
            mb.dwStyle = MB_OK | MB_USERICON;
            mb.lpszIcon = MAKEINTRESOURCEW(1);
            MessageBoxIndirectW(&mb);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Ventana principal
// ---------------------------------------------------------------------------

LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_findMsg && g_findMsg) { onFindMessage((FINDREPLACEW*)lp); return 0; }
    switch (msg) {
        case WM_CREATE: {
            g_wnd = h;
            // Barra de herramientas clásica: iconos estándar de Windows y botones con texto para ejecutar
            g_toolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                                        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN |
                                            CCS_NORESIZE,
                                        0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_TOOLBAR, g_inst, nullptr);
            SendMessageW(g_toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
            SendMessageW(g_toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DRAWDDARROWS);
            SendMessageW(g_toolbar, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            SendMessageW(g_toolbar, TB_LOADIMAGES, g_dpi >= 144 ? IDB_STD_LARGE_COLOR : IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
            auto btn = [](int image, int cmd, const wchar_t* text, BYTE style = BTNS_BUTTON) {
                TBBUTTON b{};
                b.iBitmap = image;
                b.idCommand = cmd;
                b.fsState = TBSTATE_ENABLED;
                b.fsStyle = style;
                b.iString = (INT_PTR)text;
                return b;
            };
            TBBUTTON sep{};
            sep.fsStyle = BTNS_SEP;
            TBBUTTON buttons[] = {
                btn(STD_FILENEW, CMD_NEW, L"Nuevo (Ctrl+N)"), btn(STD_FILEOPEN, CMD_OPEN, L"Abrir (Ctrl+O)"),
                btn(STD_FILESAVE, CMD_SAVE, L"Guardar (Ctrl+S)"), btn(STD_PRINT, CMD_PRINT, L"Imprimir (Ctrl+P)"), sep,
                btn(STD_CUT, CMD_CUT, L"Cortar (Ctrl+X)"), btn(STD_COPY, CMD_COPY, L"Copiar (Ctrl+C)"), btn(STD_PASTE, CMD_PASTE, L"Pegar (Ctrl+V)"), sep,
                btn(STD_UNDO, CMD_UNDO, L"Deshacer (Ctrl+Z)"), btn(STD_REDOW, CMD_REDO, L"Rehacer (Ctrl+Y)"), sep,
                btn(STD_FIND, CMD_FIND, L"Buscar (Ctrl+F)"), btn(STD_REPLACE, CMD_REPLACE, L"Reemplazar (Ctrl+H)"), sep,
                btn(I_IMAGENONE, CMD_RUN, L"▶ Ejecutar", BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE),
                btn(I_IMAGENONE, CMD_RUNPANEL, L"▷ En el panel", BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE),
                btn(I_IMAGENONE, CMD_STOP, L"■", BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE),
                btn(I_IMAGENONE, CMD_CHECK, L"✓ Comprobar", BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE),
                btn(I_IMAGENONE, CMD_WATCH, L"⟳ Repetir", BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE),
                btn(I_IMAGENONE, CMD_REPL, L"Consola", BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE), sep,
                btn(I_IMAGENONE, CMD_EXAMPLESMENU, L"Ejemplos ▾", BTNS_BUTTON | BTNS_SHOWTEXT | BTNS_AUTOSIZE),
                btn(STD_HELP, CMD_HELP_GUIDE, L"Guía del lenguaje (F1)"),
            };
            SendMessageW(g_toolbar, TB_ADDBUTTONSW, (WPARAM)std::size(buttons), (LPARAM)buttons);
            SendMessageW(g_toolbar, TB_AUTOSIZE, 0, 0);
            SendMessageW(g_toolbar, TB_ENABLEBUTTON, CMD_STOP, FALSE);
            // Pestañas de documentos
            g_tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FOCUSNEVER, 0, 0, 10, 10, h,
                                     (HMENU)(INT_PTR)ID_TABS, g_inst, nullptr);
            SendMessageW(g_tabs, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            g_tabsProc = (WNDPROC)SetWindowLongPtrW(g_tabs, GWLP_WNDPROC, (LONG_PTR)tabsProc);
            WNDCLASSW gc{};
            gc.lpfnWndProc = gutterProc;
            gc.hInstance = g_inst;
            gc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            gc.lpszClassName = L"HzGutter";
            RegisterClassW(&gc);
            g_gutter = CreateWindowExW(0, L"HzGutter", L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_GUTTER, g_inst, nullptr);
            // Panel inferior: Problemas | Salida
            g_bottomTabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FOCUSNEVER, 0, 0, 10, 10, h,
                                           (HMENU)(INT_PTR)ID_BOTTOMTABS, g_inst, nullptr);
            SendMessageW(g_bottomTabs, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            {
                const wchar_t* names[] = {L"Problemas", L"Salida"};
                for (int i = 0; i < 2; i++) {
                    TCITEMW it{};
                    it.mask = TCIF_TEXT;
                    it.pszText = const_cast<wchar_t*>(names[i]);
                    SendMessageW(g_bottomTabs, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&it);
                }
            }
            g_output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                                           ES_AUTOHSCROLL | ES_NOHIDESEL,
                                       0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_OUTPUT, g_inst, nullptr);
            g_outFont = CreateFontW(-MulDiv(10, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
            SendMessageW(g_output, WM_SETFONT, (WPARAM)g_outFont, TRUE);
            SendMessageW(g_output, EM_SETLIMITTEXT, 0, 0);
            g_outputProc = (WNDPROC)SetWindowLongPtrW(g_output, GWLP_WNDPROC, (LONG_PTR)outputProc);
            g_panel = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                      0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_PANEL, g_inst, nullptr);
            SendMessageW(g_panel, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            // ventana emergente propia: así el editor no la tapa al repintarse
            g_complete = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"LISTBOX", L"",
                                         WS_POPUP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 10, 10, h, nullptr, g_inst, nullptr);
            SendMessageW(g_complete, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            g_status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, h, (HMENU)(INT_PTR)ID_STATUS, g_inst, nullptr);
            SendMessageW(g_status, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
            DragAcceptFiles(h, TRUE);
            SetTimer(h, TIMER_GUTTER, 60, nullptr);
            addTab(); // la primera pestaña
            return 0;
        }
        case WM_SIZE: layout(); return 0;
        case WM_SETFOCUS: SetFocus(g_edit); return 0;
        // Separador entre el código y el panel de abajo: se arrastra para cambiar el alto del panel
        case WM_SETCURSOR:
            if ((HWND)wp == h && LOWORD(lp) == HTCLIENT && g_splitTop >= 0) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(h, &pt);
                if (pt.y >= g_splitTop && pt.y < g_splitBottom) { SetCursor(LoadCursorW(nullptr, IDC_SIZENS)); return TRUE; }
            }
            break;
        case WM_LBUTTONDOWN:
            if (g_splitTop >= 0 && GET_Y_LPARAM(lp) >= g_splitTop && GET_Y_LPARAM(lp) < g_splitBottom) {
                g_splitDrag = true;
                SetCapture(h);
                return 0;
            }
            break;
        case WM_MOUSEMOVE:
            if (g_splitDrag) {
                RECT rc;
                GetClientRect(h, &rc);
                int statusH = 0;
                if (g_status && g_showStatus) { RECT sr; GetWindowRect(g_status, &sr); statusH = sr.bottom - sr.top; }
                int px = (int)rc.bottom - statusH - GET_Y_LPARAM(lp) - S(2); // el ratón queda en medio de la franja
                g_panelHeight = std::clamp(MulDiv(px, 96, g_dpi), 60, 2000);
                layout();
                UpdateWindow(h);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (g_splitDrag) { ReleaseCapture(); return 0; }
            break;
        case WM_CAPTURECHANGED:
            if (g_splitDrag) {
                g_splitDrag = false;
                // si el separador se soltó más allá del límite, guardar el alto que se ve de verdad
                if (g_splitBottom >= 0) {
                    RECT rc;
                    GetClientRect(h, &rc);
                    int statusH = 0;
                    if (g_status && g_showStatus) { RECT sr; GetWindowRect(g_status, &sr); statusH = sr.bottom - sr.top; }
                    g_panelHeight = std::clamp(MulDiv((int)rc.bottom - statusH - g_splitBottom, 96, g_dpi), 60, 2000);
                }
            }
            return 0;
        case WM_TIMER:
            if (wp == TIMER_HIGHLIGHT) { KillTimer(h, TIMER_HIGHLIGHT); highlight(); }
            else if (wp == TIMER_CHECK) { KillTimer(h, TIMER_CHECK); checkProgram(); }
            else if (wp == TIMER_GUTTER) {
                int first = (int)SendMessageW(g_edit, EM_GETFIRSTVISIBLELINE, 0, 0);
                if (first != g_lastFirstLine) { g_lastFirstLine = first; InvalidateRect(g_gutter, nullptr, FALSE); }
            }
            return 0;
        case WM_COMMAND: {
            int id = LOWORD(wp), code = HIWORD(wp);
            if ((HWND)lp == g_edit && code == EN_CHANGE) {
                if (!g_formatting) {
                    bool dirty = editorText() != g_savedText; // deshacer hasta lo guardado quita la marca
                    if (dirty != g_dirty) { g_dirty = dirty; updateTitle(); }
                    SetTimer(h, TIMER_HIGHLIGHT, 150, nullptr);
                    SetTimer(h, TIMER_CHECK, 700, nullptr);
                    if (gutterWidth() != [] { RECT r; GetClientRect(g_gutter, &r); return (int)r.right; }()) layout();
                    InvalidateRect(g_gutter, nullptr, FALSE);
                }
                return 0;
            }
            if ((HWND)lp == g_panel && code == LBN_DBLCLK) {
                int i = (int)SendMessageW(g_panel, LB_GETCURSEL, 0, 0);
                if (i >= 0 && i < (int)g_problems.size() && g_problems[i].line > 0) {
                    CHARRANGE cr{g_problems[i].cpStart, g_problems[i].cpEnd};
                    SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
                    SendMessageW(g_edit, EM_SCROLLCARET, 0, 0);
                    SetFocus(g_edit);
                }
                return 0;
            }
            if ((HWND)lp == g_complete && code == LBN_DBLCLK) { acceptComplete(); SetFocus(g_edit); return 0; }
            onCommand(id);
            return 0;
        }
        case WM_NOTIFY: {
            auto* n = (NMHDR*)lp;
            if (n->hwndFrom == g_edit && n->code == EN_SELCHANGE && !g_formatting) updateStatus();
            if (n->hwndFrom == g_tabs && n->code == TCN_SELCHANGE) activate((int)SendMessageW(g_tabs, TCM_GETCURSEL, 0, 0));
            if (n->hwndFrom == g_bottomTabs && n->code == TCN_SELCHANGE) {
                g_bottomPage = (int)SendMessageW(g_bottomTabs, TCM_GETCURSEL, 0, 0);
                layout();
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC:
            if ((HWND)lp == g_output) { // la salida, en blanco como el editor aunque sea de solo lectura
                SetBkColor((HDC)wp, RGB(255, 255, 255));
                SetTextColor((HDC)wp, RGB(31, 35, 40));
                return (LRESULT)GetStockObject(WHITE_BRUSH);
            }
            break;
        case WM_APP_OUTPUT: {
            auto* chunk = (std::string*)lp;
            appendOutputUtf8(*chunk);
            delete chunk;
            return 0;
        }
        case WM_APP_DONE: runFinished((DWORD)wp); return 0;
        case WM_DROPFILES: {
            UINT count = DragQueryFileW((HDROP)wp, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; i++) {
                wchar_t buf[MAX_PATH * 2];
                if (DragQueryFileW((HDROP)wp, i, buf, (UINT)std::size(buf))) openFile(buf);
            }
            DragFinish((HDROP)wp);
            return 0;
        }
        case WM_CLOSE: {
            storeCurrent();
            for (int i = 0; i < (int)g_docs.size(); i++) {
                if (!g_docs[(size_t)i].dirty) continue;
                activate(i);
                if (!confirmDiscard()) return 0;
                storeCurrent();
            }
            saveSettings();
            DestroyWindow(h);
            return 0;
        }
        case WM_DESTROY:
            stopRun();
            for (auto& d : g_docs) if (d.tom) d.tom->Release();
            g_docs.clear();
            g_doc = nullptr;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    g_inst = inst;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    OleInitialize(nullptr);
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES};
    InitCommonControlsEx(&icc);
    LoadLibraryW(L"Msftedit.dll");
    HDC screen = GetDC(nullptr);
    g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    NONCLIENTMETRICSW ncm{sizeof ncm};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_uiFont = CreateFontIndirectW(&ncm.lfMessageFont);
    g_findMsg = RegisterWindowMessageW(FINDMSGSTRINGW);
    loadSettings();
    buildCatalog();
    collectExamples();

    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, 0);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = L"HyperEZEditor";
    RegisterClassExW(&wc);
    HWND h = CreateWindowExW(0, L"HyperEZEditor", L"HyperEZ Editor", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, S(1000), S(720),
                             nullptr, buildMenu(), inst, nullptr);
    WINDOWPLACEMENT wp{sizeof wp};
    DWORD sz = sizeof wp;
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, L"Placement", RRF_RT_REG_BINARY, nullptr, &wp, &sz) == ERROR_SUCCESS && sz == sizeof wp) {
        wp.showCmd = wp.showCmd == SW_SHOWMINIMIZED ? SW_SHOWNORMAL : wp.showCmd;
        SetWindowPlacement(h, &wp);
    } else {
        ShowWindow(h, show);
    }
    applyFont();
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool opened = false;
    for (int i = 1; i < argc; i++)
        if (fs::exists(argv[i])) { openFile(argv[i]); opened = true; }
    if (!opened) { // primera vez: un programa de bienvenida
        setText(kWelcome);
        LONG end = (LONG)editorText().size();
        CHARRANGE cr{end, end};
        SendMessageW(g_edit, EM_EXSETSEL, 0, (LPARAM)&cr);
    }
    LocalFree(argv);
    UpdateWindow(h);

    ACCEL keys[] = {
        {FCONTROL | FVIRTKEY, 'N', CMD_NEW}, {FCONTROL | FVIRTKEY, 'O', CMD_OPEN}, {FCONTROL | FVIRTKEY, 'S', CMD_SAVE},
        {FCONTROL | FSHIFT | FVIRTKEY, 'S', CMD_SAVEAS}, {FCONTROL | FVIRTKEY, 'F', CMD_FIND}, {FCONTROL | FVIRTKEY, 'H', CMD_REPLACE},
        {FVIRTKEY, VK_F3, CMD_FINDNEXT}, {FCONTROL | FVIRTKEY, 'G', CMD_GOTO}, {FVIRTKEY, VK_F5, CMD_RUN},
        {FCONTROL | FVIRTKEY, VK_F5, CMD_WATCH}, {FVIRTKEY, VK_F6, CMD_REPL}, {FVIRTKEY, VK_F7, CMD_CHECK},
        {FVIRTKEY, VK_F1, CMD_HELP_GUIDE}, {FCONTROL | FVIRTKEY, VK_OEM_2, CMD_COMMENT}, {FCONTROL | FVIRTKEY, VK_DIVIDE, CMD_COMMENT},
        {FCONTROL | FVIRTKEY, 'D', CMD_DUPLICATE}, {FCONTROL | FSHIFT | FVIRTKEY, 'F', CMD_FORMAT},{FCONTROL | FVIRTKEY, VK_OEM_PLUS, CMD_ZOOMIN}, {FCONTROL | FVIRTKEY, VK_ADD, CMD_ZOOMIN},
        {FCONTROL | FVIRTKEY, VK_OEM_MINUS, CMD_ZOOMOUT}, {FCONTROL | FVIRTKEY, VK_SUBTRACT, CMD_ZOOMOUT},
        {FCONTROL | FVIRTKEY, '0', CMD_ZOOMRESET}, {FCONTROL | FVIRTKEY, 'Y', CMD_REDO},
        {FCONTROL | FVIRTKEY, 'W', CMD_CLOSETAB}, {FCONTROL | FVIRTKEY, VK_TAB, CMD_NEXTTAB}, {FCONTROL | FSHIFT | FVIRTKEY, VK_TAB, CMD_PREVTAB},
        {FCONTROL | FVIRTKEY, 'P', CMD_PRINT}, {FSHIFT | FVIRTKEY, VK_F5, CMD_RUNPANEL}, {FCONTROL | FVIRTKEY, VK_CANCEL, CMD_STOP},
        {FCONTROL | FVIRTKEY, VK_PAUSE, CMD_STOP},
    };
    HACCEL accel = CreateAcceleratorTableW(keys, (int)std::size(keys));
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (g_findDlg && IsDialogMessageW(g_findDlg, &m)) continue;
        if (TranslateAcceleratorW(h, accel, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    OleUninitialize();
    return 0;
}
