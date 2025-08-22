#include "TimelineArrowRenderer.h"
#include "BoardView.h"
#include <iostream>
#include <cmath>
#include <algorithm>

TimelineArrowRenderer::TimelineArrowRenderer() {
    // Initialize animation state
    _animationState.animationTime = 0.0f;
    _animationState.dashOffset = 0.0f;
    _animationState.pulsePhase = 0.0f;
}

void TimelineArrowRenderer::update(float deltaTime) {
    _animationState.animationTime += deltaTime;
    _animationState.dashOffset += deltaTime * 50.0f; // Adjust speed as needed
    _animationState.pulsePhase += deltaTime * 2.0f;
}

void TimelineArrowRenderer::updateArrows(const std::vector<TimelineArrowData>& arrowData) {
    _arrows.clear();
    generateArrowsFromData(arrowData);
}

void TimelineArrowRenderer::render(Camera2D* camera, bool isUsing3D) const {
    if (isUsing3D) {
        // Skip arrow rendering in 3D mode for now
        return;
    }
    
    if (!camera) {
        std::cerr << "Camera is null in TimelineArrowRenderer::render!" << std::endl;
        return;
    }
    
    BeginMode2D(*camera);
    
    for (const auto& arrow : _arrows) {
        if (arrow.type == "progression") {
            // Draw animated dashed line for progression
            drawAnimatedDashedLine(arrow.startPos, arrow.endPos, arrow.color, arrow.thickness, _animationState.dashOffset);
        } else if (arrow.type == "branch") {
            // Draw curved arrow for branching
            drawCurvedArrow(arrow.startPos, arrow.endPos, arrow.color, arrow.thickness, _animationState.animationTime);
        }
    }
    
    EndMode2D();
}

void TimelineArrowRenderer::clear() {
    _arrows.clear();
}

void TimelineArrowRenderer::generateArrowsFromData(const std::vector<TimelineArrowData>& arrowData) {
    for (const auto& data : arrowData) {
        if (data.fromBoardView && data.toBoardView) {
            TimelineArrow arrow;
            arrow.startPos = calculateArrowPosition(data.fromBoardView, false); // end of from board
            arrow.endPos = calculateArrowPosition(data.toBoardView, true); // start of to board
            arrow.color = data.color;
            arrow.thickness = (data.arrowType == "branch") ? 5.0f : 4.0f;
            arrow.type = data.arrowType;
            arrow.isAnimated = true;
            
            _arrows.push_back(arrow);
        }
    }
}

Vector2 TimelineArrowRenderer::calculateArrowPosition(std::shared_ptr<BoardView> boardView, bool isStart) const {
    if (!boardView) return Vector2{0, 0};
    
    // Get board view center position
    Vector2 boardCenter = boardView->getBoardCenter();
    float boardSize = boardView->getBoardSize();
    
    if (isStart) {
        // Arrow starts from left side of board
        return Vector2{boardCenter.x - boardSize * 0.6f, boardCenter.y};
    } else {
        // Arrow ends at right side of board
        return Vector2{boardCenter.x + boardSize * 0.6f, boardCenter.y};
    }
}

