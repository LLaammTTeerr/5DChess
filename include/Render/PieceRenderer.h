#pragma once
#include <iostream>
#include <raylib.h>
#include <memory>

class ThemeManager;

class TexturePieceRenderer {
public:
  static void render(Texture2D& texture, Vector2 position, float size, const ThemeManager& themeManager);
};
