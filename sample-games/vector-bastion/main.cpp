#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kVirtualWidth = 512;
constexpr int kVirtualHeight = 512;
constexpr float kWorldWidth = 2400.0f;
constexpr float kWorldHeight = 2400.0f;
constexpr int kMaxTanks = 12;
constexpr int kShardCount = 38;

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct Bullet {
  Vec2 pos{};
  Vec2 vel{};
  float damage = 8.0f;
  float radius = 4.0f;
  int owner = -1;
  bool active = false;
};

struct Shard {
  Vec2 pos{};
  Vec2 drift{};
  float hp = 16.0f;
  float radius = 12.0f;
  int sides = 4;
  SDL_Color color{255, 255, 255, 255};
  bool active = false;
};

struct Tank {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  Vec2 pos{};
  Vec2 vel{};
  Vec2 aim{1.0f, 0.0f};
  float hp = 46.0f;
  float max_hp = 46.0f;
  float reload = 0.0f;
  float reload_max = 0.34f;
  float bullet_speed = 330.0f;
  float bullet_damage = 8.0f;
  float speed = 122.0f;
  float regen = 2.6f;
  float radius = 18.0f;
  float xp = 0.0f;
  float next_level = 36.0f;
  float think_timer = 0.0f;
  float respawn = 0.0f;
  float bot_skill = 1.0f;
  int level = 1;
  int score = 0;
  int upgrade_points = 0;
  int cannon_upgrades = 0;
  int reload_upgrades = 0;
  int thruster_upgrades = 0;
  int armor_upgrades = 0;
  bool human = false;
  bool alive = true;
};

struct Camera {
  Vec2 center{};
  float zoom = 1.0f;
};

enum class Difficulty { Easy = 0, Medium, Hard };
enum class Upgrade { Cannon = 0, Reload, Thrusters, Armor };
enum class Screen { Title, Upgrade, Battle };

struct GameState {
  pp_context context{};
  ToneState tone{};
  std::mt19937 rng{0x56454354u};
  std::array<Tank, kMaxTanks> tanks{};
  std::array<Bullet, 192> bullets{};
  std::array<Shard, kShardCount> shards{};
  Camera camera{};
  Screen screen = Screen::Title;
  Difficulty difficulty = Difficulty::Medium;
  int menu_index = 1;
  int upgrade_index = 0;
  int total_tanks = 10;
  std::string status = "VECTOR BASTION";
  float status_timer = 0.0f;
  Uint32 last_ticks = 0U;
};

static const SDL_Color kBg = {10, 16, 25, 255};
static const SDL_Color kGrid = {23, 39, 56, 255};
static const SDL_Color kFrame = {73, 112, 163, 255};
static const SDL_Color kPanel = {19, 28, 40, 228};
static const SDL_Color kPanelHi = {30, 43, 62, 255};
static const SDL_Color kText = {236, 242, 250, 255};
static const SDL_Color kMuted = {135, 158, 186, 255};
static const SDL_Color kAccent = {255, 201, 88, 255};
static const std::array<SDL_Color, 6> kPalette{{
    {98, 188, 255, 255},
    {255, 137, 112, 255},
    {154, 242, 141, 255},
    {200, 148, 255, 255},
    {255, 210, 94, 255},
    {98, 232, 210, 255},
}};
static const std::array<const char*, 4> kUpgradeNames{{"CANNON", "RELOAD", "THRUSTERS", "ARMOR"}};

float clampf(float value, float low, float high) {
  return std::max(low, std::min(high, value));
}

Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 mul(Vec2 a, float scalar) { return {a.x * scalar, a.y * scalar}; }
Vec2 perp(Vec2 v) { return {-v.y, v.x}; }
float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
float length_sq(Vec2 v) { return dot(v, v); }
float length(Vec2 v) { return std::sqrt(length_sq(v)); }

Vec2 normalize(Vec2 v) {
  const float len = length(v);
  if (len < 0.001f) return {1.0f, 0.0f};
  return {v.x / len, v.y / len};
}

Vec2 random_dir(std::mt19937& rng) {
  std::uniform_real_distribution<float> angle_dist(0.0f, 6.2831853f);
  const float angle = angle_dist(rng);
  return {std::cos(angle), std::sin(angle)};
}

