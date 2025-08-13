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

