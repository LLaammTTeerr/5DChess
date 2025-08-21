#include "VersusScene.h"
#include "ResourceManager.h"
#include "gameState.h"
#include "MenuController.h"
#include "MenuView.h"
#include <iostream>
#include <raylib.h>

void VersusScene::init(void) {
  initializeMenuController();
}

void VersusScene::handleInput(void) {
  // Handle versus scene input
  if (IsKeyPressed(KEY_ESCAPE)) {
    std::cout << "Escape pressed in VersusScene - returning to main menu" << std::endl;
  }
  
  if (IsKeyPressed(KEY_SPACE)) {
    std::cout << "Space pressed in VersusScene - start game" << std::endl;
  }

  // Handle menu input through controller
  if (menuController) {
    menuController->handleInput();
  }
}

void VersusScene::update(float deltaTime) {
  // Update menu controller
  if (menuController) {
    menuController->update();
  }
}

void VersusScene::render() {
  if (menuController && menuController->isMenuVisible()) {
    // Draw the menu background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {30, 30, 40, 255});
    
    // Draw menu title
    Font& font = ResourceManager::getInstance().getFont("public_sans_bold");
    const char* title = "Select Game Mode";
    float titleFontSize = 48;
    float spacing = 2;
    
    Vector2 titleSize = MeasureTextEx(font, title, titleFontSize, spacing);
    Vector2 titlePosition = {
      (GetScreenWidth() - titleSize.x) / 2.0f,
      50
    };
    DrawTextEx(font, title, titlePosition, titleFontSize, spacing, {245, 186, 187, 255});
    
    // Draw the menu using the controller
    menuController->draw();
    
    // Draw instructions
    const char* instruction = "Use mouse to select a game mode";
    float instructionFontSize = 20;
    Vector2 instructionSize = MeasureTextEx(font, instruction, instructionFontSize, spacing);
    Vector2 instructionPosition = {
      (GetScreenWidth() - instructionSize.x) / 2.0f,
      static_cast<float>(GetScreenHeight() - 50)
    };
    DrawTextEx(font, instruction, instructionPosition, instructionFontSize, spacing, {200, 200, 200, 255});
    
  } else {
    // Draw the selected game mode info
    Font& font = ResourceManager::getInstance().getFont("public_sans_bold");    
    // Draw selected game mode
    std::string modeText = "Selected Mode: " + selectedGameMode;
    DrawTextEx(font, modeText.c_str(), {50, 50}, 20, 2, {255, 255, 255, 255});
  }
}

void VersusScene::cleanup(void) {}

bool VersusScene::isActive(void) const { return _isActive; }

std::string VersusScene::getName(void) const { return "VersusScene"; }

std::string VersusScene::getGameStateName(void) const {
  return "VERSUS";
}

void VersusScene::onEnter() { 
  std::cout << "Entering VersusScene..." << std::endl;
  _isActive = true;
  selectedGameMode = "None";
  init(); // Initialize the menu when entering the scene
}

void VersusScene::onExit() { _isActive = false; }

bool VersusScene::shouldTransition() const { return false; }

void VersusScene::selectGameMode(const std::string& mode) {
  selectedGameMode = mode;
  if (menuController) {
    menuController->selectGameMode(mode);
    menuController->setMenuVisible(false);
  }
  std::cout << "Game mode selected: " << mode << std::endl;
}

void VersusScene::initializeMenuController() {
  // Create the menu controller
  menuController = std::make_unique<VersusMenuController>(this);
  
  // Create the ListMenuView with custom positioning
  Rectangle menuArea = {
    (float)(GetScreenWidth() / 2 - 150),  // Center horizontally
    150,                                   // Start below the title
    300,                                   // Width
    400                                    // Height
  };
  
  auto listMenuView = std::make_unique<ListMenuView>(menuArea);
  listMenuView->setItemHeight(50.0f);
  listMenuView->setItemSpacing(10.0f);
  listMenuView->setScrollbarWidth(20.0f);
  
  // Set the view strategy and create the menu
  menuController->setViewStrategy(std::move(listMenuView));
  menuController->createGameModeMenu();
}
