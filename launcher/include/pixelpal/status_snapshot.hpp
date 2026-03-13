#pragma once

#include <filesystem>
#include <string>

namespace pixelpal {

struct StatusSnapshot {
  int battery_percent = -1;
  bool battery_charging = false;
  bool wifi_connected = false;
  std::string wifi_ssid;
  int volume_percent = -1;
};

StatusSnapshot load_status_snapshot(const std::filesystem::path& status_dir);

}  // namespace pixelpal

