#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <algorithm>
#include <raylib.h>
#include "Render/UITheme.h"
#include "Render/Motion.h"

class MenuComponent;

class MenuItemView {
private:
    Vector2 position;
    Vector2 size;
    bool isHovered = false;
    bool isSelected = false;
    bool isPrimary = false;          // filled accent button (e.g. Submit, Play)
    mutable bool pressStartedHere = false; // left press began on this item
    mutable float hoverAmount = 0.0f; // 0..1, eased toward isHovered over ~150 ms
    bool drawSelectedOutline = true; // false when the owning view draws a sliding indicator instead
    int enterIndex = -1;             // >= 0: fade/slide in on first draw, delayed by index * stagger
    mutable bool entered = false;
    mutable UI::Motion::Tween enterTween;
    static inline float s_globalAlpha = 1.0f; // multiplies everything (nav overlay fade)

public:
    MenuItemView(Vector2 pos, Vector2 sz) : position(pos), size(sz) {
        size.y = std::max(size.y, UI::Space::buttonHeight);
    }

    bool isPointInside(Vector2 point) const;

    void setHovered(bool hovered) { isHovered = hovered; }
    bool getHovered() const { return isHovered; }

    void setSelected(bool selected) { isSelected = selected; }
    bool getSelected() const { return isSelected; }

    void setSelectionOutline(bool on) { drawSelectedOutline = on; }
    // Stagger the entrance of this item (index >= 0); default is no entrance animation
    void setEnterIndex(int index) { enterIndex = index; entered = false; }
    static void setGlobalAlpha(float a) { s_globalAlpha = a; }
    static float globalAlpha() { return s_globalAlpha; }

    void setPrimary(bool primary) { isPrimary = primary; }
    bool getPrimary() const { return isPrimary; }

    void draw(std::shared_ptr<MenuComponent> menuComponent) const;

    Vector2 getPosition() const { return position; }
    Vector2 getSize() const { return size; }

    void setPosition(Vector2 pos) { position = pos; }
    void setSize(Vector2 sz) { size = {sz.x, std::max(sz.y, UI::Space::buttonHeight)}; }
};
