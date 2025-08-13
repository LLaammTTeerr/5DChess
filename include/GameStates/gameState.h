#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>


// Forward declarations
class GameStateModel;
class GameState;
class SceneManager;
class MenuComponent;
class Scene;
class MenuItemView;

// Abstract base class for game states
class GameState {
public:
    virtual ~GameState() = default;
    virtual void onEnter(GameStateModel* context) = 0;
    virtual void onExit(GameStateModel* context) = 0;
    virtual void update(GameStateModel* context, float deltaTime) = 0;
    virtual std::string getName() const = 0;
    virtual std::unique_ptr<GameState> clone() const = 0;

    
    virtual std::shared_ptr<MenuComponent> createMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) = 0;
    // virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const = 0;
    virtual std::unique_ptr<Scene> createScene() const = 0;
    virtual std::vector<std::shared_ptr<MenuItemView>> createMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const = 0;
};


// GameStateModel class using State pattern
class GameStateModel {
private:
    std::unique_ptr<GameState> _currentState;

public:
    GameStateModel();
    ~GameStateModel() = default;

    // State management
    void setState(std::unique_ptr<GameState> newState);
    void setStateByName(const std::string& stateName);
    GameState* getCurrentState() const { return _currentState.get(); }
    std::string getCurrentStateName() const;
    
    // Update method for state pattern
    void update(float deltaTime);
    
    // Helper methods for state creation
    static std::unique_ptr<GameState> createState(const std::string& stateName);
    std::shared_ptr<MenuComponent> createMenuForCurrentState(SceneManager* sceneManager);
};
