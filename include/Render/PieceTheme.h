#pragma once
#include <raylib.h>
#include <string>
#include <unordered_map>

class Assets;
struct Settings;

// A piece theme is just the asset-id prefix of its textures ("piece.pixel." + "white_pawn"), plus whether it
// ships eyes-closed blink frames ("piece.pixel.white_pawn_blink").
struct PieceTheme {
  const char* prefix;
  bool hasBlink;
};
// (tests/view_test.cpp keeps a hard-coded copy of these prefixes to check the manifest; update it when adding a theme.)
namespace Themes {
inline constexpr PieceTheme classic{"piece.classic.", false};
inline constexpr PieceTheme modern{"piece.modern.", false};
inline constexpr PieceTheme fantasy{"piece.fantasy.", false};
inline constexpr PieceTheme pixel{"piece.pixel.", true}; // original pixel-art creatures
inline constexpr PieceTheme medieval{"piece.medieval.", false}; // original heraldic ivory and walnut pieces
// By Settings / theme_preview name ("Classic", "Modern", "Fantasy", "Pixel", "Medieval"); nullptr when unknown.
const PieceTheme* byName(const std::string& name);
// The name byName() finds the theme by ("Modern" for an unknown theme).
const char* nameOf(const PieceTheme& theme);
}

/// Resolved textures of one piece in the current theme (cached; pointers stay valid until the theme changes)
struct PieceTextures {
  Texture2D* open = nullptr;
  Texture2D* blink = nullptr; // eyes closed; nullptr when the theme has no blink frame
};

// Resolves piece textures for Settings::theme out of Assets. App owns one.
class ThemeManager {
public:
  ThemeManager(Assets& assets, Settings& settings) : _assets(assets), _settings(settings) {}
  void setTheme(const PieceTheme& theme);
  Texture2D& getPieceTexture(const std::string& pieceName);
  /// Cached lookup for the per-frame hot path (no string building after the first call per piece)
  const PieceTextures& getPieceTextures(const std::string& pieceName, bool gray = false);
  bool currentThemeHasBlink() const;

private:
  Assets& _assets;
  Settings& _settings;
  std::unordered_map<std::string, PieceTextures> _textureCache, _grayCache;
};
