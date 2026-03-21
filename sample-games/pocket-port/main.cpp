#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int kVirtualWidth = 512;
constexpr int kVirtualHeight = 512;

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct RomEntry {
  std::string title;
  std::string system;
  std::filesystem::path path;
};

struct GameState {
  pp_context context{};
  ToneState tone{};
  std::vector<RomEntry> entries;
  std::vector<int> filtered_entries;
  std::map<std::string, std::string> runners;
  int filter_index = 0;
  int menu_index = 0;
  std::string status = "DROP ROMS INTO CONFIG/ROMS";
  std::string detail = "A LAUNCH  START FILTER  B REFRESH";
};

static const SDL_Color kBg{14, 18, 28, 255};
static const SDL_Color kPanel{24, 31, 46, 238};
static const SDL_Color kPanelHi{38, 50, 72, 240};
static const SDL_Color kFrame{92, 122, 168, 255};
static const SDL_Color kText{236, 244, 255, 255};
static const SDL_Color kMuted{145, 164, 192, 255};
static const SDL_Color kAccent{247, 196, 102, 255};
static const SDL_Color kGood{122, 231, 182, 255};
static const SDL_Color kWarn{255, 186, 104, 255};

std::array<uint8_t, 7> glyph_for(char ch) {
  switch (ch) {
    case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    case 'D': return {0x1E, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1E};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J': return {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1': return {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F};
    case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3': return {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E};
    case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    default: return {0, 0, 0, 0, 0, 0, 0};
  }
}

std::string upper(std::string text) {
  for (char& ch : text) {
    if (ch >= 'a' && ch <= 'z') {
      ch = static_cast<char>(ch - 'a' + 'A');
    }
  }
  return text;
}

int text_width(const std::string& text, int scale) {
  return text.empty() ? 0 : static_cast<int>(text.size()) * (6 * scale) - scale;
}

std::string fit_text(std::string text, int max_width, int scale) {
  if (text_width(text, scale) <= max_width) return text;
  const std::string ellipsis = "...";
  while (!text.empty() && text_width(text + ellipsis, scale) > max_width) {
    text.pop_back();
  }
  return text.empty() ? ellipsis : text + ellipsis;
}

void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, int scale, SDL_Color color, bool centered = false) {
  const std::string upper_text = upper(text);
  int draw_x = centered ? x - text_width(upper_text, scale) / 2 : x;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (char ch : upper_text) {
    if (ch == ' ') {
      draw_x += 6 * scale;
      continue;
    }
    const auto glyph = glyph_for(ch);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((glyph[static_cast<std::size_t>(row)] & (1 << (4 - col))) == 0) continue;
        SDL_Rect pixel{draw_x + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    draw_x += 6 * scale;
  }
}

void draw_text_right(SDL_Renderer* renderer, const std::string& text, int right_x, int y, int scale, SDL_Color color) {
  const std::string upper_text = upper(text);
  draw_text(renderer, upper_text, right_x - text_width(upper_text, scale), y, scale, color);
}

std::vector<std::string> wrap_text(const std::string& text, int max_width, int scale) {
  std::vector<std::string> lines;
  std::string current;
  std::string word;
  auto flush_word = [&]() {
    if (word.empty()) return;
    std::string candidate = current.empty() ? word : current + " " + word;
    if (!current.empty() && text_width(candidate, scale) > max_width) {
      lines.push_back(current);
      current = word;
    } else {
      current = candidate;
    }
    word.clear();
  };
  for (char ch : text) {
    if (ch == ' ') {
      flush_word();
    } else {
      word.push_back(ch);
    }
  }
  flush_word();
  if (!current.empty()) lines.push_back(current);
  return lines;
}

void draw_wrapped_text(SDL_Renderer* renderer, const std::string& text, int x, int y, int width, int scale, int max_lines, SDL_Color color) {
  const auto lines = wrap_text(text, width, scale);
  for (int index = 0; index < static_cast<int>(lines.size()) && index < max_lines; ++index) {
    draw_text(renderer, lines[static_cast<std::size_t>(index)], x, y + index * (8 * scale + 2), scale, color);
  }
}

void audio_callback(void* userdata, Uint8* stream, int length) {
  ToneState* tone = static_cast<ToneState*>(userdata);
  auto* samples = reinterpret_cast<int16_t*>(stream);
  const int count = length / static_cast<int>(sizeof(int16_t));
  for (int index = 0; index < count; ++index) {
    int16_t sample = 0;
    if (tone->frames_remaining > 0) {
      sample = tone->phase < 3.14159f ? 1500 : -1500;
      tone->phase += (6.28318f * tone->frequency) / 48000.0f;
      if (tone->phase >= 6.28318f) tone->phase -= 6.28318f;
      --tone->frames_remaining;
    }
    samples[index] = sample;
  }
}

void trigger_tone(ToneState& tone, float frequency, int milliseconds) {
  tone.frequency = frequency;
  tone.frames_remaining = (48000 * milliseconds) / 1000;
}

std::filesystem::path config_root(const GameState& game) {
  return std::filesystem::path(pp_get_config_dir(&game.context));
}

std::filesystem::path rom_root(const GameState& game) {
  return config_root(game) / "roms";
}

std::filesystem::path runner_config_path(const GameState& game) {
  return config_root(game) / "runners.txt";
}

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.pop_back();
  return value;
}

