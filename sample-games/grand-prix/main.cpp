#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define GRAND_PRIX_PATH_SEPARATOR '\\'
#else
#define GRAND_PRIX_PATH_SEPARATOR '/'
#endif

namespace {

constexpr int kWindowWidth = 512;
constexpr int kWindowHeight = 512;
constexpr float kPi = 3.14159265f;
constexpr int kTrackRenderBottom = 372;
constexpr int kMaxNameLength = 12;

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

Vec2 operator+(Vec2 left, Vec2 right) { return {left.x + right.x, left.y + right.y}; }
Vec2 operator-(Vec2 left, Vec2 right) { return {left.x - right.x, left.y - right.y}; }
Vec2 operator*(Vec2 value, float scalar) { return {value.x * scalar, value.y * scalar}; }
Vec2 operator/(Vec2 value, float scalar) { return {value.x / scalar, value.y / scalar}; }

Vec3 operator+(Vec3 left, Vec3 right) { return {left.x + right.x, left.y + right.y, left.z + right.z}; }
Vec3 operator-(Vec3 left, Vec3 right) { return {left.x - right.x, left.y - right.y, left.z - right.z}; }
Vec3 operator*(Vec3 value, float scalar) { return {value.x * scalar, value.y * scalar, value.z * scalar}; }
Vec3 operator/(Vec3 value, float scalar) { return {value.x / scalar, value.y / scalar, value.z / scalar}; }

float dot(Vec2 left, Vec2 right) { return left.x * right.x + left.y * right.y; }

float dot(Vec3 left, Vec3 right) { return left.x * right.x + left.y * right.y + left.z * right.z; }

Vec3 cross(Vec3 left, Vec3 right) {
  return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

float length(Vec2 value) { return std::sqrt(value.x * value.x + value.y * value.y); }

float length(Vec3 value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); }

Vec2 normalize(Vec2 value) {
  const float value_length = length(value);
  if (value_length <= 0.0001f) {
    return {0.0f, 0.0f};
  }
  return value / value_length;
}

Vec3 normalize(Vec3 value) {
  const float value_length = length(value);
  if (value_length <= 0.0001f) {
    return {0.0f, 0.0f, 0.0f};
  }
  return value / value_length;
}

float clampf(float value, float low, float high) { return std::max(low, std::min(value, high)); }

float approach(float current, float target, float amount) {
  if (current < target) {
    return std::min(current + amount, target);
  }
  return std::max(current - amount, target);
}

float wrap_value(float value, float limit) {
  while (value < 0.0f) {
    value += limit;
  }
  while (value >= limit) {
    value -= limit;
  }
  return value;
}

float loop_ahead_distance(float from, float to, float total) { return wrap_value(to - from, total); }

float loop_signed_delta(float from, float to, float total) {
  float delta = wrap_value(to - from, total);
  if (delta > total * 0.5f) {
    delta -= total;
  }
  return delta;
}

SDL_Color lighten(SDL_Color color, int amount) {
  color.r = static_cast<Uint8>(std::min(255, static_cast<int>(color.r) + amount));
  color.g = static_cast<Uint8>(std::min(255, static_cast<int>(color.g) + amount));
  color.b = static_cast<Uint8>(std::min(255, static_cast<int>(color.b) + amount));
  return color;
}

SDL_Color darken(SDL_Color color, int amount) {
  color.r = static_cast<Uint8>(std::max(0, static_cast<int>(color.r) - amount));
  color.g = static_cast<Uint8>(std::max(0, static_cast<int>(color.g) - amount));
  color.b = static_cast<Uint8>(std::max(0, static_cast<int>(color.b) - amount));
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

void fill_circle(SDL_Renderer* renderer, int center_x, int center_y, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int dy = -radius; dy <= radius; ++dy) {
    const int span = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    SDL_RenderDrawLine(renderer, center_x - span, center_y + dy, center_x + span, center_y + dy);
  }
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
    case '?': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case '!': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
    default: return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
}

int text_width(const std::string& text, int scale) {
  if (text.empty()) {
    return 0;
  }
  return static_cast<int>(text.size()) * (6 * scale) - scale;
}

std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    if (ch >= 'a' && ch <= 'z') {
      return static_cast<char>(ch - ('a' - 'A'));
    }
    return static_cast<char>(ch);
  });
  return value;
}

