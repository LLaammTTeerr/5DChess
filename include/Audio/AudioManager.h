#pragma once
#include <raylib.h>
#include <string>
#include <vector>

class Assets;
struct Settings;

// Sound effects the game can trigger.
enum class Sfx { Move, Capture, Castle, Click, Draw, Win, Check, Promote, Count };

// Audio service owned by App: one streamed background track plus preloaded sound effects. Selected music
// and the sfx on/off flag live in Settings; sounds and streams are owned by Assets.
// Every call is a harmless no-op when no audio device is available (headless/CI).
// Lifetime: init() after InitWindow, update() once per frame; the destructor closes the audio device, so
// the Assets that holds the sounds must be destroyed first (App declares audio before assets).
class AudioManager {
public:
    struct Track {
        std::string name; // display name used by the Settings menu
        std::string id;   // asset id of the music stream (assets/manifest.txt)
    };

    explicit AudioManager(Settings& settings) : _settings(settings) {}
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Opens the audio device and loads the sound effects. Safe to call twice; tolerates failure.
    void init(Assets& assets);
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
    const std::string& selectedMusic() const;

    void playSfx(Sfx sfx);

    void setMusicVolume(float v);
    float musicVolume() const { return _musicVolume; }
    void setSfxVolume(float v);
    float sfxVolume() const { return _sfxVolume; }
    void setMuted(bool muted);
    bool isMuted() const { return _muted; }
    void setSfxEnabled(bool on);
    bool sfxEnabled() const;

private:
    void startSelectedMusic();

    static constexpr int kSfxCount = static_cast<int>(Sfx::Count);

    Settings& _settings;
    Assets* _assets = nullptr;
    bool _ready = false;
    bool _gesture = false;      // a mouse click has been seen
    bool _musicPlaying = false;
    Music* _music = nullptr;    // owned by Assets
    std::string _loadedName;
    Sound* _sounds[kSfxCount]{}; // owned by Assets; nullptr when missing
    float _musicVolume = 0.5f;
    float _sfxVolume = 0.7f;
    bool _muted = false;
};
