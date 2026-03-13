#include "pixelpal/build_info.hpp"
#include "pixelpal/game_manifest.hpp"
#include "pixelpal/menu_audio.hpp"
#include "pixelpal/status_snapshot.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#endif

namespace {

constexpr int kWindowWidth = 512;
constexpr int kWindowHeight = 512;
constexpr const char* kBuildLabel = PIXELPAL_BUILD_LABEL;

struct Options {
  std::filesystem::path games_root;
  std::filesystem::path cache_root;
  std::filesystem::path status_root;
  std::filesystem::path runner;
  std::filesystem::path theme_root;
  bool list_only = false;
  std::optional<std::string> launch_id;
};

struct ThemeColors {
  SDL_Color background{214, 218, 180, 255};
  SDL_Color panel{194, 200, 162, 255};
  SDL_Color panel_shadow{157, 165, 126, 255};
  SDL_Color text{18, 71, 35, 255};
  SDL_Color highlight{41, 104, 51, 255};
  SDL_Color highlight_text{233, 238, 216, 255};
  SDL_Color muted{76, 108, 61, 255};
};

enum class ScreenMode {
  start,
  library,
};

struct ButtonState {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool a = false;
  bool b = false;
  bool start = false;
  bool select = false;

  bool pressed_up = false;
  bool pressed_down = false;
  bool pressed_left = false;
  bool pressed_right = false;
  bool pressed_a = false;
  bool pressed_b = false;
  bool pressed_start = false;
  bool pressed_select = false;
};

constexpr Uint32 kPostLaunchCooldownMs = 700;

std::filesystem::path executable_dir_from(const char* argv0) {
  if (argv0 == nullptr || argv0[0] == '\0') {
    return std::filesystem::current_path();
  }
  return std::filesystem::absolute(argv0).parent_path();
}

Options default_options(const std::filesystem::path& exe_dir) {
  Options options;
#if defined(_WIN32)
  const auto build_root = exe_dir.parent_path();
  options.games_root = build_root / "games";
  options.cache_root = build_root / "cache" / "manifests";
  options.status_root = build_root / "run" / "status";
  options.theme_root = build_root / "themes" / "default";
  options.runner.clear();
#else
  options.games_root = "/opt/pixelpal/games";
  options.cache_root = "/var/cache/pixelpal/manifests";
  options.status_root = "/run/pixelpal/status";
  options.runner = "/usr/lib/pixelpal/services/pixelpal-run.sh";
  options.theme_root = "/usr/share/pixelpal/themes/default";
#endif
  return options;
}

void print_usage() {
  std::printf("pixelpal-launcher [--list] [--launch <game-id>] [--games-root <path>] "
              "[--catalog <path>] [--status-root <path>] [--runner <path>] "
              "[--theme-root <path>]\n");
}

Options parse_args(int argc, char** argv) {
  Options options = default_options(executable_dir_from(argc > 0 ? argv[0] : nullptr));

  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--help") {
      print_usage();
      std::exit(0);
    }
    if (arg == "--list") {
      options.list_only = true;
      continue;
    }
    if (arg == "--launch" && index + 1 < argc) {
      options.launch_id = argv[++index];
      continue;
    }
    if (arg == "--games-root" && index + 1 < argc) {
      options.games_root = argv[++index];
      continue;
    }
    if (arg == "--catalog" && index + 1 < argc) {
      options.cache_root = argv[++index];
      continue;
    }
    if (arg == "--status-root" && index + 1 < argc) {
      options.status_root = argv[++index];
      continue;
    }
    if (arg == "--runner" && index + 1 < argc) {
      options.runner = argv[++index];
      continue;
    }
    if (arg == "--theme-root" && index + 1 < argc) {
      options.theme_root = argv[++index];
      continue;
    }
  }

  return options;
}

pixelpal::CatalogResult load_catalog(const Options& options) {
  if (std::filesystem::exists(options.cache_root)) {
    const auto cached = pixelpal::scan_catalog(options.cache_root);
    if (!cached.games.empty()) {
      return cached;
    }
  }
  return pixelpal::scan_installed_games(options.games_root);
}

const pixelpal::GameManifest* find_game(const pixelpal::CatalogResult& catalog,
                                        const std::string& id) {
  for (const auto& game : catalog.games) {
    if (game.id == id) {
      return &game;
    }
  }
  return nullptr;
}

std::filesystem::path resolve_game_executable(const pixelpal::GameManifest& game) {
  std::filesystem::path executable = game.root_dir / game.exec;
#if defined(_WIN32)
  if (!std::filesystem::exists(executable)) {
    const auto with_extension = executable.string() + ".exe";
    if (std::filesystem::exists(with_extension)) {
      executable = with_extension;
    }
  }
#endif
  return executable;
}

