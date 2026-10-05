#include "Input.h"
#include "Audio/AudioManager.h"
#include "TestMode.h"
#include "services/Assets.h"
#include "services/Settings.h"
#include <iostream>

namespace {
const char* sfxId(Sfx s) {
    switch (s) {
        case Sfx::Move:    return "sfx.move";
        case Sfx::Capture: return "sfx.capture";
        case Sfx::Castle:  return "sfx.castle";
        case Sfx::Click:   return "sfx.click";
        case Sfx::Draw:    return "sfx.draw";
        case Sfx::Win:     return "sfx.win";
        case Sfx::Check:   return "sfx.check";
        case Sfx::Promote: return "sfx.promote";
        default:           return "";
    }
}
} // namespace

const std::vector<AudioManager::Track>& AudioManager::tracks() {
    static const std::vector<Track> list = {
        {"Calm Piano", "music.calm"},
        {"Slow Piano Intermission", "music.slow"},
        {"Solo Piano", "music.solo"},
    };
    return list;
}

AudioManager::~AudioManager() {
    if (_ready) CloseAudioDevice();
}

const std::string& AudioManager::selectedMusic() const { return _settings.music; }
void AudioManager::setSfxEnabled(bool on) { _settings.sfx = on; }
bool AudioManager::sfxEnabled() const { return _settings.sfx; }

void AudioManager::init(Assets& assets) {
    if (_ready) return;
    if (TestMode::get().audioDisabled) return; // UI test harness: stay silent, never open a device
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        std::cerr << "Audio: no audio device available, running silent." << std::endl;
        return;
    }
    _ready = true;
    _assets = &assets;
    SetMasterVolume(_muted ? 0.0f : 1.0f);
    for (int i = 0; i < kSfxCount; ++i) {
        Sound& s = assets.sound(sfxId(static_cast<Sfx>(i)));
        if (!IsSoundValid(s)) continue;
        _sounds[i] = &s;
        SetSoundVolume(s, _sfxVolume);
    }
}

void AudioManager::update() {
    if (!_ready) return;
    if (!_gesture && Input::mousePressed(MOUSE_BUTTON_LEFT)) {
        _gesture = true;
        startSelectedMusic(); // deferred until the first click (browser autoplay policy)
    }
    if (_musicPlaying) UpdateMusicStream(*_music);
}

void AudioManager::playMusic(const std::string& name) {
    _settings.music = name;
    if (!_ready) return;
    startSelectedMusic();
}

void AudioManager::stopMusic() {
    _settings.music = offName();
    if (_ready && _music) {
        StopMusicStream(*_music);
        _musicPlaying = false;
    }
}

void AudioManager::startSelectedMusic() {
    if (!_ready || !_gesture) return;
    const Track* track = nullptr;
    for (const auto& t : tracks()) {
        if (t.name == _settings.music) { track = &t; break; }
    }
    if (!track) {
        if (_music) { StopMusicStream(*_music); _musicPlaying = false; }
        return;
    }
    if (_music && _loadedName == track->name) {
        if (!_musicPlaying) { PlayMusicStream(*_music); _musicPlaying = true; }
        return;
    }
    if (_music) {
        StopMusicStream(*_music);
        _music = nullptr;
        _musicPlaying = false;
    }
    Music& m = _assets->music(track->id);
    if (!IsMusicValid(m)) {
        std::cerr << "Audio: failed to load " << track->id << std::endl;
        return;
    }
    _music = &m;
    _loadedName = track->name;
    m.looping = true;
    SetMusicVolume(m, _musicVolume);
    PlayMusicStream(m);
    _musicPlaying = true;
}

void AudioManager::playSfx(Sfx sfx) {
    if (!_ready || !_settings.sfx) return;
    int i = static_cast<int>(sfx);
    if (i < 0 || i >= kSfxCount || !_sounds[i]) return;
    PlaySound(*_sounds[i]);
}

void AudioManager::setMusicVolume(float v) {
    _musicVolume = v < 0 ? 0 : (v > 1 ? 1 : v);
    if (_ready && _music) SetMusicVolume(*_music, _musicVolume);
}

void AudioManager::setSfxVolume(float v) {
    _sfxVolume = v < 0 ? 0 : (v > 1 ? 1 : v);
    if (!_ready) return;
    for (int i = 0; i < kSfxCount; ++i)
        if (_sounds[i]) SetSoundVolume(*_sounds[i], _sfxVolume);
}

void AudioManager::setMuted(bool muted) {
    _muted = muted;
    if (_ready) SetMasterVolume(_muted ? 0.0f : 1.0f);
}
