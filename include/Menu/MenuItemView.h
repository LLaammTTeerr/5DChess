#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <raylib.h>

class MenuComponent;

class MenuItemView {
private:
    Vector2 position;
    Vector2 size;
    bool isHovered = false;
    bool isSelected = false;

    // Background colors
    Color normalBackgroundColor = {251, 243, 213, 255};// rgb(251, 243, 213)
    Color hoverBackgroundColor = {214, 218, 200, 255}; // rgb(214, 218, 200) 
    Color selectedBackgroundColor = {100, 149, 237, 255}; // Cornflower blue for selected
    Color disabledBackgroundColor = {156, 175, 170, 255}; // rgb(156, 175, 170)

    // Text colors
    Color normalTextColor = {0, 0, 0, 255}; // BLACK
    Color hoverTextColor = {255, 255, 255, 255}; // WHITE
    Color selectedTextColor = {255, 255, 255, 255}; // WHITE for selected
    Color disabledTextColor = {169, 169, 169, 255}; // DARKGRAY


    Font font = GetFontDefault(); // Default font, can be set to a custom one
    int fontSize = 20;
    float fontSpacing = 1.0f; // Default spacing, can be adjusted

public:
    MenuItemView(Vector2 pos, Vector2 size) : position(pos), size(size) {}    
    
    bool isPointInside(Vector2 point) const;
    
    void setHovered(bool hovered) { isHovered = hovered; }
    bool getHovered() const { return isHovered; }
    
    void setSelected(bool selected) { isSelected = selected; }
    bool getSelected() const { return isSelected; }

    void draw(std::shared_ptr<MenuComponent> menuComponent) const;

    Vector2 getPosition() const { return position; }
    Vector2 getSize() const { return size; }

    void setPosition(Vector2 pos) { position = pos; }
    void setSize(Vector2 sz) { size = sz; }

    void setNormalBackgroundColor(Color color) { normalBackgroundColor = color; }
    void setHoverBackgroundColor(Color color) { hoverBackgroundColor = color; }
    void setSelectedBackgroundColor(Color color) { selectedBackgroundColor = color; }
    void setDisabledBackgroundColor(Color color) { disabledBackgroundColor = color; }
    
    void setNormalTextColor(Color color) { normalTextColor = color; }
    void setHoverTextColor(Color color) { hoverTextColor = color; }
    void setSelectedTextColor(Color color) { selectedTextColor = color; }
    void setDisabledTextColor(Color color) { disabledTextColor = color; }
    
    void setFont(Font f) { font = f; }
    void setFontSize(int size) { fontSize = size; }
    void setFontSpacing(float spacing) { fontSpacing = spacing; }
};
