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
  virtual void createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) = 0;
  virtual void createInGameItemsViews(int numberOfItems) = 0;
  virtual void draw(std::shared_ptr<MenuComponent> menuModel) const = 0;

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
  ButtonMenuView() = default;

  void createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) override;
  void createInGameItemsViews(int numberOfItems) override;
  void draw(std::shared_ptr<MenuComponent> menuModel) const override; // { /* Draw button menu */ }
  // void drawTitle(const std::string& title) const override;
};

class ListMenuView : public IMenuView {
private:
  float scrollOffset = 0.0f;
  float itemHeight = 40.0f;
  float itemSpacing = 5.0f;
  float scrollbarWidth = 15.0f;
  Rectangle listArea;
  Rectangle scrollbarArea;
  bool isDragging = false;
  float maxScrollOffset = 0.0f;
  
  // Scrollbar styling
  Color scrollbarBackgroundColor = {200, 200, 200, 255};
  Color scrollbarHandleColor = {120, 120, 120, 255};
  Color scrollbarHandleHoverColor = {80, 80, 80, 255};

public:
  ListMenuView(Rectangle area = {50, 100, 300, 400});
  
  void createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) override;
  void createInGameItemsViews(int numberOfItems) override;
  void draw(std::shared_ptr<MenuComponent> menuModel) const override;
  
  // Scrolling methods
  void handleScrollInput();
  void updateScrollbar();
  bool isScrollbarHovered() const;
  float getScrollHandlePosition() const;
  float getScrollHandleHeight() const;
  
  // Setters for customization
  void setListArea(Rectangle area) { listArea = area; updateScrollbarArea(); }
  void setItemHeight(float height) { itemHeight = height; }
  void setItemSpacing(float spacing) { itemSpacing = spacing; }
  void setScrollbarWidth(float width) { scrollbarWidth = width; updateScrollbarArea(); }

private:
  void updateScrollbarArea();
  void calculateMaxScrollOffset();
};
