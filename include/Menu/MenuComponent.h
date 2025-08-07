#pragma once
#include <vector>
#include <memory>
#include <string>
#include <iostream>

// forward declaration
class ICommand;

/*
  Menu will use composite pattern to allow for
  a tree structure of menu items and submenus.
*/

// Interface for menu components
class MenuComponent {
public:
  virtual ~MenuComponent() = default;
  virtual void addItem(std::shared_ptr<MenuComponent> component) = 0;
  virtual void removeItem(std::shared_ptr<MenuComponent> component) = 0;
  virtual MenuComponent* findItem(const std::string& title) = 0;

  // Setters for title and text
  virtual void setTitle(const std::string& title) = 0;
  virtual void setText(const std::string& text) = 0;
  virtual std::string getTitle() const = 0;
  virtual std::string getText() const = 0;
  

  // enable
  virtual void setEnabled(bool enabled) = 0;
  virtual bool isEnabled() const = 0;

  
  virtual void setCommand(std::unique_ptr<ICommand> command) = 0;
  virtual std::unique_ptr<ICommand> cloneCommand() const = 0;
  virtual const std::vector<std::shared_ptr<MenuComponent>>& getChildren() const = 0;

  // virtual std::shared_ptr<MenuComponent> clone() const = 0;
};

// Concrete implementation of a menu item
class MenuItem : public MenuComponent {
private:
  std::string _title;
  std::string _text;
  bool _enabled;
  std::unique_ptr<ICommand> _command;

public:
  MenuItem(const std::string& title, bool enabled);
  void addItem(std::shared_ptr<MenuComponent> component) override;
  void removeItem(std::shared_ptr<MenuComponent> component) override;
  MenuComponent* findItem(const std::string& title) override;

  void setTitle(const std::string& title) override;
  void setText(const std::string& text) override;
  std::string getTitle() const override;
  std::string getText() const override;

  void setEnabled(bool enabled) override;
  bool isEnabled() const override;
  void setCommand(std::unique_ptr<ICommand> command) override;
  std::unique_ptr<ICommand> cloneCommand() const override;
  const std::vector<std::shared_ptr<MenuComponent>>& getChildren() const override;

  // std::shared_ptr<MenuComponent> clone() const override;
};

// Concrete implementation of a menu that can contain other menu items
class Menu : public MenuComponent {
private:
  std::string _title;
  std::string _text;
  bool _enabled;
  std::unique_ptr<ICommand> _command;
  std::vector<std::shared_ptr<MenuComponent>> _children;

public:
  Menu(const std::string& title, bool enabled);
  void addItem(std::shared_ptr<MenuComponent> component) override;
  void removeItem(std::shared_ptr<MenuComponent> component) override;
  MenuComponent* findItem(const std::string& title) override;

  void setTitle(const std::string& title) override;
  void setText(const std::string& text) override;
  std::string getTitle() const override;
  std::string getText() const override; 

  void setEnabled(bool enabled) override;
  bool isEnabled() const override;

  void setCommand(std::unique_ptr<ICommand> command) override;
  std::unique_ptr<ICommand> cloneCommand() const override;
  const std::vector<std::shared_ptr<MenuComponent>>& getChildren() const override;

  // std::shared_ptr<MenuComponent> clone() const override;
};