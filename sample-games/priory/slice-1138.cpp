#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kScreenWidth = 256;
constexpr int kScreenHeight = 224;
constexpr int kTileSize = 16;
constexpr Uint32 kWalkStepMs = 150;
constexpr Uint32 kRunStepMs = 95;
constexpr Uint32 kAreaBannerMs = 2200;
constexpr Uint32 kStatusMessageMs = 1800;
constexpr Uint32 kWarpCooldownMs = 250;
constexpr int kDialogHeight = 62;

constexpr char kGrass = '.';
constexpr char kFlower = ',';
constexpr char kPath = ':';
constexpr char kStone = ';';
constexpr char kWater = '~';
constexpr char kTree = 'T';
constexpr char kRoof = '^';
constexpr char kWall = '%';
constexpr char kDoor = '+';
constexpr char kWood = '=';
constexpr char kFloor = '_';
constexpr char kPew = '|';
constexpr char kAltar = '!';
constexpr char kDesk = 'd';
constexpr char kShelf = 'k';
constexpr char kCandle = 'c';
constexpr char kGarden = 'g';
constexpr char kFountain = 'o';
constexpr char kStall = 's';
constexpr char kGlass = 'v';
constexpr char kWindow = 'w';

enum class Direction {
  None = 0,
  Up,
  Down,
  Left,
  Right,
};

enum class SpriteRole {
  Player,
  Prior,
  Sister,
  Monk,
  Fisher,
  Merchant,
  Child,
  Elder,
  Watchman,
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

struct SpritePalette {
  SDL_Color outline;
  SDL_Color skin;
  SDL_Color hair;
  SDL_Color primary;
  SDL_Color secondary;
  SDL_Color accent;
  SDL_Color boots;
};

struct Warp {
  int x = 0;
  int y = 0;
  std::string target_area;
  int target_x = 0;
  int target_y = 0;
  Direction target_facing = Direction::Down;
};

struct Npc {
  std::string id;
  std::string name;
  SpriteRole role = SpriteRole::Monk;
  int x = 0;
  int y = 0;
  Direction facing = Direction::Down;
  bool solid = true;
};

struct Area {
  std::string id;
  std::string name;
  bool indoor = false;
  std::vector<std::string> tiles;
  std::vector<Warp> warps;
  std::vector<Npc> npcs;
};

struct DialogueScene {
  bool active = false;
  std::string speaker;
  std::vector<std::string> pages;
  std::size_t page_index = 0;
};

struct Player {
  int tile_x = 13;
  int tile_y = 14;
  int start_x = 13;
  int start_y = 14;
  int target_x = 13;
  int target_y = 14;
  Uint32 step_started_at = 0;
  Uint32 step_duration = kWalkStepMs;
  bool moving = false;
  Direction facing = Direction::Down;
};

struct QuestState {
  bool started = false;
  bool wax = false;
  bool hymn = false;
  bool oil = false;
  bool complete = false;
};

struct Note {
  float frequency = 0.0f;
  int duration_ms = 0;
};

struct AudioState {
  float melody_phase = 0.0f;
  float melody_frequency = 0.0f;
  int melody_samples_remaining = 0;
  std::size_t note_index = 0;

