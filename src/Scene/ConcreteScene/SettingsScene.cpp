#include "SettingsScene.h"
#include "App.h"
#include "Render/UITheme.h"
#include "Render/Motion.h"
#include <iostream>
#include <raylib.h>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "MenuItemView.h"
#include "MenuController.h"
#include "Audio/AudioManager.h"
#include "GameStates/ConcreteGameStates/SettingsState.h"

void SettingsScene::init(void) {
    // Holds the selected theme/music name for this scene's menu commands
    _settingsState = std::make_shared<SettingsState>();

    // Initialize the menu system first
    _settingMenuSystem = std::make_shared<Menu>("Settings Menu", true);
    
    // Theme selection section
    std::shared_ptr<MenuComponent> ThemeSection = std::make_shared<Menu>("Piece Theme", true);
    
    std::shared_ptr<MenuComponent> ClassicTheme = std::make_shared<MenuItem>("Classic", true);
    ClassicTheme->setCommand(createThemeSelectCommand("Classic"));
    
    std::shared_ptr<MenuComponent> ModernTheme = std::make_shared<MenuItem>("Modern", true);
    ModernTheme->setCommand(createThemeSelectCommand("Modern"));
    
    std::shared_ptr<MenuComponent> FantasyTheme = std::make_shared<MenuItem>("Fantasy", true);
    FantasyTheme->setCommand(createThemeSelectCommand("Fantasy"));

    std::shared_ptr<MenuComponent> PixelTheme = std::make_shared<MenuItem>("Pixel", true);
    PixelTheme->setCommand(createThemeSelectCommand("Pixel"));

    ThemeSection->addItem(ClassicTheme);
    ThemeSection->addItem(ModernTheme);
    ThemeSection->addItem(FantasyTheme);
    ThemeSection->addItem(PixelTheme);

    // Add the theme section to the main menu system
    _settingMenuSystem->addItem(ThemeSection);


    // Music selection section
    std::shared_ptr<MenuComponent> MusicSection = std::make_shared<Menu>("Music", true);

    
    // "Off" first (the default), then one entry per bundled track
    std::shared_ptr<MenuComponent> NoMusic = std::make_shared<MenuItem>(AudioManager::offName(), true);
    NoMusic->setCommand(createMusicSelectCommand(AudioManager::offName(), _settingsState.get()));
    MusicSection->addItem(NoMusic);
    for (const auto& track : AudioManager::tracks()) {
        std::shared_ptr<MenuComponent> item = std::make_shared<MenuItem>(track.name, true);
        item->setCommand(createMusicSelectCommand(track.name, _settingsState.get()));
        MusicSection->addItem(item);
    }

    // Sound effects on/off toggle (title is refreshed by the controller after a click)
    std::shared_ptr<MenuComponent> SfxToggle = std::make_shared<MenuItem>(
        SfxToggleCommand::titleFor(App::current().audio.sfxEnabled()), true);
    SfxToggle->setCommand(std::make_unique<SfxToggleCommand>());
    MusicSection->addItem(SfxToggle);

    // Add the music section to the main menu system
    _settingMenuSystem->addItem(MusicSection);
    // Display section: global Reduce motion toggle (title is refreshed by the controller after a click)
    std::shared_ptr<MenuComponent> DisplaySection = std::make_shared<Menu>("Display", true);
    std::shared_ptr<MenuComponent> MotionToggle = std::make_shared<MenuItem>(
        MotionToggleCommand::titleFor(UI::Motion::reduced()), true);
    MotionToggle->setCommand(std::make_unique<MotionToggleCommand>());
    DisplaySection->addItem(MotionToggle);
    _settingMenuSystem->addItem(DisplaySection);

    _settingsMenuController = std::make_shared<SettingMenuController>(_settingMenuSystem);
}

void SettingsScene::handleInput(void) {
    // Handle menu controller input
    if (_settingsMenuController) {
        _settingsMenuController->handleInput();
    }
}

void SettingsScene::update(float deltaTime) {
    // Update settings scene logic
    if (_settingsMenuController) {
        _settingsMenuController->update();
    }
}

void SettingsScene::render() {
    ClearBackground(UI::Color::bg);
    UI::Cursor::beginFrame();

    UI::drawSceneTitle("Settings");

    // Render the menu system
    if (_settingsMenuController) {
        _settingsMenuController->draw();
    }
}

void SettingsScene::cleanup(void) {}

bool SettingsScene::isActive(void) const { return _isActive; }

std::string SettingsScene::getName(void) const { return "SettingsScene"; }

std::string SettingsScene::getGameStateName(void) const {
    return "SETTINGS";
}

void SettingsScene::onEnter() { 
    std::cout << "Entering SettingsScene..." << std::endl;
    _isActive = true;
}

void SettingsScene::onExit() { _isActive = false; }

bool SettingsScene::shouldTransition() const { return false; }
