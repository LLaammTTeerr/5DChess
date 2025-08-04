#include "BasicInvoker.h"
#include "ICommand.h"


BasicInvoker::BasicInvoker() : m_currentIndex(-1) {}
BasicInvoker::~BasicInvoker() = default;

void BasicInvoker::executeCommand(std::unique_ptr<ICommand> command) {
    if (!command) {
        return;
    }
    
    // Execute the command
    command->execute();
    
    // Clear any commands after current index (when we're in middle of history)
    if (m_currentIndex + 1 < static_cast<int>(m_history.size())) {
        m_history.erase(m_history.begin() + m_currentIndex + 1, m_history.end());
    }
    
    // Add command to history
    m_history.push_back(std::move(command));
    m_currentIndex = static_cast<int>(m_history.size()) - 1;
    
    // Trim history if it exceeds maximum size
    trimHistory();
}

bool BasicInvoker::undo() {
    if (!canUndo()) {
        return false;
    }
    
    // Get the command to undo
    auto& command = m_history[m_currentIndex];
    if (command && command->canUndo()) {
        command->undo();
        m_currentIndex--;
        return true;
    }
    
    return false;
}

bool BasicInvoker::redo() {
    if (!canRedo()) {
        return false;
    }
    
    // Move to next command and redo it
    m_currentIndex++;
    auto& command = m_history[m_currentIndex];
    if (command && command->canRedo()) {
        command->redo();
        return true;
    }
    
    // If redo failed, revert the index
    m_currentIndex--;
    return false;
}

bool BasicInvoker::canUndo() const {
    return m_currentIndex >= 0 && 
           m_currentIndex < static_cast<int>(m_history.size()) &&
           m_history[m_currentIndex] &&
           m_history[m_currentIndex]->canUndo();
}

bool BasicInvoker::canRedo() const {
    int nextIndex = m_currentIndex + 1;
    return nextIndex >= 0 && 
           nextIndex < static_cast<int>(m_history.size()) &&
           m_history[nextIndex] &&
           m_history[nextIndex]->canRedo();
}

size_t BasicInvoker::getHistorySize() const {
    return m_history.size();
}

void BasicInvoker::clearHistory() {
    m_history.clear();
    m_currentIndex = -1;
}

std::string BasicInvoker::getCurrentCommandName() const {
    if (canUndo()) {
        return m_history[m_currentIndex]->getName();
    }
    return "No Command Available";
}

void BasicInvoker::trimHistory() {
    // Erase from the front if history exceeds max size
    while (m_history.size() > MAX_HISTORY_SIZE) {
        m_history.erase(m_history.begin());
        m_currentIndex--;
    }
    // Ensure current index is valid
    if (m_currentIndex < 0) {
        m_currentIndex = -1;
        m_history.clear();
    }
    else if (m_currentIndex >= static_cast<int>(m_history.size())) {
        m_currentIndex = static_cast<int>(m_history.size()) - 1;
    }
}

