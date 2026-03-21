#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kVirtualWidth = 512;
constexpr int kVirtualHeight = 512;
constexpr int kGridWidth = 48;
constexpr int kGridHeight = 48;
constexpr int kCellSize = 12;
constexpr int kBoardViewX = 10;
constexpr int kBoardViewY = 38;
constexpr int kBoardViewW = 374;
constexpr int kBoardViewH = 464;
constexpr int kSidePanelX = 392;
constexpr int kSidePanelY = 38;
constexpr int kSidePanelW = 110;
constexpr int kSidePanelH = 464;
constexpr int kWorldWidth = kGridWidth * kCellSize;
constexpr int kWorldHeight = kGridHeight * kCellSize;
constexpr int kTotalPlayers = 10;
constexpr float kBaseMoveSeconds = 0.18f;
constexpr float kBotThinkSeconds = 0.08f;
constexpr float kRespawnSeconds = 1.8f;

enum class Dir { None = 0, Left, Right, Up, Down };
enum class Screen { ModeSelect = 0, Playing, GameOver };

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct Vec2i {
  int x = 0;
  int y = 0;
};

struct Vec2f {
  float x = 0.0f;
  float y = 0.0f;
};

struct DifficultyInfo {
  const char* name;
  const char* label;
  const char* ai_note;
  int lives;
  float think_scale;
  float bot_speed_scale;
  int trail_goal;
  int edge_margin;
  int boldness;
};

struct Player {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  bool human = false;
  bool alive = true;
  Vec2i pos{};
  Vec2i prev_pos{};
  Vec2i spawn{};
  Dir dir = Dir::Right;
  Dir queued_dir = Dir::Right;
  float move_timer = 0.0f;
  float think_timer = 0.0f;
  float respawn_timer = 0.0f;
  float boost_timer = 0.0f;
  int territory = 0;
  int trail_len = 0;
  int retreat = 0;
  int hunt_bias = 0;
  int caution_bias = 0;
  int greed_bias = 0;
  int focus_bias = 0;
};

struct CameraState {
  float x = 0.0f;
  float y = 0.0f;
};

struct GameState {
  pp_context context{};
  std::mt19937 rng{0x52494242u};
  std::array<Player, kTotalPlayers> players{};
  std::array<std::array<int, kGridWidth>, kGridHeight> land_owner{};
  std::array<std::array<int, kGridWidth>, kGridHeight> trail_owner{};
  ToneState tone{};
  CameraState camera{};
  Screen screen = Screen::ModeSelect;
  bool paused = false;
  bool victory = false;
  int difficulty_index = 0;
  int human_lives = 0;
  int leader = 0;
  std::string status = "CHOOSE A DIFFICULTY";
  float status_timer = 0.0f;
  Uint32 last_ticks = 0;
};

struct TrailTarget {
  Vec2i cell{-1, -1};
  int owner = -1;
  int distance = 999;
};

static const std::array<DifficultyInfo, 3> kDifficulties{{
    {"EASY", "5 LIVES", "CALMER BOTS", 5, 1.35f, 1.08f, 6, 6, 10},
    {"MEDIUM", "3 LIVES", "SHARPER BOTS", 3, 1.00f, 1.00f, 9, 5, 16},
    {"HARD", "2 LIVES", "RUTHLESS BOTS", 2, 0.74f, 0.92f, 13, 3, 24},
}};

static const std::array<std::string, kTotalPlayers> kNames{{
    "YOU", "VANTA", "HEX", "SPARK", "NOVA", "GALA", "CIRRUS", "RUSH", "MINT", "FABLE"}};
static const std::array<SDL_Color, kTotalPlayers> kColors{{
    {248, 247, 255, 255}, {255, 104, 148, 255}, {112, 232, 212, 255}, {248, 176, 96, 255},
    {176, 126, 255, 255}, {122, 220, 122, 255}, {110, 188, 255, 255}, {255, 144, 88, 255},
    {104, 240, 188, 255}, {255, 214, 112, 255}}};
static const std::array<Vec2i, kTotalPlayers> kSpawnPoints{{
    {7, 7}, {40, 7}, {7, 40}, {40, 40}, {24, 7}, {24, 40}, {7, 24}, {40, 24}, {16, 16}, {31, 31}}};

static const SDL_Color kBgTop{10, 16, 30, 255};
static const SDL_Color kBgBottom{18, 32, 58, 255};
static const SDL_Color kPanel{18, 26, 42, 232};
static const SDL_Color kPanelHi{32, 46, 74, 236};
static const SDL_Color kFrame{96, 126, 176, 255};
static const SDL_Color kText{238, 244, 255, 255};
static const SDL_Color kMuted{144, 166, 192, 255};
static const SDL_Color kAccent{120, 235, 210, 255};
static const SDL_Color kWarn{255, 198, 104, 255};
static const SDL_Color kDanger{255, 98, 128, 255};
static const SDL_Color kBoardBase{16, 23, 35, 255};
static const SDL_Color kBoardGrid{26, 35, 54, 255};

void kill_player(GameState& game, int index, const std::string& reason);
void maybe_finish_match(GameState& game);
int eliminate_claimed_players(GameState& game, int claimer);

int clamp_int(int value, int lo, int hi) { return std::max(lo, std::min(hi, value)); }

float clamp_float(float value, float lo, float hi) { return std::max(lo, std::min(hi, value)); }

float lerp(float a, float b, float t) { return a + (b - a) * t; }

Vec2i add(Vec2i a, Vec2i b) { return {a.x + b.x, a.y + b.y}; }

int manhattan(Vec2i a, Vec2i b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

Dir opposite(Dir dir) {
  switch (dir) {
    case Dir::Left: return Dir::Right;
    case Dir::Right: return Dir::Left;
    case Dir::Up: return Dir::Down;
    case Dir::Down: return Dir::Up;
    default: return Dir::None;
  }
}

Vec2i dir_delta(Dir dir) {
  switch (dir) {
    case Dir::Left: return {-1, 0};
    case Dir::Right: return {1, 0};
    case Dir::Up: return {0, -1};
    case Dir::Down: return {0, 1};
    default: return {0, 0};
  }
}

bool in_bounds(Vec2i p) {
  return p.x >= 0 && p.x < kGridWidth && p.y >= 0 && p.y < kGridHeight;
}

int edge_distance(Vec2i p) {
  return std::min(std::min(p.x, kGridWidth - 1 - p.x), std::min(p.y, kGridHeight - 1 - p.y));
}

const DifficultyInfo& difficulty_info(const GameState& game) {
  return kDifficulties[static_cast<std::size_t>(clamp_int(game.difficulty_index, 0, static_cast<int>(kDifficulties.size()) - 1))];
}

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

std::string uppercase(std::string text) {
  for (char& ch : text) {
    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
  }
  return text;
}

int text_width(const std::string& text, int scale) {
  if (text.empty()) return 0;
  return static_cast<int>(text.size()) * (6 * scale) - scale;
}

void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void fill_circle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int y = -radius; y <= radius; ++y) {
    for (int x = -radius; x <= radius; ++x) {
      if (x * x + y * y <= radius * radius) SDL_RenderDrawPoint(renderer, cx + x, cy + y);
    }
  }
}

