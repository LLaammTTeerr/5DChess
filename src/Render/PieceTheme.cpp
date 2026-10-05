#include "PieceTheme.h"
#include "ResourceManager.h"

Texture2D& ClassicTheme::getTexture(const std::string& pieceName) {
    ResourceManager& resourceManager = ResourceManager::getInstance();
    return resourceManager.getTexture2D(pieceName + "_0");
}

Texture2D& ModernTheme::getTexture(const std::string& pieceName) {
    ResourceManager& resourceManager = ResourceManager::getInstance();
    return resourceManager.getTexture2D(pieceName + "_1");
}

Texture2D& Modern2Theme::getTexture(const std::string& pieceName) {
    ResourceManager& resourceManager = ResourceManager::getInstance();
    return resourceManager.getTexture2D(pieceName + "_2");
}

Texture2D& PixelTheme::getTexture(const std::string& pieceName) {
    ResourceManager& resourceManager = ResourceManager::getInstance();
    return resourceManager.getTexture2D(pieceName + "_3");
}

void ThemeManager::ensureInitialized() {
    if (!_theme) {
        // Default to ClassicTheme if no theme is set
        _theme = std::make_unique<ClassicTheme>();
    }
}

void ThemeManager::setTheme(std::unique_ptr<IPieceTheme> newTheme) {
    _theme = std::move(newTheme);
    _textureCache.clear();
}

const PieceTextures& ThemeManager::getPieceTextures(const std::string& pieceName) {
    auto it = _textureCache.find(pieceName);
    if (it != _textureCache.end()) return it->second;
    ensureInitialized();
    PieceTextures t;
    t.open = &_theme->getTexture(pieceName);
    if (_theme->hasBlink()) t.blink = &_theme->getTexture(pieceName + "_blink");
    return _textureCache.emplace(pieceName, t).first->second;
}

Texture2D& ThemeManager::getPieceTexture(const std::string& pieceName) {
    ensureInitialized();
    return _theme->getTexture(pieceName);
}

ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}
