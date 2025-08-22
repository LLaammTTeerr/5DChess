#pragma once

#include "raylib.h"
#include "RenderUtilis.h"
#include <vector>
#include <memory>

// Forward declarations
class BoardView;

/// @brief Encapsulates all present line rendering logic
class PresentLineRenderer {
public:
    PresentLineRenderer();
    ~PresentLineRenderer() = default;

    /// @brief Update animation state
    void update(float deltaTime);

    /// @brief Update present line from Controller-provided data
    void updatePresentLine(const PresentLineData& lineData);

    /// @brief Render the present line behind all boards
    void render(Camera2D* camera, bool isUsing3D, const std::vector<std::shared_ptr<BoardView>>& boardViews) const;

    /// @brief Clear the present line
    void clear();

private:
    /// @brief Calculate the world bounds for the present line based on existing boards
    std::pair<float, float> calculateLineBounds(const std::vector<std::shared_ptr<BoardView>>& boardViews) const;

    /// @brief Draw animated present line with subtle effects
    void drawAnimatedPresentLine(float x, float yStart, float yEnd, Color color, float thickness, float animationOffset) const;

private:
    PresentLineData _lineData;
    float _animationTime;
    bool _hasData;
};