void draw_text(SDL_Renderer* renderer, const std::string& raw, int x, int y, int scale, SDL_Color color, bool centered = false) {
  const std::string text = uppercase(raw);
  int draw_x = centered ? x - text_width(text, scale) / 2 : x;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (char ch : text) {
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

void draw_text_right(SDL_Renderer* renderer, const std::string& text, int right, int y, int scale, SDL_Color color) {
  draw_text(renderer, text, right - text_width(text, scale), y, scale, color, false);
}

void audio_callback(void* userdata, Uint8* stream, int length) {
  ToneState* tone = static_cast<ToneState*>(userdata);
  auto* samples = reinterpret_cast<int16_t*>(stream);
  const int count = length / static_cast<int>(sizeof(int16_t));
  for (int index = 0; index < count; ++index) {
    int16_t sample = 0;
    if (tone->frames_remaining > 0) {
      sample = tone->phase < 3.14159f ? 1600 : -1600;
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

SDL_Color shade(SDL_Color color, int delta) {
  auto channel = [&](int value) { return static_cast<Uint8>(clamp_int(value, 0, 255)); };
  return {channel(static_cast<int>(color.r) + delta), channel(static_cast<int>(color.g) + delta),
          channel(static_cast<int>(color.b) + delta), color.a};
}

void set_status(GameState& game, const std::string& text, float seconds) {
  game.status = text;
  game.status_timer = seconds;
}

void recount_territories(GameState& game) {
  for (Player& player : game.players) player.territory = 0;
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      const int owner = game.land_owner[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      if (owner >= 0 && owner < kTotalPlayers) {
        ++game.players[static_cast<std::size_t>(owner)].territory;
      }
    }
  }
}

void clear_trail(GameState& game, int index) {
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      if (game.trail_owner[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == index) {
        game.trail_owner[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = -1;
      }
    }
  }
  game.players[static_cast<std::size_t>(index)].trail_len = 0;
}

void claim_home_block(GameState& game, int index, Vec2i center) {
  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      Vec2i p{center.x + dx, center.y + dy};
      if (!in_bounds(p)) continue;
      game.land_owner[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] = index;
      game.trail_owner[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] = -1;
    }
  }
}

float move_interval_for(const GameState& game, int index) {
  const Player& player = game.players[static_cast<std::size_t>(index)];
  float interval = kBaseMoveSeconds;
  if (!player.human) {
    interval *= difficulty_info(game).bot_speed_scale;
    interval *= clamp_float(1.08f - 0.018f * static_cast<float>(player.focus_bias), 0.82f, 1.08f);
  }
  if (player.boost_timer > 0.0f) interval *= 0.65f;
  return interval;
}

Vec2f render_cell_for(const GameState& game, int index) {
  const Player& player = game.players[static_cast<std::size_t>(index)];
  const float interval = std::max(0.0001f, move_interval_for(game, index));
  const float alpha = clamp_float(player.move_timer / interval, 0.0f, 1.0f);
  return {lerp(static_cast<float>(player.prev_pos.x), static_cast<float>(player.pos.x), alpha),
          lerp(static_cast<float>(player.prev_pos.y), static_cast<float>(player.pos.y), alpha)};
}

CameraState camera_target_for(const GameState& game) {
  const Player& human = game.players[0];
  const Vec2f cell = render_cell_for(game, 0);
  const Vec2i look = dir_delta(human.queued_dir != Dir::None ? human.queued_dir : human.dir);
  float center_x = (cell.x + 0.5f) * static_cast<float>(kCellSize) + static_cast<float>(look.x * kCellSize * 5);
  float center_y = (cell.y + 0.5f) * static_cast<float>(kCellSize) + static_cast<float>(look.y * kCellSize * 4);
  return {clamp_float(center_x - kBoardViewW * 0.5f, 0.0f, static_cast<float>(kWorldWidth - kBoardViewW)),
          clamp_float(center_y - kBoardViewH * 0.5f, 0.0f, static_cast<float>(kWorldHeight - kBoardViewH))};
}

void reset_player(Player& player, int index) {
  player.pos = kSpawnPoints[static_cast<std::size_t>(index)];
  player.prev_pos = player.pos;
  player.spawn = player.pos;
  player.alive = true;
  player.dir = index % 2 == 0 ? Dir::Right : Dir::Left;
  player.queued_dir = player.dir;
  player.move_timer = 0.0f;
  player.think_timer = 0.0f;
  player.respawn_timer = 0.0f;
  player.boost_timer = 0.0f;
  player.territory = 0;
  player.trail_len = 0;
  player.retreat = 0;
  player.hunt_bias = 0;
  player.caution_bias = 0;
  player.greed_bias = 0;
  player.focus_bias = 0;
}

void reset_match(GameState& game) {
  for (auto& row : game.land_owner) row.fill(-1);
  for (auto& row : game.trail_owner) row.fill(-1);

  std::array<int, kTotalPlayers - 1> bot_mix{{-2, -1, -1, 0, 0, 0, 1, 1, 2}};
  std::shuffle(bot_mix.begin(), bot_mix.end(), game.rng);
  std::uniform_int_distribution<int> offset_roll(-1, 1);
  std::uniform_int_distribution<int> style_roll(0, 2);
  int bot_slot = 0;

  for (int index = 0; index < kTotalPlayers; ++index) {
    Player& player = game.players[static_cast<std::size_t>(index)];
    player.name = kNames[static_cast<std::size_t>(index)];
    player.color = kColors[static_cast<std::size_t>(index)];
    player.human = index == 0;
    reset_player(player, index);
    if (!player.human) {
      const int tier = bot_mix[static_cast<std::size_t>(bot_slot++)];
      const int style = style_roll(game.rng);
      player.hunt_bias = clamp_int(5 + game.difficulty_index * 2 + tier + offset_roll(game.rng), 1, 12);
      player.caution_bias = clamp_int(5 - tier + offset_roll(game.rng), 1, 12);
      player.greed_bias = clamp_int(4 + game.difficulty_index + tier + offset_roll(game.rng), 1, 12);
      player.focus_bias = clamp_int(5 + game.difficulty_index * 2 + tier + offset_roll(game.rng), 1, 12);
      if (style == 0) {
        player.hunt_bias = clamp_int(player.hunt_bias + 2, 1, 12);
        player.greed_bias = clamp_int(player.greed_bias + 1, 1, 12);
      } else if (style == 1) {
        player.caution_bias = clamp_int(player.caution_bias + 2, 1, 12);
      } else {
        player.focus_bias = clamp_int(player.focus_bias + 2, 1, 12);
      }
    }
    claim_home_block(game, index, player.spawn);
  }

  recount_territories(game);
  game.paused = false;
  game.victory = false;
  game.leader = 0;
}

void begin_match(GameState& game) {
  game.human_lives = difficulty_info(game).lives;
  game.screen = Screen::Playing;
  game.victory = false;
  reset_match(game);
  game.camera = camera_target_for(game);
  set_status(game, std::string(difficulty_info(game).name) + " RAID / " + std::to_string(game.human_lives) + " LIVES", 1.8f);
}

bool tile_has_head(const GameState& game, Vec2i p, int skip) {
  for (int index = 0; index < kTotalPlayers; ++index) {
    if (index == skip) continue;
    const Player& player = game.players[static_cast<std::size_t>(index)];
    if (!player.alive) continue;
    if (player.pos.x == p.x && player.pos.y == p.y) return true;
  }
  return false;
}

bool border_cell(const GameState& game, int index, Vec2i p) {
  if (!in_bounds(p)) return false;
  if (game.land_owner[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] != index) return false;
  for (const Vec2i delta : std::array<Vec2i, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
    const Vec2i n = add(p, delta);
    if (!in_bounds(n)) return true;
    if (game.land_owner[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] != index) return true;
  }
  return false;
}

int nearest_enemy_head_distance(const GameState& game, int index, Vec2i from) {
  int best = 999;
  for (int other = 0; other < kTotalPlayers; ++other) {
    if (other == index) continue;
    const Player& player = game.players[static_cast<std::size_t>(other)];
    if (!player.alive) continue;
    best = std::min(best, manhattan(from, player.pos));
  }
  return best;
}

TrailTarget nearest_enemy_trail(const GameState& game, int index, Vec2i from) {
  TrailTarget best{};
  int best_score = -100000;
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      const int owner = game.trail_owner[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      if (owner < 0 || owner == index) continue;
      if (!game.players[static_cast<std::size_t>(owner)].alive) continue;

      const Vec2i cell{x, y};
      const int distance = manhattan(from, cell);
      const int score = game.players[static_cast<std::size_t>(owner)].trail_len * 20 - distance * 14 + (owner == 0 ? 120 : 0);
      if (score > best_score) {
        best_score = score;
        best.cell = cell;
        best.owner = owner;
        best.distance = distance;
      }
    }
  }
  return best;
}

int flood_space(const GameState& game, Vec2i start, int moving_index, int budget) {
  if (!in_bounds(start)) return 0;
  if (game.trail_owner[static_cast<std::size_t>(start.y)][static_cast<std::size_t>(start.x)] != -1) return 0;
  if (tile_has_head(game, start, moving_index)) return 0;

  std::array<std::array<uint8_t, kGridWidth>, kGridHeight> seen{};
  std::vector<Vec2i> queue;
  queue.push_back(start);
  seen[static_cast<std::size_t>(start.y)][static_cast<std::size_t>(start.x)] = 1;
  std::size_t head = 0;
  int score = 0;

  while (head < queue.size() && score < budget) {
    const Vec2i p = queue[head++];
    ++score;
    for (const Vec2i delta : std::array<Vec2i, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
      const Vec2i n = add(p, delta);
      if (!in_bounds(n)) continue;
      if (seen[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)]) continue;
      if (game.trail_owner[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] != -1) continue;
      if (tile_has_head(game, n, moving_index)) continue;
      seen[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] = 1;
      queue.push_back(n);
    }
  }
  return score;
}

