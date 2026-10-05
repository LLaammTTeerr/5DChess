#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <raylib.h>
#include "gameState.h"
#include "MenuItemView.h"
#include "Render/UITheme.h"

// Forward declarations
class MenuComponent;
class GameStateModel;
class GameState;

// abstract class for menu view
class IMenuView {
protected:
  std::vector<std::shared_ptr<MenuItemView>> _itemViews;
  
public:
  virtual ~IMenuView() = default;
  virtual void createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) = 0;
  virtual void createInGameItemsViews(int numberOfItems) = 0;
  virtual void createSettingsMenuItemViews(int numberOfItems) = 0;
  virtual void draw(std::shared_ptr<MenuComponent> menuModel) const = 0;

  // True when the point lies inside any item view
  bool isPointOverItems(Vector2 point) const {
    for (const auto& itemView : _itemViews) {
      if (itemView && itemView->isPointInside(point)) return true;
    }
    return false;
  }

  virtual void setItemViews(const std::vector<std::shared_ptr<MenuItemView>>& views) { _itemViews = views; }
  std::vector<std::shared_ptr<MenuItemView>>& getItemViews() { return _itemViews; }
  const std::vector<std::shared_ptr<MenuItemView>>& getItemViews() const { return _itemViews; }
};

// Concrete implementation of a menu view
class ButtonMenuView : public IMenuView {
public:
  ButtonMenuView() = default;

  void createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) override;
  void createInGameItemsViews(int numberOfItems) override;
  void createSettingsMenuItemViews(int numberOfItems) override;
  void draw(std::shared_ptr<MenuComponent> menuModel) const override;
};

class ListMenuView : public IMenuView {
private:
  float scrollOffset = 0.0f;
  float itemHeight = UI::Space::buttonHeight + 4.0f;
  float itemSpacing = UI::Space::sm + 4.0f;
  float scrollbarWidth = 15.0f;
  Rectangle listArea;
  Rectangle scrollbarArea;
  bool isDragging = false;
  float maxScrollOffset = 0.0f;
  
  // Scrollbar styling (tokens)
  Color scrollbarBackgroundColor = UI::Color::surfaceAlt;
  Color scrollbarHandleColor = UI::Color::border;
  Color scrollbarHandleHoverColor = UI::Color::textMuted;

public:
  ListMenuView(Rectangle area = {50, 100, 300, 400});
  
  void createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) override;
  void createInGameItemsViews(int numberOfItems) override;
  void createSettingsMenuItemViews(int numberOfItems) override {};
  void draw(std::shared_ptr<MenuComponent> menuModel) const override;
  
  // Scrolling methods
  void handleScrollInput();
  void updateScrollbar();
  bool isScrollbarHovered() const;
  float getScrollHandlePosition() const;
  float getScrollHandleHeight() const;
  
  // Get item position accounting for scroll offset
  Vector2 getScrolledItemPosition(size_t index) const;
  
  // Get the list area rectangle
  Rectangle getListArea() const { return listArea; }
  
  // Auto-resize functionality
  void autoResizeToFitContent();
  float calculateRequiredContentHeight() const;
  
  // Setters for customization
  void setListArea(Rectangle area) { listArea = area; updateScrollbarArea(); }
  void setItemHeight(float height) { itemHeight = height; }
  void setItemSpacing(float spacing) { itemSpacing = spacing; }
  void setScrollbarWidth(float width) { scrollbarWidth = width; updateScrollbarArea(); }

private:
  void updateScrollbarArea();
  void calculateMaxScrollOffset();
};