  float bell_phase = 0.0f;
  float bell_frequency = 0.0f;
  int bell_samples_remaining = 0;
};

struct GameState {
  pp_context* context = nullptr;
  std::vector<Area> world;
  std::string current_area = "priory-court";
  Player player;
  DialogueScene dialogue;
  QuestState quest;
  bool journal_open = false;
  bool on_title = true;
  bool has_save = false;
  int title_selection = 0;
  std::string status_message;
  Uint32 status_until = 0;
  std::string area_banner = "PRIORY COURT";
  Uint32 area_banner_until = 0;
  Uint32 warp_cooldown_until = 0;
  Uint32 bells_until = 0;
  bool smoke_test = false;
};

Direction opposite(Direction direction) {
  switch (direction) {
    case Direction::Up:
      return Direction::Down;
    case Direction::Down:
      return Direction::Up;
    case Direction::Left:
      return Direction::Right;
    case Direction::Right:
      return Direction::Left;
    default:
      return Direction::None;
  }
}

void direction_delta(Direction direction, int* dx, int* dy) {
  *dx = 0;
  *dy = 0;
  switch (direction) {
    case Direction::Up:
      *dy = -1;
      break;
    case Direction::Down:
      *dy = 1;
      break;
    case Direction::Left:
      *dx = -1;
      break;
    case Direction::Right:
      *dx = 1;
      break;
    default:
      break;
  }
}

ButtonState make_buttons(const pp_input_state& current, const pp_input_state& previous) {
  ButtonState buttons;
  buttons.up = current.up != 0;
  buttons.down = current.down != 0;
  buttons.left = current.left != 0;
  buttons.right = current.right != 0;
  buttons.a = current.a != 0;
  buttons.b = current.b != 0;
  buttons.start = current.start != 0;
  buttons.select = current.select != 0;
  buttons.pressed_up = buttons.up && previous.up == 0;
  buttons.pressed_down = buttons.down && previous.down == 0;
  buttons.pressed_left = buttons.left && previous.left == 0;
  buttons.pressed_right = buttons.right && previous.right == 0;
  buttons.pressed_a = buttons.a && previous.a == 0;
  buttons.pressed_b = buttons.b && previous.b == 0;
  buttons.pressed_start = buttons.start && previous.start == 0;
  buttons.pressed_select = buttons.select && previous.select == 0;
  return buttons;
}

SDL_Color darken(SDL_Color color, int amount) {
  color.r = static_cast<Uint8>(std::max(0, static_cast<int>(color.r) - amount));
  color.g = static_cast<Uint8>(std::max(0, static_cast<int>(color.g) - amount));
  color.b = static_cast<Uint8>(std::max(0, static_cast<int>(color.b) - amount));
  return color;
}

SDL_Color lighten(SDL_Color color, int amount) {
  color.r = static_cast<Uint8>(std::min(255, static_cast<int>(color.r) + amount));
  color.g = static_cast<Uint8>(std::min(255, static_cast<int>(color.g) + amount));
  color.b = static_cast<Uint8>(std::min(255, static_cast<int>(color.b) + amount));
  return color;
}

void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void draw_pixel(SDL_Renderer* renderer, int x, int y, SDL_Color color, int size = 1) {
  fill_rect(renderer, SDL_Rect{x, y, size, size}, color);
}

void fill_circle(SDL_Renderer* renderer, int center_x, int center_y, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int dy = -radius; dy <= radius; ++dy) {
    const int span = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    SDL_RenderDrawLine(renderer, center_x - span, center_y + dy, center_x + span, center_y + dy);
  }
}

int hash_xy(int x, int y, int seed) {
  int value = x * 374761393 + y * 668265263 + seed * 982451653;
  value = (value ^ (value >> 13)) * 1274126177;
  return value ^ (value >> 16);
}

std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
  return value;
}

std::array<uint8_t, 7> glyph_for(char ch) {
  switch (ch) {
    case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    case 'D': return {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J': return {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
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
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    case ',': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x08};
    case '!': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
    case '?': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case ';': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x08, 0x00};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case '\'': return {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00};
    default: return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
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
               bool centered = false) {
  const std::string upper = uppercase(text);
  int draw_x = centered ? x - text_width(upper, scale) / 2 : x;
  for (char ch : upper) {
    const auto glyph = glyph_for(ch);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((glyph[row] & (1 << (4 - col))) != 0) {
          fill_rect(renderer,
                    SDL_Rect{draw_x + col * scale, y + row * scale, scale, scale},
                    color);
        }
      }
    }
    draw_x += 6 * scale;
  }
}

std::vector<std::string> wrap_text(const std::string& text, int max_width, int scale) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string word;
  std::string current;

  while (stream >> word) {
    const std::string candidate = current.empty() ? word : current + " " + word;
    if (!current.empty() && text_width(uppercase(candidate), scale) > max_width) {
      lines.push_back(current);
      current = word;
    } else {
      current = candidate;
    }
  }
  if (!current.empty()) {
    lines.push_back(current);
  }
  if (lines.empty()) {
    lines.push_back("");
  }
  return lines;
}

SpritePalette palette_for(SpriteRole role) {
  switch (role) {
    case SpriteRole::Player:
      return {{35, 39, 49, 255}, {245, 216, 182, 255}, {87, 63, 42, 255},
              {70, 115, 168, 255}, {232, 211, 152, 255}, {248, 236, 190, 255},
              {92, 68, 52, 255}};
    case SpriteRole::Prior:
      return {{35, 30, 34, 255}, {236, 204, 178, 255}, {197, 199, 205, 255},
              {124, 54, 70, 255}, {210, 187, 120, 255}, {240, 218, 152, 255},
              {86, 59, 43, 255}};
    case SpriteRole::Sister:
      return {{25, 28, 34, 255}, {238, 212, 188, 255}, {17, 20, 24, 255},
              {52, 54, 68, 255}, {228, 224, 215, 255}, {240, 201, 125, 255},
              {64, 45, 35, 255}};
    case SpriteRole::Fisher:
      return {{31, 41, 45, 255}, {228, 197, 165, 255}, {132, 78, 44, 255},
              {53, 124, 121, 255}, {220, 201, 150, 255}, {250, 232, 180, 255},
              {96, 65, 44, 255}};
    case SpriteRole::Merchant:
      return {{34, 39, 40, 255}, {229, 198, 172, 255}, {83, 56, 34, 255},
              {86, 131, 76, 255}, {211, 188, 117, 255}, {245, 227, 167, 255},
              {90, 63, 42, 255}};
    case SpriteRole::Child:
      return {{33, 37, 46, 255}, {244, 214, 185, 255}, {112, 80, 47, 255},
              {182, 97, 70, 255}, {233, 199, 131, 255}, {252, 231, 169, 255},
              {85, 62, 48, 255}};
    case SpriteRole::Elder:
      return {{35, 37, 39, 255}, {234, 207, 182, 255}, {175, 173, 168, 255},
              {124, 104, 88, 255}, {215, 196, 145, 255}, {242, 223, 168, 255},
              {90, 70, 52, 255}};
    case SpriteRole::Watchman:
      return {{27, 36, 44, 255}, {232, 205, 178, 255}, {61, 53, 40, 255},
              {73, 91, 130, 255}, {201, 192, 156, 255}, {237, 223, 160, 255},
              {84, 62, 48, 255}};
    default:
      return {{34, 34, 40, 255}, {237, 208, 180, 255}, {101, 65, 38, 255},
              {111, 90, 58, 255}, {194, 174, 128, 255}, {235, 212, 161, 255},
              {89, 64, 43, 255}};
  }
}

