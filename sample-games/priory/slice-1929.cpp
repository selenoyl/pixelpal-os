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

std::vector<std::string> dialogue_for_npc(GameState* state, const Npc& npc, Uint32 now) {
  if (npc.id == "prior-seraphim") {
    if (!state->quest.started) {
      state->quest.started = true;
      save_game(*state);
      set_status_message(state, "THE LANTERN WATCH BEGINS.", now);
      return {
          "THE HOUSE IS NEAR VESPERS, BUT THE STORM TOOK OUR LIGHT.",
          "BRING ME A BEESWAX TAPER, A HYMN LEAF, AND A FLASK OF VESPER OIL.",
      };
    }
    if (state->quest.wax && state->quest.hymn && state->quest.oil && !state->quest.complete) {
      state->quest.complete = true;
      state->bells_until = now + 6000;
      save_game(*state);
      set_status_message(state, "VESPERS ARE READY.", now);
      return {
          "YOU HAVE GATHERED THE HOUSE WELL.",
          "LIGHT THE LANTERN, AND LET BELLFIELD HEAR THAT THE PRIORY STILL SINGS.",
      };
    }
    if (!state->quest.complete) {
      return {
          "OUR NEEDS ARE SIMPLE. GATHER WHAT THE HOUSE LACKS AND RETURN BEFORE DUSK.",
      };
    }
    return {
        "THE BELL HAS A STRONGER VOICE TONIGHT. GO, AND HEAR HOW THE TOWN ANSWERS IT.",
    };
  }

  if (npc.id == "sister-agnes") {
    if (!state->quest.started) {
      return {"THE ROSES ARE DRINKING IN THE LAST OF THE DAY."};
    }
    if (!state->quest.wax) {
      state->quest.wax = true;
      save_game(*state);
      set_status_message(state, "YOU RECEIVED THE BEESWAX TAPER.", now);
      return {
          "I SAVED A FRESH TAPER FROM THE APIARY FOR THE CHAPEL LANTERN.",
          "TAKE IT. THE EVENING OFFICE SHOULD NOT BEGIN IN SHADOW.",
      };
    }
    return {"THE HIVES HAVE BEEN KIND THIS YEAR. MAY THE LIGHT HOLD FAST."};
  }

  if (npc.id == "brother-lucian") {
    if (!state->quest.started) {
      return {"THE INK HAS JUST DRIED ON TODAYS COPYING."};
    }
    if (!state->quest.hymn) {
      state->quest.hymn = true;
      save_game(*state);
      set_status_message(state, "YOU RECEIVED THE HYMN LEAF.", now);
      return {
          "THIS LEAF HOLDS THE HYMN FOR TONIGHTS VESPERS.",
          "CARRY IT FLAT, AND DO NOT LET THE SEA WIND TAKE IT FROM YOU.",
      };
    }
    return {"THE CHOIR WILL BE GLAD OF A CLEAN COPY."};
  }

  if (npc.id == "tomas") {
    if (!state->quest.started) {
      return {"THE TIDE IS QUIET. THAT MEANS THE NIGHT WILL NOT BE."};
    }
    if (!state->quest.oil) {
      state->quest.oil = true;
      save_game(*state);
      set_status_message(state, "YOU RECEIVED THE VESPER OIL.", now);
      return {
          "A FRESH FLASK, JUST AS THE PRIOR ASKED.",
          "THE PIERS ARE DARK ENOUGH WITHOUT A DARK CHAPEL ABOVE THEM.",
      };
    }
    return {"THE LAMPS ON THIS WHARF BURN BETTER WHEN THE PRIORY BELL IS RINGING."};
  }

  if (npc.id == "sister-helene") {
    return {
        state->quest.complete
            ? "LISTEN. EVEN THE CROWS PAUSE WHEN THE PRIORY BELL TAKES THE AIR."
            : "THE TOWN KEEPS TIME BY THE PRIORY. WE ALL FEEL IT WHEN THE LIGHTS GO OUT.",
    };
  }

  if (npc.id == "porter") {
    return {
        state->quest.started
            ? "PILGRIMS COME BY THE SOUTH ROAD. TONIGHT THEY WILL FIND THE LANTERN LIT."
            : "THE GATE IS QUIET. ENJOY IT WHILE YOU CAN.",
    };
  }

  if (npc.id == "mara") {
    return {
        state->quest.complete
            ? "THE BELL REACHED MY OVEN BEFORE THE DOUGH COULD COOL. THAT IS A GOOD SIGN."
            : "IF THE PRIORY SINGS TONIGHT, I WILL SEND LOAVES UP THE HILL BEFORE DAWN.",
    };
  }

  if (npc.id == "oswin") {
    return {
        state->quest.complete
            ? "ALL CLEAR FROM THE GATE TO THE WHARF. A FINE NIGHT TO KEEP WATCH."
            : "WITHOUT THE CHAPEL LIGHT, THE WATCH FEELS LONGER THAN IT SHOULD.",
    };
  }

  if (npc.id == "agnes-child") {
    return {
        state->quest.complete
            ? "I HEARD THE BELL FROM THE FOUNTAIN. IT SOUNDED LIKE SUMMER."
            : "WHEN THE BELL RINGS, THE WATER IN THE SQUARE SHIVERS. REALLY.",
    };
  }

  if (npc.id == "widow-joan") {
    return {
        "THE PRIORY WAS HERE BEFORE THIS MARKET WAS STONE. SOME THINGS DESERVE TO OUTLAST US.",
    };
  }

  if (npc.id == "elswyth") {
    return {
        "I CAME TO GIVE THANKS AT SAINT EDBURGAS CHAPEL. EVEN THE WHARF FEELS GENTLER ABOVE THAT HILL.",
    };
  }

  if (npc.id == "mercer") {
    return {
        "I SELL THREAD, TIN, AND PILGRIM RIBBONS. TONIGHT I MIGHT JUST STAY TO HEAR THE BELL.",
    };
  }

  return {"PEACE BE WITH YOU."};
}

