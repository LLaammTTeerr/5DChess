#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <raylib.h>

// class ICommand; // Forward declaration
#include "ICommand.h"
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

  
  virtual void setCommand(std::unique_ptr<ICommand> command) = 0;
  virtual std::unique_ptr<ICommand> cloneCommand() const = 0;
  virtual const std::vector<std::shared_ptr<MenuComponent>>& getChildren() const = 0;
};

// Concrete implementation of a menu item
class MenuItem : public MenuComponent {
private:
  std::string _title;
  std::string _text;
  bool _enabled;
  std::unique_ptr<ICommand> _command;

public:
  MenuItem(const std::string& title, bool enabled) : _title(title), _enabled(enabled), _command(nullptr) {}
  void addItem(std::shared_ptr<MenuComponent> component) override {};
  void removeItem(std::shared_ptr<MenuComponent> component) override {};
  MenuComponent* findItem(const std::string& title) override { return nullptr; };

  void setTitle(const std::string& title) override { _title = title; };
  void setText(const std::string& text) override { _text = text; };
  std::string getTitle() const override { return _title; };
  std::string getText() const override { return _text; };

  void setEnabled(bool enabled) override { _enabled = enabled; };
  bool isEnabled() const override { return _enabled; };
  void setCommand(std::unique_ptr<ICommand> command) override { _command = std::move(command); }
  std::unique_ptr<ICommand> cloneCommand() const override; 
  const std::vector<std::shared_ptr<MenuComponent>>& getChildren() const override;
};

// Concrete implementation of a menu that can contain other menu items
class Menu : public MenuComponent {
private:
  std::string _title;
  std::string _text;
  bool _enabled;
  std::unique_ptr<ICommand> _command;
  std::vector<std::shared_ptr<MenuComponent>> _children;

public:
  Menu(const std::string& title, bool enabled) : _title(title), _enabled(enabled), _command(nullptr) {}
  void addItem(std::shared_ptr<MenuComponent> component) override { _children.push_back(component); };
  void removeItem(std::shared_ptr<MenuComponent> component) override;
  MenuComponent* findItem(const std::string& title) override;

  void setTitle(const std::string& title) override { _title = title; };
  void setText(const std::string& text) override { _text = text; };
  std::string getTitle() const override { return _title; };
  std::string getText() const override { return _text; };

  void setEnabled(bool enabled) override { _enabled = enabled; };
  bool isEnabled() const override { return _enabled; };

  void setCommand(std::unique_ptr<ICommand> command) override { _command = std::move(command); }
  std::unique_ptr<ICommand> cloneCommand() const override;
  const std::vector<std::shared_ptr<MenuComponent>>& getChildren() const override { return _children; }
};

class CompositeMenuFactory {
private:
  GameStateModel* gameState;
public:
  CompositeMenuFactory(GameStateModel* state) : gameState(state) {}

  void showMenu(MenuComponent* root, const std::string& title);
  std::unique_ptr<MenuComponent> createMenuForState(GameStateModel::State state);

private:
  std::unique_ptr<MenuComponent> createMenuSystem(); 
  std::unique_ptr<MenuComponent> createMainMenu(); 
  std::unique_ptr<MenuComponent> createSettingsMenu();
  // std::unique_ptr<MenuComponent> createInGameMenu();
  // std::unique_ptr<MenuComponent> createPausedMenu();
  // std::unique_ptr<MenuComponent> createGameOverMenu();
};


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


class MenuController {
private:
  GameStateModel* _gameState;
  MenuComponent* _menuModel;
  std::unique_ptr<IMenuView> _menuView;
  CompositeMenuFactory _menuFactory;
  GameStateModel::State _lastState;

public:
  MenuController(GameStateModel* gameState, MenuComponent* menuModel) :
    _gameState(gameState), _menuModel(menuModel), _menuFactory(gameState),
    _lastState(GameStateModel::State::MAIN_MENU) {}

  void setViewStrategy(std::unique_ptr<IMenuView> view); 
  void updateMenuForCurrentState();
  // void updateSettingsDisplay();
  void handleInput(); // Handle user input for menu navigation

  void update();

  void draw() const;

  IMenuView* getMenuView() const { return _menuView.get(); }
};



class TestApp {
private:
  GameStateModel _gameState;
  std::unique_ptr<MenuController> _menuController;
  std::shared_ptr<MenuComponent> _menuModel;
public:
  void init() {
    // create menu model
    _menuModel = std::make_shared<Menu>("Main Menu", true);
    _menuModel->addItem(std::make_shared<MenuItem>("Start Game", true));
    _menuModel->addItem(std::make_shared<MenuItem>("Settings", true));
    _menuModel->addItem(std::make_shared<MenuItem>("Exit", true));
   

    _menuController = std::make_unique<MenuController>(&_gameState, _menuModel.get());
    _menuController->setViewStrategy(std::make_unique<ButtonMenuView>());
  } 

  void update() {
    if (_menuController) {
      std::cout << "Updating MenuController..." << std::endl;
      _menuController->update();
    }
  }

  void render() {
    if (_menuController) {
      _menuController->draw();
    }
  }
};