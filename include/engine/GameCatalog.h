#pragma once

// The playable game modes: one position file per mode under assets/positions (see include/engine/Position.h).
// The single registry the menus, the scenes and the developer tools use.

#include "chess.h"

#include <memory>
#include <string>
#include <vector>

namespace Chess {

struct ModeInfo {
  std::string id;           ///< stable identifier, e.g. "omit-bishop"; also the file name without extension
  std::string title;        ///< display name, from the file's `title:` line
  std::string positionFile; ///< path of the .5dp file
};

class GameCatalog {
public:
  /** Where the position files are looked up (default "assets/positions", relative to the working directory). Call before modes(). */
  static void setDirectory(std::string directory);

  /** All modes in menu order; files that fail to load are reported on stderr and left out. Loaded once. */
  static const std::vector<ModeInfo>& modes();

  static const ModeInfo* findById(const std::string& id);
  static const ModeInfo* findByTitle(const std::string& title);

  /** A fresh game of the mode, or nullptr if there is no such mode. */
  static std::shared_ptr<IGame> create(const std::string& id);
};

} // namespace Chess
