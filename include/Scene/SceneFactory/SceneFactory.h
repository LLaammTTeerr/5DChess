#pragma once
#include <memory>

class Scene; // interface for Scene (produced by factories)

// Interface for Scene Factory
class SceneFactory {
public:
  virtual ~SceneFactory() = default;
  virtual std::unique_ptr<Scene> createScene() = 0;
  virtual std::string getSceneType() const = 0;
};

// Concrete Factories
class WelcomeSceneFactory : public SceneFactory {
public:
  std::unique_ptr<Scene> createScene() override;
  std::string getSceneType() const override;
};

// // Concrete Factories for each scene type
// class MenuSceneFactory : public SceneFactory {
// public:
//   std::unique_ptr<Scene> createScene() override;
//   const char *getSceneType() const override { return "MenuScene"; }
// };

// class GameplaySceneFactory : public SceneFactory {
// public:
//   std::unique_ptr<Scene> createScene() override;
//   const char *getSceneType() const override { return "GameplayScene"; }
// };

// class PauseSceneFactory : public SceneFactory {
// public:
//   std::unique_ptr<Scene> createScene() override;
//   const char *getSceneType() const override { return "PauseScene"; }
// };

// class GameOverSceneFactory : public SceneFactory {
// public:
//   std::unique_ptr<Scene> createScene() override;
//   const char *getSceneType() const override { return "GameOverScene"; }
// };

// class SettingsSceneFactory : public SceneFactory {
// public:
//   std::unique_ptr<Scene> createScene() override;
//   const char *getSceneType() const override { return "SettingsScene"; }
// };

