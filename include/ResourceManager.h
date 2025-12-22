#pragma once

#include <raylib.h>
#include <string>
#include <map>

// Singleton
class ResourceManager {
public:
	static ResourceManager &getInstance();

	// Resource Retrieval
	Texture2D& getTexture2D(const std::string &alias);
	Font& getFont(const std::string &alias);

private:
	ResourceManager();
	~ResourceManager();

	// No copy / no move
	ResourceManager(const ResourceManager &) = delete;
	ResourceManager &operator=(const ResourceManager &) = delete;

	// Memory-based preloaders
	void _preloadTexture2D(const Texture2D &texture, const std::string &alias);
	void _preloadFont(const Font &font, const std::string &alias);

	void _unloadTexture2D(const std::string &alias);
	void _unloadFont(const std::string &alias);

	// Alias → Resource
	std::map<std::string, Texture2D> _textures;
	std::map<std::string, Font> _fonts;
};
