#include "VersusScene.h"
#include "ResourceManager.h"
#include "gameState.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
void VersusScene::init(void) {}

void VersusScene::handleInput(void) {
  // Handle versus scene input
  if (IsKeyPressed(KEY_ESCAPE)) {
    std::cout << "Escape pressed in VersusScene - returning to main menu" << std::endl;
  }
  
  if (IsKeyPressed(KEY_SPACE)) {
    std::cout << "Space pressed in VersusScene - start game" << std::endl;
  }
}

void VersusScene::update(float deltaTime) {
  std::cout << "Updating VersusScene..." << std::endl;
}

void VersusScene::render() {
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

void VersusScene::cleanup(void) {}

bool VersusScene::isActive(void) const { return _isActive; }

std::string VersusScene::getName(void) const { return "VersusScene"; }

std::string VersusScene::getGameStateName(void) const {
  return "VERSUS";
}

void VersusScene::onEnter() { 
  std::cout << "Entering VersusScene..." << std::endl;
  _isActive = true; 
}

void VersusScene::onExit() { _isActive = false; }

bool VersusScene::shouldTransition() const { return false; }
