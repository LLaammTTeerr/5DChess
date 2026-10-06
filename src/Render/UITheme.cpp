#include "Render/UITheme.h"
#include "App.h"
#include "ui/Audit.h"
#include <cmath>
#include <rlgl.h>
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
    if (ui::audit::enabled())
        ui::audit::within("centred text", text, {cx - s.x / 2.0f, y, s.x, s.y}, {0.0f, 0.0f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())});
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

void restoreOpaqueAlpha(int width, int height) {
#ifdef __EMSCRIPTEN__
    rlDrawRenderBatchActive();            // everything drawn so far blends normally
    rlColorMask(false, false, false, true);
    DrawRectangle(0, 0, width, height, ::Color{255, 255, 255, 255}); // alpha 1 over alpha anything: (1 * 1) + dst * (1 - 1)
    rlDrawRenderBatchActive();
    rlColorMask(true, true, true, true);
#else
    (void)width;
    (void)height;
#endif
}

namespace Cursor {
static Kind g_requested = Kind::Default;
void beginFrame() {
    switch (g_requested) {
        case Kind::Hand: SetMouseCursor(MOUSE_CURSOR_POINTING_HAND); break;
        case Kind::Grab: SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL); break;
        case Kind::NotAllowed: SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED); break;
        case Kind::Default: SetMouseCursor(MOUSE_CURSOR_DEFAULT); break;
    }
    g_requested = Kind::Default;
}
void request(Kind kind) {
    if (static_cast<int>(kind) > static_cast<int>(g_requested)) g_requested = kind;
}
}

}
