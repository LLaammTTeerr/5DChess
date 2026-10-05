#pragma once
#include <raylib.h>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

// Everything the game loads from assets/, driven by assets/manifest.txt (one line per asset:
// "<kind> <id> <path> [tag=value ...]"). Textures are loaded in the constructor (fail fast); fonts (per
// size), sounds and music on first use. All of it is unloaded in the destructor, so an Assets must be
// destroyed before CloseAudioDevice() (sounds, music) and CloseWindow() (GPU resources).
class Assets {
public:
    // `root` is prepended to every manifest path (the manifest itself is `root`/manifest.txt).
    explicit Assets(const std::string& root = "assets/");
    ~Assets();
    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    // All four throw std::runtime_error for an unknown id (texture() also if a file failed to load).
    Texture2D& texture(const std::string& id);
    // Display-size font with ASCII + the middle dot, cached per (id, size). Falls back to raylib's default
    // font when the file cannot be loaded.
    Font font(const std::string& id, int size);
    // Both need an open audio device; a file that fails to load comes back invalid (IsSoundValid/IsMusicValid).
    Sound& sound(const std::string& id);
    Music& music(const std::string& id);

private:
    struct Entry { std::string path; bool pointFilter = false; };
    const Entry& entry(const std::map<std::string, Entry>& kind, const std::string& id, const char* what) const;

    std::string _root;
    std::map<std::string, Entry> _textureFiles, _fontFiles, _soundFiles, _musicFiles;
    std::unordered_map<std::string, Texture2D> _textures;
    std::map<std::pair<std::string, int>, Font> _fonts;
    std::unordered_map<std::string, Sound> _sounds;
    std::unordered_map<std::string, Music> _musics;
};
