#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <raylib.h>

class ICommand; // Forward declaration

/*
  Menu will use composite pattern to allow for
  a tree structure of menu items and submenus.
*/

class GameStateModel {
public:
  enum class State {
    MAIN_MENU,
    SETTINGS_MENU,
    IN_GAME,
    PAUSED
  };

private:
  State _currentState = State::MAIN_MENU;
  int _volume = 50; // Default volume level
  std::string _gameMode = "Normal"; // Default game mode

public:
  void setState(State state) { _currentState = state; };
  State getState() const { return _currentState; };
  void setVolume(int vol) { _volume = vol; };
  int getVolume() const { return _volume; };
  void setGameMode(const std::string& mode) { _gameMode = mode; };
  const std::string& getGameMode() const { return _gameMode; };
  std::string getCurrentStateName() const;
};

// Interface for menu components
class MenuComponent {
public:
  virtual ~MenuComponent() = default;
  virtual void addItem(std::shared_ptr<MenuComponent> component) = 0;
  virtual void removeItem(std::shared_ptr<MenuComponent> component) = 0;
  virtual MenuComponent* findItem(const std::string& title) = 0;

  // Setters for title and text
  virtual void setTitle(const std::string& title) = 0;
  virtual void setText(const std::string& text) = 0;
  virtual std::string getTitle() const = 0;
  virtual std::string getText() const = 0;
  

  // enable
  virtual void setEnabled(bool enabled) = 0;
  virtual bool isEnabled() const = 0;
};

// Concrete implementation of a menu item
class MenuItem : public MenuComponent {
private:
  std::string _title;
  std::string _text;
  bool _enabled;
  std::unique_ptr<ICommand> _command;

public:
  MenuItem(std::unique_ptr<ICommand> cmd, bool enabled) : _enabled(enabled), _command(std::move(cmd)) {}
  void addItem(std::shared_ptr<MenuComponent> component) override {};
  void removeItem(std::shared_ptr<MenuComponent> component) override {};
  MenuComponent* findItem(const std::string& title) override { return nullptr; };

  void setTitle(const std::string& title) override { _title = title; };
  void setText(const std::string& text) override { _text = text; };
  std::string getTitle() const override { return _title; };
  std::string getText() const override { return _text; };

  void setEnabled(bool enabled) override { _enabled = enabled; };
  bool isEnabled() const override { return _enabled; };
};

// Concrete implementation of a menu that can contain other menu items
class Menu : public MenuComponent {
private:
  std::string _title;
  std::string _text;
  bool _enabled;
  std::unique_ptr<ICommand> _command;
  std::vector<std::shared_ptr<MenuComponent>> _components;

public:
  Menu(std::unique_ptr<ICommand> cmd, bool enabled) : _enabled(enabled), _command(std::move(cmd)) {};
  void addItem(std::shared_ptr<MenuComponent> component) override { _components.push_back(component); };
  void removeItem(std::shared_ptr<MenuComponent> component) override;
  MenuComponent* findItem(const std::string& title) override;

  void setTitle(const std::string& title) override { _title = title; };
  void setText(const std::string& text) override { _text = text; };
  std::string getTitle() const override { return _title; };
  std::string getText() const override { return _text; };

  void setEnabled(bool enabled) override { _enabled = enabled; };
  bool isEnabled() const override { return _enabled; };
};


// Visual representation of a menu item
class MenuItemView {
private:
    Vector2 position;
    Vector2 size;
    bool isHovered = false;
    Color normalBackgroundColor = DARKGRAY;
    Color hoverBackgroundColor = LIGHTGRAY;
    Color disabledBackgroundColor = GRAY;

    Color normalTextColor = BLACK;
    Color hoverTextColor = WHITE;
    Color disabledTextColor = DARKGRAY;

    Font font = GetFontDefault(); // Default font, can be set to a custom one

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
};

// abstract class for menu view
class IMenuView {
protected:
  std::vector<std::shared_ptr<MenuItemView>> _itemViews;
public:
  virtual ~IMenuView() = default;
  virtual void draw() const = 0;
  std::vector<std::shared_ptr<MenuItemView>>& getItemViews() { return _itemViews; }
  const std::vector<std::shared_ptr<MenuItemView>>& getItemViews() const { return _itemViews; }
};

// Concrete implementation of a menu view
class ButtonMenuView : public IMenuView {
public:
  void draw() const override {
    // Draw button menu
  }
};

class ListMenuView : public IMenuView {
public:
  void draw() const override {
    // Draw list menu
  }
};