float random_range(std::mt19937& rng, float low, float high) {
  std::uniform_real_distribution<float> dist(low, high);
  return dist(rng);
}

int random_int(std::mt19937& rng, int low, int high) {
  std::uniform_int_distribution<int> dist(low, high);
  return dist(rng);
}

SDL_Color shade(SDL_Color color, int delta) {
  auto channel = [&](int value) { return static_cast<Uint8>(clampf(static_cast<float>(value + delta), 0.0f, 255.0f)); };
  return {channel(color.r), channel(color.g), channel(color.b), color.a};
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
      if (x * x + y * y <= radius * radius) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
      }
    }
  }
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
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1': return {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F};
    case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3': return {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E};
    case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6': return {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
    case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    default: return {0, 0, 0, 0, 0, 0, 0};
  }
}

void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, int scale, SDL_Color color, bool centered = false) {
  const int glyph_w = 5 * scale;
  const int spacing = scale;
  int draw_x = x;
  if (centered) draw_x -= static_cast<int>(text.size()) * (glyph_w + spacing) / 2;
  for (char raw : text) {
    char ch = raw >= 'a' && raw <= 'z' ? static_cast<char>(raw - 'a' + 'A') : raw;
    if (ch == ' ') {
      draw_x += glyph_w + spacing;
      continue;
    }
    const auto glyph = glyph_for(ch);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((glyph[static_cast<std::size_t>(row)] >> (4 - col)) & 1U) {
          fill_rect(renderer, {draw_x + col * scale, y + row * scale, scale, scale}, color);
        }
      }
    }
    draw_x += glyph_w + spacing;
  }
}

void audio_callback(void* userdata, Uint8* stream, int length) {
  auto* tone = static_cast<ToneState*>(userdata);
  auto* samples = reinterpret_cast<int16_t*>(stream);
  const int count = length / static_cast<int>(sizeof(int16_t));
  for (int i = 0; i < count; ++i) {
    int16_t sample = 0;
    if (tone->frames_remaining > 0) {
      sample = tone->phase < 3.14159f ? 1600 : -1600;
      tone->phase += (6.28318f * tone->frequency) / 48000.0f;
      if (tone->phase >= 6.28318f) tone->phase -= 6.28318f;
      --tone->frames_remaining;
    }
    samples[i] = sample;
  }
}

void trigger_tone(ToneState& tone, float frequency, int ms) {
  tone.frequency = frequency;
  tone.frames_remaining = (48000 * ms) / 1000;
}

SDL_Point world_to_screen(Vec2 world, const Camera& camera) {
  return {static_cast<int>(std::lround((world.x - camera.center.x) * camera.zoom + kVirtualWidth * 0.5f)),
          static_cast<int>(std::lround((world.y - camera.center.y) * camera.zoom + kVirtualHeight * 0.5f))};
}

void refill_shard(GameState& game, Shard& shard) {
  shard.active = true;
  shard.pos = {random_range(game.rng, 120.0f, kWorldWidth - 120.0f), random_range(game.rng, 120.0f, kWorldHeight - 120.0f)};
  shard.drift = mul(random_dir(game.rng), random_range(game.rng, 6.0f, 16.0f));
  shard.sides = random_int(game.rng, 0, 2) == 0 ? 3 : 4;
  if (shard.sides == 3) {
    shard.radius = 11.0f;
    shard.hp = 14.0f;
    shard.color = {255, 152, 112, 255};
  } else {
    shard.radius = 13.0f;
    shard.hp = 19.0f;
    shard.color = {108, 198, 255, 255};
  }
}

void reset_tank(GameState& game, Tank& tank, int index) {
  tank.pos = {random_range(game.rng, 180.0f, kWorldWidth - 180.0f), random_range(game.rng, 180.0f, kWorldHeight - 180.0f)};
  tank.vel = {};
  tank.aim = random_dir(game.rng);
  tank.hp = tank.max_hp;
  tank.reload = 0.0f;
  tank.alive = true;
  tank.respawn = 0.0f;
  if (index == 0) tank.pos = {kWorldWidth * 0.42f, kWorldHeight * 0.5f};
}

