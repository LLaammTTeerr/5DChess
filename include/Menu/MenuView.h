#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <raylib.h>
#include "gameState.h"

// Forward declarations
class MenuComponent;
// Visual representation of a menu item
class MenuItemView {
private:
    Vector2 position;
    Vector2 size;
    bool isHovered = false;

    // Background colors
    Color normalBackgroundColor = DARKGRAY;
    Color hoverBackgroundColor = LIGHTGRAY;
    Color disabledBackgroundColor = GRAY;

    // Text colors
    Color normalTextColor = BLACK;
    Color hoverTextColor = WHITE;
    Color disabledTextColor = DARKGRAY;


    Font font = GetFontDefault(); // Default font, can be set to a custom one
    int fontSize = 20;
    float fontSpacing = 1.0f; // Default spacing, can be adjusted

public:
    MenuItemView(Vector2 pos, Vector2 size) : position(pos), size(size) {}    
    
    bool isPointInside(Vector2 point) const;
    
    void setHovered(bool hovered) { isHovered = hovered; }
    bool getHovered() const { return isHovered; }
    
    void draw(const MenuComponent& menuComponent) const;
    
    Vector2 getPosition() const { return position; }
    Vector2 getSize() const { return size; }

    void setPosition(Vector2 pos) { position = pos; }
    void setSize(Vector2 sz) { size = sz; }

    void setNormalBackgroundColor(Color color) { normalBackgroundColor = color; }
    void setHoverBackgroundColor(Color color) { hoverBackgroundColor = color; }
    void setDisabledBackgroundColor(Color color) { disabledBackgroundColor = color; }
    
    void setNormalTextColor(Color color) { normalTextColor = color; }
    void setHoverTextColor(Color color) { hoverTextColor = color; }
    void setDisabledTextColor(Color color) { disabledTextColor = color; }
    
    void setFont(Font f) { font = f; }
    void setFontSize(int size) { fontSize = size; }
    void setFontSpacing(float spacing) { fontSpacing = spacing; }
};


// abstract class for menu view
class IMenuView {
protected:
  std::vector<std::shared_ptr<MenuItemView>> _itemViews;
  
  // Common styling properties
  Color backgroundColor = RAYWHITE;
  Color titleColor = DARKBLUE;
  Font titleFont = GetFontDefault();
  int titleFontSize = 40;
  
public:
  virtual ~IMenuView() = default;
  virtual void draw(const MenuComponent& menuModel) const = 0;
  virtual void createItemViews(const MenuComponent& menuModel) = 0;
  
  void drawBackground() const { ClearBackground(backgroundColor); }
  // virtual void drawTitle(const std::string& title) const;
  // virtual void drawStatusInfo(const GameStateModel& gameState) const;

  std::vector<std::shared_ptr<MenuItemView>>& getItemViews() { return _itemViews; }
  const std::vector<std::shared_ptr<MenuItemView>>& getItemViews() const { return _itemViews; }

  // Common styling setters
  void setBackgroundColor(Color color) { backgroundColor = color; }
  void setTitleColor(Color color) { titleColor = color; }
  void setTitleFont(Font font) { titleFont = font; }
  void setTitleFontSize(int size) { titleFontSize = size; }


};

// Concrete implementation of a menu view
class ButtonMenuView : public IMenuView {
public:
  ~ButtonMenuView() = default;

  void createItemViews(const MenuComponent& menuModel) override;
  void draw(const MenuComponent& menuModel) const override; // { /* Draw button menu */ }
  // void drawTitle(const std::string& title) const override;
};

// class ListMenuView : public IMenuView {
// public:
//   void draw(const MenuComponent& menuModel) const override; // { /* Draw list menu */ }
// };
