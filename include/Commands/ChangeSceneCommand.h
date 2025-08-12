// #pragma once

// #include "ICommand.h"
// #include <string>

// // forward declaration
// class SceneManager;


// class ChangeSceneCommand : public ICommand {
// private:
//     SceneManager* m_sceneManager;
//     std::string m_targetSceneName;
//     std::string m_previousSceneName;
//     bool m_hasExecuted;

// public:
//     // Constructor
//     ChangeSceneCommand(SceneManager* sceneManager, const std::string& targetSceneName);
    
//     // Destructor
//     ~ChangeSceneCommand() override;

//     void execute() override; // -> SceneManager::changeScene(const std::string& sceneName);
//     void undo() override;
//     void redo() override;
//     std::string getName() const override;
//     bool canUndo() const override;
//     bool canRedo() const override;

//     std::unique_ptr<ICommand> clone() const override;
// };