void draw_text(SDL_Renderer* renderer,
               const std::string& text,
               int x,
               int y,
               int scale,
               SDL_Color color,
               bool centered) {
  int draw_x = x;
  const std::string upper = uppercase(text);
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

struct AudioState {
  float hum_phase = 0.0f;
  float tone_phase = 0.0f;
  float target_frequency = 92.0f;
  float tone_frequency = 0.0f;
  int tone_frames = 0;
};

void audio_callback(void* userdata, Uint8* stream, int length) {
  auto* audio = static_cast<AudioState*>(userdata);
  auto* samples = reinterpret_cast<int16_t*>(stream);
  const int count = length / static_cast<int>(sizeof(int16_t));
  for (int index = 0; index < count; ++index) {
    float hum = std::sin(audio->hum_phase) * 700.0f;
    float tone = 0.0f;
    audio->hum_phase += (2.0f * kPi * audio->target_frequency) / 48000.0f;
    if (audio->hum_phase > 2.0f * kPi) {
      audio->hum_phase -= 2.0f * kPi;
    }
    if (audio->tone_frames > 0) {
      tone = (audio->tone_phase < kPi) ? 1900.0f : -1900.0f;
      audio->tone_phase += (2.0f * kPi * audio->tone_frequency) / 48000.0f;
      if (audio->tone_phase > 2.0f * kPi) {
        audio->tone_phase -= 2.0f * kPi;
      }
      --audio->tone_frames;
    }
    samples[index] = static_cast<int16_t>(clampf(hum + tone, -3200.0f, 3200.0f));
  }
}

void play_tone(AudioState& audio, float frequency, int milliseconds) {
  audio.tone_frequency = frequency;
  audio.tone_frames = (48000 * milliseconds) / 1000;
}

enum class Screen { title, garage, name_entry, setup, racing, results };
enum class ItemType { none = 0, tire, oil, nos };

struct Archetype {
  const char* name;
  float acceleration;
  float top_speed;
  float handling;
  float resilience;
  SDL_Color accent;
};

const std::array<Archetype, 4> kArchetypes = {{
    {"COMET", 54.0f, 122.0f, 86.0f, 0.94f, {255, 156, 78, 255}},
    {"RIVET", 48.0f, 128.0f, 74.0f, 1.02f, {87, 189, 234, 255}},
    {"LUNA", 58.0f, 117.0f, 94.0f, 0.91f, {242, 104, 186, 255}},
    {"TORQ", 46.0f, 132.0f, 68.0f, 1.08f, {152, 223, 87, 255}},
}};

const std::array<SDL_Color, 8> kSuitColors = {{{223, 73, 73, 255},   {73, 148, 223, 255},
                                                {242, 198, 72, 255},  {77, 184, 110, 255},
                                                {168, 84, 222, 255},  {244, 130, 62, 255},
                                                {237, 237, 237, 255}, {56, 57, 73, 255}}};
const std::array<SDL_Color, 8> kHairColors = {{{52, 33, 23, 255},    {92, 58, 41, 255},
                                                {167, 103, 53, 255},  {225, 188, 117, 255},
                                                {32, 42, 67, 255},    {197, 67, 109, 255},
                                                {112, 119, 135, 255}, {228, 241, 248, 255}}};
const std::array<SDL_Color, 8> kKartColors = {{{234, 75, 83, 255},   {70, 143, 219, 255},
                                                {245, 187, 64, 255},  {84, 195, 132, 255},
                                                {170, 102, 230, 255}, {255, 124, 67, 255},
                                                {239, 239, 239, 255}, {38, 43, 61, 255}}};
const std::array<const char*, 11> kBotNames = {{"AXL", "MIRA", "BYTE", "RUSH", "PIP", "NOVA",
                                                 "SPARK", "SONNY", "JET", "ECHO", "VOLT"}};

struct TrackTheme {
  SDL_Color sky;
  SDL_Color grass;
  SDL_Color asphalt;
  SDL_Color border;
  SDL_Color lane;
  SDL_Color crowd_a;
  SDL_Color crowd_b;
  SDL_Color hud;
};

struct TrackDefinition {
  const char* id;
  const char* name;
  const char* flavor;
  float width;
  int laps;
  std::vector<Vec2> controls;
  std::vector<float> item_box_fractions;
  std::vector<float> boost_pad_fractions;
  TrackTheme theme;
};

struct TrackRuntime {
  const TrackDefinition* definition = nullptr;
  std::vector<Vec2> samples;
  std::vector<float> cumulative;
  float total_length = 1.0f;
};

struct PathSample {
  Vec2 position;
  Vec2 tangent;
  Vec2 normal;
};

struct DriverProfile {
  std::string name = "RYAN";
  int archetype = 0;
  int suit_color = 0;
  int hair_color = 3;
  int kart_color = 1;
};

struct RaceSetup {
  int track_index = 0;
  int field_size_index = 1;
};

struct Racer {
  std::string name;
  int archetype = 0;
  int suit_color = 0;
  int hair_color = 0;
  int kart_color = 0;
  bool is_player = false;
  float distance = 0.0f;
  float lane = 0.0f;
  float lane_velocity = 0.0f;
  float speed = 0.0f;
  float spin_timer = 0.0f;
  float boost_timer = 0.0f;
  float bob_phase = 0.0f;
  float flash_timer = 0.0f;
  float item_cooldown = 0.0f;
  float ai_lane_target = 0.0f;
  ItemType item = ItemType::none;
  bool finished = false;
  int finish_position = 0;
  float finish_time = 0.0f;
  int current_place = 1;
};

struct ItemBox {
  float distance = 0.0f;
  float lane = 0.0f;
  float cooldown = 0.0f;
};

struct BoostPad {
  float distance = 0.0f;
  float lane = 0.0f;
  float radius = 14.0f;
};

struct Hazard {
  bool active = false;
  float distance = 0.0f;
  float lane = 0.0f;
  float ttl = 0.0f;
  int owner = -1;
};

struct Projectile {
  bool active = false;
  float distance = 0.0f;
  float lane = 0.0f;
  float speed = 0.0f;
  float ttl = 0.0f;
  int owner = -1;
};

struct InputFrame {
  pp_input_state held{};
  pp_input_state previous{};
  bool pressed_up = false;
  bool pressed_down = false;
  bool pressed_left = false;
  bool pressed_right = false;
  bool pressed_a = false;
  bool pressed_b = false;
  bool pressed_start = false;
  bool pressed_select = false;
};

struct GameState {
  Screen screen = Screen::title;
  DriverProfile profile;
  RaceSetup setup;
  std::vector<TrackDefinition> tracks;
  TrackRuntime active_track;
  std::vector<Racer> racers;
  std::vector<ItemBox> item_boxes;
  std::vector<BoostPad> boost_pads;
  std::array<Hazard, 24> hazards{};
  std::array<Projectile, 12> projectiles{};
  int title_selection = 0;
  int garage_selection = 0;
  int setup_selection = 0;
  int keyboard_selection = 0;
  std::string draft_name;
  float race_clock = 0.0f;
  float countdown = 3.2f;
  int next_finish_position = 1;
  float results_timer = 0.0f;
  bool pause = false;
};

const std::array<int, 3> kFieldSizes = {{4, 8, 12}};
const std::array<const char*, 40> kKeyboardCells = {{"A", "B", "C", "D", "E", "F", "G", "H",
                                                     "I", "J", "K", "L", "M", "N", "O", "P",
                                                     "Q", "R", "S", "T", "U", "V", "W", "X",
                                                     "Y", "Z", "0", "1", "2", "3", "4", "5",
                                                     "6", "7", "8", "9", "-", "SP", "DEL", "DONE"}};

Vec2 catmull(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  return {(0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                   (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                   (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3)),
          (0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                   (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                   (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3))};
}

std::vector<TrackDefinition> make_tracks() {
  return {{"starter-circuit",
           "STARTER CIRCUIT",
           "A bright coastal loop with forgiving corners and long boost lanes.",
           42.0f,
           3,
           {{96.0f, 120.0f}, {168.0f, 78.0f}, {266.0f, 66.0f}, {372.0f, 90.0f}, {430.0f, 156.0f},
            {428.0f, 246.0f}, {364.0f, 322.0f}, {258.0f, 340.0f}, {150.0f, 316.0f}, {92.0f, 244.0f},
            {82.0f, 168.0f}},
           {0.10f, 0.33f, 0.58f, 0.82f},
           {0.19f, 0.67f},
           {{112, 208, 255, 255}, {84, 198, 112, 255}, {79, 83, 98, 255}, {231, 235, 237, 255},
            {255, 241, 158, 255}, {252, 132, 83, 255}, {72, 128, 233, 255}, {17, 38, 74, 255}}},
          {"harbor-run",
           "HARBOR RUN",
           "Crate-lined dock lanes and one brutal hairpin made for tire traps.",
           40.0f,
           3,
           {{116.0f, 106.0f}, {212.0f, 86.0f}, {344.0f, 94.0f}, {416.0f, 148.0f}, {402.0f, 222.0f},
            {348.0f, 286.0f}, {282.0f, 310.0f}, {214.0f, 302.0f}, {138.0f, 268.0f}, {110.0f, 212.0f},
            {138.0f, 170.0f}, {222.0f, 178.0f}, {310.0f, 194.0f}, {328.0f, 150.0f}, {260.0f, 124.0f}},
           {0.16f, 0.39f, 0.60f, 0.86f},
           {0.27f, 0.74f},
           {{144, 196, 255, 255}, {78, 140, 92, 255}, {70, 73, 87, 255}, {250, 250, 244, 255},
            {255, 221, 120, 255}, {241, 104, 69, 255}, {89, 171, 239, 255}, {15, 33, 61, 255}}},
          {"neon-yard",
           "NEON YARD",
           "An after-dark freight yard with tight rhythm turns and bold color.",
           38.0f,
           3,
           {{120.0f, 112.0f}, {186.0f, 78.0f}, {282.0f, 84.0f}, {360.0f, 120.0f}, {398.0f, 182.0f},
            {388.0f, 252.0f}, {334.0f, 304.0f}, {246.0f, 320.0f}, {164.0f, 294.0f}, {132.0f, 236.0f},
            {156.0f, 196.0f}, {236.0f, 206.0f}, {304.0f, 226.0f}, {304.0f, 276.0f}, {244.0f, 284.0f},
            {182.0f, 254.0f}, {176.0f, 198.0f}, {226.0f, 144.0f}},
           {0.12f, 0.30f, 0.55f, 0.78f},
           {0.21f, 0.47f, 0.73f},
           {{74, 86, 162, 255}, {53, 57, 93, 255}, {62, 66, 84, 255}, {244, 227, 250, 255},
            {246, 224, 120, 255}, {245, 98, 180, 255}, {86, 235, 216, 255}, {237, 230, 255, 255}}}};
}

TrackRuntime build_runtime(const TrackDefinition& definition) {
  TrackRuntime runtime;
  runtime.definition = &definition;
  const int point_count = static_cast<int>(definition.controls.size());
  for (int index = 0; index < point_count; ++index) {
    const Vec2 p0 = definition.controls[(index - 1 + point_count) % point_count];
    const Vec2 p1 = definition.controls[index];
    const Vec2 p2 = definition.controls[(index + 1) % point_count];
    const Vec2 p3 = definition.controls[(index + 2) % point_count];
    for (int step = 0; step < 20; ++step) {
      const float t = static_cast<float>(step) / 20.0f;
      const Vec2 point = catmull(p0, p1, p2, p3, t);
      if (runtime.samples.empty() || length(runtime.samples.back() - point) > 1.5f) {
        runtime.samples.push_back(point);
      }
    }
  }
  if (!runtime.samples.empty()) {
    runtime.samples.push_back(runtime.samples.front());
  }
  runtime.cumulative.push_back(0.0f);
  for (std::size_t index = 1; index < runtime.samples.size(); ++index) {
    runtime.cumulative.push_back(runtime.cumulative.back() +
                                 length(runtime.samples[index] - runtime.samples[index - 1]));
  }
  runtime.total_length = runtime.cumulative.empty() ? 1.0f : runtime.cumulative.back();
  return runtime;
}

PathSample sample_track(const TrackRuntime& track, float distance) {
  PathSample result{};
  if (track.samples.size() < 2) {
    return result;
  }
  const float wrapped = wrap_value(distance, track.total_length);
  const auto upper = std::upper_bound(track.cumulative.begin(), track.cumulative.end(), wrapped);
  std::size_t segment = 0;
  if (upper != track.cumulative.begin()) {
    segment = static_cast<std::size_t>((upper - track.cumulative.begin()) - 1);
  }
  if (segment + 1 >= track.samples.size()) {
    segment = track.samples.size() - 2;
  }
  const float start_distance = track.cumulative[segment];
  const float segment_length = std::max(0.001f, track.cumulative[segment + 1] - start_distance);
  const float local_t = (wrapped - start_distance) / segment_length;
  const Vec2 start = track.samples[segment];
  const Vec2 end = track.samples[segment + 1];
  result.position = start + (end - start) * local_t;
  result.tangent = normalize(end - start);
  result.normal = {-result.tangent.y, result.tangent.x};
  return result;
}

char path_separator() { return GRAND_PRIX_PATH_SEPARATOR; }

std::string join_path(const char* root, const char* leaf) {
  std::ostringstream builder;
  builder << root << path_separator() << leaf;
  return builder.str();
}

void save_profile(const pp_context& context, const DriverProfile& profile) {
  std::ofstream out(join_path(pp_get_config_dir(&context), "profile.txt"));
  if (!out.is_open()) {
    return;
  }
  out << profile.name << "\n";
  out << profile.archetype << "\n";
  out << profile.suit_color << "\n";
  out << profile.hair_color << "\n";
  out << profile.kart_color << "\n";
}

void load_profile(const pp_context& context, DriverProfile& profile) {
  std::ifstream in(join_path(pp_get_config_dir(&context), "profile.txt"));
  if (!in.is_open()) {
    return;
  }
  std::getline(in, profile.name);
  if (profile.name.empty()) {
    profile.name = "RYAN";
  }
  in >> profile.archetype;
  in >> profile.suit_color;
  in >> profile.hair_color;
  in >> profile.kart_color;
  profile.archetype = std::clamp(profile.archetype, 0, static_cast<int>(kArchetypes.size()) - 1);
  profile.suit_color = std::clamp(profile.suit_color, 0, static_cast<int>(kSuitColors.size()) - 1);
  profile.hair_color = std::clamp(profile.hair_color, 0, static_cast<int>(kHairColors.size()) - 1);
  profile.kart_color = std::clamp(profile.kart_color, 0, static_cast<int>(kKartColors.size()) - 1);
  if (static_cast<int>(profile.name.size()) > kMaxNameLength) {
    profile.name.resize(kMaxNameLength);
  }
}

InputFrame read_input(pp_context& context, const InputFrame& previous_frame) {
  InputFrame frame;
  frame.previous = previous_frame.held;
  pp_poll_input(&context, &frame.held);
  frame.pressed_up = frame.held.up && !frame.previous.up;
  frame.pressed_down = frame.held.down && !frame.previous.down;
  frame.pressed_left = frame.held.left && !frame.previous.left;
  frame.pressed_right = frame.held.right && !frame.previous.right;
  frame.pressed_a = frame.held.a && !frame.previous.a;
  frame.pressed_b = frame.held.b && !frame.previous.b;
  frame.pressed_start = frame.held.start && !frame.previous.start;
  frame.pressed_select = frame.held.select && !frame.previous.select;
  return frame;
}

std::string item_name(ItemType item) {
  switch (item) {
    case ItemType::tire:
      return "TIRE";
    case ItemType::oil:
      return "OIL";
    case ItemType::nos:
      return "NOS";
    case ItemType::none:
    default:
      return "NONE";
  }
}

int random_int(int low, int high) { return low + (std::rand() % (high - low + 1)); }

ItemType random_item() {
  const int roll = random_int(0, 99);
  if (roll < 34) {
    return ItemType::tire;
  }
  if (roll < 67) {
    return ItemType::oil;
  }
  return ItemType::nos;
}

void assign_grid(std::vector<Racer>& racers) {
  const std::array<float, 4> start_lanes = {{-14.0f, 14.0f, -6.0f, 6.0f}};
  for (std::size_t index = 0; index < racers.size(); ++index) {
    racers[index].distance = -static_cast<float>(index) * 18.0f;
    racers[index].lane = start_lanes[index % start_lanes.size()];
    racers[index].lane_velocity = 0.0f;
    racers[index].speed = 0.0f;
    racers[index].spin_timer = 0.0f;
    racers[index].boost_timer = 0.0f;
    racers[index].flash_timer = 0.0f;
    racers[index].item = ItemType::none;
    racers[index].item_cooldown = 1.2f + static_cast<float>(index) * 0.1f;
    racers[index].finished = false;
    racers[index].finish_position = 0;
    racers[index].finish_time = 0.0f;
    racers[index].current_place = static_cast<int>(index + 1);
    racers[index].ai_lane_target = racers[index].lane;
    racers[index].bob_phase = static_cast<float>(index) * 0.7f;
  }
}

void start_race(GameState& game) {
  game.active_track = build_runtime(game.tracks[game.setup.track_index]);
  game.racers.clear();
  game.item_boxes.clear();
  game.boost_pads.clear();
  game.hazards.fill({});
  game.projectiles.fill({});
  game.race_clock = 0.0f;
  game.countdown = 3.2f;
  game.results_timer = 0.0f;
  game.next_finish_position = 1;
  game.pause = false;

  Racer player;
  player.name = game.profile.name;
  player.archetype = game.profile.archetype;
  player.suit_color = game.profile.suit_color;
  player.hair_color = game.profile.hair_color;
  player.kart_color = game.profile.kart_color;
  player.is_player = true;
  game.racers.push_back(player);

  const int total_racers = kFieldSizes[game.setup.field_size_index];
  for (int index = 1; index < total_racers; ++index) {
    Racer bot;
    bot.name = kBotNames[(index - 1) % kBotNames.size()];
    bot.archetype = index % static_cast<int>(kArchetypes.size());
    bot.suit_color = (index + 1) % static_cast<int>(kSuitColors.size());
    bot.hair_color = (index + 2) % static_cast<int>(kHairColors.size());
    bot.kart_color = (index + 3) % static_cast<int>(kKartColors.size());
    game.racers.push_back(bot);
  }

  assign_grid(game.racers);

  for (float fraction : game.tracks[game.setup.track_index].item_box_fractions) {
    for (float lane : std::array<float, 3>{{-18.0f, 0.0f, 18.0f}}) {
      game.item_boxes.push_back({game.active_track.total_length * fraction, lane, 0.0f});
    }
  }
  for (float fraction : game.tracks[game.setup.track_index].boost_pad_fractions) {
    for (float lane : std::array<float, 2>{{-10.0f, 10.0f}}) {
      game.boost_pads.push_back({game.active_track.total_length * fraction, lane, 14.0f});
    }
  }
  game.screen = Screen::racing;
}

void draw_driver_preview(SDL_Renderer* renderer,
                         const DriverProfile& profile,
                         int center_x,
                         int center_y,
                         int scale,
                         bool facing_right) {
  const SDL_Color suit = kSuitColors[profile.suit_color];
  const SDL_Color hair = kHairColors[profile.hair_color];
  const SDL_Color kart = kKartColors[profile.kart_color];
  const SDL_Color outline{20, 24, 31, 255};
  const SDL_Color skin{245, 208, 172, 255};
  const SDL_Color visor{195, 233, 250, 255};
  const SDL_Color jacket_shadow = darken(suit, 28);
  const SDL_Color kart_shadow = darken(kart, 34);
  const SDL_Color glove{255, 205, 92, 255};
  const int direction = facing_right ? 1 : -1;
  const int u = scale / 3;

  fill_rect(renderer, {center_x - 22 * u, center_y + 14 * u, 44 * u, 8 * u}, kart_shadow);
  fill_circle(renderer, center_x, center_y + 12 * u, 20 * u, kart);
  fill_rect(renderer, {center_x - 22 * u, center_y - 2 * u, 44 * u, 18 * u}, kart);
  fill_rect(renderer, {center_x - 16 * u, center_y + 2 * u, 32 * u, 10 * u}, kart_shadow);
  fill_rect(renderer, {center_x - 8 * u, center_y - 6 * u, 18 * u, 10 * u}, visor);
  fill_circle(renderer, center_x - 14 * u, center_y + 20 * u, 7 * u, outline);
  fill_circle(renderer, center_x + 14 * u, center_y + 20 * u, 7 * u, outline);
  fill_circle(renderer, center_x - 14 * u, center_y + 20 * u, 3 * u, lighten(outline, 20));
  fill_circle(renderer, center_x + 14 * u, center_y + 20 * u, 3 * u, lighten(outline, 20));

  fill_rect(renderer, {center_x - 11 * u, center_y - 18 * u, 22 * u, 20 * u}, jacket_shadow);
  fill_rect(renderer, {center_x - 13 * u, center_y - 20 * u, 26 * u, 18 * u}, suit);
  fill_rect(renderer, {center_x - 16 * u, center_y - 16 * u, 6 * u, 14 * u}, suit);
  fill_rect(renderer, {center_x + 10 * u, center_y - 16 * u, 6 * u, 14 * u}, suit);
  fill_rect(renderer, {center_x - 7 * u, center_y - 12 * u, 14 * u, 8 * u}, skin);
  fill_rect(renderer, {center_x - 4 * u, center_y - 2 * u, 8 * u, 10 * u}, skin);
  fill_circle(renderer, center_x + direction * 2 * u, center_y - 28 * u, 11 * u, skin);
  fill_circle(renderer, center_x + direction * 1 * u, center_y - 34 * u, 12 * u, hair);
  fill_rect(renderer, {center_x - 9 * u, center_y - 34 * u, 18 * u, 6 * u}, hair);
  fill_rect(renderer, {center_x - 7 * u, center_y - 28 * u, 14 * u, 5 * u}, visor);
  fill_rect(renderer, {center_x + direction * 2 * u, center_y - 18 * u, 5 * u, 2 * u}, outline);
  fill_rect(renderer, {center_x + direction * 7 * u, center_y - 2 * u, 12 * u, 4 * u}, glove);
  fill_rect(renderer, {center_x + direction * 16 * u, center_y - 3 * u, 4 * u, 6 * u}, outline);
}

void draw_track_preview(SDL_Renderer* renderer, const TrackRuntime& runtime, SDL_Rect bounds, SDL_Color color) {
  float min_x = runtime.samples.front().x;
  float max_x = runtime.samples.front().x;
  float min_y = runtime.samples.front().y;
  float max_y = runtime.samples.front().y;
  for (const Vec2& point : runtime.samples) {
    min_x = std::min(min_x, point.x);
    max_x = std::max(max_x, point.x);
    min_y = std::min(min_y, point.y);
    max_y = std::max(max_y, point.y);
  }
  const float span_x = std::max(1.0f, max_x - min_x);
  const float span_y = std::max(1.0f, max_y - min_y);
  const float scale = std::min((bounds.w - 18.0f) / span_x, (bounds.h - 18.0f) / span_y);
  Vec2 previous{};
  bool has_previous = false;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (std::size_t index = 0; index < runtime.samples.size(); index += 3) {
    const Vec2 point = runtime.samples[index];
    const int draw_x = bounds.x + 9 + static_cast<int>((point.x - min_x) * scale);
    const int draw_y = bounds.y + 9 + static_cast<int>((point.y - min_y) * scale);
    if (has_previous) {
      SDL_RenderDrawLine(renderer, static_cast<int>(previous.x), static_cast<int>(previous.y), draw_x, draw_y);
    }
    previous = {static_cast<float>(draw_x), static_cast<float>(draw_y)};
    has_previous = true;
  }
}

void draw_title(SDL_Renderer* renderer, const GameState& game, float ticks) {
  const SDL_Color navy{20, 32, 61, 255};
  const SDL_Color cream{250, 241, 213, 255};
  const SDL_Color gold{249, 198, 82, 255};
  const SDL_Color coral{244, 115, 91, 255};
  const SDL_Color teal{76, 191, 214, 255};
  const std::array<const char*, 3> options = {{"RACE", "GARAGE", "QUIT"}};

  fill_rect(renderer, {0, 0, kWindowWidth, kWindowHeight}, navy);
  fill_rect(renderer, {0, 320, kWindowWidth, 192}, {37, 77, 52, 255});
  for (int stripe = 0; stripe < 9; ++stripe) {
    const int x = stripe * 64 + static_cast<int>(ticks * 28.0f) % 64 - 24;
    fill_rect(renderer, {x, 336, 28, 176}, {44, 94, 61, 255});
  }
  fill_rect(renderer, {0, 270, 512, 30}, cream);
  for (int band = 0; band < 16; ++band) {
    const SDL_Color sample = (band % 2 == 0) ? coral : gold;
    fill_rect(renderer, {band * 32, 270, 32, 30}, sample);
  }

  draw_text(renderer, "GRAND", 256, 60, 7, cream, true);
  draw_text(renderer, "PRIX", 256, 128, 8, gold, true);

  DriverProfile preview = game.profile;
  preview.kart_color = (static_cast<int>(ticks * 2.0f) % static_cast<int>(kKartColors.size()));
  draw_driver_preview(renderer, preview, 116 + static_cast<int>(std::sin(ticks * 2.4f) * 10.0f), 284, 5, true);
  draw_driver_preview(renderer, preview, 396 - static_cast<int>(std::sin(ticks * 2.1f) * 8.0f), 302, 4, false);

  int option_y = 328;
  for (int index = 0; index < static_cast<int>(options.size()); ++index) {
    const bool selected = index == game.title_selection;
    SDL_Rect card{116, option_y, 280, 38};
    fill_rect(renderer, card, selected ? gold : cream);
    draw_rect(renderer, card, navy);
    draw_text(renderer, options[index], 256, option_y + 12, 3, selected ? navy : coral, true);
    option_y += 48;
  }
}

void draw_garage(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Color bg{24, 44, 78, 255};
  const SDL_Color panel{239, 235, 219, 255};
  const SDL_Color accent{255, 204, 88, 255};
  const SDL_Color text{26, 34, 56, 255};
  const SDL_Color muted{82, 90, 112, 255};
  const std::array<const char*, 5> labels = {{"NAME", "DRIVER", "SUIT", "HAIR", "KART"}};

  fill_rect(renderer, {0, 0, 512, 512}, bg);
  fill_rect(renderer, {28, 28, 456, 456}, panel);
  fill_rect(renderer, {28, 28, 456, 54}, accent);
  draw_text(renderer, "GARAGE", 256, 44, 4, text, true);
  draw_text(renderer, "BUILD YOUR DRIVER", 256, 100, 2, muted, true);

  draw_driver_preview(renderer, game.profile, 126, 224, 7, true);
  draw_text(renderer, game.profile.name, 126, 314, 3, text, true);
  draw_text(renderer, kArchetypes[game.profile.archetype].name, 126, 340, 2,
            kArchetypes[game.profile.archetype].accent, true);

  int row_y = 144;
  for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
    const bool selected = index == game.garage_selection;
    SDL_Rect row{220, row_y, 230, 42};
    fill_rect(renderer, row, selected ? lighten(accent, 24) : SDL_Color{224, 220, 206, 255});
    draw_rect(renderer, row, text);
    draw_text(renderer, labels[index], 238, row_y + 14, 2, text, false);

    std::string value;
    if (index == 0) {
      value = game.profile.name;
    } else if (index == 1) {
      value = kArchetypes[game.profile.archetype].name;
    } else if (index == 2) {
      value = "COLOR " + std::to_string(game.profile.suit_color + 1);
    } else if (index == 3) {
      value = "COLOR " + std::to_string(game.profile.hair_color + 1);
    } else if (index == 4) {
      value = "COLOR " + std::to_string(game.profile.kart_color + 1);
    }
    draw_text_right(renderer, value, 430, row_y + 14, 2, selected ? text : muted);
    row_y += 52;
  }

  draw_text(renderer, "A EDIT / LEFT RIGHT TUNE / B BACK", 256, 456, 2, text, true);
}

void draw_name_entry(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Color bg{36, 35, 64, 255};
  const SDL_Color panel{231, 226, 214, 255};
  const SDL_Color text{28, 32, 55, 255};
  const SDL_Color accent{255, 196, 81, 255};
  const SDL_Color muted{109, 96, 137, 255};

  fill_rect(renderer, {0, 0, 512, 512}, bg);
  fill_rect(renderer, {32, 32, 448, 448}, panel);
  draw_text(renderer, "NAME ENTRY", 256, 52, 4, text, true);
  draw_text(renderer, "LIMITED CONTROLS, ON SCREEN KEYS", 256, 86, 2, muted, true);

  fill_rect(renderer, {78, 118, 356, 46}, {245, 242, 235, 255});
  draw_rect(renderer, {78, 118, 356, 46}, text);
  draw_text(renderer, game.draft_name.empty() ? "DRIVER" : game.draft_name, 256, 133, 3, text, true);

  for (int cell = 0; cell < static_cast<int>(kKeyboardCells.size()); ++cell) {
    const int row = cell / 8;
    const int column = cell % 8;
    const bool selected = cell == game.keyboard_selection;
    SDL_Rect key{56 + column * 50, 192 + row * 48, 44, 38};
    fill_rect(renderer, key, selected ? accent : SDL_Color{214, 209, 196, 255});
    draw_rect(renderer, key, text);
    draw_text(renderer, kKeyboardCells[cell], key.x + key.w / 2, key.y + 12, 2, text, true);
  }

  draw_text(renderer, "A PICK / B DELETE / START DONE", 256, 456, 2, text, true);
}

void draw_setup(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Color bg{18, 32, 46, 255};
  const SDL_Color panel{231, 236, 238, 255};
  const SDL_Color text{20, 29, 43, 255};
  const SDL_Color accent{98, 204, 148, 255};
  const SDL_Color muted{82, 100, 122, 255};
  const TrackDefinition& track = game.tracks[game.setup.track_index];
  const TrackRuntime preview = build_runtime(track);

  fill_rect(renderer, {0, 0, 512, 512}, bg);
  fill_rect(renderer, {24, 24, 464, 464}, panel);
  fill_rect(renderer, {24, 24, 464, 58}, accent);
  draw_text(renderer, "RACE SETUP", 256, 42, 4, text, true);

  SDL_Rect preview_box{42, 112, 188, 196};
  fill_rect(renderer, preview_box, {245, 245, 240, 255});
  draw_rect(renderer, preview_box, text);
  draw_track_preview(renderer, preview, preview_box, accent);

  draw_text(renderer, track.name, 356, 118, 3, text, true);
  draw_text(renderer, track.flavor, 356, 150, 1, muted, true);

  const std::array<const char*, 3> labels = {{"TRACK", "FIELD", "START"}};
  int row_y = 222;
  for (int index = 0; index < 3; ++index) {
    const bool selected = index == game.setup_selection;
    SDL_Rect row{252, row_y, 194, 42};
    fill_rect(renderer, row, selected ? lighten(accent, 22) : SDL_Color{214, 221, 224, 255});
    draw_rect(renderer, row, text);
    draw_text(renderer, labels[index], 266, row_y + 14, 2, text, false);
    std::string value;
    if (index == 0) {
      value = std::to_string(game.setup.track_index + 1) + "/3";
    } else if (index == 1) {
      value = std::to_string(kFieldSizes[game.setup.field_size_index]) + " RACERS";
    } else {
      value = "GO";
    }
    draw_text_right(renderer, value, 430, row_y + 14, 2, text);
    row_y += 56;
  }

  draw_text(renderer, "SOLO GRID", 256, 428, 2, muted, true);
  draw_text(renderer, "A START  LEFT RIGHT CHANGE  B BACK", 256, 456, 1, text, true);
}

struct RoadProjection {
  bool visible = false;
  float screen_x = 0.0f;
  float screen_y = 0.0f;
  float half_width = 0.0f;
  float scale = 0.0f;
  float forward = 0.0f;
};

RoadProjection project_lane_point(const GameState& game, float distance, float lane_offset, float lift = 0.0f) {
  RoadProjection projection;
  const PathSample camera = sample_track(game.active_track, game.racers.front().distance + 8.0f);
  const PathSample point = sample_track(game.active_track, distance);
  const Vec2 camera_pos = camera.position - camera.tangent * 10.0f;
  const Vec2 world = point.position + point.normal * lane_offset;
  const Vec2 relative = world - camera_pos;
  const float forward = dot(relative, camera.tangent);
  const float side = dot(relative, camera.normal);
  if (forward <= 2.0f) {
    return projection;
  }
  const float perspective = 310.0f / (1.0f + forward * 0.06f);
  projection.visible = true;
  projection.forward = forward;
  projection.scale = perspective * 0.12f;
  projection.screen_x = 256.0f + side * perspective * 0.018f;
  projection.screen_y = 106.0f + perspective - lift * perspective * 0.02f;
  projection.half_width = game.active_track.definition->width * perspective * 0.013f;
  if (projection.screen_y < -20.0f || projection.screen_y > static_cast<float>(kTrackRenderBottom + 30)) {
    projection.visible = false;
  }
  return projection;
}

void draw_track_surface(SDL_Renderer* renderer, const GameState& game, const TrackTheme& theme, float ticks) {
  std::vector<RoadProjection> road;
  fill_rect(renderer, {0, 0, 512, kTrackRenderBottom}, theme.sky);
  fill_rect(renderer, {0, 154, 512, kTrackRenderBottom - 154}, theme.grass);
  fill_rect(renderer, {0, 154, 512, 6}, lighten(theme.sky, 32));

  for (int hill = 0; hill < 6; ++hill) {
    const int x = hill * 96 - 20 + static_cast<int>(std::sin(ticks * 0.4f + hill) * 8.0f);
    fill_rect(renderer, {x, 126 + (hill % 2) * 10, 110, 44}, (hill % 2 == 0) ? theme.crowd_a : theme.crowd_b);
  }

  for (int slice = 46; slice >= 0; --slice) {
    const float distance = game.racers.front().distance + 18.0f + static_cast<float>(slice) * 7.0f;
    const RoadProjection projection = project_lane_point(game, distance, 0.0f);
    if (projection.visible) {
      road.push_back(projection);
    }
  }

  for (std::size_t index = 0; index + 1 < road.size(); ++index) {
    const RoadProjection& far = road[index];
    const RoadProjection& near = road[index + 1];
    const int y0 = static_cast<int>(far.screen_y);
    const int y1 = static_cast<int>(near.screen_y);
    if (y1 <= y0) {
      continue;
    }
    const SDL_Color road_color = (index % 2 == 0) ? theme.asphalt : darken(theme.asphalt, 10);
    const SDL_Color shoulder = (index % 3 == 0) ? theme.lane : theme.border;
    for (int y = y0; y <= y1; ++y) {
      const float t = static_cast<float>(y - y0) / static_cast<float>(std::max(1, y1 - y0));
      const float center_x = far.screen_x + (near.screen_x - far.screen_x) * t;
      const float half_width = far.half_width + (near.half_width - far.half_width) * t;
      const int road_left = static_cast<int>(center_x - half_width);
      const int road_right = static_cast<int>(center_x + half_width);
      const int border = std::max(2, static_cast<int>(half_width * 0.18f));
      SDL_SetRenderDrawColor(renderer, shoulder.r, shoulder.g, shoulder.b, shoulder.a);
      SDL_RenderDrawLine(renderer, road_left - border, y, road_left, y);
      SDL_RenderDrawLine(renderer, road_right, y, road_right + border, y);
      SDL_SetRenderDrawColor(renderer, road_color.r, road_color.g, road_color.b, road_color.a);
      SDL_RenderDrawLine(renderer, road_left, y, road_right, y);
      if (index % 4 == 0 && y % 6 < 3) {
        const int lane_half = std::max(2, static_cast<int>(half_width * 0.10f));
        SDL_SetRenderDrawColor(renderer, 255, 245, 198, 255);
        SDL_RenderDrawLine(renderer, static_cast<int>(center_x) - lane_half, y,
                           static_cast<int>(center_x) + lane_half, y);
      }
    }
  }
}

void draw_item_boxes(SDL_Renderer* renderer, const GameState& game, float ticks) {
  for (const ItemBox& box : game.item_boxes) {
    if (box.cooldown > 0.0f) {
      continue;
    }
    const RoadProjection projection = project_lane_point(game, box.distance, box.lane, 18.0f);
    if (!projection.visible || projection.forward > 270.0f) {
      continue;
    }
    const int size = std::max(8, static_cast<int>(projection.scale * 0.8f));
    const int pulse = (static_cast<int>(ticks * 8.0f + box.distance) % 2 == 0) ? 0 : 2;
    fill_rect(renderer,
              {static_cast<int>(projection.screen_x) - size - pulse, static_cast<int>(projection.screen_y) - size,
               (size + pulse) * 2, size * 2},
              {255, 236, 129, 255});
    draw_rect(renderer,
              {static_cast<int>(projection.screen_x) - size - pulse, static_cast<int>(projection.screen_y) - size,
               (size + pulse) * 2, size * 2},
              {26, 36, 59, 255});
    draw_text(renderer, "?", static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y) - size / 2,
              1 + size / 10, {26, 36, 59, 255}, true);
  }
}

void draw_boost_pads(SDL_Renderer* renderer, const GameState& game, float ticks) {
  for (const BoostPad& pad : game.boost_pads) {
    const RoadProjection projection = project_lane_point(game, pad.distance, pad.lane);
    if (!projection.visible || projection.forward > 240.0f) {
      continue;
    }
    const int width = std::max(8, static_cast<int>(projection.half_width * 0.5f));
    const int height = std::max(4, static_cast<int>(projection.scale * 0.16f));
    const SDL_Color color = (static_cast<int>(ticks * 8.0f + pad.distance) % 2 == 0)
                                ? SDL_Color{87, 243, 225, 255}
                                : SDL_Color{255, 255, 255, 255};
    fill_rect(renderer, {static_cast<int>(projection.screen_x) - width, static_cast<int>(projection.screen_y) - height,
                         width * 2, height * 2},
              color);
    fill_rect(renderer, {static_cast<int>(projection.screen_x) - width + 3,
                         static_cast<int>(projection.screen_y) - height + 2, width * 2 - 6, height * 2 - 4},
              {22, 92, 96, 255});
  }
}

void draw_hazards(SDL_Renderer* renderer, const GameState& game) {
  for (const Hazard& hazard : game.hazards) {
    if (!hazard.active) {
      continue;
    }
    const RoadProjection projection = project_lane_point(game, hazard.distance, hazard.lane);
    if (!projection.visible || projection.forward > 220.0f) {
      continue;
    }
    const int radius = std::max(5, static_cast<int>(projection.scale * 0.28f));
    fill_circle(renderer, static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y), radius,
                {29, 28, 33, 255});
    fill_circle(renderer, static_cast<int>(projection.screen_x) + radius / 2,
                static_cast<int>(projection.screen_y) + radius / 3, radius / 2, {52, 52, 60, 255});
  }
}

