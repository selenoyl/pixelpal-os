#include "pixelpal/game_manifest.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <stdexcept>

namespace pixelpal {
namespace {

std::string trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return value.substr(start, end - start);
}

std::string unquote(std::string value) {
  value = trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

bool parse_bool(const std::string& value) {
  const std::string lowered = trim(value);
  return lowered == "true" || lowered == "1" || lowered == "yes";
}

std::map<std::string, std::string> parse_key_value_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("Unable to open manifest: " + path.string());
  }

  std::map<std::string, std::string> parsed;
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }

    line = trim(line);
    if (line.empty() || line.front() == '[') {
      continue;
    }

    const std::size_t separator = line.find('=');
    if (separator == std::string::npos) {
      continue;
    }

    const std::string key = trim(line.substr(0, separator));
    const std::string value = trim(line.substr(separator + 1));
    parsed[key] = value;
  }

  return parsed;
}

GameManifest manifest_from_map(const std::map<std::string, std::string>& parsed,
                               const std::filesystem::path& manifest_path) {
  GameManifest manifest;
  manifest.id = unquote(parsed.count("id") == 0 ? "" : parsed.at("id"));
  manifest.name = unquote(parsed.count("name") == 0 ? manifest.id : parsed.at("name"));
  manifest.version = unquote(parsed.count("version") == 0 ? "0.0.0" : parsed.at("version"));
  manifest.exec = unquote(parsed.count("exec") == 0 ? "" : parsed.at("exec"));
  manifest.icon = unquote(parsed.count("icon") == 0 ? "" : parsed.at("icon"));
  manifest.splash = unquote(parsed.count("splash") == 0 ? "" : parsed.at("splash"));
  manifest.sdk_version =
      unquote(parsed.count("sdk_version") == 0 ? "1" : parsed.at("sdk_version"));
  manifest.author = unquote(parsed.count("author") == 0 ? "" : parsed.at("author"));
  manifest.description =
      unquote(parsed.count("description") == 0 ? "" : parsed.at("description"));
  manifest.min_os_version =
      unquote(parsed.count("min_os_version") == 0 ? "" : parsed.at("min_os_version"));
  manifest.supports_network_lan =
      parsed.count("supports_network_lan") != 0 &&
      parse_bool(parsed.at("supports_network_lan"));
  manifest.root_dir = parsed.count("root_dir") == 0
                          ? manifest_path.parent_path()
                          : std::filesystem::path(unquote(parsed.at("root_dir")));
  manifest.manifest_path = parsed.count("manifest_path") == 0
                               ? manifest_path
                               : std::filesystem::path(unquote(parsed.at("manifest_path")));
  return manifest;
}

bool looks_like_cache(const std::filesystem::path& path) {
  return path.extension() == ".cache";
}

}  // namespace

GameManifest load_manifest(const std::filesystem::path& manifest_path) {
  const auto parsed = parse_key_value_file(manifest_path);
  GameManifest manifest = manifest_from_map(parsed, manifest_path);

  if (manifest.id.empty()) {
    throw std::runtime_error("Manifest missing id: " + manifest_path.string());
  }
  if (manifest.exec.empty()) {
    throw std::runtime_error("Manifest missing exec: " + manifest_path.string());
  }

  return manifest;
}

CatalogResult scan_catalog(const std::filesystem::path& catalog_dir) {
  CatalogResult result;
  if (!std::filesystem::exists(catalog_dir)) {
    result.warnings.push_back("Catalog directory not found: " + catalog_dir.string());
    return result;
  }

  for (const auto& entry : std::filesystem::directory_iterator(catalog_dir)) {
    if (!entry.is_regular_file() || !looks_like_cache(entry.path())) {
      continue;
    }

    try {
      result.games.push_back(load_manifest(entry.path()));
    } catch (const std::exception& error) {
      result.warnings.push_back(error.what());
    }
  }

  std::sort(result.games.begin(), result.games.end(), [](const GameManifest& left,
                                                         const GameManifest& right) {
    return left.name < right.name;
  });

  return result;
}

CatalogResult scan_installed_games(const std::filesystem::path& games_root) {
  CatalogResult result;
  if (!std::filesystem::exists(games_root)) {
    result.warnings.push_back("Games root not found: " + games_root.string());
    return result;
  }

  for (const auto& entry : std::filesystem::directory_iterator(games_root)) {
    if (!entry.is_directory()) {
      continue;
    }

    const auto manifest_path = entry.path() / "manifest.toml";
    if (!std::filesystem::exists(manifest_path)) {
      result.warnings.push_back("Skipping missing manifest: " + manifest_path.string());
      continue;
    }

    try {
      result.games.push_back(load_manifest(manifest_path));
    } catch (const std::exception& error) {
      result.warnings.push_back(error.what());
    }
  }

  std::sort(result.games.begin(), result.games.end(), [](const GameManifest& left,
                                                         const GameManifest& right) {
    return left.name < right.name;
  });

  return result;
}

}  // namespace pixelpal
