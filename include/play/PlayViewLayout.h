#pragma once
#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>
#include "chess.h"
#include "play/BoardLayout.h"
#include "play/MultiverseView.h"

namespace play {

/// Play view: a second way to show the multiverse (README, Features -> Play view). One big card per ACTIVE timeline, showing that
/// timeline's present board, in a grid that makes the squares as large as the free rectangle allows; beside every card a stack of
/// its history; the inactive timelines in a collapsed row; and one more cell at the end of the grid, the *inspector*, that shows
/// whatever is not a card: the past boards a lifted piece can jump to (the docked target), and the history of a timeline the player
/// asked to read. No graphics dependency, so every number here is unit tested; play/PlayView draws it.
///
/// Everything is in screen pixels. The caller hands over the free rectangle (the window minus the HUD and any side panel); the layout
/// never leaves it.
namespace pv {

inline constexpr float kMaxBoard = 288.0f;  // the largest board the grid chooses on its own: 36 px squares on 8x8 (2 timelines are not drawn 50 px per square), 57 on 5x5
inline constexpr float kMinSquare = 16.0f; // a Pixel sprite's native size; below it the grid scrolls instead of shrinking further
inline constexpr float kCellGap = 6.0f;    // between cells
inline constexpr float kInner = 4.0f;      // between a cell's history column and its card
inline constexpr float kMargin = 12.0f;    // the free rectangle's side margin (PlayScreen)
inline constexpr float kInactiveRowH = 26.0f;

/// The pixel size of one cell for a square of `square` px on a board of `dim` x `dim`. The card is the BoardLayout card (the board plus
/// kCardPad all round and kCardFooter below it) scaled by `k` = board / BoardLayout::kBoardSize, so the label strip shrinks with the board.
struct CellSize {
  float board = 0, k = 1, cardW = 0, cardH = 0, histW = 0, cellW = 0, cellH = 0;
};
CellSize cellSize(float square, int dim);

/// Where the cells of a grid sit (relative to the area's top-left corner when `scrolls`, else inside the area).
struct Grid {
  int cols = 0, rows = 0;
  float square = 0;            // px per square (a whole number)
  CellSize size;
  std::vector<Rect> cells;     // cell i: column i % cols, row i / cols; the grid is centred in the area
  float contentHeight = 0;     // all rows together; larger than the area's height when the grid scrolls
  bool scrolls = false;
};
/// The grid for `count` cells of `dim` x `dim` boards in `area` that maximises the square size (capped at maxSquare; 0: kMaxBoard / dim): the number of
/// columns is tried from 1 to `count`. When even `minSquare` does not fit every row, the squares stay at minSquare and the grid scrolls.
Grid gridFor(const Rect& area, int count, int dim, float maxSquare = 0.0f, float minSquare = kMinSquare);

/// The groups the cards are ordered in: boards that must move, boards that may, the rest (waiting / ahead).
enum class Group { MustMove, Optional, Waiting };
struct OrderKey {
  int timeline = 0;
  Group group = Group::Waiting;
};
/// Timeline ids ordered by group, and by id (White's timelines first) within a group.
std::vector<int> orderCards(std::vector<OrderKey> cards);

} // namespace pv

enum class Chip { MustMove, Optional, Waiting, Moved };
const char* chipLabel(Chip chip); // "MUST MOVE", "optional", "waiting", "moved"

/// One card of the grid: a timeline's present (latest) board.
struct PlayCard {
  int timeline = 0, halfTurn = 0;
  BoardRole role = BoardRole::Past;
  Chip chip = Chip::Waiting;
  bool whiteToMove = true;
  bool created = false;      // the timeline was branched off during this turn
  int history = 0;           // boards of this timeline before the shown one
  int firstHalfTurn = 0;
  Rect cell, histRect, card, board; // screen pixels, scroll applied
};

/// A tab of the inspector's column: a board to look at.
struct InspectorTab {
  std::pair<int, int> key{0, 0};
  int targets = 0;           // Targets mode: how many squares of this board the lifted piece can reach
  bool current = false;
  Rect rect;
};

/// A collapsed inactive timeline: a chip in the row at the bottom.
struct InactiveChip {
  int timeline = 0, halfTurn = 0;
  Rect rect;
};

class PlayViewLayout {
public:
  using Key = std::pair<int, int>; // (timeline, half-turn) of a board

  // ---- the data: rebuilt when the game's state changes ----
  void reset();
  /// Reads the timelines of `view`. The order of the cards is frozen for the turn (`game.history().size()`): it is chosen from the boards'
  /// roles when the turn starts and only changes by appending a timeline branched off during the turn and dropping one that is gone (undo)
  /// or inactive. Roles and boards update in place.
  void sync(const Chess::IGame& game, const MultiverseView& view);
  /// Chips (and "moved") are shown only while the player has a turn to play.
  void setTurnActive(bool active) { _turnActive = active; }
  bool turnActive() const { return _turnActive; }

  // ---- the geometry: recomputed every frame (cheap) ----
  /// Places everything inside `area` (screen pixels). `dim`: the boards' size.
  void place(const Rect& area, int dim);
  const Rect& area() const { return _area; }
  int dim() const { return _dim; }
  const pv::Grid& grid() const { return _grid; }