void draw_projectiles(SDL_Renderer* renderer, const GameState& game, float ticks) {
  for (const Projectile& projectile : game.projectiles) {
    if (!projectile.active) {
      continue;
    }
    const RoadProjection projection = project_lane_point(game, projectile.distance, projectile.lane,
                                                         std::sin(ticks * 10.0f + projectile.distance) * 5.0f);
    if (!projection.visible || projection.forward > 250.0f) {
      continue;
    }
    const int radius = std::max(5, static_cast<int>(projection.scale * 0.24f));
    fill_circle(renderer, static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y), radius,
                {41, 34, 34, 255});
    fill_circle(renderer, static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y), radius / 2,
                {171, 157, 130, 255});
  }
}

void draw_racer(SDL_Renderer* renderer, const GameState& game, const Racer& racer) {
  const RoadProjection projection = project_lane_point(game, racer.distance, racer.lane, 20.0f);
  const SDL_Color kart = racer.flash_timer > 0.0f ? lighten(kKartColors[racer.kart_color], 64)
                                                  : kKartColors[racer.kart_color];
  const SDL_Color kart_shadow = darken(kart, 38);
  const SDL_Color suit = kSuitColors[racer.suit_color];
  const SDL_Color hair = kHairColors[racer.hair_color];
  const SDL_Color skin{245, 208, 172, 255};
  const int body_w = std::max(8, static_cast<int>(projection.scale * 0.95f));
  const int body_h = std::max(10, static_cast<int>(projection.scale * 1.15f));
  const int px = static_cast<int>(projection.screen_x);
  const int py = static_cast<int>(projection.screen_y);
  if (!projection.visible || projection.forward > 250.0f || racer.is_player) {
    return;
  }
  fill_rect(renderer, {px - body_w / 2, py, body_w, body_h}, kart);
  fill_rect(renderer, {px - body_w / 3, py + body_h / 5, body_w * 2 / 3, body_h / 2}, kart_shadow);
  fill_rect(renderer, {px - body_w / 4, py - body_h / 3, body_w / 2, body_h / 2}, suit);
  fill_circle(renderer, px, py - body_h / 2, std::max(4, body_w / 4), skin);
  fill_circle(renderer, px, py - body_h / 2 - std::max(2, body_w / 6), std::max(4, body_w / 4), hair);
  fill_rect(renderer, {px - body_w / 2 - 2, py + body_h / 2, std::max(3, body_w / 5), std::max(4, body_h / 4)},
            {20, 24, 31, 255});
  fill_rect(renderer, {px + body_w / 2 - std::max(3, body_w / 5), py + body_h / 2, std::max(3, body_w / 5),
                       std::max(4, body_h / 4)},
            {20, 24, 31, 255});
}