void trigger_bell(AudioState* audio, float frequency, int duration_ms) {
  audio->bell_frequency = frequency;
  audio->bell_samples_remaining = (48000 * duration_ms) / 1000;
}

void interact(GameState* state, Uint32 now, AudioState* audio) {
  const Area* area = current_area(*state);
  if (area == nullptr) {
    return;
  }

  int dx = 0;
  int dy = 0;
  direction_delta(state->player.facing, &dx, &dy);
  const int target_x = state->player.tile_x + dx;
  const int target_y = state->player.tile_y + dy;

  for (const auto& npc : area->npcs) {
    if (npc.x == target_x && npc.y == target_y) {
      turn_npc_toward(state, state->current_area, npc.id, opposite(state->player.facing));
      const std::vector<std::string> pages = dialogue_for_npc(state, npc, now);
      start_dialogue(state, npc.name, pages);
      if ((npc.id == "sister-agnes" && state->quest.wax) ||
          (npc.id == "brother-lucian" && state->quest.hymn) ||
          (npc.id == "tomas" && state->quest.oil)) {
        trigger_bell(audio, 880.0f, 240);
      }
      if (npc.id == "prior-seraphim" && state->quest.complete) {
        trigger_bell(audio, 523.25f, 520);
      }
      return;
    }
  }

  if (state->current_area == "bellfield" && target_x == 16 && target_y == 8) {
    start_dialogue(
        state,
        "FOUNTAIN",
        {"THE BASIN CATCHES THE LAST LIGHT FROM THE PRIORY HILL."});
    return;
  }

  if (state->current_area == "priory-court" && target_x == 14 && target_y == 10) {
    start_dialogue(state, "FOUNTAIN", {"THE COURT FOUNTAIN RUNS CLEAR, EVEN AFTER STORM."});
  }
}

void advance_dialogue(GameState* state) {
  if (!state->dialogue.active) {
    return;
  }
  if (state->dialogue.page_index + 1 < state->dialogue.pages.size()) {
    ++state->dialogue.page_index;
  } else {
    state->dialogue = {};
  }
}

float player_world_x(const Player& player, Uint32 now) {
  if (!player.moving) {
    return static_cast<float>(player.tile_x * kTileSize);
  }
  const float t = std::min(1.0f, static_cast<float>(now - player.step_started_at) /
                                     static_cast<float>(player.step_duration));
  return static_cast<float>(player.start_x * kTileSize) +
         static_cast<float>((player.target_x - player.start_x) * kTileSize) * t;
}

float player_world_y(const Player& player, Uint32 now) {
  if (!player.moving) {
    return static_cast<float>(player.tile_y * kTileSize);
  }
  const float t = std::min(1.0f, static_cast<float>(now - player.step_started_at) /
                                     static_cast<float>(player.step_duration));
  return static_cast<float>(player.start_y * kTileSize) +
         static_cast<float>((player.target_y - player.start_y) * kTileSize) * t;
}

