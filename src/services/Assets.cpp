#include "services/Assets.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

Assets::Assets(const std::string& root) : _root(root) {
    const std::string manifest = _root + "manifest.txt";
    std::ifstream in(manifest);
    if (!in) throw std::runtime_error("Cannot open asset manifest: " + manifest);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string kind, id, path, tag;
        if (!(fields >> kind) || kind[0] == '#') continue;
        if (!(fields >> id >> path)) throw std::runtime_error("Bad manifest line: " + line);
        Entry e{_root + path, false};
        while (fields >> tag) e.pointFilter |= tag == "filter=point";
        if (kind == "texture") _textureFiles[id] = e;
        else if (kind == "font") _fontFiles[id] = e;
        else if (kind == "sound") _soundFiles[id] = e;
        else if (kind == "music") _musicFiles[id] = e;
        else if (kind != "dir") throw std::runtime_error("Unknown manifest kind: " + line);
    }
    for (const auto& [id, e] : _textureFiles) {
        Texture2D t = LoadTexture(e.path.c_str());
        if (t.id == 0) throw std::runtime_error("Failed to load texture: " + e.path);
        if (e.pointFilter) SetTextureFilter(t, TEXTURE_FILTER_POINT);
        _textures[id] = t;
    }
}

Assets::~Assets() {
    for (auto& [id, t] : _textures) UnloadTexture(t);
    const unsigned defaultTex = GetFontDefault().texture.id;
    for (auto& [key, f] : _fonts)
        if (f.texture.id != defaultTex) UnloadFont(f);
    for (auto& [id, s] : _sounds)
        if (IsSoundValid(s)) UnloadSound(s);
    for (auto& [id, m] : _musics)
        if (IsMusicValid(m)) { StopMusicStream(m); UnloadMusicStream(m); }
}

const Assets::Entry& Assets::entry(const std::map<std::string, Entry>& kind, const std::string& id, const char* what) const {
    auto it = kind.find(id);
    if (it == kind.end()) throw std::runtime_error(std::string("Unknown ") + what + " asset id: " + id);
    return it->second;
}

Texture2D& Assets::texture(const std::string& id) {
    auto it = _textures.find(id);
    if (it == _textures.end()) throw std::runtime_error("Unknown texture asset id: " + id);
    return it->second;
}

Font Assets::font(const std::string& id, int size) {
    auto key = std::make_pair(id, size);
    auto it = _fonts.find(key);
    if (it != _fonts.end()) return it->second;
    const Entry& e = entry(_fontFiles, id, "font");
    std::vector<int> cps; // ASCII plus the middle dot used in HUD labels
    for (int c = 32; c < 127; ++c) cps.push_back(c);
    cps.push_back(0x00B7);
    Font f = LoadFontEx(e.path.c_str(), size, cps.data(), static_cast<int>(cps.size()));
    if (f.texture.id == 0) f = GetFontDefault();
    else SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return _fonts.emplace(key, f).first->second;
}

Sound& Assets::sound(const std::string& id) {
    auto it = _sounds.find(id);
    if (it != _sounds.end()) return it->second;
    return _sounds.emplace(id, LoadSound(entry(_soundFiles, id, "sound").path.c_str())).first->second;
}

Music& Assets::music(const std::string& id) {
    auto it = _musics.find(id);
    if (it != _musics.end()) return it->second;
    return _musics.emplace(id, LoadMusicStream(entry(_musicFiles, id, "music").path.c_str())).first->second;
}
