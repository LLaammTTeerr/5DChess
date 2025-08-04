#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>

class ICommand;

class IInvoker {
public:
  virtual ~IInvoker() = default;

  // Execute a command and add to history
  virtual void executeCommand(std::unique_ptr<ICommand> command) = 0;

  // Undo/Redo operations
  virtual bool undo() = 0;
  virtual bool redo() = 0;

  // History queries
  virtual bool canUndo() const = 0;
  virtual bool canRedo() const = 0;
  virtual size_t getHistorySize() const = 0;

  // Clear history
  virtual void clearHistory() = 0;
  
  // Get the current command name without executing
  virtual std::string getCurrentCommandName() const = 0;

  virtual void trimHistory() = 0;
};