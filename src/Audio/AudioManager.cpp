#include "Input.h"
#include "Audio/AudioManager.h"
#include "TestMode.h"
#include <iostream>

namespace {
const char* sfxFile(Sfx s) {
    switch (s) {
        case Sfx::Move:    return "assets/soundeffect/move.ogg";
        case Sfx::Capture: return "assets/soundeffect/capture.ogg";
        case Sfx::Castle:  return "assets/soundeffect/castle.ogg";
        case Sfx::Click:   return "assets/soundeffect/click.ogg";
        case Sfx::Draw:    return "assets/soundeffect/game-drawn.ogg";
        case Sfx::Win:     return "assets/soundeffect/game-won.ogg";
        case Sfx::Check:   return "assets/soundeffect/check.ogg";
        case Sfx::Promote: return "assets/soundeffect/promote.ogg";
        default:           return "";
    }
}
} // namespace

AudioManager& AudioManager::instance() {
    static AudioManager inst;
    return inst;
}

const std::vector<AudioManager::Track>& AudioManager::tracks() {
    static const std::vector<Track> list = {
        {"Calm Piano", "assets/backgroundmusic/calm-piano-vaporware.ogg"},
        {"Slow Piano Intermission", "assets/backgroundmusic/slow-piano-intermission.ogg"},
        {"Solo Piano", "assets/backgroundmusic/solo-piano-4.ogg"},
    };
    return list;
}

void AudioManager::init() {
    if (_ready) return;
    if (TestMode::get().audioDisabled) return; // UI test harness: stay silent, never open a device
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        std::cerr << "Audio: no audio device available, running silent." << std::endl;
        return;
    }
    _ready = true;
    SetMasterVolume(_muted ? 0.0f : 1.0f);
    for (int i = 0; i < kSfxCount; ++i) {
        _sounds[i] = LoadSound(sfxFile(static_cast<Sfx>(i)));
        _soundLoaded[i] = IsSoundValid(_sounds[i]);
        if (_soundLoaded[i]) SetSoundVolume(_sounds[i], _sfxVolume);
    }
}

void AudioManager::shutdown() {
    if (!_ready) return;
    if (_musicLoaded) {
        StopMusicStream(_music);
        UnloadMusicStream(_music);
        _musicLoaded = false;
        _musicPlaying = false;
    }
    for (int i = 0; i < kSfxCount; ++i) {
        if (_soundLoaded[i]) UnloadSound(_sounds[i]);
        _soundLoaded[i] = false;
    }
    CloseAudioDevice();
    _ready = false;
}

void AudioManager::update() {
    if (!_ready) return;
    if (!_gesture && Input::mousePressed(MOUSE_BUTTON_LEFT)) {
        _gesture = true;
        startSelectedMusic(); // deferred until the first click (browser autoplay policy)
    }
    if (_musicPlaying) UpdateMusicStream(_music);
}

void AudioManager::playMusic(const std::string& name) {
    _selected = name;
    if (!_ready) return;
    startSelectedMusic();
}

void AudioManager::stopMusic() {
    _selected = offName();
    if (_ready && _musicLoaded) {
        StopMusicStream(_music);
        _musicPlaying = false;
    }
}

void AudioManager::startSelectedMusic() {
    if (!_ready || !_gesture) return;
    const Track* track = nullptr;
    for (const auto& t : tracks()) {
        if (t.name == _selected) { track = &t; break; }
    }
    if (!track) {
        if (_musicLoaded) { StopMusicStream(_music); _musicPlaying = false; }
        return;
    }
    if (_musicLoaded && _loadedName == track->name) {
        if (!_musicPlaying) { PlayMusicStream(_music); _musicPlaying = true; }
        return;
    }
    if (_musicLoaded) {
        StopMusicStream(_music);
        UnloadMusicStream(_music);
        _musicLoaded = false;
        _musicPlaying = false;
    }
    _music = LoadMusicStream(track->file.c_str());
    if (!IsMusicValid(_music)) {
        std::cerr << "Audio: failed to load " << track->file << std::endl;
        return;
    }
    _musicLoaded = true;
    _loadedName = track->name;
    _music.looping = true;
    SetMusicVolume(_music, _musicVolume);
    PlayMusicStream(_music);
    _musicPlaying = true;
}

void AudioManager::playSfx(Sfx sfx) {
    if (!_ready || !_sfxEnabled) return;
    int i = static_cast<int>(sfx);
    if (i < 0 || i >= kSfxCount || !_soundLoaded[i]) return;
    PlaySound(_sounds[i]);
}

void AudioManager::setMusicVolume(float v) {
    _musicVolume = v < 0 ? 0 : (v > 1 ? 1 : v);
    if (_ready && _musicLoaded) SetMusicVolume(_music, _musicVolume);
}

void AudioManager::setSfxVolume(float v) {
    _sfxVolume = v < 0 ? 0 : (v > 1 ? 1 : v);
    if (!_ready) return;
    for (int i = 0; i < kSfxCount; ++i)
        if (_soundLoaded[i]) SetSoundVolume(_sounds[i], _sfxVolume);
}

void AudioManager::setMuted(bool muted) {
    _muted = muted;
    if (_ready) SetMasterVolume(_muted ? 0.0f : 1.0f);
}