void grant_xp(Tank& tank, float amount) {
  tank.xp += amount;
  while (tank.xp >= tank.next_level) {
    tank.xp -= tank.next_level;
    tank.next_level *= 1.2f;
    ++tank.level;
    ++tank.upgrade_points;
    tank.max_hp += 4.0f;
    tank.hp = std::min(tank.max_hp, tank.hp + 6.0f);
  }
}

void apply_upgrade(Tank& tank, Upgrade upgrade) {
  if (tank.upgrade_points <= 0) return;
  --tank.upgrade_points;
  switch (upgrade) {
    case Upgrade::Cannon:
      ++tank.cannon_upgrades;
      tank.bullet_damage += 2.0f;
      tank.bullet_speed += 18.0f;
      break;
    case Upgrade::Reload:
      ++tank.reload_upgrades;
      tank.reload_max = std::max(0.11f, tank.reload_max - 0.035f);
      break;
    case Upgrade::Thrusters:
      ++tank.thruster_upgrades;
      tank.speed += 12.0f;
      break;
    case Upgrade::Armor:
      ++tank.armor_upgrades;
      tank.max_hp += 10.0f;
      tank.hp += 10.0f;
      tank.regen += 0.45f;
      break;
  }
}

int free_bullet_slot(const GameState& game) {
  for (int i = 0; i < static_cast<int>(game.bullets.size()); ++i) {
    if (!game.bullets[static_cast<std::size_t>(i)].active) return i;
  }
  return -1;
}

void fire_bullet(GameState& game, int owner_index) {
  Tank& tank = game.tanks[static_cast<std::size_t>(owner_index)];
  if (!tank.alive || tank.reload > 0.0f) return;
  const int slot = free_bullet_slot(game);
  if (slot < 0) return;
  Bullet& bullet = game.bullets[static_cast<std::size_t>(slot)];
  bullet.active = true;
  bullet.owner = owner_index;
  bullet.damage = tank.bullet_damage;
  bullet.radius = tank.level >= 8 ? 5.0f : 4.0f;
  bullet.pos = add(tank.pos, mul(normalize(tank.aim), tank.radius + 8.0f));
  bullet.vel = mul(normalize(tank.aim), tank.bullet_speed);
  tank.reload = tank.reload_max;
  trigger_tone(game.tone, tank.human ? 820.0f : 620.0f, 24);
}

void begin_match(GameState& game) {
  game.total_tanks = game.difficulty == Difficulty::Easy ? 8 : (game.difficulty == Difficulty::Medium ? 10 : 12);
  for (auto& bullet : game.bullets) bullet = {};
  for (auto& shard : game.shards) refill_shard(game, shard);
  for (int i = 0; i < kMaxTanks; ++i) {
    Tank& tank = game.tanks[static_cast<std::size_t>(i)];
    tank = {};
    tank.name = i == 0 ? "P1" : ("BOT " + std::to_string(i));
    tank.human = i == 0;
    tank.color = kPalette[static_cast<std::size_t>(i % static_cast<int>(kPalette.size()))];
    tank.max_hp = 46.0f;
    tank.hp = tank.max_hp;
    tank.reload_max = 0.34f;
    tank.bullet_speed = 330.0f;
    tank.bullet_damage = 8.0f;
    tank.speed = 122.0f;
    tank.regen = 2.6f;
    tank.radius = 18.0f;
    tank.level = 1;
    tank.next_level = 36.0f;
    tank.bot_skill = game.difficulty == Difficulty::Easy ? 0.72f : (game.difficulty == Difficulty::Medium ? 1.0f : 1.28f);
    reset_tank(game, tank, i);
    if (i >= game.total_tanks) tank.alive = false;
  }
  game.camera.center = game.tanks[0].pos;
  game.camera.zoom = 0.78f;
  game.screen = Screen::Battle;
  game.status = "AUTO-AIM TANK SKIRMISH";
  game.status_timer = 2.8f;
}

