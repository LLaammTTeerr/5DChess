#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "chess.h"

// Saved games: the autosave and three named slots, each stored as a game record (`5dchess-record 1`, see
// docs/NOTATION.md) under a name in a Storage (a directory on desktop, the browser's localStorage on the web).
// No graphics dependency, so it is unit tested. Nothing here throws: a missing, oversized, unreadable or malformed
// file is reported in the result and left where it is (a save is never deleted because it could not be loaded).
namespace savegame {

inline constexpr int kSlots = 3;
inline constexpr size_t kMaxBytes = 1u << 20; // a record is ~30 bytes per turn; bigger files are not ours

enum class ReadStatus { Ok, Missing, TooLarge, Error };

/// Named blobs of text. Names are plain file-name-safe words ("autosave", "slot1").
class Storage {
public:
  virtual ~Storage() = default;
  /// Reads at most kMaxBytes: a bigger blob gives TooLarge without loading it.
  virtual ReadStatus read(const std::string& name, std::string& text) const = 0;
  virtual bool exists(const std::string& name) const = 0;
  virtual bool write(const std::string& name, const std::string& text) = 0; // atomic where the medium allows it
  virtual void remove(const std::string& name) = 0;
};

/// One file per name in a directory (`<name>.5dr`), created on the first write.
class FileStorage : public Storage {
public:
  explicit FileStorage(std::string directory) : _dir(std::move(directory)) {}
  ReadStatus read(const std::string& name, std::string& text) const override;
  bool exists(const std::string& name) const override;
  bool write(const std::string& name, const std::string& text) override;
  void remove(const std::string& name) override;

private:
  std::string _dir;
  std::string pathOf(const std::string& name) const;
};

/// In process only (the UI test harness, unit tests).
class MemoryStorage : public Storage {
public:
  ReadStatus read(const std::string& name, std::string& text) const override;
  bool exists(const std::string& name) const override;
  bool write(const std::string& name, const std::string& text) override;
  void remove(const std::string& name) override;
  std::vector<std::pair<std::string, std::string>> blobs; // name, text
};

/// What a slot shows in the list; read from the text without replaying it.
struct SlotSummary {
  enum class State { Empty, Ready, Unreadable } state = State::Empty;
  std::string title; ///< the mode's name, "Custom position" for an embedded one
  int turns = 0;     ///< submitted turns
  std::string date;  ///< as written by saveSlot ("2026-10-06 14:32"), possibly empty
  /// "Standard - 7 turns - 2026-10-06 14:32" / "Empty" / "Unreadable save"
  std::string describe() const;
};

struct LoadResult {
  std::shared_ptr<Chess::IGame> game; ///< null on failure
  std::string error;                  ///< why, for a log; the UI says "This save can't be loaded"
  explicit operator bool() const { return game != nullptr; }
};

/// Parses record text into a game, mapping every failure (bad syntax, an illegal move, an unknown mode...) into the result.
LoadResult loadText(std::string_view text);
/// The list line of record text.
SlotSummary summarize(std::string_view text);

class SaveStore {
public:
  explicit SaveStore(std::unique_ptr<Storage> storage) : _storage(std::move(storage)) {}

  /// The record of the submitted turns (a game without any is not written). Returns whether it was stored.
  bool autosave(const Chess::IGame& game);
  /// Is there an autosave file? (Cheap: it is not read; a game that ended deletes its autosave.)
  bool hasAutosave() const { return _storage->exists("autosave"); }
  LoadResult loadAutosave() const { return load("autosave"); }
  /// Empty (no file), Ready, or Unreadable (a file that is too large, unreadable or not a record: Continue must not be offered).
  SlotSummary autosaveSummary() const { return summaryOf("autosave"); }
  void clearAutosave() { _storage->remove("autosave"); }

  /// `stamp` is the date line shown in the slot list (the caller owns the clock). `droppedPending` as in writeRecord.
  bool saveSlot(int slot, const Chess::IGame& game, const std::string& stamp, bool* droppedPending = nullptr);
  SlotSummary slot(int slot) const;
  LoadResult loadSlot(int slot) const { return load(slotName(slot)); }
  void deleteSlot(int slot) { _storage->remove(slotName(slot)); }
  bool anySlot() const;

  Storage& storage() { return *_storage; }

private:
  std::unique_ptr<Storage> _storage;
  static std::string slotName(int slot) { return "slot" + std::to_string(slot + 1); } // slot is 0-based
  LoadResult load(const std::string& name) const;
  SlotSummary summaryOf(const std::string& name) const;
};

/// The storage of this platform: the config directory next to settings.txt on desktop, localStorage on the web, memory under
/// TestMode. (Defined in src/services/SaveStorage.cpp, part of the game.)
std::unique_ptr<Storage> defaultStorage();
/// "2026-10-06 14:32" in local time (a fixed date under TestMode, so screenshots stay deterministic). Same file as above.
std::string timestamp();

} // namespace savegame
