#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kW = 512;
constexpr int kH = 512;
constexpr Uint32 kBotDelayMs = 260;

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct EdgeInput {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool a = false;
  bool b = false;
  bool start = false;
  bool select = false;
};

enum class AppScreen { Menu, Game };
enum class GameKind { Holdem = 0, Sevens, WildSweep, HouseCurrent, SkyCrown, LoneLantern, ShadeSignal };

static const SDL_Color kBgTop = {12, 17, 29, 255};
static const SDL_Color kBgMid = {20, 36, 48, 255};
static const SDL_Color kBgBottom = {12, 24, 20, 255};
static const SDL_Color kPanel = {22, 29, 40, 236};
static const SDL_Color kPanelHi = {36, 48, 63, 244};
static const SDL_Color kFrame = {94, 125, 150, 255};
static const SDL_Color kText = {239, 243, 244, 255};
static const SDL_Color kMuted = {146, 164, 177, 255};
static const SDL_Color kAccent = {246, 199, 111, 255};
static const SDL_Color kAccent2 = {111, 221, 182, 255};
static const SDL_Color kDanger = {243, 108, 116, 255};
static const SDL_Color kGold = {240, 214, 123, 255};
static const SDL_Color kTable = {26, 79, 58, 255};

static const std::array<SDL_Color, 6> kSeatColors{{
    {243, 112, 101, 255}, {109, 168, 244, 255}, {125, 213, 141, 255},
    {246, 201, 112, 255}, {201, 141, 243, 255}, {241, 164, 108, 255},
}};

static const std::array<const char*, 6> kBotNames{{
    "CAPTAIN", "MATE", "SMOKE", "BELL", "LANTERN", "TIDE",
}};

int clamp_int(int value, int low, int high) {
  return std::max(low, std::min(high, value));
}

std::string upper(std::string text) {
  for (char& ch : text) {
    if (ch >= 'a' && ch <= 'z') {
      ch = static_cast<char>(ch - 'a' + 'A');
    }
  }
  return text;
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
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case '\'': return {0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00};
    case '!': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
    case '?': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    default: return {0, 0, 0, 0, 0, 0, 0};
  }
}

int text_width(const std::string& text, int scale) {
  return text.empty() ? 0 : static_cast<int>(text.size()) * (6 * scale) - scale;
}

std::string fit_text(std::string text, int width, int scale) {
  if (text_width(text, scale) <= width) {
    return text;
  }
  while (text.size() > 3 && text_width(text + "...", scale) > width) {
    text.pop_back();
  }
  return text + "...";
}

void draw_text(SDL_Renderer* renderer, const std::string& text_in, int x, int y, int scale,
               SDL_Color color, bool centered = false) {
  const std::string text = upper(text_in);
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
        if ((glyph[static_cast<std::size_t>(row)] & (1 << (4 - col))) == 0) {
          continue;
        }
        SDL_Rect px{draw_x + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &px);
      }
    }
    draw_x += 6 * scale;
  }
}

void draw_text_right(SDL_Renderer* renderer, const std::string& text, int right, int y, int scale,
                     SDL_Color color) {
  draw_text(renderer, text, right - text_width(text, scale), y, scale, color, false);
}

void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void draw_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
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

void draw_circle_outline(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int y = -radius; y <= radius; ++y) {
    for (int x = -radius; x <= radius; ++x) {
      const int dist = x * x + y * y;
      if (dist <= radius * radius && dist >= (radius - 1) * (radius - 1)) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
      }
    }
  }
}

void audio_callback(void* userdata, Uint8* stream, int length) {
  auto* tone = static_cast<ToneState*>(userdata);
  auto* samples = reinterpret_cast<int16_t*>(stream);
  const int count = length / static_cast<int>(sizeof(int16_t));
  for (int i = 0; i < count; ++i) {
    int16_t sample = 0;
    if (tone->frames_remaining > 0) {
      sample = tone->phase < 3.14159f ? 1400 : -1400;
      tone->phase += (6.28318f * tone->frequency) / 48000.0f;
      if (tone->phase >= 6.28318f) {
        tone->phase -= 6.28318f;
      }
      --tone->frames_remaining;
    }
    samples[i] = sample;
  }
}

void trigger_tone(ToneState& tone, float frequency, int milliseconds) {
  tone.frequency = frequency;
  tone.frames_remaining = (48000 * milliseconds) / 1000;
}

EdgeInput edge_input(const pp_input_state& input, const pp_input_state& previous) {
  EdgeInput edge;
  edge.up = input.up && !previous.up;
  edge.down = input.down && !previous.down;
  edge.left = input.left && !previous.left;
  edge.right = input.right && !previous.right;
  edge.a = input.a && !previous.a;
  edge.b = input.b && !previous.b;
  edge.start = input.start && !previous.start;
  edge.select = input.select && !previous.select;
  return edge;
}

void draw_background(SDL_Renderer* renderer) {
  for (int y = 0; y < kH; ++y) {
    const float t = static_cast<float>(y) / static_cast<float>(kH - 1);
    SDL_Color color{
        static_cast<Uint8>(kBgTop.r * (1.0f - t) + kBgBottom.r * t),
        static_cast<Uint8>(kBgTop.g * (1.0f - t) + kBgBottom.g * t),
        static_cast<Uint8>(kBgTop.b * (1.0f - t) + kBgBottom.b * t),
        255,
    };
    fill_rect(renderer, {0, y, kW, 1}, color);
  }
  fill_rect(renderer, {0, 0, kW, 84}, kBgTop);
  fill_rect(renderer, {0, 84, kW, 28}, kBgMid);
}

void draw_pack_frame(SDL_Renderer* renderer, const std::string& title, const std::string& subtitle) {
  draw_background(renderer);
  fill_rect(renderer, {20, 18, 472, 76}, {25, 33, 47, 255});
  draw_rect(renderer, {20, 18, 472, 76}, kFrame);
  draw_text(renderer, "VELVET DECK", 36, 34, 4, kGold);
  draw_text(renderer, subtitle, 38, 72, 1, kMuted);
  draw_text_right(renderer, title, 476, 44, 2, kAccent);
  fill_rect(renderer, {20, 104, 472, 388}, {9, 19, 22, 255});
  draw_rect(renderer, {20, 104, 472, 388}, kFrame);
}

void draw_panel(SDL_Renderer* renderer, SDL_Rect rect, const std::string& title) {
  fill_rect(renderer, rect, kPanel);
  draw_rect(renderer, rect, kFrame);
  if (!title.empty()) {
    draw_text(renderer, title, rect.x + 10, rect.y + 10, 1, kMuted);
  }
}

SDL_Color suit_color(int suit) {
  return (suit == 1 || suit == 2) ? SDL_Color{226, 89, 102, 255} : SDL_Color{32, 38, 49, 255};
}

std::string rank_text_standard(int rank) {
  switch (rank) {
    case 1: return "A";
    case 11: return "J";
    case 12: return "Q";
    case 13: return "K";
    default: return std::to_string(rank);
  }
}

std::string rank_text_holdem(int rank) {
  switch (rank) {
    case 14: return "A";
    case 13: return "K";
    case 12: return "Q";
    case 11: return "J";
    case 10: return "10";
    default: return std::to_string(rank);
  }
}

std::string suit_text(int suit) {
  switch (suit) {
    case 0: return "S";
    case 1: return "H";
    case 2: return "D";
    default: return "C";
  }
}

void draw_standard_card_face(SDL_Renderer* renderer, int x, int y, const std::string& rank,
                             const std::string& suit, SDL_Color color) {
  const SDL_Rect rect{x, y, 42, 58};
  fill_rect(renderer, {rect.x + 3, rect.y + 3, rect.w, rect.h}, {10, 12, 17, 120});
  fill_rect(renderer, rect, {241, 240, 233, 255});
  draw_rect(renderer, rect, {34, 76, 54, 255});
  draw_rect(renderer, {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4}, {210, 204, 194, 255});
  draw_text(renderer, rank, rect.x + 4, rect.y + 4, 1, color);
  draw_text(renderer, suit, rect.x + 4, rect.y + 13, 1, color);
  draw_text(renderer, rank, rect.x + rect.w - 4 - text_width(rank, 1), rect.y + 45, 1, color);
  draw_text(renderer, suit, rect.x + rect.w - 4 - text_width(suit, 1), rect.y + 36, 1, color);
  draw_text(renderer, suit, rect.x + 15, rect.y + 22, 4, color, true);
}

void draw_standard_card_back(SDL_Renderer* renderer, int x, int y, std::string_view label,
                             SDL_Color accent = kGold) {
  const SDL_Rect rect{x, y, 42, 58};
  fill_rect(renderer, {rect.x + 3, rect.y + 3, rect.w, rect.h}, {10, 12, 17, 120});
  fill_rect(renderer, rect, {25, 39, 53, 255});
  draw_rect(renderer, rect, {70, 89, 108, 255});
  draw_rect(renderer, {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4}, {57, 68, 80, 255});
  for (int stripe = 0; stripe < 7; ++stripe) {
    draw_line(renderer, rect.x + 3 + stripe * 6, rect.y + 5, rect.x + 16 + stripe * 6, rect.y + 54,
              {46, 63, 84, 255});
  }
  draw_text(renderer, std::string(label), rect.x + 10, rect.y + 20, 2, accent);
}

void draw_chip_stack(SDL_Renderer* renderer, int x, int y, int amount, SDL_Color accent) {
  if (amount <= 0) {
    return;
  }
  const int rows = std::min(4, 1 + amount / 30);
  for (int i = 0; i < rows; ++i) {
    const int ry = y - i * 5;
    fill_circle(renderer, x, ry, 11, {164, 129, 54, 255});
    fill_circle(renderer, x, ry, 8, {228, 202, 114, 255});
    draw_line(renderer, x - 6, ry, x + 6, ry, accent);
    draw_line(renderer, x, ry - 6, x, ry + 6, accent);
  }
}

struct HotseatGuard {
  bool active = false;
  std::string title;
  std::string detail;
  SDL_Color accent = kAccent;
};

void open_guard(HotseatGuard& guard, const std::string& title, const std::string& detail,
                SDL_Color accent = kAccent) {
  guard.active = true;
  guard.title = title;
  guard.detail = detail;
  guard.accent = accent;
}

bool update_guard(HotseatGuard& guard, const EdgeInput& edge, ToneState& tone) {
  if (!guard.active) {
    return false;
  }
  if (edge.a || edge.start) {
    guard.active = false;
    trigger_tone(tone, 620.0f, 40);
    return true;
  }
  return false;
}

void draw_guard(SDL_Renderer* renderer, const HotseatGuard& guard) {
  if (!guard.active) {
    return;
  }
  fill_rect(renderer, {72, 168, 368, 132}, {8, 13, 19, 238});
  draw_rect(renderer, {72, 168, 368, 132}, kFrame);
  draw_text(renderer, guard.title, 256, 190, 3, guard.accent, true);
  draw_text(renderer, guard.detail, 256, 234, 1, kText, true);
  draw_text(renderer, "A OR START TO REVEAL", 256, 268, 1, kMuted, true);
}

template <typename T, typename Rng>
void shuffle_vector(std::vector<T>& values, Rng& rng) {
  std::shuffle(values.begin(), values.end(), rng);
}

// ---------------------------------------------------------------------------
// River Five
// ---------------------------------------------------------------------------

namespace holdem {

constexpr int kPlayers = 6;
constexpr int kStartingStack = 1200;
constexpr int kSmallBlind = 10;
constexpr int kBigBlind = 20;
constexpr int kRaiseSize = 20;

enum class Stage { Preflop, Flop, Turn, River, Showdown };
enum class Screen { Play, Result };
enum class Action { Fold = 0, CallCheck, Raise, AllIn };

struct Card {
  int rank = 2;
  int suit = 0;
};

struct HandValue {
  int category = 0;
  std::array<int, 5> kickers{{0, 0, 0, 0, 0}};
};

struct HumanStyleStats {
  int hands_observed = 0;
  int vpip_hands = 0;
  int aggressive_hands = 0;
  int call_actions = 0;
  int check_actions = 0;
  int raise_actions = 0;
  int all_in_actions = 0;
  int folds_to_pressure = 0;
  int showdown_hands = 0;
  int bluff_showdowns = 0;
};

struct HumanRead {
  double looseness = 0.34;
  double aggression = 0.26;
  double foldiness = 0.22;
  double bluffiness = 0.10;
};

struct Player {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  bool human = false;
  bool folded = false;
  bool all_in = false;
  bool acted = false;
  bool tracked_vpip = false;
  bool tracked_aggression = false;
  int chips = kStartingStack;
  int street_contrib = 0;
  int hand_contrib = 0;
  Card hole[2]{};
  HandValue best{};
};

struct State {
  std::mt19937 rng{0x52564F46u};
  std::array<Player, kPlayers> players{};
  std::array<Card, 52> deck{};
  std::array<Card, 5> board{};
  std::vector<int> winners{};
  int human_players = 1;
  int dealer = kPlayers - 1;
  int current_player = 0;
  int action_index = 0;
  int current_bet = 0;
  int pot = 0;
  int board_count = 0;
  int deck_index = 0;
  int raise_to = 0;
  Stage stage = Stage::Preflop;
  Screen screen = Screen::Play;
  bool paused = false;
  bool raise_picker = false;
  std::string status = "PREFLOP";
  std::string detail = "UP/DOWN MOVE / A CONFIRM";
  Uint32 bot_ready_at = 0;
  HumanStyleStats human_style{};
  HotseatGuard guard{};
};

static const std::array<const char*, 4> kActionNames{{"FOLD", "CALL", "RAISE", "ALL IN"}};

bool less_hand(const HandValue& a, const HandValue& b) {
  if (a.category != b.category) return a.category < b.category;
  return a.kickers < b.kickers;
}

int next_player(int seat) {
  return (seat + 1) % kPlayers;
}

std::string stage_text(Stage stage) {
  switch (stage) {
    case Stage::Preflop: return "PREFLOP";
    case Stage::Flop: return "FLOP";
    case Stage::Turn: return "TURN";
    case Stage::River: return "RIVER";
    default: return "SHOWDOWN";
  }
}

int to_call_amount(const State& state, int seat) {
  const Player& player = state.players[static_cast<std::size_t>(seat)];
  return std::max(0, state.current_bet - player.street_contrib);
}

int max_raise_to(const State& state, int seat) {
  const Player& player = state.players[static_cast<std::size_t>(seat)];
  return player.street_contrib + player.chips;
}

int min_raise_to(const State& state, int seat) {
  const int minimum_open = std::max(kBigBlind, state.current_bet + kRaiseSize);
  return std::min(max_raise_to(state, seat), minimum_open);
}

int clamp_raise_to(const State& state, int seat, int raise_to) {
  const int minimum = min_raise_to(state, seat);
  const int maximum = max_raise_to(state, seat);
  if (maximum <= minimum) {
    return maximum;
  }
  const int stepped = minimum + ((std::max(minimum, raise_to) - minimum + (kRaiseSize / 2)) / kRaiseSize) * kRaiseSize;
  return clamp_int(stepped, minimum, maximum);
}

std::string action_label(const State& state, int seat, Action action) {
  switch (action) {
    case Action::Fold: return "FOLD";
    case Action::CallCheck: return to_call_amount(state, seat) > 0 ? "CALL" : "CHECK";
    case Action::Raise: return "RAISE";
    default: return "ALL IN";
  }
}

void resolve_showdown(State& state);

double safe_ratio(int value, int total, double fallback) {
  return total > 0 ? static_cast<double>(value) / static_cast<double>(total) : fallback;
}

void reset_round(Player& player) {
  player.folded = false;
  player.all_in = false;
  player.acted = false;
  player.tracked_vpip = false;
  player.tracked_aggression = false;
  player.street_contrib = 0;
  player.hand_contrib = 0;
  player.best = {};
}

std::vector<Card> build_deck() {
  std::vector<Card> deck;
  deck.reserve(52);
  for (int suit = 0; suit < 4; ++suit) {
    for (int rank = 2; rank <= 14; ++rank) {
      deck.push_back(Card{rank, suit});
    }
  }
  return deck;
}

void shuffle_deck(State& state) {
  auto deck = build_deck();
  shuffle_vector(deck, state.rng);
  for (std::size_t i = 0; i < deck.size(); ++i) {
    state.deck[i] = deck[i];
  }
  state.deck_index = 0;
}

Card draw_card(State& state) {
  return state.deck[static_cast<std::size_t>(state.deck_index++)];
}

int active_player_count(const State& state) {
  int count = 0;
  for (const auto& player : state.players) {
    if (!player.folded) {
      ++count;
    }
  }
  return count;
}

bool player_can_act(const Player& player) {
  return !player.folded && !player.all_in && player.chips > 0;
}

int next_active_after(const State& state, int seat) {
  for (int step = 1; step <= kPlayers; ++step) {
    const int candidate = (seat + step) % kPlayers;
    if (player_can_act(state.players[static_cast<std::size_t>(candidate)])) {
      return candidate;
    }
  }
  return -1;
}

void contribute(Player& player, int amount, int& pot) {
  amount = std::min(amount, player.chips);
  player.chips -= amount;
  player.street_contrib += amount;
  player.hand_contrib += amount;
  pot += amount;
  if (player.chips <= 0) {
    player.all_in = true;
  }
}

HandValue evaluate_five(const std::array<Card, 5>& cards) {
  std::array<int, 15> counts{};
  std::array<int, 5> ranks{};
  bool flush = true;
  for (int i = 0; i < 5; ++i) {
    counts[static_cast<std::size_t>(cards[i].rank)]++;
    ranks[static_cast<std::size_t>(i)] = cards[i].rank;
    if (cards[i].suit != cards[0].suit) flush = false;
  }
  std::sort(ranks.begin(), ranks.end(), std::greater<int>());

  std::vector<int> unique = {ranks.begin(), ranks.end()};
  std::sort(unique.begin(), unique.end());
  unique.erase(std::unique(unique.begin(), unique.end()), unique.end());

  bool straight = false;
  int straight_high = 0;
  if (unique.size() == 5) {
    if (unique.back() - unique.front() == 4) {
      straight = true;
      straight_high = unique.back();
    } else if (unique == std::vector<int>{2, 3, 4, 5, 14}) {
      straight = true;
      straight_high = 5;
    }
  }

  std::vector<std::pair<int, int>> groups;
  for (int rank = 14; rank >= 2; --rank) {
    if (counts[static_cast<std::size_t>(rank)] > 0) {
      groups.push_back({counts[static_cast<std::size_t>(rank)], rank});
    }
  }
  std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) {
    if (a.first != b.first) return a.first > b.first;
    return a.second > b.second;
  });

  HandValue value{};
  if (straight && flush) {
    value.category = 8;
    value.kickers = {straight_high, 0, 0, 0, 0};
  } else if (groups[0].first == 4) {
    value.category = 7;
    value.kickers = {groups[0].second, groups[1].second, 0, 0, 0};
  } else if (groups[0].first == 3 && groups[1].first == 2) {
    value.category = 6;
    value.kickers = {groups[0].second, groups[1].second, 0, 0, 0};
  } else if (flush) {
    value.category = 5;
    value.kickers = {ranks[0], ranks[1], ranks[2], ranks[3], ranks[4]};
  } else if (straight) {
    value.category = 4;
    value.kickers = {straight_high, 0, 0, 0, 0};
  } else if (groups[0].first == 3) {
    value.category = 3;
    value.kickers = {groups[0].second, groups[1].second, groups[2].second, 0, 0};
  } else if (groups[0].first == 2 && groups[1].first == 2) {
    value.category = 2;
    value.kickers = {std::max(groups[0].second, groups[1].second),
                     std::min(groups[0].second, groups[1].second), groups[2].second, 0, 0};
  } else if (groups[0].first == 2) {
    value.category = 1;
    value.kickers = {groups[0].second, groups[1].second, groups[2].second, groups[3].second, 0};
  } else {
    value.category = 0;
    value.kickers = {ranks[0], ranks[1], ranks[2], ranks[3], ranks[4]};
  }
  return value;
}