SDL_Color gp_lerp_color(SDL_Color left, SDL_Color right, float t) {
  t = clampf(t, 0.0f, 1.0f);
  const auto mix = [t](Uint8 a, Uint8 b) {
    return static_cast<Uint8>(a + static_cast<int>((static_cast<int>(b) - static_cast<int>(a)) * t));
  };
  return {mix(left.r, right.r), mix(left.g, right.g), mix(left.b, right.b), mix(left.a, right.a)};
}

void gp_fill_triangle(SDL_Renderer* renderer, Vec2 a, Vec2 b, Vec2 c, SDL_Color color) {
  std::array<Vec2, 3> points{{a, b, c}};
  std::sort(points.begin(), points.end(), [](Vec2 left, Vec2 right) { return left.y < right.y; });
  a = points[0];
  b = points[1];
  c = points[2];
  if (std::fabs(c.y - a.y) < 0.001f) {
    return;
  }
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  const auto sample_x = [](Vec2 left, Vec2 right, float y) {
    if (std::fabs(right.y - left.y) < 0.001f) {
      return left.x;
    }
    const float t = (y - left.y) / (right.y - left.y);
    return left.x + (right.x - left.x) * t;
  };
  const int y_start = std::max(0, static_cast<int>(std::floor(a.y)));
  const int y_end = std::min(kWindowHeight - 1, static_cast<int>(std::ceil(c.y)));
  for (int y = y_start; y <= y_end; ++y) {
    const float sample_y = static_cast<float>(y) + 0.5f;
    if (sample_y < a.y || sample_y > c.y) {
      continue;
    }
    const bool upper = sample_y < b.y || std::fabs(b.y - a.y) < 0.001f;
    float left_x = sample_x(a, c, sample_y);
    float right_x = upper ? sample_x(a, b, sample_y) : sample_x(b, c, sample_y);
    if (left_x > right_x) {
      std::swap(left_x, right_x);
    }
    SDL_RenderDrawLine(renderer, static_cast<int>(std::floor(left_x)), y, static_cast<int>(std::ceil(right_x)), y);
  }
}

