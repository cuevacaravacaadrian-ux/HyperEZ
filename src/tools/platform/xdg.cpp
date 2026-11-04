// Linux y otros escritorios freedesktop: asociación de .hz/.hez con HyperEZ (hyper register).
// Todo va en la carpeta del usuario (~/.local/share): no necesita permisos de administrador.
#if !defined(_WIN32) && !defined(__APPLE__)
#include "tools/tools.h"

#include <cstdlib>

namespace hz {

static std::string dataHome() {
    if (const char* x = std::getenv("XDG_DATA_HOME"); x && *x) return x;
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.local/share";
}

static void quiet(const std::vector<std::string>& argv) {
    ProcessOptions o;
    runCapture(argv, o); // si la herramienta no existe no pasa nada
}

bool registerFileTypes(const std::string& hyperExe, bool install) {
    std::string share = dataHome();
    std::string mimeDir = share + "/mime", appDir = share + "/applications";
    std::string iconPath = share + "/icons/hicolor/256x256/apps/hyperez.png";
    std::string mimeFile = mimeDir + "/packages/hyperez.xml", desktopFile = appDir + "/hyperez.desktop";
    if (!install) {
        std::remove(mimeFile.c_str());
        std::remove(desktopFile.c_str());
        std::remove(iconPath.c_str());
        quiet({"update-mime-database", mimeDir});
        quiet({"update-desktop-database", appDir});
        std::printf("%s✓%s Quitada la asociación de .hz y .hez\n", color::green(), color::reset());
        return true;
    }
    makeDirs(mimeDir + "/packages");
    makeDirs(appDir);
    bool ok = writeFile(mimeFile,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
        "  <mime-type type=\"text/x-hyperez\">\n"
        "    <comment>Programa HyperEZ</comment>\n"
        "    <comment xml:lang=\"en\">HyperEZ program</comment>\n"
        "    <sub-class-of type=\"text/plain\"/>\n"
        "    <icon name=\"hyperez\"/>\n"
        "    <glob pattern=\"*.hz\"/>\n"
        "    <glob pattern=\"*.hez\"/>\n"
        "  </mime-type>\n"
        "</mime-info>\n");
    ok = writeFile(desktopFile,
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=HyperEZ\n"
        "Comment=Ejecuta programas HyperEZ\n"
        "Exec=\"" + hyperExe + "\" run --pause %f\n"
        "Icon=hyperez\n"
        "Terminal=true\n"
        "MimeType=text/x-hyperez;\n"
        "Categories=Development;\n"
        "NoDisplay=false\n") && ok;
    // Icono: share/hyperez/hyperez.png junto a la instalación (bin/../share/hyperez)
    std::string bundled = pathJoin(pathDir(pathDir(hyperExe)), "share/hyperez/hyperez.png");
    std::string png;
    if (readFile(bundled, png)) {
        makeDirs(pathDir(iconPath));
        writeFile(iconPath, png);
    }
    quiet({"update-mime-database", mimeDir});
    quiet({"update-desktop-database", appDir});
    quiet({"xdg-mime", "default", "hyperez.desktop", "text/x-hyperez"});
    if (ok)
        std::printf("%s✓%s Los archivos .hz y .hez se abren con %s\n", color::green(), color::reset(), hyperExe.c_str());
    else
        std::fprintf(stderr, "%serror:%s no se pudo escribir en %s\n", color::red(), color::reset(), share.c_str());
    return ok;
}

} // namespace hz
#endif