std::string title_from_path(const std::filesystem::path& path) {
  std::string text = path.stem().string();
  for (char& ch : text) {
    if (ch == '_' || ch == '-') ch = ' ';
  }
  return text.empty() ? path.filename().string() : text;
}

void ensure_layout(GameState& game) {
  std::filesystem::create_directories(rom_root(game) / "gb");
  std::filesystem::create_directories(rom_root(game) / "gbc");
  std::filesystem::create_directories(rom_root(game) / "gba");
  const auto config_path = runner_config_path(game);
  if (!std::filesystem::exists(config_path)) {
    std::ofstream output(config_path);
    output << "# One executable path or command name per system.\n";
    output << "# mGBA handles GB / GBC / GBA and keeps native aspect with padding.\n";
    output << "gb=mgba\n";
    output << "gbc=mgba\n";
    output << "gba=mgba\n";
  }
}

void load_runners(GameState& game) {
  game.runners.clear();
  game.runners["gb"] = "mgba";
  game.runners["gbc"] = "mgba";
  game.runners["gba"] = "mgba";

  std::ifstream input(runner_config_path(game));
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) line = line.substr(0, comment);
    const std::size_t separator = line.find('=');
    if (separator == std::string::npos) continue;
    const std::string key = trim(line.substr(0, separator));
    const std::string value = trim(line.substr(separator + 1));
    if ((key == "gb" || key == "gbc" || key == "gba") && !value.empty()) {
      game.runners[key] = value;
    }
  }
}

bool rom_extension_matches(const std::filesystem::path& path, const std::string& system) {
  std::string ext = upper(path.extension().string());
  if (system == "gb") return ext == ".GB";
  if (system == "gbc") return ext == ".GBC" || ext == ".CGB";
  if (system == "gba") return ext == ".GBA";
  return false;
}

void rebuild_filter(GameState& game) {
  static const std::array<const char*, 4> kFilters{{"ALL", "GB", "GBC", "GBA"}};
  game.filtered_entries.clear();
  for (int index = 0; index < static_cast<int>(game.entries.size()); ++index) {
    if (game.filter_index == 0 || upper(game.entries[static_cast<std::size_t>(index)].system) == kFilters[static_cast<std::size_t>(game.filter_index)]) {
      game.filtered_entries.push_back(index);
    }
  }
  if (game.filtered_entries.empty()) {
    game.menu_index = 0;
  } else if (game.menu_index >= static_cast<int>(game.filtered_entries.size())) {
    game.menu_index = static_cast<int>(game.filtered_entries.size()) - 1;
  }
}

