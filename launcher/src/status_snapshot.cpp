#include "pixelpal/status_snapshot.hpp"

#include <fstream>
#include <sstream>

namespace pixelpal {
namespace {

std::string slurp(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string extract_string(const std::string& body, const std::string& key) {
  const std::string token = "\"" + key + "\"";
  const std::size_t key_pos = body.find(token);
  if (key_pos == std::string::npos) {
    return {};
  }
  const std::size_t colon = body.find(':', key_pos);
  const std::size_t first_quote = body.find('"', colon + 1);
  const std::size_t second_quote = body.find('"', first_quote + 1);
  if (colon == std::string::npos || first_quote == std::string::npos ||
      second_quote == std::string::npos) {
    return {};
  }
  return body.substr(first_quote + 1, second_quote - first_quote - 1);
}

int extract_int(const std::string& body, const std::string& key) {
  const std::string token = "\"" + key + "\"";
  const std::size_t key_pos = body.find(token);
  if (key_pos == std::string::npos) {
    return -1;
  }
  const std::size_t colon = body.find(':', key_pos);
  if (colon == std::string::npos) {
    return -1;
  }
  std::size_t start = colon + 1;
  while (start < body.size() && (body[start] == ' ' || body[start] == '\t')) {
    ++start;
  }
  std::size_t end = start;
  while (end < body.size() && (body[end] == '-' || (body[end] >= '0' && body[end] <= '9'))) {
    ++end;
  }
  if (end == start) {
    return -1;
  }
  return std::stoi(body.substr(start, end - start));
}

bool extract_bool(const std::string& body, const std::string& key) {
  const std::string token = "\"" + key + "\"";
  const std::size_t key_pos = body.find(token);
  if (key_pos == std::string::npos) {
    return false;
  }
  const std::size_t colon = body.find(':', key_pos);
  if (colon == std::string::npos) {
    return false;
  }
  const std::size_t value_start = body.find_first_not_of(" \t", colon + 1);
  if (value_start == std::string::npos) {
    return false;
  }
  return body.compare(value_start, 4, "true") == 0;
}

}  // namespace

StatusSnapshot load_status_snapshot(const std::filesystem::path& status_dir) {
  StatusSnapshot snapshot;

  const std::string power = slurp(status_dir / "power.json");
  snapshot.battery_percent = extract_int(power, "battery_percent");
  snapshot.battery_charging = extract_bool(power, "charging");

  const std::string wifi = slurp(status_dir / "wifi.json");
  snapshot.wifi_connected = extract_bool(wifi, "connected");
  snapshot.wifi_ssid = extract_string(wifi, "ssid");

  const std::string audio = slurp(status_dir / "audio.json");
  snapshot.volume_percent = extract_int(audio, "volume_percent");

  return snapshot;
}

}  // namespace pixelpal

