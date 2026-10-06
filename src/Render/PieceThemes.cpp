#include "Render/PieceThemes.h"
#include <cstring>

const PieceTheme* Themes::byName(const std::string& name) {
    for (const Entry& e : all)
        if (name == e.name) return e.theme;
    return nullptr;
}

const char* Themes::nameOf(const PieceTheme& theme) {
    for (const Entry& e : all)
        if (std::strcmp(e.theme->prefix, theme.prefix) == 0) return e.name;
    return all[0].name;
}
