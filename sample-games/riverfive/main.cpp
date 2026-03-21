#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kW = 512;
constexpr int kH = 512;
constexpr int kMaxPlayers = 6;
constexpr int kStartingStack = 1200;
constexpr int kSmallBlind = 10;
constexpr int kBigBlind = 20;
constexpr int kRaiseSize = 20;
constexpr int kBotDelayMs = 220;

enum class Screen { Config, Play, Result };
enum class Stage { Preflop, Flop, Turn, River, Showdown };
enum class ActionId { Fold = 0, CallCheck, Raise, AllIn };

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

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
  bool tracked_vpip = false;
  bool tracked_aggression = false;
  int chips = kStartingStack;
  int street_contrib = 0;
  int hand_contrib = 0;
  Card hole[2]{};
  HandValue best{};
};

struct GameState {
  pp_context context{};
  ToneState tone{};
  std::mt19937 rng{0x52564F46u};
  Screen screen = Screen::Config;
  Stage stage = Stage::Preflop;
  std::array<Player, kMaxPlayers> players{};
  std::array<Card, 52> deck{};
  std::array<Card, 5> board{};
  std::vector<int> winners{};
  int human_players = 1;
  int dealer = 0;
  int current_player = 0;
  int action_index = 0;
  int current_bet = 0;
  int pot = 0;
  int board_count = 0;
  int deck_index = 0;
  int min_raise = kRaiseSize;
  bool paused = false;
  std::string status = "UP/DOWN TO CHOOSE HUMAN SEATS";
  std::string detail = "A TO START / B TO BACK";
  HumanStyleStats human_style{};
  Uint32 last_ticks = 0;
  Uint32 bot_ready_at = 0;
};

static const SDL_Color kBgTop = {13, 18, 30, 255};
static const SDL_Color kBgMid = {18, 40, 43, 255};
static const SDL_Color kBgBottom = {24, 53, 40, 255};
static const SDL_Color kPanel = {20, 30, 37, 236};
static const SDL_Color kPanelHi = {34, 46, 57, 240};
static const SDL_Color kFrame = {94, 124, 118, 255};
static const SDL_Color kText = {238, 242, 244, 255};
static const SDL_Color kMuted = {148, 164, 171, 255};
static const SDL_Color kAccent = {247, 199, 112, 255};
static const SDL_Color kGold = {243, 212, 114, 255};
static const SDL_Color kChip = {228, 202, 114, 255};
static const SDL_Color kChipDark = {164, 129, 54, 255};
static const SDL_Color kTable = {26, 79, 58, 255};
static const SDL_Color kDanger = {249, 99, 112, 255};

static const std::array<SDL_Color, kMaxPlayers> kSeatColors{{
    {243, 112, 101, 255}, {109, 168, 244, 255}, {125, 213, 141, 255},
    {246, 201, 112, 255}, {201, 141, 243, 255}, {241, 164, 108, 255},
}};

static const std::array<const char*, 6> kBotNames{{
    "CAPTAIN", "MATE", "SMOKE", "BELL", "LANTERN", "TIDE",
}};

static const std::array<const char*, 4> kActionNames{{"FOLD", "CALL", "RAISE", "ALL IN"}};

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

double safe_ratio(int value, int total, double fallback) {
  return total > 0 ? static_cast<double>(value) / static_cast<double>(total) : fallback;
}

std::string card_rank_label(int rank) {
  switch (rank) {
    case 14: return "A";
    case 13: return "K";
    case 12: return "Q";
    case 11: return "J";
    case 10: return "10";
    default: return std::to_string(rank);
  }
}

std::string card_suit_label(int suit) {
  switch (suit) {
    case 0: return "S";
    case 1: return "H";
    case 2: return "D";
    default: return "C";
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
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case '\'': return {0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00};
    default: return {0, 0, 0, 0, 0, 0, 0};
  }
}

