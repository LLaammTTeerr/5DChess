#include "MenuItemView.h"
#include "MenuComponent.h"
#include <raylib.h>
#include <cmath>

bool MenuItemView::isPointInside(Vector2 point) const {
    return point.x >= position.x && point.x <= position.x + size.x &&
           point.y >= position.y && point.y <= position.y + size.y;
}

static Color lerpColor(Color a, Color b, float t) {
    return {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t)};
}

void MenuItemView::draw(std::shared_ptr<MenuComponent> menuComponent) const {
    const bool enabled = menuComponent->isEnabled();
    const bool hot = isHovered && enabled;
    if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) pressStartedHere = true;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) pressStartedHere = false;
    const bool pressed = hot && pressStartedHere;

    // Ease the hover amount (~150 ms)
    const float step = GetFrameTime() / 0.15f;
    hoverAmount = hot ? std::fmin(1.0f, hoverAmount + step) : std::fmax(0.0f, hoverAmount - step);
    if (hot) UI::Cursor::requestHand();

    Rectangle rect = {position.x, position.y, size.x, size.y};
    Color bg, border, textColor;
    float borderThickness = 1.0f;

    if (!enabled) {
        bg = UI::Color::disabledBg;
        border = UI::Color::disabledBg;
        textColor = UI::Color::disabledText;
    } else if (pressed) {
        bg = isPrimary ? UI::Color::primaryPressed : UI::Color::accentPressed;
        border = bg;
        textColor = UI::Color::onAccent;
    } else if (isPrimary) {
        bg = lerpColor(UI::Color::primary, UI::Color::primaryHover, hoverAmount);
        border = bg;
        textColor = UI::Color::onAccent;
    } else {
        bg = lerpColor(UI::Color::surface, UI::Color::surfaceAlt, hoverAmount);
        border = lerpColor(UI::Color::border, UI::Color::accent, hoverAmount);
        textColor = UI::Color::text;
        if (hoverAmount > 0.0f) borderThickness = 1.0f + hoverAmount;
    }

    DrawRectangleRounded(rect, UI::Space::radius, 8, bg);
    DrawRectangleRoundedLinesEx(rect, UI::Space::radius, 8, borderThickness, border);

    if (isSelected && enabled) {
        // Selected list item: accent outline plus a left bar
        DrawRectangleRoundedLinesEx(rect, UI::Space::radius, 8, UI::Space::outline, UI::Color::selected);
        const float barH = rect.height * 0.5f;
        DrawRectangleRounded({rect.x + 8, rect.y + (rect.height - barH) / 2, 4, barH}, 1.0f, 4, UI::Color::selected);
    }

    const std::string title = menuComponent->getTitle(); // keep alive: getTitle() may return by value
    const char* label = title.c_str();
    ::Font f = UI::Fonts::button();
    const float fs = UI::Font::button;
    Vector2 textSize = MeasureTextEx(f, label, fs, 0.0f);
    // Shrink the label (min 14 px) if it would not fit
    float drawSize = fs;
    const float maxW = rect.width - 2 * UI::Space::md;
    if (textSize.x > maxW) {
        drawSize = std::fmax(static_cast<float>(UI::Font::minimum), fs * maxW / textSize.x);
        textSize = MeasureTextEx(f, label, drawSize, 0.0f);
    }
    DrawTextEx(f, label,
               {std::floor(rect.x + (rect.width - textSize.x) / 2), std::floor(rect.y + (rect.height - textSize.y) / 2)},
               drawSize, 0.0f, textColor);
}