Vec2i best_frontier(const GameState& game, int index) {
  const Player& player = game.players[static_cast<std::size_t>(index)];
  Vec2i best = player.pos;
  int best_score = -100000;
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      const Vec2i p{x, y};
      if (!border_cell(game, index, p)) continue;

      int exposure = 0;
      for (const Vec2i delta : std::array<Vec2i, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
        const Vec2i n = add(p, delta);
        if (!in_bounds(n)) {
          ++exposure;
          continue;
        }
        if (game.land_owner[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] != index) ++exposure;
      }

      const int score = exposure * 16 + edge_distance(p) * 4 - manhattan(player.pos, p) -
                        std::max(0, 7 - nearest_enemy_head_distance(game, index, p)) * 10;
      if (score > best_score) {
        best_score = score;
        best = p;
      }
    }
  }
  return best;
}

Dir bfs_direction(const GameState& game, Vec2i start, Vec2i target, bool avoid_trails, int moving_index) {
  if (start.x == target.x && start.y == target.y) return Dir::None;

  std::array<std::array<int, kGridWidth>, kGridHeight> parent{};
  std::array<std::array<uint8_t, kGridWidth>, kGridHeight> seen{};
  std::vector<Vec2i> queue;
  queue.push_back(start);
  seen[static_cast<std::size_t>(start.y)][static_cast<std::size_t>(start.x)] = 1;
  std::size_t head = 0;

  while (head < queue.size()) {
    const Vec2i p = queue[head++];
    if (p.x == target.x && p.y == target.y) break;

    const std::array<Dir, 4> dirs{Dir::Left, Dir::Right, Dir::Up, Dir::Down};
    for (int dir_index = 0; dir_index < 4; ++dir_index) {
      const Dir dir = dirs[static_cast<std::size_t>(dir_index)];
      const Vec2i n = add(p, dir_delta(dir));
      if (!in_bounds(n)) continue;
      if (seen[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)]) continue;
      if (avoid_trails && game.trail_owner[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] != -1) continue;
      if (tile_has_head(game, n, moving_index)) continue;
      seen[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] = 1;
      parent[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] = dir_index + 1;
      queue.push_back(n);
    }
  }

  if (!seen[static_cast<std::size_t>(target.y)][static_cast<std::size_t>(target.x)]) return Dir::None;

  Vec2i cur = target;
  while (!(cur.x == start.x && cur.y == start.y)) {
    const int code = parent[static_cast<std::size_t>(cur.y)][static_cast<std::size_t>(cur.x)];
    const Dir dir = static_cast<Dir>(code);
    cur = add(cur, dir_delta(opposite(dir)));
    if (cur.x == start.x && cur.y == start.y) return dir;
  }
  return Dir::None;
}