void update_ai(GameState& game, Tank& tank, int index, float dt) {
  if (tank.human || !tank.alive) return;
  tank.think_timer -= dt;
  if (tank.think_timer > 0.0f) return;
  tank.think_timer = 0.14f / tank.bot_skill;
  int target_tank = -1;
  float best_enemy = 1e9f;
  for (int i = 0; i < game.total_tanks; ++i) {
    if (i == index) continue;
    const Tank& other = game.tanks[static_cast<std::size_t>(i)];
    if (!other.alive) continue;
    const float dist = length_sq(sub(other.pos, tank.pos));
    if (other.level <= tank.level + 1 && dist < best_enemy) {
      best_enemy = dist;
      target_tank = i;
    }
  }
  int target_shard = -1;
  float best_shard = 1e9f;
  for (int i = 0; i < kShardCount; ++i) {
    const Shard& shard = game.shards[static_cast<std::size_t>(i)];
    if (!shard.active) continue;
    const float dist = length_sq(sub(shard.pos, tank.pos));
    if (dist < best_shard) {
      best_shard = dist;
      target_shard = i;
    }
  }
  Vec2 goal{};
  if (target_tank >= 0 && (best_enemy < 220000.0f || target_shard < 0)) {
    const Tank& enemy = game.tanks[static_cast<std::size_t>(target_tank)];
    goal = sub(enemy.pos, tank.pos);
    if (enemy.level > tank.level + 2 && best_enemy < 180000.0f) goal = mul(goal, -1.0f);
    tank.aim = normalize(goal);
    if (best_enemy < 180000.0f) fire_bullet(game, index);
  } else if (target_shard >= 0) {
    goal = sub(game.shards[static_cast<std::size_t>(target_shard)].pos, tank.pos);
    tank.aim = normalize(goal);
    if (best_shard < 140000.0f) fire_bullet(game, index);
  }
  tank.vel = add(tank.vel, mul(normalize(goal), tank.speed * 0.18f));
  while (tank.upgrade_points > 0) apply_upgrade(tank, static_cast<Upgrade>(random_int(game.rng, 0, 3)));
}

void update_camera(GameState& game, float dt) {
  const Tank& player = game.tanks[0];
  game.camera.center.x += (player.pos.x - game.camera.center.x) * (1.0f - std::pow(0.001f, dt));
  game.camera.center.y += (player.pos.y - game.camera.center.y) * (1.0f - std::pow(0.001f, dt));
  const float target_zoom = clampf(1.02f - static_cast<float>(player.level) * 0.03f, 0.46f, 0.94f);
  game.camera.zoom += (target_zoom - game.camera.zoom) * (1.0f - std::pow(0.008f, dt));
}