void render_character(SDL_Renderer* renderer,
                      SpriteRole role,
                      Direction facing,
                      bool walking,
                      int x,
                      int y,
                      Uint32 now) {
  const SpritePalette palette = palette_for(role);
  const int frame = walking ? static_cast<int>((now / 160U) % 2U) : 0;
  const int offset_y = walking ? ((now / 240U) % 2U == 0 ? 0 : 1) : 0;
  const SDL_Color shadow = {0, 0, 0, 70};

  fill_rect(renderer, SDL_Rect{x + 4, y + 13, 8, 2}, shadow);
  fill_rect(renderer, SDL_Rect{x + 5, y + 12, 6, 1}, shadow);

  fill_rect(renderer, SDL_Rect{x + 3, y + 2 + offset_y, 10, 2}, palette.outline);
  fill_rect(renderer, SDL_Rect{x + 2, y + 4 + offset_y, 12, 3}, palette.outline);
  fill_rect(renderer, SDL_Rect{x + 3, y + 7 + offset_y, 10, 1}, palette.outline);
  fill_rect(renderer, SDL_Rect{x + 4, y + 8 + offset_y, 8, 1}, palette.skin);

  if (role == SpriteRole::Sister) {
    fill_rect(renderer, SDL_Rect{x + 4, y + 2 + offset_y, 8, 2}, palette.hair);
    fill_rect(renderer, SDL_Rect{x + 3, y + 4 + offset_y, 2, 4}, palette.secondary);
    fill_rect(renderer, SDL_Rect{x + 11, y + 4 + offset_y, 2, 4}, palette.secondary);
    fill_rect(renderer, SDL_Rect{x + 5, y + 3 + offset_y, 6, 1}, palette.secondary);
  } else {
    fill_rect(renderer, SDL_Rect{x + 4, y + 2 + offset_y, 8, 2}, palette.hair);
    fill_rect(renderer, SDL_Rect{x + 3, y + 4 + offset_y, 10, 1}, palette.hair);
  }

  if (facing == Direction::Up) {
    fill_rect(renderer, SDL_Rect{x + 4, y + 5 + offset_y, 8, 2}, palette.hair);
  } else {
    fill_rect(renderer, SDL_Rect{x + 4, y + 4 + offset_y, 8, 3}, palette.skin);
    if (facing == Direction::Down) {
      draw_pixel(renderer, x + 6, y + 5 + offset_y, palette.outline);
      draw_pixel(renderer, x + 9, y + 5 + offset_y, palette.outline);
    } else if (facing == Direction::Left) {
      draw_pixel(renderer, x + 5, y + 5 + offset_y, palette.outline);
    } else if (facing == Direction::Right) {
      draw_pixel(renderer, x + 10, y + 5 + offset_y, palette.outline);
    }
  }

  fill_rect(renderer, SDL_Rect{x + 4, y + 7 + offset_y, 8, 1}, palette.accent);
  fill_rect(renderer, SDL_Rect{x + 3, y + 8 + offset_y, 10, 3}, palette.primary);
  fill_rect(renderer, SDL_Rect{x + 4, y + 11 + offset_y, 8, 1}, palette.secondary);

  if (facing == Direction::Left) {
    fill_rect(renderer, SDL_Rect{x + 2, y + 8 + offset_y, 3, 4}, palette.primary);
    fill_rect(renderer, SDL_Rect{x + 11, y + 8 + offset_y, 2, 4}, palette.primary);
  } else if (facing == Direction::Right) {
    fill_rect(renderer, SDL_Rect{x + 11, y + 8 + offset_y, 3, 4}, palette.primary);
    fill_rect(renderer, SDL_Rect{x + 3, y + 8 + offset_y, 2, 4}, palette.primary);
  } else {
    fill_rect(renderer, SDL_Rect{x + 2, y + 8 + offset_y, 2, 4}, palette.primary);
    fill_rect(renderer, SDL_Rect{x + 12, y + 8 + offset_y, 2, 4}, palette.primary);
  }

  if (frame == 0) {
    fill_rect(renderer, SDL_Rect{x + 4, y + 12 + offset_y, 3, 2}, palette.boots);
    fill_rect(renderer, SDL_Rect{x + 9, y + 12 + offset_y, 3, 2}, palette.boots);
  } else {
    fill_rect(renderer, SDL_Rect{x + 3, y + 12 + offset_y, 3, 2}, palette.boots);
    fill_rect(renderer, SDL_Rect{x + 10, y + 12 + offset_y, 3, 2}, palette.boots);
  }

  if (role == SpriteRole::Prior) {
    fill_rect(renderer, SDL_Rect{x + 5, y + 9 + offset_y, 6, 1}, palette.accent);
  } else if (role == SpriteRole::Watchman) {
    fill_rect(renderer, SDL_Rect{x + 4, y + 1 + offset_y, 8, 1}, palette.secondary);
  } else if (role == SpriteRole::Fisher) {
    fill_rect(renderer, SDL_Rect{x + 4, y + 1 + offset_y, 8, 1}, palette.secondary);
    fill_rect(renderer, SDL_Rect{x + 6, y + 0 + offset_y, 4, 1}, palette.secondary);
  }
}

