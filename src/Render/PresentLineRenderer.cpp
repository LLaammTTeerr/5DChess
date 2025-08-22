#include "Render/PresentLineRenderer.h"
#include "Render/BoardView.h"
#include <algorithm>
#include <cmath>

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
    
    // Debug output for positioning
    std::cout << "Present Line Debug: halfTurnPosition=" << _lineData.halfTurnPosition 
              << ", xPosition=" << xPosition 
              << ", BOARD_WORLD_SIZE=" << BOARD_WORLD_SIZE 
              << ", HORIZONTAL_SPACING=" << HORIZONTAL_SPACING << std::endl;
    
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
    drawAnimatedPresentLine(xPosition, yStart, yEnd, _lineData.color, _lineData.thickness, _animationTime);
    
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

void PresentLineRenderer::drawAnimatedPresentLine(float x, float yStart, float yEnd, Color color, float thickness, float animationOffset) const {
    // Create a subtle pulsing effect
    float pulseIntensity = 0.9f + 0.1f * sinf(animationOffset * 1.5f);
    Color animatedColor = {
        static_cast<unsigned char>(color.r * pulseIntensity),
        static_cast<unsigned char>(color.g * pulseIntensity), 
        static_cast<unsigned char>(color.b * pulseIntensity),
        200  // More opaque to be more visible
    };
    
    // Make the line much wider by drawing multiple layers
    float extraWidth = thickness * 4.0f; // Make it 4x wider than specified
    
    // Draw a wide background glow (outermost layer)
    Color outerGlowColor = {animatedColor.r, animatedColor.g, animatedColor.b, 20};
    DrawLineEx({x, yStart}, {x, yEnd}, extraWidth * 2.0f, outerGlowColor);
    
    // Draw middle glow layer
    Color middleGlowColor = {animatedColor.r, animatedColor.g, animatedColor.b, 60};
    DrawLineEx({x, yStart}, {x, yEnd}, extraWidth * 1.5f, middleGlowColor);
    
    // Draw inner glow layer
    Color innerGlowColor = {animatedColor.r, animatedColor.g, animatedColor.b, 100};
    DrawLineEx({x, yStart}, {x, yEnd}, extraWidth, innerGlowColor);
    
    // Draw main line (core)
    DrawLineEx({x, yStart}, {x, yEnd}, thickness, animatedColor);
    
    // Add larger, more visible markers along the line
    float markerSpacing = BOARD_WORLD_SIZE + VERTICAL_SPACING;
    for (float y = yStart + BOARD_WORLD_SIZE; y <= yEnd - BOARD_WORLD_SIZE; y += markerSpacing) {
        // Larger diamond-shaped markers for better visibility
        float markerSize = 12.0f;
        Vector2 center = {x, y};
        
        // Draw diamond outline
        Vector2 top = {center.x, center.y - markerSize};
        Vector2 right = {center.x + markerSize, center.y};
        Vector2 bottom = {center.x, center.y + markerSize};
        Vector2 left = {center.x - markerSize, center.y};
        
        // Draw filled diamond
        DrawTriangle(top, right, center, animatedColor);
        DrawTriangle(right, bottom, center, animatedColor);
        DrawTriangle(bottom, left, center, animatedColor);
        DrawTriangle(left, top, center, animatedColor);
        
        // Add bright center highlight
        Color highlightColor = {255, 255, 255, 150};
        DrawCircle(static_cast<int>(x), static_cast<int>(y), markerSize * 0.3f, highlightColor);
    }
}

void PresentLineRenderer::clear() {
    _hasData = false;
}
