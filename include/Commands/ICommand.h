#pragma once

#include <string>
#include <memory> 
#include <functional>
#include <vector>
// Command types for execution flow control
enum class CommandType {
    STATE_CHANGING,    // Commands that change game state (break execution)
    NON_STATE,        // Commands that don't change state (can continue)
    IMMEDIATE         // Commands that need immediate execution (break but don't change state)
};

// Interface
class ICommand {
public:
    virtual ~ICommand() = default;

    // Execute the command
    virtual void execute() = 0;

    // Undo the command
    virtual void undo() = 0;

    // Redo the command
    virtual void redo() = 0;

    // Get the name of the command
    virtual std::string getName() const = 0;

    // Check if the command can be undone
    virtual bool canUndo() const = 0;

    // Check if the command can be redone
    virtual bool canRedo() const = 0;

    // Clone the command (for undo/redo purposes)
    virtual std::unique_ptr<ICommand> clone() const = 0;
    
    // Get command type for execution flow control
    virtual CommandType getType() const = 0;

protected:
    std::function<void()> _callback;

public:
    // Set a callback to be executed after the command is executed
    void setCallback(std::function<void()> callback) {
        _callback = std::move(callback);
    }

    // Execute the callback if set
    void executeCallback() const {
        if (_callback) {
            _callback();
        }
    }
};