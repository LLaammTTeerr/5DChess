#pragma once
#include "Scene.h"
#include <string>

class SettingMenuController;
class MenuComponent;

class SettingsScene : public Scene {
private:
  bool _isActive = false;
private:
  std::shared_ptr<SettingMenuController> _settingsMenuController;
  std::shared_ptr<MenuComponent> _settingMenuSystem;
public:
  SettingsScene();
    ~SettingsScene() override = default;

    void init(void) override;
    void handleInput(void) override;
    void update(float deltaTime) override;
    void render(void) override;
    void cleanup(void) override;

    bool isActive(void) const override;
    std::string getName(void) const override;
    std::string getGameStateName(void) const override;

    void onEnter(void) override;
    void onExit(void) override;

    bool shouldTransition(void) const override;
private:
  std::string _settingSelected; // "None", "Piece Theme", "Music"
};