std::vector<std::string> blank_rows(int width, int height, char fill) {
  return std::vector<std::string>(height, std::string(width, fill));
}

void fill_tiles(std::vector<std::string>* rows, int x, int y, int width, int height, char tile) {
  for (int row = y; row < y + height; ++row) {
    if (row < 0 || row >= static_cast<int>(rows->size())) {
      continue;
    }
    for (int col = x; col < x + width; ++col) {
      if (col < 0 || col >= static_cast<int>((*rows)[row].size())) {
        continue;
      }
      (*rows)[row][col] = tile;
    }
  }
}

void set_tile(std::vector<std::string>* rows, int x, int y, char tile) {
  if (y < 0 || y >= static_cast<int>(rows->size())) {
    return;
  }
  if (x < 0 || x >= static_cast<int>((*rows)[y].size())) {
    return;
  }
  (*rows)[y][x] = tile;
}

void draw_tree_border(std::vector<std::string>* rows,
                      int opening_x0,
                      int opening_x1,
                      int opening_y0,
                      int opening_y1) {
  const int height = static_cast<int>(rows->size());
  const int width = static_cast<int>((*rows)[0].size());
  for (int x = 0; x < width; ++x) {
    if (!(x >= opening_x0 && x <= opening_x1)) {
      (*rows)[0][x] = kTree;
      (*rows)[height - 1][x] = kTree;
    }
  }
  for (int y = 0; y < height; ++y) {
    if (!(y >= opening_y0 && y <= opening_y1)) {
      (*rows)[y][0] = kTree;
      (*rows)[y][width - 1] = kTree;
    }
  }
}

void stamp_house(std::vector<std::string>* rows, int x, int y, int width, int height) {
  fill_tiles(rows, x, y, width, height - 1, kRoof);
  fill_tiles(rows, x + 1, y + height - 1, width - 2, 1, kWall);
  set_tile(rows, x + width / 2, y + height - 1, kDoor);
  if (width >= 6) {
    set_tile(rows, x + 2, y + height - 1, kWindow);
    set_tile(rows, x + width - 3, y + height - 1, kWindow);
  }
}

Area make_priory_court() {
  Area area;
  area.id = "priory-court";
  area.name = "PRIORY COURT";
  area.tiles = blank_rows(28, 20, kGrass);
  draw_tree_border(&area.tiles, 12, 15, -1, -1);

  fill_tiles(&area.tiles, 11, 1, 8, 6, kRoof);
  fill_tiles(&area.tiles, 12, 6, 6, 1, kWall);
  set_tile(&area.tiles, 14, 6, kDoor);
  set_tile(&area.tiles, 12, 6, kGlass);
  set_tile(&area.tiles, 16, 6, kGlass);

  stamp_house(&area.tiles, 21, 3, 5, 5);
  stamp_house(&area.tiles, 3, 4, 5, 5);

  fill_tiles(&area.tiles, 12, 6, 5, 11, kStone);
  fill_tiles(&area.tiles, 8, 8, 13, 5, kStone);
  fill_tiles(&area.tiles, 9, 9, 11, 3, kPath);
  set_tile(&area.tiles, 14, 10, kFountain);
  fill_tiles(&area.tiles, 5, 10, 3, 3, kGarden);
  fill_tiles(&area.tiles, 20, 10, 3, 3, kGarden);
  set_tile(&area.tiles, 6, 11, kFlower);
  set_tile(&area.tiles, 21, 11, kFlower);
  fill_tiles(&area.tiles, 12, 16, 5, 4, kPath);
  fill_tiles(&area.tiles, 11, 18, 7, 1, kPath);

  area.warps.push_back({14, 6, "chapel", 7, 11, Direction::Up});
  area.warps.push_back({23, 7, "scriptorium", 6, 10, Direction::Up});
  area.warps.push_back({13, 19, "bellfield", 14, 1, Direction::Down});
  area.warps.push_back({14, 19, "bellfield", 15, 1, Direction::Down});

  area.npcs.push_back({"sister-agnes", "SISTER AGNES", SpriteRole::Sister, 7, 11, Direction::Right, true});
  area.npcs.push_back({"sister-helene", "SISTER HELENE", SpriteRole::Sister, 17, 15, Direction::Left, true});
  area.npcs.push_back({"porter", "BROTHER PORTER", SpriteRole::Monk, 5, 14, Direction::Right, true});
  return area;
}

