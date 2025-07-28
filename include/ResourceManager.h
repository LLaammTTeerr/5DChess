#pragma once
#include <iostream>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <map>

// Singleton
class ResourceManager {
public:
  static ResourceManager &getInstance();
  /* Resource Retrieval */
  Texture2D getTexture2D(const std::string &alias);
  Font getFont(const std::string &alias);

private:
  ResourceManager();
  ~ResourceManager();

  // Clean up resources properly
  ResourceManager(const ResourceManager &) = delete;
  ResourceManager &operator=(const ResourceManager &) = delete;


  /* Load Textures and Fonts from filename and map them to alias */
  void _preloadTexture2D(const std::string &filename, const std::string &alias);
  void _preloadFont(const std::string &filename, const std::string &alias);

  void _unloadTexture2D(const std::string &alias);
  void _unloadFont(const std::string &alias);

  /*
    Map filename to Texture2D and Font objects.
  */
  std::map<std::string, Texture2D> _textures;
  std::map<std::string, Font> _fonts;

  /* Map AliasToFilename */
  std::map<std::string, std::string> _MappingAliasToFilename;
};

