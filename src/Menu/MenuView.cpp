#include "MenuView.h"
#include "MenuComponent.h"
#include "gameState.h"
#include "MenuItemView.h"
#include "ResourceManager.h"
#include <cmath>


void SelectionIndicator::draw(const Rectangle* target) const {
    const float dt = GetFrameTime();
    if (target) {
        if (!_init) {
            _init = true;
            _x.init(target->x, 420.0f, 0.9f); _y.init(target->y, 420.0f, 0.9f);
            _w.init(target->width, 420.0f, 0.9f); _h.init(target->height, 420.0f, 0.9f);
            _alpha.init(0.0f, 600.0f, 1.0f);
        }
        _x.setTarget(target->x); _y.setTarget(target->y);
        _w.setTarget(target->width); _h.setTarget(target->height);
    }
    _alpha.setTarget(target ? 1.0f : 0.0f);
    _x.update(dt); _y.update(dt); _w.update(dt); _h.update(dt); _alpha.update(dt);
    const float a = std::fmax(0.0f, std::fmin(1.0f, _alpha.value)) * MenuItemView::globalAlpha();
    if (!_init || a <= 0.01f) return;

    Rectangle r = {_x.value, _y.value, _w.value, _h.value};
    auto fade = [a](Color c) { c.a = static_cast<unsigned char>(c.a * a); return c; };
    DrawRectangleRoundedLinesEx(r, UI::Space::radius, 8, UI::Space::outline, fade(UI::Color::selected));
    const float barH = r.height * 0.5f;
    DrawRectangleRounded({r.x + 8, r.y + (r.height - barH) / 2, 4, barH}, 1.0f, 4, fade(UI::Color::selected));
}

void ButtonMenuView::createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) {
    _itemViews.clear(); // Clear existing item views
    struct ResetSkip { size_t& v; ~ResetSkip() { v = 0; } } resetSkip{_skipEnterBelow};

    if (gameState == nullptr) {
        for (const auto& child : menuModel->getChildren()) {
            if (child) {
                Vector2 position = { 100, static_cast<float>(_itemViews.size() * (UI::Space::buttonHeight + UI::Space::buttonSpacing) + 100) }; // Example positioning
                Vector2 size = { 200, UI::Space::buttonHeight };
                auto itemView = std::make_shared<MenuItemView>(position, size);
                _itemViews.push_back(itemView);
            }
        }
    }
    else {
        _itemViews.clear(); // Clear existing item views
        // Create item views based on the game state
        auto itemViews = gameState->createNavigationMenuButtonItemViews(menuModel);
        for (const auto& itemView : itemViews) {
            if (itemView) {
                if (_itemViews.size() >= _skipEnterBelow)
                    itemView->setEnterIndex(static_cast<int>(_itemViews.size() - _skipEnterBelow));
                _itemViews.push_back(itemView);
            }
        }
    }
}

void ButtonMenuView::draw(std::shared_ptr<MenuComponent> menuModel) const {
    const auto& menuItems = menuModel->getChildren();
    // Draw each item view
    for (size_t i = 0; i < _itemViews.size() && i < menuItems.size(); ++i) {
        // Disabled items are still drawn (greyed out by MenuItemView) but are not clickable
        if (_itemViews[i]) {
            // Submit is the primary action of the in-game row: filled accent when enabled
            if (menuItems[i]->getTitle() == "Submit") _itemViews[i]->setPrimary(true);
            _itemViews[i]->setSelectionOutline(false);
            _itemViews[i] -> draw(menuItems[i]);
        }
    }
    // Sliding selection indicator (settings tabs / options)
    Rectangle selRect{};
    bool haveSel = false;
    for (size_t i = 0; i < _itemViews.size() && i < menuItems.size(); ++i) {
        if (_itemViews[i] && _itemViews[i]->getSelected() && menuItems[i]->isEnabled()) {
            const Vector2 p = _itemViews[i]->getPosition(), sz = _itemViews[i]->getSize();
            selRect = {p.x, p.y, sz.x, sz.y};
            haveSel = true;
            break;
        }
    }
    _indicator.draw(haveSel ? &selRect : nullptr);
}

void ButtonMenuView::createInGameItemsViews(int numberOfItems) {
    _itemViews.clear(); // Clear existing item views

    // Action row sits just below the HUD pill (drawn at the top centre by ChessView)
    const float horizontalSpacing = UI::Space::buttonSpacing;
    const float itemHeight = UI::Space::buttonHeight;
    const float itemWidth = 130.0f;
    const Rectangle menuArea = {0, 0, (float)GetScreenWidth(), 100.0f};

    const float startX = menuArea.x + (menuArea.width - numberOfItems * itemWidth - (numberOfItems - 1) * horizontalSpacing) / 2;
    const float startY = UI::Layout::actionRowY;

    _itemViews.reserve(numberOfItems); // Reserve space for the specified number of items
    for (int i = 0; i < numberOfItems; ++i) {
        Vector2 position = {startX + i * (itemWidth + horizontalSpacing), startY};
        Vector2 size = {itemWidth, itemHeight};
        auto itemView = std::make_shared<MenuItemView>(position, size);
        _itemViews.push_back(itemView);
    }
}