Area make_bellfield() {
  Area area;
  area.id = "bellfield";
  area.name = "BELLFIELD TOWN";
  area.tiles = blank_rows(32, 20, kGrass);
  draw_tree_border(&area.tiles, 14, 17, 8, 11);

  fill_tiles(&area.tiles, 14, 0, 4, 7, kPath);
  fill_tiles(&area.tiles, 10, 6, 12, 6, kStone);
  set_tile(&area.tiles, 16, 8, kFountain);
  fill_tiles(&area.tiles, 20, 8, 12, 3, kPath);
  fill_tiles(&area.tiles, 14, 11, 4, 9, kPath);
  fill_tiles(&area.tiles, 8, 14, 16, 3, kPath);

  stamp_house(&area.tiles, 2, 3, 6, 5);
  stamp_house(&area.tiles, 2, 11, 6, 5);
  stamp_house(&area.tiles, 24, 3, 6, 5);
  stamp_house(&area.tiles, 24, 11, 6, 5);

  fill_tiles(&area.tiles, 11, 13, 3, 2, kStall);
  fill_tiles(&area.tiles, 19, 13, 3, 2, kStall);
  set_tile(&area.tiles, 9, 17, kFlower);
  set_tile(&area.tiles, 22, 17, kFlower);
  set_tile(&area.tiles, 12, 17, kFlower);
  set_tile(&area.tiles, 19, 17, kFlower);

  area.warps.push_back({14, 0, "priory-court", 13, 18, Direction::Up});
  area.warps.push_back({15, 0, "priory-court", 14, 18, Direction::Up});
  area.warps.push_back({31, 9, "candlewharf", 1, 9, Direction::Right});
  area.warps.push_back({31, 10, "candlewharf", 1, 10, Direction::Right});

  area.npcs.push_back({"mara", "MARA THE BAKER", SpriteRole::Merchant, 6, 9, Direction::Down, true});
  area.npcs.push_back({"oswin", "WATCHMAN OSWIN", SpriteRole::Watchman, 25, 9, Direction::Left, true});
  area.npcs.push_back({"agnes-child", "ALICE", SpriteRole::Child, 15, 10, Direction::Up, true});
  area.npcs.push_back({"widow-joan", "JOAN", SpriteRole::Elder, 11, 17, Direction::Up, true});
  return area;
}

Area make_candlewharf() {
  Area area;
  area.id = "candlewharf";
  area.name = "CANDLEWHARF";
  area.tiles = blank_rows(28, 18, kGrass);
  draw_tree_border(&area.tiles, 0, 0, -1, -1);

  fill_tiles(&area.tiles, 0, 8, 17, 3, kPath);
  fill_tiles(&area.tiles, 12, 6, 6, 7, kStone);
  fill_tiles(&area.tiles, 18, 0, 10, 18, kWater);
  fill_tiles(&area.tiles, 16, 6, 8, 4, kWood);
  fill_tiles(&area.tiles, 21, 3, 3, 10, kWood);
  stamp_house(&area.tiles, 4, 3, 6, 5);
  stamp_house(&area.tiles, 4, 11, 6, 5);
  set_tile(&area.tiles, 24, 9, kWood);
  set_tile(&area.tiles, 24, 10, kWood);

  area.warps.push_back({0, 9, "bellfield", 30, 9, Direction::Left});
  area.warps.push_back({0, 10, "bellfield", 30, 10, Direction::Left});

  area.npcs.push_back({"tomas", "TOMAS", SpriteRole::Fisher, 22, 9, Direction::Left, true});
  area.npcs.push_back({"elswyth", "ELSWYTH", SpriteRole::Elder, 13, 13, Direction::Up, true});
  area.npcs.push_back({"mercer", "THE MERCER", SpriteRole::Merchant, 8, 9, Direction::Right, true});
  return area;
}