HandValue evaluate_best(const std::array<Card, 2>& hole, const std::array<Card, 5>& board) {
  std::array<Card, 7> pool{};
  pool[0] = hole[0];
  pool[1] = hole[1];
  for (int i = 0; i < 5; ++i) pool[2 + i] = board[static_cast<std::size_t>(i)];

  HandValue best{};
  bool first = true;
  for (int a = 0; a < 7; ++a) {
    for (int b = a + 1; b < 7; ++b) {
      for (int c = b + 1; c < 7; ++c) {
        for (int d = c + 1; d < 7; ++d) {
          for (int e = d + 1; e < 7; ++e) {
            std::array<Card, 5> hand{{pool[a], pool[b], pool[c], pool[d], pool[e]}};
            const HandValue candidate = evaluate_five(hand);
            if (first || less_hand(best, candidate)) {
              best = candidate;
              first = false;
            }
          }
        }
      }
    }
  }
  return best;
}

const char* hand_name(const HandValue& value) {
  switch (value.category) {
    case 8: return "STRAIGHT FLUSH";
    case 7: return "FOUR OF A KIND";
    case 6: return "FULL HOUSE";
    case 5: return "FLUSH";
    case 4: return "STRAIGHT";
    case 3: return "THREE OF A KIND";
    case 2: return "TWO PAIR";
    case 1: return "PAIR";
    default: return "HIGH CARD";
  }
}

void queue_guard_if_needed(State& state) {
  if (state.current_player < 0) {
    return;
  }
  const Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  if (player.human && state.human_players > 1) {
    open_guard(state.guard, player.name + " TURN", "PASS CONTROLLER AND PRESS A", player.color);
  }
}

void schedule_bot(State& state) {
  if (state.current_player >= 0 &&
      !state.players[static_cast<std::size_t>(state.current_player)].human) {
    state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
  }
}

void reset_street(State& state, Stage next_stage) {
  state.stage = next_stage;
  state.current_bet = 0;
  state.action_index = 1;
  state.raise_picker = false;
  for (auto& player : state.players) {
    player.street_contrib = 0;
    player.acted = player.folded || player.all_in;
  }
  state.status = stage_text(next_stage);
  state.detail = "BETTING ROUND";
  state.current_player = next_active_after(state, state.dealer);
  if (state.current_player < 0) {
    state.stage = Stage::Showdown;
    resolve_showdown(state);
  } else {
    queue_guard_if_needed(state);
    schedule_bot(state);
  }
}

void reveal_rest_of_board(State& state) {
  while (state.board_count < 5) {
    state.board[static_cast<std::size_t>(state.board_count++)] = draw_card(state);
  }
}

void resolve_showdown(State& state) {
  reveal_rest_of_board(state);
  state.winners.clear();
  HandValue best{};
  bool first = true;
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    if (player.folded) {
      continue;
    }
    player.best = evaluate_best({player.hole[0], player.hole[1]}, state.board);
    if (player.human) {
      ++state.human_style.showdown_hands;
      if (player.tracked_aggression && player.best.category <= 1) {
        ++state.human_style.bluff_showdowns;
      }
    }
    if (first || less_hand(best, player.best)) {
      best = player.best;
      state.winners = {seat};
      first = false;
    } else if (!less_hand(player.best, best) && !less_hand(best, player.best)) {
      state.winners.push_back(seat);
    }
  }

  if (state.winners.empty()) {
    state.winners.push_back(0);
  }

  const int share = std::max(1, state.pot / static_cast<int>(state.winners.size()));
  for (int seat : state.winners) {
    state.players[static_cast<std::size_t>(seat)].chips += share;
  }

  state.screen = Screen::Result;
  if (state.winners.size() == 1) {
    const Player& winner = state.players[static_cast<std::size_t>(state.winners.front())];
    state.status = winner.name + " SCOOPS " + std::to_string(state.pot);
    state.detail = hand_name(winner.best);
  } else {
    state.status = "SPLIT POT " + std::to_string(state.pot);
    state.detail = hand_name(best);
  }
}

void award_uncontested(State& state) {
  int winner = 0;
  for (int seat = 0; seat < kPlayers; ++seat) {
    if (!state.players[static_cast<std::size_t>(seat)].folded) {
      winner = seat;
      break;
    }
  }
  state.players[static_cast<std::size_t>(winner)].chips += state.pot;
  state.screen = Screen::Result;
  state.winners = {winner};
  state.status = state.players[static_cast<std::size_t>(winner)].name + " TAKES " + std::to_string(state.pot);
  state.detail = "EVERYONE ELSE FOLDED";
}

void maybe_advance(State& state) {
  if (active_player_count(state) == 1) {
    award_uncontested(state);
    return;
  }

  bool settled = true;
  for (const auto& player : state.players) {
    if (player.folded) continue;
    if (!player.all_in && player.chips > 0) {
      if (!player.acted || player.street_contrib != state.current_bet) {
        settled = false;
        break;
      }
    }
  }

  if (!settled) {
    return;
  }

  if (state.stage == Stage::Preflop) {
    state.board[0] = draw_card(state);
    state.board[1] = draw_card(state);
    state.board[2] = draw_card(state);
    state.board_count = 3;
    reset_street(state, Stage::Flop);
  } else if (state.stage == Stage::Flop) {
    state.board[3] = draw_card(state);
    state.board_count = 4;
    reset_street(state, Stage::Turn);
  } else if (state.stage == Stage::Turn) {
    state.board[4] = draw_card(state);
    state.board_count = 5;
    reset_street(state, Stage::River);
  } else {
    state.stage = Stage::Showdown;
    resolve_showdown(state);
  }
}

std::vector<Action> legal_actions(const State& state, int seat) {
  const Player& player = state.players[static_cast<std::size_t>(seat)];
  std::vector<Action> actions;
  if (player.folded || player.all_in) {
    return actions;
  }
  const int to_call = to_call_amount(state, seat);
  actions.push_back(Action::Fold);
  actions.push_back(Action::CallCheck);
  if (player.chips >= to_call + kRaiseSize) {
    actions.push_back(Action::Raise);
  }
  if (player.chips > 0) {
    actions.push_back(Action::AllIn);
  }
  return actions;
}

void select_next_player(State& state, int acting_seat) {
  const int next = next_active_after(state, acting_seat);
  state.current_player = next;
  if (next >= 0) {
    queue_guard_if_needed(state);
    schedule_bot(state);
  }
}

int bot_raise_to(const State& state, int seat) {
  const Player& player = state.players[static_cast<std::size_t>(seat)];
  const bool paired = player.hole[0].rank == player.hole[1].rank;
  const bool suited = player.hole[0].suit == player.hole[1].suit;
  const int high = std::max(player.hole[0].rank, player.hole[1].rank);
  const int minimum = min_raise_to(state, seat);
  const int maximum = max_raise_to(state, seat);
  int target = minimum;
  if (paired && high >= 11) {
    target = minimum + kRaiseSize * 3;
  } else if (high >= 13) {
    target = minimum + kRaiseSize * 2;
  } else if (suited && high >= 11) {
    target = minimum + kRaiseSize;
  }
  return clamp_raise_to(state, seat, std::min(target, maximum));
}

void perform_action(State& state, int seat, Action action, int raise_to = -1) {
  Player& player = state.players[static_cast<std::size_t>(seat)];
  const int to_call = to_call_amount(state, seat);
  if (player.human) {
    if (state.stage == Stage::Preflop && !player.tracked_vpip &&
        ((action == Action::CallCheck && to_call > 0) || action == Action::Raise || action == Action::AllIn)) {
      player.tracked_vpip = true;
      ++state.human_style.vpip_hands;
    }
    if (!player.tracked_aggression && (action == Action::Raise || action == Action::AllIn)) {
      player.tracked_aggression = true;
      ++state.human_style.aggressive_hands;
    }
    if (action == Action::Fold) {
      if (to_call > 0) ++state.human_style.folds_to_pressure;
    } else if (action == Action::CallCheck) {
      if (to_call > 0) {
        ++state.human_style.call_actions;
      } else {
        ++state.human_style.check_actions;
      }
    } else if (action == Action::Raise) {
      ++state.human_style.raise_actions;
    } else if (action == Action::AllIn) {
      ++state.human_style.all_in_actions;
    }
  }

  if (action == Action::Fold) {
    player.folded = true;
    player.acted = true;
    state.status = player.name + " FOLDS";
    state.detail = "POT " + std::to_string(state.pot);
  } else if (action == Action::CallCheck) {
    contribute(player, to_call, state.pot);
    player.acted = true;
    state.status = player.name + (to_call > 0 ? " CALLS " : " CHECKS ");
    if (to_call > 0) {
      state.detail = "IN FOR " + std::to_string(player.hand_contrib);
    } else {
      state.detail = "POT " + std::to_string(state.pot);
    }
  } else if (action == Action::Raise) {
    const int raise_target = clamp_raise_to(state, seat, raise_to >= 0 ? raise_to : min_raise_to(state, seat));
    const int total = std::max(0, raise_target - player.street_contrib);
    contribute(player, total, state.pot);
    state.current_bet = player.street_contrib;
    for (auto& other : state.players) {
      if (&other != &player && !other.folded && !other.all_in) {
        other.acted = false;
      }
    }
    player.acted = true;
    state.status = player.name + " RAISES";
    state.detail = "TO " + std::to_string(state.current_bet);
  } else {
    const int before = player.street_contrib;
    const int shove = player.chips;
    contribute(player, player.chips, state.pot);
    if (player.street_contrib > state.current_bet) {
      state.current_bet = player.street_contrib;
      for (auto& other : state.players) {
        if (&other != &player && !other.folded && !other.all_in) {
          other.acted = false;
        }
      }
    }
    player.acted = true;
    if (before == player.street_contrib) {
      player.folded = true;
    }
    state.status = player.name + " SHOVES";
    state.detail = "FOR " + std::to_string(shove);
  }

  maybe_advance(state);
  if (state.screen == Screen::Play && state.stage != Stage::Showdown) {
    if (state.current_player == seat || state.current_player < 0) {
      select_next_player(state, seat);
    }
    maybe_advance(state);
  }
}

Action bot_choice(const State& state, int seat) {
  const Player& player = state.players[static_cast<std::size_t>(seat)];
  const int to_call = to_call_amount(state, seat);
  const HumanRead read = [&]() {
    HumanRead value{};
    value.looseness = safe_ratio(state.human_style.vpip_hands, state.human_style.hands_observed, value.looseness);
    value.foldiness = safe_ratio(state.human_style.folds_to_pressure, state.human_style.hands_observed, value.foldiness);
    value.bluffiness = safe_ratio(state.human_style.bluff_showdowns, state.human_style.showdown_hands, value.bluffiness);
    const double aggressive_actions =
        static_cast<double>(state.human_style.raise_actions) + static_cast<double>(state.human_style.all_in_actions) * 1.25;
    const double passive_actions =
        static_cast<double>(state.human_style.call_actions) + static_cast<double>(state.human_style.check_actions) * 0.65 + 1.0;
    value.aggression = std::clamp(aggressive_actions / passive_actions, 0.08, 1.20);
    return value;
  }();
  const bool paired = player.hole[0].rank == player.hole[1].rank;
  const bool suited = player.hole[0].suit == player.hole[1].suit;
  const int high = std::max(player.hole[0].rank, player.hole[1].rank);
  const int low = std::min(player.hole[0].rank, player.hole[1].rank);
  const double sticky = read.looseness * (1.0 - read.foldiness * 0.75);
  const double steal = read.foldiness * (1.0 - read.looseness * 0.45);
  const double trap = std::min(1.0, read.aggression * 0.55 + read.bluffiness * 0.95);
  const double pressure = static_cast<double>(to_call) / std::max(1, player.chips);
  const auto actions = legal_actions(state, seat);
  const bool can_raise = std::find(actions.begin(), actions.end(), Action::Raise) != actions.end();
  const bool can_shove = std::find(actions.begin(), actions.end(), Action::AllIn) != actions.end();
  const bool broadway = high >= 13 || (high >= 11 && low >= 10);
  const bool connected = std::abs(player.hole[0].rank - player.hole[1].rank) <= 2;

  if (to_call == 0) {
    if (can_raise &&
        (paired || broadway || (suited && connected && high >= 10) ||
         (high >= 12 && (steal > 0.22 || state.stage != Stage::Preflop)))) {
      if (trap > 0.55 && high >= 12 && !paired) {
        return Action::CallCheck;
      }
      return Action::Raise;
    }
    return Action::CallCheck;
  }
  if ((paired || broadway || (suited && connected && high >= 10)) && to_call <= 60 + static_cast<int>(steal * 20.0)) {
    if (can_raise && high >= 13 && state.stage != Stage::River && trap < 0.5) {
      return Action::Raise;
    }
    return Action::CallCheck;
  }
  if (to_call >= player.chips) {
    if (can_shove && (paired || high >= 13 || (suited && high >= 12))) {
      return Action::AllIn;
    }
    return (sticky > 0.35 && high >= 12) ? Action::CallCheck : Action::Fold;
  }
  if (can_raise && steal > 0.44 && pressure < 0.08 && high >= 11) {
    return Action::Raise;
  }
  if (pressure > 0.18 && sticky < 0.24 && high < 12 && !paired) {
    return Action::Fold;
  }
  return to_call <= 20 + static_cast<int>(sticky * 24.0) ? Action::CallCheck : Action::Fold;
}

void assign_players(State& state, int humans) {
  state.human_players = clamp_int(humans, 1, 4);
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    player.human = seat < state.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                               : std::string(kBotNames[static_cast<std::size_t>(seat - state.human_players)]);
    player.color = kSeatColors[static_cast<std::size_t>(seat)];
    if (player.chips <= 0) {
      player.chips = kStartingStack;
    }
  }
}

void start_hand(State& state) {
  state.screen = Screen::Play;
  state.paused = false;
  state.stage = Stage::Preflop;
  state.current_bet = 0;
  state.pot = 0;
  state.board_count = 0;
  state.action_index = 1;
  state.raise_to = 0;
  state.raise_picker = false;
  state.winners.clear();
  state.guard.active = false;
  state.dealer = next_player(state.dealer);

  shuffle_deck(state);
  for (auto& player : state.players) {
    if (player.chips <= 0) {
      player.chips = kStartingStack;
    }
    reset_round(player);
    if (player.human) {
      ++state.human_style.hands_observed;
    }
  }

  for (int card = 0; card < 2; ++card) {
    for (int seat = 0; seat < kPlayers; ++seat) {
      state.players[static_cast<std::size_t>(seat)].hole[card] = draw_card(state);
    }
  }

  const int small = next_player(state.dealer);
  const int big = next_player(small);
  contribute(state.players[static_cast<std::size_t>(small)], kSmallBlind, state.pot);
  contribute(state.players[static_cast<std::size_t>(big)], kBigBlind, state.pot);
  state.current_bet = kBigBlind;
  state.players[static_cast<std::size_t>(small)].acted = false;
  state.players[static_cast<std::size_t>(big)].acted = false;
  state.current_player = next_active_after(state, big);
  state.status = "PREFLOP";
  state.detail = "UP/DOWN MOVE / A CONFIRM / B FOLD";
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void start_match(State& state, int humans) {
  state.human_style = {};
  assign_players(state, humans);
  start_hand(state);
}

struct SeatLayout {
  SDL_Point label;
  SDL_Point cards;
  SDL_Point chips;
  SDL_Point dealer;
};

SeatLayout seat_layout(int seat) {
  static const std::array<SeatLayout, kPlayers> layouts{{
      {{256, 334}, {222, 348}, {256, 332}, {298, 366}},
      {{100, 292}, {54, 304}, {126, 274}, {150, 330}},
      {{106, 150}, {58, 164}, {128, 222}, {154, 134}},
      {{256, 116}, {222, 126}, {256, 190}, {300, 106}},
      {{406, 150}, {364, 164}, {386, 222}, {356, 134}},
      {{412, 292}, {368, 304}, {390, 274}, {360, 330}},
  }};
  return layouts[static_cast<std::size_t>(seat)];
}

void draw_table(SDL_Renderer* renderer) {
  draw_pack_frame(renderer, "RIVER FIVE", "HOLD'EM");
  fill_rect(renderer, {32, 114, 448, 360}, {8, 19, 18, 255});
  fill_circle(renderer, 256, 288, 174, {8, 19, 18, 255});
  fill_circle(renderer, 256, 284, 164, kTable);
  for (int ring = 0; ring < 7; ++ring) {
    draw_circle_outline(renderer, 256, 284, 145 + ring * 3, {34, 103, 71, 255});
  }
  draw_circle_outline(renderer, 256, 284, 164, kFrame);
  draw_circle_outline(renderer, 256, 284, 152, {48, 126, 88, 255});
}

void draw_state(SDL_Renderer* renderer, const State& state) {
  draw_table(renderer);
  const int board_x = 146;
  for (int i = 0; i < 5; ++i) {
    const int x = board_x + i * 44;
    if (i < state.board_count) {
      const Card card = state.board[static_cast<std::size_t>(i)];
      draw_standard_card_face(renderer, x, 226, rank_text_holdem(card.rank), suit_text(card.suit),
                              suit_color(card.suit));
    } else {
      draw_standard_card_back(renderer, x, 226, "VD");
    }
  }

  for (int seat = 0; seat < kPlayers; ++seat) {
    const SeatLayout layout = seat_layout(seat);
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    SDL_Color label = player.folded ? kMuted : player.color;
    if (seat == state.current_player && state.screen == Screen::Play) label = kAccent;
    draw_text(renderer, player.name, layout.label.x, layout.label.y, 1, label, true);
    draw_text(renderer, std::to_string(player.chips) + " C", layout.label.x, layout.label.y + 12, 1, kText, true);
    if (player.folded) draw_text(renderer, "FOLDED", layout.label.x, layout.label.y + 24, 1, kMuted, true);
    if (player.all_in && !player.folded) draw_text(renderer, "ALL IN", layout.label.x, layout.label.y + 24, 1, kGold, true);
    if (seat == state.dealer) {
      fill_circle(renderer, layout.dealer.x, layout.dealer.y, 9, kGold);
      draw_text(renderer, "D", layout.dealer.x, layout.dealer.y - 5, 1, kPanel, true);
    }
    const bool reveal = state.screen == Screen::Result || (!state.guard.active && player.human && seat == state.current_player);
    if (reveal) {
      draw_standard_card_face(renderer, layout.cards.x, layout.cards.y, rank_text_holdem(player.hole[0].rank),
                              suit_text(player.hole[0].suit), suit_color(player.hole[0].suit));
      draw_standard_card_face(renderer, layout.cards.x + 26, layout.cards.y, rank_text_holdem(player.hole[1].rank),
                              suit_text(player.hole[1].suit), suit_color(player.hole[1].suit));
    } else {
      draw_standard_card_back(renderer, layout.cards.x, layout.cards.y, "VD");
      draw_standard_card_back(renderer, layout.cards.x + 26, layout.cards.y, "VD");
    }
    draw_chip_stack(renderer, layout.chips.x, layout.chips.y, player.hand_contrib,
                    seat == state.current_player ? kAccent : kFrame);
  }

  draw_panel(renderer, {24, 116, 176, 46}, "STREET");
  draw_text(renderer, stage_text(state.stage), 112, 124, 2, kText, true);
  draw_text(renderer, "POT " + std::to_string(state.pot), 34, 142, 1, kGold);
  draw_text_right(renderer, "BET " + std::to_string(state.current_bet), 190, 142, 1, kText);

  draw_panel(renderer, {24, 170, 176, 88}, "CURRENT");
  if (state.current_player >= 0) {
    const Player& current = state.players[static_cast<std::size_t>(state.current_player)];
    const int to_call = std::max(0, state.current_bet - current.street_contrib);
    draw_text(renderer, current.name, 112, 188, 2, current.human ? current.color : kText, true);
    draw_text(renderer, "TO CALL " + std::to_string(to_call), 112, 212, 1, kText, true);
    draw_text(renderer, current.human ? "YOUR TURN" : "BOT THINKS", 112, 228, 1, kMuted, true);
  }

  draw_panel(renderer, {362, 116, 126, 346}, "TABLE");
  draw_text(renderer, "POT", 374, 136, 1, kMuted);
  draw_text_right(renderer, std::to_string(state.pot), 478, 136, 1, kGold);
  draw_text(renderer, "ROUND", 374, 154, 1, kMuted);
  draw_text_right(renderer, stage_text(state.stage), 478, 154, 1, kText);
  draw_text(renderer, "DEALER", 374, 172, 1, kMuted);
  draw_text_right(renderer, state.players[static_cast<std::size_t>(state.dealer)].name, 478, 172, 1, kAccent);
  draw_text(renderer, "START STACK", 374, 190, 1, kMuted);
  draw_text_right(renderer, std::to_string(kStartingStack), 478, 190, 1, kAccent2);

  for (int seat = 0; seat < kPlayers; ++seat) {
    const int y = 222 + seat * 18;
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    if (seat == state.current_player && state.screen == Screen::Play) {
      fill_rect(renderer, {372, y - 2, 108, 14}, kPanelHi);
    }
    draw_text(renderer, fit_text(player.name, 60, 1), 376, y, 1, player.color);
    draw_text_right(renderer, std::to_string(player.chips), 478, y, 1, player.folded ? kMuted : kText);
  }

  draw_panel(renderer, {372, 332, 108, 116}, "STATUS");
  draw_text(renderer, fit_text(state.status, 88, 1), 426, 352, 1, kText, true);
  draw_text(renderer, fit_text(state.detail, 96, 1), 426, 378, 1, kMuted, true);
  draw_text(renderer, "SELECT MENU", 426, 420, 1, kMuted, true);

  if (state.screen == Screen::Play && state.current_player >= 0 &&
      state.players[static_cast<std::size_t>(state.current_player)].human) {
    const auto actions = legal_actions(state, state.current_player);
    draw_panel(renderer, {24, 364, 176, 98}, "ACTION");
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
      const SDL_Rect row{32, 384 + i * 18, 160, 14};
      if (i == state.action_index) {
        fill_rect(renderer, row, kPanelHi);
      }
      draw_text(renderer, action_label(state, state.current_player, actions[static_cast<std::size_t>(i)]),
                row.x + 4, row.y + 2, 1, i == state.action_index ? kAccent : kText);
    }
    draw_text(renderer, "B QUICK FOLD", 112, 446, 1, kMuted, true);
  }

  if (state.raise_picker && state.current_player >= 0) {
    const Player& player = state.players[static_cast<std::size_t>(state.current_player)];
    const int call_amount = to_call_amount(state, state.current_player);
    const int raise_by = std::max(0, state.raise_to - state.current_bet);
    const int extra_commit = std::max(0, state.raise_to - player.street_contrib);
    fill_rect(renderer, {86, 176, 340, 124}, {8, 12, 18, 240});
    draw_rect(renderer, {86, 176, 340, 124}, kFrame);
    draw_text(renderer, "SET RAISE", 256, 188, 3, kGold, true);
    draw_text(renderer, "CALL " + std::to_string(call_amount), 110, 226, 1, kText);
    draw_text_right(renderer, "TO " + std::to_string(state.raise_to), 402, 226, 2, kAccent);
    draw_text(renderer, "RAISE BY " + std::to_string(raise_by), 110, 248, 1, kAccent2);
    draw_text_right(renderer, "COMMIT " + std::to_string(extra_commit), 402, 248, 1, kText);
    draw_text(renderer, "UP/DOWN +/-20", 256, 270, 1, kText, true);
    draw_text(renderer, "LEFT/RIGHT +/-100  A CONFIRM  B CANCEL", 256, 286, 1, kMuted, true);
  }

  if (state.screen == Screen::Result) {
    fill_rect(renderer, {78, 188, 356, 108}, {8, 12, 18, 236});
    draw_rect(renderer, {78, 188, 356, 108}, kFrame);
    draw_text(renderer, "HAND COMPLETE", 256, 198, 3, kGold, true);
    draw_text(renderer, fit_text(state.status, 320, 1), 256, 238, 1, kText, true);
    draw_text(renderer, fit_text(state.detail, 320, 1), 256, 258, 1, kMuted, true);
    draw_text(renderer, "A NEXT HAND", 256, 278, 1, kAccent, true);
  }

  draw_guard(renderer, state.guard);
}

