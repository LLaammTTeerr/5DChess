#include "chess.h"

std::shared_ptr<TimeLine> Multiverse::createBranch(
  std::shared_ptr<TimeLine> parent,
  int forkTurn
) {
  auto branch = std::make_shared<TimeLine>(_timeLines.size(), forkTurn, parent);
  branch->pushBoard(*parent->boardAtTurn(forkTurn));
  _timeLines.push_back(branch);
  return branch;
}

void Multiverse::addMoveToQueue(const Move& move) {
  // You can only make a move from the present mark
  // This mean that the move is make from the latest board of this timeline
  Board current = *move.fromTimeLine()->latestBoard();

  // The move is made from the present mark, so we need to update the board
  // and the turn number
  current.setPreviousBoard(move.fromTimeLine()->latestBoard());
  current.setTurnNumber(current.turnNumber() + 1);
  current.placePiece(move.from(), nullptr);


  // Push to the timeline
  move.fromTimeLine()->pushBoard(current);

  // If the move is a fork (time travelling to the past), we need to create a branch
  // from the parent timeline at the target turn
  if (move.targetTurn() < _present) {
    // Create a branch from the parent timeline at the target turn
    std::shared_ptr<TimeLine> branch = createBranch(move.toTimeLine(), move.targetTurn());
    
    // Place the piece on the branch's latest board
    branch->latestBoard()->placePiece(move.to(), move.piece());
  } else {
    // Edge case where the move is a normal move in the present
    // We need to ensure that the timeline is updated correctly
    if (move.fromTimeLine() != move.toTimeLine())
      move.toTimeLine()->pushBoard();
    move.toTimeLine()->latestBoard()->placePiece(move.to(), move.piece());
  }

  // Move present mark to the next turn
  _present = move.targetTurn() + 1;
}