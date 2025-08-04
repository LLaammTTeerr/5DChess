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

void ThemeManager::ensureInitialized() {
    if (!_theme) {
        // Default to ClassicTheme if no theme is set
        _theme = std::make_unique<ClassicTheme>();
    }
}

void ThemeManager::setTheme(std::unique_ptr<IPieceTheme> newTheme) {
    _theme = std::move(newTheme);
}

Texture2D& ThemeManager::getPieceTexture(const std::string& pieceName) {
    ensureInitialized();
    return _theme->getTexture(pieceName);
}

ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}
