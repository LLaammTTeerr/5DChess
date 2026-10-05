#include "Render/UITheme.h"
#include "App.h"
#include <cmath>
#include <vector>

namespace UI {

namespace Fonts {
// Display-size fonts, loaded lazily and cached by Assets (so they are unloaded with the App).
::Font title()    { return App::current().assets.font("ui.montserrat_bold", Font::title); }
::Font section()  { return App::current().assets.font("ui.montserrat_bold", Font::section); }
::Font button()   { return App::current().assets.font("ui.public_sans_bold", Font::button); }
::Font body()     { return App::current().assets.font("ui.public_sans", Font::body); }
::Font mono()     { return App::current().assets.font("ui.mono", Font::mono); }
::Font hero()     { return App::current().assets.font("ui.montserrat_bold", Font::hero); }
::Font subtitle() { return App::current().assets.font("ui.public_sans", Font::subtitle); }
}

Vector2 drawTextCentered(::Font font, const char* text, float cx, float y, float size, ::Color color) {
    Vector2 s = MeasureTextEx(font, text, size, 0.0f);
    DrawTextEx(font, text, {std::floor(cx - s.x / 2.0f), std::floor(y)}, size, 0.0f, color);
    return s;
}

void drawSceneTitle(const char* text, float y) {
    drawTextCentered(Fonts::title(), text, GetScreenWidth() / 2.0f, y, Font::title, Color::text);
}

void drawRoundedPanel(Rectangle r, ::Color fill, ::Color border, float borderThickness) {
    DrawRectangleRounded(r, Space::radius, 8, fill);
    DrawRectangleRoundedLinesEx(r, Space::radius, 8, borderThickness, border);
}

void drawCard(Rectangle r) {
    DrawRectangleRounded({r.x + 4, r.y + 6, r.width, r.height}, 0.08f, 8, Color::shadow);
    DrawRectangleRounded(r, 0.08f, 8, Color::surface);
    DrawRectangleRoundedLinesEx(r, 0.08f, 8, 1.0f, Color::border);
}

namespace Cursor {
static bool g_requested = false;
void beginFrame() {
    SetMouseCursor(g_requested ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    g_requested = false;
}
void requestHand() { g_requested = true; }
}

}