bool update(State& state, ToneState& tone, const EdgeInput& edge) {
  if (edge.select) {
    return true;
  }
  if (update_guard(state.guard, edge, tone)) {
    return false;
  }

  if (state.screen == Screen::Result) {
    if (edge.a || edge.start) {
      start_hand(state);
      trigger_tone(tone, 700.0f, 60);
    }
    return false;
  }

  if (edge.start) {
    state.paused = !state.paused;
    state.status = state.paused ? "PAUSED" : stage_text(state.stage);
    trigger_tone(tone, state.paused ? 300.0f : 520.0f, 36);
    return false;
  }
  if (state.paused) {
    return false;
  }

  if (state.current_player >= 0 && !state.players[static_cast<std::size_t>(state.current_player)].human) {
    if (SDL_GetTicks() >= state.bot_ready_at) {
      const Action action = bot_choice(state, state.current_player);
      const int raise_to = action == Action::Raise ? bot_raise_to(state, state.current_player) : -1;
      perform_action(state, state.current_player, action, raise_to);
    }
    return false;
  }

  if (state.current_player < 0) {
    return false;
  }

  if (state.raise_picker) {
    const int seat = state.current_player;
    if (edge.left) {
      state.raise_to = clamp_raise_to(state, seat, state.raise_to - kRaiseSize * 5);
      trigger_tone(tone, 380.0f, 20);
    }
    if (edge.right) {
      state.raise_to = clamp_raise_to(state, seat, state.raise_to + kRaiseSize * 5);
      trigger_tone(tone, 540.0f, 20);
    }
    if (edge.up) {
      state.raise_to = clamp_raise_to(state, seat, state.raise_to + kRaiseSize);
      trigger_tone(tone, 480.0f, 20);
    }
    if (edge.down) {
      state.raise_to = clamp_raise_to(state, seat, state.raise_to - kRaiseSize);
      trigger_tone(tone, 320.0f, 20);
    }
    if (edge.b) {
      state.raise_picker = false;
      trigger_tone(tone, 260.0f, 30);
    }
    if (edge.a) {
      state.raise_picker = false;
      perform_action(state, seat, Action::Raise, state.raise_to);
      trigger_tone(tone, 640.0f, 40);
    }
    return false;
  }

  auto actions = legal_actions(state, state.current_player);
  if (actions.empty()) {
    return false;
  }
  if (edge.up || edge.left) {
    state.action_index = (state.action_index + static_cast<int>(actions.size()) - 1) % static_cast<int>(actions.size());
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.down || edge.right) {
    state.action_index = (state.action_index + 1) % static_cast<int>(actions.size());
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.b) {
    perform_action(state, state.current_player, Action::Fold);
    trigger_tone(tone, 260.0f, 40);
    return false;
  }
  if (edge.a) {
    const int index = clamp_int(state.action_index, 0, static_cast<int>(actions.size()) - 1);
    const Action action = actions[static_cast<std::size_t>(index)];
    if (action == Action::Raise) {
      state.raise_picker = true;
      state.raise_to = clamp_raise_to(state, state.current_player, min_raise_to(state, state.current_player));
      trigger_tone(tone, 520.0f, 30);
      return false;
    }
    perform_action(state, state.current_player, action);
    trigger_tone(tone, 640.0f, 40);
  }
  return false;
}

}  // namespace holdem

// ---------------------------------------------------------------------------
// Sevens Run
// ---------------------------------------------------------------------------

namespace sevens {

constexpr int kPlayers = 4;

struct Card {
  int rank = 1;
  int suit = 0;
};

struct Player {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  bool human = false;
  std::vector<Card> hand;
  int rounds_won = 0;
};

struct State {
  std::mt19937 rng{0x5337564Eu};
  std::array<Player, kPlayers> players{};
  std::array<int, 4> low{{8, 8, 8, 8}};
  std::array<int, 4> high{{6, 6, 6, 6}};
  int human_players = 1;
  int current_player = 0;
  int selected = 0;
  int winner = -1;
  int round = 1;
  bool result = false;
  Uint32 bot_ready_at = 0;
  std::string status = "SEVENS OPEN THE LANES";
  std::string detail = "LAY FROM SEVENS OUTWARD";
  HotseatGuard guard{};
};

bool card_less(const Card& a, const Card& b) {
  if (a.suit != b.suit) return a.suit < b.suit;
  return a.rank < b.rank;
}

std::vector<Card> build_deck() {
  std::vector<Card> deck;
  deck.reserve(52);
  for (int suit = 0; suit < 4; ++suit) {
    for (int rank = 1; rank <= 13; ++rank) {
      deck.push_back(Card{rank, suit});
    }
  }
  return deck;
}

bool legal_play(const State& state, const Card& card) {
  if (card.rank == 7) {
    return state.low[static_cast<std::size_t>(card.suit)] == 8 &&
           state.high[static_cast<std::size_t>(card.suit)] == 6;
  }
  if (state.low[static_cast<std::size_t>(card.suit)] == 8) {
    return false;
  }
  return card.rank == state.low[static_cast<std::size_t>(card.suit)] - 1 ||
         card.rank == state.high[static_cast<std::size_t>(card.suit)] + 1;
}

std::vector<int> legal_indices(const State& state, int seat) {
  std::vector<int> result;
  const auto& hand = state.players[static_cast<std::size_t>(seat)].hand;
  for (int i = 0; i < static_cast<int>(hand.size()); ++i) {
    if (legal_play(state, hand[static_cast<std::size_t>(i)])) {
      result.push_back(i);
    }
  }
  return result;
}

void queue_guard_if_needed(State& state) {
  const Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  if (player.human && state.human_players > 1) {
    open_guard(state.guard, player.name + " TURN", "PASS CONTROLLER AND PRESS A", player.color);
  }
}

void schedule_bot(State& state) {
  if (!state.players[static_cast<std::size_t>(state.current_player)].human) {
    state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
  }
}

void assign_players(State& state, int humans) {
  state.human_players = clamp_int(humans, 1, 4);
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    player.human = seat < state.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                               : std::string(kBotNames[static_cast<std::size_t>(seat - state.human_players)]);
    player.color = kSeatColors[static_cast<std::size_t>(seat)];
  }
}

