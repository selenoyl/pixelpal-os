#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pixelpal {

struct GameManifest {
  std::string id;
  std::string name;
  std::string version;
  std::string exec;
  std::string icon;
  std::string splash;
  std::string sdk_version;
  std::string author;
  std::string description;
  std::string min_os_version;
  bool supports_network_lan = false;
  std::filesystem::path root_dir;
  std::filesystem::path manifest_path;
};

struct CatalogResult {
  std::vector<GameManifest> games;
  std::vector<std::string> warnings;
};

CatalogResult scan_catalog(const std::filesystem::path& catalog_dir);
CatalogResult scan_installed_games(const std::filesystem::path& games_root);
GameManifest load_manifest(const std::filesystem::path& manifest_path);

}  // namespace pixelpal