Area make_scriptorium() {
  Area area;
  area.id = "scriptorium";
  area.name = "SCRIPTORIUM";
  area.indoor = true;
  area.tiles = blank_rows(14, 12, kFloor);

  fill_tiles(&area.tiles, 0, 0, 14, 1, kWall);
  fill_tiles(&area.tiles, 0, 11, 14, 1, kWall);
  fill_tiles(&area.tiles, 0, 0, 1, 12, kWall);
  fill_tiles(&area.tiles, 13, 0, 1, 12, kWall);
  set_tile(&area.tiles, 6, 11, kDoor);

  fill_tiles(&area.tiles, 2, 2, 10, 1, kShelf);
  fill_tiles(&area.tiles, 2, 4, 3, 1, kDesk);
  fill_tiles(&area.tiles, 8, 4, 3, 1, kDesk);
  set_tile(&area.tiles, 5, 4, kCandle);
  set_tile(&area.tiles, 8, 5, kCandle);
  set_tile(&area.tiles, 11, 2, kCandle);
  set_tile(&area.tiles, 2, 2, kCandle);

  area.warps.push_back({6, 11, "priory-court", 23, 8, Direction::Down});
  area.npcs.push_back({"brother-lucian", "BROTHER LUCIAN", SpriteRole::Monk, 6, 5, Direction::Down, true});
  return area;
}

Area make_chapel() {
  Area area;
  area.id = "chapel";
  area.name = "CHAPEL OF SAINT EDBURGA";
  area.indoor = true;
  area.tiles = blank_rows(16, 14, kStone);

  fill_tiles(&area.tiles, 0, 0, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 13, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 0, 1, 14, kWall);
  fill_tiles(&area.tiles, 15, 0, 1, 14, kWall);
  set_tile(&area.tiles, 7, 13, kDoor);

  set_tile(&area.tiles, 4, 0, kGlass);
  set_tile(&area.tiles, 7, 0, kGlass);
  set_tile(&area.tiles, 10, 0, kGlass);
  fill_tiles(&area.tiles, 6, 2, 4, 1, kAltar);
  set_tile(&area.tiles, 5, 2, kCandle);
  set_tile(&area.tiles, 10, 2, kCandle);
  fill_tiles(&area.tiles, 4, 6, 3, 1, kPew);
  fill_tiles(&area.tiles, 9, 6, 3, 1, kPew);
  fill_tiles(&area.tiles, 4, 8, 3, 1, kPew);
  fill_tiles(&area.tiles, 9, 8, 3, 1, kPew);

  area.warps.push_back({7, 13, "priory-court", 14, 7, Direction::Down});
  area.npcs.push_back({"prior-seraphim", "PRIOR SERAPHIM", SpriteRole::Prior, 7, 4, Direction::Down, true});
  return area;
}

std::vector<Area> build_world() {
  std::vector<Area> world;
  world.push_back(make_priory_court());
  world.push_back(make_bellfield());
  world.push_back(make_candlewharf());
  world.push_back(make_scriptorium());
  world.push_back(make_chapel());
  return world;
}

Area* mutable_area(GameState* state, const std::string& id) {
  for (auto& area : state->world) {
    if (area.id == id) {
      return &area;
    }
  }
  return nullptr;
}

const Area* area_for(const GameState& state, const std::string& id) {
  for (const auto& area : state.world) {
    if (area.id == id) {
      return &area;
    }
  }
  return nullptr;
}

const Area* current_area(const GameState& state) {
  return area_for(state, state.current_area);
}