void ButtonMenuView::createSettingsMenuItemViews(int numberOfItems) {
     _itemViews.clear(); // Clear existing item views

    const float horizontalSpacing = UI::Space::buttonSpacing + UI::Space::sm;
    const float itemHeight = UI::Space::buttonHeight;
    const float itemWidth = 160.0f;
    const Rectangle menuArea = {0, 150, (float)GetScreenWidth(), itemHeight}; // Example menu area

    const float startX = menuArea.x + (menuArea.width - numberOfItems * itemWidth - (numberOfItems - 1) * horizontalSpacing) / 2;
    const float startY = menuArea.y + (menuArea.height - itemHeight) / 2;

    _itemViews.reserve(numberOfItems); // Reserve space for the specified number of items
    for (int i = 0; i < numberOfItems; ++i) {
        Vector2 position = {startX + i * (itemWidth + horizontalSpacing), startY};
        Vector2 size = {itemWidth, itemHeight};
        auto itemView = std::make_shared<MenuItemView>(position, size);
        itemView->setEnterIndex(i);
        _itemViews.push_back(itemView);
    }
}

// ListMenuView implementation
ListMenuView::ListMenuView(Rectangle area) : listArea(area) {
    updateScrollbarArea();
}

void ListMenuView::updateScrollbarArea() {
    scrollbarArea = {
        listArea.x + listArea.width - scrollbarWidth,
        listArea.y,
        scrollbarWidth,
        listArea.height
    };
}

void ListMenuView::calculateMaxScrollOffset() {
    float totalContentHeight = _itemViews.size() * (itemHeight + itemSpacing) - itemSpacing;
    maxScrollOffset = fmaxf(0.0f, totalContentHeight - listArea.height);
}

void ListMenuView::createNavigationItemViews(std::shared_ptr<MenuComponent> menuModel, GameState* gameState) {
    _itemViews.clear();

    if (gameState == nullptr) {
        for (size_t i = 0; i < menuModel->getChildren().size(); ++i) {
            const auto& child = menuModel->getChildren()[i];
            if (child) {
                Vector2 position = { 
                    listArea.x + 10.0f, 
                    listArea.y + i * (itemHeight + itemSpacing) 
                };
                Vector2 size = { listArea.width - scrollbarWidth - 20.0f, itemHeight };
                auto itemView = std::make_shared<MenuItemView>(position, size);
                _itemViews.push_back(itemView);
            }
        }
    } else {
        _itemViews.clear();
        auto itemViews = gameState->createNavigationMenuButtonItemViews(menuModel);
        
        for (size_t i = 0; i < itemViews.size(); ++i) {
            if (itemViews[i]) {
                Vector2 position = { 
                    listArea.x + 10.0f, 
                    listArea.y + i * (itemHeight + itemSpacing) 
                };
                Vector2 size = { listArea.width - scrollbarWidth - 20.0f, itemHeight };
                itemViews[i]->setPosition(position);
                itemViews[i]->setSize(size);
                _itemViews.push_back(itemViews[i]);
            }
        }
    }
    
    calculateMaxScrollOffset();
    autoResizeToFitContent();
}

void ListMenuView::createInGameItemsViews(int numberOfItems) {
    _itemViews.clear();
    _itemViews.reserve(numberOfItems);
    
    for (int i = 0; i < numberOfItems; ++i) {
        Vector2 position = { 
            listArea.x + 10.0f, 
            listArea.y + i * (itemHeight + itemSpacing) 
        };
        Vector2 size = { listArea.width - scrollbarWidth - 20.0f, itemHeight };
        auto itemView = std::make_shared<MenuItemView>(position, size);
        itemView->setEnterIndex(i);
        _itemViews.push_back(itemView);
    }
    
    calculateMaxScrollOffset();
    autoResizeToFitContent();
}

void ListMenuView::handleScrollInput() {
    Vector2 mousePos = GetMousePosition();
    
    // Handle mouse wheel scrolling
    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0 && CheckCollisionPointRec(mousePos, listArea)) {
        scrollOffset -= wheelMove * 30.0f; // Scroll speed
        scrollOffset = fmaxf(0.0f, fminf(scrollOffset, maxScrollOffset));
    }
    
    // Handle scrollbar dragging
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Check if clicked on scrollbar area (not just handle)
        if (CheckCollisionPointRec(mousePos, scrollbarArea)) {
            isDragging = true;
            // If clicked on track but not handle, jump to that position
            if (!isScrollbarHovered()) {
                float relativeY = mousePos.y - scrollbarArea.y;
                if (scrollbarArea.height > 0) {
                    float scrollRatio = relativeY / scrollbarArea.height;
                    scrollOffset = fmaxf(0.0f, fminf(scrollRatio * maxScrollOffset, maxScrollOffset));
                }
            }
        }
    }
    
    if (isDragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float relativeY = mousePos.y - scrollbarArea.y;
            if (scrollbarArea.height > 0) {
                float scrollRatio = relativeY / scrollbarArea.height;
                scrollOffset = fmaxf(0.0f, fminf(scrollRatio * maxScrollOffset, maxScrollOffset));
            }
        } else {
            isDragging = false;
        }
    }
}