void start_round(State& state) {
  state.low = {8, 8, 8, 8};
  state.high = {6, 6, 6, 6};
  state.result = false;
  state.winner = -1;
  state.selected = 0;
  state.guard.active = false;
  state.status = "SEVENS OPEN THE LANES";
  state.detail = "A PLAY / B PASS IF STUCK";

  auto deck = build_deck();
  shuffle_vector(deck, state.rng);
  for (auto& player : state.players) {
    player.hand.clear();
  }
  for (int i = 0; i < 52; ++i) {
    state.players[static_cast<std::size_t>(i % kPlayers)].hand.push_back(deck[static_cast<std::size_t>(i)]);
  }
  for (auto& player : state.players) {
    std::sort(player.hand.begin(), player.hand.end(), card_less);
  }

  state.current_player = 0;
  for (int seat = 0; seat < kPlayers; ++seat) {
    const auto& hand = state.players[static_cast<std::size_t>(seat)].hand;
    const bool has_diamond_seven = std::find_if(hand.begin(), hand.end(), [](const Card& card) {
                                  return card.rank == 7 && card.suit == 2;
                                }) != hand.end();
    if (has_diamond_seven) {
      state.current_player = seat;
      break;
    }
  }
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void start_match(State& state, int humans) {
  assign_players(state, humans);
  start_round(state);
}

void advance(State& state) {
  state.current_player = (state.current_player + 1) % kPlayers;
  state.selected = 0;
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void play_index(State& state, int index) {
  Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  const Card card = player.hand[static_cast<std::size_t>(index)];
  if (card.rank == 7) {
    state.low[static_cast<std::size_t>(card.suit)] = 7;
    state.high[static_cast<std::size_t>(card.suit)] = 7;
  } else if (card.rank < state.low[static_cast<std::size_t>(card.suit)]) {
    state.low[static_cast<std::size_t>(card.suit)] = card.rank;
  } else {
    state.high[static_cast<std::size_t>(card.suit)] = card.rank;
  }
  player.hand.erase(player.hand.begin() + index);
  if (player.hand.empty()) {
    player.rounds_won += 1;
    state.winner = state.current_player;
    state.result = true;
    state.status = player.name + " CLEARS THE DECK";
    state.detail = "A NEXT ROUND";
    return;
  }
  state.status = player.name + " LAYS " + rank_text_standard(card.rank) + suit_text(card.suit);
  advance(state);
}

int bot_choice(const State& state) {
  const auto legal = legal_indices(state, state.current_player);
  if (legal.empty()) return -1;
  const auto& hand = state.players[static_cast<std::size_t>(state.current_player)].hand;
  int best_index = legal.front();
  int best_score = -999;
  for (int index : legal) {
    const Card card = hand[static_cast<std::size_t>(index)];
    int score = 0;
    if (card.rank == 7) score += 4;
    if (card.rank == 1 || card.rank == 13) score += 2;
    for (const Card& other : hand) {
      if (other.suit == card.suit && std::abs(other.rank - card.rank) == 1) score += 2;
    }
    if (score > best_score) {
      best_score = score;
      best_index = index;
    }
  }
  return best_index;
}

void draw_board(SDL_Renderer* renderer, const State& state) {
  draw_pack_frame(renderer, "SEVENS RUN", "FANTAN");
  draw_panel(renderer, {26, 116, 460, 230}, "LANES");
  const std::array<std::string, 4> suit_names{{"SPADES", "HEARTS", "DIAMONDS", "CLUBS"}};
  for (int suit = 0; suit < 4; ++suit) {
    const int row_y = 136 + suit * 50;
    draw_text(renderer, suit_names[static_cast<std::size_t>(suit)], 40, row_y + 16, 1, kMuted);
    for (int rank = 1; rank <= 13; ++rank) {
      const bool open = state.low[static_cast<std::size_t>(suit)] <= rank &&
                        state.high[static_cast<std::size_t>(suit)] >= rank;
      const int x = 188 + (rank - 1) * 22;
      if (open) {
        draw_standard_card_face(renderer, x, row_y, rank_text_standard(rank), suit_text(suit), suit_color(suit));
      } else {
        fill_rect(renderer, {x + 5, row_y + 18, 12, 20}, rank == 7 ? kPanelHi : SDL_Color{29, 41, 50, 255});
        draw_rect(renderer, {x + 5, row_y + 18, 12, 20}, rank == 7 ? kAccent : kFrame);
      }
    }
  }

  draw_panel(renderer, {26, 356, 460, 120}, "HANDS");
  for (int seat = 0; seat < kPlayers; ++seat) {
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    const int y = 374 + seat * 18;
    if (seat == state.current_player && !state.result) {
      fill_rect(renderer, {34, y - 2, 128, 14}, kPanelHi);
    }
    draw_text(renderer, player.name, 38, y, 1, player.color);
    draw_text_right(renderer, std::to_string(player.hand.size()) + " CARD", 154, y, 1, kText);
    draw_text(renderer, std::to_string(player.rounds_won) + " WIN", 170, y, 1, kMuted);
  }

  const Player& current = state.players[static_cast<std::size_t>(state.current_player)];
  const auto legal = legal_indices(state, state.current_player);
  const int count = static_cast<int>(current.hand.size());
  const int spacing = count > 1 ? clamp_int(360 / (count - 1), 20, 28) : 28;
  const int start_x = 264 - ((count - 1) * spacing) / 2;
  for (int i = 0; i < count; ++i) {
    const Card card = current.hand[static_cast<std::size_t>(i)];
    const bool legal_card = std::find(legal.begin(), legal.end(), i) != legal.end();
    int y = 402;
    if (!state.result && current.human && i == state.selected) y -= 10;
    draw_standard_card_face(renderer, start_x + i * spacing, y, rank_text_standard(card.rank), suit_text(card.suit),
                            legal_card ? suit_color(card.suit) : kMuted);
    if (!legal_card) {
      fill_rect(renderer, {start_x + i * spacing + 2, y + 2, 38, 54}, {0, 0, 0, 90});
      draw_rect(renderer, {start_x + i * spacing, y, 42, 58}, kMuted);
    }
  }

  draw_panel(renderer, {352, 120, 128, 86}, "TURN");
  draw_text(renderer, current.name, 416, 142, 2, current.color, true);
  draw_text(renderer, legal.empty() ? "PASS READY" : "PLAY READY", 416, 166, 1, legal.empty() ? kDanger : kAccent2, true);
  draw_text(renderer, fit_text(state.status, 112, 1), 416, 184, 1, kText, true);

  if (state.result) {
    fill_rect(renderer, {98, 186, 316, 92}, {8, 12, 18, 236});
    draw_rect(renderer, {98, 186, 316, 92}, kFrame);
    draw_text(renderer, "ROUND COMPLETE", 256, 196, 3, kGold, true);
    draw_text(renderer, fit_text(state.status, 280, 1), 256, 236, 1, kText, true);
    draw_text(renderer, "A NEXT ROUND", 256, 256, 1, kAccent, true);
  }

  draw_guard(renderer, state.guard);
}

bool update(State& state, ToneState& tone, const EdgeInput& edge) {
  if (edge.select) {
    return true;
  }
  if (update_guard(state.guard, edge, tone)) {
    return false;
  }
  if (state.result) {
    if (edge.a || edge.start) {
      state.round += 1;
      start_round(state);
      trigger_tone(tone, 680.0f, 60);
    }
    return false;
  }

  if (!state.players[static_cast<std::size_t>(state.current_player)].human) {
    if (SDL_GetTicks() >= state.bot_ready_at) {
      const int choice = bot_choice(state);
      if (choice >= 0) {
        play_index(state, choice);
      } else {
        state.status = state.players[static_cast<std::size_t>(state.current_player)].name + " PASSES";
        advance(state);
      }
    }
    return false;
  }

  const auto legal = legal_indices(state, state.current_player);
  Player& current = state.players[static_cast<std::size_t>(state.current_player)];
  const int hand_size = static_cast<int>(current.hand.size());
  if (hand_size == 0) {
    return false;
  }
  state.selected = clamp_int(state.selected, 0, hand_size - 1);
  if (edge.left) {
    state.selected = (state.selected + hand_size - 1) % hand_size;
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.right) {
    state.selected = (state.selected + 1) % hand_size;
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.b && legal.empty()) {
    state.status = current.name + " PASSES";
    advance(state);
    trigger_tone(tone, 300.0f, 32);
  }
  if (edge.a) {
    if (legal_play(state, current.hand[static_cast<std::size_t>(state.selected)])) {
      play_index(state, state.selected);
      trigger_tone(tone, 620.0f, 36);
    } else if (legal.empty()) {
      state.status = current.name + " PASSES";
      advance(state);
      trigger_tone(tone, 300.0f, 32);
    } else {
      state.detail = "LAY A SEVEN OR AN ADJACENT CARD";
      trigger_tone(tone, 220.0f, 30);
    }
  }
  return false;
}

}  // namespace sevens

// ---------------------------------------------------------------------------
// Wild Sweep
// ---------------------------------------------------------------------------

namespace wildsweep {

constexpr int kPlayers = 4;

enum class Color { Coral = 0, Teal, Gold, Violet, Wild };
enum class Kind { Number = 0, Skip, Reverse, DrawTwo, Wild, WildDrawFour };

struct Card {
  Color color = Color::Coral;
  Kind kind = Kind::Number;
  int value = 0;
};

struct Player {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  bool human = false;
  std::vector<Card> hand;
  int wins = 0;
};

struct State {
  std::mt19937 rng{0x57494C44u};
  std::array<Player, kPlayers> players{};
  std::vector<Card> draw_pile;
  std::vector<Card> discard;
  int human_players = 1;
  int current_player = 0;
  int direction = 1;
  int selected = 0;
  int winner = -1;
  Uint32 bot_ready_at = 0;
  bool choosing_color = false;
  int color_cursor = 0;
  bool result = false;
  HotseatGuard guard{};
  std::string status = "MATCH COLOR OR SYMBOL";
  std::string detail = "B DRAWS WHEN YOU ARE STUCK";
};

SDL_Color card_color(Color color) {
  switch (color) {
    case Color::Coral: return {236, 94, 108, 255};
    case Color::Teal: return {84, 202, 192, 255};
    case Color::Gold: return {238, 198, 72, 255};
    case Color::Violet: return {174, 109, 236, 255};
    default: return {241, 241, 241, 255};
  }
}

std::string kind_label(const Card& card) {
  switch (card.kind) {
    case Kind::Number: return std::to_string(card.value);
    case Kind::Skip: return "SK";
    case Kind::Reverse: return "RV";
    case Kind::DrawTwo: return "+2";
    case Kind::Wild: return "W";
    default: return "+4";
  }
}

std::vector<Card> build_deck() {
  std::vector<Card> deck;
  for (int color = 0; color < 4; ++color) {
    deck.push_back(Card{static_cast<Color>(color), Kind::Number, 0});
    for (int copy = 0; copy < 2; ++copy) {
      for (int value = 1; value <= 9; ++value) {
        deck.push_back(Card{static_cast<Color>(color), Kind::Number, value});
      }
      deck.push_back(Card{static_cast<Color>(color), Kind::Skip, 0});
      deck.push_back(Card{static_cast<Color>(color), Kind::Reverse, 0});
      deck.push_back(Card{static_cast<Color>(color), Kind::DrawTwo, 0});
    }
  }
  for (int i = 0; i < 4; ++i) {
    deck.push_back(Card{Color::Wild, Kind::Wild, 0});
    deck.push_back(Card{Color::Wild, Kind::WildDrawFour, 0});
  }
  return deck;
}

int next_index(const State& state, int seat) {
  return (seat + state.direction + kPlayers) % kPlayers;
}

bool can_play(const Card& card, const Card& top, Color current_color) {
  if (card.kind == Kind::Wild || card.kind == Kind::WildDrawFour) return true;
  if (card.color == current_color) return true;
  if (top.kind == Kind::Number && card.kind == Kind::Number && top.value == card.value) return true;
  if (card.kind == top.kind && card.kind != Kind::Number) return true;
  return false;
}

void queue_guard_if_needed(State& state) {
  const Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  if (player.human && state.human_players > 1) {
    open_guard(state.guard, player.name + " TURN", "PASS CONTROLLER AND PRESS A", player.color);
  }
}

void schedule_bot(State& state) {
  if (!state.players[static_cast<std::size_t>(state.current_player)].human) {
    state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
  }
}

Color current_color(const State& state) {
  const Card& top = state.discard.back();
  if (top.kind == Kind::Wild || top.kind == Kind::WildDrawFour) {
    return static_cast<Color>(state.color_cursor);
  }
  return top.color;
}

void assign_players(State& state, int humans) {
  state.human_players = clamp_int(humans, 1, 4);
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    player.human = seat < state.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                               : std::string(kBotNames[static_cast<std::size_t>(seat - state.human_players)]);
    player.color = kSeatColors[static_cast<std::size_t>(seat)];
  }
}

void replenish_if_needed(State& state) {
  if (!state.draw_pile.empty() || state.discard.size() <= 1) {
    return;
  }
  Card top = state.discard.back();
  state.discard.pop_back();
  state.draw_pile = state.discard;
  state.discard.clear();
  state.discard.push_back(top);
  shuffle_vector(state.draw_pile, state.rng);
}

Card draw_one(State& state) {
  replenish_if_needed(state);
  Card card = state.draw_pile.back();
  state.draw_pile.pop_back();
  return card;
}

void advance_turn(State& state, int skips = 1) {
  for (int i = 0; i < skips; ++i) {
    state.current_player = next_index(state, state.current_player);
  }
  state.selected = 0;
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void start_round(State& state) {
  state.result = false;
  state.winner = -1;
  state.guard.active = false;
  state.choosing_color = false;
  state.direction = 1;
  state.selected = 0;
  state.status = "MATCH COLOR OR SYMBOL";
  state.detail = "B DRAWS WHEN YOU ARE STUCK";

  auto deck = build_deck();
  shuffle_vector(deck, state.rng);
  state.draw_pile = deck;
  state.discard.clear();
  for (auto& player : state.players) {
    player.hand.clear();
  }
  for (int card = 0; card < 7; ++card) {
    for (int seat = 0; seat < kPlayers; ++seat) {
      state.players[static_cast<std::size_t>(seat)].hand.push_back(draw_one(state));
    }
  }
  Card first = draw_one(state);
  while (first.kind == Kind::Wild || first.kind == Kind::WildDrawFour) {
    state.draw_pile.insert(state.draw_pile.begin(), first);
    shuffle_vector(state.draw_pile, state.rng);
    first = draw_one(state);
  }
  state.discard.push_back(first);
  state.color_cursor = static_cast<int>(first.color);
  state.current_player = 0;
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void start_match(State& state, int humans) {
  assign_players(state, humans);
  start_round(state);
}

int choose_color_for_hand(const std::vector<Card>& hand) {
  std::array<int, 4> counts{{0, 0, 0, 0}};
  for (const Card& card : hand) {
    if (card.color != Color::Wild) {
      counts[static_cast<std::size_t>(card.color)]++;
    }
  }
  return static_cast<int>(std::max_element(counts.begin(), counts.end()) - counts.begin());
}

void finish_round(State& state, int winner) {
  state.winner = winner;
  state.players[static_cast<std::size_t>(winner)].wins += 1;
  state.result = true;
  state.status = state.players[static_cast<std::size_t>(winner)].name + " SWEEPS THE ROUND";
  state.detail = "A NEXT DEAL";
}

void apply_card(State& state, Card card, int seat, int card_index, int chosen_color) {
  Player& player = state.players[static_cast<std::size_t>(seat)];
  player.hand.erase(player.hand.begin() + card_index);
  state.discard.push_back(card);
  state.color_cursor = card.kind == Kind::Wild || card.kind == Kind::WildDrawFour
                           ? chosen_color
                           : static_cast<int>(card.color);
  state.status = player.name + " PLAYS " + kind_label(card);

  if (player.hand.empty()) {
    finish_round(state, seat);
    return;
  }

  if (card.kind == Kind::Reverse) {
    state.direction *= -1;
    advance_turn(state, 1);
  } else if (card.kind == Kind::Skip) {
    advance_turn(state, 2);
  } else if (card.kind == Kind::DrawTwo) {
    const int target = next_index(state, seat);
    state.players[static_cast<std::size_t>(target)].hand.push_back(draw_one(state));
    state.players[static_cast<std::size_t>(target)].hand.push_back(draw_one(state));
    state.status = player.name + " HITS +2";
    advance_turn(state, 2);
  } else if (card.kind == Kind::WildDrawFour) {
    const int target = next_index(state, seat);
    for (int i = 0; i < 4; ++i) {
      state.players[static_cast<std::size_t>(target)].hand.push_back(draw_one(state));
    }
    state.status = player.name + " HITS +4";
    advance_turn(state, 2);
  } else {
    advance_turn(state, 1);
  }
}

int bot_pick_card(const State& state) {
  const Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  const Card& top = state.discard.back();
  const Color live_color = current_color(state);
  int best = -1;
  int best_score = -999;
  for (int i = 0; i < static_cast<int>(player.hand.size()); ++i) {
    const Card& card = player.hand[static_cast<std::size_t>(i)];
    if (!can_play(card, top, live_color)) continue;
    int score = 0;
    if (card.kind == Kind::DrawTwo || card.kind == Kind::WildDrawFour) score += 7;
    if (card.kind == Kind::Skip || card.kind == Kind::Reverse) score += 4;
    if (card.kind == Kind::Wild) score += 2;
    if (card.kind == Kind::Number) score += card.value;
    if (player.hand.size() <= 3) score += 5;
    if (score > best_score) {
      best_score = score;
      best = i;
    }
  }
  return best;
}

void bot_turn(State& state) {
  Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  const int choice = bot_pick_card(state);
  if (choice >= 0) {
    const int chosen_color = choose_color_for_hand(player.hand);
    apply_card(state, player.hand[static_cast<std::size_t>(choice)], state.current_player, choice, chosen_color);
  } else {
    player.hand.push_back(draw_one(state));
    state.status = player.name + " DRAWS";
    advance_turn(state, 1);
  }
}

void draw_uno_card(SDL_Renderer* renderer, int x, int y, const Card& card, bool face_up, bool highlight = false) {
  SDL_Color fill = face_up ? card_color(card.color) : SDL_Color{38, 49, 63, 255};
  const SDL_Rect rect{x, y, 48, 64};
  fill_rect(renderer, {x + 3, y + 3, 48, 64}, {0, 0, 0, 100});
  fill_rect(renderer, rect, fill);
  draw_rect(renderer, rect, highlight ? kAccent : kFrame);
  draw_rect(renderer, {x + 3, y + 3, 42, 58}, {244, 244, 244, 100});
  if (face_up) {
    draw_text(renderer, kind_label(card), x + 24, y + 22, 3, card.color == Color::Gold ? kPanel : kText, true);
  } else {
    draw_text(renderer, "VD", x + 24, y + 22, 2, kGold, true);
  }
}

void draw_state(SDL_Renderer* renderer, const State& state) {
  draw_pack_frame(renderer, "WILD SWEEP", "UNO");
  draw_panel(renderer, {26, 116, 460, 126}, "TABLE");
  draw_uno_card(renderer, 210, 150, Card{Color::Wild, Kind::Number, 0}, false);
  draw_uno_card(renderer, 266, 150, state.discard.back(), true, true);
  draw_text(renderer, "DRAW", 234, 218, 1, kMuted, true);
  draw_text(renderer, "LIVE COLOR", 328, 136, 1, kMuted, true);
  fill_rect(renderer, {300, 154, 56, 26}, card_color(current_color(state)));
  draw_rect(renderer, {300, 154, 56, 26}, kFrame);
  draw_text(renderer, fit_text(state.status, 140, 1), 392, 164, 1, kText, true);
  draw_text(renderer, fit_text(state.detail, 140, 1), 392, 182, 1, kMuted, true);

  for (int seat = 0; seat < kPlayers; ++seat) {
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    const SDL_Point point = seat == 0 ? SDL_Point{256, 262}
                                      : seat == 1 ? SDL_Point{84, 200}
                                                  : seat == 2 ? SDL_Point{256, 120}
                                                              : SDL_Point{428, 200};
    if (seat == state.current_player && !state.result) {
      fill_rect(renderer, {point.x - 48, point.y - 10, 96, 14}, kPanelHi);
    }
    draw_text(renderer, player.name, point.x, point.y, 1, player.color, true);
    draw_text(renderer, std::to_string(player.hand.size()) + " CARD", point.x, point.y + 12, 1, kText, true);
    draw_text(renderer, std::to_string(player.wins) + " WIN", point.x, point.y + 24, 1, kMuted, true);
  }

  draw_panel(renderer, {26, 326, 460, 150}, "HAND");
  const Player& current = state.players[static_cast<std::size_t>(state.current_player)];
  const int hand_count = static_cast<int>(current.hand.size());
  const int spacing = hand_count > 1 ? clamp_int(372 / (hand_count - 1), 16, 32) : 32;
  const int start_x = 256 - ((hand_count - 1) * spacing) / 2 - 24;
  for (int i = 0; i < hand_count; ++i) {
    const Card& card = current.hand[static_cast<std::size_t>(i)];
    int y = 372;
    if (current.human && i == state.selected && !state.choosing_color) y -= 10;
    draw_uno_card(renderer, start_x + i * spacing, y, card, true, current.human && i == state.selected);
  }

  if (state.choosing_color) {
    fill_rect(renderer, {108, 172, 296, 100}, {8, 12, 18, 238});
    draw_rect(renderer, {108, 172, 296, 100}, kFrame);
    draw_text(renderer, "CHOOSE COLOR", 256, 184, 3, kGold, true);
    for (int i = 0; i < 4; ++i) {
      const SDL_Rect swatch{138 + i * 58, 222, 40, 20};
      fill_rect(renderer, swatch, card_color(static_cast<Color>(i)));
      draw_rect(renderer, swatch, i == state.color_cursor ? kAccent : kFrame);
    }
  }

  if (state.result) {
    fill_rect(renderer, {86, 184, 340, 96}, {8, 12, 18, 236});
    draw_rect(renderer, {86, 184, 340, 96}, kFrame);
    draw_text(renderer, "ROUND COMPLETE", 256, 194, 3, kGold, true);
    draw_text(renderer, fit_text(state.status, 300, 1), 256, 234, 1, kText, true);
    draw_text(renderer, "A NEXT DEAL", 256, 254, 1, kAccent, true);
  }

  draw_guard(renderer, state.guard);
}

bool update(State& state, ToneState& tone, const EdgeInput& edge) {
  if (edge.select) {
    return true;
  }
  if (update_guard(state.guard, edge, tone)) {
    return false;
  }
  if (state.result) {
    if (edge.a || edge.start) {
      start_round(state);
      trigger_tone(tone, 700.0f, 60);
    }
    return false;
  }

  if (!state.players[static_cast<std::size_t>(state.current_player)].human) {
    if (SDL_GetTicks() >= state.bot_ready_at) {
      bot_turn(state);
    }
    return false;
  }

  Player& current = state.players[static_cast<std::size_t>(state.current_player)];
  const Color live_color = current_color(state);
  const Card top = state.discard.back();
  const int hand_size = static_cast<int>(current.hand.size());
  if (hand_size == 0) {
    return false;
  }
  state.selected = clamp_int(state.selected, 0, hand_size - 1);

  if (state.choosing_color) {
    if (edge.left || edge.up) {
      state.color_cursor = (state.color_cursor + 3) % 4;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right || edge.down) {
      state.color_cursor = (state.color_cursor + 1) % 4;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.a || edge.start) {
      const Card card = current.hand[static_cast<std::size_t>(state.selected)];
      apply_card(state, card, state.current_player, state.selected, state.color_cursor);
      state.choosing_color = false;
      trigger_tone(tone, 620.0f, 40);
    }
    if (edge.b) {
      state.choosing_color = false;
    }
    return false;
  }

  if (edge.left) {
    state.selected = (state.selected + hand_size - 1) % hand_size;
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.right) {
    state.selected = (state.selected + 1) % hand_size;
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.b) {
    current.hand.push_back(draw_one(state));
    state.status = current.name + " DRAWS";
    advance_turn(state, 1);
    trigger_tone(tone, 300.0f, 30);
    return false;
  }
  if (edge.a) {
    const Card card = current.hand[static_cast<std::size_t>(state.selected)];
    if (!can_play(card, top, live_color)) {
      state.detail = "MATCH COLOR OR SYMBOL";
      trigger_tone(tone, 220.0f, 30);
      return false;
    }
    if (card.kind == Kind::Wild || card.kind == Kind::WildDrawFour) {
      state.color_cursor = choose_color_for_hand(current.hand);
      state.choosing_color = true;
      trigger_tone(tone, 500.0f, 30);
      return false;
    }
    apply_card(state, card, state.current_player, state.selected, state.color_cursor);
    trigger_tone(tone, 620.0f, 40);
  }
  return false;
}

}  // namespace wildsweep

// ---------------------------------------------------------------------------
// House Current
// ---------------------------------------------------------------------------

namespace blackjack {

constexpr int kPlayers = 5;
constexpr int kStartingStack = 600;
constexpr int kTableBet = 20;

enum class Action { Hit = 0, Stand, Double };

struct Card {
  int rank = 1;
  int suit = 0;
};

struct HandValue {
  int total = 0;
  bool soft = false;
  bool blackjack = false;
};

struct Player {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  bool human = false;
  bool busted = false;
  bool stood = false;
  int chips = kStartingStack;
  int bet = 0;
  std::vector<Card> hand;
};

struct State {
  std::mt19937 rng{0x484F5553u};
  std::array<Player, kPlayers> players{};
  std::vector<Card> deck;
  std::vector<Card> dealer;
  int human_players = 1;
  int current_player = 0;
  int action_index = 0;
  bool result = false;
  bool dealer_reveal = false;
  Uint32 bot_ready_at = 0;
  HotseatGuard guard{};
  std::string status = "HOUSE RULES 3:2";
  std::string detail = "A HIT / B STAND / START DOUBLE";
};

static const std::array<const char*, 3> kActionNames{{"HIT", "STAND", "DOUBLE"}};

std::vector<Card> build_deck() {
  std::vector<Card> deck;
  deck.reserve(52);
  for (int suit = 0; suit < 4; ++suit) {
    for (int rank = 1; rank <= 13; ++rank) {
      deck.push_back(Card{rank, suit});
    }
  }
  return deck;
}

Card draw_card(State& state) {
  if (state.deck.empty()) {
    state.deck = build_deck();
    shuffle_vector(state.deck, state.rng);
  }
  const Card card = state.deck.back();
  state.deck.pop_back();
  return card;
}

int card_value(const Card& card) {
  if (card.rank == 1) return 11;
  return std::min(card.rank, 10);
}

HandValue hand_value(const std::vector<Card>& hand) {
  int total = 0;
  int aces = 0;
  for (const Card& card : hand) {
    total += card_value(card);
    if (card.rank == 1) ++aces;
  }
  while (total > 21 && aces > 0) {
    total -= 10;
    --aces;
  }
  return HandValue{total, aces > 0, hand.size() == 2 && total == 21};
}

bool can_act(const Player& player) {
  return !player.busted && !player.stood && player.bet > 0;
}

int next_player_to_act(const State& state, int after) {
  for (int seat = after + 1; seat < kPlayers; ++seat) {
    if (can_act(state.players[static_cast<std::size_t>(seat)])) {
      return seat;
    }
  }
  return -1;
}

void queue_guard_if_needed(State& state) {
  if (state.current_player < 0) {
    return;
  }
  const Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  if (player.human && state.human_players > 1) {
    open_guard(state.guard, player.name + " TURN", "PASS CONTROLLER AND PRESS A", player.color);
  }
}

void schedule_bot(State& state) {
  if (state.current_player >= 0 &&
      !state.players[static_cast<std::size_t>(state.current_player)].human) {
    state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
  }
}

void assign_players(State& state, int humans) {
  state.human_players = clamp_int(humans, 1, 4);
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    player.human = seat < state.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                               : std::string(kBotNames[static_cast<std::size_t>(seat - state.human_players)]);
    player.color = kSeatColors[static_cast<std::size_t>(seat)];
    if (player.chips < kTableBet) {
      player.chips = kStartingStack / 2;
    }
  }
}

void start_round(State& state) {
  state.result = false;
  state.dealer_reveal = false;
  state.current_player = 0;
  state.action_index = 0;
  state.guard.active = false;
  state.status = "HOUSE RULES 3:2";
  state.detail = "A HIT / B STAND / START DOUBLE";

  state.deck = build_deck();
  shuffle_vector(state.deck, state.rng);
  state.dealer.clear();

  for (auto& player : state.players) {
    if (player.chips < kTableBet) {
      player.chips = kStartingStack / 2;
    }
    player.hand.clear();
    player.busted = false;
    player.stood = false;
    player.bet = std::min(kTableBet, player.chips);
    player.chips -= player.bet;
  }

  for (int pass = 0; pass < 2; ++pass) {
    for (auto& player : state.players) {
      player.hand.push_back(draw_card(state));
    }
    state.dealer.push_back(draw_card(state));
  }

  for (auto& player : state.players) {
    if (hand_value(player.hand).blackjack) {
      player.stood = true;
    }
  }

  state.current_player = next_player_to_act(state, -1);
  if (state.current_player < 0) {
    state.dealer_reveal = true;
  } else {
    queue_guard_if_needed(state);
    schedule_bot(state);
  }
}

void start_match(State& state, int humans) {
  assign_players(state, humans);
  start_round(state);
}

std::vector<Action> legal_actions(const State& state, int seat) {
  const Player& player = state.players[static_cast<std::size_t>(seat)];
  std::vector<Action> actions;
  if (!can_act(player)) return actions;
  actions.push_back(Action::Hit);
  actions.push_back(Action::Stand);
  if (player.hand.size() == 2 && player.chips >= player.bet) {
    actions.push_back(Action::Double);
  }
  return actions;
}

void settle_round(State& state) {
  state.result = true;
  state.dealer_reveal = true;
  const HandValue dealer_value = hand_value(state.dealer);
  int winners = 0;
  for (auto& player : state.players) {
    const HandValue value = hand_value(player.hand);
    if (player.bet <= 0) continue;
    if (value.blackjack && !dealer_value.blackjack) {
      player.chips += player.bet * 5 / 2;
      ++winners;
    } else if (player.busted) {
    } else if (dealer_value.total > 21 || value.total > dealer_value.total) {
      player.chips += player.bet * 2;
      ++winners;
    } else if (value.total == dealer_value.total) {
      player.chips += player.bet;
    }
  }
  state.status = winners > 0 ? "TABLE WINS " + std::to_string(winners) : "HOUSE CLEANS UP";
  state.detail = dealer_value.total > 21 ? "DEALER BUSTS" : "DEALER SHOWS " + std::to_string(dealer_value.total);
}

void run_dealer(State& state) {
  state.dealer_reveal = true;
  while (true) {
    const HandValue value = hand_value(state.dealer);
    if (value.total > 21 || value.total > 17 || (value.total == 17 && !value.soft)) {
      break;
    }
    if (value.total == 17 && value.soft) {
      break;
    }
    state.dealer.push_back(draw_card(state));
  }
  settle_round(state);
}

void advance_turn(State& state) {
  state.current_player = next_player_to_act(state, state.current_player);
  if (state.current_player < 0) {
    run_dealer(state);
  } else {
    queue_guard_if_needed(state);
    schedule_bot(state);
  }
}

void apply_action(State& state, int seat, Action action) {
  Player& player = state.players[static_cast<std::size_t>(seat)];
  if (action == Action::Hit) {
    player.hand.push_back(draw_card(state));
    const HandValue value = hand_value(player.hand);
    if (value.total > 21) {
      player.busted = true;
      state.status = player.name + " BUSTS";
      advance_turn(state);
    } else if (value.total == 21) {
      player.stood = true;
      state.status = player.name + " LOCKS 21";
      advance_turn(state);
    } else {
      state.status = player.name + " HITS";
    }
  } else if (action == Action::Stand) {
    player.stood = true;
    state.status = player.name + " STANDS";
    advance_turn(state);
  } else {
    if (player.chips >= player.bet) {
      player.chips -= player.bet;
      player.bet *= 2;
      player.hand.push_back(draw_card(state));
      const HandValue value = hand_value(player.hand);
      player.stood = true;
      player.busted = value.total > 21;
      state.status = player.name + (player.busted ? " DOUBLES AND BUSTS" : " DOUBLES");
      advance_turn(state);
    }
  }
}

Action bot_choice(const State& state, int seat) {
  const Player& player = state.players[static_cast<std::size_t>(seat)];
  const HandValue self = hand_value(player.hand);
  const int dealer_up = std::min(state.dealer.front().rank, 10);
  if (player.hand.size() == 2 && player.chips >= player.bet && self.total >= 9 && self.total <= 11 &&
      dealer_up <= 6) {
    return Action::Double;
  }
  if (self.total <= 11) return Action::Hit;
  if (self.total >= 17) return Action::Stand;
  if (dealer_up >= 7) return Action::Hit;
  return Action::Stand;
}

struct SeatLayout {
  SDL_Point label;
  SDL_Point cards;
};

SeatLayout seat_layout(int seat) {
  static const std::array<SeatLayout, kPlayers> layouts{{
      {{256, 336}, {224, 348}},
      {{88, 282}, {42, 296}},
      {{108, 152}, {62, 166}},
      {{404, 152}, {360, 166}},
      {{424, 282}, {380, 296}},
  }};
  return layouts[static_cast<std::size_t>(seat)];
}

void draw_player_hand(SDL_Renderer* renderer, int x, int y, const std::vector<Card>& hand,
                      bool reveal, int selected = -1) {
  for (int i = 0; i < static_cast<int>(hand.size()); ++i) {
    const int draw_x = x + i * 18;
    if (reveal) {
      draw_standard_card_face(renderer, draw_x, y, rank_text_standard(hand[static_cast<std::size_t>(i)].rank),
                              suit_text(hand[static_cast<std::size_t>(i)].suit),
                              suit_color(hand[static_cast<std::size_t>(i)].suit));
    } else {
      draw_standard_card_back(renderer, draw_x, y, "VD");
    }
    if (i == selected) {
      draw_rect(renderer, {draw_x - 2, y - 2, 46, 62}, kAccent);
    }
  }
}

void draw_state(SDL_Renderer* renderer, const State& state) {
  draw_pack_frame(renderer, "HOUSE CURRENT", "BLACKJACK");
  fill_rect(renderer, {30, 114, 452, 360}, {8, 19, 18, 255});
  fill_circle(renderer, 256, 284, 170, {8, 19, 18, 255});
  fill_circle(renderer, 256, 280, 160, kTable);
  draw_circle_outline(renderer, 256, 280, 160, kFrame);
  draw_circle_outline(renderer, 256, 280, 148, {48, 126, 88, 255});

  draw_text(renderer, "DEALER", 256, 122, 2, kGold, true);
  const HandValue dealer_value = hand_value(state.dealer);
  draw_player_hand(renderer, 202, 144, state.dealer, state.dealer_reveal, -1);
  draw_text(renderer, state.dealer_reveal ? std::to_string(dealer_value.total) : "HIDDEN", 256, 212, 1, kText, true);

  for (int seat = 0; seat < kPlayers; ++seat) {
    const SeatLayout layout = seat_layout(seat);
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    const HandValue value = hand_value(player.hand);
    SDL_Color label = player.color;
    if (seat == state.current_player && !state.result) label = kAccent;
    if (player.busted) label = kMuted;
    draw_text(renderer, player.name, layout.label.x, layout.label.y, 1, label, true);
    draw_text(renderer, std::to_string(player.chips) + " C", layout.label.x, layout.label.y + 12, 1, kText, true);
    draw_text(renderer, "BET " + std::to_string(player.bet), layout.label.x, layout.label.y + 24, 1, kGold, true);
    if (player.busted) {
      draw_text(renderer, "BUST", layout.label.x, layout.label.y + 36, 1, kDanger, true);
    } else if (player.stood) {
      draw_text(renderer, "STAND " + std::to_string(value.total), layout.label.x, layout.label.y + 36, 1, kAccent2, true);
    } else {
      draw_text(renderer, std::to_string(value.total), layout.label.x, layout.label.y + 36, 1, kText, true);
    }
    const bool reveal = state.result || (!state.guard.active && player.human && seat == state.current_player) || !player.human;
    draw_player_hand(renderer, layout.cards.x, layout.cards.y, player.hand, reveal, -1);
  }

  draw_panel(renderer, {22, 116, 148, 98}, "TABLE");
  draw_text(renderer, fit_text(state.status, 128, 1), 96, 140, 1, kText, true);
  draw_text(renderer, fit_text(state.detail, 128, 1), 96, 160, 1, kMuted, true);
  draw_text(renderer, "DEALER " + (state.dealer_reveal ? std::to_string(dealer_value.total) : "?"), 96, 182, 1, kGold, true);

  if (!state.result && state.current_player >= 0 && state.players[static_cast<std::size_t>(state.current_player)].human) {
    const auto actions = legal_actions(state, state.current_player);
    draw_panel(renderer, {22, 360, 156, 98}, "ACTIONS");
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
      const SDL_Rect row{30, 382 + i * 18, 140, 14};
      if (i == state.action_index) {
        fill_rect(renderer, row, kPanelHi);
      }
      draw_text(renderer, kActionNames[static_cast<std::size_t>(actions[static_cast<std::size_t>(i)])],
                row.x + 4, row.y + 2, 1, i == state.action_index ? kAccent : kText);
    }
    draw_text(renderer, "B QUICK STAND", 100, 442, 1, kMuted, true);
  }

  if (state.result) {
    fill_rect(renderer, {92, 188, 328, 94}, {8, 12, 18, 236});
    draw_rect(renderer, {92, 188, 328, 94}, kFrame);
    draw_text(renderer, "HAND COMPLETE", 256, 198, 3, kGold, true);
    draw_text(renderer, fit_text(state.status, 290, 1), 256, 236, 1, kText, true);
    draw_text(renderer, fit_text(state.detail, 290, 1), 256, 254, 1, kMuted, true);
  }

  draw_guard(renderer, state.guard);
}

bool update(State& state, ToneState& tone, const EdgeInput& edge) {
  if (edge.select) {
    return true;
  }
  if (update_guard(state.guard, edge, tone)) {
    return false;
  }
  if (state.result) {
    if (edge.a || edge.start) {
      start_round(state);
      trigger_tone(tone, 700.0f, 60);
    }
    return false;
  }

  if (state.current_player >= 0 && !state.players[static_cast<std::size_t>(state.current_player)].human) {
    if (SDL_GetTicks() >= state.bot_ready_at) {
      apply_action(state, state.current_player, bot_choice(state, state.current_player));
    }
    return false;
  }

  if (state.current_player < 0) {
    return false;
  }

  const auto actions = legal_actions(state, state.current_player);
  if (actions.empty()) {
    return false;
  }
  if (edge.up || edge.left) {
    state.action_index = (state.action_index + static_cast<int>(actions.size()) - 1) % static_cast<int>(actions.size());
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.down || edge.right) {
    state.action_index = (state.action_index + 1) % static_cast<int>(actions.size());
    trigger_tone(tone, 420.0f, 20);
  }
  if (edge.b) {
    apply_action(state, state.current_player, Action::Stand);
    trigger_tone(tone, 280.0f, 30);
    return false;
  }
  if (edge.start && std::find(actions.begin(), actions.end(), Action::Double) != actions.end()) {
    apply_action(state, state.current_player, Action::Double);
    trigger_tone(tone, 620.0f, 36);
    return false;
  }
  if (edge.a) {
    const int index = clamp_int(state.action_index, 0, static_cast<int>(actions.size()) - 1);
    apply_action(state, state.current_player, actions[static_cast<std::size_t>(index)]);
    trigger_tone(tone, 620.0f, 36);
  }
  return false;
}

}  // namespace blackjack