void gp_fill_quad(SDL_Renderer* renderer, Vec2 a, Vec2 b, Vec2 c, Vec2 d, SDL_Color color) {
  gp_fill_triangle(renderer, a, b, c, color);
  gp_fill_triangle(renderer, a, c, d, color);
}

struct GpCamera3D {
  Vec3 position;
  Vec3 forward;
  Vec3 right;
  Vec3 up;
};

struct GpProjection {
  bool visible = false;
  float screen_x = 0.0f;
  float screen_y = 0.0f;
  float scale = 0.0f;
  float depth = 0.0f;
};

float gp_track_height(const GameState& game, float distance) {
  switch (game.setup.track_index) {
    case 0:
      return std::sin(distance * 0.015f) * 10.0f + std::cos(distance * 0.008f + 0.8f) * 4.0f;
    case 1:
      return std::sin(distance * 0.011f + 1.4f) * 8.0f + std::cos(distance * 0.021f) * 3.0f;
    case 2:
    default:
      return std::sin(distance * 0.020f + 0.5f) * 7.0f + std::cos(distance * 0.034f) * 4.0f;
  }
}

Vec3 gp_track_world_point(const GameState& game, float distance, float lane_offset, float lift = 0.0f) {
  const PathSample sample = sample_track(game.active_track, distance);
  const Vec2 point = sample.position + sample.normal * lane_offset;
  return {point.x, gp_track_height(game, distance) + lift, point.y};
}

GpCamera3D gp_make_camera(const GameState& game) {
  const Racer& player = game.racers.front();
  const PathSample player_path = sample_track(game.active_track, player.distance);
  const Vec3 player_world = gp_track_world_point(game, player.distance, player.lane, 2.0f);
  const Vec3 focus =
      gp_track_world_point(game, player.distance + 40.0f + player.speed * 0.18f, player.lane * 0.24f, 2.0f);
  const Vec3 world_up{0.0f, 1.0f, 0.0f};

  GpCamera3D camera;
  camera.position =
      player_world - Vec3{player_path.tangent.x, 0.0f, player_path.tangent.y} * 18.0f + world_up * 15.0f;
  if (game.countdown > 0.0f) {
    camera.position = camera.position + world_up * 2.5f;
  }
  camera.forward = normalize(focus - camera.position);
  camera.right = normalize(cross(camera.forward, world_up));
  if (length(camera.right) < 0.001f) {
    camera.right = {1.0f, 0.0f, 0.0f};
  }
  camera.up = normalize(cross(camera.right, camera.forward));
  return camera;
}

GpProjection gp_project_world_point(const GpCamera3D& camera, Vec3 world) {
  GpProjection projection;
  const Vec3 relative = world - camera.position;
  const float side = dot(relative, camera.right);
  const float height = dot(relative, camera.up);
  const float forward = dot(relative, camera.forward);
  if (forward <= 1.0f) {
    return projection;
  }
  constexpr float kFocalLength = 360.0f;
  projection.visible = true;
  projection.depth = forward;
  projection.scale = kFocalLength / forward;
  projection.screen_x = (kWindowWidth * 0.5f) + side * projection.scale;
  projection.screen_y = 220.0f - height * projection.scale;
  if (projection.screen_x < -260.0f || projection.screen_x > kWindowWidth + 260.0f ||
      projection.screen_y < -220.0f || projection.screen_y > kWindowHeight + 220.0f) {
    projection.visible = false;
  }
  return projection;
}

void gp_draw_billboard_quad(SDL_Renderer* renderer,
                            const GpCamera3D& camera,
                            Vec3 center,
                            float width,
                            float height,
                            SDL_Color fill,
                            SDL_Color outline) {
  const Vec3 half_right = camera.right * (width * 0.5f);
  const Vec3 world_up{0.0f, height, 0.0f};
  const GpProjection bl = gp_project_world_point(camera, center - half_right);
  const GpProjection br = gp_project_world_point(camera, center + half_right);
  const GpProjection tr = gp_project_world_point(camera, center + half_right + world_up);
  const GpProjection tl = gp_project_world_point(camera, center - half_right + world_up);
  if (!bl.visible || !br.visible || !tr.visible || !tl.visible) {
    return;
  }
  gp_fill_quad(renderer, {bl.screen_x, bl.screen_y}, {br.screen_x, br.screen_y}, {tr.screen_x, tr.screen_y},
               {tl.screen_x, tl.screen_y}, fill);
  SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, outline.a);
  SDL_RenderDrawLine(renderer, static_cast<int>(bl.screen_x), static_cast<int>(bl.screen_y),
                     static_cast<int>(br.screen_x), static_cast<int>(br.screen_y));
  SDL_RenderDrawLine(renderer, static_cast<int>(br.screen_x), static_cast<int>(br.screen_y),
                     static_cast<int>(tr.screen_x), static_cast<int>(tr.screen_y));
  SDL_RenderDrawLine(renderer, static_cast<int>(tr.screen_x), static_cast<int>(tr.screen_y),
                     static_cast<int>(tl.screen_x), static_cast<int>(tl.screen_y));
  SDL_RenderDrawLine(renderer, static_cast<int>(tl.screen_x), static_cast<int>(tl.screen_y),
                     static_cast<int>(bl.screen_x), static_cast<int>(bl.screen_y));
}

void gp_draw_track_surface(SDL_Renderer* renderer,
                           const GameState& game,
                           const TrackTheme& theme,
                           const GpCamera3D& camera,
                           float ticks) {
  for (int band = 0; band < 10; ++band) {
    const float t = static_cast<float>(band) / 9.0f;
    const SDL_Color stripe = gp_lerp_color(lighten(theme.sky, 18), darken(theme.sky, 26), t);
    fill_rect(renderer, {0, band * 20, kWindowWidth, 22}, stripe);
  }
  fill_rect(renderer, {0, 200, kWindowWidth, kTrackRenderBottom - 200}, theme.grass);
  for (int ridge = 0; ridge < 7; ++ridge) {
    const int width = 110 + (ridge % 3) * 20;
    const int x = ridge * 82 - 30 + static_cast<int>(std::sin(ticks * 0.4f + ridge) * 10.0f);
    const int y = 152 + (ridge % 2) * 12;
    fill_rect(renderer, {x, y, width, 56}, ridge % 2 == 0 ? theme.crowd_a : theme.crowd_b);
  }

  const float track_width = game.active_track.definition->width;
  for (int slice = 72; slice >= 0; --slice) {
    const float near_distance = game.racers.front().distance + 8.0f + static_cast<float>(slice) * 5.5f;
    const float far_distance = near_distance + 5.5f;
    const Vec3 near_left_outer = gp_track_world_point(game, near_distance, -(track_width * 0.5f + 7.0f));
    const Vec3 near_left = gp_track_world_point(game, near_distance, -track_width * 0.5f);
    const Vec3 near_right = gp_track_world_point(game, near_distance, track_width * 0.5f);
    const Vec3 near_right_outer = gp_track_world_point(game, near_distance, track_width * 0.5f + 7.0f);
    const Vec3 far_left_outer = gp_track_world_point(game, far_distance, -(track_width * 0.5f + 7.0f));
    const Vec3 far_left = gp_track_world_point(game, far_distance, -track_width * 0.5f);
    const Vec3 far_right = gp_track_world_point(game, far_distance, track_width * 0.5f);
    const Vec3 far_right_outer = gp_track_world_point(game, far_distance, track_width * 0.5f + 7.0f);

    const GpProjection nlo = gp_project_world_point(camera, near_left_outer);
    const GpProjection nl = gp_project_world_point(camera, near_left);
    const GpProjection nr = gp_project_world_point(camera, near_right);
    const GpProjection nro = gp_project_world_point(camera, near_right_outer);
    const GpProjection flo = gp_project_world_point(camera, far_left_outer);
    const GpProjection fl = gp_project_world_point(camera, far_left);
    const GpProjection fr = gp_project_world_point(camera, far_right);
    const GpProjection fro = gp_project_world_point(camera, far_right_outer);
    if (!nl.visible || !nr.visible || !fl.visible || !fr.visible) {
      continue;
    }

    const SDL_Color curb = (slice % 2 == 0) ? theme.border : lighten(theme.border, 26);
    const SDL_Color road = (slice % 2 == 0) ? theme.asphalt : darken(theme.asphalt, 12);

    if (nlo.visible && flo.visible) {
      gp_fill_quad(renderer, {flo.screen_x, flo.screen_y}, {fl.screen_x, fl.screen_y}, {nl.screen_x, nl.screen_y},
                   {nlo.screen_x, nlo.screen_y}, curb);
    }
    gp_fill_quad(renderer, {fl.screen_x, fl.screen_y}, {fr.screen_x, fr.screen_y}, {nr.screen_x, nr.screen_y},
                 {nl.screen_x, nl.screen_y}, road);
    if (nro.visible && fro.visible) {
      gp_fill_quad(renderer, {fr.screen_x, fr.screen_y}, {fro.screen_x, fro.screen_y}, {nro.screen_x, nro.screen_y},
                   {nr.screen_x, nr.screen_y}, curb);
    }

    if (slice % 4 == 0) {
      const float stripe_half = track_width * 0.04f;
      const GpProjection stripe_fl =
          gp_project_world_point(camera, gp_track_world_point(game, far_distance, -stripe_half, 0.1f));
      const GpProjection stripe_fr =
          gp_project_world_point(camera, gp_track_world_point(game, far_distance, stripe_half, 0.1f));
      const GpProjection stripe_nl =
          gp_project_world_point(camera, gp_track_world_point(game, near_distance, -stripe_half, 0.1f));
      const GpProjection stripe_nr =
          gp_project_world_point(camera, gp_track_world_point(game, near_distance, stripe_half, 0.1f));
      if (stripe_fl.visible && stripe_fr.visible && stripe_nl.visible && stripe_nr.visible) {
        gp_fill_quad(renderer, {stripe_fl.screen_x, stripe_fl.screen_y}, {stripe_fr.screen_x, stripe_fr.screen_y},
                     {stripe_nr.screen_x, stripe_nr.screen_y}, {stripe_nl.screen_x, stripe_nl.screen_y},
                     {255, 246, 210, 255});
      }
    }
  }
}

