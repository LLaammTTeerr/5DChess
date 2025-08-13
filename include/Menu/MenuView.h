#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <raylib.h>
#include "gameState.h"
#include "MenuItemView.h"

// Forward declarations
class MenuComponent;
class GameStateModel;
class GameState;
// Visual representation of a menu item


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
  virtual void draw(std::shared_ptr<MenuComponent> menuModel) const = 0;
  virtual void createItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) = 0;

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

  void createItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) override;
  void draw(std::shared_ptr<MenuComponent> menuModel) const override; // { /* Draw button menu */ }
  // void drawTitle(const std::string& title) const override;
};

// class ListMenuView : public IMenuView {
// public:
//   void draw(const MenuComponent& menuModel) const override; // { /* Draw list menu */ }
// };
