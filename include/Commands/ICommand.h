#pragma once

#include <string>
#include <memory> 

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
};