void gp_draw_trackside(SDL_Renderer* renderer, const GameState& game, const GpCamera3D& camera, float ticks) {
  const TrackTheme& theme = game.active_track.definition->theme;
  const float track_width = game.active_track.definition->width;
  for (int prop = 20; prop >= 0; --prop) {
    const float distance = game.racers.front().distance + 26.0f + static_cast<float>(prop) * 18.0f;
    const float side = track_width * 0.8f + static_cast<float>((prop % 3) * 10);
    const SDL_Color banner = (prop % 2 == 0) ? theme.crowd_a : theme.crowd_b;
    gp_draw_billboard_quad(renderer, camera, gp_track_world_point(game, distance, -side, 0.0f), 14.0f, 26.0f, banner,
                           darken(banner, 30));
    gp_draw_billboard_quad(renderer, camera, gp_track_world_point(game, distance + 8.0f, side, 0.0f), 14.0f, 26.0f,
                           lighten(banner, 14), darken(banner, 24));
    if (prop % 3 == 0) {
      const GpProjection glow =
          gp_project_world_point(camera, gp_track_world_point(game, distance + 5.0f, side * 0.96f, 24.0f));
      if (glow.visible) {
        fill_circle(renderer, static_cast<int>(glow.screen_x), static_cast<int>(glow.screen_y),
                    std::max(2, static_cast<int>(glow.scale * 2.3f)),
                    (static_cast<int>(ticks * 7.0f + prop) % 2 == 0) ? lighten(theme.lane, 30) : theme.lane);
      }
    }
  }
}

void gp_draw_item_boxes(SDL_Renderer* renderer, const GameState& game, const GpCamera3D& camera, float ticks) {
  for (const ItemBox& box : game.item_boxes) {
    if (box.cooldown > 0.0f) {
      continue;
    }
    const Vec3 center =
        gp_track_world_point(game, box.distance, box.lane, 10.0f + std::sin(ticks * 4.0f + box.distance) * 1.4f);
    const GpProjection projection = gp_project_world_point(camera, center);
    if (!projection.visible || projection.depth > 260.0f) {
      continue;
    }
    const int size = std::max(8, static_cast<int>(projection.scale * 9.0f));
    fill_rect(renderer, {static_cast<int>(projection.screen_x) - size, static_cast<int>(projection.screen_y) - size,
                         size * 2, size * 2},
              {255, 236, 129, 255});
    draw_rect(renderer, {static_cast<int>(projection.screen_x) - size, static_cast<int>(projection.screen_y) - size,
                         size * 2, size * 2},
              {26, 36, 59, 255});
    draw_text(renderer, "?", static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y) - size / 2,
              std::max(1, size / 8), {26, 36, 59, 255}, true);
  }
}

void gp_draw_boost_pads(SDL_Renderer* renderer, const GameState& game, const GpCamera3D& camera, float ticks) {
  for (const BoostPad& pad : game.boost_pads) {
    const Vec3 fl = gp_track_world_point(game, pad.distance - 5.0f, pad.lane - 8.5f, 0.1f);
    const Vec3 fr = gp_track_world_point(game, pad.distance - 5.0f, pad.lane + 8.5f, 0.1f);
    const Vec3 nr = gp_track_world_point(game, pad.distance + 5.0f, pad.lane + 8.5f, 0.1f);
    const Vec3 nl = gp_track_world_point(game, pad.distance + 5.0f, pad.lane - 8.5f, 0.1f);
    const GpProjection pfl = gp_project_world_point(camera, fl);
    const GpProjection pfr = gp_project_world_point(camera, fr);
    const GpProjection pnr = gp_project_world_point(camera, nr);
    const GpProjection pnl = gp_project_world_point(camera, nl);
    if (!pfl.visible || !pfr.visible || !pnr.visible || !pnl.visible) {
      continue;
    }
    const SDL_Color boost = (static_cast<int>(ticks * 8.0f + pad.distance) % 2 == 0)
                                ? SDL_Color{87, 243, 225, 255}
                                : SDL_Color{255, 255, 255, 255};
    gp_fill_quad(renderer, {pfl.screen_x, pfl.screen_y}, {pfr.screen_x, pfr.screen_y}, {pnr.screen_x, pnr.screen_y},
                 {pnl.screen_x, pnl.screen_y}, boost);
    gp_fill_quad(renderer, {pfl.screen_x + 2.0f, pfl.screen_y + 1.0f}, {pfr.screen_x - 2.0f, pfr.screen_y + 1.0f},
                 {pnr.screen_x - 2.0f, pnr.screen_y - 1.0f}, {pnl.screen_x + 2.0f, pnl.screen_y - 1.0f},
                 {18, 94, 112, 255});
  }
}

void gp_draw_hazards(SDL_Renderer* renderer, const GameState& game, const GpCamera3D& camera) {
  for (const Hazard& hazard : game.hazards) {
    if (!hazard.active) {
      continue;
    }
    const GpProjection projection = gp_project_world_point(camera, gp_track_world_point(game, hazard.distance, hazard.lane, 2.0f));
    if (!projection.visible || projection.depth > 220.0f) {
      continue;
    }
    const int radius = std::max(5, static_cast<int>(projection.scale * 5.4f));
    fill_circle(renderer, static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y), radius,
                {29, 28, 33, 255});
    fill_circle(renderer, static_cast<int>(projection.screen_x) + radius / 2,
                static_cast<int>(projection.screen_y) + radius / 3, std::max(2, radius / 2), {52, 52, 60, 255});
  }
}

void gp_draw_projectiles(SDL_Renderer* renderer, const GameState& game, const GpCamera3D& camera, float ticks) {
  for (const Projectile& projectile : game.projectiles) {
    if (!projectile.active) {
      continue;
    }
    const GpProjection projection =
        gp_project_world_point(camera, gp_track_world_point(game, projectile.distance, projectile.lane,
                                                            7.0f + std::sin(ticks * 8.0f + projectile.distance) * 3.2f));
    if (!projection.visible || projection.depth > 250.0f) {
      continue;
    }
    const int radius = std::max(5, static_cast<int>(projection.scale * 4.8f));
    fill_circle(renderer, static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y), radius,
                {41, 34, 34, 255});
    fill_circle(renderer, static_cast<int>(projection.screen_x), static_cast<int>(projection.screen_y),
                std::max(2, radius / 2), {171, 157, 130, 255});
  }
}

void gp_draw_racer(SDL_Renderer* renderer, const GameState& game, const GpCamera3D& camera, const Racer& racer) {
  if (racer.is_player) {
    return;
  }
  const GpProjection base = gp_project_world_point(camera, gp_track_world_point(game, racer.distance, racer.lane, 3.0f));
  if (!base.visible || base.depth > 260.0f) {
    return;
  }

  const SDL_Color kart =
      racer.flash_timer > 0.0f ? lighten(kKartColors[racer.kart_color], 64) : kKartColors[racer.kart_color];
  const SDL_Color suit = kSuitColors[racer.suit_color];
  const SDL_Color hair = kHairColors[racer.hair_color];
  const SDL_Color shadow = darken(kart, 34);
  const SDL_Color skin{245, 208, 172, 255};
  const int body_w = std::max(12, static_cast<int>(base.scale * 18.0f));
  const int body_h = std::max(12, static_cast<int>(base.scale * 14.0f));
  const int px = static_cast<int>(base.screen_x);
  const int py = static_cast<int>(base.screen_y);
  const int wheel_y = py + body_h / 2;

  fill_rect(renderer, {px - body_w / 2, py - body_h / 6, body_w, body_h / 2}, kart);
  fill_rect(renderer, {px - body_w / 3, py, body_w * 2 / 3, body_h / 2}, shadow);
  fill_rect(renderer, {px - body_w / 5, py - body_h / 2, body_w / 2, body_h / 3}, suit);
  fill_circle(renderer, px, py - body_h / 2, std::max(4, body_w / 5), skin);
  fill_circle(renderer, px, py - body_h / 2 - std::max(2, body_w / 8), std::max(4, body_w / 5), hair);
  fill_circle(renderer, px - body_w / 3, wheel_y, std::max(3, body_w / 6), {20, 24, 31, 255});
  fill_circle(renderer, px + body_w / 3, wheel_y, std::max(3, body_w / 6), {20, 24, 31, 255});
}

void gp_draw_player_kart(SDL_Renderer* renderer, const GameState& game, float ticks) {
  const Racer& player = game.racers.front();
  const SDL_Color kart = kKartColors[player.kart_color];
  const SDL_Color kart_dark = darken(kart, 34);
  const SDL_Color suit = kSuitColors[player.suit_color];
  const SDL_Color hair = kHairColors[player.hair_color];
  const SDL_Color skin{245, 208, 172, 255};
  const float sway = std::sin(player.bob_phase * 1.6f + ticks * 2.0f) * 5.0f - player.lane_velocity * 26.0f;
  const int center_x = 256 + static_cast<int>(sway);
  const int base_y = 420 + static_cast<int>(std::sin(player.bob_phase) * 2.0f);

  fill_rect(renderer, {center_x - 96, base_y + 18, 192, 22}, darken(kart_dark, 10));
  fill_circle(renderer, center_x - 70, base_y + 44, 18, {17, 20, 28, 255});
  fill_circle(renderer, center_x + 70, base_y + 44, 18, {17, 20, 28, 255});
  fill_rect(renderer, {center_x - 74, base_y - 2, 148, 44}, kart);
  fill_rect(renderer, {center_x - 56, base_y + 12, 112, 26}, kart_dark);
  fill_rect(renderer, {center_x - 28, base_y - 22, 56, 24}, suit);
  fill_rect(renderer, {center_x - 40, base_y - 10, 80, 10}, lighten(kart, 20));
  fill_circle(renderer, center_x, base_y - 28, 20, skin);
  fill_circle(renderer, center_x, base_y - 40, 22, hair);
  fill_rect(renderer, {center_x - 34, base_y - 36, 68, 10}, {194, 232, 250, 255});
  fill_rect(renderer, {center_x - 50, base_y + 6, 16, 8}, {255, 205, 92, 255});
  fill_rect(renderer, {center_x + 34, base_y + 6, 16, 8}, {255, 205, 92, 255});
}

std::vector<int> ranking_order(const GameState& game) {
  std::vector<int> order(game.racers.size());
  for (int index = 0; index < static_cast<int>(game.racers.size()); ++index) {
    order[index] = index;
  }
  std::sort(order.begin(), order.end(), [&](int left, int right) {
    const Racer& a = game.racers[left];
    const Racer& b = game.racers[right];
    if (a.finished != b.finished) {
      return a.finished;
    }
    if (a.finished && b.finished && a.finish_position != b.finish_position) {
      return a.finish_position < b.finish_position;
    }
    return a.distance > b.distance;
  });
  return order;
}

void update_places(GameState& game) {
  const auto order = ranking_order(game);
  for (int index = 0; index < static_cast<int>(order.size()); ++index) {
    game.racers[order[index]].current_place = index + 1;
  }
}

void try_pick_item(GameState& game, Racer& racer) {
  for (ItemBox& box : game.item_boxes) {
    if (box.cooldown > 0.0f || racer.item != ItemType::none) {
      continue;
    }
    if (std::fabs(loop_signed_delta(racer.distance, box.distance, game.active_track.total_length)) < 7.0f &&
        std::fabs(racer.lane - box.lane) < 10.0f) {
      racer.item = random_item();
      racer.item_cooldown = 0.8f;
      box.cooldown = 4.0f;
      return;
    }
  }
}

void use_item(GameState& game, std::size_t racer_index) {
  Racer& racer = game.racers[racer_index];
  if (racer.item == ItemType::none || racer.item_cooldown > 0.0f) {
    return;
  }
  if (racer.item == ItemType::nos) {
    racer.boost_timer = std::max(racer.boost_timer, 1.4f);
  } else if (racer.item == ItemType::oil) {
    for (Hazard& hazard : game.hazards) {
      if (hazard.active) {
        continue;
      }
      hazard.active = true;
      hazard.distance = racer.distance - 10.0f;
      hazard.lane = racer.lane;
      hazard.ttl = 7.0f;
      hazard.owner = static_cast<int>(racer_index);
      break;
    }
  } else if (racer.item == ItemType::tire) {
    for (Projectile& projectile : game.projectiles) {
      if (projectile.active) {
        continue;
      }
      projectile.active = true;
      projectile.distance = racer.distance + 12.0f;
      projectile.lane = racer.lane;
      projectile.speed = 140.0f;
      projectile.ttl = 2.8f;
      projectile.owner = static_cast<int>(racer_index);
      break;
    }
  }
  racer.item = ItemType::none;
  racer.item_cooldown = 0.7f;
}

