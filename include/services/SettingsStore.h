#pragma once
#include <functional>
#include <string>
#include "services/Settings.h"
#include "services/SettingsFile.h"

// Persistence of Settings: a small text file in the user's config directory on desktop
// (see services/SettingsFile.h for the format and the paths), the browser's localStorage on the web.
// Both functions swallow every I/O error: a missing, unreadable or half-written file means defaults.
// The UI test harness (TestMode::active) never touches them, so screenshots stay deterministic.
namespace SettingsStore {

/// Applies parsed settings-file values to `settings`: only valid ones (a theme or board view that is not known,
/// e.g. a removed piece theme, leaves the field as it is). `isKnownMusic` says which stored track names still exist;
/// without it every name is accepted. (Pure, so the unit tests cover it.)
void apply(const settingsfile::Values& values, Settings& settings,
           const std::function<bool(const std::string&)>& isKnownMusic = {});

/// Overwrites the fields of `settings` that the stored file holds and that are valid; the rest is left as it is.
void load(Settings& settings);
/// Writes all fields (to a temporary file first, then renamed over the old one).
void save(const Settings& settings);

} // namespace SettingsStore
