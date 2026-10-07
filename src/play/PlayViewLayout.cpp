#include "play/PlayViewLayout.h"
#include <algorithm>
#include <cmath>

namespace play {

namespace pv {

CellSize cellSize(float square, int dim) {
  CellSize c;
  c.board = square * static_cast<float>(dim);
  c.k = c.board / BoardLayout::kBoardSize;
  c.cardW = c.board + 2.0f * BoardLayout::kCardPad * c.k;
  c.cardH = c.board + (2.0f * BoardLayout::kCardPad + BoardLayout::kCardFooter) * c.k;
  c.histW = std::clamp(std::round(2.0f * square), 44.0f, 64.0f);
  c.cellW = c.histW + kInner + c.cardW;
  c.cellH = c.cardH;
  return c;
}

namespace {
struct Fit {
  int cols = 0, rows = 0;
  float square = 0;
};

// The largest whole square that lets `cols` x rows cells fit; 0 when even minSquare does not
float largestSquare(const Rect& area, int cols, int rows, int dim, float maxSquare, float minSquare) {
  const float w = area.w - static_cast<float>(cols - 1) * kCellGap, h = area.h - static_cast<float>(rows - 1) * kCellGap;
  for (float s = std::floor(maxSquare); s >= std::ceil(minSquare); s -= 1.0f) {
    const CellSize c = cellSize(s, dim);
    if (c.cellW * static_cast<float>(cols) <= w && c.cellH * static_cast<float>(rows) <= h) return s;
  }
  return 0.0f;
}
} // namespace

Grid gridFor(const Rect& area, int count, int dim, float maxSquare, float minSquare) {
  Grid g;
  if (count <= 0 || dim <= 0) return g;
  if (maxSquare <= 0.0f) maxSquare = std::floor(kMaxBoard / static_cast<float>(dim));
  Fit best;
  for (int cols = 1; cols <= count; ++cols) {
    const int rows = (count + cols - 1) / cols;
    const float s = largestSquare(area, cols, rows, dim, maxSquare, minSquare);
    if (s <= 0.0f) continue;
    // the largest square wins; then the fewest empty cells, then the fewest rows
    const bool better = best.cols == 0 || s > best.square ||
                        (s == best.square && (cols * rows < best.cols * best.rows ||
                                              (cols * rows == best.cols * best.rows && rows < best.rows)));
    if (better) best = {cols, rows, s};
  }
  if (best.cols == 0) { // nothing fits at minSquare: keep it and scroll
    const CellSize c = cellSize(std::ceil(minSquare), dim);
    const int cols = std::clamp(static_cast<int>(std::floor((area.w + kCellGap) / (c.cellW + kCellGap))), 1, count);
    best = {cols, (count + cols - 1) / cols, std::ceil(minSquare)};
  }
  return gridWith(area, count, dim, best.cols, best.square);
}

Grid gridWith(const Rect& area, int count, int dim, int cols, float square) {
  Grid g;
  if (count <= 0 || dim <= 0 || cols <= 0) return g;
  const int rows = (count + cols - 1) / cols;
  g.cols = cols;
  g.rows = rows;
  g.square = square;
  g.size = cellSize(square, dim);
  const float totalW = static_cast<float>(g.cols) * g.size.cellW + static_cast<float>(g.cols - 1) * kCellGap;
  g.contentHeight = static_cast<float>(g.rows) * g.size.cellH + static_cast<float>(g.rows - 1) * kCellGap;
  g.scrolls = g.contentHeight > area.h + 0.5f;
  const float x0 = std::floor(area.x + std::max(0.0f, (area.w - totalW) / 2.0f));
  const float y0 = g.scrolls ? area.y : std::floor(area.y + std::max(0.0f, (area.h - g.contentHeight) / 2.0f));
  for (int i = 0; i < count; ++i) {
    const int col = i % g.cols, row = i / g.cols;
    g.cells.push_back({std::floor(x0 + static_cast<float>(col) * (g.size.cellW + kCellGap)),
                       std::floor(y0 + static_cast<float>(row) * (g.size.cellH + kCellGap)), std::ceil(g.size.cellW), std::ceil(g.size.cellH)});
  }
  return g;
}

std::vector<int> orderCards(std::vector<OrderKey> cards) {
  std::stable_sort(cards.begin(), cards.end(), [](const OrderKey& a, const OrderKey& b) {
    return a.group != b.group ? static_cast<int>(a.group) < static_cast<int>(b.group) : a.timeline > b.timeline;
  });
  std::vector<int> ids;
  for (const OrderKey& k : cards) ids.push_back(k.timeline);
  return ids;
}

} // namespace pv

const char* chipLabel(Chip chip) {
  switch (chip) {
    case Chip::MustMove: return "MUST MOVE";
    case Chip::Optional: return "optional";
    case Chip::Waiting: return "waiting";
    case Chip::Moved: return "moved";
  }
  return "";
}

// ---------------------------------------------------------------------------------------------------------------------
// The data

void PlayViewLayout::reset() {
  _seeded = false;
  _turnKey = 0;
  _order.clear();
  _start.clear();
  _cards.clear();
  _inactive.clear();
  _span.clear();
  _activeOf.clear();
  _browse.reset();
  _pick.reset();
  _targetBoards.clear();
  _mode = Mode::Empty;
  _inspectKey.reset();
  _tabs.clear();
  _scroll = 0.0f;
  _tabScroll = 0;
  _decided = false;
}

void PlayViewLayout::sync(const Chess::IGame& game, const MultiverseView& view) {
  const size_t turnKey = game.history().size();
  _span.clear();
  _activeOf.clear();
  for (const TimelineInfo& t : view.timelines) {
    _span[t.id] = {t.firstHalfTurn, t.lastHalfTurn};
    _activeOf[t.id] = t.active;
  }
  auto roleOf = [&](const TimelineInfo& t) {
    const BoardInfo* b = view.board(t.id, t.lastHalfTurn);
    return b ? b->role : BoardRole::Past;
  };

  if (!_seeded || turnKey != _turnKey) { // a new turn: choose the order from the boards' roles
    _seeded = true;
    _decided = false; // the grid's columns, square size and the inspector's place are chosen again for the new turn
    _turnKey = turnKey;
    _start.clear();
    std::vector<pv::OrderKey> keys;
    for (const TimelineInfo& t : view.timelines) {
      _start[t.id] = t.lastHalfTurn;
      if (!t.active) continue;
      const BoardRole role = roleOf(t);
      keys.push_back({t.id, role == BoardRole::Mandatory ? pv::Group::MustMove : role == BoardRole::Optional ? pv::Group::Optional : pv::Group::Waiting});
    }
    _order = pv::orderCards(std::move(keys));
  } else { // the same turn: nothing moves; a timeline that is gone or inactive drops out, a new one is appended
    _order.erase(std::remove_if(_order.begin(), _order.end(),
                                [&](int id) {
                                  const TimelineInfo* t = view.timeline(id);
                                  return !t || !t->active;
                                }),
                 _order.end());
    std::vector<int> added;
    for (const TimelineInfo& t : view.timelines)
      if (t.active && std::find(_order.begin(), _order.end(), t.id) == _order.end()) added.push_back(t.id);
    std::sort(added.begin(), added.end(), std::greater<int>());
    _order.insert(_order.end(), added.begin(), added.end());
  }

  _cards.clear();
  for (int id : _order) {
    const TimelineInfo* t = view.timeline(id);
    if (!t) continue;
    PlayCard c;
    c.timeline = id;
    c.halfTurn = t->lastHalfTurn;
    c.role = roleOf(*t);
    c.whiteToMove = c.halfTurn % 2 == 0;
    c.firstHalfTurn = t->firstHalfTurn;
    c.history = t->lastHalfTurn - t->firstHalfTurn;
    const auto start = _start.find(id);
    c.created = start == _start.end();
    const bool moved = c.created || start->second != c.halfTurn;
    c.chip = moved ? Chip::Moved : c.role == BoardRole::Mandatory ? Chip::MustMove : c.role == BoardRole::Optional ? Chip::Optional : Chip::Waiting;
    _cards.push_back(c);
  }

  _inactive.clear();
  for (const TimelineInfo& t : view.timelines)
    if (!t.active) _inactive.push_back({t.id, t.lastHalfTurn, {}});
  std::sort(_inactive.begin(), _inactive.end(), [](const InactiveChip& a, const InactiveChip& b) { return a.timeline > b.timeline; });

  if (_justMoved) {
    const auto active = _activeOf.find(*_justMoved);
    const auto span = _span.find(*_justMoved);
    if (active != _activeOf.end() && !active->second && span != _span.end()) _browse = Key{*_justMoved, span->second.second};
    _justMoved.reset();
  }
  if (_browse) { // the board browsed is gone (an undo)
    const auto span = _span.find(_browse->first);
    if (span == _span.end() || _browse->second < span->second.first || _browse->second > span->second.second) _browse.reset();
  }
}

const PlayCard* PlayViewLayout::card(int timeline) const {
  for (const PlayCard& c : _cards)
    if (c.timeline == timeline) return &c;
  return nullptr;
}

bool PlayViewLayout::anyPlayable() const {
  return std::any_of(_cards.begin(), _cards.end(), [](const PlayCard& c) { return c.role != BoardRole::Past; });
}

PlayViewLayout::Counts PlayViewLayout::counts() const {
  Counts n;
  for (const PlayCard& c : _cards) {
    switch (c.chip) {
      case Chip::MustMove: ++n.must; break;
      case Chip::Optional: ++n.optional; break;
      case Chip::Waiting: ++n.waiting; break;
      case Chip::Moved: ++n.moved; break;
    }
  }
  return n;
}

bool PlayViewLayout::isCard(Key key) const {
  const PlayCard* c = card(key.first);
  return c && c->halfTurn == key.second;
}

// ---------------------------------------------------------------------------------------------------------------------
// The inspector

void PlayViewLayout::setSelection(const std::optional<Chess::Core::Coord>& from, const std::vector<Chess::Core::Coord>& targets) {
  _targetBoards.clear();
  _selFrom = from;
  _targetCards.clear();
  if (from)
    for (const Chess::Core::Coord& t : targets)
      if (std::find(_targetCards.begin(), _targetCards.end(), t.l) == _targetCards.end()) _targetCards.push_back(t.l);
  if (from) {
    std::vector<std::pair<Key, int>> boards;
    for (const Chess::Core::Coord& t : targets) {
      const Key key{t.l, t.t};
      if (isCard(key)) continue;
      auto it = std::find_if(boards.begin(), boards.end(), [&](const auto& b) { return b.first == key; });
      if (it == boards.end()) boards.push_back({key, 1});
      else ++it->second;
    }
    std::sort(boards.begin(), boards.end(), [&](const auto& a, const auto& b) {
      const int da = std::abs(a.first.first - from->l), db = std::abs(b.first.first - from->l);
      return da != db ? da < db : a.first.second != b.first.second ? a.first.second > b.first.second : a.first.first < b.first.first;
    });
    _targetBoards = std::move(boards);
  }
  if (_pick && std::none_of(_targetBoards.begin(), _targetBoards.end(), [&](const auto& b) { return b.first == *_pick; })) _pick.reset();
}

void PlayViewLayout::browse(int timeline) {
  const auto span = _span.find(timeline);
  if (span == _span.end()) return;
  const auto active = _activeOf.find(timeline);
  const int top = active != _activeOf.end() && active->second ? span->second.second - 1 : span->second.second;
  if (top < span->second.first) return;
  _browse = Key{timeline, top};
  _tabScroll = 0;
}

void PlayViewLayout::browseBoard(int timeline, int halfTurn) {
  const auto span = _span.find(timeline);
  if (span == _span.end() || halfTurn < span->second.first || halfTurn > span->second.second) return;
  _browse = Key{timeline, halfTurn};
  _tabScroll = 0;
}

void PlayViewLayout::closeBrowse() {
  _browse.reset();
  _tabScroll = 0;
}

void PlayViewLayout::stepBrowse(int direction) {
  if (!_browse || _mode != Mode::History) return;
  const auto span = _span.find(_browse->first);
  if (span == _span.end()) return;
  const auto active = _activeOf.find(_browse->first);
  const int top = active != _activeOf.end() && active->second ? span->second.second - 1 : span->second.second;
  _browse->second = std::clamp(_browse->second + (direction > 0 ? 1 : -1), span->second.first, top);
}

void PlayViewLayout::pickTarget(Key key) { _pick = key; }

void PlayViewLayout::resolveInspector() {
  _inspectKey.reset();
  _mode = Mode::Empty;
  if (!_targetBoards.empty()) {
    _mode = Mode::Targets;
    _inspectKey = _pick ? *_pick : _targetBoards.front().first;
  } else if (_browse) {
    _mode = Mode::History;
    _inspectKey = _browse;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// The geometry

PlayViewLayout::Cell PlayViewLayout::makeCell(const Rect& cell, const pv::CellSize& size) {
  Cell c;
  c.cell = cell;
  c.hist = {cell.x, cell.y, size.histW, size.cellH};
  c.card = {cell.x + size.histW + pv::kInner, cell.y, size.cardW, size.cardH};
  const float pad = BoardLayout::kCardPad * size.k;
  c.board = {std::floor(c.card.x + pad), std::floor(c.card.y + pad), size.board, size.board};
  return c;
}

void PlayViewLayout::place(const Rect& area, int dim) {
  _area = area;
  _dim = dim;
  constexpr float kGap = 6.0f;
  Rect gridArea = area;
  _inactiveRow = {};
  if (!_inactive.empty()) {
    _inactiveRow = {area.x, area.y + area.h - pv::kInactiveRowH, area.w, pv::kInactiveRowH};
    gridArea.h -= pv::kInactiveRowH + kGap;
  }
  _gridArea = gridArea;
  resolveInspector();

  // Once a turn (and again when the window changes) the grid's columns and square size are chosen, and whether the inspector gets a cell of its
  // own: only when that costs no square size. Afterwards a card that appears (a branch) adds a row, or scrolls: nothing is reflowed.
  const int cards = static_cast<int>(_cards.size());
  if (cards > 0 && (!_decided || _decidedDim != dim || _decidedArea.w != gridArea.w || _decidedArea.h != gridArea.h)) {
    const pv::Grid without = pv::gridFor(gridArea, cards, dim), with = pv::gridFor(gridArea, cards + 1, dim);
    _reserved = with.square >= without.square;
    const pv::Grid& chosen = _reserved ? with : without;
    _cols = chosen.cols;
    _square = chosen.square;
    _decided = true;
    _decidedDim = dim;
    _decidedArea = gridArea;
  }
  const int count = cards + (_reserved ? 1 : 0);
  _grid = pv::gridWith(gridArea, count, dim, std::max(1, _cols), _square);
  _scroll = _grid.scrolls ? std::clamp(_scroll, 0.0f, std::max(0.0f, _grid.contentHeight - gridArea.h)) : 0.0f;
  for (size_t i = 0; i < _cards.size(); ++i) {
    Rect cell = _grid.cells[i];
    cell.y -= _scroll;
    const Cell c = makeCell(cell, _grid.size);
    _cards[i].cell = c.cell;
    _cards[i].histRect = c.hist;
    _cards[i].card = c.card;
    _cards[i].board = c.board;
  }

  // The inspector: the grid's last cell when it was reserved, else (only while it has something to show) over the least relevant card
  _covered.reset();
  _inspectorShown = false;
  if (_reserved && !_grid.cells.empty()) {
    Rect last = _grid.cells.back();
    last.y -= _scroll;
    _inspector = makeCell(last, _grid.size);
    _inspectorShown = true;
  } else if (_mode != Mode::Empty && !_cards.empty()) {
    int best = -1, bestScore = -1;
    for (size_t i = 0; i < _cards.size(); ++i) {
      const PlayCard& c = _cards[i];
      int score = c.chip == Chip::Waiting ? 0 : c.chip == Chip::Moved ? 1 : c.chip == Chip::Optional ? 2 : 3;
      const bool involved = (_selFrom && _selFrom->l == c.timeline) ||
                            std::find(_targetCards.begin(), _targetCards.end(), c.timeline) != _targetCards.end();
      if (involved) score += 10; // the lifted piece's card and the cards it can reach stay uncovered when anything else will do
      if (best < 0 || score <= bestScore) { // the later card wins a tie
        best = static_cast<int>(i);
        bestScore = score;
      }
    }
    _covered = _cards[static_cast<size_t>(best)].timeline;
    _inspector = Cell{_cards[static_cast<size_t>(best)].cell, _cards[static_cast<size_t>(best)].histRect, _cards[static_cast<size_t>(best)].card,
                      _cards[static_cast<size_t>(best)].board};
    _inspectorShown = true;
  }

  // the chips of the inactive timelines, after a label
  float x = area.x + 74.0f;
  for (InactiveChip& chip : _inactive) {
    chip.rect = {x, _inactiveRow.y + 2.0f, 96.0f, pv::kInactiveRowH - 4.0f};
    if (_inactiveRow.h <= 0.0f || chip.rect.x + chip.rect.w > area.x + area.w) chip.rect = {};
    x += 96.0f + 4.0f;
  }
  placeTabs();
}

void PlayViewLayout::placeTabs() {
  _tabs.clear();
  _closeRect = {};
  if (_mode == Mode::Empty || !_inspectKey || !_inspectorShown) return;
  const Rect col = _inspector.hist;
  float y = col.y;
  float chipH = 36.0f; // two lines of 12 px
  if (_mode == Mode::History) { // a close button first, then every board of the timeline, newest first
    _closeRect = {col.x, y, col.w, 26.0f};
    y += 30.0f;
    chipH = 26.0f;
  }
  std::vector<std::pair<Key, int>> entries;
  if (_mode == Mode::Targets) {
    entries = _targetBoards;
  } else {
    const auto span = _span.find(_inspectKey->first);
    if (span != _span.end()) {
      const auto active = _activeOf.find(_inspectKey->first);
      const int top = active != _activeOf.end() && active->second ? span->second.second - 1 : span->second.second;
      for (int t = top; t >= span->second.first; --t) entries.push_back({{_inspectKey->first, t}, 0});
    }
  }
  const int fit = std::max(1, static_cast<int>(std::floor((col.y + col.h - y + 3.0f) / (chipH + 3.0f))));
  _tabScroll = std::clamp(_tabScroll, 0, std::max(0, static_cast<int>(entries.size()) - fit));
  for (size_t i = static_cast<size_t>(_tabScroll); i < entries.size() && static_cast<int>(i) - _tabScroll < fit; ++i) {
    InspectorTab tab;
    tab.key = entries[i].first;
    tab.targets = entries[i].second;
    tab.current = tab.key == *_inspectKey;
    tab.rect = {col.x, y, col.w, chipH};
    _tabs.push_back(tab);
    y += chipH + 3.0f;
  }
}

void PlayViewLayout::scrollBy(float dy) {
  if (!_grid.scrolls) return;
  _scroll = std::clamp(_scroll + dy, 0.0f, std::max(0.0f, _grid.contentHeight - _gridArea.h));
}

void PlayViewLayout::reveal(const Rect& rect) {
  if (!_grid.scrolls) return;
  if (rect.y < _gridArea.y) scrollBy(rect.y - _gridArea.y);
  else if (rect.y + rect.h > _gridArea.y + _gridArea.h) scrollBy(rect.y + rect.h - (_gridArea.y + _gridArea.h));
}

// ---------------------------------------------------------------------------------------------------------------------
// Looking things up

std::optional<Rect> PlayViewLayout::boardRect(Key key) const {
  if (_inspectorShown && _inspectKey && *_inspectKey == key) return _inspector.board;
  if (const PlayCard* c = card(key.first); c && c->halfTurn == key.second) return c->board;
  if (_inspectKey && *_inspectKey == key) return _inspector.board;
  return std::nullopt;
}

std::optional<Rect> PlayViewLayout::cardRect(Key key) const {
  if (_inspectorShown && _inspectKey && *_inspectKey == key) return _inspector.card;
  if (const PlayCard* c = card(key.first); c && c->halfTurn == key.second) return c->card;
  if (_inspectKey && *_inspectKey == key) return _inspector.card;
  return std::nullopt;
}

std::optional<Rect> PlayViewLayout::squareRect(const Chess::Core::Coord& c) const {
  const auto board = boardRect({c.l, c.t});
  if (!board) return std::nullopt;
  return BoardLayout::squareRect(*board, _dim, c.x, c.y);
}

std::optional<Rect> PlayViewLayout::squareOnTimeline(int timeline, int x, int y) const {
  const PlayCard* c = card(timeline);
  if (!c) return std::nullopt;
  return BoardLayout::squareRect(c->board, _dim, x, y);
}

PlayViewLayout::Hit PlayViewLayout::hit(float x, float y) const {
  Hit h;
  if (!_area.contains(x, y)) return h;
  if (_inactiveRow.h > 0.0f && _inactiveRow.contains(x, y)) {
    for (const InactiveChip& chip : _inactive)
      if (chip.rect.w > 0.0f && chip.rect.contains(x, y)) {
        h.kind = Hit::Kind::Inactive;
        h.key = {chip.timeline, chip.halfTurn};
        h.timeline = chip.timeline;
      }
    return h;
  }
  if (!_gridArea.contains(x, y)) return h;
  auto onBoard = [&](const Rect& board, const Rect& card, Key key) {
    if (board.contains(x, y)) {
      const float sq = board.w / static_cast<float>(_dim);
      const int fx = std::clamp(static_cast<int>(std::floor((x - board.x) / sq)), 0, _dim - 1);
      const int fy = std::clamp(static_cast<int>(std::floor((y - board.y) / sq)), 0, _dim - 1);
      h.kind = Hit::Kind::Square;
      h.key = key;
      h.square = Chess::Core::Coord{static_cast<int8_t>(fx), static_cast<int8_t>(_dim - 1 - fy), static_cast<int16_t>(key.second),
                                    static_cast<int16_t>(key.first)};
      return true;
    }
    if (card.contains(x, y)) {
      h.kind = Hit::Kind::Chrome;
      h.key = key;
      return true;
    }
    return false;
  };
  if (_covered) { // the inspector lies over a card: it takes the clicks there
    if (_inspectKey && onBoard(_inspector.board, _inspector.card, *_inspectKey)) return h;
    if (_closeRect.w > 0.0f && _closeRect.contains(x, y)) {
      h.kind = Hit::Kind::Close;
      return h;
    }
    for (const InspectorTab& tab : _tabs)
      if (tab.rect.contains(x, y)) {
        h.kind = Hit::Kind::Tab;
        h.key = tab.key;
        return h;
      }
    if (_inspector.cell.contains(x, y)) return h; // the rest of the covered cell: nothing
  }
  for (const PlayCard& c : _cards) {
    if (_covered && c.timeline == *_covered) continue;
    if (onBoard(c.board, c.card, {c.timeline, c.halfTurn})) return h;
    if (c.history > 0 && c.histRect.contains(x, y)) {
      h.kind = Hit::Kind::History;
      h.timeline = c.timeline;
      return h;
    }
  }
  if (!_covered && _inspectorShown && _inspectKey && onBoard(_inspector.board, _inspector.card, *_inspectKey)) return h;
  if (!_covered && _closeRect.w > 0.0f && _closeRect.contains(x, y)) {
    h.kind = Hit::Kind::Close;
    return h;
  }
  if (!_covered)
    for (const InspectorTab& tab : _tabs)
      if (tab.rect.contains(x, y)) {
        h.kind = Hit::Kind::Tab;
        h.key = tab.key;
        return h;
      }
  return h;
}

std::optional<int> PlayViewLayout::neighbour(int timeline, BoardLayout::Dir dir) const {
  const int n = static_cast<int>(_cards.size());
  int index = -1;
  for (int i = 0; i < n; ++i)
    if (_cards[static_cast<size_t>(i)].timeline == timeline) index = i;
  if (index < 0 || _grid.cols <= 0) return std::nullopt;
  const int cols = _grid.cols;
  int to = -1;
  switch (dir) {
    case BoardLayout::Dir::Left: to = index % cols > 0 ? index - 1 : -1; break;
    case BoardLayout::Dir::Right: to = index % cols < cols - 1 && index + 1 < n ? index + 1 : -1; break;
    case BoardLayout::Dir::Up: to = index - cols >= 0 ? index - cols : -1; break;
    case BoardLayout::Dir::Down: to = index + cols < n ? index + cols : (index / cols < (n - 1) / cols ? n - 1 : -1); break;
  }
  if (to < 0) return std::nullopt;
  return _cards[static_cast<size_t>(to)].timeline;
}

} // namespace play
