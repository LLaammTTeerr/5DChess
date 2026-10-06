#include "services/Assets.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
// Pixel art (filter=point) stays nearest-neighbour. Everything else is smoothed: bilinear, and trilinear with mipmaps
// when the size is a power of two, so smooth artwork stays clean when drawn much smaller than its file. Non
// power-of-two textures get no mipmaps (WebGL 1 does not support them).
void applyFilter(Texture2D& t, bool point) {
    if (point) { SetTextureFilter(t, TEXTURE_FILTER_POINT); return; }
    const auto pow2 = [](int v) { return v > 0 && (v & (v - 1)) == 0; };
    if (pow2(t.width) && pow2(t.height)) {
        GenTextureMipmaps(&t);
        SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    } else {
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    }
}
} // namespace

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
        bool webOnly = true;
        while (fields >> tag) {
            if (tag == "filter=point") e.pointFilter = true;
            else if (tag == "web=no") webOnly = false;
            else throw std::runtime_error("Unknown manifest tag '" + tag + "' (valid: filter=point, web=no): " + line);
        }
        if (!webOnly) {
            // Textures are loaded eagerly and drawn unconditionally, so they cannot be optional.
            if (kind == "texture") throw std::runtime_error("web=no is not allowed for textures: " + line);
#ifdef __EMSCRIPTEN__
            _skipped.insert(id); // not preloaded in the web build: font() falls back, sound()/music() come back invalid
            continue;
#endif
        }
        if (kind == "texture") _textureFiles[id] = e;
        else if (kind == "font") _fontFiles[id] = e;
        else if (kind == "sound") _soundFiles[id] = e;
        else if (kind == "music") _musicFiles[id] = e;
        else if (kind != "dir") throw std::runtime_error("Unknown manifest kind: " + line);
    }
    for (const auto& [id, e] : _textureFiles) {
        Texture2D t = LoadTexture(e.path.c_str());
        if (t.id == 0) throw std::runtime_error("Failed to load texture: " + e.path);
        applyFilter(t, e.pointFilter);
        _textures[id] = t;
    }
}

Assets::~Assets() {
    for (auto& [id, t] : _textures) UnloadTexture(t);
    for (auto& [id, t] : _grayTextures) UnloadTexture(t);
    const unsigned defaultTex = GetFontDefault().texture.id;
    for (auto& [key, f] : _fonts)
        if (f.texture.id != defaultTex) UnloadFont(f);
    for (auto& [id, s] : _sounds)
        if (IsSoundValid(s)) UnloadSound(s);
    for (auto& [id, m] : _musics)
        if (IsMusicValid(m)) { StopMusicStream(m); UnloadMusicStream(m); }
}

const Assets::Entry* Assets::entry(const std::map<std::string, Entry>& kind, const std::string& id, const char* what) const {
    auto it = kind.find(id);
    if (it == kind.end()) {
        if (_skipped.count(id)) return nullptr;
        throw std::runtime_error(std::string("Unknown ") + what + " asset id: " + id);
    }
    return &it->second;
}

Texture2D& Assets::texture(const std::string& id) {
    entry(_textureFiles, id, "texture"); // throws for an unknown id; every known texture was loaded in the constructor
    return _textures.at(id);
}

Texture2D& Assets::grayTexture(const std::string& id) {
    auto it = _grayTextures.find(id);
    if (it != _grayTextures.end()) return it->second;
    const Entry* e = entry(_textureFiles, id, "texture");
    Image image = LoadImage(e->path.c_str());
    // Grey by hand on RGBA: ImageColorGrayscale would switch to a 1-channel format and lose the alpha
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* px = static_cast<Color*>(image.data);
    for (int i = 0; i < image.width * image.height; ++i) {
        const float lum = 0.299f * px[i].r + 0.587f * px[i].g + 0.114f * px[i].b;
        const float c = std::clamp((lum - 128.0f) * 1.1f + 128.0f, 0.0f, 255.0f); // a little contrast
        px[i].r = px[i].g = px[i].b = static_cast<unsigned char>(c);
    }
    Texture2D t = LoadTextureFromImage(image);
    UnloadImage(image);
    if (t.id == 0) return texture(id); // cannot happen for a texture that loaded in the constructor; stay in colour
    applyFilter(t, e->pointFilter);
    return _grayTextures.emplace(id, t).first->second;
}

Font Assets::font(const std::string& id, int size) {
    auto key = std::make_pair(id, size);
    auto it = _fonts.find(key);
    if (it != _fonts.end()) return it->second;
    const Entry* e = entry(_fontFiles, id, "font");
    std::vector<int> cps; // ASCII plus the middle dot used in HUD labels
    for (int c = 32; c < 127; ++c) cps.push_back(c);
    cps.push_back(0x00B7);
    Font f = e ? LoadFontEx(e->path.c_str(), size, cps.data(), static_cast<int>(cps.size())) : Font{};
    if (f.texture.id == 0) f = GetFontDefault();
    else SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return _fonts.emplace(key, f).first->second;
}

Sound& Assets::sound(const std::string& id) {
    auto it = _sounds.find(id);
    if (it != _sounds.end()) return it->second;
    const Entry* e = entry(_soundFiles, id, "sound");
    return _sounds.emplace(id, e ? LoadSound(e->path.c_str()) : Sound{}).first->second;
}

Music& Assets::music(const std::string& id) {
    auto it = _musics.find(id);
    if (it != _musics.end()) return it->second;
    const Entry* e = entry(_musicFiles, id, "music");
    return _musics.emplace(id, e ? LoadMusicStream(e->path.c_str()) : Music{}).first->second;
}
