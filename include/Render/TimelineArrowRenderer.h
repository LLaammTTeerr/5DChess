#pragma once

#include "raylib.h"
#include <vector>
#include <memory>
#include <string>

// Forward declarations
class BoardView;

/// @brief Data structure for timeline arrow information passed from Controller
struct TimelineArrowData {
    std::shared_ptr<BoardView> fromBoardView;
    std::shared_ptr<BoardView> toBoardView;
    Color color;
    std::string arrowType; // "progression" or "branch"
    
    // Constructor to match usage in Controller.cpp
    TimelineArrowData(std::shared_ptr<BoardView> from, std::shared_ptr<BoardView> to, 
                     const std::string& type, Color col)
        : fromBoardView(from), toBoardView(to), arrowType(type), color(col) {}
};

/// @brief Internal arrow representation for rendering
struct TimelineArrow {
    Vector2 startPos;
    Vector2 endPos;
    Color color;
    float thickness;
    std::string type;
    bool isAnimated;
};

/// @brief Animation state for arrow effects
struct ArrowAnimationState {
    float animationTime = 0.0f;
    float dashOffset = 0.0f;
    float pulsePhase = 0.0f;
};

/// @brief Encapsulates all timeline arrow rendering logic
class TimelineArrowRenderer {
public:
    TimelineArrowRenderer();
    ~TimelineArrowRenderer() = default;

    /// @brief Update animation state
    void update(float deltaTime);

    /// @brief Update arrows from Controller-provided data
    void updateArrows(const std::vector<TimelineArrowData>& arrowData);

    /// @brief Render all timeline arrows
    void render(Camera2D* camera, bool isUsing3D) const;

    /// @brief Clear all arrows
    void clear();

private:
    /// @brief Generate arrows from Controller-provided data
    void generateArrowsFromData(const std::vector<TimelineArrowData>& arrowData);

    /// @brief Calculate arrow position on board edge
    Vector2 calculateArrowPosition(std::shared_ptr<BoardView> boardView, bool isStart) const;

    /// @brief Draw curved arrow for branching
    void drawCurvedArrow(Vector2 start, Vector2 end, Color color, float thickness, float animationOffset) const;

    /// @brief Draw animated dashed line for progression
    void drawAnimatedDashedLine(Vector2 start, Vector2 end, Color color, float thickness, float dashOffset) const;

private:
    std::vector<TimelineArrow> _arrows;
    ArrowAnimationState _animationState;
};