Dir bfs_direction_to_owned_land(const GameState& game, int index, Vec2i start) {
  std::array<std::array<int, kGridWidth>, kGridHeight> parent{};
  std::array<std::array<uint8_t, kGridWidth>, kGridHeight> seen{};
  std::vector<Vec2i> queue;
  queue.push_back(start);
  seen[static_cast<std::size_t>(start.y)][static_cast<std::size_t>(start.x)] = 1;
  std::size_t head = 0;
  Vec2i found{-1, -1};

  while (head < queue.size()) {
    const Vec2i p = queue[head++];
    if (!(p.x == start.x && p.y == start.y) &&
        game.land_owner[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] == index) {
      found = p;
      break;
    }

    const std::array<Dir, 4> dirs{Dir::Left, Dir::Right, Dir::Up, Dir::Down};
    for (int dir_index = 0; dir_index < 4; ++dir_index) {
      const Dir dir = dirs[static_cast<std::size_t>(dir_index)];
      const Vec2i n = add(p, dir_delta(dir));
      if (!in_bounds(n)) continue;
      if (seen[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)]) continue;
      if (game.trail_owner[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] != -1) continue;
      if (tile_has_head(game, n, index)) continue;
      seen[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] = 1;
      parent[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] = dir_index + 1;
      queue.push_back(n);
    }
  }

  if (found.x < 0) return Dir::None;

  Vec2i cur = found;
  while (!(cur.x == start.x && cur.y == start.y)) {
    const int code = parent[static_cast<std::size_t>(cur.y)][static_cast<std::size_t>(cur.x)];
    const Dir dir = static_cast<Dir>(code);
    cur = add(cur, dir_delta(opposite(dir)));
    if (cur.x == start.x && cur.y == start.y) return dir;
  }
  return Dir::None;
}

void capture_region(GameState& game, int index) {
  std::array<std::array<uint8_t, kGridWidth>, kGridHeight> visited{};
  std::vector<Vec2i> queue;

  for (int x = 0; x < kGridWidth; ++x) {
    queue.push_back({x, 0});
    queue.push_back({x, kGridHeight - 1});
  }
  for (int y = 0; y < kGridHeight; ++y) {
    queue.push_back({0, y});
    queue.push_back({kGridWidth - 1, y});
  }

  auto is_blocked = [&](Vec2i p) {
    if (!in_bounds(p)) return true;
    return game.land_owner[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] == index ||
           game.trail_owner[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] == index;
  };

  std::size_t head = 0;
  while (head < queue.size()) {
    const Vec2i p = queue[head++];
    if (!in_bounds(p)) continue;
    if (visited[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)]) continue;
    if (is_blocked(p)) continue;
    visited[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] = 1;
    queue.push_back({p.x - 1, p.y});
    queue.push_back({p.x + 1, p.y});
    queue.push_back({p.x, p.y - 1});
    queue.push_back({p.x, p.y + 1});
  }

  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      if (visited[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) continue;
      game.land_owner[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = index;
    }
  }

  clear_trail(game, index);
  recount_territories(game);
  game.players[static_cast<std::size_t>(index)].retreat = 0;
  const int eliminated = eliminate_claimed_players(game, index);
  if (game.players[static_cast<std::size_t>(index)].human) {
    trigger_tone(game.tone, 840.0f, 52);
    if (eliminated > 0) {
      set_status(game, "SEALED OUT " + std::to_string(eliminated), 1.1f);
    } else {
      set_status(game, "SWEEP CLAIMED", 1.0f);
    }
  }
  maybe_finish_match(game);
}

void respawn_player(GameState& game, int index) {
  Player& player = game.players[static_cast<std::size_t>(index)];
  player.alive = true;
  player.pos = player.spawn;
  player.prev_pos = player.spawn;
  player.dir = index % 2 == 0 ? Dir::Right : Dir::Left;
  player.queued_dir = player.dir;
  player.move_timer = 0.0f;
  player.think_timer = 0.0f;
  player.respawn_timer = 0.0f;
  player.boost_timer = 0.0f;
  player.trail_len = 0;
  player.retreat = 0;
  claim_home_block(game, index, player.spawn);
  recount_territories(game);
  if (player.human) {
    trigger_tone(game.tone, 520.0f, 56);
    set_status(game, std::to_string(game.human_lives) + " LIVES STILL IN PLAY", 0.9f);
  }
}

void kill_player(GameState& game, int index, const std::string& reason) {
  Player& player = game.players[static_cast<std::size_t>(index)];
  if (!player.alive) return;
  clear_trail(game, index);
  player.alive = false;
  player.prev_pos = player.pos;
  player.move_timer = 0.0f;
  player.boost_timer = 0.0f;
  player.retreat = 0;

  if (player.human) {
    game.human_lives = std::max(0, game.human_lives - 1);
    trigger_tone(game.tone, 160.0f, 130);
    if (game.human_lives > 0) {
      player.respawn_timer = kRespawnSeconds;
      set_status(game, reason + " / " + std::to_string(game.human_lives) + " LEFT", 1.4f);
    } else {
      player.respawn_timer = 0.0f;
      game.screen = Screen::GameOver;
      game.paused = false;
      set_status(game, "OUT OF LIVES", 2.0f);
    }
  } else {
    player.respawn_timer = 0.0f;
  }

  maybe_finish_match(game);
}

int eliminate_claimed_players(GameState& game, int claimer) {
  std::vector<int> victims;
  for (int index = 0; index < kTotalPlayers; ++index) {
    if (index == claimer) continue;
    const Player& player = game.players[static_cast<std::size_t>(index)];
    if (!player.alive) continue;
    if (player.territory > 0) continue;
    victims.push_back(index);
  }

  for (int victim : victims) {
    const bool human_victim = game.players[static_cast<std::size_t>(victim)].human;
    const std::string reason =
        human_victim ? game.players[static_cast<std::size_t>(claimer)].name + " SEALED YOU OUT"
                     : game.players[static_cast<std::size_t>(victim)].name + " WAS SEALED OUT";
    kill_player(game, victim, reason);
  }

  if (!victims.empty()) recount_territories(game);
  return static_cast<int>(victims.size());
}

void maybe_finish_match(GameState& game) {
  if (game.screen != Screen::Playing) return;

  int living_bots = 0;
  for (int index = 1; index < kTotalPlayers; ++index) {
    if (game.players[static_cast<std::size_t>(index)].alive) ++living_bots;
  }
  if (living_bots != 0) return;
  if (game.human_lives <= 0) return;

  game.victory = true;
  game.leader = 0;
  game.screen = Screen::GameOver;
  game.paused = false;
  trigger_tone(game.tone, 980.0f, 92);
  set_status(game, "ARENA CLEARED", 2.0f);
}

