#include "MainMenuScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
void MainMenuScene::init(void) {}

void MainMenuScene::update(float deltaTime) {
  std::cout << "Updating MainMenuScene..." << std::endl;
}

void MainMenuScene::render() {
  ClearBackground(Color{164, 204, 217, 255}); // rgb(164, 204, 217)
  Texture2D& welcomeBackground = ResourceManager::getInstance().getTexture2D("welcomeImage");
  DrawTexturePro(welcomeBackground, {0, 0, (float)welcomeBackground.width, (float)welcomeBackground.height},
                 {300, 0, (float)GetScreenWidth() - 300, (float)GetScreenHeight()},
                 {0, 0}, 0.0f, WHITE); // Draw the background image covering the whole screen

  Font& font = ResourceManager::getInstance().getFont("public_sans_bold");
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
