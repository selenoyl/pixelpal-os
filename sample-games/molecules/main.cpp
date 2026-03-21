#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#if defined(_WIN32)
#define MOLECULES_PATH_SEPARATOR '\\'
#else
#define MOLECULES_PATH_SEPARATOR '/'
#endif

namespace {

constexpr int kVirtualWidth = 512;
constexpr int kVirtualHeight = 512;
constexpr float kWorldWidth = 2400.0f;
constexpr float kWorldHeight = 2400.0f;
constexpr int kTotalAgents = 24;
constexpr int kMaxCellsPerAgent = 8;
constexpr int kFoodCount = 260;
constexpr int kVirusCount = 24;
constexpr int kEjectCount = 96;
constexpr float kSpawnMass = 42.0f;
constexpr float kFoodMassMin = 2.0f;
constexpr float kFoodMassMax = 5.0f;
constexpr float kVirusRadius = 23.0f;
constexpr float kVirusBurstMass = 110.0f;
constexpr float kMergeDelay = 5.5f;
constexpr float kSplitCooldown = 1.0f;
constexpr float kEjectCooldown = 0.18f;
constexpr float kRespawnDelay = 2.4f;
constexpr float kBotThinkDelay = 0.12f;

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Cell {
  Vec2 pos{};
  Vec2 vel{};
  float mass = 0.0f;
  float merge_timer = 0.0f;
  float invulnerable = 0.0f;
  bool alive = false;
};

struct Agent {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  bool human = false;
  std::array<Cell, kMaxCellsPerAgent> cells{};
  Vec2 aim{1.0f, 0.0f};
  float throttle = 1.0f;
  float think_timer = 0.0f;
  float split_cooldown = 0.0f;
  float eject_cooldown = 0.0f;
  float respawn_timer = 0.0f;
  float wander_timer = 0.0f;
  Vec2 wander_dir{1.0f, 0.0f};
};

struct Food {
  Vec2 pos{};
  float mass = 0.0f;
  SDL_Color color{255, 255, 255, 255};
  bool active = false;
};

struct Virus {
  Vec2 pos{};
  float radius = kVirusRadius;
  bool active = false;
};

struct Ejecta {
  Vec2 pos{};
  Vec2 vel{};
  float mass = 8.0f;
  int owner = -1;
  float age = 0.0f;
  SDL_Color color{255, 255, 255, 255};
  bool active = false;
};

struct Camera {
  Vec2 center{};
  float zoom = 1.0f;
};

struct GameState {
  pp_context context{};
  std::mt19937 rng{0x4D4F4C45u};
  std::array<Agent, kTotalAgents> agents{};
  std::array<Food, kFoodCount> food{};
  std::array<Virus, kVirusCount> viruses{};
  std::array<Ejecta, kEjectCount> ejecta{};
  Camera camera{};
  ToneState tone{};
  bool paused = false;
  std::string status = "ABSORB THE PETRI DISH";
  float status_timer = 3.0f;
  int best_mass = 0;
  int stored_best_mass = 0;
  Uint32 last_ticks = 0;
};

static const SDL_Color kBg = {15, 20, 28, 255};
static const SDL_Color kGrid = {25, 34, 46, 255};
static const SDL_Color kWorldBorder = {62, 84, 112, 255};
static const SDL_Color kPanel = {23, 31, 43, 228};
static const SDL_Color kFrame = {78, 116, 155, 255};
static const SDL_Color kText = {230, 242, 255, 255};
static const SDL_Color kMuted = {126, 153, 179, 255};
static const SDL_Color kAccent = {117, 235, 202, 255};
static const SDL_Color kWarning = {255, 188, 98, 255};
static const SDL_Color kDanger = {255, 98, 110, 255};
static const std::array<SDL_Color, 8> kFoodColors{{
    {117, 235, 202, 255},
    {131, 190, 255, 255},
    {255, 188, 98, 255},
    {255, 110, 152, 255},
    {179, 142, 255, 255},
    {168, 255, 121, 255},
    {255, 231, 130, 255},
    {124, 222, 255, 255},
}};

float clampf(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 mul(Vec2 a, float scalar) { return {a.x * scalar, a.y * scalar}; }
float length_sq(Vec2 v) { return v.x * v.x + v.y * v.y; }
float length(Vec2 v) { return std::sqrt(length_sq(v)); }

Vec2 normalize(Vec2 v) {
  const float len = length(v);
  if (len < 0.0001f) return {0.0f, 0.0f};
  return {v.x / len, v.y / len};
}

float radius_for_mass(float mass) {
  return 4.0f + std::sqrt(std::max(1.0f, mass)) * 2.18f;
}

float speed_for_mass(float mass) {
  const float scaled_mass = std::pow(std::max(1.0f, mass), 0.58f);
  return clampf(320.0f / (3.2f + scaled_mass * 1.05f), 18.0f, 52.0f);
}

float total_mass(const Agent& agent) {
  float sum = 0.0f;
  for (const Cell& cell : agent.cells) {
    if (cell.alive) sum += cell.mass;
  }
  return sum;
}

int living_cells(const Agent& agent) {
  int count = 0;
  for (const Cell& cell : agent.cells) count += cell.alive ? 1 : 0;
  return count;
}

bool alive(const Agent& agent) {
  return living_cells(agent) > 0;
}

int largest_cell_index(const Agent& agent) {
  int best = -1;
  float best_mass = -1.0f;
  for (int index = 0; index < kMaxCellsPerAgent; ++index) {
    if (agent.cells[static_cast<std::size_t>(index)].alive && agent.cells[static_cast<std::size_t>(index)].mass > best_mass) {
      best_mass = agent.cells[static_cast<std::size_t>(index)].mass;
      best = index;
    }
  }
  return best;
}

Vec2 random_unit(std::mt19937& rng) {
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

int free_cell_index(const Agent& agent) {
  for (int index = 0; index < kMaxCellsPerAgent; ++index) {
    if (!agent.cells[static_cast<std::size_t>(index)].alive) {
      return index;
    }
  }
  return -1;
}

void fill_rect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
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

void draw_text(SDL_Renderer* renderer, const std::string& text_in, int x, int y, int scale, SDL_Color color, bool centered = false) {
  const std::string text = uppercase(text_in);
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
  int16_t* samples = reinterpret_cast<int16_t*>(stream);
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

void save_best_mass(const pp_context& context, int best_mass) {
  char path[PP_PATH_CAPACITY];
  std::snprintf(path, sizeof(path), "%s%cbest_mass.txt", pp_get_save_dir(&context), MOLECULES_PATH_SEPARATOR);
  if (FILE* file = std::fopen(path, "w")) {
    std::fprintf(file, "%d\n", best_mass);
    std::fclose(file);
  }
}

int load_best_mass(const pp_context& context) {
  char path[PP_PATH_CAPACITY];
  std::snprintf(path, sizeof(path), "%s%cbest_mass.txt", pp_get_save_dir(&context), MOLECULES_PATH_SEPARATOR);
  int value = 0;
  if (FILE* file = std::fopen(path, "r")) {
    if (std::fscanf(file, "%d", &value) != 1) value = 0;
    std::fclose(file);
  }
  return value;
}

SDL_Color shade(SDL_Color color, int delta) {
  auto clamp_channel = [&](int value) { return static_cast<Uint8>(std::clamp(value, 0, 255)); };
  return {clamp_channel(static_cast<int>(color.r) + delta), clamp_channel(static_cast<int>(color.g) + delta),
          clamp_channel(static_cast<int>(color.b) + delta), color.a};
}

Vec2 safe_spawn_position(GameState& game) {
  for (int attempt = 0; attempt < 48; ++attempt) {
    const Vec2 candidate{random_range(game.rng, 120.0f, kWorldWidth - 120.0f),
                         random_range(game.rng, 120.0f, kWorldHeight - 120.0f)};
    bool safe = true;
    for (const Agent& agent : game.agents) {
      for (const Cell& cell : agent.cells) {
        if (!cell.alive) continue;
        if (length_sq(sub(candidate, cell.pos)) < 210.0f * 210.0f) {
          safe = false;
          break;
        }
      }
      if (!safe) break;
    }
    if (safe) {
      for (const Virus& virus : game.viruses) {
        if (!virus.active) continue;
        const float avoid = virus.radius + 84.0f;
        if (length_sq(sub(candidate, virus.pos)) < avoid * avoid) {
          safe = false;
          break;
        }
      }
    }
    if (safe) return candidate;
  }
  return {random_range(game.rng, 120.0f, kWorldWidth - 120.0f), random_range(game.rng, 120.0f, kWorldHeight - 120.0f)};
}

void clear_agent(Agent& agent) {
  for (Cell& cell : agent.cells) cell = {};
}

void set_status(GameState& game, const std::string& text, float time_seconds) {
  game.status = text;
  game.status_timer = time_seconds;
}

void fill_virus_slot(GameState& game, Virus& virus) {
  for (int attempt = 0; attempt < 64; ++attempt) {
    const Vec2 candidate{random_range(game.rng, 80.0f, kWorldWidth - 80.0f),
                         random_range(game.rng, 80.0f, kWorldHeight - 80.0f)};
    bool safe = true;
    for (const Virus& other : game.viruses) {
      if (!other.active) continue;
      const float spacing = other.radius + kVirusRadius + 42.0f;
      if (length_sq(sub(candidate, other.pos)) < spacing * spacing) {
        safe = false;
        break;
      }
    }
    if (!safe) continue;
    virus.active = true;
    virus.pos = candidate;
    virus.radius = kVirusRadius + random_range(game.rng, -2.5f, 3.5f);
    return;
  }
  virus.active = true;
  virus.pos = {random_range(game.rng, 80.0f, kWorldWidth - 80.0f),
               random_range(game.rng, 80.0f, kWorldHeight - 80.0f)};
  virus.radius = kVirusRadius;
}

void respawn_agent(GameState& game, int index) {
  Agent& agent = game.agents[static_cast<std::size_t>(index)];
  clear_agent(agent);
  agent.aim = random_unit(game.rng);
  agent.throttle = 1.0f;
  agent.split_cooldown = 1.1f;
  agent.eject_cooldown = 0.2f;
  agent.think_timer = random_range(game.rng, 0.05f, 0.15f);
  agent.wander_timer = random_range(game.rng, 0.8f, 1.8f);
  agent.wander_dir = random_unit(game.rng);
  agent.respawn_timer = 0.0f;
  agent.cells[0].alive = true;
  agent.cells[0].pos = safe_spawn_position(game);
  agent.cells[0].mass = kSpawnMass;
  agent.cells[0].merge_timer = 0.0f;
  agent.cells[0].invulnerable = 1.8f;
}

void fill_food_slot(GameState& game, Food& pellet) {
  pellet.active = true;
  pellet.pos = {random_range(game.rng, 20.0f, kWorldWidth - 20.0f), random_range(game.rng, 20.0f, kWorldHeight - 20.0f)};
  pellet.mass = random_range(game.rng, kFoodMassMin, kFoodMassMax);
  pellet.color = kFoodColors[static_cast<std::size_t>(random_int(game.rng, 0, static_cast<int>(kFoodColors.size()) - 1))];
}

void init_agents(GameState& game) {
  static const std::array<std::string, kTotalAgents - 1> bot_names{{
      "ION",     "PLASMA", "RADON", "QUARK",  "PIXEL",  "FUSION", "ATOMIX", "VAPOR",  "ORBIT",  "SPARK",
      "NOVA",    "POLY",   "CYTO",  "VECTOR", "BOND",   "PROTON", "RIBBON", "CHARGE", "PULSE",  "VACUUM",
      "NEON",    "AXION",  "PHOTON"}};
  static const std::array<SDL_Color, 8> palette{{
      {117, 235, 202, 255}, {120, 188, 255, 255}, {255, 177, 95, 255}, {255, 110, 152, 255},
      {175, 150, 255, 255}, {164, 255, 120, 255}, {255, 229, 120, 255}, {158, 239, 255, 255},
  }};

  game.agents[0].name = "YOU";
  game.agents[0].color = {235, 247, 255, 255};
  game.agents[0].human = true;
  clear_agent(game.agents[0]);

  for (int index = 1; index < kTotalAgents; ++index) {
    game.agents[static_cast<std::size_t>(index)].name = bot_names[static_cast<std::size_t>(index - 1)];
    game.agents[static_cast<std::size_t>(index)].color = palette[static_cast<std::size_t>((index - 1) % static_cast<int>(palette.size()))];
    game.agents[static_cast<std::size_t>(index)].human = false;
    clear_agent(game.agents[static_cast<std::size_t>(index)]);
  }
}

void reset_match(GameState& game) {
  game.paused = false;
  game.status = "ABSORB THE PETRI DISH";
  game.status_timer = 2.5f;
  std::fill(game.ejecta.begin(), game.ejecta.end(), Ejecta{});
  for (Agent& agent : game.agents) clear_agent(agent);
  for (Virus& virus : game.viruses) fill_virus_slot(game, virus);
  for (Food& pellet : game.food) fill_food_slot(game, pellet);
  for (int index = 0; index < kTotalAgents; ++index) respawn_agent(game, index);
}

void compact_agent_cells(Agent& agent) {
  std::array<Cell, kMaxCellsPerAgent> compact{};
  int write = 0;
  for (const Cell& cell : agent.cells) {
    if (cell.alive && write < kMaxCellsPerAgent) {
      compact[static_cast<std::size_t>(write)] = cell;
      ++write;
    }
  }
  agent.cells = compact;
}

void spawn_ejecta(GameState& game, Agent& agent, int owner_index) {
  const int largest = largest_cell_index(agent);
  if (largest < 0) return;
  Cell& cell = agent.cells[static_cast<std::size_t>(largest)];
  if (cell.mass < 55.0f || agent.eject_cooldown > 0.0f) return;

  Ejecta* slot = nullptr;
  for (Ejecta& ejecta : game.ejecta) {
    if (!ejecta.active) {
      slot = &ejecta;
      break;
    }
  }
  if (slot == nullptr) return;

  Vec2 aim = agent.aim;
  if (length_sq(aim) < 0.01f) aim = {1.0f, 0.0f};
  aim = normalize(aim);
  const float radius = radius_for_mass(cell.mass);
  const float ejected_mass = 10.0f;
  cell.mass -= ejected_mass;
  slot->active = true;
  slot->owner = owner_index;
  slot->mass = ejected_mass;
  slot->color = shade(agent.color, 28);
  slot->age = 0.0f;
  slot->pos = add(cell.pos, mul(aim, radius + 9.0f));
  slot->vel = mul(aim, 260.0f);
  agent.eject_cooldown = kEjectCooldown;
  trigger_tone(game.tone, 560.0f, 50);
}

void split_agent(GameState& game, Agent& agent) {
  if (agent.split_cooldown > 0.0f) return;
  if (living_cells(agent) <= 0 || living_cells(agent) >= kMaxCellsPerAgent) return;

  Vec2 aim = agent.aim;
  if (length_sq(aim) < 0.01f) aim = {1.0f, 0.0f};
  aim = normalize(aim);

  const int source_index = largest_cell_index(agent);
  const int free_index = free_cell_index(agent);
  if (source_index < 0 || free_index < 0) return;

  Cell& cell = agent.cells[static_cast<std::size_t>(source_index)];
  if (cell.mass < 72.0f) return;

  Cell next{};
  next.alive = true;
  next.mass = cell.mass * 0.5f;
  cell.mass *= 0.5f;
  next.pos = add(cell.pos, mul(aim, radius_for_mass(next.mass) + 22.0f));
  next.vel = add(cell.vel, mul(aim, 300.0f));
  next.merge_timer = kMergeDelay;
  next.invulnerable = 0.6f;
  cell.merge_timer = kMergeDelay;
  cell.invulnerable = std::max(cell.invulnerable, 0.3f);
  cell.vel = add(cell.vel, mul(aim, -55.0f));
  agent.cells[static_cast<std::size_t>(free_index)] = next;
  agent.split_cooldown = kSplitCooldown;
  trigger_tone(game.tone, 820.0f, 65);
}

void burst_agent_on_virus(GameState& game, Agent& agent, int source_index) {
  Cell& source = agent.cells[static_cast<std::size_t>(source_index)];
  const int alive_count = living_cells(agent);
  const int room = kMaxCellsPerAgent - alive_count;
  const int extra_cells = std::min(room, std::max(2, std::min(4, static_cast<int>(source.mass / 90.0f))));
  if (extra_cells <= 0) return;

  const float piece_mass = source.mass / static_cast<float>(extra_cells + 1);
  source.mass = piece_mass;
  source.merge_timer = kMergeDelay + 1.0f;
  source.invulnerable = 0.4f;

  for (int spawn = 0; spawn < extra_cells; ++spawn) {
    const int slot_index = free_cell_index(agent);
    if (slot_index < 0) break;
    Vec2 direction = random_unit(game.rng);
    if (length_sq(direction) < 0.01f) direction = {1.0f, 0.0f};
    Cell fragment{};
    fragment.alive = true;
    fragment.mass = piece_mass;
    fragment.pos = add(source.pos, mul(direction, radius_for_mass(piece_mass) + 18.0f));
    fragment.vel = add(source.vel, mul(direction, random_range(game.rng, 240.0f, 360.0f)));
    fragment.merge_timer = kMergeDelay + 1.0f;
    fragment.invulnerable = 0.45f;
    agent.cells[static_cast<std::size_t>(slot_index)] = fragment;
  }
}

void move_cells(Agent& agent, float dt) {
  if (agent.split_cooldown > 0.0f) agent.split_cooldown = std::max(0.0f, agent.split_cooldown - dt);
  if (agent.eject_cooldown > 0.0f) agent.eject_cooldown = std::max(0.0f, agent.eject_cooldown - dt);
  for (Cell& cell : agent.cells) {
    if (!cell.alive) continue;
    if (cell.merge_timer > 0.0f) cell.merge_timer = std::max(0.0f, cell.merge_timer - dt);
    if (cell.invulnerable > 0.0f) cell.invulnerable = std::max(0.0f, cell.invulnerable - dt);

    const Vec2 push = mul(normalize(agent.aim), speed_for_mass(cell.mass) * agent.throttle);
    cell.vel = add(mul(cell.vel, std::pow(0.18f, dt)), mul(push, dt * 9.0f));
    cell.pos = add(cell.pos, mul(cell.vel, dt));
    const float radius = radius_for_mass(cell.mass);
    cell.pos.x = clampf(cell.pos.x, radius, kWorldWidth - radius);
    cell.pos.y = clampf(cell.pos.y, radius, kWorldHeight - radius);
  }
}

void resolve_same_agent(Agent& agent) {
  for (int i = 0; i < kMaxCellsPerAgent; ++i) {
    Cell& a = agent.cells[static_cast<std::size_t>(i)];
    if (!a.alive) continue;
    for (int j = i + 1; j < kMaxCellsPerAgent; ++j) {
      Cell& b = agent.cells[static_cast<std::size_t>(j)];
      if (!b.alive) continue;
      const float ra = radius_for_mass(a.mass);
      const float rb = radius_for_mass(b.mass);
      const Vec2 delta = sub(b.pos, a.pos);
      float dist = length(delta);
      if (dist < 0.001f) dist = 0.001f;

      const bool can_merge_now = a.merge_timer <= 0.0f && b.merge_timer <= 0.0f;
      const Vec2 dir = {delta.x / dist, delta.y / dist};

      if (can_merge_now && dist < std::max(ra, rb) * 0.9f) {
        if (a.mass >= b.mass) {
          a.mass += b.mass;
          b = {};
        } else {
          b.mass += a.mass;
          a = {};
        }
        continue;
      }

      const float desired = can_merge_now ? std::max(ra, rb) * 0.42f : ra + rb + 6.0f;
      if (can_merge_now && dist > desired) {
        const float pull = std::min(5.0f, (dist - desired) * 0.08f);
        a.pos = add(a.pos, mul(dir, pull));
        b.pos = sub(b.pos, mul(dir, pull));
      } else if (dist < desired) {
        const float push = (desired - dist) * 0.5f;
        a.pos = sub(a.pos, mul(dir, push));
        b.pos = add(b.pos, mul(dir, push));
      }
    }
  }
  compact_agent_cells(agent);
}

void consume_food(GameState& game, Agent& agent) {
  for (Cell& cell : agent.cells) {
    if (!cell.alive) continue;
    const float radius = radius_for_mass(cell.mass);
    for (Food& pellet : game.food) {
      if (!pellet.active) continue;
      if (length_sq(sub(cell.pos, pellet.pos)) <= radius * radius) {
        cell.mass += pellet.mass;
        fill_food_slot(game, pellet);
      }
    }
  }
}

void consume_ejecta(GameState& game, Agent& agent, int agent_index) {
  for (Cell& cell : agent.cells) {
    if (!cell.alive) continue;
    const float radius = radius_for_mass(cell.mass);
    for (Ejecta& ejecta : game.ejecta) {
      if (!ejecta.active || (ejecta.owner == agent_index && ejecta.age < 0.7f)) continue;
      if (length_sq(sub(cell.pos, ejecta.pos)) <= radius * radius) {
        cell.mass += ejecta.mass * 0.92f;
        ejecta.active = false;
      }
    }
  }
}

void update_ejecta(GameState& game, float dt) {
  for (Ejecta& ejecta : game.ejecta) {
    if (!ejecta.active) continue;
    ejecta.age += dt;
    ejecta.vel = mul(ejecta.vel, std::pow(0.08f, dt));
    ejecta.pos = add(ejecta.pos, mul(ejecta.vel, dt));
    ejecta.pos.x = clampf(ejecta.pos.x, 8.0f, kWorldWidth - 8.0f);
    ejecta.pos.y = clampf(ejecta.pos.y, 8.0f, kWorldHeight - 8.0f);
    if (ejecta.age > 10.0f) ejecta.active = false;
  }
}

void update_virus_collisions(GameState& game) {
  for (Virus& virus : game.viruses) {
    if (!virus.active) continue;
    bool burst = false;
    for (int agent_index = 0; agent_index < kTotalAgents && !burst; ++agent_index) {
      Agent& agent = game.agents[static_cast<std::size_t>(agent_index)];
      for (int cell_index = 0; cell_index < kMaxCellsPerAgent; ++cell_index) {
        Cell& cell = agent.cells[static_cast<std::size_t>(cell_index)];
        if (!cell.alive) continue;
        const float cell_radius = radius_for_mass(cell.mass);
        const float dist = length(sub(cell.pos, virus.pos));
        if (cell.mass >= kVirusBurstMass && cell_radius >= virus.radius * 0.94f &&
            dist < std::max(virus.radius, cell_radius * 0.4f)) {
          burst_agent_on_virus(game, agent, cell_index);
          compact_agent_cells(agent);
          fill_virus_slot(game, virus);
          if (agent.human) {
            set_status(game, "GREEN BLOB BURST", 1.2f);
            trigger_tone(game.tone, 980.0f, 90);
          }
          burst = true;
          break;
        }
      }
    }
  }
}

bool can_eat(const Cell& larger, const Cell& smaller) {
  if (!larger.alive || !smaller.alive) return false;
  if (larger.mass <= smaller.mass * 1.12f) return false;
  const float lr = radius_for_mass(larger.mass);
  const float sr = radius_for_mass(smaller.mass);
  const float dist = length(sub(larger.pos, smaller.pos));
  return dist < lr - sr * 0.32f;
}

void resolve_combat(GameState& game) {
  for (int a_index = 0; a_index < kTotalAgents; ++a_index) {
    for (int b_index = a_index + 1; b_index < kTotalAgents; ++b_index) {
      Agent& a_agent = game.agents[static_cast<std::size_t>(a_index)];
      Agent& b_agent = game.agents[static_cast<std::size_t>(b_index)];
      for (Cell& a_cell : a_agent.cells) {
        if (!a_cell.alive) continue;
        for (Cell& b_cell : b_agent.cells) {
          if (!b_cell.alive || a_cell.invulnerable > 0.0f || b_cell.invulnerable > 0.0f) continue;
          if (can_eat(a_cell, b_cell)) {
            a_cell.mass += b_cell.mass * 0.96f;
            b_cell = {};
            if (a_agent.human) trigger_tone(game.tone, 920.0f, 65);
          } else if (can_eat(b_cell, a_cell)) {
            b_cell.mass += a_cell.mass * 0.96f;
            a_cell = {};
            if (b_agent.human) trigger_tone(game.tone, 920.0f, 65);
            break;
          }
        }
      }
      compact_agent_cells(a_agent);
      compact_agent_cells(b_agent);
    }
  }
}

void update_best_mass(GameState& game) {
  const int human_mass = static_cast<int>(std::round(total_mass(game.agents[0])));
  if (human_mass > game.best_mass) {
    game.best_mass = human_mass;
    if (game.best_mass > game.stored_best_mass) {
      game.stored_best_mass = game.best_mass;
      save_best_mass(game.context, game.best_mass);
    }
  }
}

Camera camera_target_for(const GameState& game) {
  Camera camera{};
  const Agent& human = game.agents[0];
  if (alive(human)) {
    const int largest = largest_cell_index(human);
    Vec2 min_pos = human.cells[static_cast<std::size_t>(largest)].pos;
    Vec2 max_pos = min_pos;
    float widest_radius = radius_for_mass(human.cells[static_cast<std::size_t>(largest)].mass);
    for (const Cell& cell : human.cells) {
      if (!cell.alive) continue;
      const float radius = radius_for_mass(cell.mass);
      widest_radius = std::max(widest_radius, radius);
      min_pos.x = std::min(min_pos.x, cell.pos.x - radius);
      min_pos.y = std::min(min_pos.y, cell.pos.y - radius);
      max_pos.x = std::max(max_pos.x, cell.pos.x + radius);
      max_pos.y = std::max(max_pos.y, cell.pos.y + radius);
    }
    camera.center = {(min_pos.x + max_pos.x) * 0.5f, (min_pos.y + max_pos.y) * 0.5f};
    const float span_x = max_pos.x - min_pos.x;
    const float span_y = max_pos.y - min_pos.y;
    const float focus_span = std::max({span_x, span_y, widest_radius * 2.0f});
    camera.zoom = clampf(1.0f - focus_span / 390.0f, 0.24f, 0.98f);
  } else {
    camera.center = {kWorldWidth * 0.5f, kWorldHeight * 0.5f};
    camera.zoom = 0.7f;
  }
  return camera;
}

void update_camera(GameState& game, float dt) {
  const Camera target = camera_target_for(game);
  const float center_blend = 1.0f - std::pow(0.0008f, dt);
  const float zoom_blend = 1.0f - std::pow(0.004f, dt);
  game.camera.center.x += (target.center.x - game.camera.center.x) * center_blend;
  game.camera.center.y += (target.center.y - game.camera.center.y) * center_blend;
  game.camera.zoom += (target.zoom - game.camera.zoom) * zoom_blend;
}

SDL_Point world_to_screen(Vec2 world, const Camera& camera) {
  const float screen_x = (world.x - camera.center.x) * camera.zoom + kVirtualWidth * 0.5f;
  const float screen_y = (world.y - camera.center.y) * camera.zoom + kVirtualHeight * 0.5f;
  return {static_cast<int>(std::lround(screen_x)), static_cast<int>(std::lround(screen_y))};
}

void draw_virus(SDL_Renderer* renderer, int x, int y, int radius) {
  fill_circle(renderer, x, y, radius, {68, 169, 76, 228});
  fill_circle(renderer, x, y, std::max(4, radius - 4), {111, 228, 96, 228});
  fill_circle(renderer, x, y, std::max(2, radius - 10), {176, 255, 143, 210});
  for (int spike = 0; spike < 8; ++spike) {
    const float angle = static_cast<float>(spike) * 0.78539816f;
    const int sx = static_cast<int>(std::lround(x + std::cos(angle) * (radius - 2)));
    const int sy = static_cast<int>(std::lround(y + std::sin(angle) * (radius - 2)));
    fill_circle(renderer, sx, sy, std::max(2, radius / 4), {82, 196, 92, 228});
  }
}

void draw_world(SDL_Renderer* renderer, const GameState& game, const Camera& camera) {
  fill_rect(renderer, {0, 0, kVirtualWidth, kVirtualHeight}, kBg);

  const int grid_step = static_cast<int>(std::lround(120.0f * camera.zoom));
  if (grid_step >= 12) {
    SDL_SetRenderDrawColor(renderer, kGrid.r, kGrid.g, kGrid.b, kGrid.a);
    const float origin_x = std::fmod(-camera.center.x * camera.zoom + kVirtualWidth * 0.5f, static_cast<float>(grid_step));
    const float origin_y = std::fmod(-camera.center.y * camera.zoom + kVirtualHeight * 0.5f, static_cast<float>(grid_step));
    for (int x = static_cast<int>(origin_x); x < kVirtualWidth; x += grid_step) SDL_RenderDrawLine(renderer, x, 0, x, kVirtualHeight);
    for (int y = static_cast<int>(origin_y); y < kVirtualHeight; y += grid_step) SDL_RenderDrawLine(renderer, 0, y, kVirtualWidth, y);
  }

  const SDL_Point world_tl = world_to_screen({0.0f, 0.0f}, camera);
  const SDL_Point world_br = world_to_screen({kWorldWidth, kWorldHeight}, camera);
  draw_rect(renderer, {world_tl.x, world_tl.y, world_br.x - world_tl.x, world_br.y - world_tl.y}, kWorldBorder);

  for (const Food& pellet : game.food) {
    if (!pellet.active) continue;
    const SDL_Point p = world_to_screen(pellet.pos, camera);
    const int radius = std::max(1, static_cast<int>(std::lround((1.5f + pellet.mass * 0.55f) * camera.zoom)));
    fill_circle(renderer, p.x, p.y, radius, pellet.color);
  }

  for (const Ejecta& ejecta : game.ejecta) {
    if (!ejecta.active) continue;
    const SDL_Point p = world_to_screen(ejecta.pos, camera);
    const int radius = std::max(2, static_cast<int>(std::lround(radius_for_mass(ejecta.mass) * camera.zoom * 0.65f)));
    fill_circle(renderer, p.x, p.y, radius, ejecta.color);
    fill_circle(renderer, p.x, p.y, std::max(1, radius - 2), shade(ejecta.color, 30));
  }

  struct DrawBlob { int agent = 0; int cell = 0; float mass = 0.0f; };
  std::vector<DrawBlob> draw_list;
  for (int agent_index = 0; agent_index < kTotalAgents; ++agent_index) {
    for (int cell_index = 0; cell_index < kMaxCellsPerAgent; ++cell_index) {
      if (game.agents[static_cast<std::size_t>(agent_index)].cells[static_cast<std::size_t>(cell_index)].alive) {
        draw_list.push_back({agent_index, cell_index, game.agents[static_cast<std::size_t>(agent_index)].cells[static_cast<std::size_t>(cell_index)].mass});
      }
    }
  }
  std::sort(draw_list.begin(), draw_list.end(), [](const DrawBlob& a, const DrawBlob& b) { return a.mass < b.mass; });

  for (const DrawBlob& entry : draw_list) {
    const Agent& agent = game.agents[static_cast<std::size_t>(entry.agent)];
    const Cell& cell = agent.cells[static_cast<std::size_t>(entry.cell)];
    const SDL_Point p = world_to_screen(cell.pos, camera);
    const int radius = std::max(4, static_cast<int>(std::lround(radius_for_mass(cell.mass) * camera.zoom)));
    SDL_Color body = agent.human ? SDL_Color{225, 245, 255, 255} : agent.color;
    fill_circle(renderer, p.x, p.y, radius, shade(body, -18));
    fill_circle(renderer, p.x, p.y, std::max(2, radius - 2), body);
    fill_circle(renderer, p.x - radius / 4, p.y - radius / 4, std::max(1, radius / 6), shade(body, 38));
    if (radius >= 12) draw_text(renderer, agent.name, p.x, p.y - 4, radius >= 20 ? 2 : 1, {12, 16, 22, 255}, true);
  }

  for (const Virus& virus : game.viruses) {
    if (!virus.active) continue;
    const SDL_Point p = world_to_screen(virus.pos, camera);
    const int radius = std::max(8, static_cast<int>(std::lround(virus.radius * camera.zoom)));
    draw_virus(renderer, p.x, p.y, radius);
  }
}

void draw_hud(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {10, 10, 186, 56}, kPanel);
  draw_rect(renderer, {10, 10, 186, 56}, kFrame);
  draw_text(renderer, "MOLECULES", 22, 20, 2, kText);
  draw_text(renderer, "BEST", 22, 44, 1, kMuted);
  draw_text_right(renderer, std::to_string(game.best_mass), 92, 44, 1, kAccent);
  draw_text(renderer, "MASS", 114, 44, 1, kMuted);
  draw_text_right(renderer, std::to_string(static_cast<int>(std::round(total_mass(game.agents[0])))), 182, 44, 1, kText);

  fill_rect(renderer, {346, 10, 156, 170}, kPanel);
  draw_rect(renderer, {346, 10, 156, 170}, kFrame);
  draw_text(renderer, "TOP CELLS", 356, 20, 1, kText);
  std::vector<int> ranking(kTotalAgents);
  std::iota(ranking.begin(), ranking.end(), 0);
  std::sort(ranking.begin(), ranking.end(), [&](int a, int b) { return total_mass(game.agents[static_cast<std::size_t>(a)]) > total_mass(game.agents[static_cast<std::size_t>(b)]); });
  for (int i = 0; i < 6; ++i) {
    const Agent& agent = game.agents[static_cast<std::size_t>(ranking[static_cast<std::size_t>(i)])];
    SDL_Color row_color = agent.human ? kAccent : agent.color;
    draw_text(renderer, agent.name, 356, 42 + i * 18, 1, row_color);
    draw_text_right(renderer, std::to_string(static_cast<int>(std::round(total_mass(agent)))), 494, 42 + i * 18, 1, kText);
  }
  draw_text(renderer, "LOBBY", 356, 152, 1, kMuted);
  draw_text_right(renderer, std::to_string(kTotalAgents), 494, 152, 1, kWarning);

  fill_rect(renderer, {10, 430, 276, 72}, kPanel);
  draw_rect(renderer, {10, 430, 276, 72}, kFrame);
  draw_text(renderer, "A SPLIT", 20, 440, 1, kText);
  draw_text(renderer, "B EJECT MASS", 20, 456, 1, kText);
  draw_text(renderer, "GREEN BLOBS BURST BIG CELLS", 20, 472, 1, kText);
  draw_text(renderer, "START PAUSE  SELECT EXIT", 20, 488, 1, kText);

  if (game.status_timer > 0.0f) {
    fill_rect(renderer, {106, 18, 300, 28}, {10, 15, 22, 214});
    draw_rect(renderer, {106, 18, 300, 28}, kFrame);
    draw_text(renderer, game.status, 256, 26, 1, kWarning, true);
  }

  if (!alive(game.agents[0])) {
    fill_rect(renderer, {120, 220, 272, 62}, {8, 12, 18, 224});
    draw_rect(renderer, {120, 220, 272, 62}, kDanger);
    draw_text(renderer, "YOU WERE ABSORBED", 256, 232, 2, kText, true);
    const int seconds = std::max(0, static_cast<int>(std::ceil(game.agents[0].respawn_timer)));
    draw_text(renderer, "RESPAWN " + std::to_string(seconds), 256, 258, 1, kWarning, true);
  }

  if (game.paused) {
    fill_rect(renderer, {126, 170, 260, 92}, {8, 12, 18, 230});
    draw_rect(renderer, {126, 170, 260, 92}, kFrame);
    draw_text(renderer, "PAUSED", 256, 188, 3, kText, true);
    draw_text(renderer, "START RESUME", 256, 226, 1, kAccent, true);
    draw_text(renderer, "SELECT EXIT", 256, 244, 1, kMuted, true);
  }
}

void update_bot_ai(GameState& game, int index, float dt) {
  Agent& agent = game.agents[static_cast<std::size_t>(index)];
  if (agent.human || !alive(agent)) return;

  agent.think_timer -= dt;
  agent.wander_timer -= dt;
  if (agent.wander_timer <= 0.0f) {
    agent.wander_timer = random_range(game.rng, 1.1f, 2.1f);
    agent.wander_dir = random_unit(game.rng);
  }
  if (agent.think_timer > 0.0f) return;
  agent.think_timer = kBotThinkDelay + random_range(game.rng, 0.0f, 0.08f);

  const int largest = largest_cell_index(agent);
  if (largest < 0) return;

  const Cell& focus = agent.cells[static_cast<std::size_t>(largest)];
  Vec2 force = mul(agent.wander_dir, 0.75f);
  const float focus_radius = radius_for_mass(focus.mass);
  force = add(force, mul(normalize(sub(Vec2{kWorldWidth * 0.5f, kWorldHeight * 0.5f}, focus.pos)), 0.55f));

  for (const Food& pellet : game.food) {
    if (!pellet.active) continue;
    const Vec2 to = sub(pellet.pos, focus.pos);
    const float dist2 = length_sq(to);
    if (dist2 > 520.0f * 520.0f) continue;
    force = add(force, mul(normalize(to), 3800.0f / (120.0f + dist2)));
  }

  for (const Virus& virus : game.viruses) {
    if (!virus.active) continue;
    const Vec2 to = sub(virus.pos, focus.pos);
    const float dist2 = std::max(1.0f, length_sq(to));
    if (focus.mass >= kVirusBurstMass && focus_radius >= virus.radius * 0.92f) {
      force = add(force, mul(normalize(mul(to, -1.0f)), 120000.0f / dist2));
    } else if (dist2 < 220.0f * 220.0f) {
      force = add(force, mul(normalize(to), 9000.0f / (160.0f + dist2)));
    }
  }

  float split_score = 0.0f;
  for (int other_index = 0; other_index < kTotalAgents; ++other_index) {
    if (other_index == index) continue;
    const Agent& other = game.agents[static_cast<std::size_t>(other_index)];
    for (int cell_index = 0; cell_index < kMaxCellsPerAgent; ++cell_index) {
      const Cell& prey = other.cells[static_cast<std::size_t>(cell_index)];
      if (!prey.alive) continue;
      const Vec2 to = sub(prey.pos, focus.pos);
      const float dist2 = std::max(1.0f, length_sq(to));
      const float dist = std::sqrt(dist2);
      if (prey.mass > focus.mass * 1.08f) {
        force = add(force, mul(normalize(mul(to, -1.0f)), (prey.mass / focus.mass) * 88000.0f / dist2));
      } else if (focus.mass > prey.mass * 1.2f) {
        force = add(force, mul(normalize(to), (focus.mass / std::max(8.0f, prey.mass)) * 21000.0f / dist2));
        split_score = std::max(split_score, focus.mass / std::max(8.0f, prey.mass) - dist / 180.0f);
      }
    }
  }

  if (living_cells(agent) > 1) {
    Vec2 cluster{};
    int cluster_count = 0;
    for (const Cell& cell : agent.cells) {
      if (!cell.alive) continue;
      cluster = add(cluster, cell.pos);
      ++cluster_count;
    }
    if (cluster_count > 0) {
      cluster = mul(cluster, 1.0f / static_cast<float>(cluster_count));
      force = add(force, mul(normalize(sub(cluster, focus.pos)), 0.85f));
    }
  }

  const float edge_margin = 260.0f;
  if (focus.pos.x < edge_margin) force = add(force, {(edge_margin - focus.pos.x) / 42.0f, 0.0f});
  if (focus.pos.x > kWorldWidth - edge_margin) force = add(force, {-(focus.pos.x - (kWorldWidth - edge_margin)) / 42.0f, 0.0f});
  if (focus.pos.y < edge_margin) force = add(force, {0.0f, (edge_margin - focus.pos.y) / 42.0f});
  if (focus.pos.y > kWorldHeight - edge_margin) force = add(force, {0.0f, -(focus.pos.y - (kWorldHeight - edge_margin)) / 42.0f});

  if (length_sq(force) < 0.01f) force = agent.wander_dir;
  agent.aim = normalize(force);
  agent.throttle = 1.0f;
  if (agent.split_cooldown <= 0.0f && split_score > 0.8f && living_cells(agent) < 4) {
    split_agent(game, agent);
  }
}

void update_human_controls(GameState& game, const pp_input_state& input, bool a_pressed, bool b_pressed, bool start_pressed, bool select_pressed) {
  Agent& human = game.agents[0];
  if (start_pressed) {
    game.paused = !game.paused;
    set_status(game, game.paused ? "PAUSED" : "RESUMED", 1.0f);
    trigger_tone(game.tone, game.paused ? 240.0f : 620.0f, 70);
  }
  if (select_pressed) pp_request_exit(&game.context);
  if (game.paused) {
    human.throttle = 0.0f;
    return;
  }

  Vec2 aim{};
  if (input.left) aim.x -= 1.0f;
  if (input.right) aim.x += 1.0f;
  if (input.up) aim.y -= 1.0f;
  if (input.down) aim.y += 1.0f;
  if (length_sq(aim) > 0.0f) {
    human.aim = normalize(aim);
    human.throttle = 1.0f;
  } else {
    human.throttle = 0.0f;
  }

  if (a_pressed) split_agent(game, human);
  if (b_pressed) spawn_ejecta(game, human, 0);
}

void update_respawn(GameState& game, Agent& agent, int index, float dt) {
  if (alive(agent)) {
    agent.respawn_timer = 0.0f;
    return;
  }
  if (agent.respawn_timer <= 0.0f) {
    agent.respawn_timer = kRespawnDelay + (agent.human ? 0.0f : random_range(game.rng, 0.0f, 1.0f));
    if (agent.human) {
      set_status(game, "YOU WERE ABSORBED", 1.5f);
      trigger_tone(game.tone, 180.0f, 220);
    }
  } else {
    agent.respawn_timer -= dt;
    if (agent.respawn_timer <= 0.0f) {
      respawn_agent(game, index);
      if (agent.human) set_status(game, "RESPAWNED", 1.1f);
    }
  }
}

void update_game(GameState& game, float dt) {
  if (game.paused) {
    update_camera(game, dt);
    return;
  }
  for (int index = 1; index < kTotalAgents; ++index) update_bot_ai(game, index, dt);
  for (Agent& agent : game.agents) move_cells(agent, dt);
  update_ejecta(game, dt);
  for (Agent& agent : game.agents) resolve_same_agent(agent);
  update_virus_collisions(game);
  for (int index = 0; index < kTotalAgents; ++index) {
    consume_food(game, game.agents[static_cast<std::size_t>(index)]);
    consume_ejecta(game, game.agents[static_cast<std::size_t>(index)], index);
  }
  resolve_combat(game);
  for (int index = 0; index < kTotalAgents; ++index) {
    update_respawn(game, game.agents[static_cast<std::size_t>(index)], index, dt);
  }
  update_best_mass(game);
  if (game.status_timer > 0.0f) game.status_timer = std::max(0.0f, game.status_timer - dt);
  update_camera(game, dt);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) return 1;

  GameState game{};
  if (pp_init(&game.context, "molecules") != 0) {
    SDL_Quit();
    return 1;
  }

  int width = kVirtualWidth;
  int height = kVirtualHeight;
  pp_get_framebuffer_size(&game.context, &width, &height);
  if (width <= 0 || height <= 0) {
    width = kVirtualWidth;
    height = kVirtualHeight;
  }

  SDL_Window* window = SDL_CreateWindow("Molecules", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
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

  init_agents(game);
  game.stored_best_mass = load_best_mass(game.context);
  game.best_mass = game.stored_best_mass;
  reset_match(game);
  game.camera = camera_target_for(game);
  game.last_ticks = SDL_GetTicks();

  pp_input_state input{};
  pp_input_state previous{};
  while (!pp_should_exit(&game.context)) {
    const Uint32 now = SDL_GetTicks();
    float dt = (now - game.last_ticks) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    game.last_ticks = now;

    pp_poll_input(&game.context, &input);
    const bool a_pressed = input.a && !previous.a;
    const bool b_pressed = input.b && !previous.b;
    const bool start_pressed = input.start && !previous.start;
    const bool select_pressed = input.select && !previous.select;

    update_human_controls(game, input, a_pressed, b_pressed, start_pressed, select_pressed);
    update_game(game, dt);

    draw_world(renderer, game, game.camera);
    draw_hud(renderer, game);
    SDL_RenderPresent(renderer);

    previous = input;
    SDL_Delay(16);
  }

  save_best_mass(game.context, game.best_mass);
  if (audio_device != 0U) SDL_CloseAudioDevice(audio_device);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&game.context);
  SDL_Quit();
  return 0;
}