void update_world(GameState& game, float dt, const pp_input_state& input) {
  Tank& player = game.tanks[0];
  if (game.screen == Screen::Upgrade && player.upgrade_points > 0) return;
  Vec2 move{};
  if (input.left) move.x -= 1.0f;
  if (input.right) move.x += 1.0f;
  if (input.up) move.y -= 1.0f;
  if (input.down) move.y += 1.0f;
  if (length_sq(move) > 0.1f) {
    player.aim = normalize(move);
    player.vel = add(player.vel, mul(normalize(move), player.speed * 0.2f));
  } else {
    int nearest = -1;
    float best = 1e9f;
    for (int i = 1; i < game.total_tanks; ++i) {
      const Tank& other = game.tanks[static_cast<std::size_t>(i)];
      if (!other.alive) continue;
      const float dist = length_sq(sub(other.pos, player.pos));
      if (dist < best) {
        best = dist;
        nearest = i;
      }
    }
    if (nearest >= 0) player.aim = normalize(sub(game.tanks[static_cast<std::size_t>(nearest)].pos, player.pos));
  }
  fire_bullet(game, 0);
  for (int i = 1; i < game.total_tanks; ++i) update_ai(game, game.tanks[static_cast<std::size_t>(i)], i, dt);
  for (int i = 0; i < game.total_tanks; ++i) {
    Tank& tank = game.tanks[static_cast<std::size_t>(i)];
    if (!tank.alive) {
      tank.respawn -= dt;
      if (tank.respawn <= 0.0f) reset_tank(game, tank, i);
      continue;
    }
    tank.reload = std::max(0.0f, tank.reload - dt);
    tank.hp = std::min(tank.max_hp, tank.hp + tank.regen * dt);
    tank.vel = mul(tank.vel, std::pow(0.12f, dt));
    tank.pos = add(tank.pos, mul(tank.vel, dt));
    tank.pos.x = clampf(tank.pos.x, tank.radius, kWorldWidth - tank.radius);
    tank.pos.y = clampf(tank.pos.y, tank.radius, kWorldHeight - tank.radius);
  }
  for (Shard& shard : game.shards) {
    if (!shard.active) continue;
    shard.pos = add(shard.pos, mul(shard.drift, dt));
    if (shard.pos.x < 40.0f || shard.pos.x > kWorldWidth - 40.0f) shard.drift.x = -shard.drift.x;
    if (shard.pos.y < 40.0f || shard.pos.y > kWorldHeight - 40.0f) shard.drift.y = -shard.drift.y;
  }
  for (Bullet& bullet : game.bullets) {
    if (!bullet.active) continue;
    bullet.pos = add(bullet.pos, mul(bullet.vel, dt));
    if (bullet.pos.x < -40.0f || bullet.pos.y < -40.0f || bullet.pos.x > kWorldWidth + 40.0f || bullet.pos.y > kWorldHeight + 40.0f) {
      bullet.active = false;
      continue;
    }
    for (int i = 0; i < kShardCount && bullet.active; ++i) {
      Shard& shard = game.shards[static_cast<std::size_t>(i)];
      if (!shard.active) continue;
      if (length_sq(sub(bullet.pos, shard.pos)) <= (shard.radius + bullet.radius) * (shard.radius + bullet.radius)) {
        shard.hp -= bullet.damage;
        bullet.active = false;
        if (shard.hp <= 0.0f) {
          grant_xp(game.tanks[static_cast<std::size_t>(bullet.owner)], shard.sides == 3 ? 14.0f : 20.0f);
          ++game.tanks[static_cast<std::size_t>(bullet.owner)].score;
          refill_shard(game, shard);
        }
      }
    }
    for (int i = 0; i < game.total_tanks && bullet.active; ++i) {
      if (i == bullet.owner) continue;
      Tank& tank = game.tanks[static_cast<std::size_t>(i)];
      if (!tank.alive) continue;
      if (length_sq(sub(bullet.pos, tank.pos)) <= (tank.radius + bullet.radius) * (tank.radius + bullet.radius)) {
        tank.hp -= bullet.damage;
        bullet.active = false;
        if (tank.hp <= 0.0f) {
          tank.alive = false;
          tank.respawn = 2.4f;
          grant_xp(game.tanks[static_cast<std::size_t>(bullet.owner)], 36.0f + tank.level * 4.0f);
          game.tanks[static_cast<std::size_t>(bullet.owner)].score += 3;
          if (i == 0) {
            game.status = "RESPAWNING";
            game.status_timer = 2.0f;
          }
        }
      }
    }
  }
  if (player.upgrade_points > 0 && game.screen == Screen::Battle) {
    game.upgrade_index = 0;
    game.screen = Screen::Upgrade;
  }
  if (game.status_timer > 0.0f) game.status_timer = std::max(0.0f, game.status_timer - dt);
  update_camera(game, dt);
}

void draw_bar(SDL_Renderer* renderer, int x, int y, int w, float t, SDL_Color color) {
  fill_rect(renderer, {x, y, w, 6}, {19, 26, 36, 255});
  fill_rect(renderer, {x, y, static_cast<int>(std::lround(w * clampf(t, 0.0f, 1.0f))), 6}, color);
  draw_rect(renderer, {x, y, w, 6}, {55, 73, 97, 255});
}

void draw_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void draw_circle_outline(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  int x = radius;
  int y = 0;
  int err = 1 - radius;
  while (x >= y) {
    SDL_RenderDrawPoint(renderer, cx + x, cy + y);
    SDL_RenderDrawPoint(renderer, cx + y, cy + x);
    SDL_RenderDrawPoint(renderer, cx - y, cy + x);
    SDL_RenderDrawPoint(renderer, cx - x, cy + y);
    SDL_RenderDrawPoint(renderer, cx - x, cy - y);
    SDL_RenderDrawPoint(renderer, cx - y, cy - x);
    SDL_RenderDrawPoint(renderer, cx + y, cy - x);
    SDL_RenderDrawPoint(renderer, cx + x, cy - y);
    ++y;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      --x;
      err += 2 * (y - x) + 1;
    }
  }
}

