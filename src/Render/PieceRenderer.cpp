#include "PieceRenderer.h"
#include "ThemeManager.h"

void TexturePieceRenderer::render(Texture2D& texture, Vector2 position, float size, const ThemeManager& themeManager)
{
    // Calculate destination rectangle
    Rectangle destRect = { position.x, position.y, size, size };

    // Draw the texture
    DrawTexturePro(texture, { 0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height) }, destRect, { 0, 0 }, 0.0f, WHITE);
}