void TimelineArrowRenderer::drawCurvedArrow(Vector2 start, Vector2 end, Color color, float thickness, float animationOffset) const {
    // Calculate control point for curved arrow
    Vector2 midPoint = {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
    Vector2 direction = {end.x - start.x, end.y - start.y};
    Vector2 perpendicular = {-direction.y, direction.x};
    
    // Normalize perpendicular vector
    float length = sqrtf(perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y);
    if (length > 0) {
        perpendicular.x /= length;
        perpendicular.y /= length;
    }
    
    // Create curved control point
    float curvature = 50.0f; // Adjust curve intensity
    Vector2 controlPoint = {
        midPoint.x + perpendicular.x * curvature,
        midPoint.y + perpendicular.y * curvature
    };
    
    // Draw curved line using multiple segments
    int segments = 20;
    for (int i = 0; i < segments; ++i) {
        float t1 = (float)i / segments;
        float t2 = (float)(i + 1) / segments;
        
        // Quadratic Bezier curve calculation
        Vector2 p1 = {
            (1 - t1) * (1 - t1) * start.x + 2 * (1 - t1) * t1 * controlPoint.x + t1 * t1 * end.x,
            (1 - t1) * (1 - t1) * start.y + 2 * (1 - t1) * t1 * controlPoint.y + t1 * t1 * end.y
        };
        Vector2 p2 = {
            (1 - t2) * (1 - t2) * start.x + 2 * (1 - t2) * t2 * controlPoint.x + t2 * t2 * end.x,
            (1 - t2) * (1 - t2) * start.y + 2 * (1 - t2) * t2 * controlPoint.y + t2 * t2 * end.y
        };
        
        // Add pulsing effect
        float pulse = 1.0f + 0.3f * sinf(_animationState.pulsePhase + t1 * 3.14159f);
        Color animatedColor = {
            (unsigned char)(color.r * pulse),
            (unsigned char)(color.g * pulse),
            (unsigned char)(color.b * pulse),
            color.a
        };
        
        DrawLineEx(p1, p2, thickness, animatedColor);
    }
    
    // Draw arrowhead at the end
    Vector2 arrowDir = {end.x - controlPoint.x, end.y - controlPoint.y};
    float arrowLength = sqrtf(arrowDir.x * arrowDir.x + arrowDir.y * arrowDir.y);
    if (arrowLength > 0) {
        arrowDir.x /= arrowLength;
        arrowDir.y /= arrowLength;
        
        Vector2 arrowSide = {-arrowDir.y, arrowDir.x};
        float arrowSize = thickness * 2;
        
        Vector2 arrowPoint1 = {
            end.x - arrowDir.x * arrowSize + arrowSide.x * arrowSize * 0.5f,
            end.y - arrowDir.y * arrowSize + arrowSide.y * arrowSize * 0.5f
        };
        Vector2 arrowPoint2 = {
            end.x - arrowDir.x * arrowSize - arrowSide.x * arrowSize * 0.5f,
            end.y - arrowDir.y * arrowSize - arrowSide.y * arrowSize * 0.5f
        };
        
        DrawTriangle(end, arrowPoint1, arrowPoint2, color);
    }
}

void TimelineArrowRenderer::drawAnimatedDashedLine(Vector2 start, Vector2 end, Color color, float thickness, float dashOffset) const {
    Vector2 direction = {end.x - start.x, end.y - start.y};
    float totalLength = sqrtf(direction.x * direction.x + direction.y * direction.y);
    
    if (totalLength == 0) return;
    
    direction.x /= totalLength;
    direction.y /= totalLength;
    
    float dashLength = 10.0f;
    float gapLength = 5.0f;
    float segmentLength = dashLength + gapLength;
    
    // Apply animation offset
    float animatedOffset = fmodf(dashOffset, segmentLength);
    
    for (float distance = -animatedOffset; distance < totalLength; distance += segmentLength) {
        float segmentStart = fmaxf(0, distance);
        float segmentEnd = fminf(totalLength, distance + dashLength);
        
        if (segmentStart < segmentEnd) {
            Vector2 lineStart = {
                start.x + direction.x * segmentStart,
                start.y + direction.y * segmentStart
            };
            Vector2 lineEnd = {
                start.x + direction.x * segmentEnd,
                start.y + direction.y * segmentEnd
            };
            
            DrawLineEx(lineStart, lineEnd, thickness, color);
        }
    }
    
    // Draw arrowhead at the end
    Vector2 arrowPoint1 = {
        end.x - direction.x * thickness * 2 + direction.y * thickness,
        end.y - direction.y * thickness * 2 - direction.x * thickness
    };
    Vector2 arrowPoint2 = {
        end.x - direction.x * thickness * 2 - direction.y * thickness,
        end.y - direction.y * thickness * 2 + direction.x * thickness
    };
    
    DrawTriangle(end, arrowPoint1, arrowPoint2, color);
}
