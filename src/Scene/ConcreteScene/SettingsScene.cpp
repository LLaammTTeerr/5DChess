#include "SettingsScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h>
#include "MenuComponent.h"
#include "MenuCommand.h"
#include "MenuItemView.h"
#include "MenuController.h"

SettingsScene::SettingsScene() {
    init();
}

void SettingsScene::init(void) {
    // Initialize the menu system first
    _settingMenuSystem = std::make_shared<Menu>("Settings Menu", true);
    
    // Theme selection section
    std::shared_ptr<MenuComponent> ThemeSection = std::make_shared<Menu>("Piece Theme", true);
    
    std::shared_ptr<MenuComponent> ClassicTheme = std::make_shared<MenuItem>("Classic", true);
    ClassicTheme->setCommand(createThemeSelectCommand("Classic"));
    
    std::shared_ptr<MenuComponent> ModernTheme = std::make_shared<MenuItem>("Modern", true);
    ModernTheme->setCommand(createThemeSelectCommand("Modern"));
    
    std::shared_ptr<MenuComponent> Modern2Theme = std::make_shared<MenuItem>("Modern2", true);
    Modern2Theme->setCommand(createThemeSelectCommand("Modern2"));

    ThemeSection->addItem(ClassicTheme);
    ThemeSection->addItem(ModernTheme);
    ThemeSection->addItem(Modern2Theme);

    // Add the theme section to the main menu system
    _settingMenuSystem->addItem(ThemeSection);


    // Music selection section
    std::shared_ptr<MenuComponent> MusicSection = std::make_shared<Menu>("Music", true);

    
    std::shared_ptr<MenuComponent> NoMusic = std::make_shared<MenuItem>("None", true);
    NoMusic->setCommand(createMusicSelectCommand("None"));
    
    std::shared_ptr<MenuComponent> Beethoven = std::make_shared<MenuItem>("Beethoven Fur Elise", true);
    Beethoven->setCommand(createMusicSelectCommand("Beethoven Fur Elise"));
    
    std::shared_ptr<MenuComponent> Canon = std::make_shared<MenuItem>("Canon in D", true);
    Canon->setCommand(createMusicSelectCommand("Canon in D"));
    
    std::shared_ptr<MenuComponent> Tchaikovsky = std::make_shared<MenuItem>("Dance of Sugar Plum", true);
    Tchaikovsky->setCommand(createMusicSelectCommand("Dance of Sugar Plum"));
    
    std::shared_ptr<MenuComponent> StarSky = std::make_shared<MenuItem>("Star Sky", true);
    StarSky->setCommand(createMusicSelectCommand("Star Sky"));
    
    std::shared_ptr<MenuComponent> Victory = std::make_shared<MenuItem>("Victory", true);
    Victory->setCommand(createMusicSelectCommand("Victory"));
    
    std::shared_ptr<MenuComponent> GloriousMorning = std::make_shared<MenuItem>("Glorious Morning", true);
    GloriousMorning->setCommand(createMusicSelectCommand("Glorious Morning"));
    
    MusicSection->addItem(NoMusic);
    MusicSection->addItem(Beethoven);
    MusicSection->addItem(Canon);
    MusicSection->addItem(Tchaikovsky);
    MusicSection->addItem(StarSky);
    MusicSection->addItem(Victory);
    MusicSection->addItem(GloriousMorning);
    
    // // Add the music section to the main menu system
    _settingMenuSystem->addItem(MusicSection);
    _settingsMenuController = std::make_shared<SettingMenuController>(_settingMenuSystem);
}

void SettingsScene::handleInput(void) {    
    update(GetFrameTime());

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
    ClearBackground(Color{164, 204, 217, 255}); // Same background as other scenes
    
    // Draw title
    Font& font = ResourceManager::getInstance().getFont("public_sans_bold");
    const char* title = "Settings";
    float titleFontSize = 48;
    float spacing = 2;
    
    Vector2 titleSize = MeasureTextEx(font, title, titleFontSize, spacing);
    Vector2 titlePosition = {
        (GetScreenWidth() - titleSize.x) / 2.0f, 50
    };
    DrawTextEx(font, title, titlePosition, titleFontSize, spacing, {255, 249, 175, 255});

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
