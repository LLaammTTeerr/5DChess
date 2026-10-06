#pragma once
#include <raylib.h>
#include "Render/PieceThemes.h"
#include <string>
#include <unordered_map>

class Assets;
struct Settings;

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