float nearest_lane_for_box(const GameState& game, float racer_distance) {
  float best_delta = 99999.0f;
  float best_lane = 0.0f;
  for (const ItemBox& box : game.item_boxes) {
    if (box.cooldown > 0.0f) {
      continue;
    }
    const float ahead = loop_ahead_distance(racer_distance, box.distance, game.active_track.total_length);
    if (ahead < 90.0f && ahead < best_delta) {
      best_delta = ahead;
      best_lane = box.lane;
    }
  }
  return best_lane;
}

void update_bot(GameState& game, std::size_t index, float dt) {
  Racer& racer = game.racers[index];
  const Archetype& archetype = kArchetypes[racer.archetype];
  float desired_lane = std::sin((racer.distance / game.active_track.total_length) * 5.3f + static_cast<float>(index)) *
                       (game.active_track.definition->width * 0.18f);
  if (racer.item == ItemType::none) {
    desired_lane = nearest_lane_for_box(game, racer.distance);
  }
  for (const Hazard& hazard : game.hazards) {
    if (!hazard.active) {
      continue;
    }
    const float ahead = loop_ahead_distance(racer.distance, hazard.distance, game.active_track.total_length);
    if (ahead < 28.0f && std::fabs(racer.lane - hazard.lane) < 14.0f) {
      desired_lane = (racer.lane <= hazard.lane) ? -game.active_track.definition->width * 0.30f
                                                 : game.active_track.definition->width * 0.30f;
    }
  }
  for (std::size_t other = 0; other < game.racers.size(); ++other) {
    if (other == index) {
      continue;
    }
    const Racer& rival = game.racers[other];
    const float ahead = loop_ahead_distance(racer.distance, rival.distance, game.active_track.total_length);
    if (ahead < 26.0f && std::fabs(racer.lane - rival.lane) < 10.0f) {
      desired_lane = (racer.lane <= rival.lane) ? -game.active_track.definition->width * 0.28f
                                                : game.active_track.definition->width * 0.28f;
    }
  }
  racer.ai_lane_target = clampf(desired_lane, -game.active_track.definition->width * 0.34f,
                                game.active_track.definition->width * 0.34f);
  racer.lane_velocity += clampf((racer.ai_lane_target - racer.lane) / 12.0f, -1.0f, 1.0f) *
                         archetype.handling * dt;
  if (racer.item == ItemType::nos && racer.current_place > 3 && random_int(0, 1000) < 8) {
    use_item(game, index);
  }
  if (racer.item == ItemType::oil) {
    for (std::size_t other = 0; other < game.racers.size(); ++other) {
      if (other == index) {
        continue;
      }
      const float behind = loop_ahead_distance(game.racers[other].distance, racer.distance, game.active_track.total_length);
      if (behind < 18.0f && std::fabs(game.racers[other].lane - racer.lane) < 12.0f) {
        use_item(game, index);
        break;
      }
    }
  }
  if (racer.item == ItemType::tire) {
    for (std::size_t other = 0; other < game.racers.size(); ++other) {
      if (other == index) {
        continue;
      }
      const float ahead = loop_ahead_distance(racer.distance, game.racers[other].distance, game.active_track.total_length);
      if (ahead < 48.0f && std::fabs(game.racers[other].lane - racer.lane) < 12.0f) {
        use_item(game, index);
        break;
      }
    }
  }
}

void update_race(GameState& game, const InputFrame& input, AudioState& audio, float dt) {
  const float total_race_distance = game.active_track.total_length * game.active_track.definition->laps;
  const bool live_racing = game.countdown <= 0.0f && !game.pause;
  if (input.pressed_start && game.countdown <= 0.0f) {
    game.pause = !game.pause;
  }
  if (!game.pause) {
    game.race_clock += dt;
  }
  if (game.countdown > 0.0f && !game.pause) {
    const float previous = game.countdown;
    game.countdown = std::max(0.0f, game.countdown - dt);
    if (std::floor(previous) != std::floor(game.countdown)) {
      play_tone(audio, 660.0f + (3.0f - std::floor(game.countdown)) * 80.0f, 110);
    }
  }
  for (ItemBox& box : game.item_boxes) {
    box.cooldown = std::max(0.0f, box.cooldown - dt);
  }
  for (Hazard& hazard : game.hazards) {
    if (hazard.active) {
      hazard.ttl -= dt;
      if (hazard.ttl <= 0.0f) {
        hazard.active = false;
      }
    }
  }
  for (Projectile& projectile : game.projectiles) {
    if (projectile.active) {
      projectile.distance += projectile.speed * dt;
      projectile.ttl -= dt;
      if (projectile.ttl <= 0.0f) {
        projectile.active = false;
      }
    }
  }

  for (std::size_t index = 0; index < game.racers.size(); ++index) {
    Racer& racer = game.racers[index];
    const Archetype& archetype = kArchetypes[racer.archetype];
    racer.spin_timer = std::max(0.0f, racer.spin_timer - dt);
    racer.boost_timer = std::max(0.0f, racer.boost_timer - dt);
    racer.flash_timer = std::max(0.0f, racer.flash_timer - dt);
    racer.item_cooldown = std::max(0.0f, racer.item_cooldown - dt);
    racer.bob_phase += dt * (2.2f + racer.speed * 0.02f);
    if (racer.finished) {
      continue;
    }
    if (!racer.is_player && live_racing) {
      update_bot(game, index, dt);
    }

    float steer = 0.0f;
    bool throttle = !racer.is_player;
    bool brake = false;
    if (racer.is_player) {
      steer = static_cast<float>((input.held.right ? 1 : 0) - (input.held.left ? 1 : 0));
      throttle = input.held.a || input.held.up;
      brake = input.held.down != 0;
      if (input.pressed_b) {
        use_item(game, index);
      }
    }
    if (!live_racing) {
      throttle = false;
      brake = false;
      if (!racer.is_player) {
        steer = 0.0f;
      }
    }
    if (!racer.is_player) {
      steer = clampf((racer.ai_lane_target - racer.lane) / 10.0f, -1.0f, 1.0f);
    }
    if (racer.spin_timer > 0.0f) {
      throttle = false;
      brake = true;
      steer = std::sin(game.race_clock * 11.0f + static_cast<float>(index)) * 0.7f;
    }

    racer.lane_velocity += steer * archetype.handling * dt;
    racer.lane_velocity *= 0.82f;
    racer.lane += racer.lane_velocity * dt;
    racer.lane = clampf(racer.lane, -game.active_track.definition->width * 0.35f,
                        game.active_track.definition->width * 0.35f);

    float max_speed = archetype.top_speed;
    if (racer.boost_timer > 0.0f) {
      max_speed += 34.0f;
    }
    if (std::fabs(racer.lane) > game.active_track.definition->width * 0.28f) {
      max_speed *= 0.86f;
    }
    if (racer.spin_timer > 0.0f) {
      max_speed *= 0.42f;
    }
    float target_speed = throttle ? max_speed : max_speed * 0.62f;
    if (brake) {
      target_speed *= 0.36f;
    }
    racer.speed = approach(racer.speed, target_speed, (throttle ? archetype.acceleration : 42.0f) * dt);
    racer.distance += racer.speed * dt;

    for (const BoostPad& pad : game.boost_pads) {
      if (std::fabs(loop_signed_delta(racer.distance, pad.distance, game.active_track.total_length)) < 10.0f &&
          std::fabs(racer.lane - pad.lane) < 12.0f) {
        racer.boost_timer = std::max(racer.boost_timer, 0.8f);
      }
    }
    try_pick_item(game, racer);
    for (Hazard& hazard : game.hazards) {
      if (!hazard.active || hazard.owner == static_cast<int>(index)) {
        continue;
      }
      if (std::fabs(loop_signed_delta(racer.distance, hazard.distance, game.active_track.total_length)) < 8.0f &&
          std::fabs(racer.lane - hazard.lane) < 10.0f) {
        racer.spin_timer = std::max(racer.spin_timer, 0.95f * archetype.resilience);
        racer.flash_timer = 0.35f;
        hazard.active = false;
        if (racer.is_player) {
          play_tone(audio, 170.0f, 180);
        }
      }
    }
    for (Projectile& projectile : game.projectiles) {
      if (!projectile.active || projectile.owner == static_cast<int>(index)) {
        continue;
      }
      if (std::fabs(loop_signed_delta(racer.distance, projectile.distance, game.active_track.total_length)) < 7.0f &&
          std::fabs(racer.lane - projectile.lane) < 8.0f) {
        racer.spin_timer = std::max(racer.spin_timer, 1.15f * archetype.resilience);
        racer.flash_timer = 0.45f;
        projectile.active = false;
        if (racer.is_player) {
          play_tone(audio, 140.0f, 220);
        }
      }
    }
    if (racer.distance >= total_race_distance && !racer.finished) {
      racer.finished = true;
      racer.finish_position = game.next_finish_position++;
      racer.finish_time = game.race_clock;
      racer.speed *= 0.72f;
      if (racer.is_player) {
        play_tone(audio, 980.0f, 250);
      }
    }
  }

  for (std::size_t left = 0; left < game.racers.size(); ++left) {
    for (std::size_t right = left + 1; right < game.racers.size(); ++right) {
      const float gap = std::fabs(loop_signed_delta(game.racers[left].distance, game.racers[right].distance,
                                                    game.active_track.total_length));
      if (gap < 8.0f && std::fabs(game.racers[left].lane - game.racers[right].lane) < 7.0f) {
        game.racers[left].speed *= 0.992f;
        game.racers[right].speed *= 0.992f;
        const float push = (game.racers[left].lane <= game.racers[right].lane) ? -0.4f : 0.4f;
        game.racers[left].lane += push;
        game.racers[right].lane -= push;
      }
    }
  }

  update_places(game);
  audio.target_frequency = 90.0f + game.racers.front().speed * 1.55f + (game.racers.front().boost_timer > 0.0f ? 70.0f : 0.0f);
  if (game.racers.front().finished) {
    game.results_timer += dt;
    if (game.results_timer > 2.0f) {
      for (Racer& racer : game.racers) {
        if (!racer.finished) {
          racer.finished = true;
          racer.finish_position = game.next_finish_position++;
        }
      }
      game.screen = Screen::results;
    }
  }
}

void draw_hud(SDL_Renderer* renderer, const GameState& game) {
  const Racer& player = game.racers.front();
  const TrackDefinition& track = *game.active_track.definition;
  fill_rect(renderer, {0, 0, 512, 52}, {17, 27, 43, 216});
  fill_rect(renderer, {18, 10, 124, 32}, darken(track.theme.hud, 18));
  fill_rect(renderer, {154, 10, 92, 32}, darken(track.theme.hud, 18));
  fill_rect(renderer, {258, 10, 104, 32}, darken(track.theme.hud, 18));
  fill_rect(renderer, {374, 10, 120, 32}, darken(track.theme.hud, 18));
  draw_text(renderer, track.name, 24, 20, 2, {255, 247, 220, 255}, false);
  draw_text(renderer,
            "L" + std::to_string(std::min(track.laps, static_cast<int>(player.distance / game.active_track.total_length) + 1)) +
                "/" + std::to_string(track.laps),
            176, 20, 2, {255, 247, 220, 255}, false);
  draw_text(renderer,
            "P" + std::to_string(player.current_place) + "/" + std::to_string(static_cast<int>(game.racers.size())),
            276, 20, 2, {255, 247, 220, 255}, false);
  draw_text(renderer, std::to_string(static_cast<int>(player.speed)) + " KM", 394, 20, 2, {255, 247, 220, 255}, false);
  draw_text(renderer, item_name(player.item), 472, 20, 2, {255, 247, 220, 255}, true);
}