void scan_roms(GameState& game) {
  game.entries.clear();
  for (const std::string& system : {"gb", "gbc", "gba"}) {
    const auto root = rom_root(game) / system;
    if (!std::filesystem::exists(root)) continue;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
      if (!entry.is_regular_file()) continue;
      if (!rom_extension_matches(entry.path(), system)) continue;
      game.entries.push_back({title_from_path(entry.path()), system, entry.path()});
    }
  }

  std::sort(game.entries.begin(), game.entries.end(), [](const RomEntry& left, const RomEntry& right) {
    if (left.system != right.system) return left.system < right.system;
    return left.title < right.title;
  });

  rebuild_filter(game);
  if (game.entries.empty()) {
    game.status = "NO ROMS FOUND";
    game.detail = "PUT .GB / .GBC / .GBA FILES IN CONFIG/ROMS";
  } else {
    game.status = "ROM SHELF READY";
    game.detail = "A LAUNCH  START FILTER  B REFRESH";
  }
}

bool launch_selected_rom(SDL_Window* window, GameState& game) {
  if (game.filtered_entries.empty()) {
    game.status = "NO ROM TO LAUNCH";
    return false;
  }

  const RomEntry& rom = game.entries[static_cast<std::size_t>(game.filtered_entries[static_cast<std::size_t>(game.menu_index)])];
  const std::string runner = game.runners[rom.system];
  if (runner.empty()) {
    game.status = "NO RUNNER FOR " + upper(rom.system);
    game.detail = "EDIT RUNNERS.TXT IN CONFIG";
    return false;
  }

  const std::string command = "\"" + runner + "\" \"" + rom.path.string() + "\"";
  game.status = "LAUNCHING " + upper(rom.system) + " ROM";
  game.detail = rom.title;
  trigger_tone(game.tone, 820.0f, 70);
  SDL_MinimizeWindow(window);
  const int result = std::system(command.c_str());
  SDL_RestoreWindow(window);
  if (result != 0) {
    game.status = "RUNNER FAILED";
    game.detail = "CHECK RUNNERS.TXT OR INSTALL MGBA";
    return false;
  }
  game.status = "RETURNED FROM ROM";
  game.detail = rom.title;
  return true;
}

void draw_header(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Rect box{18, 18, 476, 68};
  fill_rect(renderer, box, kPanel);
  draw_rect(renderer, box, kFrame);
  draw_text(renderer, "POCKET PORT", 36, 30, 4, kText);
  draw_text(renderer, "GB / GBC / GBA ROM SHELF", 38, 62, 1, kMuted);
  draw_text_right(renderer, "NATIVE ASPECT", box.x + box.w - 16, 42, 1, kAccent);
  draw_text_right(renderer, "PADDED BY YOUR CORE", box.x + box.w - 16, 58, 1, kMuted);
}

void draw_filters(SDL_Renderer* renderer, const GameState& game) {
  static const std::array<const char*, 4> kFilters{{"ALL", "GB", "GBC", "GBA"}};
  for (int index = 0; index < 4; ++index) {
    SDL_Rect pill{24 + index * 74, 98, 64, 22};
    fill_rect(renderer, pill, index == game.filter_index ? kPanelHi : kPanel);
    draw_rect(renderer, pill, kFrame);
    draw_text(renderer, kFilters[static_cast<std::size_t>(index)], pill.x + pill.w / 2, pill.y + 6, 2,
              index == game.filter_index ? kAccent : kText, true);
  }
}

void draw_list(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Rect box{18, 132, 476, 264};
  fill_rect(renderer, box, kPanel);
  draw_rect(renderer, box, kFrame);
  draw_text(renderer, "ROMS", box.x + 14, box.y + 10, 2, kText);

  if (game.filtered_entries.empty()) {
    draw_text(renderer, "DROP ROMS INTO CONFIG/ROMS/GB, GBC, OR GBA", box.x + 18, box.y + 52, 1, kWarn);
    draw_text(renderer, "EDIT RUNNERS.TXT IF MGBA IS NOT ON PATH", box.x + 18, box.y + 70, 1, kMuted);
    draw_text(renderer, "REAL CART SLOT SUPPORT CAN PLUG INTO THIS SHELF LATER", box.x + 18, box.y + 88, 1, kMuted);
    return;
  }

  const int visible_count = 10;
  const int start = std::max(0, std::min(game.menu_index - visible_count / 2,
                                         static_cast<int>(game.filtered_entries.size()) - visible_count));
  const int end = std::min(start + visible_count, static_cast<int>(game.filtered_entries.size()));
  for (int row = start; row < end; ++row) {
    const RomEntry& entry = game.entries[static_cast<std::size_t>(game.filtered_entries[static_cast<std::size_t>(row)])];
    const SDL_Rect item{box.x + 12, box.y + 32 + (row - start) * 22, box.w - 24, 18};
    if (row == game.menu_index) fill_rect(renderer, item, kPanelHi);
    draw_text(renderer, upper(entry.system), item.x + 6, item.y + 3, 1, row == game.menu_index ? kAccent : kMuted);
    draw_text(renderer, fit_text(entry.title, item.w - 52, 1), item.x + 46, item.y + 3, 1, kText);
  }
}

