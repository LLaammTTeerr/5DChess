#pragma once
#include <string>
#include <memory>
#include <iostream>
/*
  Menu will use composite pattern to allow for
  a tree structure of menu items and submenus.
*/

// interface for menu components
class MenuComponent {
public:
  virtual ~MenuComponent() = default;
  virtual void add(std::shared_ptr<MenuComponent> component) = 0;
  virtual void handleInput() = 0;
  virtual std::string getName() const = 0;
};


class ICommand; // Forward declaration
class MenuItem : public MenuComponent {
private:
  std::string name;
  std::unique_ptr<ICommand> command;

public:
  MenuItem(const std::string& name, std::unique_ptr<ICommand> cmd);
  void add(std::shared_ptr<MenuComponent> component) override;
  void handleInput() override;
  std::string getName() const override;
};

class Menu : public MenuComponent {
private:
  std::string name;
  std::unique_ptr<ICommand> command;
  std::vector<std::shared_ptr<MenuComponent>> components;

public:
  Menu(const std::string& name);
  void add(std::shared_ptr<MenuComponent> component) override;
  void handleInput() override;
  std::string getName() const override;
  void run();
};
