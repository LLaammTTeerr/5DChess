#pragma once
#include <raylib.h>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

/* Interface for PieceTheme */
class IPieceTheme  {
public:
  virtual Texture2D& getTexture(const std::string& pieceName) = 0;
  /// True when "<piece>_blink" textures (eyes closed) exist, e.g. "white_pawn_blink"
  virtual bool hasBlink() const { return false; }
  virtual ~IPieceTheme() = default;
};

class ClassicTheme : public IPieceTheme {
public:
  Texture2D& getTexture(const std::string& pieceName) override;
};

class ModernTheme : public IPieceTheme {
public:
  Texture2D& getTexture(const std::string& pieceName) override;
};

class Modern2Theme : public IPieceTheme {
public:
  Texture2D& getTexture(const std::string& pieceName) override;
};

/* Original pixel-art creature pieces (assets/images/Theme_3) */
class PixelTheme : public IPieceTheme {
public:
  Texture2D& getTexture(const std::string& pieceName) override;
  bool hasBlink() const override { return true; }
};

/// Resolved textures of one piece in the current theme (cached; pointers stay valid until the theme changes)
struct PieceTextures {
  Texture2D* open = nullptr;
  Texture2D* blink = nullptr; // eyes closed; nullptr when the theme has no blink frame
};

// singleton class to manage themes
class ThemeManager {
public:
  static ThemeManager& getInstance();
  void setTheme(std::unique_ptr<IPieceTheme> newTheme);
  Texture2D& getPieceTexture(const std::string& pieceName);
  /// Cached lookup for the per-frame hot path (no string building after the first call per piece)
  const PieceTextures& getPieceTextures(const std::string& pieceName);
  bool isPixelTheme() { return currentThemeHasBlink(); }
  bool currentThemeHasBlink() { ensureInitialized(); return _theme->hasBlink(); }

  ThemeManager(const ThemeManager&) = delete;
  ThemeManager(ThemeManager&&) = delete;
  ThemeManager& operator=(const ThemeManager&) = delete;
  ThemeManager& operator=(ThemeManager&&) = delete;
private:
  ThemeManager() = default;
  ~ThemeManager() = default;
  std::unique_ptr<IPieceTheme> _theme;
  std::unordered_map<std::string, PieceTextures> _textureCache;
  void ensureInitialized();
};