void move_player(GameState& game, int index, Dir requested_dir) {
  Player& player = game.players[static_cast<std::size_t>(index)];
  if (!player.alive) return;

  if (requested_dir != Dir::None && requested_dir != opposite(player.dir)) {
    player.queued_dir = requested_dir;
  }

  const Dir dir = player.queued_dir;
  const Vec2i step = dir_delta(dir);
  if (step.x == 0 && step.y == 0) return;

  const Vec2i next = add(player.pos, step);
  if (!in_bounds(next)) {
    kill_player(game, index, player.name + " HIT THE RIM");
    return;
  }

  if (tile_has_head(game, next, index)) {
    for (int other = 0; other < kTotalPlayers; ++other) {
      if (other == index) continue;
      if (!game.players[static_cast<std::size_t>(other)].alive) continue;
      if (game.players[static_cast<std::size_t>(other)].pos.x == next.x &&
          game.players[static_cast<std::size_t>(other)].pos.y == next.y) {
        kill_player(game, other, game.players[static_cast<std::size_t>(other)].name + " LOST THE COLLISION");
      }
    }
    if (game.screen != Screen::Playing) return;
  }

  const int next_trail = game.trail_owner[static_cast<std::size_t>(next.y)][static_cast<std::size_t>(next.x)];
  if (next_trail == index) {
    kill_player(game, index, "YOUR RIBBON CAUGHT YOU");
    return;
  }
  if (next_trail != -1) {
    const int victim = next_trail;
    kill_player(game, victim, game.players[static_cast<std::size_t>(victim)].name + " WAS CUT DOWN");
    if (index == 0 && game.screen == Screen::Playing) {
      trigger_tone(game.tone, 920.0f, 38);
      set_status(game, game.players[static_cast<std::size_t>(victim)].name + " RIBBON CUT", 1.0f);
    }
    if (game.screen != Screen::Playing) return;
  }

  const int current_land = game.land_owner[static_cast<std::size_t>(player.pos.y)][static_cast<std::size_t>(player.pos.x)];
  const int next_land = game.land_owner[static_cast<std::size_t>(next.y)][static_cast<std::size_t>(next.x)];

  player.prev_pos = player.pos;

  if (current_land == index && next_land != index &&
      game.trail_owner[static_cast<std::size_t>(player.pos.y)][static_cast<std::size_t>(player.pos.x)] == -1) {
    game.trail_owner[static_cast<std::size_t>(player.pos.y)][static_cast<std::size_t>(player.pos.x)] = index;
    player.trail_len = 1;
  } else if (current_land != index && player.trail_len > 0) {
    ++player.trail_len;
  }

  player.pos = next;
  player.dir = dir;

  if (next_land == index && player.trail_len > 0) {
    capture_region(game, index);
    return;
  }

  if (next_land != index) {
    game.trail_owner[static_cast<std::size_t>(next.y)][static_cast<std::size_t>(next.x)] = index;
    if (player.trail_len == 0) {
      player.trail_len = 1;
    } else {
      ++player.trail_len;
    }
  }
}

Dir score_best_direction(const GameState& game, int index, Vec2i target, const TrailTarget& trail_target, Dir retreat_dir,
                         bool on_home) {
  const Player& bot = game.players[static_cast<std::size_t>(index)];
  const DifficultyInfo& tuning = difficulty_info(game);
  const std::array<Dir, 4> dirs{Dir::Left, Dir::Right, Dir::Up, Dir::Down};

  int best_score = -1000000;
  Dir best_dir = bot.dir;
  for (Dir dir : dirs) {
    const Vec2i next = add(bot.pos, dir_delta(dir));
    if (!in_bounds(next)) continue;
    if (tile_has_head(game, next, index)) continue;

    const int next_trail = game.trail_owner[static_cast<std::size_t>(next.y)][static_cast<std::size_t>(next.x)];
    if (next_trail == index) continue;
    const bool cutting_ribbon = next_trail >= 0 && next_trail != index;

    const bool on_own_land = game.land_owner[static_cast<std::size_t>(next.y)][static_cast<std::size_t>(next.x)] == index;
    const int open_score = cutting_ribbon ? 48 : flood_space(game, next, index, 110 + tuning.boldness);
    const int enemy_dist = nearest_enemy_head_distance(game, index, next);
    const int edge = edge_distance(next);

    int score = open_score * 2 + edge * 14;
    score += dir == bot.dir ? 18 : 0;
    score -= dir == opposite(bot.dir) ? 50 : 0;
    score -= std::max(0, 8 - enemy_dist) * std::max(6, 18 - bot.hunt_bias);

    if (target.x >= 0) {
      score -= manhattan(next, target) * (on_home ? 4 : 2);
    }
    if (trail_target.owner >= 0) {
      const int trail_distance = manhattan(next, trail_target.cell);
      score -= trail_distance * (4 + bot.hunt_bias * 2);
      score += trail_target.owner == 0 ? 80 + bot.hunt_bias * 10 : 30 + bot.hunt_bias * 4;
      if (on_home && bot.trail_len == 0 && trail_target.distance <= 10) score += 70 + bot.hunt_bias * 8;
    }
    if (cutting_ribbon) {
      score += 900 + bot.hunt_bias * 70 + game.players[static_cast<std::size_t>(next_trail)].trail_len * 26;
      if (next_trail == 0) score += 240;
    }

    if (bot.trail_len == 0) {
      score += on_own_land ? 6 : (34 + tuning.boldness);
      if (!on_own_land && edge < tuning.edge_margin) {
        score -= (tuning.edge_margin - edge + 1) * 22;
      }
    } else {
      score -= bot.trail_len * 3;
      if (on_own_land) score += 130 + bot.trail_len * 5;
      if (retreat_dir == dir) score += bot.trail_len > tuning.trail_goal ? 160 : 38;
      if (!on_own_land) score += std::max(0, tuning.trail_goal - bot.trail_len) * 10;
      if (open_score < 18) score -= 100;
      if (edge < tuning.edge_margin) score -= (tuning.edge_margin - edge + 1) * 28;
    }

    if (score > best_score) {
      best_score = score;
      best_dir = dir;
    }
  }
  return best_dir;
}

