#include "engine/GameCatalog.h"

#include "engine/Position.h"

#include <iostream>
#include <optional>

namespace Chess {

namespace {

// Menu order. The id is the file name (assets/positions/<id>.5dp); ids are stable, titles may change.
constexpr const char* kModeIds[] = {
    "standard",    "omit-bishop",       "omit-knight",     "omit-queen",       "omit-rook",
    "knight-vs-bishop", "timeline-invasion", "timeline-battle", "timeline-fragment",
};

std::string& directory() {
  static std::string dir = "assets/positions";
  return dir;
}

std::optional<std::vector<ModeInfo>>& cache() {
  static std::optional<std::vector<ModeInfo>> modes;
  return modes;
}

} // namespace

void GameCatalog::setDirectory(std::string dir) {
  directory() = std::move(dir);
  cache().reset();
}

const std::vector<ModeInfo>& GameCatalog::modes() {
  if (!cache()) {
    std::vector<ModeInfo> list;
    for (const char* id : kModeIds) {
      ModeInfo info;
      info.id = id;
      info.positionFile = directory() + "/" + id + ".5dp";
      try {
        info.title = Core::loadPositionFile(info.positionFile).title;
      } catch (const std::exception& e) {
        std::cerr << "game mode '" << id << "' unavailable: " << e.what() << std::endl;
        continue;
      }
      list.push_back(std::move(info));
    }
    cache() = std::move(list);
  }
  return *cache();
}

const ModeInfo* GameCatalog::findById(const std::string& id) {
  for (const ModeInfo& m : modes())
    if (m.id == id) return &m;
  return nullptr;
}

const ModeInfo* GameCatalog::findByTitle(const std::string& title) {
  for (const ModeInfo& m : modes())
    if (m.title == title) return &m;
  return nullptr;
}

std::shared_ptr<IGame> GameCatalog::create(const std::string& id) {
  const ModeInfo* mode = findById(id);
  if (mode == nullptr) return nullptr;
  try {
    return Core::loadPositionFile(mode->positionFile).makeGame();
  } catch (const std::exception& e) {
    std::cerr << "game mode '" << id << "' failed to load: " << e.what() << std::endl;
    return nullptr;
  }
}

} // namespace Chess