void draw_race(SDL_Renderer* renderer, const GameState& game, float ticks) {
  const GpCamera3D camera = gp_make_camera(game);
  const TrackDefinition& track = *game.active_track.definition;
  gp_draw_track_surface(renderer, game, track.theme, camera, ticks);
  gp_draw_trackside(renderer, game, camera, ticks);
  gp_draw_boost_pads(renderer, game, camera, ticks);
  gp_draw_item_boxes(renderer, game, camera, ticks);
  gp_draw_hazards(renderer, game, camera);
  gp_draw_projectiles(renderer, game, camera, ticks);
  std::vector<std::pair<float, int>> rivals;
  for (int index = 0; index < static_cast<int>(game.racers.size()); ++index) {
    if (game.racers[index].is_player) {
      continue;
    }
    const GpProjection projection =
        gp_project_world_point(camera, gp_track_world_point(game, game.racers[index].distance, game.racers[index].lane, 3.0f));
    if (projection.visible) {
      rivals.push_back({projection.depth, index});
    }
  }
  std::sort(rivals.begin(), rivals.end(), [](const auto& left, const auto& right) { return left.first > right.first; });
  for (const auto& rival : rivals) {
    gp_draw_racer(renderer, game, camera, game.racers[rival.second]);
  }
  draw_hud(renderer, game);
  gp_draw_player_kart(renderer, game, ticks);
  if (game.countdown > 0.0f) {
    const int countdown_number = static_cast<int>(std::ceil(game.countdown));
    const std::string text = countdown_number > 0 ? std::to_string(countdown_number) : "GO";
    fill_rect(renderer, {180, 134, 152, 90}, {20, 28, 43, 186});
    draw_text(renderer, text, 256, 160, 7, {255, 238, 128, 255}, true);
  }
  if (game.pause) {
    fill_rect(renderer, {148, 144, 216, 100}, {20, 28, 43, 216});
    draw_text(renderer, "PAUSED", 256, 168, 5, {255, 247, 220, 255}, true);
    draw_text(renderer, "START TO RESUME", 256, 214, 2, {210, 231, 247, 255}, true);
  }
}

void draw_results(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Color bg{17, 30, 44, 255};
  const SDL_Color panel{236, 233, 223, 255};
  const SDL_Color text{24, 32, 47, 255};
  const SDL_Color accent{248, 206, 87, 255};
  const auto order = ranking_order(game);
  fill_rect(renderer, {0, 0, 512, 512}, bg);
  fill_rect(renderer, {38, 28, 436, 456}, panel);
  fill_rect(renderer, {38, 28, 436, 64}, accent);
  draw_text(renderer, "RACE RESULTS", 256, 46, 4, text, true);
  draw_text(renderer, game.tracks[game.setup.track_index].name, 256, 104, 2, text, true);
  int row_y = 138;
  for (int place = 0; place < static_cast<int>(order.size()) && place < 8; ++place) {
    const Racer& racer = game.racers[order[place]];
    const bool player = racer.is_player;
    SDL_Rect row{70, row_y, 372, 34};
    fill_rect(renderer, row, player ? lighten(accent, 28) : SDL_Color{221, 217, 205, 255});
    draw_rect(renderer, row, text);
    draw_text(renderer, std::to_string(place + 1), 86, row_y + 11, 2, text, false);
    draw_text(renderer, racer.name, 126, row_y + 11, 2, text, false);
    draw_text_right(renderer, kArchetypes[racer.archetype].name, 422, row_y + 11, 2, text);
    row_y += 42;
  }
  draw_text(renderer, "A REMATCH / B MENU", 256, 454, 2, text, true);
}

void handle_title(GameState& game, const InputFrame& input) {
  if (input.pressed_up) {
    game.title_selection = (game.title_selection + 2) % 3;
  }
  if (input.pressed_down) {
    game.title_selection = (game.title_selection + 1) % 3;
  }
  if (input.pressed_a || input.pressed_start) {
    if (game.title_selection == 0) {
      game.screen = Screen::setup;
    } else if (game.title_selection == 1) {
      game.screen = Screen::garage;
    }
  }
}

void handle_garage(GameState& game, const InputFrame& input, const pp_context& context) {
  if (input.pressed_up) {
    game.garage_selection = (game.garage_selection + 4) % 5;
  }
  if (input.pressed_down) {
    game.garage_selection = (game.garage_selection + 1) % 5;
  }
  if (input.pressed_left) {
    if (game.garage_selection == 1) {
      game.profile.archetype =
          (game.profile.archetype + static_cast<int>(kArchetypes.size()) - 1) % static_cast<int>(kArchetypes.size());
    } else if (game.garage_selection == 2) {
      game.profile.suit_color =
          (game.profile.suit_color + static_cast<int>(kSuitColors.size()) - 1) % static_cast<int>(kSuitColors.size());
    } else if (game.garage_selection == 3) {
      game.profile.hair_color =
          (game.profile.hair_color + static_cast<int>(kHairColors.size()) - 1) % static_cast<int>(kHairColors.size());
    } else if (game.garage_selection == 4) {
      game.profile.kart_color =
          (game.profile.kart_color + static_cast<int>(kKartColors.size()) - 1) % static_cast<int>(kKartColors.size());
    }
    save_profile(context, game.profile);
  }
  if (input.pressed_right) {
    if (game.garage_selection == 1) {
      game.profile.archetype = (game.profile.archetype + 1) % static_cast<int>(kArchetypes.size());
    } else if (game.garage_selection == 2) {
      game.profile.suit_color = (game.profile.suit_color + 1) % static_cast<int>(kSuitColors.size());
    } else if (game.garage_selection == 3) {
      game.profile.hair_color = (game.profile.hair_color + 1) % static_cast<int>(kHairColors.size());
    } else if (game.garage_selection == 4) {
      game.profile.kart_color = (game.profile.kart_color + 1) % static_cast<int>(kKartColors.size());
    }
    save_profile(context, game.profile);
  }
  if ((input.pressed_a || input.pressed_start) && game.garage_selection == 0) {
    game.draft_name = game.profile.name;
    game.keyboard_selection = 0;
    game.screen = Screen::name_entry;
  }
  if (input.pressed_b || input.pressed_select) {
    save_profile(context, game.profile);
    game.screen = Screen::title;
  }
}

void handle_name_entry(GameState& game, const InputFrame& input, const pp_context& context) {
  if (input.pressed_left) {
    game.keyboard_selection = (game.keyboard_selection + 39) % 40;
  }
  if (input.pressed_right) {
    game.keyboard_selection = (game.keyboard_selection + 1) % 40;
  }
  if (input.pressed_up) {
    game.keyboard_selection = (game.keyboard_selection + 32) % 40;
  }
  if (input.pressed_down) {
    game.keyboard_selection = (game.keyboard_selection + 8) % 40;
  }
  if (input.pressed_b && !game.draft_name.empty()) {
    game.draft_name.pop_back();
  }
  if (input.pressed_a || input.pressed_start) {
    const std::string cell = kKeyboardCells[game.keyboard_selection];
    if (cell == "DONE") {
      if (!game.draft_name.empty()) {
        game.profile.name = game.draft_name;
        save_profile(context, game.profile);
      }
      game.screen = Screen::garage;
      return;
    }
    if (cell == "DEL") {
      if (!game.draft_name.empty()) {
        game.draft_name.pop_back();
      }
      return;
    }
    if (static_cast<int>(game.draft_name.size()) >= kMaxNameLength) {
      return;
    }
    if (cell == "SP") {
      game.draft_name.push_back('-');
    } else {
      game.draft_name += cell;
    }
  }
  if (input.pressed_select) {
    game.screen = Screen::garage;
  }
}

void handle_setup(GameState& game, const InputFrame& input) {
  if (input.pressed_up) {
    game.setup_selection = (game.setup_selection + 2) % 3;
  }
  if (input.pressed_down) {
    game.setup_selection = (game.setup_selection + 1) % 3;
  }
  if (input.pressed_left) {
    if (game.setup_selection == 0) {
      game.setup.track_index = (game.setup.track_index + static_cast<int>(game.tracks.size()) - 1) %
                               static_cast<int>(game.tracks.size());
    } else if (game.setup_selection == 1) {
      game.setup.field_size_index = (game.setup.field_size_index + static_cast<int>(kFieldSizes.size()) - 1) %
                                    static_cast<int>(kFieldSizes.size());
    }
  }
  if (input.pressed_right) {
    if (game.setup_selection == 0) {
      game.setup.track_index = (game.setup.track_index + 1) % static_cast<int>(game.tracks.size());
    } else if (game.setup_selection == 1) {
      game.setup.field_size_index = (game.setup.field_size_index + 1) % static_cast<int>(kFieldSizes.size());
    }
  }
  if ((input.pressed_a || input.pressed_start) && game.setup_selection == 2) {
    start_race(game);
  }
  if (input.pressed_b || input.pressed_select) {
    game.screen = Screen::title;
  }
}

void handle_results(GameState& game, const InputFrame& input) {
  if (input.pressed_a || input.pressed_start) {
    start_race(game);
  }
  if (input.pressed_b || input.pressed_select) {
    game.screen = Screen::title;
  }
}

}  // namespace

int main(int, char**) {
  pp_context context{};
  pp_audio_spec audio_spec{};
  SDL_AudioDeviceID audio_device = 0;
  AudioState audio{};
  InputFrame input{};
  GameState game;
  int width = 512;
  int height = 512;

  std::srand(static_cast<unsigned int>(std::time(nullptr)));
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  if (pp_init(&context, "grand-prix") != 0) {
    std::fprintf(stderr, "pp_init failed.\n");
    SDL_Quit();
    return 1;
  }

  pp_get_framebuffer_size(&context, &width, &height);
  load_profile(context, game.profile);
  game.tracks = make_tracks();

  SDL_Window* window =
      SDL_CreateWindow("Grand Prix", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }
  SDL_RenderSetLogicalSize(renderer, kWindowWidth, kWindowHeight);

  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &audio;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }

  Uint32 last_ticks = SDL_GetTicks();
  while (!pp_should_exit(&context)) {
    const Uint32 now = SDL_GetTicks();
    const float dt = clampf(static_cast<float>(now - last_ticks) / 1000.0f, 0.0f, 0.05f);
    const float seconds = static_cast<float>(now) / 1000.0f;
    last_ticks = now;
    input = read_input(context, input);

    if (game.screen == Screen::title) {
      handle_title(game, input);
      if ((input.pressed_a || input.pressed_start) && game.title_selection == 2) {
        pp_request_exit(&context);
      }
    } else if (game.screen == Screen::garage) {
      handle_garage(game, input, context);
    } else if (game.screen == Screen::name_entry) {
      handle_name_entry(game, input, context);
    } else if (game.screen == Screen::setup) {
      handle_setup(game, input);
    } else if (game.screen == Screen::racing) {
      update_race(game, input, audio, dt);
    } else if (game.screen == Screen::results) {
      handle_results(game, input);
    }

    if (input.pressed_select && game.screen != Screen::name_entry && game.screen != Screen::racing) {
      game.screen = Screen::title;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (game.screen == Screen::title) {
      draw_title(renderer, game, seconds);
    } else if (game.screen == Screen::garage) {
      draw_garage(renderer, game);
    } else if (game.screen == Screen::name_entry) {
      draw_name_entry(renderer, game);
    } else if (game.screen == Screen::setup) {
      draw_setup(renderer, game);
    } else if (game.screen == Screen::racing) {
      draw_race(renderer, game, seconds);
    } else if (game.screen == Screen::results) {
      draw_results(renderer, game);
    }
    SDL_RenderPresent(renderer);
  }

  save_profile(context, game.profile);
  if (audio_device != 0) {
    SDL_CloseAudioDevice(audio_device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&context);
  SDL_Quit();
  return 0;
}
