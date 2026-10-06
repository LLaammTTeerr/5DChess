#pragma once
#include <string>

/// How the multiverse is drawn (Settings -> Display -> Board view). The look itself is data: see play/BoardStyle.h.
enum class BoardView { DeepSpace, Atlas, Blueprint };

namespace boardview {
inline constexpr BoardView all[] = {BoardView::DeepSpace, BoardView::Atlas, BoardView::Blueprint};

inline const char* name(BoardView view) {
  switch (view) {
    case BoardView::DeepSpace: return "Deep space";
    case BoardView::Atlas: return "Atlas";
    case BoardView::Blueprint: return "Blueprint";
  }
  return "Deep space";
}

/// By display name; false (and `view` untouched) when unknown.
inline bool fromName(const std::string& text, BoardView& view) {
  for (BoardView v : all)
    if (text == name(v)) {
      view = v;
      return true;
    }
  return false;
}

inline BoardView next(BoardView view) {
  return all[(static_cast<int>(view) + 1) % static_cast<int>(sizeof(all) / sizeof(all[0]))];
}
} // namespace boardview