  const std::vector<PlayCard>& cards() const { return _cards; }
  const PlayCard* card(int timeline) const;
  bool anyPlayable() const;
  /// How many cards carry each chip (the summary line above the grid).
  struct Counts {
    int must = 0, optional = 0, waiting = 0, moved = 0;
  };
  Counts counts() const;

  // ---- the inspector (the grid's last cell) ----
  enum class Mode { Empty, Targets, History };
  /// What the lifted piece can reach: boards that are not cards go to the inspector. nullopt `from`: nothing is lifted.
  void setSelection(const std::optional<Chess::Core::Coord>& from, const std::vector<Chess::Core::Coord>& targets);
  /// Show the history of `timeline` in the inspector (its previous board first); Targets mode wins while a piece with such targets is lifted.
  void browse(int timeline);
  /// Show one board of any timeline (an inactive timeline's chip).
  void browseBoard(int timeline, int halfTurn);
  void closeBrowse();
  /// A move just made landed on `timeline`'s new board: when that timeline turns out to be inactive (it has no card) the next sync() shows
  /// the board in the inspector, so the player sees where the piece went.
  void noteMove(int timeline) { _justMoved = timeline; }
  /// One board older (-1) / newer (+1) in History mode.
  void stepBrowse(int direction);
  /// Pick the board shown in Targets mode (nullopt: the first).
  void pickTarget(Key key);
  Mode mode() const { return _mode; }
  std::optional<Key> inspectorKey() const { return _inspectKey; }
  std::optional<int> browsingTimeline() const { return _browse ? std::optional<int>(_browse->first) : std::nullopt; }
  const std::vector<InspectorTab>& tabs() const { return _tabs; }
  const Rect& inspectorCell() const { return _inspector.cell; }
  const Rect& inspectorCard() const { return _inspector.card; }
  const Rect& inspectorBoard() const { return _inspector.board; }
  const Rect& inspectorTabs() const { return _inspector.hist; }
  const Rect& closeRect() const { return _closeRect; } // History mode: the "close" button above the tabs
  const Rect& gridArea() const { return _gridArea; }   // what the cards are clipped to
  const std::vector<InactiveChip>& inactive() const { return _inactive; }
  const Rect& inactiveRow() const { return _inactiveRow; }
  /// The strip above the grid that says how many boards need a move (screen pixels; empty when there is no room).
  const Rect& summaryRect() const { return _summary; }

  // ---- scrolling ----
  bool scrolls() const { return _grid.scrolls; }
  void scrollBy(float dy);
  float scroll() const { return _scroll; }
  /// Scroll so that `rect` (screen pixels, a card) is wholly inside the area.
  void reveal(const Rect& rect);
  void scrollTabs(int steps) { _tabScroll = std::max(0, _tabScroll + steps); }

  // ---- looking things up ----
  struct Hit {
    enum class Kind { None, Square, Chrome, History, Tab, Close, Inactive } kind = Kind::None;
    Chess::Core::Coord square{};   // Square
    Key key{0, 0};                 // Square / Chrome: the board; Tab / Inactive: the board it shows
    int timeline = 0;              // History: the timeline whose stack it is
  };
  Hit hit(float x, float y) const;
  /// The screen rectangle of a board that is on screen, as a card (the timeline's present board) or in the inspector.
  std::optional<Rect> boardRect(Key key) const;
  std::optional<Rect> cardRect(Key key) const;
  /// A square of such a board, or of a timeline's present board (a move's destination, whatever half-turn the engine gave it).
  std::optional<Rect> squareRect(const Chess::Core::Coord& c) const;
  std::optional<Rect> squareOnTimeline(int timeline, int x, int y) const;
  /// Is the board one of the grid's cards (its timeline's latest)?
  bool isCard(Key key) const;

  /// Arrow keys: the card in that direction of the grid (by row and column); the last card for Down past the end of a short row.
  std::optional<int> neighbour(int timeline, BoardLayout::Dir dir) const;

private:
  struct Cell {
    Rect cell, hist, card, board;
  };
  bool _turnActive = true;
  bool _seeded = false;
  size_t _turnKey = 0;
  std::vector<int> _order;             // timeline ids, frozen for the turn
  std::map<int, int> _start;           // timeline -> latest half-turn when the turn started
  std::vector<PlayCard> _cards;
  std::vector<InactiveChip> _inactive;
  std::map<int, std::pair<int, int>> _span; // timeline -> (first, last) half-turn, for the History tabs
  std::map<int, bool> _activeOf;

  Rect _area, _summary, _inactiveRow, _gridArea, _closeRect;
  int _dim = 8;
  pv::Grid _grid;
  float _scroll = 0.0f;
  int _tabScroll = 0;
  Cell _inspector;

  // inspector state
  std::optional<Key> _browse;                       // History mode: the board shown
  std::optional<int> _justMoved;
  std::optional<Key> _pick;                         // Targets mode: the tab the player chose
  std::vector<std::pair<Key, int>> _targetBoards;   // boards that are not cards, with their target counts, nearest first
  Mode _mode = Mode::Empty;
  std::optional<Key> _inspectKey;
  std::vector<InspectorTab> _tabs;

  void placeCards();
  void resolveInspector();
  void placeTabs();
  static Cell makeCell(const Rect& cell, const pv::CellSize& size);
};

} // namespace play
