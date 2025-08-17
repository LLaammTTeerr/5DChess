#include <raylib.h>
#include <string>
#include <vector>
#include <functional>
#include <iostream>

// Model: Manages to-do list data and notifies Controller
class TaskModel {
public:
    struct Task {
        std::string name;
        bool completed;
        Task(const std::string& n) : name(n), completed(false) {}
    };
private:
    std::vector<Task> tasks;
    std::function<void()> onTasksChanged;
public:
    void addTask(const std::string& taskName) {
        tasks.emplace_back(taskName);
        std::cout << "Task added: " << taskName << std::endl;
        notify();
    }
    
    void deleteTask(size_t index) {
        if (index < tasks.size()) {
            tasks.erase(tasks.begin() + index);
            notify();
        }
    }
    void toggleTaskCompletion(size_t index) {
        if (index < tasks.size()) {
            tasks[index].completed = !tasks[index].completed;
            notify();
        }
    }
    void clearTasks() {
        tasks.clear();
        std::cout << "All tasks cleared." << std::endl;
        notify();
    }
    const std::vector<Task>& getTasks() const {
        return tasks;
    }
    void setTasksChangedCallback(std::function<void()> callback) {
        onTasksChanged = callback;
    }
private:
    void notify() {
        if (onTasksChanged) onTasksChanged();
    }
};

// View: Uses Raylib to detect inputs and render UI
class TaskView {
public:
    struct TaskDisplayData {
        std::string displayText;
        bool isCompleted;
        size_t index;
    };
private:
    std::string inputText;
    std::function<void(const std::string&)> onAddButtonClick;
    std::function<void(size_t)> onDeleteButtonClick;
    std::function<void(size_t)> onToggleButtonClick;
    std::function<void()> onClearButtonClick;
    std::vector<TaskDisplayData> tasks; // For button collision detection
    std::vector<std::function<void()>> updateQueue; // Queue for pending updates
public:
    TaskView() : inputText("") {}
    void setAddButtonCallback(std::function<void(const std::string&)> callback) { onAddButtonClick = callback; }
    void setDeleteButtonCallback(std::function<void(size_t)> callback) { onDeleteButtonClick = callback; }
    void setToggleButtonCallback(std::function<void(size_t)> callback) { onToggleButtonClick = callback; }
    void setClearButtonCallback(std::function<void()> callback) { onClearButtonClick = callback; }

    void handleInputs() {
        // Text input for task name
        int key = GetCharPressed();
        if (key > 0) {
            if (key == '\b' && !inputText.empty()) {
                inputText.pop_back();
            } else if (key >= 32 && key <= 126 && inputText.length() < 20) {
                inputText += static_cast<char>(key);
            }
        }

        // Detect mouse clicks on buttons
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePos = GetMousePosition();

            // Add button (x: 10, y: 50, w: 100, h: 30)
            if (CheckCollisionPointRec(mousePos, {10, 50, 100, 30})) {
                if (onAddButtonClick && !inputText.empty()) {
                    onAddButtonClick(inputText);
                    inputText.clear();
                }
            }
            // Clear button (x: 120, y: 50, w: 100, h: 30)
            if (CheckCollisionPointRec(mousePos, {120, 50, 100, 30})) {
                if (onClearButtonClick) onClearButtonClick();
            }
            // Task buttons (Delete/Toggle for each task)
            for (size_t i = 0; i < tasks.size(); ++i) {
                // Delete button (x: 300, y: 100+i*30, w: 60, h: 20)
                if (CheckCollisionPointRec(mousePos, {300, float(100 + i * 30), 60, 20})) {
                    if (onDeleteButtonClick) onDeleteButtonClick(i);
                }
                // Toggle button (x: 370, y: 100+i*30, w: 60, h: 20)
                if (CheckCollisionPointRec(mousePos, {370, float(100 + i * 30), 60, 20})) {
                    if (onToggleButtonClick) onToggleButtonClick(i);
                }
            }
        }
    }

    // Queue an update instead of immediate execution
    void queueUpdate(const std::vector<TaskDisplayData>& newTasks) {
        updateQueue.push_back([this, newTasks]() {
            this->tasks = newTasks;
        });
    }
    
    // Process all queued updates just before rendering
    void processQueuedUpdates() {
        for (auto& update : updateQueue) {
            update();
        }
        updateQueue.clear();
    }

    void displayTasks(const std::vector<TaskDisplayData>& tasks) {
        this->tasks = tasks; // Store for button collision checks

        // Draw input field and text
        DrawRectangle(10, 10, 210, 30, LIGHTGRAY);
        DrawText(inputText.c_str(), 15, 15, 20, BLACK);

        // Draw Add and Clear buttons
        DrawRectangle(10, 50, 100, 30, SKYBLUE);
        DrawText("Add Task", 15, 55, 20, BLACK);
        DrawRectangle(120, 50, 100, 30, SKYBLUE);
        DrawText("Clear All", 125, 55, 20, BLACK);

        // Draw task list and buttons
        if (tasks.empty()) {
            DrawText("No tasks", 10, 100, 20, GRAY);
        } else {
            for (size_t i = 0; i < tasks.size(); ++i) {
                DrawText(tasks[i].displayText.c_str(), 10, 100 + i * 30, 20, BLACK);
                DrawRectangle(300, 100 + i * 30, 60, 20, RED);
                DrawText("Delete", 305, 100 + i * 30, 15, WHITE);
                DrawRectangle(370, 100 + i * 30, 60, 20, GREEN);
                DrawText("Toggle", 375, 100 + i * 30, 15, WHITE);
            }
        }
    }
};

