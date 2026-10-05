#include "Render/UITheme.h"
#include <cmath>
#include <vector>

namespace UI {

namespace {
::Font loadSized(const char* path, int size) {
    // ASCII plus the middle dot used in HUD labels
    std::vector<int> cps;
    for (int c = 32; c < 127; ++c) cps.push_back(c);
    cps.push_back(0x00B7);
    ::Font f = LoadFontEx(path, size, cps.data(), static_cast<int>(cps.size()));
    if (f.texture.id == 0) return GetFontDefault();
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return f;
}
}

namespace Fonts {
namespace {
struct Cache { ::Font title{}, section{}, button{}, body{}, mono{}, hero{}, subtitle{};
               bool t=false, s=false, b=false, o=false, m=false, h=false, u=false; };
Cache g;
void release(::Font& f, bool& loaded) {
    if (loaded && f.texture.id != GetFontDefault().texture.id) UnloadFont(f);
    f = ::Font{};
    loaded = false;
}
}
::Font title()   { if (!g.t) { g.title   = loadSized("assets/fonts/Montserrat-Bold.ttf", Font::title);    g.t = true; } return g.title; }
::Font section() { if (!g.s) { g.section = loadSized("assets/fonts/Montserrat-Bold.ttf", Font::section);  g.s = true; } return g.section; }
::Font button()  { if (!g.b) { g.button  = loadSized("assets/fonts/PublicSans-Bold.ttf", Font::button);   g.b = true; } return g.button; }
::Font body()    { if (!g.o) { g.body    = loadSized("assets/fonts/PublicSans-Regular.ttf", Font::body);  g.o = true; } return g.body; }
::Font mono()    { if (!g.m) { g.mono    = loadSized("assets/fonts/intelone-mono-font-family-regular.ttf", Font::mono); g.m = true; } return g.mono; }
::Font hero()     { if (!g.h) { g.hero     = loadSized("assets/fonts/Montserrat-Bold.ttf", Font::hero);          g.h = true; } return g.hero; }
::Font subtitle() { if (!g.u) { g.subtitle = loadSized("assets/fonts/PublicSans-Regular.ttf", Font::subtitle);   g.u = true; } return g.subtitle; }
void unloadAll() {
    release(g.title, g.t); release(g.section, g.s); release(g.button, g.b);
    release(g.body, g.o);  release(g.mono, g.m);
    release(g.hero, g.h);  release(g.subtitle, g.u);
}
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