// ---------------------------------------------------------------------------
// Sky Crown
// ---------------------------------------------------------------------------

namespace skycrown {

constexpr int kPlayers = 4;
constexpr int kTargetScore = 300;

enum class Phase { Bidding, KittyColor, KittyDiscard, Play, RoundResult, GameResult };

struct Card {
  int color = 0;
  int rank = 1;
};

struct Player {
  std::string name;
  SDL_Color ui_color{255, 255, 255, 255};
  bool human = false;
  bool passed = false;
  int bid = 0;
  int preferred_trump = 0;
  std::vector<Card> hand;
};

struct State {
  std::mt19937 rng{0x534B5943u};
  std::array<Player, kPlayers> players{};
  std::vector<Card> kitty;
  std::array<std::optional<Card>, kPlayers> trick{};
  Phase phase = Phase::Bidding;
  int human_players = 1;
  int dealer = kPlayers - 1;
  int current_player = 0;
  int leader = 0;
  int bidder = -1;
  int high_bid = 0;
  int bid_cursor = 75;
  int trump = 0;
  int discards_needed = 5;
  int trick_count = 0;
  int trick_points[2]{0, 0};
  int team_score[2]{0, 0};
  int kitty_points = 0;
  int selected = 0;
  Uint32 bot_ready_at = 0;
  HotseatGuard guard{};
  std::string status = "BID FOR TRUMP";
  std::string detail = "A CONFIRM / B PASS";
};

SDL_Color suit_color(int color) {
  switch (color) {
    case 0: return {236, 94, 108, 255};
    case 1: return {91, 216, 160, 255};
    case 2: return {245, 208, 88, 255};
    case 3: return {127, 158, 255, 255};
    default: return {240, 240, 240, 255};
  }
}

std::string color_name(int color) {
  switch (color) {
    case 0: return "CRIMSON";
    case 1: return "MOSS";
    case 2: return "SUN";
    case 3: return "TIDE";
    default: return "CROWN";
  }
}

std::string card_label(const Card& card) {
  if (card.color == 4) return "CR";
  return std::to_string(card.rank);
}

int card_points(const Card& card) {
  if (card.color == 4) return 20;
  if (card.rank == 5) return 5;
  if (card.rank == 10 || card.rank == 14) return 10;
  return 0;
}

std::vector<Card> build_deck() {
  std::vector<Card> deck;
  for (int color = 0; color < 4; ++color) {
    for (int rank = 1; rank <= 14; ++rank) {
      deck.push_back(Card{color, rank});
    }
  }
  deck.push_back(Card{4, 15});
  return deck;
}

bool card_less(const Card& a, const Card& b) {
  if (a.color != b.color) return a.color < b.color;
  return a.rank < b.rank;
}

int team_for_seat(int seat) {
  return seat % 2;
}

void queue_guard_if_needed(State& state) {
  const Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  if (player.human && state.human_players > 1 &&
      (state.phase == Phase::Bidding || state.phase == Phase::KittyColor ||
       state.phase == Phase::KittyDiscard || state.phase == Phase::Play)) {
    open_guard(state.guard, player.name + " TURN", "PASS CONTROLLER AND PRESS A", player.ui_color);
  }
}

void schedule_bot(State& state) {
  if (!state.players[static_cast<std::size_t>(state.current_player)].human) {
    state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
  }
}

void assign_players(State& state, int humans) {
  state.human_players = clamp_int(humans, 1, 4);
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    player.human = seat < state.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                               : std::string(kBotNames[static_cast<std::size_t>(seat - state.human_players)]);
    player.ui_color = kSeatColors[static_cast<std::size_t>(seat)];
  }
}

int estimate_trump_strength(const std::vector<Card>& hand, int color) {
  int score = 0;
  for (const Card& card : hand) {
    if (card.color == color) score += 2 + card.rank / 3;
    if (card.color == 4) score += 8;
  }
  return score;
}

int best_trump(const std::vector<Card>& hand) {
  int best_color = 0;
  int best_score = -999;
  for (int color = 0; color < 4; ++color) {
    const int score = estimate_trump_strength(hand, color);
    if (score > best_score) {
      best_score = score;
      best_color = color;
    }
  }
  return best_color;
}

int estimate_bid(const std::vector<Card>& hand) {
  int score = estimate_trump_strength(hand, best_trump(hand));
  int bid = 70 + score * 2;
  bid = ((bid + 4) / 5) * 5;
  return clamp_int(bid, 70, 160);
}

