// #include "SceneFactoryManager.h"
// #include "SceneFactory.h"
// #include "Scene.h"
// #include <cassert>


// SceneFactoryManager::SceneFactoryManager() {
//   registerFactory("WelcomeScene", std::make_unique<WelcomeSceneFactory>());
//   registerFactory("VersusScene", std::make_unique<VersusSceneFactory>());
// }

// SceneFactoryManager &SceneFactoryManager::getInstance() {
//   static SceneFactoryManager instance;
//   return instance;
// }

// std::unique_ptr<Scene> SceneFactoryManager::createScene(const std::string& sceneName) {
//   auto it = _factories.find(sceneName);
//   if (it != _factories.end()) {
//     return it->second->createScene();
//   }
//   return nullptr; // or throw an exception if preferred
// }

// void SceneFactoryManager::registerFactory(const std::string &sceneName, std::unique_ptr<SceneFactory> factory) {
//   _factories[sceneName] = std::move(factory);
// }

// bool SceneFactoryManager::hasFactory(const std::string &sceneName) const {
//   return _factories.find(sceneName) != _factories.end();
// }
