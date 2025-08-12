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
};

// Concrete state classes
class MainMenuState : public GameState {
public:
    void onEnter(GameStateModel* context) override;
    void onExit(GameStateModel* context) override;
    void update(GameStateModel* context, float deltaTime) override;
    std::string getName() const override { return "MAIN_MENU"; }
    std::unique_ptr<GameState> clone() const override;
    std::shared_ptr<MenuComponent> createMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) override;
    // virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
    virtual std::unique_ptr<Scene> createScene() const override;
};

class VersusState : public GameState {
public:
    void onEnter(GameStateModel* context) override;
    void onExit(GameStateModel* context) override;
    void update(GameStateModel* context, float deltaTime) override;
    std::string getName() const override { return "VERSUS"; }
    std::unique_ptr<GameState> clone() const override;
    std::shared_ptr<MenuComponent> createMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) override;
    // virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
    virtual std::unique_ptr<Scene> createScene() const override;
};

// class PuzzlesState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "PUZZLES"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class GuideState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "GUIDE"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class SettingsMenuState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "SETTINGS_MENU"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class InGameState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "IN_GAME"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

// class PausedState : public GameState {
// public:
//     void onEnter(GameStateModel* context) override;
//     void onExit(GameStateModel* context) override;
//     void update(GameStateModel* context, float deltaTime) override;
//     std::string getName() const override { return "PAUSED"; }
//     std::unique_ptr<GameState> clone() const override;
//     std::shared_ptr<MenuComponent> createMenu() const override;
//     virtual void renderMenu(std::shared_ptr<MenuComponent> menu) const override;
// };

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