int launch_game(const Options& options, const pixelpal::GameManifest& game) {
  const auto original_cwd = std::filesystem::current_path();
  const auto executable = resolve_game_executable(game);
  std::ostringstream command;

  if (!options.runner.empty() && std::filesystem::exists(options.runner)) {
    command << '"' << options.runner.string() << '"' << ' ' << '"' << game.root_dir.string()
            << '"';
    return std::system(command.str().c_str());
  }

  if (!std::filesystem::exists(executable)) {
    return 127;
  }

  std::error_code error;
  command << '"' << executable.string() << '"';
  std::filesystem::current_path(executable.parent_path(), error);

  const int exit_code = std::system(command.str().c_str());
  std::filesystem::current_path(original_cwd, error);
  return exit_code;
}

std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
  return value;
}

std::array<uint8_t, 7> glyph_for(char ch) {
  switch (ch) {
    case 'A':
      return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'B':
      return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C':
      return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    case 'D':
      return {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
    case 'E':
      return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F':
      return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G':
      return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    case 'H':
      return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I':
      return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J':
      return {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    case 'K':
      return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L':
      return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M':
      return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N':
      return {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
    case 'O':
      return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P':
      return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'Q':
      return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case 'R':
      return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S':
      return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T':
      return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U':
      return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V':
      return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case 'W':
      return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    case 'X':
      return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y':
      return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case 'Z':
      return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    case '0':
      return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1':
      return {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F};
    case '2':
      return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3':
      return {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E};
    case '4':
      return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5':
      return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6':
      return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7':
      return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8':
      return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9':
      return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case '.':
      return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    case '-':
      return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case ':':
      return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case '%':
      return {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
    default:
      return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
}

int text_width(const std::string& text, int scale) {
  if (text.empty()) {
    return 0;
  }
  return static_cast<int>(text.size()) * (6 * scale) - scale;
}

void draw_text(SDL_Renderer* renderer,
               const std::string& text,
               int x,
               int y,
               int scale,
               SDL_Color color,
               bool centered) {
  const std::string upper = uppercase(text);
  int draw_x = x;

  if (centered) {
    draw_x -= text_width(upper, scale) / 2;
  }

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (char ch : upper) {
    const auto glyph = glyph_for(ch);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((glyph[row] & (1 << (4 - column))) == 0) {
          continue;
        }
        SDL_Rect pixel{draw_x + column * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    draw_x += 6 * scale;
  }
}

void draw_text_right(SDL_Renderer* renderer,
                     const std::string& text,
                     int right_x,
                     int y,
                     int scale,
                     SDL_Color color) {
  draw_text(renderer, text, right_x - text_width(text, scale), y, scale, color, false);
}

void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void update_button(bool& current, bool& pressed, bool down) {
  if (down && !current) {
    pressed = true;
  }
  current = down;
}

void reset_pressed(ButtonState& buttons) {
  buttons.pressed_up = false;
  buttons.pressed_down = false;
  buttons.pressed_left = false;
  buttons.pressed_right = false;
  buttons.pressed_a = false;
  buttons.pressed_b = false;
  buttons.pressed_start = false;
  buttons.pressed_select = false;
}

void clear_buttons(ButtonState& buttons) {
  buttons.up = false;
  buttons.down = false;
  buttons.left = false;
  buttons.right = false;
  buttons.a = false;
  buttons.b = false;
  buttons.start = false;
  buttons.select = false;
  reset_pressed(buttons);
}

bool any_action_held(const ButtonState& buttons) {
  return buttons.a || buttons.b || buttons.start || buttons.select;
}

void handle_key(ButtonState& buttons, SDL_Keycode key, bool down) {
  switch (key) {
    case SDLK_UP:
      update_button(buttons.up, buttons.pressed_up, down);
      break;
    case SDLK_DOWN:
      update_button(buttons.down, buttons.pressed_down, down);
      break;
    case SDLK_LEFT:
      update_button(buttons.left, buttons.pressed_left, down);
      break;
    case SDLK_RIGHT:
      update_button(buttons.right, buttons.pressed_right, down);
      break;
    case SDLK_z:
    case SDLK_c:
    case SDLK_a:
      update_button(buttons.a, buttons.pressed_a, down);
      break;
    case SDLK_b:
    case SDLK_x:
      update_button(buttons.b, buttons.pressed_b, down);
      break;
    case SDLK_RETURN:
      update_button(buttons.start, buttons.pressed_start, down);
      break;
    case SDLK_SPACE:
    case SDLK_RSHIFT:
    case SDLK_BACKSPACE:
    case SDLK_ESCAPE:
      update_button(buttons.select, buttons.pressed_select, down);
      break;
    default:
      break;
  }
}

void handle_controller(ButtonState& buttons, Uint8 button, bool down) {
  switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      update_button(buttons.up, buttons.pressed_up, down);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      update_button(buttons.down, buttons.pressed_down, down);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      update_button(buttons.left, buttons.pressed_left, down);
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      update_button(buttons.right, buttons.pressed_right, down);
      break;
    case SDL_CONTROLLER_BUTTON_A:
      update_button(buttons.a, buttons.pressed_a, down);
      break;
    case SDL_CONTROLLER_BUTTON_B:
      update_button(buttons.b, buttons.pressed_b, down);
      break;
    case SDL_CONTROLLER_BUTTON_START:
      update_button(buttons.start, buttons.pressed_start, down);
      break;
    case SDL_CONTROLLER_BUTTON_BACK:
      update_button(buttons.select, buttons.pressed_select, down);
      break;
    default:
      break;
  }
}

void poll_input(ButtonState& buttons, bool& running, SDL_GameController*& controller) {
  SDL_Event event;
  reset_pressed(buttons);

  while (SDL_PollEvent(&event) == 1) {
    switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_KEYDOWN:
        if (event.key.repeat == 0) {
          handle_key(buttons, event.key.keysym.sym, true);
        }
        break;
      case SDL_KEYUP:
        handle_key(buttons, event.key.keysym.sym, false);
        break;
      case SDL_CONTROLLERDEVICEADDED:
        if (controller == nullptr && SDL_IsGameController(event.cdevice.which) == SDL_TRUE) {
          controller = SDL_GameControllerOpen(event.cdevice.which);
        }
        break;
      case SDL_CONTROLLERDEVICEREMOVED:
        if (controller != nullptr &&
            SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) ==
                event.cdevice.which) {
          SDL_GameControllerClose(controller);
          controller = nullptr;
        }
        break;
      case SDL_CONTROLLERBUTTONDOWN:
        handle_controller(buttons, event.cbutton.button, true);
        break;
      case SDL_CONTROLLERBUTTONUP:
        handle_controller(buttons, event.cbutton.button, false);
        break;
      default:
        break;
    }
  }
}

void draw_background(SDL_Renderer* renderer, const ThemeColors& colors) {
  SDL_Rect body{0, 0, kWindowWidth, kWindowHeight};
  fill_rect(renderer, body, colors.background);

  SDL_Rect frame{28, 28, kWindowWidth - 56, kWindowHeight - 56};
  draw_rect(renderer, frame, colors.muted);
}

void draw_status_bar(SDL_Renderer* renderer,
                     const ThemeColors& colors,
                     const pixelpal::StatusSnapshot& status) {
  const std::string left = kBuildLabel;
  std::string right;
  if (status.wifi_connected) {
    right = "WIFI";
  } else {
    right = "OFFLINE";
  }
  draw_text(renderer, left, 36, 36, 2, colors.muted, false);
  draw_text_right(renderer, right, 476, 36, 2, colors.muted);
}

void draw_start_screen(SDL_Renderer* renderer,
                       const ThemeColors& colors,
                       Uint32 ticks,
                       const pixelpal::CatalogResult&) {
  draw_text(renderer, "PIXELPAL", kWindowWidth / 2, 164, 7, colors.text, true);

  if ((ticks / 500U) % 2U == 0U) {
    draw_text(renderer, "PRESS START", kWindowWidth / 2, 258, 4, colors.text, true);
  }

  draw_text(renderer, "MADE BY RYAN RAIKES", kWindowWidth / 2, 316, 2, colors.text, true);
}

void draw_library(SDL_Renderer* renderer,
                  const ThemeColors& colors,
                  const pixelpal::CatalogResult& catalog,
                  const pixelpal::StatusSnapshot& status,
                  std::size_t selected,
                  const std::string& message) {
  draw_text(renderer, "PIXELPAL OS", kWindowWidth / 2, 92, 4, colors.text, true);
  draw_status_bar(renderer, colors, status);

  if (catalog.games.empty()) {
    draw_text(renderer, "NO GAMES FOUND", kWindowWidth / 2, 204, 4, colors.text, true);
    draw_text(renderer, "CHECK WINDOWS STAGING", kWindowWidth / 2, 244, 2, colors.muted, true);
  } else {
    int card_y = 154;
    for (std::size_t index = 0; index < catalog.games.size(); ++index) {
      const bool active = index == selected;
      SDL_Rect shadow{76, card_y + 8, 360, 78};
      SDL_Rect card{70, card_y, 360, 78};
      fill_rect(renderer, shadow, colors.panel_shadow);
      fill_rect(renderer, card, active ? colors.highlight : colors.panel);
      draw_rect(renderer, card, colors.muted);

      draw_text(renderer, catalog.games[index].name, 250, card_y + 28, 3,
                active ? colors.highlight_text : colors.text, true);
      card_y += 90;
    }
  }

  if (!message.empty()) {
    draw_text(renderer, message, kWindowWidth / 2, 450, 2, colors.muted, true);
  }
}

std::string make_launch_message(int exit_code) {
  if (exit_code == 0) {
    return "";
  }
  if (exit_code == 127) {
    return "GAME BINARY NOT FOUND";
  }
  return "GAME EXIT " + std::to_string(exit_code);
}

}  // namespace

int main(int argc, char** argv) {
  const Options options = parse_args(argc, argv);
  pixelpal::MenuAudio audio;
  pixelpal::CatalogResult catalog = load_catalog(options);
  pixelpal::StatusSnapshot status = pixelpal::load_status_snapshot(options.status_root);

  if (options.list_only) {
    for (const auto& warning : catalog.warnings) {
      std::fprintf(stderr, "warning: %s\n", warning.c_str());
    }
    if (catalog.games.empty()) {
      std::printf("No games installed.\n");
      return 0;
    }
    std::printf("Installed games:\n");
    for (std::size_t index = 0; index < catalog.games.size(); ++index) {
      std::printf("  %zu. %s [%s]\n", index + 1, catalog.games[index].name.c_str(),
                  catalog.games[index].id.c_str());
    }
    return 0;
  }

  if (options.launch_id.has_value()) {
    const auto* game = find_game(catalog, *options.launch_id);
    if (game == nullptr) {
      std::fprintf(stderr, "Game not found: %s\n", options.launch_id->c_str());
      return 1;
    }
    return launch_game(options, *game);
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window =
      SDL_CreateWindow("PixelPal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_GameController* controller = nullptr;
  if (SDL_NumJoysticks() > 0) {
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
      if (SDL_IsGameController(index) == SDL_TRUE) {
        controller = SDL_GameControllerOpen(index);
        break;
      }
    }
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  audio.initialize(options.theme_root);

  const ThemeColors colors;
  ScreenMode screen = ScreenMode::start;
  ButtonState buttons;
  bool running = true;
  std::size_t selected = 0;
  std::string message = catalog.games.empty() ? "NO GAMES STAGED YET" : "";
  Uint32 input_lock_until = 0;
  bool wait_for_release = false;

  while (running) {
    poll_input(buttons, running, controller);
    if (wait_for_release && !any_action_held(buttons)) {
      wait_for_release = false;
    }
    if (SDL_GetTicks() < input_lock_until || wait_for_release) {
      reset_pressed(buttons);
    }

    if (screen == ScreenMode::start) {
      if (buttons.pressed_start || buttons.pressed_a) {
        audio.play_confirm();
        screen = ScreenMode::library;
      }
      if (buttons.pressed_select) {
        audio.play_back();
        running = false;
      }
    } else {
      if (buttons.pressed_b) {
        audio.play_back();
        screen = ScreenMode::start;
      }
      if (buttons.pressed_select) {
        audio.play_back();
        running = false;
      }
      if (!catalog.games.empty()) {
        if (buttons.pressed_down) {
          audio.play_move();
          selected = (selected + 1) % catalog.games.size();
        }
        if (buttons.pressed_up) {
          audio.play_move();
          selected = selected == 0 ? catalog.games.size() - 1 : selected - 1;
        }
        if (buttons.pressed_a || buttons.pressed_start) {
          audio.play_confirm();
          SDL_HideWindow(window);
          const int exit_code = launch_game(options, catalog.games[selected]);
          SDL_ShowWindow(window);
          SDL_RaiseWindow(window);
          SDL_FlushEvent(SDL_KEYDOWN);
          SDL_FlushEvent(SDL_KEYUP);
          SDL_FlushEvent(SDL_CONTROLLERBUTTONDOWN);
          SDL_FlushEvent(SDL_CONTROLLERBUTTONUP);
          clear_buttons(buttons);
          input_lock_until = SDL_GetTicks() + kPostLaunchCooldownMs;
          wait_for_release = true;
          catalog = load_catalog(options);
          status = pixelpal::load_status_snapshot(options.status_root);
          if (selected >= catalog.games.size() && !catalog.games.empty()) {
            selected = catalog.games.size() - 1;
          } else if (catalog.games.empty()) {
            selected = 0;
          }
          message = make_launch_message(exit_code);
        }
      }
    }

    draw_background(renderer, colors);
    if (screen == ScreenMode::start) {
      draw_start_screen(renderer, colors, SDL_GetTicks(), catalog);
    } else {
      draw_library(renderer, colors, catalog, status, selected, message);
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  if (controller != nullptr) {
    SDL_GameControllerClose(controller);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
