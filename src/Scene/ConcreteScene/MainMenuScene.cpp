#include "MainMenuScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
void MainMenuScene::init(void) {
  // Initialize main menu scene resources
  loadResources();
}

void MainMenuScene::update(float deltaTime) {
  std::cout << "Updating MainMenuScene..." << std::endl;
}

void MainMenuScene::render() {
  Font& font = ResourceManager::getInstance().getFont("public_sans_bold");
  
  const char* text = "Welcome to 5D Chess";
  float fontSize = 50;  // Reduced from 50 to avoid broken letters
  float spacing = 2;
  
  // Measure text size
  Vector2 textSize = MeasureTextEx(font, text, fontSize, spacing);
  
  // Calculate center position
  Vector2 position = {
    (GetScreenWidth() - textSize.x) / 2.0f,
    (GetScreenHeight() - textSize.y) / 2.0f
  };
  
  // Draw centered text (using DrawTextEx is simpler for centering)
  DrawTextEx(font, text, position, fontSize, spacing, { 245, 186, 187, 255 });

  const char* subtitle = "With Multiverse Time Travel!";
  // draw subtitle below the main text
  Vector2 subtitleSize = MeasureTextEx(font, subtitle, fontSize * 0.8f, spacing);
  Vector2 subtitlePosition = {
    (GetScreenWidth() - subtitleSize.x) / 2.0f,
    position.y + textSize.y + 20 // 20 pixels below the main text
  };
  DrawTextEx(font, subtitle, subtitlePosition, fontSize * 0.8f, spacing, { 200, 200, 200, 255 });
}

void MainMenuScene::cleanup(void) {}

bool MainMenuScene::isActive(void) const { return _isActive; }

std::string MainMenuScene::getName(void) const { return "MainMenuScene"; }

std::string MainMenuScene::getGameStateName(void) const {
  return "MAIN_MENU"; 
}

void MainMenuScene::onEnter() { _isActive = true; }

void MainMenuScene::onExit() { _isActive = false; }

bool MainMenuScene::shouldTransition() const { return false; }

void MainMenuScene::loadResources(void) {}

void MainMenuScene::unloadResources(void) {}