#include "SingleBoardRenderer.h"
#include "PieceTheme.h"
#include "chess.h"
#include "PieceRenderer.h"
// #include "Board.h"

SingleBoardRenderer::SingleBoardRenderer(Board* board, ThemeManager& themeManager) 
  : _board(board), _themeManager(themeManager) {
  _boardTexture = nullptr;
  _boardPosition = {0.0f, 0.0f};
  _boardSize = 1.0f;
  // _scale = 1.0f;
} 

void SingleBoardRenderer::setPosition(std::pair<float, float> position) {
  _boardPosition = position;
}

void SingleBoardRenderer::setSize(float size) {
  _boardSize = size;
}

void SingleBoardRenderer::setBoard(Board* board) {
  _board = board;
}

// void SingleBoardRendering::setScale(float scale) {
//   _scale = scale;
// }

void SingleBoardRenderer::setBoardTexture(Texture2D* texture) {
  _boardTexture = texture;
}

std::pair<float, float> SingleBoardRenderer::getPosition() const {
  return _boardPosition;
}

float SingleBoardRenderer::getSize() const {
  return _boardSize;
}

void SingleBoardRenderer::render() const {
  static std::vector<std::string> pieceNames = {
    "pawn", "knight", "bishop", "rook", "queen", "king"
  };

  // Render the board using _boardTexture, _boardPosition, _boardSize, and _scale
  if (_boardTexture) {
    DrawTextureEx(*_boardTexture, { _boardPosition.first, _boardPosition.second }, 0.0f, _scale, WHITE);
  }
  else {
    // Draw a rectangle to simulate a board
    DrawRectangle(_boardPosition.first, _boardPosition.second, _boardSize, _boardSize, BLUE);
    // Draw 8x8 grid lines
    for (int i = 0; i <= 8; ++i) {
      DrawLine(_boardPosition.first + i * (_boardSize / 8), _boardPosition.second, 
               _boardPosition.first + i * (_boardSize / 8), _boardPosition.second + _boardSize, BLACK);
      DrawLine(_boardPosition.first, _boardPosition.second + i * (_boardSize / 8), 
               _boardPosition.first + _boardSize, _boardPosition.second + i * (_boardSize / 8), BLACK);
    }
  }
  
  // Draw the pieces
  if (_board) {
    for (const auto& piece : _board->getPieces()) {
      const std::string& pieceName = piece.getColor() +  piece.getName();
      // Alternatively, if you want to use the piece type and color:
      // const std::string& pieceName = (piece.color == PieceColor::BLACK ? "black" : "white") + "_" + pieceNames[static_cast<int>(piece.type())];
      Texture2D& texture = _themeManager.getPieceTexture(pieceName);
      Vector2 position = { _boardPosition.first + piece.getX() * (_boardSize / 8), 
                           _boardPosition.second + piece.getY() * (_boardSize / 8) };

      TexturePieceRenderer::render(texture, position, _boardSize / 8, _themeManager);
    }
  }
}