void draw_tank_sprite(SDL_Renderer* renderer, const Tank& tank, const Camera& camera, bool highlight) {
  const SDL_Point p = world_to_screen(tank.pos, camera);
  const int radius = std::max(8, static_cast<int>(std::lround(tank.radius * camera.zoom)));
  const Vec2 aim = normalize(tank.aim);
  const Vec2 side = perp(aim);
  const int barrel_count = 1 + (tank.reload_upgrades >= 2 ? 1 : 0) + (tank.reload_upgrades >= 5 ? 1 : 0);
  const float barrel_spread = barrel_count <= 1 ? 0.0f : std::max(4.0f, camera.zoom * 8.0f);
  const int barrel_length = radius + 12 + tank.cannon_upgrades * 3;
  const int barrel_width = std::max(2, 3 + tank.cannon_upgrades / 2);
  const SDL_Color shell = tank.color;
  const SDL_Color rim = shade(tank.color, -58);
  const SDL_Color core = shade(tank.color, 28);
  const SDL_Color trim = shade(tank.color, 72);

  if (tank.thruster_upgrades > 0) {
    const int thruster_count = 1 + (tank.thruster_upgrades >= 2 ? 1 : 0) + (tank.thruster_upgrades >= 5 ? 1 : 0);
    const float thruster_spread = thruster_count <= 1 ? 0.0f : std::max(4.0f, camera.zoom * 9.0f);
    for (int i = 0; i < thruster_count; ++i) {
      const float lane = static_cast<float>(i - (thruster_count - 1) * 0.5f) * thruster_spread;
      const SDL_Point exhaust{
          static_cast<int>(std::lround(p.x - aim.x * (radius + 6) + side.x * lane)),
          static_cast<int>(std::lround(p.y - aim.y * (radius + 6) + side.y * lane))};
      fill_circle(renderer, exhaust.x, exhaust.y, std::max(2, radius / 4), {255, 176, 86, 230});
      fill_circle(renderer,
                  static_cast<int>(std::lround(exhaust.x - aim.x * (4 + tank.thruster_upgrades))),
                  static_cast<int>(std::lround(exhaust.y - aim.y * (4 + tank.thruster_upgrades))),
                  std::max(2, radius / 5), {255, 232, 164, 210});
    }
  }

  for (int i = 0; i < barrel_count; ++i) {
    const float lane = static_cast<float>(i - (barrel_count - 1) * 0.5f) * barrel_spread;
    const SDL_Point start{
        static_cast<int>(std::lround(p.x + aim.x * (radius * 0.35f) + side.x * lane)),
        static_cast<int>(std::lround(p.y + aim.y * (radius * 0.35f) + side.y * lane))};
    const SDL_Point tip{
        static_cast<int>(std::lround(p.x + aim.x * barrel_length + side.x * lane)),
        static_cast<int>(std::lround(p.y + aim.y * barrel_length + side.y * lane))};
    for (int o = -barrel_width; o <= barrel_width; ++o) {
      draw_line(renderer,
                static_cast<int>(std::lround(start.x + side.x * o)),
                static_cast<int>(std::lround(start.y + side.y * o)),
                static_cast<int>(std::lround(tip.x + side.x * o)),
                static_cast<int>(std::lround(tip.y + side.y * o)),
                i == 0 ? shell : trim);
    }
  }

  if (tank.armor_upgrades > 0) {
    for (int ring = 0; ring < std::min(3, tank.armor_upgrades); ++ring) {
      draw_circle_outline(renderer, p.x, p.y, radius + 3 + ring * 3, shade(tank.color, 28 - ring * 18));
    }
  }

  fill_circle(renderer, p.x, p.y, radius, shell);
  fill_circle(renderer, p.x, p.y, std::max(4, radius - 3), rim);
  fill_circle(renderer, p.x, p.y, std::max(3, radius - 8), core);
  fill_circle(renderer,
              static_cast<int>(std::lround(p.x - aim.x * radius * 0.18f)),
              static_cast<int>(std::lround(p.y - aim.y * radius * 0.18f)),
              std::max(2, radius / 5), trim);

  if (highlight) {
    draw_rect(renderer, {p.x - radius - 3, p.y - radius - 3, radius * 2 + 6, radius * 2 + 6}, kAccent);
  }
}

