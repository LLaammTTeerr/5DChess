#pragma once
#include "gameState.h"
#include <memory>
#include <vector>

class MenuComponent;
class SceneManager;
class MenuItemView;
class Scene;

class SettingsState : public GameState {
private:
    std::string selectedTheme = "Classic";
    std::string selectedMusic = "None";
    mutable int menuVersion = 0;
    
public:
    void onEnter(GameStateModel* context) override;
    void onExit(GameStateModel* context) override;
    void update(GameStateModel* context, float deltaTime) override;
    std::string getName() const override { return "SETTINGS"; }
    std::unique_ptr<GameState> clone() const override;
    std::shared_ptr<MenuComponent> createNavigationMenu(GameStateModel* gameStateModel, SceneManager* sceneManager) override;
    std::unique_ptr<Scene> createScene() const override;
    std::vector<std::shared_ptr<MenuItemView>> createNavigationMenuButtonItemViews(std::shared_ptr<MenuComponent> menu) const override;
    
    // Settings management methods
    void setTheme(const std::string& theme);
    void setMusic(const std::string& music);
    std::string getSelectedTheme() const { return selectedTheme; }
    std::string getSelectedMusic() const { return selectedMusic; }
};