// Controller: Handles inputs and mediates Model/View updates
class TaskController {
private:
    TaskModel& model;
    TaskView& view;
public:
    TaskController(TaskModel& m, TaskView& v) : model(m), view(v) {
        setupViewCallbacks();
        setupModelCallback();
    }
    
    void setupViewCallbacks() {
        // Bind button callbacks
        view.setAddButtonCallback([this](const std::string& taskName) {
            handleAddButtonClick(taskName);
        });
        view.setDeleteButtonCallback([this](size_t index) {
            handleDeleteButtonClick(index);
        });
        view.setToggleButtonCallback([this](size_t index) {
            handleToggleButtonClick(index);
        });
        view.setClearButtonCallback([this]() {
            handleClearButtonClick();
        });
    }
    
    void setupModelCallback() {
        // Bind Model's change callback to Controller - forwards to view's queue
        model.setTasksChangedCallback([this]() {
            auto displayData = convertToDisplayData(model.getTasks());
            view.queueUpdate(displayData);
        });
    }
    
    // Convert model data to display data
    std::vector<TaskView::TaskDisplayData> convertToDisplayData(const std::vector<TaskModel::Task>& modelTasks) {
        std::vector<TaskView::TaskDisplayData> displayData;
        for (size_t i = 0; i < modelTasks.size(); ++i) {
            TaskView::TaskDisplayData data;
            data.displayText = std::to_string(i) + ": " + modelTasks[i].name +
                              (modelTasks[i].completed ? " [Done]" : " [Pending]");
            data.isCompleted = modelTasks[i].completed;
            data.index = i;
            displayData.push_back(data);
        }
        return displayData;
    }
    
    void handleAddButtonClick(const std::string& taskName) {
        if (!taskName.empty()) {
            model.addTask(taskName);
        }
    }
    void handleDeleteButtonClick(size_t index) {
        model.deleteTask(index);
    }
    void handleToggleButtonClick(size_t index) {
        model.toggleTaskCompletion(index);
    }
    void handleClearButtonClick() {
        model.clearTasks();
    }
    
    // For initial render and direct rendering when safe
    // void handleModelChange() {
    //     view.displayTasks(model.getTasks());
    // }
};

// Main: Set up Raylib window and run MVC loop
// int main() {
//     InitWindow(450, 400, "To-Do List (MVC with Raylib)");
//     SetTargetFPS(60);

//     TaskModel model;
//     TaskView view;
//     TaskController controller(model, view);


//     while (!WindowShouldClose()) {
//         // Handle inputs BEFORE drawing
//         view.handleInputs(); // Process user inputs
        
//         // Process all queued updates in the view
//         view.processQueuedUpdates();
        
//         // Drawing phase - only rendering here
//         BeginDrawing();
//         ClearBackground(RAYWHITE);
        
//         // Render the current state of tasks
//         auto displayData = controller.convertToDisplayData(model.getTasks());
//         view.displayTasks(displayData);
//         EndDrawing();
//     }

//     CloseWindow();
//     return 0;
// }