void step_ai(GameState& game, int index, float dt) {
  Player& bot = game.players[static_cast<std::size_t>(index)];
  if (!bot.alive) return;

  bot.think_timer -= dt;
  if (bot.think_timer > 0.0f) return;

  const DifficultyInfo& tuning = difficulty_info(game);
  bot.think_timer =
      kBotThinkSeconds * tuning.think_scale * clamp_float(1.18f - 0.04f * static_cast<float>(bot.focus_bias), 0.68f, 1.12f) +
      0.004f * static_cast<float>(index % 3);

  const bool on_home = game.land_owner[static_cast<std::size_t>(bot.pos.y)][static_cast<std::size_t>(bot.pos.x)] == index;
  const Dir retreat_dir = bot.trail_len > 0 ? bfs_direction_to_owned_land(game, index, bot.pos) : Dir::None;
  const TrailTarget trail_target = nearest_enemy_trail(game, index, bot.pos);
  Vec2i target = best_frontier(game, index);
  if (trail_target.owner >= 0 && trail_target.distance <= 12 + bot.hunt_bias) {
    target = trail_target.cell;
  }

  Dir desired = score_best_direction(game, index, target, trail_target, retreat_dir, on_home);
  const int edge = edge_distance(bot.pos);
  const int retreat_goal = std::max(4, tuning.trail_goal + (bot.greed_bias - bot.caution_bias) / 2);
  const int safe_edge = std::max(1, tuning.edge_margin + (bot.caution_bias - bot.greed_bias) / 3);
  const Vec2i desired_next = desired != Dir::None ? add(bot.pos, dir_delta(desired)) : bot.pos;
  const int desired_trail =
      in_bounds(desired_next) ? game.trail_owner[static_cast<std::size_t>(desired_next.y)][static_cast<std::size_t>(desired_next.x)]
                              : -1;
  const bool immediate_cut = desired_trail >= 0 && desired_trail != index;
  const int next_space = immediate_cut ? 999 : flood_space(game, desired_next, index, 72 + bot.focus_bias * 4);
  const int min_space = std::max(10, 12 + bot.caution_bias * 2 - bot.greed_bias);
  const bool ribbon_pressure = trail_target.owner >= 0 && trail_target.distance <= 4 + bot.hunt_bias / 2;

  if (bot.retreat) {
    if (retreat_dir != Dir::None) desired = retreat_dir;
    bot.retreat = 0;
  } else if (bot.trail_len > 0 &&
             (bot.trail_len > retreat_goal || edge < safe_edge || next_space < min_space) &&
             !(immediate_cut || (ribbon_pressure && bot.trail_len <= retreat_goal + 2 && next_space >= min_space / 2))) {
    if (retreat_dir != Dir::None) desired = retreat_dir;
  } else if (desired == Dir::None && retreat_dir != Dir::None) {
    desired = retreat_dir;
  }

  if (desired == Dir::None) desired = bot.dir;
  bot.queued_dir = desired;
}

void update_camera(GameState& game, float dt) {
  const CameraState target = camera_target_for(game);
  const float blend = clamp_float(dt * 6.5f, 0.0f, 1.0f);
  game.camera.x = lerp(game.camera.x, target.x, blend);
  game.camera.y = lerp(game.camera.y, target.y, blend);
}

void update_human(GameState& game, const pp_input_state& input, bool a_pressed, bool b_pressed, bool start_pressed, bool select_pressed) {
  Player& human = game.players[0];

  if (start_pressed) {
    game.paused = !game.paused;
    trigger_tone(game.tone, game.paused ? 240.0f : 620.0f, 60);
  }
  if (select_pressed) pp_request_exit(&game.context);
  if (game.paused || !human.alive) return;

  Dir requested = Dir::None;
  if (input.left) requested = Dir::Left;
  if (input.right) requested = Dir::Right;
  if (input.up) requested = Dir::Up;
  if (input.down) requested = Dir::Down;

  if (requested != Dir::None && requested != opposite(human.dir)) {
    human.queued_dir = requested;
    human.retreat = 0;
  }

  if (a_pressed) {
    human.boost_timer = 0.72f;
    trigger_tone(game.tone, 780.0f, 44);
  }

  if (b_pressed && human.trail_len > 0) {
    human.retreat = 1;
    trigger_tone(game.tone, 520.0f, 34);
  }

  if (human.retreat && human.trail_len > 0) {
    const Dir retreat_dir = bfs_direction_to_owned_land(game, 0, human.pos);
    if (retreat_dir != Dir::None && retreat_dir != opposite(human.dir)) {
      human.queued_dir = retreat_dir;
    }
  } else if (human.trail_len == 0) {
    human.retreat = 0;
  }
}

void update_player(GameState& game, int index, float dt) {
  Player& player = game.players[static_cast<std::size_t>(index)];

  if (!player.alive) {
    if (!player.human) return;
    if (game.human_lives <= 0) return;
    player.respawn_timer = std::max(0.0f, player.respawn_timer - dt);
    if (player.respawn_timer <= 0.0f) respawn_player(game, index);
    return;
  }

  if (player.boost_timer > 0.0f) player.boost_timer = std::max(0.0f, player.boost_timer - dt);
  player.move_timer += dt;
  const float interval = move_interval_for(game, index);
  if (player.move_timer < interval) return;
  player.move_timer -= interval;

  if (player.queued_dir != Dir::None && player.queued_dir != opposite(player.dir)) {
    player.dir = player.queued_dir;
  }
  move_player(game, index, player.dir);
}

void update_match(GameState& game, float dt, const pp_input_state& input, bool a_pressed, bool b_pressed, bool start_pressed, bool select_pressed) {
  update_human(game, input, a_pressed, b_pressed, start_pressed, select_pressed);
  if (game.paused || game.screen != Screen::Playing) return;

  for (int index = 1; index < kTotalPlayers; ++index) step_ai(game, index, dt);
  for (int index = 0; index < kTotalPlayers; ++index) {
    update_player(game, index, dt);
    if (game.screen != Screen::Playing) break;
  }

  int leader = 0;
  for (int index = 1; index < kTotalPlayers; ++index) {
    if (game.players[static_cast<std::size_t>(index)].territory >
        game.players[static_cast<std::size_t>(leader)].territory) {
      leader = index;
    }
  }
  game.leader = leader;
  if (game.status_timer > 0.0f) game.status_timer = std::max(0.0f, game.status_timer - dt);
  update_camera(game, dt);
}

void handle_mode_select(GameState& game, const pp_input_state& input, const pp_input_state& previous) {
  const bool up = input.up && !previous.up;
  const bool down = input.down && !previous.down;
  const bool a = input.a && !previous.a;
  const bool start = input.start && !previous.start;
  const bool select = input.select && !previous.select;

  if (up) {
    game.difficulty_index = (game.difficulty_index + static_cast<int>(kDifficulties.size()) - 1) % static_cast<int>(kDifficulties.size());
    trigger_tone(game.tone, 430.0f, 26);
  }
  if (down) {
    game.difficulty_index = (game.difficulty_index + 1) % static_cast<int>(kDifficulties.size());
    trigger_tone(game.tone, 430.0f, 26);
  }
  if (a || start) {
    trigger_tone(game.tone, 760.0f, 48);
    begin_match(game);
  }
  if (select) pp_request_exit(&game.context);
}

void handle_game_over(GameState& game, const pp_input_state& input, const pp_input_state& previous) {
  const bool a = input.a && !previous.a;
  const bool b = input.b && !previous.b;
  const bool start = input.start && !previous.start;
  const bool select = input.select && !previous.select;

  if (a || start) {
    trigger_tone(game.tone, 720.0f, 48);
    begin_match(game);
  } else if (b) {
    game.screen = Screen::ModeSelect;
    game.paused = false;
    game.status = "CHOOSE A DIFFICULTY";
    game.status_timer = 0.0f;
    trigger_tone(game.tone, 420.0f, 42);
  } else if (select) {
    pp_request_exit(&game.context);
  }
}

