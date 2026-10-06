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

void ThemeManager::setTheme(const PieceTheme& theme) {
    _settings.theme = theme;
    _textureCache.clear();
}

bool ThemeManager::currentThemeHasBlink() const { return _settings.theme.hasBlink; }

Texture2D& ThemeManager::getPieceTexture(const std::string& pieceName) {
    return _assets.texture(_settings.theme.prefix + pieceName);
}

const PieceTextures& ThemeManager::getPieceTextures(const std::string& pieceName) {
    auto it = _textureCache.find(pieceName);
    if (it != _textureCache.end()) return it->second;
    PieceTextures t;
    t.open = &getPieceTexture(pieceName);
    if (_settings.theme.hasBlink) t.blink = &getPieceTexture(pieceName + "_blink");
    return _textureCache.emplace(pieceName, t).first->second;
}