void start_round(State& state) {
  state.phase = Phase::Bidding;
  state.dealer = (state.dealer + 1) % kPlayers;
  state.current_player = (state.dealer + 1) % kPlayers;
  state.leader = 0;
  state.bidder = -1;
  state.high_bid = 0;
  state.bid_cursor = 75;
  state.trump = 0;
  state.discards_needed = 5;
  state.trick_count = 0;
  state.trick_points[0] = 0;
  state.trick_points[1] = 0;
  state.kitty_points = 0;
  state.trick = {};
  state.guard.active = false;
  state.status = "BID FOR TRUMP";
  state.detail = "A CONFIRM / B PASS";

  auto deck = build_deck();
  shuffle_vector(deck, state.rng);
  for (auto& player : state.players) {
    player.hand.clear();
    player.passed = false;
    player.bid = 0;
    player.preferred_trump = 0;
  }
  for (int i = 0; i < 52; ++i) {
    state.players[static_cast<std::size_t>(i % kPlayers)].hand.push_back(deck[static_cast<std::size_t>(i)]);
  }
  state.kitty.assign(deck.begin() + 52, deck.end());
  for (auto& player : state.players) {
    std::sort(player.hand.begin(), player.hand.end(), card_less);
    player.preferred_trump = best_trump(player.hand);
  }
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void start_match(State& state, int humans) {
  assign_players(state, humans);
  start_round(state);
}

bool bidding_done(const State& state) {
  int active = 0;
  for (const auto& player : state.players) {
    if (!player.passed) ++active;
  }
  return active <= 1;
}

void move_to_kitty(State& state) {
  if (state.bidder < 0) {
    state.bidder = state.dealer;
    state.high_bid = 70;
  }
  Player& bidder = state.players[static_cast<std::size_t>(state.bidder)];
  bidder.hand.insert(bidder.hand.end(), state.kitty.begin(), state.kitty.end());
  std::sort(bidder.hand.begin(), bidder.hand.end(), card_less);
  state.current_player = state.bidder;
  state.phase = Phase::KittyColor;
  state.trump = bidder.preferred_trump;
  state.status = bidder.name + " CLAIMS THE KITTY";
  state.detail = "CHOOSE TRUMP";
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void advance_bidder_turn(State& state) {
  state.current_player = (state.current_player + 1) % kPlayers;
  while (state.players[static_cast<std::size_t>(state.current_player)].passed) {
    state.current_player = (state.current_player + 1) % kPlayers;
  }
  queue_guard_if_needed(state);
  schedule_bot(state);
}

void finalize_discard(State& state) {
  state.phase = Phase::Play;
  state.leader = state.bidder;
  state.current_player = state.leader;
  state.selected = 0;
  state.status = "TRICKS FOR " + color_name(state.trump);
  state.detail = "FOLLOW LEAD IF YOU CAN";
  queue_guard_if_needed(state);
  schedule_bot(state);
}

bool can_follow(const std::vector<Card>& hand, int lead_color) {
  for (const Card& card : hand) {
    if (card.color == lead_color) return true;
  }
  return false;
}

bool legal_play(const State& state, int seat, int index) {
  const auto& hand = state.players[static_cast<std::size_t>(seat)].hand;
  if (index < 0 || index >= static_cast<int>(hand.size())) return false;
  if (!state.trick[static_cast<std::size_t>(state.leader)].has_value()) {
    return true;
  }
  int lead_color = state.trick[static_cast<std::size_t>(state.leader)]->color;
  if (lead_color == 4) lead_color = state.trump;
  const Card card = hand[static_cast<std::size_t>(index)];
  if (card.color == 4) {
    return !can_follow(hand, lead_color);
  }
  if (can_follow(hand, lead_color)) {
    return card.color == lead_color;
  }
  return true;
}

int trick_winner(const State& state) {
  int winner = state.leader;
  auto score = [&](const Card& card, int lead_color) {
    int value = card.rank;
    const int color = card.color == 4 ? state.trump : card.color;
    if (card.color == 4) return 1000;
    if (color == state.trump) return 500 + value;
    if (color == lead_color) return 200 + value;
    return value;
  };
  int lead_color = state.trick[static_cast<std::size_t>(state.leader)]->color;
  if (lead_color == 4) lead_color = state.trump;
  int best = -1;
  for (int seat = 0; seat < kPlayers; ++seat) {
    const Card& card = *state.trick[static_cast<std::size_t>(seat)];
    const int value = score(card, lead_color);
    if (value > best) {
      best = value;
      winner = seat;
    }
  }
  return winner;
}

void score_trick(State& state) {
  int points = 0;
  for (int seat = 0; seat < kPlayers; ++seat) {
    points += card_points(*state.trick[static_cast<std::size_t>(seat)]);
  }
  if (state.trick_count == 12) {
    points += 10;
  }
  const int winner = trick_winner(state);
  state.trick_points[team_for_seat(winner)] += points;
  state.leader = winner;
  state.current_player = winner;
  state.trick = {};
  state.trick_count += 1;

  if (state.trick_count >= 13) {
    const int bidder_team = team_for_seat(state.bidder);
    state.trick_points[bidder_team] += state.kitty_points;
    if (state.trick_points[bidder_team] >= state.high_bid) {
      state.team_score[bidder_team] += state.trick_points[bidder_team];
    } else {
      state.team_score[bidder_team] -= state.high_bid;
    }
    const int other_team = 1 - bidder_team;
    state.team_score[other_team] += state.trick_points[other_team];
    state.phase = (state.team_score[0] >= kTargetScore || state.team_score[1] >= kTargetScore)
                      ? Phase::GameResult
                      : Phase::RoundResult;
    state.status = state.team_score[0] > state.team_score[1] ? "TEAM A LEADS" : "TEAM B LEADS";
    state.detail = "A NEXT ROUND";
    return;
  }

  queue_guard_if_needed(state);
  schedule_bot(state);
}

void bot_discard(State& state) {
  Player& bidder = state.players[static_cast<std::size_t>(state.bidder)];
  while (state.discards_needed > 0) {
    int choice = 0;
    int best_score = 999;
    for (int i = 0; i < static_cast<int>(bidder.hand.size()); ++i) {
      const Card& card = bidder.hand[static_cast<std::size_t>(i)];
      int score = card_points(card) * 5 + card.rank;
      if (card.color == state.trump || card.color == 4) score += 30;
      if (score < best_score) {
        best_score = score;
        choice = i;
      }
    }
    state.kitty_points += card_points(bidder.hand[static_cast<std::size_t>(choice)]);
    bidder.hand.erase(bidder.hand.begin() + choice);
    state.discards_needed--;
  }
  finalize_discard(state);
}

int bot_play_choice(const State& state) {
  const auto& hand = state.players[static_cast<std::size_t>(state.current_player)].hand;
  int best = -1;
  int best_score = 999;
  for (int i = 0; i < static_cast<int>(hand.size()); ++i) {
    if (!legal_play(state, state.current_player, i)) continue;
    const Card& card = hand[static_cast<std::size_t>(i)];
    int score = card_points(card) * 6 + card.rank;
    if (card.color == state.trump || card.color == 4) score += 20;
    if (score < best_score) {
      best_score = score;
      best = i;
    }
  }
  return best < 0 ? 0 : best;
}

void bot_turn(State& state) {
  Player& player = state.players[static_cast<std::size_t>(state.current_player)];
  if (state.phase == Phase::Bidding) {
    const int minimum = state.high_bid == 0 ? 70 : state.high_bid + 5;
    const int estimate = estimate_bid(player.hand);
    if (estimate >= minimum) {
      state.high_bid = clamp_int(((estimate + 4) / 5) * 5, minimum, 160);
      state.bidder = state.current_player;
      player.bid = state.high_bid;
      state.status = player.name + " BIDS " + std::to_string(state.high_bid);
    } else {
      player.passed = true;
      state.status = player.name + " PASSES";
    }
    if (bidding_done(state)) {
      move_to_kitty(state);
    } else {
      advance_bidder_turn(state);
    }
    return;
  }
  if (state.phase == Phase::KittyColor) {
    state.trump = best_trump(player.hand);
    state.phase = Phase::KittyDiscard;
    state.status = color_name(state.trump) + " IS TRUMP";
    bot_discard(state);
    return;
  }
  if (state.phase == Phase::Play) {
    const int choice = bot_play_choice(state);
    const Card card = player.hand[static_cast<std::size_t>(choice)];
    player.hand.erase(player.hand.begin() + choice);
    state.trick[static_cast<std::size_t>(state.current_player)] = card;
    state.status = player.name + " PLAYS " + card_label(card);
    if (state.current_player == (state.leader + 3) % kPlayers) {
      score_trick(state);
    } else {
      state.current_player = (state.current_player + 1) % kPlayers;
      queue_guard_if_needed(state);
      schedule_bot(state);
    }
  }
}

void draw_card(SDL_Renderer* renderer, int x, int y, const Card& card, bool highlight = false) {
  const SDL_Rect rect{x, y, 42, 58};
  fill_rect(renderer, {x + 3, y + 3, 42, 58}, {0, 0, 0, 96});
  fill_rect(renderer, rect, card.color == 4 ? SDL_Color{33, 37, 48, 255} : SDL_Color{243, 241, 232, 255});
  draw_rect(renderer, rect, highlight ? kAccent : kFrame);
  if (card.color == 4) {
    draw_text(renderer, "CR", x + 21, y + 18, 3, kGold, true);
  } else {
    const SDL_Color color = suit_color(card.color);
    draw_text(renderer, card_label(card), x + 5, y + 4, 1, color);
    draw_text(renderer, color_name(card.color).substr(0, 1), x + 5, y + 14, 1, color);
    draw_text(renderer, card_label(card), x + 21, y + 20, 3, color, true);
  }
}

void draw_state(SDL_Renderer* renderer, const State& state) {
  draw_pack_frame(renderer, "SKY CROWN", "ROOK");
  draw_panel(renderer, {26, 116, 460, 182}, "TABLE");

  draw_text(renderer, "TRUMP", 38, 136, 1, kMuted);
  draw_text(renderer, color_name(state.trump), 118, 132, 2, suit_color(state.trump));
  draw_text(renderer, "BID", 38, 162, 1, kMuted);
  draw_text(renderer, state.high_bid == 0 ? "OPEN" : std::to_string(state.high_bid), 118, 158, 2, kGold);
  draw_text(renderer, "KITTY", 38, 188, 1, kMuted);
  draw_text(renderer, std::to_string(state.kitty.size()) + " CARD", 118, 184, 2, kText);
  draw_text(renderer, fit_text(state.status, 230, 1), 324, 138, 1, kText, true);
  draw_text(renderer, fit_text(state.detail, 230, 1), 324, 160, 1, kMuted, true);

  draw_panel(renderer, {26, 308, 460, 168}, "HANDS");
  for (int seat = 0; seat < kPlayers; ++seat) {
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    const int y = 326 + seat * 18;
    if (seat == state.current_player &&
        (state.phase == Phase::Bidding || state.phase == Phase::KittyColor ||
         state.phase == Phase::KittyDiscard || state.phase == Phase::Play)) {
      fill_rect(renderer, {34, y - 2, 140, 14}, kPanelHi);
    }
    draw_text(renderer, player.name, 38, y, 1, player.ui_color);
    draw_text(renderer, std::to_string(player.hand.size()) + " CARD", 140, y, 1, kText);
    if (player.bid > 0) {
      draw_text(renderer, "BID " + std::to_string(player.bid), 238, y, 1, kAccent);
    } else if (player.passed && state.phase == Phase::Bidding) {
      draw_text(renderer, "PASS", 238, y, 1, kMuted);
    }
  }
  draw_text(renderer, "TEAM A " + std::to_string(state.team_score[0]), 362, 326, 1, kSeatColors[0]);
  draw_text(renderer, "TEAM B " + std::to_string(state.team_score[1]), 362, 344, 1, kSeatColors[1]);
  draw_text(renderer, "ROUND A " + std::to_string(state.trick_points[0]), 362, 370, 1, kMuted);
  draw_text(renderer, "ROUND B " + std::to_string(state.trick_points[1]), 362, 388, 1, kMuted);

  const auto& hand = state.players[static_cast<std::size_t>(state.current_player)].hand;
  const int count = static_cast<int>(hand.size());
  const int spacing = count > 1 ? clamp_int(372 / (count - 1), 16, 28) : 28;
  const int start_x = 256 - ((count - 1) * spacing) / 2 - 21;
  for (int i = 0; i < count; ++i) {
    int y = 390;
    if (i == state.selected &&
        (state.phase == Phase::KittyDiscard || state.phase == Phase::Play)) y -= 10;
    draw_card(renderer, start_x + i * spacing, y, hand[static_cast<std::size_t>(i)], i == state.selected);
    if (state.phase == Phase::Play && !legal_play(state, state.current_player, i)) {
      fill_rect(renderer, {start_x + i * spacing + 2, y + 2, 38, 54}, {0, 0, 0, 100});
      draw_rect(renderer, {start_x + i * spacing, y, 42, 58}, kMuted);
    }
  }

  for (int seat = 0; seat < kPlayers; ++seat) {
    if (!state.trick[static_cast<std::size_t>(seat)].has_value()) continue;
    const SDL_Point pos = seat == 0 ? SDL_Point{236, 212}
                                    : seat == 1 ? SDL_Point{164, 196}
                                                : seat == 2 ? SDL_Point{236, 144}
                                                            : SDL_Point{308, 196};
    draw_card(renderer, pos.x, pos.y, *state.trick[static_cast<std::size_t>(seat)], seat == state.leader);
  }

  if (state.phase == Phase::Bidding) {
    draw_panel(renderer, {340, 178, 132, 96}, "BID");
    draw_text(renderer, std::to_string(state.bid_cursor), 406, 204, 4, kGold, true);
    draw_text(renderer, "A BID", 406, 238, 1, kAccent, true);
    draw_text(renderer, "B PASS", 406, 254, 1, kMuted, true);
  } else if (state.phase == Phase::KittyColor) {
    fill_rect(renderer, {114, 162, 284, 92}, {8, 12, 18, 238});
    draw_rect(renderer, {114, 162, 284, 92}, kFrame);
    draw_text(renderer, "CHOOSE TRUMP", 256, 174, 3, kGold, true);
    for (int i = 0; i < 4; ++i) {
      const SDL_Rect swatch{138 + i * 56, 214, 36, 18};
      fill_rect(renderer, swatch, suit_color(i));
      draw_rect(renderer, swatch, i == state.trump ? kAccent : kFrame);
    }
  }

  if (state.phase == Phase::RoundResult || state.phase == Phase::GameResult) {
    fill_rect(renderer, {92, 182, 328, 98}, {8, 12, 18, 236});
    draw_rect(renderer, {92, 182, 328, 98}, kFrame);
    draw_text(renderer, state.phase == Phase::GameResult ? "MATCH COMPLETE" : "ROUND COMPLETE",
              256, 194, 3, kGold, true);
    draw_text(renderer, fit_text(state.status, 290, 1), 256, 234, 1, kText, true);
    draw_text(renderer, "A CONTINUE", 256, 254, 1, kAccent, true);
  }

  draw_guard(renderer, state.guard);
}

bool update(State& state, ToneState& tone, const EdgeInput& edge) {
  if (edge.select) {
    return true;
  }
  if (update_guard(state.guard, edge, tone)) {
    return false;
  }

  if (state.phase == Phase::RoundResult || state.phase == Phase::GameResult) {
    if (edge.a || edge.start) {
      if (state.phase == Phase::GameResult) {
        state.team_score[0] = 0;
        state.team_score[1] = 0;
      }
      start_round(state);
      trigger_tone(tone, 700.0f, 60);
    }
    return false;
  }

  if (!state.players[static_cast<std::size_t>(state.current_player)].human) {
    if (SDL_GetTicks() >= state.bot_ready_at) {
      bot_turn(state);
    }
    return false;
  }

  Player& current = state.players[static_cast<std::size_t>(state.current_player)];
  if (state.phase == Phase::Bidding) {
    const int minimum = state.high_bid == 0 ? 70 : state.high_bid + 5;
    state.bid_cursor = clamp_int(std::max(state.bid_cursor, minimum), minimum, 160);
    if (edge.left || edge.down) {
      state.bid_cursor = std::max(minimum, state.bid_cursor - 5);
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right || edge.up) {
      state.bid_cursor = std::min(160, state.bid_cursor + 5);
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.b) {
      current.passed = true;
      state.status = current.name + " PASSES";
      if (bidding_done(state)) {
        move_to_kitty(state);
      } else {
        advance_bidder_turn(state);
      }
      trigger_tone(tone, 300.0f, 30);
    }
    if (edge.a) {
      state.high_bid = state.bid_cursor;
      state.bidder = state.current_player;
      current.bid = state.high_bid;
      state.status = current.name + " BIDS " + std::to_string(state.high_bid);
      if (bidding_done(state)) {
        move_to_kitty(state);
      } else {
        advance_bidder_turn(state);
      }
      trigger_tone(tone, 620.0f, 40);
    }
    return false;
  }

  if (state.phase == Phase::KittyColor) {
    if (edge.left || edge.up) {
      state.trump = (state.trump + 3) % 4;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right || edge.down) {
      state.trump = (state.trump + 1) % 4;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.a || edge.start) {
      state.phase = Phase::KittyDiscard;
      state.detail = "DISCARD 5 CARDS";
      trigger_tone(tone, 620.0f, 40);
    }
    return false;
  }

  if (state.phase == Phase::KittyDiscard) {
    const int hand_size = static_cast<int>(current.hand.size());
    if (hand_size == 0) return false;
    state.selected = clamp_int(state.selected, 0, hand_size - 1);
    if (edge.left) {
      state.selected = (state.selected + hand_size - 1) % hand_size;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right) {
      state.selected = (state.selected + 1) % hand_size;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.a) {
      state.kitty_points += card_points(current.hand[static_cast<std::size_t>(state.selected)]);
      current.hand.erase(current.hand.begin() + state.selected);
      state.discards_needed -= 1;
      if (state.discards_needed <= 0) {
        finalize_discard(state);
      }
      trigger_tone(tone, 620.0f, 32);
    }
    return false;
  }

  if (state.phase == Phase::Play) {
    const int hand_size = static_cast<int>(current.hand.size());
    if (hand_size == 0) return false;
    state.selected = clamp_int(state.selected, 0, hand_size - 1);
    if (edge.left) {
      state.selected = (state.selected + hand_size - 1) % hand_size;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right) {
      state.selected = (state.selected + 1) % hand_size;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.a) {
      if (!legal_play(state, state.current_player, state.selected)) {
        state.detail = "FOLLOW THE LEAD COLOR";
        trigger_tone(tone, 220.0f, 30);
        return false;
      }
      const Card card = current.hand[static_cast<std::size_t>(state.selected)];
      current.hand.erase(current.hand.begin() + state.selected);
      state.trick[static_cast<std::size_t>(state.current_player)] = card;
      state.status = current.name + " PLAYS " + card_label(card);
      if (state.current_player == (state.leader + 3) % kPlayers) {
        score_trick(state);
      } else {
        state.current_player = (state.current_player + 1) % kPlayers;
        queue_guard_if_needed(state);
        schedule_bot(state);
      }
      trigger_tone(tone, 620.0f, 36);
    }
  }
  return false;
}

}  // namespace skycrown

// ---------------------------------------------------------------------------
// Lone Lantern
// ---------------------------------------------------------------------------

namespace solitaire {

struct Card {
  int rank = 1;
  int suit = 0;
};

struct TableauCard {
  Card card{};
  bool face_up = false;
};

struct MoveSource {
  bool from_waste = false;
  int tableau = -1;
};

struct State {
  std::mt19937 rng{0x534F4C4Fu};
  std::array<std::vector<TableauCard>, 7> tableaus{};
  std::array<std::vector<Card>, 4> foundations{};
  std::vector<Card> stock;
  std::vector<Card> waste;
  std::vector<Card> held;
  MoveSource held_source{};
  bool holding = false;
  bool won = false;
  bool top_row = true;
  int slot = 0;
  int depth = 0;
  std::string status = "DRAW ONE KLONDIKE";
  std::string detail = "A PICK/DROP / B AUTO TO FOUNDATION";
};

bool is_red(int suit) {
  return suit == 1 || suit == 2;
}

std::vector<Card> build_deck() {
  std::vector<Card> deck;
  for (int suit = 0; suit < 4; ++suit) {
    for (int rank = 1; rank <= 13; ++rank) {
      deck.push_back(Card{rank, suit});
    }
  }
  return deck;
}

void start_game(State& state) {
  state = State{};
  auto deck = build_deck();
  shuffle_vector(deck, state.rng);
  int index = 0;
  for (int pile = 0; pile < 7; ++pile) {
    state.tableaus[static_cast<std::size_t>(pile)].clear();
    for (int card = 0; card <= pile; ++card) {
      state.tableaus[static_cast<std::size_t>(pile)].push_back(TableauCard{deck[static_cast<std::size_t>(index++)],
                                                                           card == pile});
    }
  }
  state.stock.assign(deck.begin() + index, deck.end());
}

bool can_stack_tableau(const Card& moving, const TableauCard& target) {
  return target.face_up && target.card.rank == moving.rank + 1 &&
         is_red(target.card.suit) != is_red(moving.suit);
}

bool can_stack_foundation(const Card& moving, const std::vector<Card>& foundation) {
  if (foundation.empty()) return moving.rank == 1;
  return foundation.back().suit == moving.suit && foundation.back().rank + 1 == moving.rank;
}

void recycle_stock(State& state) {
  std::reverse(state.waste.begin(), state.waste.end());
  state.stock = state.waste;
  state.waste.clear();
}

bool auto_foundation(State& state) {
  if (!state.waste.empty()) {
    const Card card = state.waste.back();
    if (can_stack_foundation(card, state.foundations[static_cast<std::size_t>(card.suit)])) {
      state.foundations[static_cast<std::size_t>(card.suit)].push_back(card);
      state.waste.pop_back();
      return true;
    }
  }
  if (!state.top_row) {
    auto& pile = state.tableaus[static_cast<std::size_t>(state.slot)];
    if (!pile.empty() && pile.back().face_up) {
      const Card card = pile.back().card;
      if (can_stack_foundation(card, state.foundations[static_cast<std::size_t>(card.suit)])) {
        state.foundations[static_cast<std::size_t>(card.suit)].push_back(card);
        pile.pop_back();
        if (!pile.empty() && !pile.back().face_up) pile.back().face_up = true;
        return true;
      }
    }
  }
  return false;
}

bool check_win(const State& state) {
  for (const auto& foundation : state.foundations) {
    if (foundation.size() != 13) return false;
  }
  return true;
}

void cancel_hold(State& state) {
  if (!state.holding) return;
  if (state.held_source.from_waste) {
    state.waste.insert(state.waste.end(), state.held.begin(), state.held.end());
  } else if (state.held_source.tableau >= 0) {
    auto& pile = state.tableaus[static_cast<std::size_t>(state.held_source.tableau)];
    for (const Card& card : state.held) {
      pile.push_back(TableauCard{card, true});
    }
  }
  state.held.clear();
  state.holding = false;
}

void draw_card(SDL_Renderer* renderer, int x, int y, const Card& card, bool face_up, bool highlight = false) {
  if (face_up) {
    draw_standard_card_face(renderer, x, y, rank_text_standard(card.rank), suit_text(card.suit), suit_color(card.suit));
  } else {
    draw_standard_card_back(renderer, x, y, "VD");
  }
  if (highlight) {
    draw_rect(renderer, {x - 2, y - 2, 46, 62}, kAccent);
  }
}

void draw_state(SDL_Renderer* renderer, const State& state) {
  draw_pack_frame(renderer, "LONE LANTERN", "SOLITAIRE");
  draw_panel(renderer, {26, 116, 460, 360}, "TABLE");

  const int top_y = 136;
  if (!state.stock.empty()) {
    draw_standard_card_back(renderer, 42, top_y, "VD");
  } else {
    fill_rect(renderer, {42, top_y, 42, 58}, {24, 33, 45, 255});
    draw_rect(renderer, {42, top_y, 42, 58}, kFrame);
  }
  if (!state.waste.empty()) {
    draw_card(renderer, 96, top_y, state.waste.back(), true, state.top_row && state.slot == 1);
  } else {
    fill_rect(renderer, {96, top_y, 42, 58}, {24, 33, 45, 255});
    draw_rect(renderer, {96, top_y, 42, 58}, state.top_row && state.slot == 1 ? kAccent : kFrame);
  }
  if (state.top_row && state.slot == 0) {
    draw_rect(renderer, {40, top_y - 2, 46, 62}, kAccent);
  }

  for (int foundation = 0; foundation < 4; ++foundation) {
    const int x = 242 + foundation * 54;
    if (!state.foundations[static_cast<std::size_t>(foundation)].empty()) {
      draw_card(renderer, x, top_y, state.foundations[static_cast<std::size_t>(foundation)].back(), true,
                state.top_row && state.slot == 2 + foundation);
    } else {
      fill_rect(renderer, {x, top_y, 42, 58}, {24, 33, 45, 255});
      draw_rect(renderer, {x, top_y, 42, 58}, state.top_row && state.slot == 2 + foundation ? kAccent : kFrame);
      draw_text(renderer, suit_text(foundation), x + 21, top_y + 22, 2, suit_color(foundation), true);
    }
  }

  for (int pile = 0; pile < 7; ++pile) {
    const int x = 34 + pile * 64;
    const auto& cards = state.tableaus[static_cast<std::size_t>(pile)];
    if (cards.empty()) {
      fill_rect(renderer, {x, 220, 42, 58}, {24, 33, 45, 255});
      draw_rect(renderer, {x, 220, 42, 58}, !state.top_row && state.slot == pile ? kAccent : kFrame);
    }
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
      const int y = 220 + i * (cards[static_cast<std::size_t>(i)].face_up ? 16 : 8);
      const bool highlight = !state.top_row && state.slot == pile &&
                             static_cast<int>(cards.size()) - 1 - state.depth == i;
      draw_card(renderer, x, y, cards[static_cast<std::size_t>(i)].card, cards[static_cast<std::size_t>(i)].face_up,
                highlight);
    }
  }

  draw_panel(renderer, {370, 124, 104, 106}, "STATUS");
  draw_text(renderer, fit_text(state.status, 88, 1), 422, 144, 1, kText, true);
  draw_text(renderer, fit_text(state.detail, 92, 1), 422, 166, 1, kMuted, true);
  if (state.holding) {
    draw_text(renderer, "HOLDING", 422, 188, 1, kAccent, true);
    draw_text(renderer, std::to_string(state.held.size()) + " CARD", 422, 206, 1, kText, true);
  }

  if (state.won) {
    fill_rect(renderer, {92, 184, 328, 92}, {8, 12, 18, 236});
    draw_rect(renderer, {92, 184, 328, 92}, kFrame);
    draw_text(renderer, "SOLITAIRE CLEAR", 256, 194, 3, kGold, true);
    draw_text(renderer, "A NEW DEAL", 256, 236, 1, kAccent, true);
  }
}

bool update(State& state, ToneState& tone, const EdgeInput& edge) {
  if (edge.select) {
    return true;
  }
  if (state.won) {
    if (edge.a || edge.start) {
      start_game(state);
      trigger_tone(tone, 680.0f, 60);
    }
    return false;
  }

  if (edge.b) {
    if (state.holding) {
      cancel_hold(state);
    } else if (auto_foundation(state)) {
      state.status = "AUTO TO FOUNDATION";
      trigger_tone(tone, 560.0f, 36);
      state.won = check_win(state);
    }
    return false;
  }

  if (state.top_row) {
    if (edge.left) {
      state.slot = (state.slot + 5) % 6;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right) {
      state.slot = (state.slot + 1) % 6;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.down) {
      state.top_row = false;
      state.slot = clamp_int(state.slot, 0, 6);
      state.depth = 0;
      trigger_tone(tone, 420.0f, 20);
    }
  } else {
    auto& pile = state.tableaus[static_cast<std::size_t>(state.slot)];
    if (edge.left) {
      state.slot = std::max(0, state.slot - 1);
      state.depth = 0;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right) {
      state.slot = std::min(6, state.slot + 1);
      state.depth = 0;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.up) {
      if (state.depth > 0) {
        state.depth -= 1;
      } else {
        state.top_row = true;
        state.slot = clamp_int(state.slot, 0, 5);
      }
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.down && !pile.empty()) {
      const int visible = static_cast<int>(std::count_if(pile.begin(), pile.end(), [](const TableauCard& card) {
        return card.face_up;
      }));
      if (visible > 0) {
        state.depth = std::min(visible - 1, state.depth + 1);
      }
      trigger_tone(tone, 420.0f, 20);
    }
  }

  if (edge.a) {
    if (!state.holding) {
      if (state.top_row) {
        if (state.slot == 0) {
          if (!state.stock.empty()) {
            state.waste.push_back(state.stock.back());
            state.stock.pop_back();
            state.status = "DRAWN FROM STOCK";
          } else if (!state.waste.empty()) {
            recycle_stock(state);
            state.status = "STOCK RESET";
          }
          trigger_tone(tone, 520.0f, 30);
        } else if (state.slot == 1 && !state.waste.empty()) {
          state.held.push_back(state.waste.back());
          state.waste.pop_back();
          state.holding = true;
          state.held_source.from_waste = true;
          state.held_source.tableau = -1;
          state.status = "PICKED FROM WASTE";
          trigger_tone(tone, 620.0f, 36);
        }
      } else {
        auto& pile = state.tableaus[static_cast<std::size_t>(state.slot)];
        if (!pile.empty()) {
          int selected_index = static_cast<int>(pile.size()) - 1 - state.depth;
          selected_index = clamp_int(selected_index, 0, static_cast<int>(pile.size()) - 1);
          if (!pile[static_cast<std::size_t>(selected_index)].face_up &&
              selected_index == static_cast<int>(pile.size()) - 1) {
            pile.back().face_up = true;
            trigger_tone(tone, 520.0f, 30);
          } else if (pile[static_cast<std::size_t>(selected_index)].face_up) {
            state.held.clear();
            for (int i = selected_index; i < static_cast<int>(pile.size()); ++i) {
              state.held.push_back(pile[static_cast<std::size_t>(i)].card);
            }
            pile.erase(pile.begin() + selected_index, pile.end());
            state.holding = true;
            state.held_source.from_waste = false;
            state.held_source.tableau = state.slot;
            state.status = "PICKED A RUN";
            trigger_tone(tone, 620.0f, 36);
          }
        }
      }
    } else {
      bool dropped = false;
      if (state.top_row && state.slot >= 2 && state.slot <= 5 && state.held.size() == 1) {
        auto& foundation = state.foundations[static_cast<std::size_t>(state.slot - 2)];
        if (can_stack_foundation(state.held.front(), foundation)) {
          foundation.push_back(state.held.front());
          dropped = true;
        }
      } else if (!state.top_row) {
        auto& pile = state.tableaus[static_cast<std::size_t>(state.slot)];
        if (pile.empty()) {
          if (state.held.front().rank == 13) {
            for (const Card& card : state.held) pile.push_back(TableauCard{card, true});
            dropped = true;
          }
        } else if (can_stack_tableau(state.held.front(), pile.back())) {
          for (const Card& card : state.held) pile.push_back(TableauCard{card, true});
          dropped = true;
        }
      }
      if (dropped) {
        if (!state.held_source.from_waste && state.held_source.tableau >= 0) {
          auto& source = state.tableaus[static_cast<std::size_t>(state.held_source.tableau)];
          if (!source.empty() && !source.back().face_up) source.back().face_up = true;
        }
        state.held.clear();
        state.holding = false;
        state.status = "MOVE PLACED";
        state.won = check_win(state);
        trigger_tone(tone, 620.0f, 36);
      } else {
        state.status = "CAN'T DROP THERE";
        trigger_tone(tone, 220.0f, 30);
      }
    }
  }
  return false;
}

}  // namespace solitaire