void draw_gradient(SDL_Renderer* renderer) {
  for (int y = 0; y < kVirtualHeight; ++y) {
    const float t = static_cast<float>(y) / static_cast<float>(kVirtualHeight - 1);
    SDL_Color row{
        static_cast<Uint8>(kBgTop.r + (kBgBottom.r - kBgTop.r) * t),
        static_cast<Uint8>(kBgTop.g + (kBgBottom.g - kBgTop.g) * t),
        static_cast<Uint8>(kBgTop.b + (kBgBottom.b - kBgTop.b) * t), 255};
    SDL_SetRenderDrawColor(renderer, row.r, row.g, row.b, row.a);
    SDL_RenderDrawLine(renderer, 0, y, kVirtualWidth, y);
  }

  SDL_SetRenderDrawColor(renderer, 26, 40, 62, 88);
  for (int x = 0; x < kVirtualWidth; x += 18) SDL_RenderDrawLine(renderer, x, 0, x, kVirtualHeight);
}

void draw_board(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Rect board_rect{kBoardViewX, kBoardViewY, kBoardViewW, kBoardViewH};
  fill_rect(renderer, board_rect, kBoardBase);
  draw_rect(renderer, board_rect, kFrame);

  SDL_RenderSetClipRect(renderer, &board_rect);

  const int start_x = std::max(0, static_cast<int>(game.camera.x) / kCellSize - 1);
  const int start_y = std::max(0, static_cast<int>(game.camera.y) / kCellSize - 1);
  const int end_x = std::min(kGridWidth - 1, static_cast<int>(game.camera.x + kBoardViewW) / kCellSize + 2);
  const int end_y = std::min(kGridHeight - 1, static_cast<int>(game.camera.y + kBoardViewH) / kCellSize + 2);

  for (int y = start_y; y <= end_y; ++y) {
    for (int x = start_x; x <= end_x; ++x) {
      const int screen_x = kBoardViewX + x * kCellSize - static_cast<int>(game.camera.x);
      const int screen_y = kBoardViewY + y * kCellSize - static_cast<int>(game.camera.y);
      SDL_Rect rect{screen_x, screen_y, kCellSize - 1, kCellSize - 1};
      fill_rect(renderer, rect, kBoardGrid);

      const int land = game.land_owner[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
      const int trail = game.trail_owner[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];

      if (land >= 0) {
        const SDL_Color color = game.players[static_cast<std::size_t>(land)].color;
        fill_rect(renderer, rect, shade(color, -6));
        fill_rect(renderer, {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2}, shade(color, 18));
      }
      if (trail >= 0) {
        const SDL_Color color = game.players[static_cast<std::size_t>(trail)].color;
        fill_rect(renderer, rect, shade(color, 18));
        fill_rect(renderer, {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4}, shade(color, 58));
      }
    }
  }

  for (int index = 0; index < kTotalPlayers; ++index) {
    const Player& player = game.players[static_cast<std::size_t>(index)];
    if (!player.alive) continue;

    const Vec2f cell = render_cell_for(game, index);
    const float px = static_cast<float>(kBoardViewX) + cell.x * kCellSize - game.camera.x + kCellSize * 0.5f;
    const float py = static_cast<float>(kBoardViewY) + cell.y * kCellSize - game.camera.y + kCellSize * 0.5f;
    if (px < kBoardViewX - 10 || px > kBoardViewX + kBoardViewW + 10 ||
        py < kBoardViewY - 10 || py > kBoardViewY + kBoardViewH + 10) {
      continue;
    }

    fill_circle(renderer, static_cast<int>(px), static_cast<int>(py), 6, shade(player.color, -48));
    fill_circle(renderer, static_cast<int>(px), static_cast<int>(py), 4, player.color);
    fill_circle(renderer, static_cast<int>(px) - 2, static_cast<int>(py) - 2, 1, {255, 255, 255, 255});
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void draw_sidebar(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {kSidePanelX, kSidePanelY, kSidePanelW, kSidePanelH}, kPanel);
  draw_rect(renderer, {kSidePanelX, kSidePanelY, kSidePanelW, kSidePanelH}, kFrame);

  const DifficultyInfo& tuning = difficulty_info(game);
  draw_text(renderer, "MODE", kSidePanelX + 10, kSidePanelY + 12, 1, kMuted);
  draw_text(renderer, tuning.name, kSidePanelX + 10, kSidePanelY + 28, 2, kAccent);
  draw_text(renderer, tuning.label, kSidePanelX + 10, kSidePanelY + 48, 1, kText);
  draw_text(renderer, tuning.ai_note, kSidePanelX + 10, kSidePanelY + 64, 1, kWarn);

  draw_text(renderer, "LIVES", kSidePanelX + 10, kSidePanelY + 92, 1, kMuted);
  draw_text(renderer, std::to_string(game.human_lives), kSidePanelX + 10, kSidePanelY + 108, 3,
            game.human_lives > 1 ? kText : kDanger);

  const Player& human = game.players[0];
  draw_text(renderer, "AREA", kSidePanelX + 10, kSidePanelY + 144, 1, kMuted);
  draw_text(renderer, std::to_string(human.territory), kSidePanelX + 10, kSidePanelY + 160, 2, human.color);
  draw_text(renderer, "TRAIL", kSidePanelX + 10, kSidePanelY + 182, 1, kMuted);
  draw_text(renderer, std::to_string(human.trail_len), kSidePanelX + 10, kSidePanelY + 198, 2, kText);

  draw_text(renderer, "SCORES", kSidePanelX + 10, kSidePanelY + 230, 1, kText);
  std::vector<int> ranking(kTotalPlayers);
  for (int index = 0; index < kTotalPlayers; ++index) ranking[static_cast<std::size_t>(index)] = index;
  std::sort(ranking.begin(), ranking.end(), [&](int a, int b) {
    return game.players[static_cast<std::size_t>(a)].territory > game.players[static_cast<std::size_t>(b)].territory;
  });
  for (int row = 0; row < 6; ++row) {
    const int index = ranking[static_cast<std::size_t>(row)];
    const Player& player = game.players[static_cast<std::size_t>(index)];
    const int y = kSidePanelY + 248 + row * 18;
    draw_text(renderer, player.name, kSidePanelX + 10, y, 1, index == 0 ? human.color : player.color);
    draw_text_right(renderer, std::to_string(player.territory), kSidePanelX + kSidePanelW - 10, y, 1, kText);
  }

  draw_text(renderer, "TRAILS", kSidePanelX + 10, kSidePanelY + 372, 1, kMuted);
  draw_text(renderer, "ARE", kSidePanelX + 10, kSidePanelY + 388, 1, kMuted);
  draw_text(renderer, "LETHAL", kSidePanelX + 10, kSidePanelY + 404, 1, kWarn);
  draw_text(renderer, "D TURN", kSidePanelX + 10, kSidePanelY + 432, 1, kMuted);
  draw_text(renderer, "A BURST", kSidePanelX + 10, kSidePanelY + 448, 1, kMuted);
  draw_text(renderer, "B HOME", kSidePanelX + 10, kSidePanelY + 464, 1, kMuted);
  draw_text(renderer, "START", kSidePanelX + 10, kSidePanelY + 480, 1, kMuted);
  draw_text(renderer, "SELECT", kSidePanelX + 58, kSidePanelY + 480, 1, kMuted);
}

void draw_top_bar(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {10, 8, 492, 24}, kPanel);
  draw_rect(renderer, {10, 8, 492, 24}, kFrame);
  draw_text(renderer, "RIBBON RAID", 22, 12, 2, kText);
  draw_text(renderer, game.paused ? "PAUSED" : "SCROLLING ARENA", 184, 14, 1, game.paused ? kWarn : kAccent);
  draw_text_right(renderer, "START PAUSE", 490, 14, 1, kMuted);

  if (game.status_timer > 0.0f) {
    fill_rect(renderer, {110, 36, 174, 22}, {8, 14, 24, 220});
    draw_rect(renderer, {110, 36, 174, 22}, kFrame);
    draw_text(renderer, game.status, 197, 43, 1, kWarn, true);
  }
}

void draw_play_scene(SDL_Renderer* renderer, const GameState& game) {
  draw_gradient(renderer);
  fill_rect(renderer, {kBoardViewX - 2, kBoardViewY - 2, kBoardViewW + 4, kBoardViewH + 4}, kPanelHi);
  draw_board(renderer, game);
  draw_top_bar(renderer, game);
  draw_sidebar(renderer, game);
}

void draw_pause_overlay(SDL_Renderer* renderer) {
  fill_rect(renderer, {130, 212, 252, 80}, {10, 18, 30, 228});
  draw_rect(renderer, {130, 212, 252, 80}, kFrame);
  draw_text(renderer, "PAUSED", 256, 230, 3, kWarn, true);
  draw_text(renderer, "START RESUMES", 256, 262, 1, kText, true);
}

void draw_mode_select(SDL_Renderer* renderer, const GameState& game) {
  draw_gradient(renderer);
  fill_rect(renderer, {34, 32, 444, 70}, kPanel);
  draw_rect(renderer, {34, 32, 444, 70}, kFrame);
  draw_text(renderer, "RIBBON RAID", 256, 46, 5, kText, true);
  draw_text(renderer, "BIGGER MAPS / SMARTER BOTS / LIVES", 256, 82, 1, kMuted, true);

  for (int index = 0; index < static_cast<int>(kDifficulties.size()); ++index) {
    const DifficultyInfo& entry = kDifficulties[static_cast<std::size_t>(index)];
    const bool selected = index == game.difficulty_index;
    const SDL_Rect card{70, 140 + index * 104, 372, 84};
    fill_rect(renderer, card, selected ? kPanelHi : kPanel);
    draw_rect(renderer, card, selected ? kAccent : kFrame);
    draw_text(renderer, entry.name, card.x + 18, card.y + 16, 3, selected ? kAccent : kText);
    draw_text(renderer, entry.label, card.x + 18, card.y + 46, 1, kText);
    draw_text(renderer, entry.ai_note, card.x + 156, card.y + 46, 1, selected ? kWarn : kMuted);
    draw_text(renderer, selected ? "READY" : "D PAD", card.x + 296, card.y + 20, 2, selected ? kAccent : kMuted);
  }

  fill_rect(renderer, {70, 460, 372, 28}, kPanel);
  draw_rect(renderer, {70, 460, 372, 28}, kFrame);
  draw_text(renderer, "UP / DOWN SELECT   A OR START BEGIN   SELECT EXIT", 256, 468, 1, kText, true);
}

void draw_game_over(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {110, 144, 292, 194}, {10, 18, 30, 236});
  draw_rect(renderer, {110, 144, 292, 194}, kFrame);
  draw_text(renderer, game.victory ? "ARENA CLEARED" : "OUT OF LIVES", 256, 166, 3, game.victory ? kAccent : kDanger, true);
  draw_text(renderer, game.victory ? "CHAMPION" : "LEADER", 256, 204, 1, kMuted, true);
  draw_text(renderer, game.players[static_cast<std::size_t>(game.leader)].name, 256, 220, 2,
            game.players[static_cast<std::size_t>(game.leader)].color, true);
  draw_text(renderer, game.victory ? "YOU CLEARED THE RAID" : "YOUR AREA " + std::to_string(game.players[0].territory), 256,
            256, 1, kText, true);
  draw_text(renderer, "A OR START REPLAY", 256, 288, 1, kAccent, true);
  draw_text(renderer, "B DIFFICULTY MENU", 256, 304, 1, kText, true);
  draw_text(renderer, "SELECT EXIT", 256, 320, 1, kMuted, true);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    return 1;
  }

  GameState game{};
  game.rng.seed(static_cast<unsigned>(SDL_GetTicks()) ^ 0x52494242u);
  if (pp_init(&game.context, "ribbon-raid") != 0) {
    SDL_Quit();
    return 1;
  }

  int width = kVirtualWidth;
  int height = kVirtualHeight;
  pp_get_framebuffer_size(&game.context, &width, &height);
  width = std::max(width, kVirtualWidth);
  height = std::max(height, kVirtualHeight);

  SDL_Window* window = SDL_CreateWindow("Ribbon Raid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
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

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_RenderSetLogicalSize(renderer, kVirtualWidth, kVirtualHeight);

  pp_audio_spec audio_spec{};
  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &game.tone;
  SDL_AudioDeviceID audio_device = 0;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }

  game.last_ticks = SDL_GetTicks();
  reset_match(game);
  game.camera = camera_target_for(game);

  pp_input_state input{};
  pp_input_state previous{};
  while (!pp_should_exit(&game.context)) {
    const Uint32 now = SDL_GetTicks();
    float dt = static_cast<float>(now - game.last_ticks) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    game.last_ticks = now;

    pp_poll_input(&game.context, &input);
    const bool a_pressed = input.a && !previous.a;
    const bool b_pressed = input.b && !previous.b;
    const bool start_pressed = input.start && !previous.start;
    const bool select_pressed = input.select && !previous.select;

    if (game.screen == Screen::ModeSelect) {
      handle_mode_select(game, input, previous);
    } else if (game.screen == Screen::Playing) {
      update_match(game, dt, input, a_pressed, b_pressed, start_pressed, select_pressed);
    } else {
      handle_game_over(game, input, previous);
    }

    if (game.screen == Screen::ModeSelect) {
      draw_mode_select(renderer, game);
    } else {
      draw_play_scene(renderer, game);
      if (game.paused && game.screen == Screen::Playing) draw_pause_overlay(renderer);
      if (game.screen == Screen::GameOver) draw_game_over(renderer, game);
    }

    SDL_RenderPresent(renderer);
    previous = input;
    SDL_Delay(16);
  }

  if (audio_device != 0) SDL_CloseAudioDevice(audio_device);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&game.context);
  SDL_Quit();
  return 0;
}
