#pragma once
#include <raylib.h>
#include <iostream>

/* Interface for PieceTheme */
class IPieceTheme  {
public:
  virtual Texture2D& getTexture(const std::string& pieceName) = 0;
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


// singleton class to manage themes
class ThemeManager {
public:
  static ThemeManager& getInstance();
  void setTheme(std::unique_ptr<IPieceTheme> newTheme);
  Texture2D& getPieceTexture(const std::string& pieceName);

  ThemeManager(const ThemeManager&) = delete;
  ThemeManager& operator=(const ThemeManager&) = delete;
private:
  std::unique_ptr<IPieceTheme> _theme;
  void ensureInitialized();
};
