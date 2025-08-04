#include "WelcomeScene.h"
#include "ResourceManager.h"
#include <iostream>
#include <raylib.h> // Assuming raylib is used for rendering
void WelcomeScene::init(void) {
  // Initialize welcome scene resources
  loadResources();
}

void WelcomeScene::update(float deltaTime) {}

void WelcomeScene::render() {
  // Clear background
  BeginDrawing();

  ClearBackground(RAYWHITE);
  // draw hello
  DrawText("Welcome to 5D Chess!", 10, 10, 20, DARKGRAY);

  EndDrawing();
}

void WelcomeScene::cleanup(void) {}

bool WelcomeScene::isActive(void) const { return _isActive; }

std::string WelcomeScene::getName(void) const { return "WelcomeScene"; }

void WelcomeScene::onEnter() { _isActive = true; }

void WelcomeScene::onExit() { _isActive = false; }

bool WelcomeScene::shouldTransition() const { return false; }

void WelcomeScene::loadResources(void) {}

void WelcomeScene::unloadResources(void) {}