#include "Render/PresentLineRenderer.h"
#include "Render/BoardView.h"
#include "Render/UITheme.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

// External constants (should be defined in BoardView.cpp)
extern const float BOARD_WORLD_SIZE;
extern const float HORIZONTAL_SPACING;
extern const float VERTICAL_SPACING;

PresentLineRenderer::PresentLineRenderer() 
    : _animationTime(0.0f), _hasData(false) {
}

void PresentLineRenderer::update(float deltaTime) {
    _animationTime += deltaTime;
}

void PresentLineRenderer::updatePresentLine(const PresentLineData& lineData) {
    _lineData = lineData;
    _hasData = true;
}

void PresentLineRenderer::render(Camera2D* camera, bool isUsing3D, const std::vector<std::shared_ptr<BoardView>>& boardViews) const {
    if (!_hasData || !_lineData.isVisible || boardViews.empty()) {
        return;
    }

    if (isUsing3D) {
        // Skip present line rendering in 3D mode for now
        return;
    }
    
    if (!camera) {
        std::cerr << "Camera is null in PresentLineRenderer::render!" << std::endl;
        return;
    }
    
    BeginMode2D(*camera);

    // Calculate the x position based on half turn - position it at the center of the board
    float xPosition = _lineData.halfTurnPosition * (BOARD_WORLD_SIZE + HORIZONTAL_SPACING) + (BOARD_WORLD_SIZE / 2.0f);
    
    // Calculate the vertical bounds of the line
    auto [yStart, yEnd] = calculateLineBounds(boardViews);
    
    // Add much more padding to make it longer
    float padding = BOARD_WORLD_SIZE * 1.5f;
    yStart -= padding;
    yEnd += padding;
    
    // Debug output (can be removed later)
    #ifdef DEBUG_PRESENT_LINE
    std::cout << "Present line - Half turn: " << _lineData.halfTurnPosition 
              << ", X pos: " << xPosition 
              << ", Y range: [" << yStart << ", " << yEnd << "]" 
              << ", Board count: " << boardViews.size() << std::endl;
    #endif
    
    // Draw the animated present line
    drawAnimatedPresentLine(xPosition, yStart, yEnd, _lineData.color, _lineData.thickness, _animationTime, camera->zoom);
    
    EndMode2D();
}

std::pair<float, float> PresentLineRenderer::calculateLineBounds(const std::vector<std::shared_ptr<BoardView>>& boardViews) const {
    if (boardViews.empty()) {
        return {0.0f, 0.0f};
    }
    
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    
    for (const auto& boardView : boardViews) {
        if (!boardView) continue;
        
        Rectangle renderArea = boardView->getArea();
        minY = std::min(minY, renderArea.y);
        maxY = std::max(maxY, renderArea.y + renderArea.height);
    }
    
    return {minY, maxY};
}

void PresentLineRenderer::drawAnimatedPresentLine(float x, float yStart, float yEnd, Color color, float thickness, float /*animationOffset*/, float zoom) const {
    // thickness is in screen pixels; convert to world units so the line stays crisp at any zoom
    const float px = 1.0f / std::max(zoom, 0.05f);
    const float core = thickness * px;
    // One faint glow step, then the solid core
    DrawLineEx({x, yStart}, {x, yEnd}, core + 4.0f * px, UI::withAlpha(color, 40));
    DrawLineEx({x, yStart}, {x, yEnd}, core, color);
}

void PresentLineRenderer::clear() {
    _hasData = false;
}