void draw_footer(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {18, 408, 476, 86}, kPanel);
  draw_rect(renderer, {18, 408, 476, 86}, kFrame);
  draw_text(renderer, "STATUS", 30, 420, 1, kMuted);
  draw_text(renderer, game.status, 30, 438, 2, kGood);
  draw_wrapped_text(renderer, game.detail, 30, 462, 360, 1, 2, kText);
  draw_text(renderer, "A LAUNCH", 400, 420, 1, kText);
  draw_text(renderer, "START FILTER", 400, 438, 1, kText);
  draw_text(renderer, "B REFRESH", 400, 456, 1, kText);
  draw_text(renderer, "SELECT EXIT", 400, 474, 1, kMuted);
}

void render_scene(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {0, 0, kVirtualWidth, kVirtualHeight}, kBg);
  draw_header(renderer, game);
  draw_filters(renderer, game);
  draw_list(renderer, game);
  draw_footer(renderer, game);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) return 1;

  GameState game{};
  if (pp_init(&game.context, "pocket-port") != 0) {
    SDL_Quit();
    return 1;
  }

  ensure_layout(game);
  load_runners(game);
  scan_roms(game);

  int width = kVirtualWidth;
  int height = kVirtualHeight;
  pp_get_framebuffer_size(&game.context, &width, &height);
  width = std::max(width, kVirtualWidth);
  height = std::max(height, kVirtualHeight);

  SDL_Window* window = SDL_CreateWindow("Pocket Port", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    pp_shutdown(&game.context);
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (renderer == nullptr) {
    SDL_DestroyWindow(window);
    pp_shutdown(&game.context);
    SDL_Quit();
    return 1;
  }

  SDL_RenderSetLogicalSize(renderer, kVirtualWidth, kVirtualHeight);

  pp_audio_spec audio_spec{};
  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &game.tone;
  SDL_AudioDeviceID audio_device = 0;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) SDL_PauseAudioDevice(audio_device, 0);

  pp_input_state input{};
  pp_input_state previous{};
  while (!pp_should_exit(&game.context)) {
    pp_poll_input(&game.context, &input);

    if (input.up && !previous.up && !game.filtered_entries.empty()) {
      game.menu_index = (game.menu_index + static_cast<int>(game.filtered_entries.size()) - 1) % static_cast<int>(game.filtered_entries.size());
      trigger_tone(game.tone, 460.0f, 24);
    }
    if (input.down && !previous.down && !game.filtered_entries.empty()) {
      game.menu_index = (game.menu_index + 1) % static_cast<int>(game.filtered_entries.size());
      trigger_tone(game.tone, 460.0f, 24);
    }
    if (input.start && !previous.start) {
      game.filter_index = (game.filter_index + 1) % 4;
      rebuild_filter(game);
      trigger_tone(game.tone, 520.0f, 34);
    }
    if (input.b && !previous.b) {
      load_runners(game);
      scan_roms(game);
      trigger_tone(game.tone, 340.0f, 40);
    }
    if (input.a && !previous.a) {
      launch_selected_rom(window, game);
    }
    if (input.select && !previous.select) {
      pp_request_exit(&game.context);
    }

    render_scene(renderer, game);
    SDL_RenderPresent(renderer);
    previous = input;
    SDL_Delay(16);
  }

  if (audio_device != 0U) SDL_CloseAudioDevice(audio_device);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&game.context);
  SDL_Quit();
  return 0;
}