int area_index_for(const GameState& state, const std::string& id) {
  for (std::size_t index = 0; index < state.world.size(); ++index) {
    if (state.world[index].id == id) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

const Npc* find_npc(const GameState& state, const std::string& area_id, const std::string& npc_id) {
  const Area* area = area_for(state, area_id);
  if (area == nullptr) {
    return nullptr;
  }
  for (const auto& npc : area->npcs) {
    if (npc.id == npc_id) {
      return &npc;
    }
  }
  return nullptr;
}

Npc* mutable_npc(GameState* state, const std::string& area_id, const std::string& npc_id) {
  Area* area = mutable_area(state, area_id);
  if (area == nullptr) {
    return nullptr;
  }
  for (auto& npc : area->npcs) {
    if (npc.id == npc_id) {
      return &npc;
    }
  }
  return nullptr;
}

bool tile_in_bounds(const Area& area, int x, int y) {
  return y >= 0 && y < static_cast<int>(area.tiles.size()) &&
         x >= 0 && x < static_cast<int>(area.tiles[y].size());
}

char tile_at(const Area& area, int x, int y) {
  if (!tile_in_bounds(area, x, y)) {
    return kTree;
  }
  return area.tiles[y][x];
}

bool tile_passable(char tile) {
  switch (tile) {
    case kGrass:
    case kFlower:
    case kPath:
    case kStone:
    case kDoor:
    case kWood:
    case kFloor:
    case kGarden:
      return true;
    default:
      return false;
  }
}

bool tile_occupied_by_npc(const GameState& state, const std::string& area_id, int x, int y) {
  const Area* area = area_for(state, area_id);
  if (area == nullptr) {
    return false;
  }
  for (const auto& npc : area->npcs) {
    if (npc.solid && npc.x == x && npc.y == y) {
      return true;
    }
  }
  return false;
}

const Warp* warp_at(const Area& area, int x, int y) {
  for (const auto& warp : area.warps) {
    if (warp.x == x && warp.y == y) {
      return &warp;
    }
  }
  return nullptr;
}

std::filesystem::path save_path(const pp_context* context) {
  return std::filesystem::path(pp_get_save_dir(context)) / "pilgrimage-save.txt";
}

void set_status_message(GameState* state, const std::string& message, Uint32 now) {
  state->status_message = message;
  state->status_until = now + kStatusMessageMs;
}

void set_area_banner(GameState* state, const std::string& area_name, Uint32 now) {
  state->area_banner = area_name;
  state->area_banner_until = now + kAreaBannerMs;
}

void save_game(const GameState& state) {
  if (state.context == nullptr || state.smoke_test) {
    return;
  }

  std::ofstream output(save_path(state.context));
  if (!output.is_open()) {
    return;
  }

  output << "area=" << state.current_area << "\n";
  output << "x=" << state.player.tile_x << "\n";
  output << "y=" << state.player.tile_y << "\n";
  output << "facing=" << static_cast<int>(state.player.facing) << "\n";
  output << "started=" << (state.quest.started ? 1 : 0) << "\n";
  output << "wax=" << (state.quest.wax ? 1 : 0) << "\n";
  output << "hymn=" << (state.quest.hymn ? 1 : 0) << "\n";
  output << "oil=" << (state.quest.oil ? 1 : 0) << "\n";
  output << "complete=" << (state.quest.complete ? 1 : 0) << "\n";
}

bool load_game(GameState* state) {
  if (state->context == nullptr) {
    return false;
  }

  std::ifstream input(save_path(state->context));
  if (!input.is_open()) {
    return false;
  }

  std::map<std::string, std::string> values;
  std::string line;
  while (std::getline(input, line)) {
    const std::size_t split = line.find('=');
    if (split == std::string::npos) {
      continue;
    }
    values[line.substr(0, split)] = line.substr(split + 1);
  }

  if (values.count("area") == 0 || values.count("x") == 0 || values.count("y") == 0) {
    return false;
  }

  const Area* area = area_for(*state, values["area"]);
  if (area == nullptr) {
    return false;
  }

  const int x = std::atoi(values["x"].c_str());
  const int y = std::atoi(values["y"].c_str());
  if (!tile_in_bounds(*area, x, y) || !tile_passable(tile_at(*area, x, y))) {
    return false;
  }

  state->current_area = values["area"];
  state->player.tile_x = x;
  state->player.tile_y = y;
  state->player.start_x = x;
  state->player.start_y = y;
  state->player.target_x = x;
  state->player.target_y = y;
  state->player.moving = false;
  state->player.facing = static_cast<Direction>(std::atoi(values["facing"].c_str()));
  state->quest.started = values["started"] == "1";
  state->quest.wax = values["wax"] == "1";
  state->quest.hymn = values["hymn"] == "1";
  state->quest.oil = values["oil"] == "1";
  state->quest.complete = values["complete"] == "1";
  return true;
}

void start_dialogue(GameState* state, const std::string& speaker, std::vector<std::string> pages) {
  state->dialogue.active = true;
  state->dialogue.speaker = speaker;
  state->dialogue.pages = std::move(pages);
  state->dialogue.page_index = 0;
}

std::string objective_text(const GameState& state) {
  if (!state.quest.started) {
    return "ENTER THE CHAPEL AND SPEAK TO PRIOR SERAPHIM.";
  }
  if (!state.quest.wax || !state.quest.hymn || !state.quest.oil) {
    std::vector<std::string> missing;
    if (!state.quest.wax) {
      missing.push_back("BEESWAX TAPER");
    }
    if (!state.quest.hymn) {
      missing.push_back("HYMN LEAF");
    }
    if (!state.quest.oil) {
      missing.push_back("VESPER OIL");
    }

    std::string text = "GATHER ";
    for (std::size_t index = 0; index < missing.size(); ++index) {
      if (index != 0U) {
        text += index + 1U == missing.size() ? " AND " : ", ";
      }
      text += missing[index];
    }
    text += ".";
    return text;
  }
  if (!state.quest.complete) {
    return "RETURN TO PRIOR SERAPHIM IN THE CHAPEL.";
  }
  return "THE HOUSE IS GATHERED. WALK THE TOWNS AND SPEAK WITH THE FAITHFUL.";
}

void start_new_game(GameState* state, Uint32 now) {
  state->current_area = "priory-court";
  state->player.tile_x = 14;
  state->player.tile_y = 15;
  state->player.start_x = 14;
  state->player.start_y = 15;
  state->player.target_x = 14;
  state->player.target_y = 15;
  state->player.moving = false;
  state->player.facing = Direction::Up;
  state->quest = {};
  state->dialogue = {};
  state->journal_open = false;
  state->on_title = false;
  state->title_selection = 0;
  state->bells_until = 0;
  set_area_banner(state, "PRIORY COURT", now);
  start_dialogue(
      state,
      "CHRONICLE",
      {
          "THE SEA WIND HAS DAMPENED THE LANTERNS OF SAINT EDBURGA PRIORY.",
          "PRIOR SERAPHIM HAS SENT YOU TO READY THE HOUSE BEFORE VESPERS.",
      });
  save_game(*state);
}

void apply_warp(GameState* state, const Warp& warp, Uint32 now) {
  state->current_area = warp.target_area;
  state->player.tile_x = warp.target_x;
  state->player.tile_y = warp.target_y;
  state->player.start_x = warp.target_x;
  state->player.start_y = warp.target_y;
  state->player.target_x = warp.target_x;
  state->player.target_y = warp.target_y;
  state->player.facing = warp.target_facing;
  state->player.moving = false;
  state->warp_cooldown_until = now + kWarpCooldownMs;
  if (const Area* area = current_area(*state)) {
    set_area_banner(state, area->name, now);
  }
  save_game(*state);
}

void maybe_trigger_warp(GameState* state, Uint32 now) {
  if (now < state->warp_cooldown_until) {
    return;
  }
  const Area* area = current_area(*state);
  if (area == nullptr) {
    return;
  }
  const Warp* warp = warp_at(*area, state->player.tile_x, state->player.tile_y);
  if (warp != nullptr) {
    apply_warp(state, *warp, now);
  }
}

bool can_occupy(const GameState& state, const std::string& area_id, int x, int y) {
  const Area* area = area_for(state, area_id);
  if (area == nullptr) {
    return false;
  }
  if (!tile_in_bounds(*area, x, y)) {
    return false;
  }
  if (!tile_passable(tile_at(*area, x, y))) {
    return false;
  }
  if (tile_occupied_by_npc(state, area_id, x, y)) {
    return false;
  }
  return true;
}

bool step_player_instant(GameState* state, Direction direction, Uint32 now) {
  int dx = 0;
  int dy = 0;
  state->player.facing = direction;
  direction_delta(direction, &dx, &dy);
  if (dx == 0 && dy == 0) {
    return false;
  }

  const int next_x = state->player.tile_x + dx;
  const int next_y = state->player.tile_y + dy;
  if (!can_occupy(*state, state->current_area, next_x, next_y)) {
    return false;
  }

  state->player.tile_x = next_x;
  state->player.tile_y = next_y;
  state->player.start_x = next_x;
  state->player.start_y = next_y;
  state->player.target_x = next_x;
  state->player.target_y = next_y;
  maybe_trigger_warp(state, now);
  return true;
}

void begin_player_step(GameState* state, Direction direction, Uint32 now, bool running) {
  int dx = 0;
  int dy = 0;
  state->player.facing = direction;
  direction_delta(direction, &dx, &dy);
  if (dx == 0 && dy == 0) {
    return;
  }

  const int next_x = state->player.tile_x + dx;
  const int next_y = state->player.tile_y + dy;
  if (!can_occupy(*state, state->current_area, next_x, next_y)) {
    return;
  }

  state->player.start_x = state->player.tile_x;
  state->player.start_y = state->player.tile_y;
  state->player.target_x = next_x;
  state->player.target_y = next_y;
  state->player.step_started_at = now;
  state->player.step_duration = running ? kRunStepMs : kWalkStepMs;
  state->player.moving = true;
}

void update_player(GameState* state, const ButtonState& buttons, Uint32 now) {
  if (state->player.moving) {
    if (now - state->player.step_started_at >= state->player.step_duration) {
      state->player.moving = false;
      state->player.tile_x = state->player.target_x;
      state->player.tile_y = state->player.target_y;
      state->player.start_x = state->player.tile_x;
      state->player.start_y = state->player.tile_y;
      maybe_trigger_warp(state, now);
    }
    return;
  }

  if (buttons.up) {
    begin_player_step(state, Direction::Up, now, buttons.b);
  } else if (buttons.down) {
    begin_player_step(state, Direction::Down, now, buttons.b);
  } else if (buttons.left) {
    begin_player_step(state, Direction::Left, now, buttons.b);
  } else if (buttons.right) {
    begin_player_step(state, Direction::Right, now, buttons.b);
  }
}

void turn_npc_toward(GameState* state, const std::string& area_id, const std::string& npc_id, Direction facing) {
  if (Npc* npc = mutable_npc(state, area_id, npc_id)) {
    npc->facing = facing;
  }
}

}
int main() { return 0; }
