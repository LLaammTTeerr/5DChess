#pragma once
#include <raylib.h>
#include <string>
#include <vector>

// Sound effects the game can trigger.
enum class Sfx { Move, Capture, Castle, Click, Draw, Win, Check, Promote, Count };

// Global audio service: one streamed background track plus preloaded sound effects.
// Every call is a harmless no-op when no audio device is available (headless/CI).
// Lifetime: init() after InitWindow, update() once per frame, shutdown() before CloseWindow.
class AudioManager {
public:
    struct Track {
        std::string name; // display name used by the Settings menu
        std::string file; // path relative to the application directory
    };

    static AudioManager& instance();

    // Opens the audio device and loads the sound effects. Safe to call twice; tolerates failure.
    void init();
    // Unloads music/sounds and closes the device. Must run before CloseWindow().
    void shutdown();
    // Feeds the music stream; also detects the first mouse click (browsers need a user gesture).
    void update();

    bool isReady() const { return _ready; }

    // Selectable tracks (excluding "Off").
    static const std::vector<Track>& tracks();
    static const char* offName() { return "Off"; }

    // Select a track by display name; "Off" or an unknown name stops the music.
    // The track is streamed lazily and only starts after the first user click.
    void playMusic(const std::string& name);
    void stopMusic();
    const std::string& selectedMusic() const { return _selected; }

    void playSfx(Sfx sfx);

    void setMusicVolume(float v);
    float musicVolume() const { return _musicVolume; }
    void setSfxVolume(float v);
    float sfxVolume() const { return _sfxVolume; }
    void setMuted(bool muted);
    bool isMuted() const { return _muted; }
    void setSfxEnabled(bool on) { _sfxEnabled = on; }
    bool sfxEnabled() const { return _sfxEnabled; }

private:
    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    void startSelectedMusic();

    static constexpr int kSfxCount = static_cast<int>(Sfx::Count);

    bool _ready = false;
    bool _gesture = false;      // a mouse click has been seen
    bool _musicLoaded = false;
    bool _musicPlaying = false;
    Music _music{};
    std::string _selected = "Off";
    std::string _loadedName;
    Sound _sounds[kSfxCount]{};
    bool _soundLoaded[kSfxCount]{};
    float _musicVolume = 0.5f;
    float _sfxVolume = 0.7f;
    bool _muted = false;
    bool _sfxEnabled = true;
};
