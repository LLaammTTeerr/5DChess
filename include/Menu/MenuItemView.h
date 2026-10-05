#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <algorithm>
#include <raylib.h>
#include "Render/UITheme.h"

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

public:
    MenuItemView(Vector2 pos, Vector2 sz) : position(pos), size(sz) {
        size.y = std::max(size.y, UI::Space::buttonHeight);
    }

    bool isPointInside(Vector2 point) const;

    void setHovered(bool hovered) { isHovered = hovered; }
    bool getHovered() const { return isHovered; }

    void setSelected(bool selected) { isSelected = selected; }
    bool getSelected() const { return isSelected; }

    void setPrimary(bool primary) { isPrimary = primary; }
    bool getPrimary() const { return isPrimary; }

    void draw(std::shared_ptr<MenuComponent> menuComponent) const;

    Vector2 getPosition() const { return position; }
    Vector2 getSize() const { return size; }

    void setPosition(Vector2 pos) { position = pos; }
    void setSize(Vector2 sz) { size = {sz.x, std::max(sz.y, UI::Space::buttonHeight)}; }
};