bool ListMenuView::isScrollbarHovered() const {
    Vector2 mousePos = GetMousePosition();
    Rectangle handleRect = {
        scrollbarArea.x,
        scrollbarArea.y + getScrollHandlePosition(),
        scrollbarArea.width,
        getScrollHandleHeight()
    };
    return CheckCollisionPointRec(mousePos, handleRect);
}

float ListMenuView::getScrollHandlePosition() const {
    if (maxScrollOffset <= 0) return 0.0f;
    float scrollRatio = scrollOffset / maxScrollOffset;
    return scrollRatio * (scrollbarArea.height - getScrollHandleHeight());
}

float ListMenuView::getScrollHandleHeight() const {
    if (maxScrollOffset <= 0) return scrollbarArea.height;
    float visibleRatio = listArea.height / (listArea.height + maxScrollOffset);
    return fmaxf(20.0f, visibleRatio * scrollbarArea.height);
}

void ListMenuView::updateScrollbar() {
    calculateMaxScrollOffset();
    
    // Clamp scroll offset to valid range
    scrollOffset = fmaxf(0.0f, fminf(scrollOffset, maxScrollOffset));
    
    // Update scrollbar area dimensions
    updateScrollbarArea();
}

Vector2 ListMenuView::getScrolledItemPosition(size_t index) const {
    if (index >= _itemViews.size() || !_itemViews[index]) {
        return {0, 0};
    }
    
    Vector2 originalPos = _itemViews[index]->getPosition();
    return {originalPos.x, originalPos.y - scrollOffset};
}

float ListMenuView::calculateRequiredContentHeight() const {
    if (_itemViews.empty()) {
        return 0.0f;
    }
    
    return _itemViews.size() * (itemHeight + itemSpacing) - itemSpacing;
}

void ListMenuView::autoResizeToFitContent() {
    float requiredHeight = calculateRequiredContentHeight();
    
    // Only resize if the required height is less than the current listArea height
    if (requiredHeight > 0 && requiredHeight < listArea.height) {
        // Maintain the same x, y, and width, but adjust the height
        listArea.height = requiredHeight;
        
        // Update related components
        updateScrollbarArea();
        calculateMaxScrollOffset();
        
        // Reset scroll offset since we no longer need scrolling
        scrollOffset = 0.0f;
    }
}

void ListMenuView::draw(std::shared_ptr<MenuComponent> menuModel) const {
    const auto& menuItems = menuModel->getChildren();
    
    
    // Begin scissor mode for clipping
    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width - (int)scrollbarWidth, (int)listArea.height);
    
    // Draw menu items with scroll offset
    Rectangle selRect{};
    bool haveSel = false;
    for (size_t i = 0; i < _itemViews.size() && i < menuItems.size(); ++i) {
        if (_itemViews[i]) {
            _itemViews[i]->setSelectionOutline(false);
            // Calculate item position with scroll offset
            Vector2 originalPos = _itemViews[i]->getPosition();
            Vector2 scrolledPos = { originalPos.x, originalPos.y - scrollOffset };
            
            // Only draw items that are visible in the list area
            if (scrolledPos.y + itemHeight >= listArea.y && 
                scrolledPos.y <= listArea.y + listArea.height) {
                
                // Temporarily update position for drawing
                const_cast<MenuItemView*>(_itemViews[i].get())->setPosition(scrolledPos);
                _itemViews[i]->draw(menuItems[i]);
                // Restore original position
                const_cast<MenuItemView*>(_itemViews[i].get())->setPosition(originalPos);
                if (_itemViews[i]->getSelected() && menuItems[i]->isEnabled()) {
                    selRect = {scrolledPos.x, scrolledPos.y, _itemViews[i]->getSize().x, _itemViews[i]->getSize().y};
                    haveSel = true;
                }
            }
        }
    }
    _indicator.draw(haveSel ? &selRect : nullptr);
    
    EndScissorMode();
    
    // Draw scrollbar if needed
    if (maxScrollOffset > 0) {
        // Draw scrollbar background
        DrawRectangleRounded(scrollbarArea, 1.0f, 6, scrollbarBackgroundColor);
        
        // Draw scrollbar handle
        Rectangle handleRect = {
            scrollbarArea.x,
            scrollbarArea.y + getScrollHandlePosition(),
            scrollbarArea.width,
            getScrollHandleHeight()
        };
        
        Color handleColor = isScrollbarHovered() ? scrollbarHandleHoverColor : scrollbarHandleColor;
        DrawRectangleRounded(handleRect, 1.0f, 6, handleColor);
    }
}