int text_width(const std::string& text, int scale) {
  return text.empty() ? 0 : static_cast<int>(text.size()) * (6 * scale) - scale;
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

void draw_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void draw_circle_outline(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int y = -radius; y <= radius; ++y) {
    for (int x = -radius; x <= radius; ++x) {
      const int d = x * x + y * y;
      if (d <= radius * radius && d >= (radius - 1) * (radius - 1)) {
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

void shuffle_deck(GameState& game) {
  int index = 0;
  for (int suit = 0; suit < 4; ++suit) {
    for (int rank = 2; rank <= 14; ++rank) {
      game.deck[static_cast<std::size_t>(index++)] = Card{rank, suit};
    }
  }
  std::shuffle(game.deck.begin(), game.deck.end(), game.rng);
  game.deck_index = 0;
}

Card draw_card(GameState& game) {
  return game.deck[static_cast<std::size_t>(game.deck_index++)];
}

std::vector<Card> visible_cards(const GameState& game, int seat) {
  std::vector<Card> cards;
  cards.reserve(7);
  cards.push_back(game.players[static_cast<std::size_t>(seat)].hole[0]);
  cards.push_back(game.players[static_cast<std::size_t>(seat)].hole[1]);
  for (int i = 0; i < game.board_count; ++i) {
    cards.push_back(game.board[static_cast<std::size_t>(i)]);
  }
  return cards;
}

int straight_high(std::array<int, 5> ranks) {
  std::sort(ranks.begin(), ranks.end());
  std::array<int, 5> uniq{};
  int count = 0;
  for (int rank : ranks) {
    if (count == 0 || uniq[static_cast<std::size_t>(count - 1)] != rank) {
      uniq[static_cast<std::size_t>(count++)] = rank;
    }
  }
  if (count != 5) {
    return 0;
  }
  if (uniq[4] - uniq[0] == 4) {
    return uniq[4];
  }
  if (uniq[0] == 2 && uniq[1] == 3 && uniq[2] == 4 && uniq[3] == 5 && uniq[4] == 14) {
    return 5;
  }
  return 0;
}

std::array<int, 5> ranks_desc(const std::array<Card, 5>& cards) {
  std::array<int, 5> ranks{};
  for (int i = 0; i < 5; ++i) {
    ranks[static_cast<std::size_t>(i)] = cards[static_cast<std::size_t>(i)].rank;
  }
  std::sort(ranks.begin(), ranks.end(), std::greater<int>());
  return ranks;
}

bool is_flush(const std::array<Card, 5>& cards) {
  const int suit = cards[0].suit;
  for (int i = 1; i < 5; ++i) {
    if (cards[static_cast<std::size_t>(i)].suit != suit) {
      return false;
    }
  }
  return true;
}

HandValue evaluate_five(const std::array<Card, 5>& cards) {
  std::array<int, 15> counts{};
  for (const Card& card : cards) {
    ++counts[static_cast<std::size_t>(card.rank)];
  }

  const bool flush = is_flush(cards);
  std::array<int, 5> ranks{};
  for (int i = 0; i < 5; ++i) {
    ranks[static_cast<std::size_t>(i)] = cards[static_cast<std::size_t>(i)].rank;
  }
  const int straight = straight_high(ranks);

  std::vector<std::pair<int, int>> groups;
  for (int rank = 14; rank >= 2; --rank) {
    if (counts[static_cast<std::size_t>(rank)] > 0) {
      groups.push_back({counts[static_cast<std::size_t>(rank)], rank});
    }
  }
  std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) {
    if (a.first != b.first) {
      return a.first > b.first;
    }
    return a.second > b.second;
  });

  HandValue value{};
  if (straight > 0 && flush) {
    value.category = 8;
    value.kickers = {straight, 0, 0, 0, 0};
    return value;
  }
  if (!groups.empty() && groups[0].first == 4) {
    int kicker = 0;
    for (int rank = 14; rank >= 2; --rank) {
      if (counts[static_cast<std::size_t>(rank)] == 1) {
        kicker = rank;
        break;
      }
    }
    value.category = 7;
    value.kickers = {groups[0].second, kicker, 0, 0, 0};
    return value;
  }
  if (groups.size() >= 2 && groups[0].first == 3 && groups[1].first == 2) {
    value.category = 6;
    value.kickers = {groups[0].second, groups[1].second, 0, 0, 0};
    return value;
  }
  if (flush) {
    value.category = 5;
    value.kickers = ranks_desc(cards);
    return value;
  }
  if (straight > 0) {
    value.category = 4;
    value.kickers = {straight, 0, 0, 0, 0};
    return value;
  }
  if (!groups.empty() && groups[0].first == 3) {
    std::array<int, 2> kickers{};
    int used = 0;
    for (int rank = 14; rank >= 2 && used < 2; --rank) {
      if (counts[static_cast<std::size_t>(rank)] == 1) {
        kickers[static_cast<std::size_t>(used++)] = rank;
      }
    }
    value.category = 3;
    value.kickers = {groups[0].second, kickers[0], kickers[1], 0, 0};
    return value;
  }
  if (groups.size() >= 2 && groups[0].first == 2 && groups[1].first == 2) {
    int kicker = 0;
    for (int rank = 14; rank >= 2; --rank) {
      if (counts[static_cast<std::size_t>(rank)] == 1) {
        kicker = rank;
        break;
      }
    }
    value.category = 2;
    value.kickers = {std::max(groups[0].second, groups[1].second), std::min(groups[0].second, groups[1].second), kicker, 0, 0};
    return value;
  }
  if (!groups.empty() && groups[0].first == 2) {
    std::array<int, 3> kickers{};
    int used = 0;
    for (int rank = 14; rank >= 2 && used < 3; --rank) {
      if (counts[static_cast<std::size_t>(rank)] == 1) {
        kickers[static_cast<std::size_t>(used++)] = rank;
      }
    }
    value.category = 1;
    value.kickers = {groups[0].second, kickers[0], kickers[1], kickers[2], 0};
    return value;
  }
  value.category = 0;
  value.kickers = ranks_desc(cards);
  return value;
}

bool operator<(const HandValue& a, const HandValue& b) {
  if (a.category != b.category) {
    return a.category < b.category;
  }
  for (std::size_t i = 0; i < a.kickers.size(); ++i) {
    if (a.kickers[i] != b.kickers[i]) {
      return a.kickers[i] < b.kickers[i];
    }
  }
  return false;
}

HandValue best_hand_for(const std::vector<Card>& cards) {
  if (cards.size() < 5) {
    HandValue value{};
    std::array<int, 5> ranks{};
    for (std::size_t i = 0; i < cards.size() && i < ranks.size(); ++i) {
      ranks[i] = cards[i].rank;
    }
    std::sort(ranks.begin(), ranks.end(), std::greater<int>());
    value.kickers = ranks;
    return value;
  }

  HandValue best{};
  bool have_best = false;
  for (int a = 0; a < static_cast<int>(cards.size()) - 4; ++a) {
    for (int b = a + 1; b < static_cast<int>(cards.size()) - 3; ++b) {
      for (int c = b + 1; c < static_cast<int>(cards.size()) - 2; ++c) {
        for (int d = c + 1; d < static_cast<int>(cards.size()) - 1; ++d) {
          for (int e = d + 1; e < static_cast<int>(cards.size()); ++e) {
            std::array<Card, 5> subset{cards[static_cast<std::size_t>(a)], cards[static_cast<std::size_t>(b)],
                                       cards[static_cast<std::size_t>(c)], cards[static_cast<std::size_t>(d)],
                                       cards[static_cast<std::size_t>(e)]};
            const HandValue value = evaluate_five(subset);
            if (!have_best || best < value) {
              best = value;
              have_best = true;
            }
          }
        }
      }
    }
  }
  return best;
}

std::string hand_name(const HandValue& value) {
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

int live_count(const GameState& game) {
  int count = 0;
  for (const Player& player : game.players) {
    if (!player.folded) {
      ++count;
    }
  }
  return count;
}

int actionable_count(const GameState& game) {
  int count = 0;
  for (const Player& player : game.players) {
    if (!player.folded && !player.all_in && player.chips > 0) {
      ++count;
    }
  }
  return count;
}

int next_seat(const GameState& game, int start, bool actionable_only) {
  for (int step = 1; step <= kMaxPlayers; ++step) {
    const int seat = (start + step) % kMaxPlayers;
    const Player& player = game.players[static_cast<std::size_t>(seat)];
    if (player.folded) {
      continue;
    }
    if (actionable_only && (player.all_in || player.chips <= 0)) {
      continue;
    }
    return seat;
  }
  return -1;
}

int active_after(const GameState& game, int start) { return next_seat(game, start, true); }
int live_after(const GameState& game, int start) { return next_seat(game, start, false); }

double estimate_strength(const GameState& game, int seat) {
  const auto cards = visible_cards(game, seat);
  const HandValue best = best_hand_for(cards);
  double score = static_cast<double>(best.category) * 100.0;
  score += static_cast<double>(best.kickers[0]) * 2.0;
  score += static_cast<double>(best.kickers[1]) * 0.75;
  if (cards.size() >= 2 && cards[0].rank == cards[1].rank) score += 36.0;
  if (cards.size() >= 2 && cards[0].suit == cards[1].suit) score += 8.0;
  if (cards.size() >= 2 && std::abs(cards[0].rank - cards[1].rank) <= 2) score += 5.0;
  if (game.stage == Stage::Preflop) score *= 0.72;
  if (game.stage == Stage::Flop) score *= 0.86;
  if (game.stage == Stage::Turn) score *= 0.95;
  return score;
}

bool can_check_or_call(const GameState& game, int seat) {
  return game.players[static_cast<std::size_t>(seat)].chips > 0;
}

bool can_raise(const GameState& game, int seat) {
  const Player& player = game.players[static_cast<std::size_t>(seat)];
  const int to_call = std::max(0, game.current_bet - player.street_contrib);
  return player.chips >= to_call + game.min_raise;
}

std::vector<ActionId> legal_actions(const GameState& game, int seat) {
  std::vector<ActionId> actions;
  actions.push_back(ActionId::Fold);
  if (can_check_or_call(game, seat)) actions.push_back(ActionId::CallCheck);
  if (can_raise(game, seat)) actions.push_back(ActionId::Raise);
  if (game.players[static_cast<std::size_t>(seat)].chips > 0) actions.push_back(ActionId::AllIn);
  return actions;
}

void apply_chips(GameState& game, int seat, int amount) {
  Player& player = game.players[static_cast<std::size_t>(seat)];
  amount = std::max(0, std::min(amount, player.chips));
  player.chips -= amount;
  player.street_contrib += amount;
  player.hand_contrib += amount;
  game.pot += amount;
  if (player.chips == 0) {
    player.all_in = true;
  }
}

void reset_round_state(Player& player) {
  player.folded = false;
  player.all_in = false;
  player.tracked_vpip = false;
  player.tracked_aggression = false;
  player.street_contrib = 0;
  player.hand_contrib = 0;
  player.best = {};
}

void deal_hole_cards(GameState& game) {
  for (int round = 0; round < 2; ++round) {
    for (int seat = 0; seat < kMaxPlayers; ++seat) {
      game.players[static_cast<std::size_t>(seat)].hole[round] = draw_card(game);
    }
  }
}

void reset_street(GameState& game) {
  for (Player& player : game.players) {
    player.street_contrib = 0;
  }
  game.current_bet = 0;
}

void finish_by_fold(GameState& game, int winner) {
  game.players[static_cast<std::size_t>(winner)].chips += game.pot;
  game.pot = 0;
  game.winners = {winner};
  game.status = game.players[static_cast<std::size_t>(winner)].name + " TAKES THE POT";
  game.detail = "A OR START NEXT HAND";
  game.screen = Screen::Result;
}

void settle_side_pots(GameState& game) {
  std::vector<int> tiers;
  for (const Player& player : game.players) {
    if (player.hand_contrib > 0) tiers.push_back(player.hand_contrib);
  }
  std::sort(tiers.begin(), tiers.end());
  tiers.erase(std::unique(tiers.begin(), tiers.end()), tiers.end());
  int previous = 0;
  for (int tier : tiers) {
    int contributors = 0;
    std::vector<int> eligible;
    for (int seat = 0; seat < kMaxPlayers; ++seat) {
      const Player& player = game.players[static_cast<std::size_t>(seat)];
      if (player.hand_contrib >= tier) contributors += 1;
      if (!player.folded && player.hand_contrib >= tier) eligible.push_back(seat);
    }
    const int amount = (tier - previous) * contributors;
    previous = tier;
    if (amount <= 0 || eligible.empty()) continue;

    HandValue best{};
    bool have_best = false;
    for (int seat : eligible) {
      const HandValue value = game.players[static_cast<std::size_t>(seat)].best;
      if (!have_best || best < value) {
        best = value;
        have_best = true;
      }
    }
    std::vector<int> winners;
    for (int seat : eligible) {
      const HandValue& value = game.players[static_cast<std::size_t>(seat)].best;
      if (!(value < best) && !(best < value)) {
        winners.push_back(seat);
      }
    }
    if (winners.empty()) continue;
    const int share = amount / static_cast<int>(winners.size());
    int remainder = amount % static_cast<int>(winners.size());
    for (int seat : winners) {
      game.players[static_cast<std::size_t>(seat)].chips += share;
      if (remainder > 0) {
        game.players[static_cast<std::size_t>(seat)].chips += 1;
        --remainder;
      }
    }
  }
  game.pot = 0;
}

void reveal_all_board(GameState& game) {
  while (game.board_count < 5) {
    game.board[static_cast<std::size_t>(game.board_count++)] = draw_card(game);
  }
}

void resolve_showdown(GameState& game) {
  for (int seat = 0; seat < kMaxPlayers; ++seat) {
    if (!game.players[static_cast<std::size_t>(seat)].folded) {
      game.players[static_cast<std::size_t>(seat)].best = best_hand_for(visible_cards(game, seat));
      if (game.players[static_cast<std::size_t>(seat)].human) {
        ++game.human_style.showdown_hands;
        if (game.players[static_cast<std::size_t>(seat)].tracked_aggression &&
            game.players[static_cast<std::size_t>(seat)].best.category <= 1) {
          ++game.human_style.bluff_showdowns;
        }
      }
    }
  }

  settle_side_pots(game);

  HandValue best{};
  bool have_best = false;
  for (int seat = 0; seat < kMaxPlayers; ++seat) {
    const Player& player = game.players[static_cast<std::size_t>(seat)];
    if (player.folded) continue;
    if (!have_best || best < player.best) {
      best = player.best;
      have_best = true;
    }
  }

  game.winners.clear();
  for (int seat = 0; seat < kMaxPlayers; ++seat) {
    const Player& player = game.players[static_cast<std::size_t>(seat)];
    if (player.folded) continue;
    if (!(player.best < best) && !(best < player.best)) {
      game.winners.push_back(seat);
    }
  }

  std::string text;
  for (std::size_t i = 0; i < game.winners.size(); ++i) {
    if (i > 0) text += ", ";
    text += game.players[static_cast<std::size_t>(game.winners[i])].name;
  }
  if (game.winners.size() == 1) {
    text += " WINS WITH ";
  } else {
    text += " SPLITS WITH ";
  }
  text += hand_name(best);
  game.status = text;
  game.detail = "A OR START NEXT HAND";
  game.screen = Screen::Result;
}

void advance_stage(GameState& game) {
  reset_street(game);
  if (game.stage == Stage::Preflop) {
    (void)draw_card(game);
    game.board[0] = draw_card(game);
    game.board[1] = draw_card(game);
    game.board[2] = draw_card(game);
    game.board_count = 3;
    game.stage = Stage::Flop;
    game.status = "THE FLOP";
  } else if (game.stage == Stage::Flop) {
    (void)draw_card(game);
    game.board[3] = draw_card(game);
    game.board_count = 4;
    game.stage = Stage::Turn;
    game.status = "THE TURN";
  } else if (game.stage == Stage::Turn) {
    (void)draw_card(game);
    game.board[4] = draw_card(game);
    game.board_count = 5;
    game.stage = Stage::River;
    game.status = "THE RIVER";
  } else if (game.stage == Stage::River) {
    game.stage = Stage::Showdown;
    game.status = "SHOWDOWN";
  }
  game.current_player = active_after(game, game.dealer);
  game.action_index = 0;
}

void maybe_progress(GameState& game) {
  if (live_count(game) == 1) {
    for (int seat = 0; seat < kMaxPlayers; ++seat) {
      if (!game.players[static_cast<std::size_t>(seat)].folded) {
        finish_by_fold(game, seat);
        return;
      }
    }
  }

  while (game.screen == Screen::Play) {
    if (game.stage == Stage::Showdown) {
      reveal_all_board(game);
      resolve_showdown(game);
      return;
    }

    bool complete = true;
    for (const Player& player : game.players) {
      if (!player.folded && !player.all_in && player.street_contrib != game.current_bet) {
        complete = false;
        break;
      }
    }
    if (!complete) {
      return;
    }

    if (actionable_count(game) <= 1 || game.stage == Stage::River) {
      if (game.stage == Stage::River) {
        reveal_all_board(game);
        resolve_showdown(game);
        return;
      }
      advance_stage(game);
      continue;
    }

    advance_stage(game);
  }
}

void start_hand(GameState& game) {
  game.screen = Screen::Play;
  game.stage = Stage::Preflop;
  game.pot = 0;
  game.board_count = 0;
  game.current_bet = 0;
  game.min_raise = kRaiseSize;
  game.winners.clear();
  shuffle_deck(game);
  for (Player& player : game.players) {
    if (player.chips <= 0) player.chips = kStartingStack;
    reset_round_state(player);
    if (player.human) {
      ++game.human_style.hands_observed;
    }
  }
  game.dealer = live_after(game, game.dealer);
  deal_hole_cards(game);

  const int small_blind = active_after(game, game.dealer);
  const int big_blind = active_after(game, small_blind);
  if (small_blind >= 0) {
    apply_chips(game, small_blind, std::min(kSmallBlind, game.players[static_cast<std::size_t>(small_blind)].chips));
  }
  if (big_blind >= 0) {
    apply_chips(game, big_blind, std::min(kBigBlind, game.players[static_cast<std::size_t>(big_blind)].chips));
    game.current_bet = game.players[static_cast<std::size_t>(big_blind)].street_contrib;
  }
  game.current_player = active_after(game, big_blind);
  game.action_index = 0;
  game.status = "PREFLOP";
  game.detail = "UP/DOWN MOVE / A CONFIRM / B FOLD";
  game.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
}

void reset_match(GameState& game) {
  game.human_players = clamp_int(game.human_players, 1, 4);
  game.human_style = {};
  for (int seat = 0; seat < kMaxPlayers; ++seat) {
    Player& player = game.players[static_cast<std::size_t>(seat)];
    player.human = seat < game.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                                : std::string(kBotNames[static_cast<std::size_t>(seat - game.human_players)]);
    player.color = kSeatColors[static_cast<std::size_t>(seat)];
    player.chips = kStartingStack;
    reset_round_state(player);
  }
  game.dealer = kMaxPlayers - 1;
  game.screen = Screen::Play;
  game.status = "PREFLOP";
  game.detail = "UP/DOWN MOVE / A CONFIRM / B FOLD";
  start_hand(game);
}

void observe_human_action(GameState& game, int seat, ActionId action, int to_call) {
  Player& player = game.players[static_cast<std::size_t>(seat)];
  if (!player.human) return;

  if (game.stage == Stage::Preflop && !player.tracked_vpip &&
      ((action == ActionId::CallCheck && to_call > 0) || action == ActionId::Raise || action == ActionId::AllIn)) {
    player.tracked_vpip = true;
    ++game.human_style.vpip_hands;
  }

  if (!player.tracked_aggression && (action == ActionId::Raise || action == ActionId::AllIn)) {
    player.tracked_aggression = true;
    ++game.human_style.aggressive_hands;
  }

  if (action == ActionId::Fold) {
    if (to_call > 0) ++game.human_style.folds_to_pressure;
  } else if (action == ActionId::CallCheck) {
    if (to_call > 0) {
      ++game.human_style.call_actions;
    } else {
      ++game.human_style.check_actions;
    }
  } else if (action == ActionId::Raise) {
    ++game.human_style.raise_actions;
  } else if (action == ActionId::AllIn) {
    ++game.human_style.all_in_actions;
  }
}

HumanRead human_read(const GameState& game) {
  const HumanStyleStats& style = game.human_style;
  HumanRead read{};
  read.looseness = safe_ratio(style.vpip_hands, style.hands_observed, read.looseness);
  read.foldiness = safe_ratio(style.folds_to_pressure, style.hands_observed, read.foldiness);
  read.bluffiness = safe_ratio(style.bluff_showdowns, style.showdown_hands, read.bluffiness);
  const double aggressive_actions =
      static_cast<double>(style.raise_actions) + static_cast<double>(style.all_in_actions) * 1.25;
  const double passive_actions =
      static_cast<double>(style.call_actions) + static_cast<double>(style.check_actions) * 0.65 + 1.0;
  read.aggression = std::clamp(aggressive_actions / passive_actions, 0.08, 1.20);
  return read;
}

double bot_aggression(const GameState& game, int seat) {
  const Player& player = game.players[static_cast<std::size_t>(seat)];
  const int to_call = std::max(0, game.current_bet - player.street_contrib);
  const double pressure = static_cast<double>(to_call) / std::max(1, player.chips);
  const HumanRead read = human_read(game);
  double score = estimate_strength(game, seat) * 0.01 - pressure * 1.1;
  score += read.foldiness * 0.12;
  score -= read.looseness * 0.08;
  score -= read.bluffiness * 0.06;
  if (game.stage == Stage::River) score += 0.08;
  if (game.stage == Stage::Preflop) score -= 0.05;
  return score;
}

ActionId choose_bot_action(const GameState& game, int seat) {
  const Player& player = game.players[static_cast<std::size_t>(seat)];
  const int to_call = std::max(0, game.current_bet - player.street_contrib);
  const double strength = estimate_strength(game, seat);
  const double aggression = bot_aggression(game, seat);
  const HumanRead read = human_read(game);
  const double sticky = read.looseness * (1.0 - read.foldiness * 0.75);
  const double steal = read.foldiness * (1.0 - read.looseness * 0.45);
  const double trap = std::min(1.0, read.aggression * 0.55 + read.bluffiness * 0.95);
  const double pressure = static_cast<double>(to_call) / std::max(1, player.chips);
  if (to_call > 0) {
    const double fold_threshold = 104.0 + pressure * 88.0 + sticky * 14.0 - (read.aggression + read.bluffiness) * 22.0;
    const double raise_threshold = 162.0 + sticky * 22.0 + trap * 10.0 - steal * 14.0;
    const bool trap_call = trap > 0.48 && strength > 170.0 && strength < 232.0;
    if (strength < fold_threshold && to_call > player.chips / 6 && pressure > 0.10) return ActionId::Fold;
    if (can_raise(game, seat) && strength > raise_threshold && aggression > 0.42 && !trap_call) return ActionId::Raise;
    if (can_raise(game, seat) && steal > 0.44 && pressure < 0.08 && strength > 132.0 && aggression > 0.34) return ActionId::Raise;
    if (player.chips <= to_call) return ActionId::AllIn;
    if (strength > 228.0 && aggression > 0.34 && !trap_call) return ActionId::AllIn;
    return ActionId::CallCheck;
  }
  const double open_raise_threshold = 150.0 + sticky * 20.0 - steal * 26.0;
  const double steal_threshold = 120.0 + sticky * 10.0 - steal * 22.0;
  if (can_raise(game, seat) && strength > open_raise_threshold) {
    if (trap > 0.55 && strength > 174.0 && strength < 224.0) return ActionId::CallCheck;
    return ActionId::Raise;
  }
  if (can_raise(game, seat) && aggression > 0.56 - steal * 0.16 + sticky * 0.05 && strength > steal_threshold) {
    return ActionId::Raise;
  }
  if (player.chips > 0 && strength > 220.0 + sticky * 10.0 - steal * 14.0 && aggression > 0.36) return ActionId::AllIn;
  return ActionId::CallCheck;
}

void perform_action(GameState& game, int seat, ActionId action) {
  const Stage stage_before = game.stage;
  Player& player = game.players[static_cast<std::size_t>(seat)];
  const int to_call = std::max(0, game.current_bet - player.street_contrib);
  observe_human_action(game, seat, action, to_call);
  if (action == ActionId::Fold) {
    player.folded = true;
    game.status = player.name + " FOLDS";
  } else if (action == ActionId::CallCheck) {
    if (to_call > 0) {
      apply_chips(game, seat, std::min(to_call, player.chips));
      game.status = (player.chips == 0) ? (player.name + " IS ALL IN") : (player.name + " CALLS");
    } else {
      game.status = player.name + " CHECKS";
    }
  } else if (action == ActionId::Raise) {
    if (can_raise(game, seat)) {
      const int target = game.current_bet + game.min_raise;
      apply_chips(game, seat, std::min(target - player.street_contrib, player.chips));
      game.current_bet = std::max(game.current_bet, player.street_contrib);
      game.status = player.name + " RAISES";
    } else {
      apply_chips(game, seat, std::min(to_call, player.chips));
      game.status = player.name + " CALLS";
    }
  } else if (action == ActionId::AllIn) {
    apply_chips(game, seat, player.chips);
    game.current_bet = std::max(game.current_bet, player.street_contrib);
    game.status = player.name + " GOES ALL IN";
  }

  maybe_progress(game);
  if (game.screen == Screen::Play && game.stage == stage_before && seat == game.current_player) {
    const int next = active_after(game, seat);
    if (next >= 0) {
      game.current_player = next;
      game.action_index = 0;
      if (!game.players[static_cast<std::size_t>(next)].human) {
        game.bot_ready_at = SDL_GetTicks() + kBotDelayMs;
      }
    }
  }
}

void bot_turn(GameState& game) {
  const int seat = game.current_player;
  if (seat < 0 || game.players[static_cast<std::size_t>(seat)].human ||
      game.players[static_cast<std::size_t>(seat)].folded || game.players[static_cast<std::size_t>(seat)].all_in) {
    return;
  }
  perform_action(game, seat, choose_bot_action(game, seat));
}

void seat_positions(std::array<SDL_Point, kMaxPlayers>& seats) {
  seats[0] = {256, 406};
  seats[1] = {108, 354};
  seats[2] = {88, 176};
  seats[3] = {256, 118};
  seats[4] = {424, 176};
  seats[5] = {404, 354};
}

SDL_Point seat_card_origin(int seat) {
  switch (seat) {
    case 0: return {232, 364};
    case 1: return {84, 312};
    case 2: return {64, 134};
    case 3: return {232, 76};
    case 4: return {404, 134};
    default: return {384, 312};
  }
}

SDL_Point seat_bet_origin(int seat) {
  switch (seat) {
    case 0: return {256, 334};
    case 1: return {166, 318};
    case 2: return {172, 236};
    case 3: return {256, 208};
    case 4: return {340, 236};
    default: return {346, 318};
  }
}

SDL_Point seat_dealer_origin(int seat) {
  switch (seat) {
    case 0: return {316, 400};
    case 1: return {150, 348};
    case 2: return {132, 170};
    case 3: return {316, 116};
    case 4: return {438, 170};
    default: return {438, 348};
  }
}

void draw_bg(SDL_Renderer* renderer) {
  for (int y = 0; y < kH; ++y) {
    const float t = static_cast<float>(y) / static_cast<float>(kH - 1);
    SDL_Color color{
        static_cast<Uint8>(kBgTop.r + (kBgBottom.r - kBgTop.r) * t),
        static_cast<Uint8>(kBgTop.g + (kBgBottom.g - kBgTop.g) * t),
        static_cast<Uint8>(kBgTop.b + (kBgBottom.b - kBgTop.b) * t),
        255,
    };
    fill_rect(renderer, {0, y, kW, 1}, color);
  }
  fill_rect(renderer, {0, 0, kW, 92}, kBgTop);
  fill_rect(renderer, {0, 92, kW, 28}, kBgMid);
}

void draw_card(SDL_Renderer* renderer, int x, int y, const Card& card, bool face_up) {
  const SDL_Color border = face_up ? SDL_Color{34, 76, 54, 255} : SDL_Color{70, 89, 108, 255};
  const SDL_Color fill = face_up ? SDL_Color{241, 240, 233, 255} : SDL_Color{25, 39, 53, 255};
  SDL_Rect rect{x, y, 42, 58};
  fill_rect(renderer, {rect.x + 3, rect.y + 3, rect.w, rect.h}, {10, 12, 17, 120});
  fill_rect(renderer, rect, fill);
  draw_rect(renderer, rect, border);
  draw_rect(renderer, {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4}, face_up ? SDL_Color{210, 204, 194, 255}
                                                                              : SDL_Color{57, 68, 80, 255});
  if (face_up) {
    const SDL_Color suit = (card.suit == 1 || card.suit == 2) ? SDL_Color{232, 88, 102, 255} : SDL_Color{27, 32, 42, 255};
    const std::string rank = card_rank_label(card.rank);
    const std::string suit_label = card_suit_label(card.suit);
    draw_text(renderer, rank, rect.x + 4, rect.y + 4, 1, suit);
    draw_text(renderer, suit_label, rect.x + 4, rect.y + 13, 1, suit);
    draw_text(renderer, rank, rect.x + rect.w - 4 - text_width(rank, 1), rect.y + 45, 1, suit);
    draw_text(renderer, suit_label, rect.x + rect.w - 4 - text_width(suit_label, 1), rect.y + 36, 1, suit);
    draw_text(renderer, suit_label, rect.x + 15, rect.y + 22, 4, suit, true);
  } else {
    for (int stripe = 0; stripe < 7; ++stripe) {
      draw_line(renderer, rect.x + 3 + stripe * 6, rect.y + 5, rect.x + 16 + stripe * 6, rect.y + 54,
                SDL_Color{46, 63, 84, 255});
    }
    draw_text(renderer, "RF", rect.x + 10, rect.y + 20, 2, kGold);
  }
}

void draw_chip_stack(SDL_Renderer* renderer, int x, int y, int amount, SDL_Color accent) {
  if (amount <= 0) {
    return;
  }
  const int rows = clamp_int(amount / 100, 1, 6);
  for (int i = 0; i < rows; ++i) {
    const int ry = y - i * 5;
    fill_circle(renderer, x, ry, 11, kChipDark);
    fill_circle(renderer, x, ry, 8, kChip);
    draw_line(renderer, x - 6, ry, x + 6, ry, accent);
    draw_line(renderer, x, ry - 6, x, ry + 6, accent);
  }
}

void draw_table(SDL_Renderer* renderer) {
  draw_bg(renderer);
  fill_rect(renderer, {20, 84, 472, 396}, {10, 22, 19, 255});
  fill_circle(renderer, 256, 292, 180, {10, 22, 19, 255});
  fill_circle(renderer, 256, 288, 172, kTable);
  for (int ring = 0; ring < 8; ++ring) {
    draw_circle_outline(renderer, 256, 288, 150 + ring * 2, {34, 103, 71, 255});
  }
  draw_circle_outline(renderer, 256, 288, 171, kFrame);
  draw_circle_outline(renderer, 256, 288, 158, {48, 126, 88, 255});
  fill_rect(renderer, {38, 84, 436, 10}, {53, 34, 21, 255});
  fill_rect(renderer, {52, 92, 408, 4}, {207, 177, 120, 255});
  draw_text(renderer, "RIVER FIVE", 256, 12, 4, kGold, true);
  draw_text(renderer, "RIVERBOAT HOLD'EM", 256, 48, 1, kMuted, true);
}

void draw_config(SDL_Renderer* renderer, const GameState& game) {
  draw_table(renderer);
  fill_rect(renderer, {86, 120, 340, 238}, kPanel);
  draw_rect(renderer, {86, 120, 340, 238}, kFrame);
  draw_text(renderer, "PILOTHOUSE SETUP", 256, 132, 2, kText, true);
  draw_text(renderer, "CHOOSE LOCAL HUMAN SEATS", 256, 164, 1, kMuted, true);
  draw_text(renderer, "BOTS FILL THE REST OF THE TABLE", 256, 184, 1, kMuted, true);
  draw_text(renderer, "HUMAN SEATS", 124, 230, 1, kMuted);
  draw_text_right(renderer, std::to_string(game.human_players), 378, 222, 6, kAccent);
  draw_text(renderer, "1 TO 4", 256, 280, 1, kMuted, true);
  draw_text(renderer, "UP/DOWN OR LEFT/RIGHT CHANGE", 256, 300, 1, kMuted, true);
  draw_text(renderer, "A OR START TO DEAL", 256, 320, 1, kText, true);
  draw_text(renderer, "B TO NUDGE BACK", 256, 340, 1, kMuted, true);
}

void draw_board(SDL_Renderer* renderer, const GameState& game) {
  const int start_x = 136;
  for (int i = 0; i < 5; ++i) {
    const int x = start_x + i * 48;
    if (i < game.board_count) {
      draw_card(renderer, x, 228, game.board[static_cast<std::size_t>(i)], true);
    } else {
      draw_card(renderer, x, 228, Card{}, false);
    }
  }
}

void draw_action_menu(SDL_Renderer* renderer, const GameState& game) {
  const int seat = game.current_player;
  if (seat < 0 || game.screen != Screen::Play) return;
  const auto actions = legal_actions(game, seat);
  fill_rect(renderer, {18, 360, 180, 116}, kPanel);
  draw_rect(renderer, {18, 360, 180, 116}, kFrame);
  draw_text(renderer, "ACTION", 30, 370, 1, kMuted);
  const Player& player = game.players[static_cast<std::size_t>(seat)];
  const int to_call = std::max(0, game.current_bet - player.street_contrib);
  draw_text(renderer, "TO CALL " + std::to_string(to_call), 112, 370, 1, kText, true);
  for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
    const SDL_Rect row{26, 386 + i * 18, 164, 14};
    if (i == game.action_index) {
      fill_rect(renderer, row, kPanelHi);
    }
    draw_text(renderer, kActionNames[static_cast<std::size_t>(actions[i])], row.x + 4, row.y + 2, 1,
              i == game.action_index ? kAccent : kText);
  }
  draw_text(renderer, "B FOLD", 30, 450, 1, kMuted);
}

void draw_sidebar(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {362, 100, 132, 372}, kPanel);
  draw_rect(renderer, {362, 100, 132, 372}, kFrame);
  draw_text(renderer, "TABLE", 374, 110, 1, kMuted);
  draw_text_right(renderer, "POT " + std::to_string(game.pot), 482, 110, 1, kGold);
  draw_text(renderer, "BOARD", 374, 132, 1, kMuted);
  const char* stage_text = game.stage == Stage::Preflop ? "PREFLOP" :
                           game.stage == Stage::Flop ? "FLOP" :
                           game.stage == Stage::Turn ? "TURN" :
                           game.stage == Stage::River ? "RIVER" : "SHOWDOWN";
  draw_text_right(renderer, stage_text, 482, 132, 1, kText);
  draw_text(renderer, "BET", 374, 154, 1, kMuted);
  draw_text_right(renderer, std::to_string(game.current_bet), 482, 154, 1, kText);
  draw_text(renderer, "BIG BLIND", 374, 176, 1, kMuted);
  draw_text_right(renderer, std::to_string(kBigBlind), 482, 176, 1, kText);
  draw_text(renderer, "DEALER", 374, 198, 1, kMuted);
  draw_text_right(renderer, game.players[static_cast<std::size_t>(game.dealer)].name, 482, 198, 1, kAccent);
  for (int seat = 0; seat < kMaxPlayers; ++seat) {
    const int y = 244 + seat * 18;
    const Player& player = game.players[static_cast<std::size_t>(seat)];
    if (seat == game.current_player && game.screen == Screen::Play) {
      fill_rect(renderer, {370, y - 1, 112, 14}, kPanelHi);
    }
    draw_text(renderer, player.name, 374, y, 1, player.color);
    draw_text_right(renderer, std::to_string(player.chips), 482, y, 1, player.folded ? kMuted : kText);
  }
  fill_rect(renderer, {370, 362, 108, 92}, kPanelHi);
  draw_rect(renderer, {370, 362, 108, 92}, kFrame);
  draw_text(renderer, "STATUS", 380, 372, 1, kMuted);
  draw_text(renderer, game.status, 424, 390, 1, kText, true);
  draw_text(renderer, game.detail, 424, 408, 1, kMuted, true);
}

void draw_play(SDL_Renderer* renderer, const GameState& game) {
  draw_table(renderer);
  draw_board(renderer, game);
  std::array<SDL_Point, kMaxPlayers> seats{};
  seat_positions(seats);
  for (int seat = 0; seat < kMaxPlayers; ++seat) {
    const Player& player = game.players[static_cast<std::size_t>(seat)];
    const SDL_Point pos = seats[static_cast<std::size_t>(seat)];
    SDL_Color label = player.folded ? kMuted : player.color;
    if (seat == game.current_player && game.screen == Screen::Play) label = kAccent;
    draw_text(renderer, player.name, pos.x, pos.y, 1, label, true);
    draw_text(renderer, std::to_string(player.chips) + " C", pos.x, pos.y + 12, 1, kText, true);
    if (player.all_in) draw_text(renderer, "ALL IN", pos.x, pos.y + 24, 1, kGold, true);
    if (player.folded) draw_text(renderer, "FOLDED", pos.x, pos.y + 24, 1, kMuted, true);
    if (seat == game.dealer) {
      const SDL_Point dealer_pos = seat_dealer_origin(seat);
      fill_circle(renderer, dealer_pos.x, dealer_pos.y, 8, kGold);
      draw_text(renderer, "D", dealer_pos.x, dealer_pos.y - 5, 1, kPanel, true);
    }
    const SDL_Point origin = seat_card_origin(seat);
    const SDL_Point bet_pos = seat_bet_origin(seat);
    const bool reveal = game.screen == Screen::Result;
    const bool show_current_human = game.screen == Screen::Play && seat == game.current_player && player.human;
    draw_card(renderer, origin.x, origin.y, player.hole[0], reveal || show_current_human);
    draw_card(renderer, origin.x + 26, origin.y, player.hole[1], reveal || show_current_human);
    draw_chip_stack(renderer, bet_pos.x, bet_pos.y, player.hand_contrib,
                    seat == game.current_player ? kAccent : kChipDark);
  }

  fill_rect(renderer, {20, 100, 174, 46}, kPanel);
  draw_rect(renderer, {20, 100, 174, 46}, kFrame);
  draw_text(renderer, "STREET", 30, 110, 1, kMuted);
  draw_text(renderer, game.stage == Stage::Preflop ? "PREFLOP" :
                     game.stage == Stage::Flop ? "FLOP" :
                     game.stage == Stage::Turn ? "TURN" :
                     game.stage == Stage::River ? "RIVER" : "SHOWDOWN", 102, 108, 2, kText, true);
  draw_text(renderer, "POT " + std::to_string(game.pot), 30, 126, 1, kGold);
  draw_text_right(renderer, "BET " + std::to_string(game.current_bet), 184, 126, 1, kText);

  fill_rect(renderer, {20, 154, 174, 90}, kPanel);
  draw_rect(renderer, {20, 154, 174, 90}, kFrame);
  draw_text(renderer, "CURRENT", 30, 164, 1, kMuted);
  if (game.current_player >= 0) {
    const Player& current = game.players[static_cast<std::size_t>(game.current_player)];
    draw_text(renderer, current.name, 102, 180, 2, current.human ? current.color : kText, true);
    const int to_call = std::max(0, game.current_bet - current.street_contrib);
    draw_text(renderer, "TO CALL " + std::to_string(to_call), 102, 204, 1, kText, true);
    draw_text(renderer, current.human ? "YOUR TURN" : "BOT THINKS", 102, 220, 1, kMuted, true);
  }

  if (game.screen == Screen::Play && game.current_player >= 0 && game.players[static_cast<std::size_t>(game.current_player)].human) {
    draw_action_menu(renderer, game);
  }
  draw_sidebar(renderer, game);
}

void draw_result(SDL_Renderer* renderer, const GameState& game) {
  draw_play(renderer, game);
  fill_rect(renderer, {74, 182, 364, 112}, {10, 14, 18, 232});
  draw_rect(renderer, {74, 182, 364, 112}, kFrame);
  draw_text(renderer, "HAND COMPLETE", 256, 192, 3, kGold, true);
  draw_text(renderer, game.status, 256, 232, 1, kText, true);
  draw_text(renderer, game.detail, 256, 256, 1, kMuted, true);
  draw_text(renderer, "A OR START NEXT HAND", 256, 274, 1, kAccent, true);
}

void draw_scene(SDL_Renderer* renderer, const GameState& game) {
  if (game.screen == Screen::Config) {
    draw_config(renderer, game);
  } else if (game.screen == Screen::Play) {
    draw_play(renderer, game);
  } else {
    draw_result(renderer, game);
  }
}

void start_new_match(GameState& game) {
  game.human_players = clamp_int(game.human_players, 1, 4);
  game.human_style = {};
  for (int seat = 0; seat < kMaxPlayers; ++seat) {
    Player& player = game.players[static_cast<std::size_t>(seat)];
    player.human = seat < game.human_players;
    player.name = player.human ? ("P" + std::to_string(seat + 1))
                                : std::string(kBotNames[static_cast<std::size_t>(seat - game.human_players)]);
    player.color = kSeatColors[static_cast<std::size_t>(seat)];
    player.chips = kStartingStack;
    reset_round_state(player);
  }
  game.dealer = kMaxPlayers - 1;
  game.screen = Screen::Play;
  game.status = "PREFLOP";
  game.detail = "UP/DOWN MOVE / A CONFIRM / B FOLD";
  start_hand(game);
}

void handle_config_input(GameState& game, const pp_input_state& input, const pp_input_state& previous) {
  const bool up = input.up && !previous.up;
  const bool down = input.down && !previous.down;
  const bool left = input.left && !previous.left;
  const bool right = input.right && !previous.right;
  const bool a = input.a && !previous.a;
  const bool b = input.b && !previous.b;
  const bool start = input.start && !previous.start;

  if (up || right) {
    game.human_players = std::min(4, game.human_players + 1);
    trigger_tone(game.tone, 520.0f, 34);
  }
  if (down || left || b) {
    game.human_players = std::max(1, game.human_players - 1);
    trigger_tone(game.tone, 360.0f, 34);
  }
  if (a || start) {
    start_new_match(game);
    trigger_tone(game.tone, 720.0f, 60);
  }
}

void handle_play_input(GameState& game, const pp_input_state& input, const pp_input_state& previous) {
  const bool up = input.up && !previous.up;
  const bool down = input.down && !previous.down;
  const bool left = input.left && !previous.left;
  const bool right = input.right && !previous.right;
  const bool a = input.a && !previous.a;
  const bool b = input.b && !previous.b;
  const bool start = input.start && !previous.start;
  const bool select = input.select && !previous.select;

  if (select) {
    pp_request_exit(&game.context);
    return;
  }
  if (game.screen == Screen::Result) {
    if (a || start) {
      start_hand(game);
      trigger_tone(game.tone, 720.0f, 60);
    }
    return;
  }
  if (start) {
    game.paused = !game.paused;
    game.status = game.paused ? "PAUSED" : "RESUMED";
    trigger_tone(game.tone, game.paused ? 280.0f : 560.0f, 40);
    return;
  }
  if (game.paused) {
    return;
  }
  if (game.current_player < 0) {
    return;
  }

  Player& current = game.players[static_cast<std::size_t>(game.current_player)];
  if (!current.human || current.folded || current.all_in) {
    return;
  }

  const auto actions = legal_actions(game, game.current_player);
  if (actions.empty()) {
    return;
  }
  if (up || left) {
    game.action_index = (game.action_index + static_cast<int>(actions.size()) - 1) % static_cast<int>(actions.size());
    trigger_tone(game.tone, 420.0f, 20);
  }
  if (down || right) {
    game.action_index = (game.action_index + 1) % static_cast<int>(actions.size());
    trigger_tone(game.tone, 420.0f, 20);
  }
  if (b) {
    perform_action(game, game.current_player, ActionId::Fold);
    trigger_tone(game.tone, 260.0f, 40);
    return;
  }
  if (a) {
    const int index = clamp_int(game.action_index, 0, static_cast<int>(actions.size()) - 1);
    perform_action(game, game.current_player, actions[static_cast<std::size_t>(index)]);
    trigger_tone(game.tone, 620.0f, 40);
  }
}

void update_game(GameState& game, const pp_input_state& input, const pp_input_state& previous) {
  if (game.screen == Screen::Config) {
    handle_config_input(game, input, previous);
    return;
  }

  handle_play_input(game, input, previous);
  if (game.screen != Screen::Play || game.paused) {
    return;
  }
  if (game.current_player >= 0 && !game.players[static_cast<std::size_t>(game.current_player)].human &&
      SDL_GetTicks() >= game.bot_ready_at) {
    bot_turn(game);
  }
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    return 1;
  }

  GameState game{};
  if (pp_init(&game.context, "riverfive") != 0) {
    SDL_Quit();
    return 1;
  }

  int width = kW;
  int height = kH;
  pp_get_framebuffer_size(&game.context, &width, &height);
  if (width <= 0 || height <= 0) {
    width = kW;
    height = kH;
  }

  SDL_Window* window = SDL_CreateWindow("River Five", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                        SDL_WINDOW_SHOWN);
  if (window == nullptr) {
    pp_shutdown(&game.context);
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (renderer == nullptr) {
    SDL_DestroyWindow(window);
    pp_shutdown(&game.context);
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
  audio_spec.userdata = &game.tone;
  SDL_AudioDeviceID audio_device = 0;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }

  pp_input_state input{};
  pp_input_state previous{};
  game.last_ticks = SDL_GetTicks();

  while (!pp_should_exit(&game.context)) {
    const Uint32 now = SDL_GetTicks();
    float dt = static_cast<float>(now - game.last_ticks) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    game.last_ticks = now;
    (void)dt;

    pp_poll_input(&game.context, &input);
    update_game(game, input, previous);
    draw_scene(renderer, game);
    SDL_RenderPresent(renderer);
    previous = input;
    SDL_Delay(16);
  }

  if (audio_device != 0) {
    SDL_CloseAudioDevice(audio_device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&game.context);
  SDL_Quit();
  return 0;
}
