#include "ChangeSceneCommand.h"
#include "SceneManager.h"
#include "Scene.h"  


ChangeSceneCommand::~ChangeSceneCommand() = default; // Default destructor

ChangeSceneCommand::ChangeSceneCommand(SceneManager* sceneManager, const std::string& targetSceneName)
    : m_sceneManager(sceneManager), m_targetSceneName(targetSceneName), m_hasExecuted(false) {
    // Store current scene name for undo functionality
    if (m_sceneManager && m_sceneManager->getCurrentScene()) {
        m_previousSceneName = m_sceneManager->getCurrentScene()->getName(); // Assuming Scene has getName()
    }
}

void ChangeSceneCommand::execute() {
    if (!m_sceneManager) {
        return;
    }
    
    // Store the current scene name before changing (if not already stored)
    if (!m_hasExecuted && m_sceneManager->getCurrentScene()) {
        m_previousSceneName = m_sceneManager->getCurrentScene()->getName();
    }
    
    // Change to the target scene
    m_sceneManager->changeScene(m_targetSceneName);
    m_hasExecuted = true;
}

void ChangeSceneCommand::undo() {
    if (!m_sceneManager || !m_hasExecuted || m_previousSceneName.empty()) {
        return;
    }
    
    // Change back to the previous scene
    m_sceneManager->changeScene(m_previousSceneName);
}

void ChangeSceneCommand::redo() {
    if (!m_sceneManager || !m_hasExecuted) {
        return;
    }
    
    // Re-execute the command
    m_sceneManager->changeScene(m_targetSceneName);
}

std::string ChangeSceneCommand::getName() const {
    return "Change Scene to: " + m_targetSceneName;
}

bool ChangeSceneCommand::canUndo() const {
    return m_hasExecuted && !m_previousSceneName.empty() && m_sceneManager != nullptr;
}

bool ChangeSceneCommand::canRedo() const {
    return m_hasExecuted && m_sceneManager != nullptr;
}


std::unique_ptr<ICommand> ChangeSceneCommand::clone() const {
    return std::make_unique<ChangeSceneCommand>(m_sceneManager, m_targetSceneName);
}