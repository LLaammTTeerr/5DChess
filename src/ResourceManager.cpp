#include "ResourceManager.h"
#include "assets/assets.h"
#include <stdexcept>

/* ---------------- Memory loaders ---------------- */

static Texture2D LoadTextureFromMemoryWrapper(
    const unsigned char* data,
    unsigned int size)
{
    Image img = LoadImageFromMemory(".png", data, size);
    if (img.data == nullptr) {
        throw std::runtime_error("Failed to load image from memory");
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    if (tex.id == 0) {
        throw std::runtime_error("Failed to create texture from image");
    }

    return tex;
}

static Font LoadFontFromMemoryWrapper(
    const unsigned char* data,
    unsigned int size)
{
    const int fontSize = 16;

    int codepoints[95];
    for (int i = 0; i < 95; i++) {
        codepoints[i] = 32 + i;
    }

    Font font = LoadFontFromMemory(
        ".ttf",
        data,
        size,
        fontSize,
        codepoints,
        95);

    if (font.baseSize == 0) {
        throw std::runtime_error("Failed to load font from memory");
    }

    return font;
}

/* ---------------- Singleton ---------------- */

ResourceManager& ResourceManager::getInstance()
{
    static ResourceManager instance;
    return instance;
}

/* ---------------- Internal preload ---------------- */

void ResourceManager::_preloadTexture2D(
    const Texture2D& texture,
    const std::string& alias)
{
    _textures[alias] = texture;
}

void ResourceManager::_preloadFont(
    const Font& font,
    const std::string& alias)
{
    _fonts[alias] = font;
}

/* ---------------- Unload ---------------- */

void ResourceManager::_unloadTexture2D(const std::string& alias)
{
    auto it = _textures.find(alias);
    if (it != _textures.end()) {
        UnloadTexture(it->second);
        _textures.erase(it);
    }
}

void ResourceManager::_unloadFont(const std::string& alias)
{
    auto it = _fonts.find(alias);
    if (it != _fonts.end()) {
        UnloadFont(it->second);
        _fonts.erase(it);
    }
}

/* ---------------- Flyweight access ---------------- */

Texture2D& ResourceManager::getTexture2D(const std::string& alias)
{
    auto it = _textures.find(alias);
    if (it == _textures.end()) {
        throw std::runtime_error("Texture alias not found: " + alias);
    }
    return it->second;
}

Font& ResourceManager::getFont(const std::string& alias)
{
    auto it = _fonts.find(alias);
    if (it == _fonts.end()) {
        throw std::runtime_error("Font alias not found: " + alias);
    }
    return it->second;
}

/* ---------------- Lifetime ---------------- */

ResourceManager::~ResourceManager()
{
    for (auto& t : _textures) {
        UnloadTexture(t.second);
    }
    for (auto& f : _fonts) {
        UnloadFont(f.second);
    }
}

/* ---------------- Constructor ---------------- */

ResourceManager::ResourceManager()
{
    // Single images
    _preloadTexture2D(
        LoadTextureFromMemoryWrapper(assets_chess_png, assets_chess_png_len),
        "chess");

    _preloadTexture2D(
        LoadTextureFromMemoryWrapper(assets_5DChess_png, assets_5DChess_png_len),
        "welcomeImage");

    _preloadTexture2D(
        LoadTextureFromMemoryWrapper(assets_ChessBoardNoBound_png, assets_ChessBoardNoBound_png_len),
        "mainChessBoard");

    _preloadTexture2D(
        LoadTextureFromMemoryWrapper(assets_images_EndGame_png, assets_images_EndGame_png_len),
        "endGameImage");

    // ---------------- Pieces (themes) ----------------

    struct PieceAsset {
        const unsigned char* data;
        unsigned int len;
        const char* alias;
    };

    // Theme 0
    PieceAsset theme0_pieces[] = {
        { assets_images_Theme_0_black_bishop_png, assets_images_Theme_0_black_bishop_png_len, "black_bishop_0" },
        { assets_images_Theme_0_black_king_png, assets_images_Theme_0_black_king_png_len, "black_king_0" },
        { assets_images_Theme_0_black_knight_png, assets_images_Theme_0_black_knight_png_len, "black_knight_0" },
        { assets_images_Theme_0_black_pawn_png, assets_images_Theme_0_black_pawn_png_len, "black_pawn_0" },
        { assets_images_Theme_0_black_queen_png, assets_images_Theme_0_black_queen_png_len, "black_queen_0" },
        { assets_images_Theme_0_black_rook_png, assets_images_Theme_0_black_rook_png_len, "black_rook_0" },
        { assets_images_Theme_0_white_bishop_png, assets_images_Theme_0_white_bishop_png_len, "white_bishop_0" },
        { assets_images_Theme_0_white_king_png, assets_images_Theme_0_white_king_png_len, "white_king_0" },
        { assets_images_Theme_0_white_knight_png, assets_images_Theme_0_white_knight_png_len, "white_knight_0" },
        { assets_images_Theme_0_white_pawn_png, assets_images_Theme_0_white_pawn_png_len, "white_pawn_0" },
        { assets_images_Theme_0_white_queen_png, assets_images_Theme_0_white_queen_png_len, "white_queen_0" },
        { assets_images_Theme_0_white_rook_png, assets_images_Theme_0_white_rook_png_len, "white_rook_0" },
    };

    // Theme 1
    PieceAsset theme1_pieces[] = {
        { assets_images_Theme_1_black_bishop_png, assets_images_Theme_1_black_bishop_png_len, "black_bishop_1" },
        { assets_images_Theme_1_black_king_png, assets_images_Theme_1_black_king_png_len, "black_king_1" },
        { assets_images_Theme_1_black_knight_png, assets_images_Theme_1_black_knight_png_len, "black_knight_1" },
        { assets_images_Theme_1_black_pawn_png, assets_images_Theme_1_black_pawn_png_len, "black_pawn_1" },
        { assets_images_Theme_1_black_queen_png, assets_images_Theme_1_black_queen_png_len, "black_queen_1" },
        { assets_images_Theme_1_black_rook_png, assets_images_Theme_1_black_rook_png_len, "black_rook_1" },
        { assets_images_Theme_1_white_bishop_png, assets_images_Theme_1_white_bishop_png_len, "white_bishop_1" },
        { assets_images_Theme_1_white_king_png, assets_images_Theme_1_white_king_png_len, "white_king_1" },
        { assets_images_Theme_1_white_knight_png, assets_images_Theme_1_white_knight_png_len, "white_knight_1" },
        { assets_images_Theme_1_white_pawn_png, assets_images_Theme_1_white_pawn_png_len, "white_pawn_1" },
        { assets_images_Theme_1_white_queen_png, assets_images_Theme_1_white_queen_png_len, "white_queen_1" },
        { assets_images_Theme_1_white_rook_png, assets_images_Theme_1_white_rook_png_len, "white_rook_1" },
    };

    // Theme 2
    PieceAsset theme2_pieces[] = {
        { assets_images_Theme_2_black_bishop_png, assets_images_Theme_2_black_bishop_png_len, "black_bishop_2" },
        { assets_images_Theme_2_black_king_png, assets_images_Theme_2_black_king_png_len, "black_king_2" },
        { assets_images_Theme_2_black_knight_png, assets_images_Theme_2_black_knight_png_len, "black_knight_2" },
        { assets_images_Theme_2_black_pawn_png, assets_images_Theme_2_black_pawn_png_len, "black_pawn_2" },
        { assets_images_Theme_2_black_queen_png, assets_images_Theme_2_black_queen_png_len, "black_queen_2" },
        { assets_images_Theme_2_black_rook_png, assets_images_Theme_2_black_rook_png_len, "black_rook_2" },
        { assets_images_Theme_2_white_bishop_png, assets_images_Theme_2_white_bishop_png_len, "white_bishop_2" },
        { assets_images_Theme_2_white_king_png, assets_images_Theme_2_white_king_png_len, "white_king_2" },
        { assets_images_Theme_2_white_knight_png, assets_images_Theme_2_white_knight_png_len, "white_knight_2" },
        { assets_images_Theme_2_white_pawn_png, assets_images_Theme_2_white_pawn_png_len, "white_pawn_2" },
        { assets_images_Theme_2_white_queen_png, assets_images_Theme_2_white_queen_png_len, "white_queen_2" },
        { assets_images_Theme_2_white_rook_png, assets_images_Theme_2_white_rook_png_len, "white_rook_2" },
    };

    // Loop over arrays to preload
    for (auto& p : theme0_pieces)
        _preloadTexture2D(LoadTextureFromMemoryWrapper(p.data, p.len), p.alias);
    for (auto& p : theme1_pieces)
        _preloadTexture2D(LoadTextureFromMemoryWrapper(p.data, p.len), p.alias);
    for (auto& p : theme2_pieces)
        _preloadTexture2D(LoadTextureFromMemoryWrapper(p.data, p.len), p.alias);

    // ---------------- Fonts ----------------
    _preloadFont(
        LoadFontFromMemoryWrapper(assets_fonts_PublicSans_Regular_ttf, assets_fonts_PublicSans_Regular_ttf_len),
        "public_sans_regular");

    _preloadFont(
        LoadFontFromMemoryWrapper(assets_fonts_PublicSans_Bold_ttf, assets_fonts_PublicSans_Bold_ttf_len),
        "public_sans_bold");

    _preloadFont(
        LoadFontFromMemoryWrapper(assets_fonts_Nunito_VariableFont_wght_ttf, assets_fonts_Nunito_VariableFont_wght_ttf_len),
        "nunito");
}
