// // Factory Manager (Registry Pattern + Factory Pattern)
// #pragma once
// #include <iostream>
// #include <memory>
// #include <unordered_map>
// #include <string>

// class SceneFactory; 
// class Scene;


// class SceneFactoryManager {
// public:
//   static SceneFactoryManager &getInstance();

//   std::unique_ptr<Scene> createScene(const std::string &sceneName);

//   void registerFactory(const std::string &sceneName,
//             std::unique_ptr<SceneFactory> factory);

//   bool hasFactory(const std::string &sceneName) const;


//   /// delete copy and move constructors and assignment operators
//   SceneFactoryManager(const SceneFactoryManager&) = delete;
//   SceneFactoryManager& operator=(const SceneFactoryManager&) = delete;
//   SceneFactoryManager(SceneFactoryManager&&) = delete;
//   SceneFactoryManager& operator=(SceneFactoryManager&&) = delete;
  
// private:
//   SceneFactoryManager();
//   std::unordered_map<std::string, std::unique_ptr<SceneFactory>> _factories;
// };