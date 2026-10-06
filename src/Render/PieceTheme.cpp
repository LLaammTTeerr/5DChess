#include "PieceTheme.h"
#include "services/Assets.h"
#include "services/Settings.h"

const PieceTheme* Themes::byName(const std::string& name) {
    if (name == "Classic") return &classic;
    if (name == "Modern") return &modern;
    if (name == "Fantasy") return &fantasy;
    if (name == "Pixel") return &pixel;
    return nullptr;
}

const char* Themes::nameOf(const PieceTheme& theme) {
    for (const char* name : {"Classic", "Modern", "Fantasy", "Pixel"})
        if (std::string(byName(name)->prefix) == theme.prefix) return name;
    return "Modern";
}

void ThemeManager::setTheme(const PieceTheme& theme) {
    _settings.theme = theme;
    _textureCache.clear();
    _grayCache.clear();
}

bool ThemeManager::currentThemeHasBlink() const { return _settings.theme.hasBlink; }

Texture2D& ThemeManager::getPieceTexture(const std::string& pieceName) {
    return _assets.texture(_settings.theme.prefix + pieceName);
}

const PieceTextures& ThemeManager::getPieceTextures(const std::string& pieceName, bool gray) {
    auto& cache = gray ? _grayCache : _textureCache;
    auto it = cache.find(pieceName);
    if (it != cache.end()) return it->second;
    auto lookup = [&](const std::string& name) -> Texture2D* {
        return gray ? &_assets.grayTexture(_settings.theme.prefix + name) : &getPieceTexture(name);
    };
    PieceTextures t;
    t.open = lookup(pieceName);
    if (_settings.theme.hasBlink) t.blink = lookup(pieceName + "_blink");
    return cache.emplace(pieceName, t).first->second;
}
