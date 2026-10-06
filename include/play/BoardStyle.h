#pragma once
#include <raylib.h>
#include "play/BoardView.h"
#include "ui/Widgets.h"

namespace play {

/// One board view as data: the palette and a handful of "kinds" that pick how background, lanes, the present, board
/// cards and branch connectors are drawn. The renderer (BoardScene, BoardRenderer, TimelineArrows, Hud) is the same for
/// every view; it only switches on these fields.
struct BoardStyle {
  BoardView id;

  enum class Background { Paper, Space, Plain } background;      // cream + dots / indigo gradient + stars / flat
  enum class Lane { Band, Thread, Hairline } lane;               // tinted bands / glowing threads / hairlines
  enum class Present { Glow, Aurora, Rule } present;             // warm glow column / aurora / two accent rules
  enum class Card { Paper, Glow, Ink } card;                     // soft shadow + frame / outer glow / ink outline
  enum class Connector { Curve, Luminous, Elbow } connector;     // bezier / glowing bezier / orthogonal subway line
  bool grayPieces = false;                                       // Blueprint draws the piece sprites in greyscale

  // Background: Space uses bg0 (centre) -> bg1 -> bg2 (edge); Paper and Plain use bg0. `grid`: Paper dots / Plain guides.
  Color bg0, bg1, bg2, grid;

  // Text and accents. accent: the present, the selection; accent2: the second accent (selection outline, mandatory glow).
  Color ink, muted, accent, accent2, check;
  Color whiteBranch, blackBranch; // connectors, coloured by the player who created the timeline
  Color thread;                   // the line through the boards of a timeline
  Color laneTint[4], lanePill[4]; // Band lanes: tint and label colour, picked by timeline id modulo 4

  Color squareLight, squareDark, targetDot, selectTint, checkTint;
  Color cardWhite, cardBlack;           // card fill when White / Black is to move on the board
  Color cardEdgeWhite, cardEdgeBlack;   // card frame
  Color glowWhite, glowBlack;           // Glow cards: warm / cool halo
  Color mandatory, optional;            // frame colour of boards that must / may be moved on

  // The in-game HUD (status pill, controls bar, turn banner) and the action buttons
  Color hudFill, hudBorder, hudShadow, hudText, hudMuted, hudHint;
  bool hudSquare = false;
  ui::Skin skin;
};

const BoardStyle& boardStyle(BoardView view);

} // namespace play
