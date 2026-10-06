#include "Render/PieceThemes.h"
#include <cstring>

const PieceTheme* Themes::byName(const std::string& name) {
    if (name == "Pixel") return &pixel;
    if (name == "Medieval") return &medieval;
    if (name == "Bauhaus") return &bauhaus;
    if (name == "Neon") return &neon;
    if (name == "Origami") return &origami;
    if (name == "Ink") return &ink;
    return nullptr;
}

const char* Themes::nameOf(const PieceTheme& theme) {
    for (const char* name : names)
        if (std::strcmp(byName(name)->prefix, theme.prefix) == 0) return name;
    return "Pixel";
}