void draw_tile(SDL_Renderer* renderer, char tile, int tile_x, int tile_y, int screen_x, int screen_y, Uint32 now) {
  const SDL_Rect rect = {screen_x, screen_y, kTileSize, kTileSize};
  const int noise = hash_xy(tile_x, tile_y, static_cast<int>(now / 320U));

  switch (tile) {
    case kGrass: {
      const SDL_Color base = {96, 156, 92, 255};
      const SDL_Color dark = {63, 122, 65, 255};
      const SDL_Color light = {134, 191, 120, 255};
      fill_rect(renderer, rect, base);
      draw_pixel(renderer, screen_x + 3 + (noise & 1), screen_y + 4, light);
      draw_pixel(renderer, screen_x + 8, screen_y + 11, dark);
      draw_pixel(renderer, screen_x + 12, screen_y + 6 + ((noise >> 2) & 1), light);
      draw_pixel(renderer, screen_x + 5, screen_y + 13, dark);
      break;
    }
    case kFlower:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      draw_pixel(renderer, screen_x + 5, screen_y + 6, SDL_Color{244, 196, 206, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 7, SDL_Color{255, 236, 130, 255});
      draw_pixel(renderer, screen_x + 10, screen_y + 9, SDL_Color{238, 144, 170, 255});
      break;
    case kPath: {
      const SDL_Color base = {191, 160, 109, 255};
      const SDL_Color dark = {152, 123, 79, 255};
      const SDL_Color light = {217, 192, 141, 255};
      fill_rect(renderer, rect, base);
      draw_pixel(renderer, screen_x + 4, screen_y + 4, light);
      draw_pixel(renderer, screen_x + 11, screen_y + 5, dark);
      draw_pixel(renderer, screen_x + 6, screen_y + 11, dark);
      draw_pixel(renderer, screen_x + 13, screen_y + 12, light);
      break;
    }
    case kStone:
      fill_rect(renderer, rect, SDL_Color{188, 181, 166, 255});
      for (int px = 0; px < kTileSize; px += 4) {
        draw_pixel(renderer, screen_x + px, screen_y + 7, SDL_Color{145, 138, 124, 255});
      }
      draw_pixel(renderer, screen_x + 8, screen_y + 3, SDL_Color{145, 138, 124, 255});
      draw_pixel(renderer, screen_x + 3, screen_y + 11, SDL_Color{145, 138, 124, 255});
      break;
    case kWater:
      fill_rect(renderer, rect, SDL_Color{64, 123, 189, 255});
      for (int row = 3; row <= 12; row += 4) {
        const int wave = (static_cast<int>(now / 150U) + tile_x + row) % 4;
        fill_rect(renderer, SDL_Rect{screen_x + 2 + wave, screen_y + row, 6, 1},
                  SDL_Color{122, 188, 235, 255});
        fill_rect(renderer, SDL_Rect{screen_x + 9 - wave / 2, screen_y + row + 1, 5, 1},
                  SDL_Color{43, 86, 148, 255});
      }
      break;
    case kTree:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 8}, SDL_Color{52, 101, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 3, 10, 6}, SDL_Color{88, 145, 80, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 10, 2, 4}, SDL_Color{110, 74, 45, 255});
      break;
    case kRoof:
      fill_rect(renderer, rect, SDL_Color{145, 74, 72, 255});
      for (int row = 1; row < kTileSize; row += 4) {
        fill_rect(renderer, SDL_Rect{screen_x, screen_y + row, kTileSize, 1},
                  SDL_Color{170, 95, 92, 255});
      }
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 13, kTileSize, 3},
                SDL_Color{104, 50, 48, 255});
      break;
    case kWall:
      fill_rect(renderer, rect, SDL_Color{220, 210, 188, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4},
                SDL_Color{184, 171, 145, 255});
      break;
    case kWindow:
      fill_rect(renderer, rect, SDL_Color{220, 210, 188, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 5, 8, 6}, SDL_Color{109, 162, 201, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 5, 8, 6}, SDL_Color{89, 111, 140, 255});
      break;
    case kGlass:
      fill_rect(renderer, rect, SDL_Color{196, 184, 166, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 2, 8, 12}, SDL_Color{76, 120, 170, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 5, SDL_Color{255, 212, 113, 255}, 2);
      break;
    case kDoor:
      fill_rect(renderer, rect, SDL_Color{136, 103, 67, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 2, 10, 13}, SDL_Color{92, 62, 35, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 7, SDL_Color{219, 188, 124, 255});
      break;
    case kWood:
      fill_rect(renderer, rect, SDL_Color{132, 96, 61, 255});
      for (int x = 0; x < kTileSize; x += 4) {
        fill_rect(renderer, SDL_Rect{screen_x + x, screen_y, 1, kTileSize}, SDL_Color{90, 62, 39, 255});
      }
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 7, kTileSize, 1}, SDL_Color{168, 126, 82, 255});
      break;
    case kFloor:
      fill_rect(renderer, rect, SDL_Color{168, 134, 101, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 7, kTileSize, 1}, SDL_Color{132, 102, 77, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y, 1, kTileSize}, SDL_Color{132, 102, 77, 255});
      break;
    case kPew:
      fill_rect(renderer, rect, SDL_Color{113, 77, 47, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 3, 14, 10}, SDL_Color{145, 102, 63, 255});
      break;
    case kAltar:
      fill_rect(renderer, rect, SDL_Color{201, 187, 170, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 3, 10, 8}, SDL_Color{225, 217, 202, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 2, SDL_Color{251, 210, 104, 255}, 2);
      break;
    case kDesk:
      fill_rect(renderer, rect, SDL_Color{123, 84, 48, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 4, 12, 7}, SDL_Color{153, 108, 65, 255});
      break;
    case kShelf:
      fill_rect(renderer, rect, SDL_Color{90, 57, 39, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 2, 14, 12}, SDL_Color{116, 74, 47, 255});
      draw_pixel(renderer, screen_x + 4, screen_y + 4, SDL_Color{235, 214, 167, 255});
      draw_pixel(renderer, screen_x + 8, screen_y + 7, SDL_Color{182, 211, 102, 255});
      draw_pixel(renderer, screen_x + 11, screen_y + 10, SDL_Color{197, 150, 81, 255});
      break;
    case kCandle:
      fill_rect(renderer, rect, SDL_Color{167, 149, 126, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 4, 4, 8}, SDL_Color{242, 229, 202, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 2 + ((now / 140U) % 2U), SDL_Color{255, 209, 96, 255}, 2);
      break;
    case kGarden:
      fill_rect(renderer, rect, SDL_Color{94, 114, 67, 255});
      draw_pixel(renderer, screen_x + 4, screen_y + 5, SDL_Color{131, 175, 86, 255});
      draw_pixel(renderer, screen_x + 8, screen_y + 9, SDL_Color{168, 212, 110, 255});
      draw_pixel(renderer, screen_x + 11, screen_y + 4, SDL_Color{131, 175, 86, 255});
      break;
    case kFountain:
      fill_rect(renderer, rect, SDL_Color{170, 170, 177, 255});
      fill_circle(renderer, screen_x + 8, screen_y + 8, 5, SDL_Color{104, 159, 216, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 3, SDL_Color{245, 245, 255, 255}, 2);
      break;
    case kStall:
      fill_rect(renderer, rect, SDL_Color{123, 86, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 3, 14, 3}, SDL_Color{191, 98, 74, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 8, 14, 6}, SDL_Color{224, 210, 161, 255});
      break;
    default:
      fill_rect(renderer, rect, SDL_Color{255, 0, 255, 255});
      break;
  }
}

void draw_objective_tokens(SDL_Renderer* renderer, const GameState& state) {
  const SDL_Rect panel = {168, 8, 80, 20};
  fill_rect(renderer, panel, SDL_Color{246, 238, 215, 230});
  draw_rect(renderer, panel, SDL_Color{116, 106, 82, 255});

  const auto draw_token = [&](int x, const std::string& label, bool acquired) {
    const SDL_Color frame = acquired ? SDL_Color{122, 154, 77, 255} : SDL_Color{136, 128, 116, 255};
    const SDL_Color fill = acquired ? SDL_Color{233, 242, 194, 255} : SDL_Color{221, 213, 200, 255};
    fill_rect(renderer, SDL_Rect{x, 11, 18, 14}, fill);
    draw_rect(renderer, SDL_Rect{x, 11, 18, 14}, frame);
    draw_text(renderer, label, x + 6, 15, 1,
              acquired ? SDL_Color{54, 90, 53, 255} : SDL_Color{102, 92, 82, 255});
  };

  draw_token(173, "W", state.quest.wax);
  draw_token(194, "H", state.quest.hymn);
  draw_token(215, "O", state.quest.oil);
}

void render_world(SDL_Renderer* renderer, const GameState& state, Uint32 now) {
  const Area* area = current_area(state);
  if (area == nullptr) {
    return;
  }

  const float player_x = player_world_x(state.player, now);
  const float player_y = player_world_y(state.player, now);
  const int map_width = static_cast<int>(area->tiles[0].size()) * kTileSize;
  const int map_height = static_cast<int>(area->tiles.size()) * kTileSize;

  int camera_x = static_cast<int>(player_x) - kScreenWidth / 2 + kTileSize / 2;
  int camera_y = static_cast<int>(player_y) - (kScreenHeight - 16) / 2 + kTileSize / 2;
  camera_x = std::max(0, std::min(camera_x, std::max(0, map_width - kScreenWidth)));
  camera_y = std::max(0, std::min(camera_y, std::max(0, map_height - kScreenHeight)));

  fill_rect(renderer, SDL_Rect{0, 0, kScreenWidth, kScreenHeight},
            area->indoor ? SDL_Color{74, 66, 60, 255} : SDL_Color{112, 178, 214, 255});

  const int first_tile_x = std::max(0, camera_x / kTileSize);
  const int first_tile_y = std::max(0, camera_y / kTileSize);
  const int last_tile_x =
      std::min(static_cast<int>(area->tiles[0].size()) - 1, (camera_x + kScreenWidth) / kTileSize + 1);
  const int last_tile_y =
      std::min(static_cast<int>(area->tiles.size()) - 1, (camera_y + kScreenHeight) / kTileSize + 1);

  for (int y = first_tile_y; y <= last_tile_y; ++y) {
    for (int x = first_tile_x; x <= last_tile_x; ++x) {
      draw_tile(renderer, tile_at(*area, x, y), x, y,
                x * kTileSize - camera_x, y * kTileSize - camera_y, now);
    }
  }

  struct Drawable {
    int sort_y;
    int screen_x;
    int screen_y;
    SpriteRole role;
    Direction facing;
    bool walking;
  };

  std::vector<Drawable> drawables;
  for (const auto& npc : area->npcs) {
    drawables.push_back(
        {npc.y * kTileSize, npc.x * kTileSize - camera_x, npc.y * kTileSize - camera_y, npc.role, npc.facing, false});
  }
  drawables.push_back({static_cast<int>(player_y), static_cast<int>(player_x) - camera_x,
                       static_cast<int>(player_y) - camera_y, SpriteRole::Player, state.player.facing,
                       state.player.moving});

  std::sort(drawables.begin(), drawables.end(), [](const Drawable& left, const Drawable& right) {
    return left.sort_y < right.sort_y;
  });

  for (const auto& drawable : drawables) {
    render_character(renderer, drawable.role, drawable.facing, drawable.walking,
                     drawable.screen_x, drawable.screen_y, now);
  }

  if (now < state.bells_until) {
    for (int index = 0; index < 12; ++index) {
      const int sparkle_x = 80 + (index * 13 + static_cast<int>(now / 18U)) % 110;
      const int sparkle_y = 18 + (index * 11) % 28;
      draw_pixel(renderer, sparkle_x, sparkle_y, SDL_Color{255, 231, 152, 180});
      draw_pixel(renderer, sparkle_x + 1, sparkle_y + 1, SDL_Color{255, 245, 209, 180});
    }
  }

  draw_objective_tokens(renderer, state);
}

void render_area_banner(SDL_Renderer* renderer, const GameState& state, Uint32 now) {
  if (now >= state.area_banner_until) {
    return;
  }
  const SDL_Rect plaque = {36, 10, 184, 24};
  fill_rect(renderer, plaque, SDL_Color{239, 232, 214, 245});
  draw_rect(renderer, plaque, SDL_Color{117, 103, 74, 255});
  draw_text(renderer, state.area_banner, 128, 16, 2, SDL_Color{64, 78, 56, 255}, true);
}

void render_status_message(SDL_Renderer* renderer, const GameState& state, Uint32 now) {
  if (state.status_message.empty() || now >= state.status_until) {
    return;
  }
  const SDL_Rect plaque = {44, 36, 168, 18};
  fill_rect(renderer, plaque, SDL_Color{247, 239, 214, 230});
  draw_rect(renderer, plaque, SDL_Color{122, 110, 84, 255});
  draw_text(renderer, state.status_message, 128, 41, 1, SDL_Color{72, 89, 58, 255}, true);
}

void render_dialogue(SDL_Renderer* renderer, const GameState& state) {
  if (!state.dialogue.active) {
    return;
  }

  const SDL_Rect box = {8, kScreenHeight - kDialogHeight, kScreenWidth - 16, kDialogHeight - 8};
  fill_rect(renderer, box, SDL_Color{248, 243, 228, 245});
  draw_rect(renderer, box, SDL_Color{95, 86, 67, 255});
  fill_rect(renderer, SDL_Rect{18, kScreenHeight - kDialogHeight - 8, 92, 14},
            SDL_Color{201, 181, 136, 255});
  draw_rect(renderer, SDL_Rect{18, kScreenHeight - kDialogHeight - 8, 92, 14},
            SDL_Color{95, 86, 67, 255});
  draw_text(renderer, state.dialogue.speaker, 22, kScreenHeight - kDialogHeight - 5, 1,
            SDL_Color{58, 56, 41, 255});

  const std::vector<std::string> lines =
      wrap_text(state.dialogue.pages[state.dialogue.page_index], box.w - 18, 2);
  int draw_y = box.y + 10;
  for (std::size_t index = 0; index < lines.size() && index < 3; ++index) {
    draw_text(renderer, lines[index], box.x + 10, draw_y, 2, SDL_Color{42, 46, 52, 255});
    draw_y += 16;
  }
  draw_text(renderer, "A", box.x + box.w - 18, box.y + box.h - 15, 1, SDL_Color{95, 86, 67, 255});
}

void render_journal(SDL_Renderer* renderer, const GameState& state) {
  if (!state.journal_open) {
    return;
  }
  fill_rect(renderer, SDL_Rect{20, 18, 216, 188}, SDL_Color{242, 236, 220, 250});
  draw_rect(renderer, SDL_Rect{20, 18, 216, 188}, SDL_Color{86, 79, 60, 255});
  draw_text(renderer, "PILGRIMS JOURNAL", 128, 28, 2, SDL_Color{72, 84, 58, 255}, true);
  draw_text(renderer, "OBJECTIVE", 32, 52, 1, SDL_Color{86, 79, 60, 255});

  const auto objective_lines = wrap_text(objective_text(state), 184, 2);
  int y = 64;
  for (const auto& line : objective_lines) {
    draw_text(renderer, line, 32, y, 2, SDL_Color{42, 46, 52, 255});
    y += 16;
  }

  draw_text(renderer, "LANTERN WATCH", 32, 120, 1, SDL_Color{86, 79, 60, 255});
  draw_text(renderer, state.quest.wax ? "BEESWAX TAPER READY" : "BEESWAX TAPER MISSING",
            32, 132, 1, state.quest.wax ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
  draw_text(renderer, state.quest.hymn ? "HYMN LEAF READY" : "HYMN LEAF MISSING",
            32, 145, 1, state.quest.hymn ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
  draw_text(renderer, state.quest.oil ? "VESPER OIL READY" : "VESPER OIL MISSING",
            32, 158, 1, state.quest.oil ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
  draw_text(renderer, state.quest.complete ? "HOUSE GATHERED" : "VESPERS NOT YET READY",
            32, 171, 1, state.quest.complete ? SDL_Color{62, 103, 57, 255}
                                             : SDL_Color{112, 88, 72, 255});
  draw_text(renderer, "START OR B TO CLOSE", 128, 189, 1, SDL_Color{86, 79, 60, 255}, true);
}

void render_title(SDL_Renderer* renderer, const GameState& state, Uint32 now) {
  fill_rect(renderer, SDL_Rect{0, 0, kScreenWidth, kScreenHeight}, SDL_Color{112, 172, 204, 255});
  fill_rect(renderer, SDL_Rect{0, 80, kScreenWidth, 144}, SDL_Color{128, 181, 120, 255});
  fill_rect(renderer, SDL_Rect{64, 54, 128, 64}, SDL_Color{173, 112, 88, 255});
  fill_rect(renderer, SDL_Rect{76, 58, 104, 44}, SDL_Color{202, 140, 116, 255});
  fill_rect(renderer, SDL_Rect{92, 26, 72, 40}, SDL_Color{124, 68, 70, 255});
  fill_rect(renderer, SDL_Rect{100, 34, 56, 24}, SDL_Color{156, 85, 85, 255});
  fill_rect(renderer, SDL_Rect{122, 74, 12, 28}, SDL_Color{112, 79, 48, 255});
  draw_pixel(renderer, 127, 21, SDL_Color{255, 221, 129, 255}, 3);

  const int wave = static_cast<int>((now / 150U) % 4U);
  fill_rect(renderer, SDL_Rect{0, 184, kScreenWidth, 40}, SDL_Color{71, 121, 178, 255});
  for (int row = 188; row < 220; row += 6) {
    fill_rect(renderer, SDL_Rect{10 + wave, row, 40, 1}, SDL_Color{138, 195, 232, 255});
    fill_rect(renderer, SDL_Rect{90 - wave, row + 2, 56, 1}, SDL_Color{138, 195, 232, 255});
    fill_rect(renderer, SDL_Rect{172 + wave, row + 1, 50, 1}, SDL_Color{138, 195, 232, 255});
  }

  draw_text(renderer, "PRIORY", 128, 22, 5, SDL_Color{248, 240, 222, 255}, true);
  draw_text(renderer, "THE LANTERN WATCH", 128, 56, 2, SDL_Color{70, 84, 62, 255}, true);
  draw_text(renderer, "A PIXELPAL PILGRIMAGE", 128, 72, 1, SDL_Color{70, 84, 62, 255}, true);

  const std::vector<std::string> options =
      state.has_save ? std::vector<std::string>{"CONTINUE", "NEW PILGRIMAGE"}
                     : std::vector<std::string>{"BEGIN PILGRIMAGE"};
  for (std::size_t index = 0; index < options.size(); ++index) {
    const bool selected = static_cast<int>(index) == state.title_selection;
    fill_rect(renderer, SDL_Rect{66, 124 + static_cast<int>(index) * 22, 124, 18},
              selected ? SDL_Color{246, 239, 221, 255} : SDL_Color{194, 184, 157, 220});
    draw_rect(renderer, SDL_Rect{66, 124 + static_cast<int>(index) * 22, 124, 18},
              SDL_Color{96, 83, 61, 255});
    draw_text(renderer, options[index], 128, 129 + static_cast<int>(index) * 22, 1,
              selected ? SDL_Color{59, 87, 54, 255} : SDL_Color{83, 74, 54, 255}, true);
  }

  draw_text(renderer, "A TO SELECT", 128, 202, 1, SDL_Color{245, 236, 215, 255}, true);
}

void audio_callback(void* userdata, Uint8* stream, int length) {
  AudioState* state = static_cast<AudioState*>(userdata);
  int16_t* samples = reinterpret_cast<int16_t*>(stream);
  const int sample_count = length / static_cast<int>(sizeof(int16_t));
  static const std::array<Note, 16> kTheme = {{
      {261.63f, 240}, {329.63f, 240}, {392.00f, 320}, {329.63f, 160},
      {293.66f, 220}, {349.23f, 220}, {392.00f, 320}, {440.00f, 260},
      {392.00f, 220}, {349.23f, 220}, {329.63f, 260}, {293.66f, 180},
      {261.63f, 260}, {329.63f, 260}, {293.66f, 260}, {196.00f, 420},
  }};

  for (int index = 0; index < sample_count; ++index) {
    if (state->melody_samples_remaining <= 0) {
      state->melody_frequency = kTheme[state->note_index].frequency;
      state->melody_samples_remaining = (48000 * kTheme[state->note_index].duration_ms) / 1000;
      state->note_index = (state->note_index + 1) % kTheme.size();
    }

    float sample = 0.0f;
    if (state->melody_frequency > 0.0f) {
      state->melody_phase += (6.2831853f * state->melody_frequency) / 48000.0f;
      if (state->melody_phase >= 6.2831853f) {
        state->melody_phase -= 6.2831853f;
      }
      sample += (state->melody_phase < 3.1415926f ? 0.10f : -0.10f);
      --state->melody_samples_remaining;
    }

    if (state->bell_samples_remaining > 0) {
      state->bell_phase += (6.2831853f * state->bell_frequency) / 48000.0f;
      if (state->bell_phase >= 6.2831853f) {
        state->bell_phase -= 6.2831853f;
      }
      sample += std::sin(state->bell_phase) * 0.18f;
      --state->bell_samples_remaining;
    }

    samples[index] = static_cast<int16_t>(std::clamp(sample, -0.8f, 0.8f) * 32767.0f);
  }
}

struct SearchNode {
  int area_index = 0;
  int x = 0;
  int y = 0;
};

std::string node_key(const SearchNode& node) {
  return std::to_string(node.area_index) + ":" + std::to_string(node.x) + ":" + std::to_string(node.y);
}

bool can_occupy_for_path(const GameState& state, int area_index, int x, int y) {
  if (area_index < 0 || area_index >= static_cast<int>(state.world.size())) {
    return false;
  }
  const Area& area = state.world[area_index];
  if (!tile_in_bounds(area, x, y) || !tile_passable(tile_at(area, x, y))) {
    return false;
  }
  return !tile_occupied_by_npc(state, area.id, x, y);
}

std::vector<Direction> find_path(const GameState& state,
                                 const std::string& target_area_id,
                                 int target_x,
                                 int target_y) {
  const int start_area = area_index_for(state, state.current_area);
  const int target_area = area_index_for(state, target_area_id);
  if (start_area < 0 || target_area < 0) {
    return {};
  }

  const SearchNode start{start_area, state.player.tile_x, state.player.tile_y};
  const SearchNode goal{target_area, target_x, target_y};

  std::queue<SearchNode> queue;
  std::map<std::string, std::pair<std::string, Direction>> parent;
  queue.push(start);
  parent[node_key(start)] = {"", Direction::None};

  while (!queue.empty()) {
    const SearchNode current = queue.front();
    queue.pop();
    if (current.area_index == goal.area_index && current.x == goal.x && current.y == goal.y) {
      break;
    }

    static const std::array<Direction, 4> kDirections = {
        Direction::Up, Direction::Down, Direction::Left, Direction::Right};
    for (Direction direction : kDirections) {
      int dx = 0;
      int dy = 0;
      direction_delta(direction, &dx, &dy);
      const SearchNode next{current.area_index, current.x + dx, current.y + dy};
      if (!can_occupy_for_path(state, next.area_index, next.x, next.y)) {
        continue;
      }
      const std::string key = node_key(next);
      if (parent.count(key) != 0U) {
        continue;
      }
      parent[key] = {node_key(current), direction};
      queue.push(next);
    }

    const Area& area = state.world[current.area_index];
    if (const Warp* warp = warp_at(area, current.x, current.y)) {
      const SearchNode next{area_index_for(state, warp->target_area), warp->target_x, warp->target_y};
      if (next.area_index >= 0) {
        const std::string key = node_key(next);
        if (parent.count(key) == 0U) {
          parent[key] = {node_key(current), Direction::None};
          queue.push(next);
        }
      }
    }
  }

  const std::string goal_key = node_key(goal);
  if (parent.count(goal_key) == 0U) {
    return {};
  }

  std::vector<Direction> path;
  std::string cursor = goal_key;
  while (parent[cursor].first != "") {
    if (parent[cursor].second != Direction::None) {
      path.push_back(parent[cursor].second);
    }
    cursor = parent[cursor].first;
  }
  std::reverse(path.begin(), path.end());
  return path;
}

bool move_to_tile(GameState* state, const std::string& area_id, int x, int y) {
  const std::vector<Direction> path = find_path(*state, area_id, x, y);
  if (path.empty() && !(state->current_area == area_id && state->player.tile_x == x &&
                        state->player.tile_y == y)) {
    return false;
  }

  Uint32 now = 0;
  for (Direction direction : path) {
    if (!step_player_instant(state, direction, now)) {
      return false;
    }
    now += 200;
  }
  return state->current_area == area_id && state->player.tile_x == x && state->player.tile_y == y;
}

bool talk_to_npc(GameState* state, const std::string& area_id, const std::string& npc_id, AudioState* audio) {
  const Npc* npc = find_npc(*state, area_id, npc_id);
  if (npc == nullptr) {
    return false;
  }

  static const std::array<std::pair<int, int>, 4> kOffsets = {{{0, 1}, {0, -1}, {1, 0}, {-1, 0}}};
  for (const auto& offset : kOffsets) {
    const int tile_x = npc->x + offset.first;
    const int tile_y = npc->y + offset.second;
    if (!can_occupy(*state, area_id, tile_x, tile_y)) {
      continue;
    }
    GameState probe = *state;
    if (!move_to_tile(&probe, area_id, tile_x, tile_y)) {
      continue;
    }
    *state = probe;
    if (offset.first == 1) {
      state->player.facing = Direction::Left;
    } else if (offset.first == -1) {
      state->player.facing = Direction::Right;
    } else if (offset.second == 1) {
      state->player.facing = Direction::Up;
    } else {
      state->player.facing = Direction::Down;
    }
    interact(state, 0, audio);
    while (state->dialogue.active) {
      advance_dialogue(state);
    }
    return true;
  }
  return false;
}

bool run_smoke_test(GameState* state) {
  AudioState audio;
  start_new_game(state, 0);
  while (state->dialogue.active) {
    advance_dialogue(state);
  }

  if (!talk_to_npc(state, "chapel", "prior-seraphim", &audio)) {
    std::fprintf(stderr, "Smoke test failed: could not talk to prior.\n");
    return false;
  }
  if (!state->quest.started) {
    std::fprintf(stderr, "Smoke test failed: quest did not start.\n");
    return false;
  }
  if (!talk_to_npc(state, "priory-court", "sister-agnes", &audio) || !state->quest.wax) {
    std::fprintf(stderr, "Smoke test failed: wax step missing.\n");
    return false;
  }
  if (!talk_to_npc(state, "scriptorium", "brother-lucian", &audio) || !state->quest.hymn) {
    std::fprintf(stderr, "Smoke test failed: hymn step missing.\n");
    return false;
  }
  if (!talk_to_npc(state, "candlewharf", "tomas", &audio) || !state->quest.oil) {
    std::fprintf(stderr, "Smoke test failed: oil step missing.\n");
    return false;
  }
  if (!talk_to_npc(state, "chapel", "prior-seraphim", &audio) || !state->quest.complete) {
    std::fprintf(stderr, "Smoke test failed: completion did not trigger.\n");
    return false;
  }

  std::printf("PRIORY SMOKE TEST OK\n");
  return true;
}

}  // namespace
