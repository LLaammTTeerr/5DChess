#pragma once
#include <raylib.h>
#include <iostream>

class Board;
class ThemeManager;

class SingleBoardRenderer {
private:
  Board* _board;
  Texture2D* _boardTexture;
  std::pair<float, float> _boardPosition;
  float _boardSize;
  // float _scale;
  ThemeManager& _themeManager;

public:
  SingleBoardRenderer(Board* board, ThemeManager& themeManager);
  
  void render() const;
  

  /* setter */
  void setPosition(std::pair<float, float> position);
  void setSize(float size);
  void setBoard(Board* board);
  // void setScale(float scale);
  void setBoardTexture(Texture2D* texture);

  /* getter */
  std::pair<float, float> getPosition() const;
  float getSize() const;

  // /* Highlight specific squares */
  // void highlightSquare(int x, int y, bool enable, const Color& color = Color::Default);
  // void clearHighlights();

  // /* Animate piece movement */
  // void animatePieceMove(int fromX, int fromY, int toX, int toY, float duration);
};