// ---------------------------------------------------------------------------
// Shade Signal
// ---------------------------------------------------------------------------

namespace shadesignal {

constexpr int kPlayers = 6;

struct WordEntry {
  const char* word;
  std::array<const char*, 3> clues;
};

struct Category {
  const char* name;
  std::array<WordEntry, 16> words;
  std::array<const char*, 4> bluff;
};

static const std::array<Category, 4> kCategories{{
    {"FOODS",
     {{{"PIZZA", {"SLICE", "CHEESE", "OVEN"}},
       {"BURGER", {"STACK", "GRILL", "BUN"}},
       {"TACO", {"SHELL", "SALSA", "FOLD"}},
       {"RAMEN", {"BROTH", "NOODLE", "STEAM"}},
       {"COOKIE", {"SWEET", "BAKE", "CRISP"}},
       {"APPLE", {"CORE", "ORCHARD", "CRUNCH"}},
       {"SUSHI", {"ROLL", "RICE", "SEA"}},
       {"PASTA", {"TWIRL", "SAUCE", "BOWL"}},
       {"STEAK", {"SEAR", "KNIFE", "JUICE"}},
       {"SOUP", {"SPOON", "SIMMER", "WARM"}},
       {"DONUT", {"GLAZE", "ROUND", "BOX"}},
       {"SALAD", {"GREEN", "TOSS", "FRESH"}},
       {"PANCAKE", {"STACK", "SYRUP", "GRIDDLE"}},
       {"POPCORN", {"MOVIE", "KERNEL", "CRUNCH"}},
       {"CURRY", {"SPICE", "POT", "RICE"}},
       {"ICE CREAM", {"COLD", "SCOOP", "MELT"}}}},
     {"SWEET", "SHARP", "HOT", "FRESH"}},
    {"PLACES",
     {{{"BEACH", {"SAND", "TIDE", "SUN"}},
       {"LIBRARY", {"SHELF", "QUIET", "BOOK"}},
       {"MUSEUM", {"EXHIBIT", "FRAME", "HALL"}},
       {"AIRPORT", {"GATE", "RUNWAY", "BOARDING"}},
       {"MARKET", {"STALL", "CROWD", "BARGAIN"}},
       {"THEATER", {"CURTAIN", "TICKET", "STAGE"}},
       {"PARK", {"GRASS", "BENCH", "PATH"}},
       {"SUBWAY", {"RAIL", "TUNNEL", "MAP"}},
       {"CASTLE", {"STONE", "TOWER", "BANNER"}},
       {"HARBOR", {"DOCK", "BOAT", "ROPE"}},
       {"DESERT", {"DUNE", "DRY", "HEAT"}},
       {"STADIUM", {"SEAT", "CHANT", "LIGHT"}},
       {"BAKERY", {"WINDOW", "LOAF", "SCENT"}},
       {"CAMP", {"TENT", "FIRE", "WOODS"}},
       {"ARCADE", {"TOKEN", "SCREEN", "NOISE"}},
       {"FACTORY", {"SMOKE", "BELT", "SHIFT"}}}},
     {"LOUD", "QUIET", "OPEN", "FAR"}},
    {"ANIMALS",
     {{{"TIGER", {"STRIPE", "PROWL", "JUNGLE"}},
       {"PENGUIN", {"WADDLE", "ICE", "COLONY"}},
       {"DOLPHIN", {"WAVE", "CLICK", "SMILE"}},
       {"OWL", {"NIGHT", "PERCH", "WING"}},
       {"FOX", {"SNEAK", "TAIL", "DEN"}},
       {"WHALE", {"GIANT", "SPRAY", "OCEAN"}},
       {"RABBIT", {"HOP", "BURROW", "EARS"}},
       {"HORSE", {"MANE", "GALLOP", "SADDLE"}},
       {"LIZARD", {"SCALE", "ROCK", "SUN"}},
       {"OTTER", {"RIVER", "FLOAT", "PLAY"}},
       {"SPIDER", {"WEB", "LEG", "THREAD"}},
       {"PARROT", {"FEATHER", "MIMIC", "TROPIC"}},
       {"WOLF", {"HOWL", "PACK", "SNOW"}},
       {"BEAR", {"PAW", "FOREST", "HONEY"}},
       {"KOALA", {"TREE", "NAP", "LEAF"}},
       {"SHARK", {"FIN", "DEEP", "TEETH"}}}},
     {"FAST", "LOUD", "SHY", "WILD"}},
    {"OBJECTS",
     {{{"LANTERN", {"GLOW", "HOOK", "NIGHT"}},
       {"COMPASS", {"NORTH", "POINTER", "TRAVEL"}},
       {"CAMERA", {"FLASH", "LENS", "SNAP"}},
       {"UMBRELLA", {"RAIN", "FOLD", "ARC"}},
       {"CLOCK", {"TICK", "FACE", "HOUR"}},
       {"VIOLIN", {"BOW", "STRING", "WOOD"}},
       {"KEY", {"LOCK", "METAL", "TURN"}},
       {"TELESCOPE", {"LENS", "SKY", "TRIPOD"}},
       {"BACKPACK", {"STRAP", "ZIP", "TRAIL"}},
       {"CANDLE", {"WAX", "FLAME", "DRIP"}},
       {"BICYCLE", {"PEDAL", "WHEEL", "LANE"}},
       {"GLASSES", {"FRAME", "LENS", "LOOK"}},
       {"HAMMER", {"NAIL", "SWING", "WORK"}},
       {"RADIO", {"TUNE", "STATIC", "DIAL"}},
       {"KETTLE", {"STEAM", "SPOUT", "POUR"}},
       {"PAINTBRUSH", {"BRISTLE", "STROKE", "COLOR"}}}},
     {"SMALL", "SHARP", "OLD", "BRIGHT"}}}};

enum class Phase { RevealPass, RevealRole, Clue, Vote, Guess, Result };

struct Player {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  bool human = false;
  bool shade = false;
  std::string clue;
  int vote = -1;
  int wins = 0;
};

struct State {
  std::mt19937 rng{0x53484144u};
  std::array<Player, kPlayers> players{};
  int human_players = 1;
  int category_index = 0;
  int secret_index = 0;
  int shade_seat = 0;
  Phase phase = Phase::RevealPass;
  int reveal_human = 0;
  int turn = 0;
  int vote_turn = 0;
  int accused = -1;
  int clue_choice = 0;
  int vote_cursor = 0;
  int guess_cursor = 0;
  Uint32 bot_ready_at = 0;
  std::string status = "PASS TO THE NEXT PLAYER";
  std::string detail = "A REVEAL";
};

const Category& category(const State& state) {
  return kCategories[static_cast<std::size_t>(state.category_index)];
}

void assign_players(State& state, int humans) {
  state.human_players = clamp_int(humans, 1, kPlayers);
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    player.human = seat < state.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                               : std::string(kBotNames[static_cast<std::size_t>(seat - state.human_players)]);
    player.color = kSeatColors[static_cast<std::size_t>(seat)];
  }
}

std::vector<int> human_seats(const State& state) {
  std::vector<int> seats;
  for (int seat = 0; seat < kPlayers; ++seat) {
    if (state.players[static_cast<std::size_t>(seat)].human) seats.push_back(seat);
  }
  return seats;
}

int current_reveal_seat(const State& state) {
  const auto seats = human_seats(state);
  if (state.reveal_human >= 0 && state.reveal_human < static_cast<int>(seats.size())) {
    return seats[static_cast<std::size_t>(state.reveal_human)];
  }
  return 0;
}

void start_round(State& state) {
  std::uniform_int_distribution<int> cat_dist(0, static_cast<int>(kCategories.size()) - 1);
  state.category_index = cat_dist(state.rng);
  std::uniform_int_distribution<int> word_dist(0, 15);
  state.secret_index = word_dist(state.rng);
  std::uniform_int_distribution<int> seat_dist(0, kPlayers - 1);
  state.shade_seat = seat_dist(state.rng);
  for (int seat = 0; seat < kPlayers; ++seat) {
    Player& player = state.players[static_cast<std::size_t>(seat)];
    player.shade = seat == state.shade_seat;
    player.clue.clear();
    player.vote = -1;
  }
  state.phase = Phase::RevealPass;
  state.reveal_human = 0;
  state.turn = 0;
  state.vote_turn = 0;
  state.accused = -1;
  state.clue_choice = 0;
  state.vote_cursor = 0;
  state.guess_cursor = 0;
  state.status = "PASS TO THE NEXT PLAYER";
  state.detail = "A REVEAL";
  state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
}

void start_match(State& state, int humans) {
  assign_players(state, humans);
  start_round(state);
}


std::string bot_pick_clue(const State& state, int seat) {
  const auto& cat = category(state);
  if (state.players[static_cast<std::size_t>(seat)].shade) {
    return cat.bluff[static_cast<std::size_t>((seat + state.secret_index) % cat.bluff.size())];
  }
  const auto& word = cat.words[static_cast<std::size_t>(state.secret_index)];
  return word.clues[static_cast<std::size_t>((seat + state.turn) % word.clues.size())];
}

int bot_vote(const State& state, int seat) {
  const auto& cat = category(state);
  int best = 0;
  int best_score = -999;
  for (int candidate = 0; candidate < kPlayers; ++candidate) {
    if (candidate == seat) continue;
    int score = 0;
    const std::string clue = upper(state.players[static_cast<std::size_t>(candidate)].clue);
    bool match = false;
    for (const auto& secret_clue : cat.words[static_cast<std::size_t>(state.secret_index)].clues) {
      if (clue == upper(secret_clue)) {
        match = true;
      }
    }
    if (!match) score += 5;
    if (candidate == state.shade_seat) score += state.players[static_cast<std::size_t>(seat)].shade ? -3 : 2;
    if (score > best_score) {
      best_score = score;
      best = candidate;
    }
  }
  return best;
}

int bot_guess(const State& state) {
  const auto& cat = category(state);
  int best = 0;
  int best_score = -999;
  for (int index = 0; index < 16; ++index) {
    int score = 0;
    for (int seat = 0; seat < kPlayers; ++seat) {
      const std::string clue = upper(state.players[static_cast<std::size_t>(seat)].clue);
      for (const auto& candidate : cat.words[static_cast<std::size_t>(index)].clues) {
        if (clue == upper(candidate)) score += 3;
      }
    }
    if (score > best_score) {
      best_score = score;
      best = index;
    }
  }
  return best;
}

void finish_result(State& state, bool shade_wins) {
  if (shade_wins) {
    state.players[static_cast<std::size_t>(state.shade_seat)].wins += 1;
    state.status = state.players[static_cast<std::size_t>(state.shade_seat)].name + " OUTWITS THE ROOM";
  } else {
    for (int seat = 0; seat < kPlayers; ++seat) {
      if (!state.players[static_cast<std::size_t>(seat)].shade) {
        state.players[static_cast<std::size_t>(seat)].wins += 1;
      }
    }
    state.status = "THE TABLE CATCHES THE SHADE";
  }
  state.detail = "ANSWER: " + std::string(category(state).words[static_cast<std::size_t>(state.secret_index)].word);
  state.phase = Phase::Result;
}

void advance_to_vote_or_guess(State& state) {
  std::array<int, kPlayers> counts{};
  for (const auto& player : state.players) {
    if (player.vote >= 0) counts[static_cast<std::size_t>(player.vote)]++;
  }
  int best = 0;
  int best_count = counts[0];
  bool tie = false;
  for (int seat = 1; seat < kPlayers; ++seat) {
    if (counts[static_cast<std::size_t>(seat)] > best_count) {
      best_count = counts[static_cast<std::size_t>(seat)];
      best = seat;
      tie = false;
    } else if (counts[static_cast<std::size_t>(seat)] == best_count) {
      tie = true;
    }
  }
  state.accused = best;
  if (tie || state.accused != state.shade_seat) {
    finish_result(state, true);
  } else {
    state.phase = Phase::Guess;
    state.guess_cursor = bot_guess(state);
    state.status = state.players[static_cast<std::size_t>(state.shade_seat)].name + " MUST GUESS";
    state.detail = "FIND THE SECRET WORD";
    state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
  }
}

