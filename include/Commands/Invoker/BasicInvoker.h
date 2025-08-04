#pragma once

#include <vector>
#include <memory>
#include <stack>
#include <string>
#include "IInvoker.h"

class ICommand;

class BasicInvoker : public IInvoker {
private:
  std::vector<std::unique_ptr<ICommand>> m_history;
  std::stack<std::unique_ptr<ICommand>> m_undoStack;
  int m_currentIndex = -1;
  static constexpr int MAX_HISTORY_SIZE = 100;

public:
  BasicInvoker();
  ~BasicInvoker() override;

  // Execute a command and add to history
  void executeCommand(std::unique_ptr<ICommand> command) override;
    
  // Undo/Redo operations
  bool undo() override;
  bool redo() override;

  // History queries
  bool canUndo() const override;
  bool canRedo() const override;
  size_t getHistorySize() const override;

  std::string getCurrentCommandName() const override;

  // Clear history
  void clearHistory() override;
  
  void trimHistory() override;
};