void draw_world(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {0, 0, kVirtualWidth, kVirtualHeight}, kBg);
  const int grid_step = std::max(16, static_cast<int>(std::lround(120.0f * game.camera.zoom)));
  const int ox = static_cast<int>(std::fmod(-game.camera.center.x * game.camera.zoom + kVirtualWidth * 0.5f, static_cast<float>(grid_step)));
  const int oy = static_cast<int>(std::fmod(-game.camera.center.y * game.camera.zoom + kVirtualHeight * 0.5f, static_cast<float>(grid_step)));
  SDL_SetRenderDrawColor(renderer, kGrid.r, kGrid.g, kGrid.b, kGrid.a);
  for (int x = ox; x < kVirtualWidth; x += grid_step) SDL_RenderDrawLine(renderer, x, 0, x, kVirtualHeight);
  for (int y = oy; y < kVirtualHeight; y += grid_step) SDL_RenderDrawLine(renderer, 0, y, kVirtualWidth, y);
  for (const Shard& shard : game.shards) {
    if (!shard.active) continue;
    const SDL_Point p = world_to_screen(shard.pos, game.camera);
    fill_circle(renderer, p.x, p.y, std::max(4, static_cast<int>(std::lround(shard.radius * game.camera.zoom))), shard.color);
    fill_circle(renderer, p.x, p.y, std::max(2, static_cast<int>(std::lround((shard.radius - 4.0f) * game.camera.zoom))), kBg);
  }
  for (const Bullet& bullet : game.bullets) {
    if (!bullet.active) continue;
    const SDL_Point p = world_to_screen(bullet.pos, game.camera);
    fill_circle(renderer, p.x, p.y, std::max(2, static_cast<int>(std::lround(bullet.radius * game.camera.zoom))), {255, 239, 168, 255});
  }
  for (int i = 0; i < game.total_tanks; ++i) {
    const Tank& tank = game.tanks[static_cast<std::size_t>(i)];
    if (!tank.alive) continue;
    draw_tank_sprite(renderer, tank, game.camera, tank.human);
  }
}

void draw_hud(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {10, 10, 188, 98}, kPanel);
  draw_rect(renderer, {10, 10, 188, 98}, kFrame);
  const Tank& player = game.tanks[0];
  draw_text(renderer, "VECTOR BASTION", 20, 18, 2, kText);
  draw_text(renderer, "LEVEL " + std::to_string(player.level), 20, 42, 1, kAccent);
  draw_text(renderer, "SCORE " + std::to_string(player.score), 104, 42, 1, kText);
  draw_bar(renderer, 20, 58, 160, player.hp / std::max(1.0f, player.max_hp), {86, 216, 124, 255});
  draw_bar(renderer, 20, 72, 160, player.xp / std::max(1.0f, player.next_level), {104, 190, 255, 255});
  if (game.status_timer > 0.0f) draw_text(renderer, game.status, kVirtualWidth / 2, 18, 1, kAccent, true);

  fill_rect(renderer, {326, 10, 176, 112}, kPanel);
  draw_rect(renderer, {326, 10, 176, 112}, kFrame);
  draw_text(renderer, "RANK", 338, 18, 1, kMuted);
  std::vector<int> order(game.total_tanks);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return game.tanks[static_cast<std::size_t>(a)].score > game.tanks[static_cast<std::size_t>(b)].score;
  });
  for (int i = 0; i < std::min(6, game.total_tanks); ++i) {
    const Tank& tank = game.tanks[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])];
    draw_text(renderer, tank.name, 338, 34 + i * 14, 1, tank.color);
    draw_text(renderer, std::to_string(tank.score), 476, 34 + i * 14, 1, kText, true);
  }
}

void draw_title(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {0, 0, kVirtualWidth, kVirtualHeight}, kBg);
  for (int stripe = 0; stripe < 8; ++stripe) {
    fill_rect(renderer, {0, 30 + stripe * 48, kVirtualWidth, 22}, stripe % 2 == 0 ? SDL_Color{18, 28, 41, 255} : SDL_Color{14, 22, 32, 255});
  }
  draw_text(renderer, "VECTOR BASTION", kVirtualWidth / 2, 72, 4, kText, true);
  draw_text(renderer, "AUTO-AIM TANK ARENA", kVirtualWidth / 2, 126, 1, kMuted, true);
  const std::array<const char*, 3> options{{"EASY", "MEDIUM", "HARD"}};
  for (int i = 0; i < 3; ++i) {
    const SDL_Rect row{156, 182 + i * 56, 200, 40};
    fill_rect(renderer, row, i == game.menu_index ? kPanelHi : kPanel);
    draw_rect(renderer, row, i == game.menu_index ? kAccent : kFrame);
    draw_text(renderer, options[static_cast<std::size_t>(i)], row.x + row.w / 2, row.y + 12, 2,
              i == game.menu_index ? kAccent : kText, true);
  }
  draw_text(renderer, "D-PAD STEERS / AUTO FIRE TRACKS YOUR NEAREST THREAT", kVirtualWidth / 2, 380, 1, kMuted, true);
  draw_text(renderer, "A START  UP/DOWN DIFFICULTY", kVirtualWidth / 2, 404, 1, kAccent, true);
}

