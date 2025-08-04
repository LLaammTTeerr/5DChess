#include "SceneFactory.h"
#include "Scene.h"
#include "WelcomeScene.h"


std::unique_ptr<Scene> WelcomeSceneFactory::createScene() {
  return std::make_unique<WelcomeScene>();
}


std::string WelcomeSceneFactory::getSceneType() const {
  return "WelcomeScene";
}