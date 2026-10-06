#pragma once
#include "services/Settings.h"

// Persistence of Settings: a small text file in the user's config directory on desktop
// (see services/SettingsFile.h for the format and the paths), the browser's localStorage on the web.
// Both functions swallow every I/O error: a missing, unreadable or half-written file means defaults.
// The UI test harness (TestMode::active) never touches them, so screenshots stay deterministic.
namespace SettingsStore {

/// Overwrites the fields of `settings` that the stored file holds and that are valid; the rest is left as it is.
void load(Settings& settings);
/// Writes all fields (to a temporary file first, then renamed over the old one).
void save(const Settings& settings);

} // namespace SettingsStore
