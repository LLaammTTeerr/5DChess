#pragma once
#include <string>
// abstract class
class Scene {
public:
  virtual ~Scene(void) = default;
  virtual void init(void) = 0;
  virtual void update(float deltaTime) = 0;
  virtual void render() = 0;
  virtual void cleanup() = 0;

  // Scene state management
  virtual bool isActive(void) const = 0;
  virtual std::string getName(void) const = 0;

  // Scene transition methods
  virtual void onEnter(void) = 0;
  virtual void onExit(void) = 0;

  virtual bool shouldTransition(void) const { return false; }

  // Resource management
  virtual void loadResources(void) = 0;  // Load scene-specific resources
  virtual void unloadResources(void) = 0;  // Unload when scene ends

protected:
  bool _isActive = false;
  float _transitionTimer = 0.0f;
};