void draw_state(SDL_Renderer* renderer, const State& state) {
  draw_pack_frame(renderer, "SHADE SIGNAL", "CHAMELEON");
  draw_panel(renderer, {26, 116, 298, 206}, "CATEGORY GRID");
  draw_text(renderer, category(state).name, 174, 134, 2, kGold, true);
  for (int index = 0; index < 16; ++index) {
    const int x = 42 + (index % 4) * 68;
    const int y = 156 + (index / 4) * 36;
    const SDL_Rect cell{x, y, 60, 26};
    fill_rect(renderer, cell, (state.phase == Phase::Guess && index == state.guess_cursor) ? kPanelHi : kPanel);
    draw_rect(renderer, cell, kFrame);
    const bool reveal_word = state.phase == Phase::RevealRole && !state.players[static_cast<std::size_t>(current_reveal_seat(state))].shade &&
                             index == state.secret_index;
    draw_text(renderer, fit_text(category(state).words[static_cast<std::size_t>(index)].word, 54, 1),
              x + 30, y + 8, 1, reveal_word ? kAccent2 : kText, true);
  }

  draw_panel(renderer, {336, 116, 150, 206}, "TABLE");
  draw_text(renderer, fit_text(state.status, 132, 1), 410, 138, 1, kText, true);
  draw_text(renderer, fit_text(state.detail, 132, 1), 410, 156, 1, kMuted, true);
  for (int seat = 0; seat < kPlayers; ++seat) {
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    const int y = 188 + seat * 18;
    if ((state.phase == Phase::Clue && seat == state.turn) ||
        (state.phase == Phase::Vote && seat == state.vote_turn)) {
      fill_rect(renderer, {346, y - 2, 132, 14}, kPanelHi);
    }
    draw_text(renderer, player.name, 350, y, 1, player.color);
    draw_text_right(renderer, std::to_string(player.wins), 474, y, 1, kText);
  }

  draw_panel(renderer, {26, 334, 460, 142}, "CLUES");
  for (int seat = 0; seat < kPlayers; ++seat) {
    const Player& player = state.players[static_cast<std::size_t>(seat)];
    const int y = 352 + seat * 18;
    draw_text(renderer, player.name, 40, y, 1, player.color);
    if (!player.clue.empty()) {
      draw_text(renderer, player.clue, 156, y, 1, kText);
    }
    if (state.phase == Phase::Vote || state.phase == Phase::Guess || state.phase == Phase::Result) {
      if (player.vote >= 0) {
        draw_text_right(renderer, "VOTE " + state.players[static_cast<std::size_t>(player.vote)].name, 476, y, 1, kMuted);
      }
    }
  }

  if (state.phase == Phase::RevealPass) {
    fill_rect(renderer, {88, 184, 336, 96}, {8, 12, 18, 236});
    draw_rect(renderer, {88, 184, 336, 96}, kFrame);
    draw_text(renderer, state.players[static_cast<std::size_t>(current_reveal_seat(state))].name + " ONLY",
              256, 194, 3, kGold, true);
    draw_text(renderer, "A TO REVEAL ROLE", 256, 236, 1, kText, true);
  } else if (state.phase == Phase::RevealRole) {
    const Player& player = state.players[static_cast<std::size_t>(current_reveal_seat(state))];
    fill_rect(renderer, {72, 174, 368, 116}, {8, 12, 18, 236});
    draw_rect(renderer, {72, 174, 368, 116}, kFrame);
    draw_text(renderer, player.shade ? "YOU ARE THE SHADE" : "YOU KNOW THE WORD", 256, 184, 3,
              player.shade ? kDanger : kAccent2, true);
    draw_text(renderer, player.shade ? category(state).name : category(state).words[static_cast<std::size_t>(state.secret_index)].word,
              256, 226, 2, kText, true);
    draw_text(renderer, "A CONTINUE", 256, 258, 1, kAccent, true);
  } else if (state.phase == Phase::Clue) {
    const Player& player = state.players[static_cast<std::size_t>(state.turn)];
    if (player.human) {
      fill_rect(renderer, {94, 184, 324, 92}, {8, 12, 18, 236});
      draw_rect(renderer, {94, 184, 324, 92}, kFrame);
      draw_text(renderer, player.name + " CHOOSES A CLUE", 256, 196, 2, player.color, true);
      for (int i = 0; i < 4; ++i) {
        const SDL_Rect row{116, 220 + i * 14, 280, 12};
        const bool active = i == state.clue_choice;
        fill_rect(renderer, row, active ? kPanelHi : kPanel);
        draw_text(renderer,
                  player.shade ? category(state).bluff[static_cast<std::size_t>(i)]
                               : category(state).words[static_cast<std::size_t>(state.secret_index)].clues[static_cast<std::size_t>(i % 3)],
                  122, row.y + 2, 1, active ? kAccent : kText);
      }
    }
  } else if (state.phase == Phase::Vote) {
    const Player& player = state.players[static_cast<std::size_t>(state.vote_turn)];
    if (player.human) {
      fill_rect(renderer, {102, 180, 308, 96}, {8, 12, 18, 236});
      draw_rect(renderer, {102, 180, 308, 96}, kFrame);
      draw_text(renderer, player.name + " VOTES", 256, 192, 3, player.color, true);
      draw_text(renderer, state.players[static_cast<std::size_t>(state.vote_cursor)].name, 256, 230, 2, kAccent, true);
      draw_text(renderer, "LEFT/RIGHT PICK / A CONFIRM", 256, 254, 1, kMuted, true);
    }
  } else if (state.phase == Phase::Guess) {
    fill_rect(renderer, {104, 182, 304, 88}, {8, 12, 18, 236});
    draw_rect(renderer, {104, 182, 304, 88}, kFrame);
    draw_text(renderer, "GUESS THE SECRET", 256, 192, 3, kGold, true);
    draw_text(renderer, state.players[static_cast<std::size_t>(state.shade_seat)].name + " IS ON THE CLOCK",
              256, 230, 1, kText, true);
    draw_text(renderer, "USE GRID / A CONFIRM", 256, 248, 1, kMuted, true);
  } else if (state.phase == Phase::Result) {
    fill_rect(renderer, {82, 184, 348, 96}, {8, 12, 18, 236});
    draw_rect(renderer, {82, 184, 348, 96}, kFrame);
    draw_text(renderer, "ROUND COMPLETE", 256, 194, 3, kGold, true);
    draw_text(renderer, fit_text(state.status, 310, 1), 256, 234, 1, kText, true);
    draw_text(renderer, fit_text(state.detail, 310, 1), 256, 252, 1, kMuted, true);
  }
}

bool update(State& state, ToneState& tone, const EdgeInput& edge) {
  if (edge.select) {
    return true;
  }
  const auto seats = human_seats(state);

  if (state.phase == Phase::RevealPass) {
    if (edge.a || edge.start) {
      state.phase = Phase::RevealRole;
      trigger_tone(tone, 620.0f, 36);
    }
    return false;
  }
  if (state.phase == Phase::RevealRole) {
    if (edge.a || edge.start) {
      state.reveal_human += 1;
      if (state.reveal_human >= static_cast<int>(seats.size())) {
        state.phase = Phase::Clue;
        state.turn = 0;
        state.clue_choice = 0;
        state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
      } else {
        state.phase = Phase::RevealPass;
      }
      trigger_tone(tone, 620.0f, 36);
    }
    return false;
  }
  if (state.phase == Phase::Clue) {
    if (!state.players[static_cast<std::size_t>(state.turn)].human) {
      if (SDL_GetTicks() >= state.bot_ready_at) {
        state.players[static_cast<std::size_t>(state.turn)].clue = bot_pick_clue(state, state.turn);
        state.turn += 1;
        if (state.turn >= kPlayers) {
          state.phase = Phase::Vote;
          state.vote_turn = 0;
          state.vote_cursor = 0;
        } else {
          state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
        }
      }
      return false;
    }
    Player& player = state.players[static_cast<std::size_t>(state.turn)];
    const int option_count = player.shade ? 4 : 3;
    if (edge.left || edge.up) {
      state.clue_choice = (state.clue_choice + option_count - 1) % option_count;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right || edge.down) {
      state.clue_choice = (state.clue_choice + 1) % option_count;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.a) {
      player.clue = player.shade ? category(state).bluff[static_cast<std::size_t>(state.clue_choice)]
                                 : category(state).words[static_cast<std::size_t>(state.secret_index)].clues[static_cast<std::size_t>(state.clue_choice)];
      state.turn += 1;
      state.clue_choice = 0;
      if (state.turn >= kPlayers) {
        state.phase = Phase::Vote;
        state.vote_turn = 0;
        state.vote_cursor = 0;
      } else {
        state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
      }
      trigger_tone(tone, 620.0f, 36);
    }
    return false;
  }
  if (state.phase == Phase::Vote) {
    if (!state.players[static_cast<std::size_t>(state.vote_turn)].human) {
      if (SDL_GetTicks() >= state.bot_ready_at) {
        state.players[static_cast<std::size_t>(state.vote_turn)].vote = bot_vote(state, state.vote_turn);
        state.vote_turn += 1;
        if (state.vote_turn >= kPlayers) {
          advance_to_vote_or_guess(state);
        } else {
          state.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
        }
      }
      return false;
    }
    if (edge.left || edge.up) {
      state.vote_cursor = (state.vote_cursor + kPlayers - 1) % kPlayers;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right || edge.down) {
      state.vote_cursor = (state.vote_cursor + 1) % kPlayers;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.a) {
      state.players[static_cast<std::size_t>(state.vote_turn)].vote = state.vote_cursor;
      state.vote_turn += 1;
      if (state.vote_turn >= kPlayers) {
        advance_to_vote_or_guess(state);
      }
      trigger_tone(tone, 620.0f, 36);
    }
    return false;
  }
  if (state.phase == Phase::Guess) {
    if (!state.players[static_cast<std::size_t>(state.shade_seat)].human) {
      if (SDL_GetTicks() >= state.bot_ready_at) {
        finish_result(state, bot_guess(state) == state.secret_index);
      }
      return false;
    }
    if (edge.left) {
      state.guess_cursor = (state.guess_cursor + 15) % 16;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.right) {
      state.guess_cursor = (state.guess_cursor + 1) % 16;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.up) {
      state.guess_cursor = (state.guess_cursor + 12) % 16;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.down) {
      state.guess_cursor = (state.guess_cursor + 4) % 16;
      trigger_tone(tone, 420.0f, 20);
    }
    if (edge.a) {
      finish_result(state, state.guess_cursor == state.secret_index);
      trigger_tone(tone, 620.0f, 36);
    }
    return false;
  }
  if (state.phase == Phase::Result) {
    if (edge.a || edge.start) {
      start_round(state);
      trigger_tone(tone, 700.0f, 60);
    }
  }
  return false;
}

}  // namespace shadesignal


struct MenuEntry {
  GameKind kind;
  const char* title;
  const char* tag;
  const char* description;
  int min_humans;
  int max_humans;
  int seats;
};

static const std::array<MenuEntry, 7> kMenuEntries{{
    {GameKind::Holdem, "RIVER FIVE", "HOLD'EM", "Riverboat table with bots filling the six-seat ring.", 1, 4, 6},
    {GameKind::Sevens, "SEVENS RUN", "FANTAN", "A suit-lane shedding race where sevens crack each road open.", 1, 4, 4},
    {GameKind::WildSweep, "WILD SWEEP", "UNO", "A fast color-chaos discard brawl with action cards and bot tables.", 1, 4, 4},
    {GameKind::HouseCurrent, "HOUSE CURRENT", "BLACKJACK", "Five seats against the house with bots, doubles, and hidden dealer reveals.", 1, 4, 5},
    {GameKind::SkyCrown, "SKY CROWN", "ROOK", "A bidding trick-taker with a crown trump, kitty grabs, and team scoring.", 1, 4, 4},
    {GameKind::LoneLantern, "LONE LANTERN", "SOLITAIRE", "Single-player klondike with controller-first pile movement.", 1, 1, 1},
    {GameKind::ShadeSignal, "SHADE SIGNAL", "CHAMELEON", "Hidden-role clue game built for hotseat now and LAN later.", 1, 6, 6},
}};

struct AppState {
  pp_context context{};
  ToneState tone{};
  AppScreen screen = AppScreen::Menu;
  int menu_index = 0;
  std::array<int, 7> humans{{1, 1, 1, 1, 1, 1, 1}};
  holdem::State holdem{};
  sevens::State sevens{};
  wildsweep::State wildsweep{};
  blackjack::State blackjack{};
  skycrown::State skycrown{};
  solitaire::State solitaire{};
  shadesignal::State shadesignal{};
};

const MenuEntry& current_entry(const AppState& app) {
  return kMenuEntries[static_cast<std::size_t>(app.menu_index)];
}

void start_selected_game(AppState& app) {
  const MenuEntry& entry = current_entry(app);
  switch (entry.kind) {
    case GameKind::Holdem:
      holdem::start_match(app.holdem, app.humans[static_cast<std::size_t>(app.menu_index)]);
      break;
    case GameKind::Sevens:
      sevens::start_match(app.sevens, app.humans[static_cast<std::size_t>(app.menu_index)]);
      break;
    case GameKind::WildSweep:
      wildsweep::start_match(app.wildsweep, app.humans[static_cast<std::size_t>(app.menu_index)]);
      break;
    case GameKind::HouseCurrent:
      blackjack::start_match(app.blackjack, app.humans[static_cast<std::size_t>(app.menu_index)]);
      break;
    case GameKind::SkyCrown:
      skycrown::start_match(app.skycrown, app.humans[static_cast<std::size_t>(app.menu_index)]);
      break;
    case GameKind::LoneLantern:
      solitaire::start_game(app.solitaire);
      break;
    case GameKind::ShadeSignal:
      shadesignal::start_match(app.shadesignal, app.humans[static_cast<std::size_t>(app.menu_index)]);
      break;
  }
  app.screen = AppScreen::Game;
}

void draw_menu(SDL_Renderer* renderer, const AppState& app) {
  draw_pack_frame(renderer, "VELVET DECK", "SEVEN TABLES");
  draw_panel(renderer, {28, 116, 196, 360}, "GAMES");
  draw_panel(renderer, {236, 116, 248, 206}, "DETAIL");
  draw_panel(renderer, {236, 334, 248, 142}, "CONTROLS");

  for (int i = 0; i < static_cast<int>(kMenuEntries.size()); ++i) {
    const auto& entry = kMenuEntries[static_cast<std::size_t>(i)];
    const SDL_Rect row{36, 136 + i * 52, 180, 40};
    if (i == app.menu_index) {
      fill_rect(renderer, row, kPanelHi);
    }
    draw_text(renderer, entry.title, row.x + 8, row.y + 6, 2, i == app.menu_index ? kAccent : kText);
    draw_text(renderer, entry.tag, row.x + 8, row.y + 24, 1, kMuted);
  }

  const auto& entry = current_entry(app);
  draw_text(renderer, entry.title, 360, 140, 3, kGold, true);
  draw_text(renderer, entry.tag, 360, 176, 1, kMuted, true);
  draw_text(renderer, fit_text(entry.description, 220, 1), 360, 206, 1, kText, true);
  const int human_count = app.humans[static_cast<std::size_t>(app.menu_index)];
  draw_text(renderer, "LOCAL PLAYERS", 250, 246, 1, kMuted);
  draw_text_right(renderer, std::to_string(human_count), 472, 238, 6, kAccent);
  if (entry.seats > 1) {
    draw_text(renderer, "BOTS FILL TO " + std::to_string(entry.seats), 360, 286, 1, kAccent2, true);
  } else {
    draw_text(renderer, "SOLO TABLE", 360, 286, 1, kAccent2, true);
  }

  draw_text(renderer, "UP/DOWN PICK TABLE", 360, 354, 1, kText, true);
  draw_text(renderer, "LEFT/RIGHT CHANGE LOCAL PLAYERS", 360, 374, 1, kMuted, true);
  draw_text(renderer, "A OR START TO OPEN", 360, 406, 1, kAccent, true);
  draw_text(renderer, "SELECT EXIT", 360, 434, 1, kMuted, true);
}

void update_menu(AppState& app, const EdgeInput& edge) {
  if (edge.select) {
    pp_request_exit(&app.context);
    return;
  }
  if (edge.up) {
    app.menu_index = (app.menu_index + static_cast<int>(kMenuEntries.size()) - 1) %
                     static_cast<int>(kMenuEntries.size());
    trigger_tone(app.tone, 440.0f, 20);
  }
  if (edge.down) {
    app.menu_index = (app.menu_index + 1) % static_cast<int>(kMenuEntries.size());
    trigger_tone(app.tone, 440.0f, 20);
  }
  auto& humans = app.humans[static_cast<std::size_t>(app.menu_index)];
  const auto& entry = current_entry(app);
  if (edge.left) {
    humans = std::max(entry.min_humans, humans - 1);
    trigger_tone(app.tone, 360.0f, 20);
  }
  if (edge.right) {
    humans = std::min(entry.max_humans, humans + 1);
    trigger_tone(app.tone, 520.0f, 20);
  }
  if (edge.a || edge.start) {
    start_selected_game(app);
    trigger_tone(app.tone, 700.0f, 50);
  }
}

void update_game(AppState& app, const EdgeInput& edge) {
  const MenuEntry& entry = current_entry(app);
  bool leave = false;
  switch (entry.kind) {
    case GameKind::Holdem: leave = holdem::update(app.holdem, app.tone, edge); break;
    case GameKind::Sevens: leave = sevens::update(app.sevens, app.tone, edge); break;
    case GameKind::WildSweep: leave = wildsweep::update(app.wildsweep, app.tone, edge); break;
    case GameKind::HouseCurrent: leave = blackjack::update(app.blackjack, app.tone, edge); break;
    case GameKind::SkyCrown: leave = skycrown::update(app.skycrown, app.tone, edge); break;
    case GameKind::LoneLantern: leave = solitaire::update(app.solitaire, app.tone, edge); break;
    case GameKind::ShadeSignal: leave = shadesignal::update(app.shadesignal, app.tone, edge); break;
  }
  if (leave) {
    app.screen = AppScreen::Menu;
    trigger_tone(app.tone, 300.0f, 30);
  }
}

void draw_game(SDL_Renderer* renderer, const AppState& app) {
  const MenuEntry& entry = current_entry(app);
  switch (entry.kind) {
    case GameKind::Holdem: holdem::draw_state(renderer, app.holdem); break;
    case GameKind::Sevens: sevens::draw_board(renderer, app.sevens); break;
    case GameKind::WildSweep: wildsweep::draw_state(renderer, app.wildsweep); break;
    case GameKind::HouseCurrent: blackjack::draw_state(renderer, app.blackjack); break;
    case GameKind::SkyCrown: skycrown::draw_state(renderer, app.skycrown); break;
    case GameKind::LoneLantern: solitaire::draw_state(renderer, app.solitaire); break;
    case GameKind::ShadeSignal: shadesignal::draw_state(renderer, app.shadesignal); break;
  }
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    return 1;
  }

  AppState app{};
  if (pp_init(&app.context, "velvet-deck") != 0) {
    SDL_Quit();
    return 1;
  }

  int width = kW;
  int height = kH;
  pp_get_framebuffer_size(&app.context, &width, &height);
  if (width <= 0 || height <= 0) {
    width = kW;
    height = kH;
  }

  SDL_Window* window = SDL_CreateWindow("Velvet Deck", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                        SDL_WINDOW_SHOWN);
  if (window == nullptr) {
    pp_shutdown(&app.context);
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (renderer == nullptr) {
    SDL_DestroyWindow(window);
    pp_shutdown(&app.context);
    SDL_Quit();
    return 1;
  }

  SDL_RenderSetLogicalSize(renderer, kW, kH);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  pp_audio_spec audio_spec{};
  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &app.tone;
  SDL_AudioDeviceID audio_device = 0;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }

  pp_input_state input{};
  pp_input_state previous{};

  while (!pp_should_exit(&app.context)) {
    pp_poll_input(&app.context, &input);
    const EdgeInput edge = edge_input(input, previous);
    if (app.screen == AppScreen::Menu) {
      update_menu(app, edge);
      draw_menu(renderer, app);
    } else {
      update_game(app, edge);
      draw_game(renderer, app);
    }
    SDL_RenderPresent(renderer);
    previous = input;
    SDL_Delay(16);
  }

  if (audio_device != 0) {
    SDL_CloseAudioDevice(audio_device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&app.context);
  SDL_Quit();
  return 0;
}