void draw_upgrade(SDL_Renderer* renderer, const GameState& game) {
  draw_world(renderer, game);
  draw_hud(renderer, game);
  fill_rect(renderer, {112, 118, 288, 180}, {12, 18, 26, 236});
  draw_rect(renderer, {112, 118, 288, 180}, kFrame);
  draw_text(renderer, "LEVEL UP", kVirtualWidth / 2, 136, 3, kAccent, true);
  for (int i = 0; i < 4; ++i) {
    const SDL_Rect row{138, 168 + i * 26, 236, 22};
    fill_rect(renderer, row, i == game.upgrade_index ? kPanelHi : kPanel);
    draw_rect(renderer, row, i == game.upgrade_index ? kAccent : kFrame);
    draw_text(renderer, kUpgradeNames[static_cast<std::size_t>(i)], row.x + 8, row.y + 6, 1,
              i == game.upgrade_index ? kAccent : kText);
  }
  draw_text(renderer, "A APPLY", kVirtualWidth / 2, 276, 1, kMuted, true);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  GameState game;
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_AudioDeviceID audio = 0U;
  pp_audio_spec audio_spec{};
  pp_input_state input{};
  pp_input_state previous{};
  int width = 0;
  int height = 0;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
  if (pp_init(&game.context, "vector-bastion") != 0) {
    SDL_Quit();
    return 1;
  }
  pp_get_framebuffer_size(&game.context, &width, &height);
  width = std::max(width, 512);
  height = std::max(height, 512);
  window = SDL_CreateWindow("Vector Bastion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_RenderSetLogicalSize(renderer, kVirtualWidth, kVirtualHeight);
  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &game.tone;
  if (pp_audio_open(&audio_spec, &audio) == 0) SDL_PauseAudioDevice(audio, 0);
  game.camera.center = {kWorldWidth * 0.5f, kWorldHeight * 0.5f};
  game.camera.zoom = 0.78f;
  while (!pp_should_exit(&game.context)) {
    pp_poll_input(&game.context, &input);
    const Uint32 now = SDL_GetTicks();
    const float dt = game.last_ticks == 0U ? 1.0f / 60.0f : clampf((now - game.last_ticks) / 1000.0f, 1.0f / 240.0f, 1.0f / 20.0f);
    game.last_ticks = now;
    const bool up = input.up && !previous.up;
    const bool down = input.down && !previous.down;
    const bool a = input.a && !previous.a;
    if (game.screen == Screen::Title) {
      if (up) game.menu_index = (game.menu_index + 2) % 3;
      if (down) game.menu_index = (game.menu_index + 1) % 3;
      if (a) {
        game.difficulty = static_cast<Difficulty>(game.menu_index);
        begin_match(game);
      }
      draw_title(renderer, game);
    } else if (game.screen == Screen::Upgrade) {
      if (up) game.upgrade_index = (game.upgrade_index + 3) % 4;
      if (down) game.upgrade_index = (game.upgrade_index + 1) % 4;
      if (a) {
        apply_upgrade(game.tanks[0], static_cast<Upgrade>(game.upgrade_index));
        game.screen = game.tanks[0].upgrade_points > 0 ? Screen::Upgrade : Screen::Battle;
      }
      draw_upgrade(renderer, game);
    } else {
      update_world(game, dt, input);
      draw_world(renderer, game);
      draw_hud(renderer, game);
    }
    SDL_RenderPresent(renderer);
    previous = input;
  }
  if (audio != 0U) SDL_CloseAudioDevice(audio);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&game.context);
  SDL_Quit();
  return 0;
}
