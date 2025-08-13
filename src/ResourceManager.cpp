#include "ResourceManager.h"
#include <filesystem>

ResourceManager& ResourceManager::getInstance() {
    static ResourceManager instance;
    return instance;
}


void ResourceManager::_preloadTexture2D(const std::string &filename, const std::string &alias) {
    if (_textures.find(filename) != _textures.end()) {
        std::cerr << "Texture already loaded: " << filename << std::endl;
        return;
    }

    Texture2D texture = LoadTexture(filename.c_str());
    if (texture.id == 0) {
        throw std::runtime_error("Failed to load texture: " + filename);
    }

    _textures[filename] = texture;
    _MappingAliasToFilename[alias] = filename;
}

void ResourceManager::_preloadFont(const std::string &filename, const std::string &alias) {
    if (_fonts.find(filename) != _fonts.end()) {
        std::cerr << "Font already loaded: " << filename << std::endl;
        return;
    }

    Font font = LoadFont(filename.c_str());
    if (font.baseSize == 0) {
        throw std::runtime_error("Failed to load font: " + filename);
    }

    _fonts[filename] = font;
    _MappingAliasToFilename[alias] = filename;
}

void ResourceManager::_unloadTexture2D(const std::string &alias) {
    auto it = _MappingAliasToFilename.find(alias);
    if (it != _MappingAliasToFilename.end()) {
        const std::string &filename = it->second;
        auto texIt = _textures.find(filename);
        if (texIt != _textures.end()) {
            UnloadTexture(texIt->second);
            _textures.erase(texIt);
        }
        _MappingAliasToFilename.erase(it);
    } else {
        std::cerr << "Texture alias not found: " << alias << std::endl;
    }
}


void ResourceManager::_unloadFont(const std::string &alias) {
    auto it = _MappingAliasToFilename.find(alias);
    if (it != _MappingAliasToFilename.end()) {
        const std::string &filename = it->second;
        auto fontIt = _fonts.find(filename);
        if (fontIt != _fonts.end()) {
            UnloadFont(fontIt->second);
            _fonts.erase(fontIt);
        }
        _MappingAliasToFilename.erase(it);
    } else {
        std::cerr << "Font alias not found: " << alias << std::endl;
    }
}


// flyweight pattern implementation
Texture2D& ResourceManager::getTexture2D(const std::string &alias) {
    auto it = _MappingAliasToFilename.find(alias);
    if (it != _MappingAliasToFilename.end()) {
        const std::string &filename = it->second;
        auto texIt = _textures.find(filename);
        if (texIt != _textures.end()) {
            return texIt->second;
        } else {
            throw std::runtime_error("Texture not found: " + filename);
        }
    } else {
        throw std::runtime_error("Texture alias not found: " + alias);
    }
}

Font& ResourceManager::getFont(const std::string &alias) {
    auto it = _MappingAliasToFilename.find(alias);
    if (it != _MappingAliasToFilename.end()) {
        const std::string &filename = it->second;
        auto fontIt = _fonts.find(filename);
        if (fontIt != _fonts.end()) {
            return fontIt->second;
        } else {
            throw std::runtime_error("Font not found: " + filename);
        }
    } else {
        throw std::runtime_error("Font alias not found: " + alias);
    }
}

ResourceManager::~ResourceManager() {
    // Unload all textures and fonts
    for (const auto &pair : _textures) {
        UnloadTexture(pair.second);
    }
    _textures.clear();

    for (const auto &pair : _fonts) {
        UnloadFont(pair.second);
    }
    _fonts.clear();

    _MappingAliasToFilename.clear();
}

ResourceManager::ResourceManager() {    
    _preloadTexture2D("assets/chess.png", "chess");

    /* Theme 0 */
    _preloadTexture2D("assets/images/Theme_0/black_bishop.png", "black_bishop_0");
    _preloadTexture2D("assets/images/Theme_0/black_king.png", "black_king_0");
    _preloadTexture2D("assets/images/Theme_0/black_knight.png", "black_knight_0");
    _preloadTexture2D("assets/images/Theme_0/black_pawn.png", "black_pawn_0");
    _preloadTexture2D("assets/images/Theme_0/black_queen.png", "black_queen_0");
    _preloadTexture2D("assets/images/Theme_0/black_rook.png", "black_rook_0");
    _preloadTexture2D("assets/images/Theme_0/white_bishop.png", "white_bishop_0");
    _preloadTexture2D("assets/images/Theme_0/white_king.png", "white_king_0");
    _preloadTexture2D("assets/images/Theme_0/white_knight.png", "white_knight_0");
    _preloadTexture2D("assets/images/Theme_0/white_pawn.png", "white_pawn_0");
    _preloadTexture2D("assets/images/Theme_0/white_queen.png", "white_queen_0");
    _preloadTexture2D("assets/images/Theme_0/white_rook.png", "white_rook_0");

    /* Theme 1 */
    _preloadTexture2D("assets/images/Theme_1/black_bishop.png", "black_bishop_1");
    _preloadTexture2D("assets/images/Theme_1/black_king.png", "black_king_1");
    _preloadTexture2D("assets/images/Theme_1/black_knight.png", "black_knight_1");
    _preloadTexture2D("assets/images/Theme_1/black_pawn.png", "black_pawn_1");
    _preloadTexture2D("assets/images/Theme_1/black_queen.png", "black_queen_1");
    _preloadTexture2D("assets/images/Theme_1/black_rook.png", "black_rook_1");
    _preloadTexture2D("assets/images/Theme_1/white_bishop.png", "white_bishop_1");
    _preloadTexture2D("assets/images/Theme_1/white_king.png", "white_king_1");
    _preloadTexture2D("assets/images/Theme_1/white_knight.png", "white_knight_1");
    _preloadTexture2D("assets/images/Theme_1/white_pawn.png", "white_pawn_1");
    _preloadTexture2D("assets/images/Theme_1/white_queen.png", "white_queen_1");
    _preloadTexture2D("assets/images/Theme_1/white_rook.png", "white_rook_1");  

    /* Theme 2 */
    _preloadTexture2D("assets/images/Theme_2/black_bishop.png", "black_bishop_2");
    _preloadTexture2D("assets/images/Theme_2/black_king.png", "black_king_2");
    _preloadTexture2D("assets/images/Theme_2/black_knight.png", "black_knight_2");
    _preloadTexture2D("assets/images/Theme_2/black_pawn.png", "black_pawn_2");
    _preloadTexture2D("assets/images/Theme_2/black_queen.png", "black_queen_2");
    _preloadTexture2D("assets/images/Theme_2/black_rook.png", "black_rook_2");
    _preloadTexture2D("assets/images/Theme_2/white_bishop.png", "white_bishop_2");
    _preloadTexture2D("assets/images/Theme_2/white_king.png", "white_king_2");
    _preloadTexture2D("assets/images/Theme_2/white_knight.png", "white_knight_2");
    _preloadTexture2D("assets/images/Theme_2/white_pawn.png", "white_pawn_2");
    _preloadTexture2D("assets/images/Theme_2/white_queen.png", "white_queen_2");
    _preloadTexture2D("assets/images/Theme_2/white_rook.png", "white_rook_2");


    _preloadFont("assets/fonts/PublicSans-Regular.ttf", "public_sans_regular");
    _preloadFont("assets/fonts/PublicSans-Bold.ttf", "public_sans_bold");
    _preloadFont("assets/fonts/Nunito-VariableFont_wght.ttf", "nunito");


    _preloadTexture2D("assets/5DChess.png", "welcomeImage");
}
