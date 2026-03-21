#include "pixelpal/pixelpal.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kWindowWidth = 512;
constexpr int kWindowHeight = 512;
constexpr int kPanelX = 366;
constexpr int kPanelY = 84;
constexpr int kPanelW = 136;
constexpr int kPanelH = 390;
constexpr int kVictoryPointsToWin = 10;
constexpr Uint32 kCpuDelayMs = 420U;

enum class Resource { Wood = 0, Brick, Sheep, Wheat, Ore, Gold, Desert, Sea };
enum class GameMode { Cpu = 0, Local };
enum class Variant { Classic = 0, Seafarers };
enum class DevCard { Knight = 0, VictoryPoint, RoadBuilding, YearOfPlenty, Monopoly };
enum class TradeMode { None = 0, Player, Bank, Port };
enum class RouteKind { None = 0, Road, Ship };
enum class Screen {
  Variant,
  Mode,
  Humans,
  Bots,
  Intro,
  SetupSettlement,
  SetupRoute,
  Roll,
  Action,
  PlaceSettlement,
  PlaceRoad,
  PlaceShip,
  MoveShip,
  PlaceCity,
  TradeMenu,
  TradePartner,
  TradeGive,
  TradeTake,
  TradeOfferPrompt,
  DevCards,
  DevPlenty,
  DevMonopoly,
  RaidChoice,
  Robber,
  Pirate,
  RobberSteal,
  PirateSteal,
  GoldChoice,
  GameOver
};
enum class TargetKind { None = 0, Node, Edge, Hex };
enum class ActionId { Roll = 0, BuildRoad, BuildShip, MoveShip, BuildSettlement, BuildCity, BuyDevCard, ViewDevCards, Trade, EndTurn };

struct Theme {
  SDL_Color bg{18, 24, 34, 255};
  SDL_Color panel{33, 40, 58, 255};
  SDL_Color panel_hi{45, 53, 76, 255};
  SDL_Color frame{80, 97, 139, 255};
  SDL_Color text{241, 242, 246, 255};
  SDL_Color muted{148, 160, 192, 255};
  SDL_Color accent{255, 204, 115, 255};
  SDL_Color overlay{8, 12, 18, 255};
};

struct ToneState {
  float phase = 0.0f;
  float frequency = 0.0f;
  int frames_remaining = 0;
};

struct Point {
  int x = 0;
  int y = 0;
};

struct Hex {
  Resource resource = Resource::Wood;
  int token = 0;
  int q = 0;
  int r = 0;
  int cx = 0;
  int cy = 0;
  int robber = 0;
  int pirate = 0;
  int island = -1;
  std::array<int, 6> nodes{};
};

struct Node {
  int x = 0;
  int y = 0;
  int owner = -1;
  int city = 0;
  std::vector<int> hexes;
  std::vector<int> edges;
  std::vector<int> neighbors;
};

struct Edge {
  int a = -1;
  int b = -1;
  int owner = -1;
  RouteKind route = RouteKind::None;
  int built_turn = -1;
  std::array<int, 2> hexes{{-1, -1}};
  int hex_count = 0;
};

struct Player {
  std::string name;
  SDL_Color color{255, 255, 255, 255};
  int human = 1;
  std::array<int, 5> resources{};
  std::array<int, 5> dev_cards{};
  std::array<int, 5> new_dev_cards{};
  int roads_left = 15;
  int ships_left = 15;
  int settlements_left = 5;
  int cities_left = 4;
  int special_points = 0;
  int victory_points = 0;
  int longest_road = 0;
  int has_longest_road = 0;
  int used_knights = 0;
  int has_largest_army = 0;
};

struct BoardState {
  std::vector<Hex> hexes;
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  int expanded = 0;
  int seafarers = 0;
  std::vector<int> island_claimed_by;
};

struct PortInfo {
  Point ocean{};
  int edge_index = -1;
  int rate = 3;
  int resource = -1;
  std::string label;
};

struct TradeProposal {
  int from = -1;
  int to = -1;
  int give = -1;
  int want = -1;
  int rate = 1;
  double score = -9999.0;
};

struct BoardView {
  SDL_Rect clip{};
  int offset_x = 0;
  int offset_y = 0;
};

struct GameState {
  Theme theme;
  ToneState tone;
  std::mt19937 rng{0x51E771E7u};
  Screen screen = Screen::Variant;
  GameMode mode = GameMode::Cpu;
  Variant variant = Variant::Classic;
  int menu_index = 0;
  int local_humans = 2;
  int bots = 1;
  int total_players = 2;
  BoardState board;
  std::vector<Player> players;
  int current_player = 0;
  int setup_index = 0;
  int last_setup_node = -1;
  int action_index = 0;
  int last_roll = 0;
  int trade_resource = 0;
  TradeMode trade_mode = TradeMode::None;
  std::vector<int> trade_partners;
  int trade_partner = -1;
  int trade_give = -1;
  int trade_want = -1;
  int trade_rate = 4;
  TradeProposal pending_trade{};
  int prompt_choice = 0;
  TargetKind target_kind = TargetKind::None;
  std::vector<int> valid_targets;
  int target_index = 0;
  std::vector<DevCard> dev_deck;
  std::vector<int> robber_targets;
  int robber_target_index = 0;
  int played_dev_this_turn = 0;
  int free_roads_to_place = 0;
  int plenty_picks_left = 0;
  int turn_number = 0;
  RouteKind route_mode = RouteKind::Road;
  int raid_choice = 0;
  std::vector<std::pair<int, int>> gold_picks;
  int gold_pick_resource = 0;
  int selected_ship_edge = -1;
  int dice_a = 0;
  int dice_b = 0;
  std::string status = "HEX FRONTIER";
  Uint32 ai_ready_at = 0U;
  int winner = -1;
  float board_view_x = 0.0f;
  float board_view_y = 0.0f;
  int board_view_ready = 0;
};

int edge_land_count(const BoardState& board, const Edge& edge);
int edge_sea_count(const BoardState& board, const Edge& edge);

constexpr std::array<SDL_Color, 6> kPlayerColors{{
    {236, 96, 89, 255},
    {82, 129, 196, 255},
    {94, 183, 116, 255},
    {240, 191, 91, 255},
    {189, 107, 198, 255},
    {228, 150, 98, 255},
}};

constexpr std::array<const char*, 5> kResourceNames{{"WOOD", "BRICK", "SHEEP", "WHEAT", "ORE"}};
constexpr std::array<const char*, 5> kDevCardNames{{"KNIGHT", "VICTORY POINT", "ROAD BUILDING", "YEAR OF PLENTY", "MONOPOLY"}};
constexpr std::array<const char*, 5> kDevCardTooltip{{"MOVE ROBBER / STEAL 1 CARD",
                                                      "HIDDEN VP / SCORES AUTOMATICALLY",
                                                      "PLACE 2 FREE ROADS",
                                                      "TAKE ANY 2 RESOURCES",
                                                      "TAKE 1 RESOURCE TYPE FROM ALL"}};

std::string uppercase(const std::string& value) {
  std::string out = value;
  for (char& ch : out) {
    if (ch >= 'a' && ch <= 'z') {
      ch = static_cast<char>(ch - 'a' + 'A');
    }
  }
  return out;
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
    case '#': return {0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00};
    case '.': return {0, 0, 0, 0, 0, 0x0C, 0x0C};
    default: return {0, 0, 0, 0, 0, 0, 0};
  }
}

int text_width(const std::string& text, int scale) {
  return text.empty() ? 0 : static_cast<int>(text.size()) * (6 * scale) - scale;
}

void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer, &rect);
}

void draw_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer, &rect);
}

void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, int scale, SDL_Color color,
               bool centered = false) {
  const std::string upper = uppercase(text);
  int draw_x = centered ? x - text_width(upper, scale) / 2 : x;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (char ch : upper) {
    const auto glyph = glyph_for(ch);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((glyph[row] & (1 << (4 - col))) == 0) {
          continue;
        }
        SDL_Rect pixel{draw_x + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    draw_x += 6 * scale;
  }
}

void draw_text_right(SDL_Renderer* renderer, const std::string& text, int right_x, int y, int scale, SDL_Color color) {
  draw_text(renderer, text, right_x - text_width(text, scale), y, scale, color, false);
}

int fit_text_scale(const std::string& text, int max_width, int preferred_scale, int min_scale = 1) {
  for (int scale = preferred_scale; scale >= min_scale; --scale) {
    if (text_width(text, scale) <= max_width) {
      return scale;
    }
  }
  return min_scale;
}

std::vector<std::string> wrap_text(const std::string& text, int max_width, int scale) {
  std::vector<std::string> lines;
  if (text.empty()) {
    return lines;
  }

  std::string current;
  std::string word;
  auto flush_word = [&]() {
    if (word.empty()) {
      return;
    }
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
  if (!current.empty()) {
    lines.push_back(current);
  }
  return lines;
}

void draw_wrapped_text(SDL_Renderer* renderer, const std::string& text, int x, int y, int width, int scale, int max_lines,
                       SDL_Color color) {
  const auto lines = wrap_text(text, width, scale);
  for (int i = 0; i < static_cast<int>(lines.size()) && i < max_lines; ++i) {
    draw_text(renderer, lines[static_cast<std::size_t>(i)], x, y + i * (8 * scale + 2), scale, color);
  }
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

void audio_callback(void* userdata, Uint8* stream, int length) {
  auto* tone = static_cast<ToneState*>(userdata);
  auto* samples = reinterpret_cast<int16_t*>(stream);
  const int count = length / static_cast<int>(sizeof(int16_t));
  for (int i = 0; i < count; ++i) {
    int16_t sample = 0;
    if (tone->frames_remaining > 0) {
      sample = tone->phase < 3.14159f ? 1600 : -1600;
      tone->phase += (6.28318f * tone->frequency) / 48000.0f;
      if (tone->phase >= 6.28318f) {
        tone->phase -= 6.28318f;
      }
      --tone->frames_remaining;
    }
    samples[i] = sample;
  }
}

void trigger_tone(ToneState& tone, float frequency, int ms) {
  tone.frequency = frequency;
  tone.frames_remaining = (48000 * ms) / 1000;
}

int pip_value(int token) {
  switch (token) {
    case 2:
    case 12: return 1;
    case 3:
    case 11: return 2;
    case 4:
    case 10: return 3;
    case 5:
    case 9: return 4;
    case 6:
    case 8: return 5;
    default: return 0;
  }
}

SDL_Color resource_color(Resource resource) {
  switch (resource) {
    case Resource::Wood: return {77, 133, 92, 255};
    case Resource::Brick: return {182, 97, 80, 255};
    case Resource::Sheep: return {172, 206, 116, 255};
    case Resource::Wheat: return {227, 195, 102, 255};
    case Resource::Ore: return {124, 130, 147, 255};
    case Resource::Gold: return {226, 182, 74, 255};
    case Resource::Desert: return {211, 186, 125, 255};
    case Resource::Sea: return {59, 108, 160, 255};
    default: return {200, 200, 200, 255};
  }
}

bool land_resource(Resource resource) {
  return resource != Resource::Sea;
}

bool productive_resource(Resource resource) {
  return resource != Resource::Sea && resource != Resource::Desert;
}

std::vector<Point> hex_points(int cx, int cy, int radius) {
  std::vector<Point> points;
  for (int i = 0; i < 6; ++i) {
    const double angle = (60.0 * i - 30.0) * 3.1415926535 / 180.0;
    points.push_back({static_cast<int>(std::round(cx + radius * std::cos(angle))),
                      static_cast<int>(std::round(cy + radius * std::sin(angle)))});
  }
  return points;
}

void fill_polygon(SDL_Renderer* renderer, const std::vector<Point>& points, SDL_Color color) {
  if (points.size() < 3) {
    return;
  }
  int min_y = points.front().y;
  int max_y = points.front().y;
  for (const auto& point : points) {
    min_y = std::min(min_y, point.y);
    max_y = std::max(max_y, point.y);
  }
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int y = min_y; y <= max_y; ++y) {
    std::vector<int> hits;
    for (std::size_t i = 0; i < points.size(); ++i) {
      const Point a = points[i];
      const Point b = points[(i + 1) % points.size()];
      if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
        const float t = static_cast<float>(y - a.y) / static_cast<float>(b.y - a.y);
        hits.push_back(static_cast<int>(std::round(a.x + (b.x - a.x) * t)));
      }
    }
    std::sort(hits.begin(), hits.end());
    for (std::size_t i = 1; i < hits.size(); i += 2) {
      SDL_RenderDrawLine(renderer, hits[i - 1], y, hits[i], y);
    }
  }
}

void draw_hex_outline(SDL_Renderer* renderer, const std::vector<Point>& points, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (std::size_t i = 0; i < points.size(); ++i) {
    const Point a = points[i];
    const Point b = points[(i + 1) % points.size()];
    SDL_RenderDrawLine(renderer, a.x, a.y, b.x, b.y);
  }
}

std::vector<std::pair<int, int>> layout_rows(int expanded) {
  const int top = expanded ? 112 : 124;
  const int step = expanded ? 38 : 48;
  return expanded ? std::vector<std::pair<int, int>>{{3, top + step * 0},
                                                      {4, top + step * 1},
                                                      {5, top + step * 2},
                                                      {6, top + step * 3},
                                                      {5, top + step * 4},
                                                      {4, top + step * 5},
                                                      {3, top + step * 6}}
                  : std::vector<std::pair<int, int>>{{3, top + step * 0},
                                                      {4, top + step * 1},
                                                      {5, top + step * 2},
                                                      {4, top + step * 3},
                                                      {3, top + step * 4}};
}

int board_radius(int expanded) {
  return expanded ? 25 : 32;
}

int board_step_x(int expanded) {
  return expanded ? 44 : 56;
}

int board_step_y(int expanded) {
  return expanded ? 38 : 48;
}

int board_center_x(int expanded) {
  return expanded ? 174 : 182;
}

SDL_Rect board_clip_rect() {
  return {8, 88, kPanelX - 20, 298};
}

int shared_hex_count(const BoardState& board, const Edge& edge) {
  int count = 0;
  for (int a_hex : board.nodes[edge.a].hexes) {
    for (int b_hex : board.nodes[edge.b].hexes) {
      if (a_hex == b_hex) {
        ++count;
      }
    }
  }
  return count;
}

std::vector<Point> ocean_centers(const BoardState& board) {
  if (board.seafarers) {
    std::vector<Point> ocean;
    for (int hex_index = 0; hex_index < static_cast<int>(board.hexes.size()); ++hex_index) {
      const Hex& hex = board.hexes[hex_index];
      if (hex.resource != Resource::Sea) {
        continue;
      }
      bool coastal = false;
      for (int node_index : hex.nodes) {
        for (int neighbor_hex : board.nodes[node_index].hexes) {
          if (neighbor_hex != hex_index && land_resource(board.hexes[neighbor_hex].resource)) {
            coastal = true;
            break;
          }
        }
        if (coastal) {
          break;
        }
      }
      if (coastal) {
        ocean.push_back({hex.cx, hex.cy});
      }
    }
    return ocean;
  }
  const int expanded = board.expanded;
  const int step_x = board_step_x(expanded);
  const int step_y = board_step_y(expanded);
  std::vector<Point> ocean;
  auto has_center = [](const std::vector<Point>& points, int x, int y, int tolerance_sq) {
    for (const Point& point : points) {
      const int dx = point.x - x;
      const int dy = point.y - y;
      if (dx * dx + dy * dy <= tolerance_sq) {
        return true;
      }
    }
    return false;
  };

  std::vector<Point> land;
  for (const Hex& hex : board.hexes) {
    land.push_back({hex.cx, hex.cy});
  }

  const std::array<Point, 6> offsets{{{step_x, 0}, {-step_x, 0}, {step_x / 2, -step_y},
                                      {-step_x / 2, -step_y}, {step_x / 2, step_y}, {-step_x / 2, step_y}}};
  for (const Point& center : land) {
    for (const Point& offset : offsets) {
      const int x = center.x + offset.x;
      const int y = center.y + offset.y;
      if (!has_center(land, x, y, 36) && !has_center(ocean, x, y, 25)) {
        ocean.push_back({x, y});
      }
    }
  }
  return ocean;
}

std::vector<PortInfo> build_ports(const GameState& game) {
  auto ocean = ocean_centers(game.board);
  const int center_x = board_center_x(game.board.expanded);
  const int center_y = game.board.expanded ? 226 : 220;
  std::sort(ocean.begin(), ocean.end(), [&](const Point& a, const Point& b) {
    return std::atan2(static_cast<double>(a.y - center_y), static_cast<double>(a.x - center_x)) <
           std::atan2(static_cast<double>(b.y - center_y), static_cast<double>(b.x - center_x));
  });

  struct PortSeed {
    const char* label;
    int rate;
    int resource;
  };
  const std::vector<PortSeed> seeds = game.board.expanded
                                          ? std::vector<PortSeed>{{"3:1", 3, -1}, {"WOOD", 2, static_cast<int>(Resource::Wood)},
                                                                  {"3:1", 3, -1}, {"BRICK", 2, static_cast<int>(Resource::Brick)},
                                                                  {"3:1", 3, -1}, {"SHEEP", 2, static_cast<int>(Resource::Sheep)},
                                                                  {"WHEAT", 2, static_cast<int>(Resource::Wheat)}, {"3:1", 3, -1},
                                                                  {"ORE", 2, static_cast<int>(Resource::Ore)}, {"3:1", 3, -1},
                                                                  {"3:1", 3, -1}}
                                          : std::vector<PortSeed>{{"3:1", 3, -1}, {"WOOD", 2, static_cast<int>(Resource::Wood)},
                                                                  {"3:1", 3, -1}, {"BRICK", 2, static_cast<int>(Resource::Brick)},
                                                                  {"SHEEP", 2, static_cast<int>(Resource::Sheep)},
                                                                  {"WHEAT", 2, static_cast<int>(Resource::Wheat)},
                                                                  {"ORE", 2, static_cast<int>(Resource::Ore)}, {"3:1", 3, -1},
                                                                  {"3:1", 3, -1}};

  std::vector<int> port_indices;
  if (!ocean.empty()) {
    for (int i = 0; i < static_cast<int>(seeds.size()); ++i) {
      const int index = (i * static_cast<int>(ocean.size())) / static_cast<int>(seeds.size());
      if (std::find(port_indices.begin(), port_indices.end(), index) == port_indices.end()) {
        port_indices.push_back(index);
      }
    }
  }

  std::vector<int> coastal_edges;
  for (int edge_index = 0; edge_index < static_cast<int>(game.board.edges.size()); ++edge_index) {
    if (game.board.seafarers) {
      if (edge_land_count(game.board, game.board.edges[edge_index]) > 0 && edge_sea_count(game.board, game.board.edges[edge_index]) > 0) {
        coastal_edges.push_back(edge_index);
      }
    } else if (shared_hex_count(game.board, game.board.edges[edge_index]) == 1) {
      coastal_edges.push_back(edge_index);
    }
  }

  std::vector<PortInfo> ports;
  std::set<int> used_edges;
  for (int i = 0; i < static_cast<int>(port_indices.size()) && i < static_cast<int>(seeds.size()); ++i) {
    const Point& ocean_center = ocean[static_cast<std::size_t>(port_indices[static_cast<std::size_t>(i)])];
    int best_edge = -1;
    int best_distance_sq = 1 << 30;
    for (int edge_index : coastal_edges) {
      if (used_edges.count(edge_index) != 0) {
        continue;
      }
      const Edge& edge = game.board.edges[edge_index];
      const Point midpoint{(game.board.nodes[edge.a].x + game.board.nodes[edge.b].x) / 2,
                           (game.board.nodes[edge.a].y + game.board.nodes[edge.b].y) / 2};
      const int dx = midpoint.x - ocean_center.x;
      const int dy = midpoint.y - ocean_center.y;
      const int distance_sq = dx * dx + dy * dy;
      if (distance_sq < best_distance_sq) {
        best_distance_sq = distance_sq;
        best_edge = edge_index;
      }
    }
    if (best_edge >= 0) {
      used_edges.insert(best_edge);
      ports.push_back(PortInfo{ocean_center, best_edge, seeds[static_cast<std::size_t>(i)].rate,
                               seeds[static_cast<std::size_t>(i)].resource, seeds[static_cast<std::size_t>(i)].label});
    }
  }
  return ports;
}

void reset_trade_state(GameState& game) {
  game.trade_mode = TradeMode::None;
  game.trade_partners.clear();
  game.trade_partner = -1;
  game.trade_give = -1;
  game.trade_want = -1;
  game.trade_rate = 4;
  game.pending_trade = {};
  game.prompt_choice = 0;
}

std::array<int, 5> player_trade_rates(const GameState& game, int player_index) {
  std::array<int, 5> rates{};
  rates.fill(4);
  for (const PortInfo& port : build_ports(game)) {
    if (port.edge_index < 0) {
      continue;
    }
    const Edge& edge = game.board.edges[port.edge_index];
    if (game.board.nodes[edge.a].owner != player_index && game.board.nodes[edge.b].owner != player_index) {
      continue;
    }
    if (port.resource >= 0) {
      rates[port.resource] = std::min(rates[port.resource], port.rate);
    } else {
      for (int resource = 0; resource < 5; ++resource) {
        rates[resource] = std::min(rates[resource], port.rate);
      }
    }
  }
  return rates;
}

bool has_port_access(const GameState& game, int player_index) {
  const auto rates = player_trade_rates(game, player_index);
  return std::any_of(rates.begin(), rates.end(), [](int rate) { return rate < 4; });
}

int total_resource_cards(const Player& p);
void apply_player_trade(GameState& game, int from, int to, int give_resource, int want_resource);

std::vector<int> available_trade_partners(const GameState& game, int player_index) {
  std::vector<int> partners;
  for (int partner = 0; partner < game.total_players; ++partner) {
    if (partner == player_index) {
      continue;
    }
    if (total_resource_cards(game.players[partner]) > 0) {
      partners.push_back(partner);
    }
  }
  return partners;
}

int trade_rate_for(const GameState& game, int player_index, TradeMode mode, int resource) {
  if (mode == TradeMode::Bank) {
    return 4;
  }
  if (mode == TradeMode::Port) {
    return player_trade_rates(game, player_index)[resource];
  }
  return 1;
}

std::string describe_trade_offer(const GameState& game, const TradeProposal& trade) {
  return game.players[trade.from].name + " OFFERS " + kResourceNames[trade.give] + " FOR " + kResourceNames[trade.want];
}

void resolve_trade_prompt(GameState& game, bool accepted) {
  if (accepted) {
    apply_player_trade(game, game.pending_trade.from, game.pending_trade.to, game.pending_trade.give, game.pending_trade.want);
    game.status = game.players[game.pending_trade.from].name + " TRADED " +
                  kResourceNames[game.pending_trade.give] + " FOR " + kResourceNames[game.pending_trade.want];
  } else {
    game.status = game.players[game.pending_trade.to].name + " REJECTED " + game.players[game.pending_trade.from].name + " OFFER";
  }
  game.screen = Screen::Action;
  game.pending_trade = {};
  game.prompt_choice = 0;
  reset_trade_state(game);
}

int find_or_create_node(BoardState& board, const Point& point) {
  constexpr int kNodeMergeTolerance = 6;
  for (int index = 0; index < static_cast<int>(board.nodes.size()); ++index) {
    const int dx = board.nodes[index].x - point.x;
    const int dy = board.nodes[index].y - point.y;
    if (dx * dx + dy * dy <= kNodeMergeTolerance * kNodeMergeTolerance) {
      board.nodes[index].x = (board.nodes[index].x + point.x) / 2;
      board.nodes[index].y = (board.nodes[index].y + point.y) / 2;
      return index;
    }
  }

  Node node;
  node.x = point.x;
  node.y = point.y;
  board.nodes.push_back(node);
  return static_cast<int>(board.nodes.size()) - 1;
}

Point axial_to_point(int q, int r, int expanded) {
  const int center_x = board_center_x(expanded);
  const int center_y = expanded ? 220 : 220;
  const int step_x = board_step_x(expanded);
  const int step_y = board_step_y(expanded);
  return {center_x + q * step_x + (r * step_x) / 2, center_y + r * step_y};
}

struct HexSeed {
  int q = 0;
  int r = 0;
  Resource resource = Resource::Sea;
  int token = 0;
};

void add_seed_hex(BoardState& board, std::map<std::pair<int, int>, int>& edge_map, const HexSeed& seed) {
  const int radius = board_radius(board.expanded);
  const Point center = axial_to_point(seed.q, seed.r, board.expanded);
  const int hex_index = static_cast<int>(board.hexes.size());
  Hex hex;
  hex.q = seed.q;
  hex.r = seed.r;
  hex.cx = center.x;
  hex.cy = center.y;
  hex.resource = seed.resource;
  if (seed.resource == Resource::Desert) {
    hex.robber = 1;
  }
  if (seed.resource == Resource::Sea) {
    hex.token = 0;
  } else {
    hex.token = seed.token;
  }
  const auto pts = hex_points(hex.cx, hex.cy, radius);
  for (int v = 0; v < 6; ++v) {
    hex.nodes[v] = find_or_create_node(board, pts[v]);
    board.nodes[hex.nodes[v]].hexes.push_back(hex_index);
  }
  board.hexes.push_back(hex);
  for (int v = 0; v < 6; ++v) {
    const int a = board.hexes[hex_index].nodes[v];
    const int b = board.hexes[hex_index].nodes[(v + 1) % 6];
    const auto key = std::make_pair(std::min(a, b), std::max(a, b));
    int edge_index = -1;
    auto it = edge_map.find(key);
    if (it == edge_map.end()) {
      Edge edge;
      edge.a = a;
      edge.b = b;
      board.edges.push_back(edge);
      edge_index = static_cast<int>(board.edges.size()) - 1;
      edge_map[key] = edge_index;
      board.nodes[a].edges.push_back(edge_index);
      board.nodes[b].edges.push_back(edge_index);
      board.nodes[a].neighbors.push_back(b);
      board.nodes[b].neighbors.push_back(a);
    } else {
      edge_index = it->second;
    }
    Edge& edge = board.edges[edge_index];
    if (edge.hex_count < 2) {
      edge.hexes[static_cast<std::size_t>(edge.hex_count)] = hex_index;
      ++edge.hex_count;
    }
  }
}

void assign_island_ids(BoardState& board) {
  constexpr std::array<std::pair<int, int>, 6> kDirections{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, -1}, {-1, 1}}};
  std::map<std::pair<int, int>, int> index_by_pos;
  for (int i = 0; i < static_cast<int>(board.hexes.size()); ++i) {
    board.hexes[i].island = -1;
    index_by_pos[{board.hexes[i].q, board.hexes[i].r}] = i;
  }

  int next_island = 0;
  std::vector<int> pending;
  for (int i = 0; i < static_cast<int>(board.hexes.size()); ++i) {
    Hex& origin = board.hexes[i];
    if (origin.resource == Resource::Sea || origin.island >= 0) {
      continue;
    }
    origin.island = next_island;
    pending.clear();
    pending.push_back(i);
    while (!pending.empty()) {
      const int current = pending.back();
      pending.pop_back();
      const int q = board.hexes[current].q;
      const int r = board.hexes[current].r;
      for (const auto& dir : kDirections) {
        const auto it = index_by_pos.find({q + dir.first, r + dir.second});
        if (it == index_by_pos.end()) {
          continue;
        }
        Hex& neighbor = board.hexes[it->second];
        if (neighbor.resource == Resource::Sea || neighbor.island >= 0) {
          continue;
        }
        neighbor.island = next_island;
        pending.push_back(it->second);
      }
    }
    ++next_island;
  }
}

BoardState build_classic_board(std::mt19937& rng, int total_players) {
  BoardState board;
  board.expanded = total_players > 4 ? 1 : 0;
  const int radius = board_radius(board.expanded);
  const int step_x = board_step_x(board.expanded);
  const int center_x = board_center_x(board.expanded);
  std::vector<Resource> resources;
  if (board.expanded) {
    resources.insert(resources.end(), 6, Resource::Wood);
    resources.insert(resources.end(), 5, Resource::Brick);
    resources.insert(resources.end(), 6, Resource::Sheep);
    resources.insert(resources.end(), 6, Resource::Wheat);
    resources.insert(resources.end(), 5, Resource::Ore);
    resources.insert(resources.end(), 2, Resource::Desert);
  } else {
    resources.insert(resources.end(), 4, Resource::Wood);
    resources.insert(resources.end(), 3, Resource::Brick);
    resources.insert(resources.end(), 4, Resource::Sheep);
    resources.insert(resources.end(), 4, Resource::Wheat);
    resources.insert(resources.end(), 3, Resource::Ore);
    resources.push_back(Resource::Desert);
  }
  std::shuffle(resources.begin(), resources.end(), rng);
  std::vector<int> tokens{2, 3, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 10, 11, 11, 12};
  if (board.expanded) {
    tokens.insert(tokens.end(), {2, 3, 4, 5, 6, 8, 9, 10, 11, 12});
  }
  std::shuffle(tokens.begin(), tokens.end(), rng);
  std::map<std::pair<int, int>, int> edge_map;
  int res_i = 0;
  int tok_i = 0;
  for (const auto& row : layout_rows(board.expanded)) {
    const int count = row.first;
    const int y = row.second;
    const int width = (count - 1) * step_x;
    int start_x = center_x - width / 2;
    for (int i = 0; i < count; ++i) {
      Hex hex;
      hex.cx = start_x + i * step_x;
      hex.cy = y;
      hex.resource = resources[res_i++];
      if (hex.resource == Resource::Desert) {
        hex.robber = 1;
      } else {
        hex.token = tokens[tok_i++];
      }
      const auto pts = hex_points(hex.cx, hex.cy, radius);
      for (int v = 0; v < 6; ++v) {
        hex.nodes[v] = find_or_create_node(board, pts[v]);
        board.nodes[hex.nodes[v]].hexes.push_back(static_cast<int>(board.hexes.size()));
      }
      for (int v = 0; v < 6; ++v) {
        const int a = hex.nodes[v];
        const int b = hex.nodes[(v + 1) % 6];
        const auto key = std::make_pair(std::min(a, b), std::max(a, b));
        if (edge_map.find(key) == edge_map.end()) {
          Edge edge;
          edge.a = a;
          edge.b = b;
          board.edges.push_back(edge);
          const int edge_index = static_cast<int>(board.edges.size()) - 1;
          edge_map[key] = edge_index;
          board.nodes[a].edges.push_back(edge_index);
          board.nodes[b].edges.push_back(edge_index);
          board.nodes[a].neighbors.push_back(b);
          board.nodes[b].neighbors.push_back(a);
          board.edges[edge_index].hexes[0] = static_cast<int>(board.hexes.size());
          board.edges[edge_index].hex_count = 1;
        } else {
          const int edge_index = edge_map[key];
          Edge& edge = board.edges[edge_index];
          edge.hexes[static_cast<std::size_t>(edge.hex_count)] = static_cast<int>(board.hexes.size());
          ++edge.hex_count;
        }
      }
      board.hexes.push_back(hex);
    }
  }
  return board;
}

BoardState build_seafarers_board() {
  BoardState board;
  board.expanded = 1;
  board.seafarers = 1;
  std::map<std::pair<int, int>, int> edge_map;
  const std::vector<HexSeed> seeds{
      {-4, 0, Resource::Wood, 5},   {-3, 0, Resource::Sheep, 6}, {-4, 1, Resource::Brick, 4}, {-3, 1, Resource::Wheat, 9},
      {-2, 1, Resource::Ore, 10},   {-3, 2, Resource::Wheat, 8},
      {-5, -1, Resource::Sea, 0},   {-4, -1, Resource::Sea, 0},  {-3, -1, Resource::Sea, 0},  {-2, -1, Resource::Sea, 0},
      {-5, 0, Resource::Sea, 0},    {-2, 0, Resource::Sea, 0},   {-5, 1, Resource::Sea, 0},   {-1, 0, Resource::Sea, 0},
      {-5, 2, Resource::Sea, 0},    {-4, 2, Resource::Sea, 0},   {-2, 2, Resource::Sea, 0},   {-4, 3, Resource::Sea, 0},
      {-3, 3, Resource::Sea, 0},    {-2, 3, Resource::Sea, 0},   {-1, 1, Resource::Sea, 0},   {-1, 2, Resource::Sea, 0},
      {0, -1, Resource::Sea, 0},    {0, 0, Resource::Sea, 0},    {0, 1, Resource::Sea, 0},    {1, -1, Resource::Sea, 0},
      {1, 0, Resource::Sea, 0},     {1, 1, Resource::Sea, 0},    {2, -1, Resource::Sea, 0},   {2, 0, Resource::Sea, 0},
      {2, 1, Resource::Sea, 0},     {3, 1, Resource::Sea, 0},    {0, -2, Resource::Sea, 0},   {1, -2, Resource::Sea, 0},
      {2, -2, Resource::Sea, 0},    {3, -2, Resource::Sea, 0},   {3, -1, Resource::Sea, 0},   {4, -2, Resource::Sea, 0},
      {4, -1, Resource::Sea, 0},    {4, 0, Resource::Sea, 0},    {3, 0, Resource::Sea, 0},    {0, 2, Resource::Sea, 0},
      {1, 2, Resource::Sea, 0},     {2, 2, Resource::Sea, 0},    {3, 2, Resource::Sea, 0},    {4, 1, Resource::Sea, 0},
      {4, 2, Resource::Sea, 0},     {2, 3, Resource::Sea, 0},
      {1, -3, Resource::Gold, 4},   {2, -3, Resource::Sheep, 11},
      {2, 4, Resource::Wood, 3},    {3, 4, Resource::Desert, 0},
      {4, 3, Resource::Brick, 10},  {3, 3, Resource::Ore, 8},
      {4, -3, Resource::Gold, 5},   {5, -3, Resource::Brick, 9}, {5, -2, Resource::Sheep, 3},
  };
  for (const HexSeed& seed : seeds) {
    add_seed_hex(board, edge_map, seed);
  }
  assign_island_ids(board);
  int island_count = -1;
  for (const Hex& hex : board.hexes) {
    island_count = std::max(island_count, hex.island);
  }
  board.island_claimed_by.assign(static_cast<std::size_t>(std::max(0, island_count + 1)), -1);
  return board;
}

BoardState build_board(std::mt19937& rng, int total_players, Variant variant) {
  if (variant == Variant::Seafarers) {
    return build_seafarers_board();
  }
  return build_classic_board(rng, total_players);
}

bool seafarers_active(const GameState& game) {
  return game.variant == Variant::Seafarers;
}

int edge_land_count(const BoardState& board, const Edge& edge) {
  int count = 0;
  for (int i = 0; i < edge.hex_count; ++i) {
    const Hex& hex = board.hexes[edge.hexes[static_cast<std::size_t>(i)]];
    if (hex.resource != Resource::Sea) {
      ++count;
    }
  }
  return count;
}

int edge_sea_count(const BoardState& board, const Edge& edge) {
  int count = 0;
  for (int i = 0; i < edge.hex_count; ++i) {
    const Hex& hex = board.hexes[edge.hexes[static_cast<std::size_t>(i)]];
    if (hex.resource == Resource::Sea) {
      ++count;
    }
  }
  return count;
}

bool edge_can_host_road(const BoardState& board, const Edge& edge) {
  return edge_land_count(board, edge) > 0;
}

bool edge_can_host_ship(const BoardState& board, const Edge& edge) {
  return edge_sea_count(board, edge) > 0;
}

bool edge_blocked_by_pirate(const GameState& game, int edge_index) {
  const Edge& edge = game.board.edges[edge_index];
  for (int i = 0; i < edge.hex_count; ++i) {
    if (game.board.hexes[edge.hexes[static_cast<std::size_t>(i)]].pirate) {
      return true;
    }
  }
  return false;
}

bool node_coastal(const BoardState& board, int node_index) {
  bool has_land = false;
  bool has_sea = false;
  for (int hex_index : board.nodes[node_index].hexes) {
    const Resource resource = board.hexes[hex_index].resource;
    has_land = has_land || resource != Resource::Sea;
    has_sea = has_sea || resource == Resource::Sea;
  }
  return has_land && has_sea;
}

bool node_on_foreign_island(const BoardState& board, int node_index) {
  for (int hex_index : board.nodes[node_index].hexes) {
    const Hex& hex = board.hexes[hex_index];
    if (hex.resource != Resource::Sea && hex.island > 0) {
      return true;
    }
  }
  return false;
}

bool player_has_settlement_on_island(const GameState& game, int player_index, int island_id) {
  for (const Node& node : game.board.nodes) {
    if (node.owner != player_index) {
      continue;
    }
    for (int hex_index : node.hexes) {
      const Hex& hex = game.board.hexes[hex_index];
      if (hex.island == island_id) {
        return true;
      }
    }
  }
  return false;
}

bool can_afford_road(const Player& p) { return p.resources[0] >= 1 && p.resources[1] >= 1; }
bool can_afford_ship(const Player& p) { return p.resources[0] >= 1 && p.resources[2] >= 1; }
bool can_afford_settlement(const Player& p) {
  return p.resources[0] >= 1 && p.resources[1] >= 1 && p.resources[2] >= 1 && p.resources[3] >= 1;
}
bool can_afford_city(const Player& p) { return p.resources[3] >= 2 && p.resources[4] >= 3; }
bool can_afford_dev_card(const Player& p) { return p.resources[2] >= 1 && p.resources[3] >= 1 && p.resources[4] >= 1; }
void spend_road(Player& p) { --p.resources[0]; --p.resources[1]; }
void spend_ship(Player& p) { --p.resources[0]; --p.resources[2]; }
void spend_settlement(Player& p) { --p.resources[0]; --p.resources[1]; --p.resources[2]; --p.resources[3]; }
void spend_city(Player& p) { p.resources[3] -= 2; p.resources[4] -= 3; }
void spend_dev_card(Player& p) { --p.resources[2]; --p.resources[3]; --p.resources[4]; }
bool player_human(const GameState& game, int player_index);
bool ship_movable(const GameState& game, int edge_index);

int total_resource_cards(const Player& p) {
  return std::accumulate(p.resources.begin(), p.resources.end(), 0);
}

std::vector<DevCard> build_dev_deck(std::mt19937& rng, int expanded) {
  std::vector<DevCard> deck;
  deck.insert(deck.end(), expanded ? 18 : 14, DevCard::Knight);
  deck.insert(deck.end(), expanded ? 6 : 5, DevCard::VictoryPoint);
  deck.insert(deck.end(), expanded ? 3 : 2, DevCard::RoadBuilding);
  deck.insert(deck.end(), expanded ? 3 : 2, DevCard::YearOfPlenty);
  deck.insert(deck.end(), expanded ? 3 : 2, DevCard::Monopoly);
  std::shuffle(deck.begin(), deck.end(), rng);
  return deck;
}

int node_score(const BoardState& board, int node_index) {
  int score = 0;
  for (int hex_index : board.nodes[node_index].hexes) {
    score += pip_value(board.hexes[hex_index].token);
  }
  return score;
}

bool node_open(const BoardState& board, int node_index) {
  if (board.nodes[node_index].owner >= 0) {
    return false;
  }
  for (int neighbor : board.nodes[node_index].neighbors) {
    if (board.nodes[neighbor].owner >= 0) {
      return false;
    }
  }
  return true;
}

bool node_connected(const BoardState& board, int node_index, int player_index) {
  for (int edge_index : board.nodes[node_index].edges) {
    if (board.edges[edge_index].owner == player_index) {
      return true;
    }
  }
  return false;
}

bool node_has_route(const BoardState& board, int node_index, int player_index, RouteKind route) {
  for (int edge_index : board.nodes[node_index].edges) {
    const Edge& edge = board.edges[edge_index];
    if (edge.owner != player_index) {
      continue;
    }
    if (route == RouteKind::None || edge.route == route) {
      return true;
    }
  }
  return false;
}

bool valid_settlement_node(const GameState& game, int player_index, int node_index, bool setup_mode) {
  if (!node_open(game.board, node_index)) {
    return false;
  }
  if (setup_mode) {
    if (!seafarers_active(game)) {
      return true;
    }
    for (int hex_index : game.board.nodes[node_index].hexes) {
      const Hex& hex = game.board.hexes[hex_index];
      if (hex.resource != Resource::Sea && hex.island == 0) {
        return true;
      }
    }
    return false;
  }
  return node_connected(game.board, node_index, player_index);
}

bool valid_city_node(const GameState& game, int player_index, int node_index) {
  const Node& node = game.board.nodes[node_index];
  return node.owner == player_index && node.city == 0;
}

bool valid_road_edge(const GameState& game, int player_index, int edge_index, bool setup_mode) {
  const Edge& edge = game.board.edges[edge_index];
  if (edge.route != RouteKind::None || !edge_can_host_road(game.board, edge)) {
    return false;
  }
  if (setup_mode) {
    return edge.a == game.last_setup_node || edge.b == game.last_setup_node;
  }
  if (game.board.nodes[edge.a].owner == player_index || game.board.nodes[edge.b].owner == player_index) {
    return true;
  }
  for (int node_index : {edge.a, edge.b}) {
    for (int other_edge : game.board.nodes[node_index].edges) {
      const Edge& adjacent = game.board.edges[other_edge];
      if (adjacent.owner != player_index) {
        continue;
      }
      if (adjacent.route == RouteKind::Road) {
        return true;
      }
      if (seafarers_active(game) && adjacent.route == RouteKind::Ship && game.board.nodes[node_index].owner == player_index) {
        return true;
      }
    }
  }
  return false;
}

bool valid_ship_edge(const GameState& game, int player_index, int edge_index, bool setup_mode) {
  if (!seafarers_active(game)) {
    return false;
  }
  const Edge& edge = game.board.edges[edge_index];
  if (edge.route != RouteKind::None || !edge_can_host_ship(game.board, edge) || edge_blocked_by_pirate(game, edge_index)) {
    return false;
  }
  if (setup_mode) {
    if (edge.a != game.last_setup_node && edge.b != game.last_setup_node) {
      return false;
    }
    return node_coastal(game.board, game.last_setup_node);
  }
  for (int node_index : {edge.a, edge.b}) {
    if (game.board.nodes[node_index].owner == player_index && node_coastal(game.board, node_index)) {
      return true;
    }
    for (int other_edge : game.board.nodes[node_index].edges) {
      const Edge& adjacent = game.board.edges[other_edge];
      if (adjacent.owner == player_index && adjacent.route == RouteKind::Ship) {
        return true;
      }
    }
  }
  return false;
}

void assign_targets(GameState& game, TargetKind kind) {
  game.target_kind = kind;
  game.valid_targets.clear();
  game.target_index = 0;
  if (kind == TargetKind::Node) {
    const bool setup_mode = game.screen == Screen::SetupSettlement;
    const bool city_mode = game.screen == Screen::PlaceCity;
    for (int i = 0; i < static_cast<int>(game.board.nodes.size()); ++i) {
      if (city_mode ? valid_city_node(game, game.current_player, i)
                    : valid_settlement_node(game, game.current_player, i, setup_mode)) {
        game.valid_targets.push_back(i);
      }
    }
  } else if (kind == TargetKind::Edge) {
    const bool setup_mode = game.screen == Screen::SetupRoute;
    const bool selecting_ship_to_move = game.screen == Screen::MoveShip && game.selected_ship_edge < 0;
    const bool want_ship = game.screen == Screen::PlaceShip ||
                           (game.screen == Screen::SetupRoute && game.route_mode == RouteKind::Ship) ||
                           (game.screen == Screen::MoveShip && game.selected_ship_edge >= 0);
    for (int i = 0; i < static_cast<int>(game.board.edges.size()); ++i) {
      if (selecting_ship_to_move) {
        if (ship_movable(game, i)) {
          game.valid_targets.push_back(i);
        }
      } else if (want_ship ? valid_ship_edge(game, game.current_player, i, setup_mode)
                           : valid_road_edge(game, game.current_player, i, setup_mode)) {
        game.valid_targets.push_back(i);
      }
    }
  } else if (kind == TargetKind::Hex) {
    for (int i = 0; i < static_cast<int>(game.board.hexes.size()); ++i) {
      const bool want_pirate = game.screen == Screen::Pirate;
      if (want_pirate) {
        if (game.board.hexes[i].resource == Resource::Sea && !game.board.hexes[i].pirate) {
          game.valid_targets.push_back(i);
        }
      } else if (game.board.hexes[i].resource != Resource::Sea && !game.board.hexes[i].robber) {
        game.valid_targets.push_back(i);
      }
    }
  }
}

int longest_road_from(const BoardState& board, int player_index, int node_index, int from_edge, std::set<int>& used) {
  if (board.nodes[node_index].owner >= 0 && board.nodes[node_index].owner != player_index) {
    return 0;
  }
  int best = 0;
  for (int edge_index : board.nodes[node_index].edges) {
    if (edge_index == from_edge || used.count(edge_index) || board.edges[edge_index].owner != player_index) {
      continue;
    }
    if (from_edge >= 0 && board.edges[from_edge].route != board.edges[edge_index].route && board.nodes[node_index].owner != player_index) {
      continue;
    }
    used.insert(edge_index);
    const int next = board.edges[edge_index].a == node_index ? board.edges[edge_index].b : board.edges[edge_index].a;
    best = std::max(best, 1 + longest_road_from(board, player_index, next, edge_index, used));
    used.erase(edge_index);
  }
  return best;
}

int compute_longest_road(const BoardState& board, int player_index) {
  std::set<int> used;
  int best = 0;
  for (int i = 0; i < static_cast<int>(board.nodes.size()); ++i) {
    best = std::max(best, longest_road_from(board, player_index, i, -1, used));
  }
  return best;
}

void update_victory(GameState& game) {
  const int target_points = seafarers_active(game) ? 12 : kVictoryPointsToWin;
  int road_holder = -1;
  int road_best = 4;
  int army_holder = -1;
  int army_best = 2;
  for (int p = 0; p < static_cast<int>(game.players.size()); ++p) {
    int points = 0;
    for (const Node& node : game.board.nodes) {
      if (node.owner == p) {
        points += node.city ? 2 : 1;
      }
    }
    points += game.players[p].special_points;
    game.players[p].longest_road = compute_longest_road(game.board, p);
    game.players[p].has_longest_road = 0;
    game.players[p].has_largest_army = 0;
    if (game.players[p].longest_road > road_best) {
      road_best = game.players[p].longest_road;
      road_holder = p;
    }
    if (game.players[p].used_knights > army_best) {
      army_best = game.players[p].used_knights;
      army_holder = p;
    }
    points += game.players[p].dev_cards[static_cast<int>(DevCard::VictoryPoint)] +
              game.players[p].new_dev_cards[static_cast<int>(DevCard::VictoryPoint)];
    game.players[p].victory_points = points;
  }
  if (road_holder >= 0) {
    game.players[road_holder].has_longest_road = 1;
    game.players[road_holder].victory_points += 2;
  }
  if (army_holder >= 0) {
    game.players[army_holder].has_largest_army = 1;
    game.players[army_holder].victory_points += 2;
  }
  for (int p = 0; p < static_cast<int>(game.players.size()); ++p) {
    if (game.players[p].victory_points >= target_points) {
      game.winner = p;
      game.screen = Screen::GameOver;
    }
  }
}

void place_settlement(GameState& game, int node_index, bool setup_mode) {
  std::vector<int> bonus_islands;
  if (seafarers_active(game)) {
    for (int hex_index : game.board.nodes[node_index].hexes) {
      const Hex& hex = game.board.hexes[hex_index];
      if (hex.resource == Resource::Sea || hex.island <= 0) {
        continue;
      }
      if (hex.island < static_cast<int>(game.board.island_claimed_by.size()) &&
          game.board.island_claimed_by[static_cast<std::size_t>(hex.island)] < 0 &&
          std::find(bonus_islands.begin(), bonus_islands.end(), hex.island) == bonus_islands.end()) {
        bonus_islands.push_back(hex.island);
      }
    }
  }
  game.board.nodes[node_index].owner = game.current_player;
  game.board.nodes[node_index].city = 0;
  if (!setup_mode) {
    --game.players[game.current_player].settlements_left;
    spend_settlement(game.players[game.current_player]);
  } else if (game.setup_index >= game.total_players) {
    for (int hex_index : game.board.nodes[node_index].hexes) {
      const Hex& hex = game.board.hexes[hex_index];
      if (hex.resource != Resource::Desert && hex.resource != Resource::Sea && hex.resource != Resource::Gold) {
        game.players[game.current_player].resources[static_cast<int>(hex.resource)] += 1;
      }
    }
  }
  for (int island : bonus_islands) {
    if (island >= 0 && island < static_cast<int>(game.board.island_claimed_by.size())) {
      game.board.island_claimed_by[static_cast<std::size_t>(island)] = game.current_player;
    }
  }
  game.players[game.current_player].special_points += static_cast<int>(bonus_islands.size()) * 2;
  game.last_setup_node = node_index;
  update_victory(game);
}

void place_city(GameState& game, int node_index) {
  game.board.nodes[node_index].city = 1;
  ++game.players[game.current_player].settlements_left;
  --game.players[game.current_player].cities_left;
  spend_city(game.players[game.current_player]);
  update_victory(game);
}

void place_road(GameState& game, int edge_index, bool setup_mode) {
  game.board.edges[edge_index].owner = game.current_player;
  game.board.edges[edge_index].route = RouteKind::Road;
  game.board.edges[edge_index].built_turn = game.turn_number;
  if (!setup_mode) {
    --game.players[game.current_player].roads_left;
    spend_road(game.players[game.current_player]);
  }
  update_victory(game);
}

void place_ship(GameState& game, int edge_index, bool setup_mode, bool spend_piece) {
  game.board.edges[edge_index].owner = game.current_player;
  game.board.edges[edge_index].route = RouteKind::Ship;
  game.board.edges[edge_index].built_turn = game.turn_number;
  if (!setup_mode && spend_piece) {
    --game.players[game.current_player].ships_left;
    spend_ship(game.players[game.current_player]);
  }
  update_victory(game);
}

bool ship_open_end(const GameState& game, int edge_index, int node_index) {
  const Edge& edge = game.board.edges[edge_index];
  if (edge.owner != game.current_player || edge.route != RouteKind::Ship) {
    return false;
  }
  if (game.board.nodes[node_index].owner == game.current_player) {
    return false;
  }
  int adjacent = 0;
  for (int other_edge : game.board.nodes[node_index].edges) {
    if (other_edge == edge_index) {
      continue;
    }
    const Edge& other = game.board.edges[other_edge];
    if (other.owner == game.current_player && other.route == RouteKind::Ship) {
      ++adjacent;
    }
  }
  return adjacent == 0;
}

bool ship_movable(const GameState& game, int edge_index) {
  const Edge& edge = game.board.edges[edge_index];
  if (edge.owner != game.current_player || edge.route != RouteKind::Ship || edge.built_turn == game.turn_number) {
    return false;
  }
  if (edge_blocked_by_pirate(game, edge_index)) {
    return false;
  }
  return ship_open_end(game, edge_index, edge.a) || ship_open_end(game, edge_index, edge.b);
}

void discard_half(Player& player) {
  int total = total_resource_cards(player);
  int discard = total / 2;
  while (discard > 0) {
    int best = 0;
    for (int r = 1; r < 5; ++r) {
      if (player.resources[r] > player.resources[best]) {
        best = r;
      }
    }
    if (player.resources[best] <= 0) {
      break;
    }
    --player.resources[best];
    --discard;
  }
}

bool needs_pass_device(const GameState& game) {
  return game.mode == GameMode::Local && game.local_humans > 1 && player_human(game, game.current_player);
}

std::vector<int> robber_victims(const GameState& game, int hex_index) {
  std::vector<int> victims;
  for (int node_index : game.board.hexes[hex_index].nodes) {
    const int owner = game.board.nodes[node_index].owner;
    if (owner < 0 || owner == game.current_player || total_resource_cards(game.players[owner]) <= 0) {
      continue;
    }
    if (std::find(victims.begin(), victims.end(), owner) == victims.end()) {
      victims.push_back(owner);
    }
  }
  return victims;
}

std::vector<int> pirate_victims(const GameState& game, int hex_index) {
  std::vector<int> victims;
  for (const Edge& edge : game.board.edges) {
    if (edge.route != RouteKind::Ship || edge.owner < 0) {
      continue;
    }
    bool adjacent = false;
    for (int i = 0; i < edge.hex_count; ++i) {
      if (edge.hexes[static_cast<std::size_t>(i)] == hex_index) {
        adjacent = true;
        break;
      }
    }
    if (!adjacent || edge.owner == game.current_player || total_resource_cards(game.players[edge.owner]) <= 0) {
      continue;
    }
    if (std::find(victims.begin(), victims.end(), edge.owner) == victims.end()) {
      victims.push_back(edge.owner);
    }
  }
  return victims;
}

void steal_random_resource(GameState& game, int victim_index) {
  Player& victim = game.players[victim_index];
  std::vector<int> pool;
  for (int resource = 0; resource < 5; ++resource) {
    for (int count = 0; count < victim.resources[resource]; ++count) {
      pool.push_back(resource);
    }
  }
  if (pool.empty()) {
    game.status = "ROBBER MOVED";
    return;
  }
  std::uniform_int_distribution<int> pick(0, static_cast<int>(pool.size()) - 1);
  const int resource = pool[static_cast<std::size_t>(pick(game.rng))];
  --victim.resources[resource];
  ++game.players[game.current_player].resources[resource];
  game.status = "STOLE " + std::string(kResourceNames[static_cast<std::size_t>(resource)]) + " FROM " + victim.name;
}

int preferred_gold_resource(const GameState& game, int player_index) {
  const Player& player = game.players[player_index];
  int best = 0;
  int score = 1 << 30;
  for (int resource = 0; resource < 5; ++resource) {
    int value = player.resources[resource] * 3;
    if (resource == static_cast<int>(Resource::Wheat) || resource == static_cast<int>(Resource::Ore)) {
      value -= 1;
    }
    if (value < score) {
      score = value;
      best = resource;
    }
  }
  return best;
}

void process_gold_queue(GameState& game) {
  while (!game.gold_picks.empty()) {
    const int player_index = game.gold_picks.front().first;
    if (player_human(game, player_index)) {
      game.gold_pick_resource = 0;
      game.screen = Screen::GoldChoice;
      game.status = game.players[player_index].name + " PICKS GOLD RESOURCE";
      return;
    }
    const int pick = preferred_gold_resource(game, player_index);
    ++game.players[player_index].resources[pick];
    game.gold_picks.erase(game.gold_picks.begin());
  }
  game.screen = Screen::Action;
  game.status = "BUILD OR END";
}

void open_dev_cards(GameState& game) {
  game.menu_index = 0;
  game.screen = Screen::DevCards;
  game.status = "DEV CARD STASH";
}

void finish_robber_move(GameState& game, int hex_index) {
  for (Hex& hex : game.board.hexes) {
    hex.robber = 0;
  }
  game.board.hexes[hex_index].robber = 1;
  game.robber_targets = robber_victims(game, hex_index);
  game.robber_target_index = 0;
  if (game.robber_targets.empty()) {
    game.screen = Screen::Action;
    game.status = "ROBBER MOVED";
  } else if (game.robber_targets.size() == 1) {
    steal_random_resource(game, game.robber_targets.front());
    game.screen = Screen::Action;
  } else {
    game.screen = Screen::RobberSteal;
    game.status = "CHOOSE A PLAYER TO STEAL";
  }
}

void finish_pirate_move(GameState& game, int hex_index) {
  for (Hex& hex : game.board.hexes) {
    hex.pirate = 0;
  }
  game.board.hexes[hex_index].pirate = 1;
  game.robber_targets = pirate_victims(game, hex_index);
  game.robber_target_index = 0;
  if (game.robber_targets.empty()) {
    game.screen = Screen::Action;
    game.status = "PIRATE MOVED";
  } else if (game.robber_targets.size() == 1) {
    steal_random_resource(game, game.robber_targets.front());
    game.screen = Screen::Action;
  } else {
    game.screen = Screen::PirateSteal;
    game.status = "CHOOSE A SHIP TO RAID";
  }
}

void distribute_resources(GameState& game, int roll) {
  if (roll == 7) {
    for (Player& player : game.players) {
      discard_half(player);
    }
    if (seafarers_active(game)) {
      game.raid_choice = 0;
      game.screen = Screen::RaidChoice;
      game.status = "ROBBER OR PIRATE";
    } else {
      game.status = "MOVE ROBBER";
      game.screen = Screen::Robber;
      assign_targets(game, TargetKind::Hex);
    }
    return;
  }
  game.gold_picks.clear();
  for (const Hex& hex : game.board.hexes) {
    if (hex.robber || hex.token != roll || hex.resource == Resource::Desert || hex.resource == Resource::Sea) {
      continue;
    }
    for (int node_index : hex.nodes) {
      const Node& node = game.board.nodes[node_index];
      if (node.owner >= 0) {
        if (hex.resource == Resource::Gold) {
          for (int grant = 0; grant < (node.city ? 2 : 1); ++grant) {
            game.gold_picks.push_back({node.owner, 1});
          }
        } else {
          game.players[node.owner].resources[static_cast<int>(hex.resource)] += node.city ? 2 : 1;
        }
      }
    }
  }
  process_gold_queue(game);
}

bool player_human(const GameState& game, int player_index) {
  return game.players[player_index].human != 0;
}

void start_turn(GameState& game) {
  game.last_roll = 0;
  game.dice_a = 0;
  game.dice_b = 0;
  game.action_index = 0;
  game.played_dev_this_turn = 0;
  game.free_roads_to_place = 0;
  game.selected_ship_edge = -1;
  for (int card = 0; card < 5; ++card) {
    game.players[game.current_player].dev_cards[card] += game.players[game.current_player].new_dev_cards[card];
    game.players[game.current_player].new_dev_cards[card] = 0;
  }
  game.screen = Screen::Intro;
  game.status = needs_pass_device(game) ? "PASS DEVICE" : "READY TO ROLL";
}

void next_player(GameState& game) {
  ++game.turn_number;
  game.current_player = (game.current_player + 1) % game.total_players;
  start_turn(game);
}

void begin_game(GameState& game) {
  game.total_players = std::min(6, std::max(2, game.local_humans + game.bots));
  game.board = build_board(game.rng, game.total_players, game.variant);
  game.dev_deck = build_dev_deck(game.rng, game.board.expanded);
  game.players.clear();
  for (int i = 0; i < game.total_players; ++i) {
    Player player;
    player.name = i < game.local_humans ? ("P" + std::to_string(i + 1)) : ("CPU " + std::to_string(i + 1 - game.local_humans));
    player.human = i < game.local_humans ? 1 : 0;
    player.color = kPlayerColors[static_cast<std::size_t>(i)];
    game.players.push_back(player);
  }
  game.current_player = 0;
  game.setup_index = 0;
  game.last_setup_node = -1;
  game.winner = -1;
  game.turn_number = 0;
  game.route_mode = RouteKind::Road;
  game.raid_choice = 0;
  game.gold_picks.clear();
  game.last_roll = 0;
  game.dice_a = 0;
  game.dice_b = 0;
  game.board_view_x = 0.0f;
  game.board_view_y = 0.0f;
  game.board_view_ready = 0;
  game.screen = Screen::SetupSettlement;
  game.status = "PLACE STARTING SETTLEMENT";
  assign_targets(game, TargetKind::Node);
}

int best_setup_node(const GameState& game) {
  int best = -1;
  int score = -1;
  for (int i = 0; i < static_cast<int>(game.board.nodes.size()); ++i) {
    if (!valid_settlement_node(game, game.current_player, i, true)) {
      continue;
    }
    const int value = node_score(game.board, i);
    if (value > score) {
      score = value;
      best = i;
    }
  }
  return best;
}

int best_setup_road(const GameState& game) {
  int best = -1;
  int score = -1;
  for (int i = 0; i < static_cast<int>(game.board.edges.size()); ++i) {
    if (!valid_road_edge(game, game.current_player, i, true)) {
      continue;
    }
    const Edge& edge = game.board.edges[i];
    const int next_node = edge.a == game.last_setup_node ? edge.b : edge.a;
    const int value = node_score(game.board, next_node);
    if (value > score) {
      score = value;
      best = i;
    }
  }
  return best;
}

int best_city(const GameState& game) {
  int best = -1;
  int score = -1;
  for (int i = 0; i < static_cast<int>(game.board.nodes.size()); ++i) {
    if (!valid_city_node(game, game.current_player, i)) {
      continue;
    }
    const int value = node_score(game.board, i);
    if (value > score) {
      score = value;
      best = i;
    }
  }
  return best;
}

int best_settlement(const GameState& game) {
  int best = -1;
  int score = -1;
  for (int i = 0; i < static_cast<int>(game.board.nodes.size()); ++i) {
    if (!valid_settlement_node(game, game.current_player, i, false)) {
      continue;
    }
    const int value = node_score(game.board, i);
    if (value > score) {
      score = value;
      best = i;
    }
  }
  return best;
}

int best_road(const GameState& game) {
  int best = -1;
  int score = -1000;
  for (int i = 0; i < static_cast<int>(game.board.edges.size()); ++i) {
    if (!valid_road_edge(game, game.current_player, i, false)) {
      continue;
    }
    const Edge& edge = game.board.edges[i];
    const int value = std::max(node_score(game.board, edge.a), node_score(game.board, edge.b));
    if (value > score) {
      score = value;
      best = i;
    }
  }
  return best;
}

int best_setup_ship(const GameState& game) {
  int best = -1;
  int score = -1000;
  for (int i = 0; i < static_cast<int>(game.board.edges.size()); ++i) {
    if (!valid_ship_edge(game, game.current_player, i, true)) {
      continue;
    }
    const Edge& edge = game.board.edges[i];
    const int next_node = edge.a == game.last_setup_node ? edge.b : edge.a;
    int value = node_score(game.board, next_node) + 6;
    if (node_on_foreign_island(game.board, next_node)) {
      value += 14;
    }
    if (value > score) {
      score = value;
      best = i;
    }
  }
  return best;
}

int best_ship(const GameState& game) {
  int best = -1;
  int score = -1000;
  for (int i = 0; i < static_cast<int>(game.board.edges.size()); ++i) {
    if (!valid_ship_edge(game, game.current_player, i, false)) {
      continue;
    }
    const Edge& edge = game.board.edges[i];
    int value = 8;
    value += std::max(node_score(game.board, edge.a), node_score(game.board, edge.b));
    if (node_on_foreign_island(game.board, edge.a) || node_on_foreign_island(game.board, edge.b)) {
      value += 18;
    }
    if (value > score) {
      score = value;
      best = i;
    }
  }
  return best;
}

double hand_score(const GameState& game, int player_index, const std::array<int, 5>& resources) {
  auto score_cost = [&](const std::array<int, 5>& cost, double value) {
    int missing = 0;
    for (int resource = 0; resource < 5; ++resource) {
      missing += std::max(0, cost[resource] - resources[resource]);
    }
    if (missing == 0) {
      return value + 4.0;
    }
    return value - static_cast<double>(missing) * 1.6;
  };

  double score = 0.1 * static_cast<double>(std::accumulate(resources.begin(), resources.end(), 0));
  if (best_city(game) >= 0) {
    score += score_cost({0, 0, 0, 2, 3}, 9.5);
  }
  if (best_settlement(game) >= 0) {
    score += score_cost({1, 1, 1, 1, 0}, 7.0);
  }
  if (best_road(game) >= 0) {
    score += score_cost({1, 1, 0, 0, 0}, 3.5);
  }
  if (seafarers_active(game) && best_ship(game) >= 0) {
    score += score_cost({1, 0, 1, 0, 0}, 4.2);
  }
  if (!game.dev_deck.empty()) {
    score += score_cost({0, 0, 1, 1, 1}, 3.0);
  }

  const auto rates = player_trade_rates(game, player_index);
  for (int resource = 0; resource < 5; ++resource) {
    if (rates[resource] < 4) {
      score += 0.15 * static_cast<double>(resources[resource]);
    }
  }
  return score;
}

bool ai_accepts_trade(const GameState& game, int player_index, int give_resource, int want_resource) {
  if (player_index < 0 || player_index >= game.total_players || give_resource == want_resource ||
      game.players[player_index].resources[give_resource] <= 0) {
    return false;
  }
  auto before = game.players[player_index].resources;
  auto after = before;
  --after[give_resource];
  ++after[want_resource];
  return hand_score(game, player_index, after) > hand_score(game, player_index, before) + 0.35;
}

void apply_player_trade(GameState& game, int from, int to, int give_resource, int want_resource) {
  --game.players[from].resources[give_resource];
  ++game.players[from].resources[want_resource];
  ++game.players[to].resources[give_resource];
  --game.players[to].resources[want_resource];
}

TradeProposal best_ai_player_trade(const GameState& game, int player_index) {
  TradeProposal best;
  best.score = -9999.0;
  const auto current = game.players[player_index].resources;
  const double baseline = hand_score(game, player_index, current);
  for (int give = 0; give < 5; ++give) {
    if (current[give] <= 0) {
      continue;
    }
    for (int want = 0; want < 5; ++want) {
      if (want == give) {
        continue;
      }
      auto after = current;
      --after[give];
      ++after[want];
      const double gain = hand_score(game, player_index, after) - baseline;
      if (gain < 0.6) {
        continue;
      }
      for (int partner = 0; partner < game.total_players; ++partner) {
        if (partner == player_index || game.players[partner].resources[want] <= 0) {
          continue;
        }
        if (!ai_accepts_trade(game, partner, want, give)) {
          continue;
        }
        const double total = gain + (player_human(game, partner) ? 0.15 : 0.0);
        if (total > best.score) {
          best = {player_index, partner, give, want, 1, total};
        }
      }
    }
  }
  return best;
}

bool try_best_maritime_trade(GameState& game, int player_index) {
  auto resources = game.players[player_index].resources;
  const double baseline = hand_score(game, player_index, resources);
  const auto rates = player_trade_rates(game, player_index);
  int best_give = -1;
  int best_want = -1;
  int best_rate = 4;
  double best_score = baseline;

  for (int give = 0; give < 5; ++give) {
    const int rate = rates[give];
    if (resources[give] < rate) {
      continue;
    }
    for (int want = 0; want < 5; ++want) {
      if (want == give) {
        continue;
      }
      auto after = resources;
      after[give] -= rate;
      ++after[want];
      const double score = hand_score(game, player_index, after);
      if (score > best_score + 0.45) {
        best_score = score;
        best_give = give;
        best_want = want;
        best_rate = rate;
      }
    }
  }

  if (best_give < 0) {
    return false;
  }
  game.players[player_index].resources[best_give] -= best_rate;
  ++game.players[player_index].resources[best_want];
  game.status = (best_rate == 4 ? "BANK" : "PORT") + std::string(" TRADE FOR ") + kResourceNames[best_want];
  return true;
}

void cpu_take_turn(GameState& game) {
  Player& player = game.players[game.current_player];
  if (game.screen == Screen::Intro) {
    game.screen = Screen::Roll;
    game.status = "ROLL DICE";
    return;
  }
  if (game.screen == Screen::SetupSettlement) {
    const int node = best_setup_node(game);
    if (node >= 0) {
      place_settlement(game, node, true);
      game.screen = Screen::SetupRoute;
      game.status = "PLACE STARTING ROUTE";
      assign_targets(game, TargetKind::Edge);
    }
    return;
  }
  if (game.screen == Screen::SetupRoute) {
    int edge = best_setup_road(game);
    bool ship = false;
    if (seafarers_active(game)) {
      const int ship_edge = best_setup_ship(game);
      const int road_score = edge >= 0 ? std::max(node_score(game.board, game.board.edges[edge].a),
                                                   node_score(game.board, game.board.edges[edge].b))
                                       : -1000;
      const int ship_score = ship_edge >= 0 ? std::max(node_score(game.board, game.board.edges[ship_edge].a),
                                                       node_score(game.board, game.board.edges[ship_edge].b)) + 6
                                            : -1000;
      if (ship_score > road_score + 2) {
        edge = ship_edge;
        ship = true;
      }
    }
    if (edge >= 0) {
      if (ship) {
        place_ship(game, edge, true, false);
      } else {
        place_road(game, edge, true);
      }
      ++game.setup_index;
      if (game.setup_index >= game.total_players * 2) {
        game.current_player = 0;
        start_turn(game);
        game.screen = Screen::Roll;
        game.status = "ROLL DICE";
      } else {
        const bool reverse = game.setup_index >= game.total_players;
        game.current_player = reverse ? (game.total_players - 1 - (game.setup_index - game.total_players))
                                      : game.setup_index;
        game.screen = Screen::SetupSettlement;
        game.status = "PLACE STARTING SETTLEMENT";
        assign_targets(game, TargetKind::Node);
      }
    }
    return;
  }
  if (game.screen == Screen::Roll) {
    std::uniform_int_distribution<int> die(1, 6);
    game.dice_a = die(game.rng);
    game.dice_b = die(game.rng);
    game.last_roll = game.dice_a + game.dice_b;
    distribute_resources(game, game.last_roll);
    return;
  }
  if (game.screen == Screen::RaidChoice) {
    game.screen = Screen::Pirate;
    game.status = "MOVE PIRATE";
    assign_targets(game, TargetKind::Hex);
    return;
  }
  if (game.screen == Screen::Robber) {
    int best = 0;
    int score = -999;
    for (int i = 0; i < static_cast<int>(game.board.hexes.size()); ++i) {
      if (game.board.hexes[i].robber || game.board.hexes[i].resource == Resource::Desert) {
        continue;
      }
      int value = pip_value(game.board.hexes[i].token);
      for (int node_index : game.board.hexes[i].nodes) {
        const Node& node = game.board.nodes[node_index];
        if (node.owner >= 0 && node.owner != game.current_player) {
          value += node.city ? 4 : 2;
        }
      }
      if (value > score) {
        score = value;
        best = i;
      }
    }
    finish_robber_move(game, best);
    return;
  }
  if (game.screen == Screen::Pirate) {
    int best = 0;
    int score = -999;
    for (int i = 0; i < static_cast<int>(game.board.hexes.size()); ++i) {
      if (game.board.hexes[i].pirate || game.board.hexes[i].resource != Resource::Sea) {
        continue;
      }
      int value = 0;
      for (const Edge& edge : game.board.edges) {
        if (edge.owner < 0 || edge.owner == game.current_player || edge.route != RouteKind::Ship) {
          continue;
        }
        for (int h = 0; h < edge.hex_count; ++h) {
          if (edge.hexes[static_cast<std::size_t>(h)] == i) {
            value += 3;
          }
        }
      }
      if (value > score) {
        score = value;
        best = i;
      }
    }
    finish_pirate_move(game, best);
    return;
  }
  if (game.screen == Screen::RobberSteal) {
    int best = game.robber_targets.front();
    int score = -1;
    for (int victim : game.robber_targets) {
      const int total = total_resource_cards(game.players[victim]);
      if (total > score) {
        score = total;
        best = victim;
      }
    }
    steal_random_resource(game, best);
    game.screen = Screen::Action;
    return;
  }
  if (game.screen == Screen::PirateSteal) {
    int best = game.robber_targets.front();
    int score = -1;
    for (int victim : game.robber_targets) {
      const int total = total_resource_cards(game.players[victim]);
      if (total > score) {
        score = total;
        best = victim;
      }
    }
    steal_random_resource(game, best);
    game.screen = Screen::Action;
    return;
  }
  if (game.screen == Screen::GoldChoice) {
    process_gold_queue(game);
    return;
  }
  if (game.screen != Screen::Action) {
    return;
  }
  if (can_afford_city(player)) {
    const int target = best_city(game);
    if (target >= 0) {
      place_city(game, target);
      game.status = player.name + " BUILT CITY";
      return;
    }
  }
  if (can_afford_settlement(player)) {
    const int target = best_settlement(game);
    if (target >= 0) {
      place_settlement(game, target, false);
      game.status = player.name + " BUILT SETTLEMENT";
      return;
    }
  }
  if (can_afford_road(player)) {
    const int target = best_road(game);
    if (target >= 0) {
      if (game.free_roads_to_place > 0) {
        game.board.edges[target].owner = game.current_player;
        game.board.edges[target].route = RouteKind::Road;
        game.board.edges[target].built_turn = game.turn_number;
        --game.players[game.current_player].roads_left;
        --game.free_roads_to_place;
        update_victory(game);
      } else {
        place_road(game, target, false);
      }
      game.status = player.name + " BUILT ROAD";
      if (game.free_roads_to_place == 0) {
        game.screen = Screen::Action;
      }
      return;
    }
  }
  if (seafarers_active(game) && (game.free_roads_to_place > 0 || can_afford_ship(player))) {
    const int target = best_ship(game);
    if (target >= 0) {
      if (game.free_roads_to_place > 0) {
        place_ship(game, target, false, false);
        --game.players[game.current_player].ships_left;
        --game.free_roads_to_place;
      } else {
        place_ship(game, target, false, true);
      }
      game.status = player.name + " LAUNCHED SHIP";
      return;
    }
  }
  if (game.free_roads_to_place > 0) {
    const int target = best_road(game);
    if (target >= 0) {
      game.board.edges[target].owner = game.current_player;
      game.board.edges[target].route = RouteKind::Road;
      game.board.edges[target].built_turn = game.turn_number;
      --game.players[game.current_player].roads_left;
      --game.free_roads_to_place;
      update_victory(game);
      game.status = player.name + " BUILT ROAD";
      return;
    }
    game.free_roads_to_place = 0;
  }
  const TradeProposal proposal = best_ai_player_trade(game, game.current_player);
  if (proposal.from >= 0) {
    if (player_human(game, proposal.to)) {
      game.pending_trade = proposal;
      game.prompt_choice = 0;
      game.screen = Screen::TradeOfferPrompt;
      game.status = describe_trade_offer(game, proposal);
      return;
    }
    apply_player_trade(game, proposal.from, proposal.to, proposal.give, proposal.want);
    game.status = player.name + " TRADED FOR " + kResourceNames[proposal.want];
    return;
  }
  if (try_best_maritime_trade(game, game.current_player)) {
    return;
  }
  if (!game.dev_deck.empty() && can_afford_dev_card(player)) {
    spend_dev_card(player);
    const DevCard card = game.dev_deck.back();
    game.dev_deck.pop_back();
    ++player.new_dev_cards[static_cast<int>(card)];
    game.status = player.name + " BOUGHT DEV CARD";
    update_victory(game);
    return;
  }
  next_player(game);
}

void draw_house(SDL_Renderer* renderer, SDL_Color color, int x, int y, int city, int selected) {
  fill_rect(renderer, {x - (city ? 7 : 5), y - 6, city ? 14 : 10, city ? 12 : 10}, color);
  fill_rect(renderer, {x - (city ? 4 : 3), y - (city ? 11 : 9), city ? 8 : 6, 5}, color);
  if (selected) {
    draw_rect(renderer, {x - 10, y - 14, 20, 20}, {255, 224, 129, 255});
  }
}

void draw_die_face(SDL_Renderer* renderer, int x, int y, int value, const Theme& theme) {
  fill_rect(renderer, {x, y, 24, 24}, {236, 239, 245, 255});
  draw_rect(renderer, {x, y, 24, 24}, theme.frame);
  auto pip = [&](int px, int py) { fill_circle(renderer, x + px, y + py, 2, theme.bg); };
  switch (value) {
    case 1: pip(12, 12); break;
    case 2: pip(7, 7); pip(17, 17); break;
    case 3: pip(7, 7); pip(12, 12); pip(17, 17); break;
    case 4: pip(7, 7); pip(17, 7); pip(7, 17); pip(17, 17); break;
    case 5: pip(7, 7); pip(17, 7); pip(12, 12); pip(7, 17); pip(17, 17); break;
    case 6:
      pip(7, 6); pip(17, 6); pip(7, 12); pip(17, 12); pip(7, 18); pip(17, 18);
      break;
    default: break;
  }
}

Point lerp_point(const Point& a, const Point& b, double t) {
  return {static_cast<int>(std::lround(static_cast<double>(a.x) + (static_cast<double>(b.x) - static_cast<double>(a.x)) * t)),
          static_cast<int>(std::lround(static_cast<double>(a.y) + (static_cast<double>(b.y) - static_cast<double>(a.y)) * t))};
}

Point target_point(const GameState& game, TargetKind kind, int target);

Point board_screen_point(const Point& world, const BoardView& view) {
  return {world.x + view.offset_x, world.y + view.offset_y};
}

SDL_Rect board_world_bounds(const GameState& game) {
  bool initialized = false;
  int min_x = 0;
  int min_y = 0;
  int max_x = 0;
  int max_y = 0;

  auto grow = [&](const Point& point, int pad_x, int pad_y) {
    const int left = point.x - pad_x;
    const int right = point.x + pad_x;
    const int top = point.y - pad_y;
    const int bottom = point.y + pad_y;
    if (!initialized) {
      min_x = left;
      max_x = right;
      min_y = top;
      max_y = bottom;
      initialized = true;
      return;
    }
    min_x = std::min(min_x, left);
    max_x = std::max(max_x, right);
    min_y = std::min(min_y, top);
    max_y = std::max(max_y, bottom);
  };

  const int radius = board_radius(game.board.expanded);
  for (const Hex& hex : game.board.hexes) {
    grow({hex.cx, hex.cy}, radius + 10, radius + 10);
  }
  for (const Point& ocean : ocean_centers(game.board)) {
    grow(ocean, radius + 26, radius + 30);
  }
  for (const Node& node : game.board.nodes) {
    grow({node.x, node.y}, 14, 14);
  }

  if (!initialized) {
    return {0, 0, 1, 1};
  }
  return {min_x, min_y, std::max(1, max_x - min_x), std::max(1, max_y - min_y)};
}

Point board_focus_point(const GameState& game) {
  if (!game.valid_targets.empty() &&
      (game.screen == Screen::SetupSettlement || game.screen == Screen::SetupRoute || game.screen == Screen::PlaceSettlement ||
       game.screen == Screen::PlaceRoad || game.screen == Screen::PlaceCity || game.screen == Screen::Robber)) {
    return target_point(game, game.target_kind, game.valid_targets[static_cast<std::size_t>(game.target_index)]);
  }
  return {board_center_x(game.board.expanded), game.board.expanded ? 226 : 220};
}

BoardView compute_board_view(GameState& game) {
  BoardView view;
  view.clip = board_clip_rect();
  const SDL_Rect bounds = board_world_bounds(game);
  const Point focus = board_focus_point(game);

  float offset_x = static_cast<float>(view.clip.x + view.clip.w / 2 - focus.x);
  float offset_y = static_cast<float>(view.clip.y + view.clip.h / 2 - focus.y);

  if (bounds.w <= view.clip.w) {
    offset_x = static_cast<float>(view.clip.x + (view.clip.w - bounds.w) / 2 - bounds.x);
  } else {
    offset_x = static_cast<float>(std::clamp(static_cast<int>(std::lround(offset_x)),
                                             view.clip.x + view.clip.w - (bounds.x + bounds.w), view.clip.x - bounds.x));
  }

  if (bounds.h <= view.clip.h) {
    offset_y = static_cast<float>(view.clip.y + (view.clip.h - bounds.h) / 2 - bounds.y);
  } else {
    offset_y = static_cast<float>(std::clamp(static_cast<int>(std::lround(offset_y)),
                                             view.clip.y + view.clip.h - (bounds.y + bounds.h), view.clip.y - bounds.y));
  }

  if (!game.board_view_ready) {
    game.board_view_x = offset_x;
    game.board_view_y = offset_y;
    game.board_view_ready = 1;
  } else {
    game.board_view_x += (offset_x - game.board_view_x) * 0.22f;
    game.board_view_y += (offset_y - game.board_view_y) * 0.22f;
  }

  view.offset_x = static_cast<int>(std::lround(game.board_view_x));
  view.offset_y = static_cast<int>(std::lround(game.board_view_y));
  return view;
}

void draw_port_connector(SDL_Renderer* renderer, const GameState& game, const PortInfo& port, const BoardView& view, int radius) {
  if (port.edge_index < 0) {
    return;
  }

  const Edge& edge = game.board.edges[port.edge_index];
  const Point a = board_screen_point({game.board.nodes[edge.a].x, game.board.nodes[edge.a].y}, view);
  const Point b = board_screen_point({game.board.nodes[edge.b].x, game.board.nodes[edge.b].y}, view);
  const Point ocean = board_screen_point(port.ocean, view);
  const Point midpoint{(a.x + b.x) / 2, (a.y + b.y) / 2};

  double vx = static_cast<double>(midpoint.x - ocean.x);
  double vy = static_cast<double>(midpoint.y - ocean.y);
  const double length = std::sqrt(vx * vx + vy * vy);
  if (length < 0.001) {
    return;
  }
  vx /= length;
  vy /= length;

  const Point bridge_start{static_cast<int>(std::lround(ocean.x + vx * (radius - 8))),
                           static_cast<int>(std::lround(ocean.y + vy * (radius - 8)))};
  const Point bridge_hub{static_cast<int>(std::lround(midpoint.x - vx * 3.0)),
                         static_cast<int>(std::lround(midpoint.y - vy * 3.0))};
  const Point marker_a = a;
  const Point marker_b = b;

  SDL_SetRenderDrawColor(renderer, 184, 148, 92, 255);
  for (int thickness = -1; thickness <= 1; ++thickness) {
    SDL_RenderDrawLine(renderer, bridge_start.x + thickness, bridge_start.y, bridge_hub.x + thickness, bridge_hub.y);
    SDL_RenderDrawLine(renderer, bridge_hub.x, bridge_hub.y + thickness, marker_a.x, marker_a.y + thickness);
    SDL_RenderDrawLine(renderer, bridge_hub.x, bridge_hub.y + thickness, marker_b.x, marker_b.y + thickness);
  }

  const Point span_start = lerp_point(bridge_start, bridge_hub, 0.18);
  const Point span_mid = lerp_point(bridge_start, bridge_hub, 0.5);
  const Point span_end = lerp_point(bridge_start, bridge_hub, 0.82);
  const int cross_dx = static_cast<int>(std::lround(-vy * 4.0));
  const int cross_dy = static_cast<int>(std::lround(vx * 4.0));
  SDL_SetRenderDrawColor(renderer, 224, 202, 154, 255);
  for (const Point& span : {span_start, span_mid, span_end}) {
    SDL_RenderDrawLine(renderer, span.x - cross_dx, span.y - cross_dy, span.x + cross_dx, span.y + cross_dy);
  }

  fill_circle(renderer, bridge_hub.x, bridge_hub.y, 3, {221, 202, 165, 255});
  fill_circle(renderer, marker_a.x, marker_a.y, 5, {238, 225, 198, 255});
  fill_circle(renderer, marker_b.x, marker_b.y, 5, {238, 225, 198, 255});
  draw_rect(renderer, {marker_a.x - 5, marker_a.y - 5, 10, 10}, {104, 85, 56, 255});
  draw_rect(renderer, {marker_b.x - 5, marker_b.y - 5, 10, 10}, {104, 85, 56, 255});
  fill_rect(renderer, {marker_a.x - 2, marker_a.y - 1, 4, 3}, {118, 171, 205, 255});
  fill_rect(renderer, {marker_b.x - 2, marker_b.y - 1, 4, 3}, {118, 171, 205, 255});
}

void draw_ocean_tiles(SDL_Renderer* renderer, const GameState& game, const BoardView& view) {
  const int radius = board_radius(game.board.expanded);
  const auto ocean = ocean_centers(game.board);
  const auto ports = build_ports(game);

  if (!game.board.seafarers) {
    for (int i = 0; i < static_cast<int>(ocean.size()); ++i) {
      const Point screen_ocean = board_screen_point(ocean[static_cast<std::size_t>(i)], view);
      const auto pts = hex_points(screen_ocean.x, screen_ocean.y, radius);
      fill_polygon(renderer, pts, {56, 96, 149, 255});
      draw_hex_outline(renderer, pts, {34, 60, 98, 255});
    }
  }

  for (const PortInfo& port : ports) {
    draw_port_connector(renderer, game, port, view, radius);

    const Point screen_ocean = board_screen_point(port.ocean, view);
    const int label_width = std::max(36, text_width(port.label, 1) + 8);
    const int label_x = std::clamp(screen_ocean.x - label_width / 2, view.clip.x + 4, view.clip.x + view.clip.w - label_width - 4);
    const int label_y = std::clamp(screen_ocean.y - 8, view.clip.y + 4, view.clip.y + view.clip.h - 24);
    const int label_cx = label_x + label_width / 2;
    fill_rect(renderer, {label_x, label_y, label_width, 16}, {229, 220, 193, 255});
    draw_rect(renderer, {label_x, label_y, label_width, 16}, {95, 81, 56, 255});
    draw_text(renderer, port.label, label_cx, label_y + 4, 1, {70, 55, 31, 255}, true);
    fill_rect(renderer, {label_cx - 6, label_y + 19, 12, 3}, {182, 149, 92, 255});
    fill_rect(renderer, {label_cx - 12, label_y + 22, 24, 3}, {182, 149, 92, 255});
  }
}

void draw_board(SDL_Renderer* renderer, const GameState& game, const BoardView& view) {
  const int radius = board_radius(game.board.expanded);
  draw_ocean_tiles(renderer, game, view);
  for (int i = 0; i < static_cast<int>(game.board.hexes.size()); ++i) {
    const Hex& hex = game.board.hexes[i];
    const Point center = board_screen_point({hex.cx, hex.cy}, view);
    const auto pts = hex_points(center.x, center.y, radius);
    fill_polygon(renderer, pts, resource_color(hex.resource));
    draw_hex_outline(renderer, pts, {26, 32, 46, 255});
    if (hex.resource == Resource::Sea) {
      fill_circle(renderer, center.x, center.y, 8, {84, 156, 220, 255});
      fill_rect(renderer, {center.x - 10, center.y + 6, 20, 2}, {117, 190, 240, 255});
    } else if (hex.resource == Resource::Desert) {
      draw_text(renderer, "D", center.x, center.y - 4, 2, {108, 86, 49, 255}, true);
    } else if (hex.resource == Resource::Gold) {
      fill_circle(renderer, center.x, center.y, 12, {244, 226, 168, 255});
      draw_text(renderer, std::to_string(hex.token), center.x, center.y - 4, 2,
                (hex.token == 6 || hex.token == 8) ? SDL_Color{180, 44, 44, 255} : SDL_Color{34, 36, 40, 255}, true);
      draw_text(renderer, "G", center.x + 15, center.y - 18, 1, {255, 214, 102, 255}, true);
    } else {
      fill_circle(renderer, center.x, center.y, 12, {238, 232, 216, 255});
      draw_text(renderer, std::to_string(hex.token), center.x, center.y - 4, 2,
                (hex.token == 6 || hex.token == 8) ? SDL_Color{180, 44, 44, 255} : SDL_Color{34, 36, 40, 255}, true);
    }
    if (hex.robber) {
      fill_circle(renderer, center.x + 14, center.y + 14, 7, {24, 24, 30, 255});
    }
    if (hex.pirate) {
      fill_circle(renderer, center.x - 14, center.y + 14, 7, {10, 10, 14, 255});
      draw_text(renderer, "P", center.x - 14, center.y + 10, 1, {222, 232, 245, 255}, true);
    }
  }
  for (int edge_index = 0; edge_index < static_cast<int>(game.board.edges.size()); ++edge_index) {
    const Edge& edge = game.board.edges[edge_index];
    const Point a = board_screen_point({game.board.nodes[edge.a].x, game.board.nodes[edge.a].y}, view);
    const Point b = board_screen_point({game.board.nodes[edge.b].x, game.board.nodes[edge.b].y}, view);
    SDL_Color color = edge.owner >= 0 ? game.players[edge.owner].color : SDL_Color{66, 72, 92, 255};
    if (edge.route == RouteKind::Ship) {
      color = {static_cast<Uint8>(std::min(255, color.r + 18)), static_cast<Uint8>(std::min(255, color.g + 18)),
               static_cast<Uint8>(std::min(255, color.b + 18)), 255};
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    const int width = edge.route == RouteKind::Ship ? 1 : 2;
    for (int o = -width; o <= width; ++o) {
      SDL_RenderDrawLine(renderer, a.x + o, a.y, b.x + o, b.y);
      SDL_RenderDrawLine(renderer, a.x, a.y + o, b.x, b.y + o);
    }
    if (edge.route == RouteKind::Ship) {
      const Point mid{(a.x + b.x) / 2, (a.y + b.y) / 2};
      fill_circle(renderer, mid.x, mid.y, 3, {238, 243, 248, 255});
    }
  }
  for (int i = 0; i < static_cast<int>(game.board.nodes.size()); ++i) {
    const Node& node = game.board.nodes[i];
    if (node.owner >= 0) {
      const Point pos = board_screen_point({node.x, node.y}, view);
      draw_house(renderer, game.players[node.owner].color, pos.x, pos.y, node.city, 0);
    }
  }
  if (!game.valid_targets.empty()) {
    const int target = game.valid_targets[static_cast<std::size_t>(game.target_index)];
    if (game.target_kind == TargetKind::Node) {
      const Point pos = board_screen_point({game.board.nodes[target].x, game.board.nodes[target].y}, view);
      draw_rect(renderer, {pos.x - 10, pos.y - 10, 20, 20}, game.theme.accent);
    } else if (game.target_kind == TargetKind::Edge) {
      const Edge& edge = game.board.edges[target];
      const Point a = board_screen_point({game.board.nodes[edge.a].x, game.board.nodes[edge.a].y}, view);
      const Point b = board_screen_point({game.board.nodes[edge.b].x, game.board.nodes[edge.b].y}, view);
      SDL_SetRenderDrawColor(renderer, game.theme.accent.r, game.theme.accent.g, game.theme.accent.b, game.theme.accent.a);
      for (int o = -3; o <= 3; ++o) {
        SDL_RenderDrawLine(renderer, a.x + o, a.y, b.x + o, b.y);
      }
    } else if (game.target_kind == TargetKind::Hex) {
      const Point center = board_screen_point({game.board.hexes[target].cx, game.board.hexes[target].cy}, view);
      draw_hex_outline(renderer, hex_points(center.x, center.y, radius + 4), game.theme.accent);
    }
  }
}

bool play_selected_dev_card(GameState& game, int index) {
  Player& player = game.players[game.current_player];
  if (index < 0 || index >= 5 || player.dev_cards[index] <= 0) {
    return false;
  }
  if (player.new_dev_cards[index] > 0 && player.dev_cards[index] == player.new_dev_cards[index]) {
    game.status = "NEW DEV CARDS WAIT ONE TURN";
    return false;
  }
  if (game.played_dev_this_turn && index != static_cast<int>(DevCard::VictoryPoint)) {
    game.status = "ONLY ONE DEV CARD PER TURN";
    return false;
  }

  const DevCard card = static_cast<DevCard>(index);
  if (card == DevCard::VictoryPoint) {
    game.status = "VICTORY POINTS SCORE AUTOMATICALLY";
    return false;
  }

  --player.dev_cards[index];
  game.played_dev_this_turn = 1;
  switch (card) {
    case DevCard::Knight:
      ++player.used_knights;
      update_victory(game);
      if (seafarers_active(game)) {
        game.raid_choice = 0;
        game.screen = Screen::RaidChoice;
        game.status = "KNIGHT / ROBBER OR PIRATE";
      } else {
        game.screen = Screen::Robber;
        game.status = "KNIGHT / MOVE ROBBER";
        assign_targets(game, TargetKind::Hex);
      }
      return true;
    case DevCard::RoadBuilding:
      game.free_roads_to_place = 2;
      game.screen = Screen::Action;
      game.action_index = 0;
      game.status = seafarers_active(game) ? "PLACE 2 FREE ROUTES" : "PLACE 2 FREE ROADS";
      return true;
    case DevCard::YearOfPlenty:
      game.plenty_picks_left = 2;
      game.menu_index = 0;
      game.screen = Screen::DevPlenty;
      game.status = "YEAR OF PLENTY";
      return true;
    case DevCard::Monopoly:
      game.menu_index = 0;
      game.screen = Screen::DevMonopoly;
      game.status = "MONOPOLY";
      return true;
    case DevCard::VictoryPoint: break;
  }
  return false;
}

std::vector<ActionId> current_actions(const GameState& game) {
  if (game.screen == Screen::Roll) {
    return {ActionId::Roll};
  }
  if (game.free_roads_to_place > 0) {
    if (seafarers_active(game)) {
      return {ActionId::BuildRoad, ActionId::BuildShip, ActionId::EndTurn};
    }
    return {ActionId::BuildRoad, ActionId::EndTurn};
  }
  std::vector<ActionId> actions;
  actions.push_back(ActionId::BuildRoad);
  if (seafarers_active(game)) {
    actions.push_back(ActionId::BuildShip);
    actions.push_back(ActionId::MoveShip);
  }
  actions.push_back(ActionId::BuildSettlement);
  actions.push_back(ActionId::BuildCity);
  actions.push_back(ActionId::BuyDevCard);
  actions.push_back(ActionId::ViewDevCards);
  actions.push_back(ActionId::Trade);
  actions.push_back(ActionId::EndTurn);
  return actions;
}

std::string action_label(ActionId id) {
  switch (id) {
    case ActionId::Roll: return "ROLL DICE";
    case ActionId::BuildRoad: return "BUILD ROAD";
    case ActionId::BuildShip: return "BUILD SHIP";
    case ActionId::MoveShip: return "MOVE SHIP";
    case ActionId::BuildSettlement: return "BUILD SETTLEMENT";
    case ActionId::BuildCity: return "BUILD CITY";
    case ActionId::BuyDevCard: return "BUY DEV CARD";
    case ActionId::ViewDevCards: return "VIEW DEV CARDS";
    case ActionId::Trade: return "TRADE";
    case ActionId::EndTurn: return "END TURN";
    default: return "";
  }
}

std::string trade_mode_title(TradeMode mode) {
  switch (mode) {
    case TradeMode::Player: return "PLAYER TRADE";
    case TradeMode::Bank: return "BANK TRADE";
    case TradeMode::Port: return "PORT TRADE";
    default: return "TRADE";
  }
}

void draw_overlay(SDL_Renderer* renderer, const Theme& theme, const std::string& title, const std::string& subtitle) {
  const SDL_Rect box{(kWindowWidth - 304) / 2, 8, 304, 54};
  fill_rect(renderer, box, theme.overlay);
  draw_rect(renderer, box, theme.frame);
  const int title_scale = fit_text_scale(title, box.w - 24, 2, 1);
  draw_text(renderer, title, box.x + box.w / 2, box.y + 7, title_scale, theme.text, true);
  draw_wrapped_text(renderer, subtitle, box.x + 12, box.y + 28, box.w - 24, 1, 2, theme.muted);
}

void draw_dev_card_overlay(SDL_Renderer* renderer, const GameState& game) {
  const SDL_Rect box{18, 150, 322, 152};
  fill_rect(renderer, box, game.theme.overlay);
  draw_rect(renderer, box, game.theme.frame);
  draw_text(renderer, "DEV CARDS", box.x + box.w / 2, box.y + 10, 2, game.theme.text, true);
  const Player& player = game.players[game.current_player];
  for (int index = 0; index < 5; ++index) {
    const SDL_Rect row{box.x + 12, box.y + 36 + index * 16, 148, 12};
    if (index == game.menu_index) {
      fill_rect(renderer, row, game.theme.panel_hi);
    }
    draw_text(renderer, kDevCardNames[static_cast<std::size_t>(index)], row.x + 4, row.y + 2, 1,
              index == game.menu_index ? game.theme.accent : game.theme.text);
    draw_text_right(renderer, std::to_string(player.dev_cards[index] + player.new_dev_cards[index]), box.x + 168, row.y + 2, 1,
                    game.theme.text);
  }
  fill_rect(renderer, {box.x + 182, box.y + 36, 128, 88}, game.theme.panel_hi);
  draw_text(renderer, "CARD NOTE", box.x + 192, box.y + 44, 1, game.theme.text);
  draw_wrapped_text(renderer, kDevCardTooltip[static_cast<std::size_t>(game.menu_index)], box.x + 192, box.y + 60, 108, 1, 4,
                    game.theme.muted);
  draw_wrapped_text(renderer, player.new_dev_cards[game.menu_index] > 0 ? "NEW CARDS WAIT UNTIL NEXT TURN" :
                                                          "A PLAY / B CLOSE",
                    box.x + 192, box.y + 102, 108, 1, 3, game.theme.accent);
}

void draw_resource_picker_overlay(SDL_Renderer* renderer, const GameState& game, const std::string& title,
                                  const std::string& subtitle) {
  draw_overlay(renderer, game.theme, title, subtitle);
  const SDL_Rect box{18, 146, 222, 128};
  fill_rect(renderer, box, game.theme.overlay);
  draw_rect(renderer, box, game.theme.frame);
  for (int index = 0; index < 5; ++index) {
    const SDL_Rect row{box.x + 12, box.y + 10 + index * 22, box.w - 24, 18};
    if (index == game.menu_index) {
      fill_rect(renderer, row, game.theme.panel_hi);
    }
    draw_text(renderer, kResourceNames[static_cast<std::size_t>(index)], row.x + 6, row.y + 3, 2,
              index == game.menu_index ? game.theme.accent : game.theme.text);
  }
}

void draw_trade_menu_overlay(SDL_Renderer* renderer, const GameState& game) {
  draw_overlay(renderer, game.theme, "TRADE", "CHOOSE TRADE TYPE");
  const SDL_Rect box{18, 146, 228, 108};
  fill_rect(renderer, box, game.theme.overlay);
  draw_rect(renderer, box, game.theme.frame);
  const std::array<const char*, 3> options{{"PLAYER", "BANK", "PORT"}};
  for (int index = 0; index < 3; ++index) {
    const SDL_Rect row{box.x + 12, box.y + 12 + index * 28, box.w - 24, 22};
    if (index == game.menu_index) {
      fill_rect(renderer, row, game.theme.panel_hi);
    }
    draw_text(renderer, options[static_cast<std::size_t>(index)], row.x + 8, row.y + 4, 2,
              index == game.menu_index ? game.theme.accent : game.theme.text);
  }
}

void draw_trade_partner_overlay(SDL_Renderer* renderer, const GameState& game) {
  draw_overlay(renderer, game.theme, "PLAYER TRADE", "CHOOSE A TRADE PARTNER");
  const SDL_Rect box{18, 146, 236, 128};
  fill_rect(renderer, box, game.theme.overlay);
  draw_rect(renderer, box, game.theme.frame);
  for (int index = 0; index < static_cast<int>(game.trade_partners.size()) && index < 5; ++index) {
    const int partner = game.trade_partners[static_cast<std::size_t>(index)];
    const SDL_Rect row{box.x + 12, box.y + 10 + index * 22, box.w - 24, 18};
    if (index == game.menu_index) {
      fill_rect(renderer, row, game.theme.panel_hi);
    }
    draw_text(renderer, game.players[partner].name, row.x + 6, row.y + 3, 2,
              index == game.menu_index ? game.theme.accent : game.players[partner].color);
  }
}

void draw_trade_offer_prompt(SDL_Renderer* renderer, const GameState& game) {
  const TradeProposal& trade = game.pending_trade;
  draw_overlay(renderer, game.theme, "TRADE OFFER", "EXAMINE THE OFFER");
  const SDL_Rect box{60, 146, 392, 108};
  fill_rect(renderer, box, game.theme.overlay);
  draw_rect(renderer, box, game.theme.frame);
  draw_text(renderer, game.players[trade.from].name, box.x + 18, box.y + 16, 2, game.players[trade.from].color);
  draw_text(renderer, "OFFERS YOU", box.x + 148, box.y + 16, 2, game.theme.text);
  draw_text(renderer, kResourceNames[trade.give], box.x + 18, box.y + 44, 2,
            resource_color(static_cast<Resource>(trade.give)));
  draw_text(renderer, "FOR", box.x + 162, box.y + 44, 2, game.theme.muted);
  draw_text(renderer, kResourceNames[trade.want], box.x + 214, box.y + 44, 2,
            resource_color(static_cast<Resource>(trade.want)));
  const SDL_Rect yes_box{box.x + 24, box.y + 72, 132, 24};
  const SDL_Rect no_box{box.x + box.w - 156, box.y + 72, 132, 24};
  fill_rect(renderer, yes_box, game.prompt_choice == 0 ? game.theme.panel_hi : game.theme.panel);
  fill_rect(renderer, no_box, game.prompt_choice == 1 ? game.theme.panel_hi : game.theme.panel);
  draw_rect(renderer, yes_box, game.theme.frame);
  draw_rect(renderer, no_box, game.theme.frame);
  draw_text(renderer, "YES", yes_box.x + yes_box.w / 2, yes_box.y + 6, 2,
            game.prompt_choice == 0 ? game.theme.accent : game.theme.text, true);
  draw_text(renderer, "NO", no_box.x + no_box.w / 2, no_box.y + 6, 2,
            game.prompt_choice == 1 ? game.theme.accent : game.theme.text, true);
}

void draw_road_badge(SDL_Renderer* renderer, int x, int y, bool active) {
  const SDL_Color color = active ? SDL_Color{233, 194, 92, 255} : SDL_Color{106, 114, 132, 255};
  fill_rect(renderer, {x, y + 11, 22, 5}, color);
  fill_rect(renderer, {x + 2, y + 7, 18, 4}, color);
  fill_rect(renderer, {x + 5, y + 3, 4, 4}, color);
  fill_rect(renderer, {x + 13, y + 3, 4, 4}, color);
  draw_rect(renderer, {x - 1, y + 2, 24, 15}, {48, 53, 66, 255});
}

void draw_sword_badge(SDL_Renderer* renderer, int x, int y, bool active) {
  const SDL_Color color = active ? SDL_Color{233, 194, 92, 255} : SDL_Color{106, 114, 132, 255};
  fill_rect(renderer, {x + 10, y + 2, 3, 14}, color);
  fill_rect(renderer, {x + 6, y + 6, 11, 3}, color);
  fill_rect(renderer, {x + 8, y + 16, 7, 3}, color);
  fill_rect(renderer, {x + 9, y + 19, 5, 3}, color);
  draw_rect(renderer, {x + 4, y + 1, 15, 22}, {48, 53, 66, 255});
}

void render_sidebar(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {kPanelX + 6, kPanelY + 8, kPanelW, kPanelH}, game.theme.frame);
  fill_rect(renderer, {kPanelX, kPanelY, kPanelW, kPanelH}, game.theme.panel);
  draw_rect(renderer, {kPanelX, kPanelY, kPanelW, kPanelH}, game.theme.muted);
  const Player& player = game.players[game.current_player];
  draw_text(renderer, player.name, kPanelX + 12, kPanelY + 14, 2, game.theme.text);
  draw_text_right(renderer, std::to_string(player.victory_points) + " VP", kPanelX + kPanelW - 48, kPanelY + 16, 1, game.theme.accent);
  draw_road_badge(renderer, kPanelX + kPanelW - 44, kPanelY + 8, player.has_longest_road != 0);
  draw_sword_badge(renderer, kPanelX + kPanelW - 22, kPanelY + 6, player.has_largest_army != 0);
  draw_text(renderer, "ROLL", kPanelX + 12, kPanelY + 40, 1, game.theme.muted);
  draw_text_right(renderer, game.last_roll == 0 ? "--" : std::to_string(game.last_roll), kPanelX + kPanelW - 12, kPanelY + 40, 1, game.theme.text);
  if (game.dice_a > 0 && game.dice_b > 0) {
    draw_die_face(renderer, kPanelX + 12, kPanelY + 54, game.dice_a, game.theme);
    draw_die_face(renderer, kPanelX + 42, kPanelY + 54, game.dice_b, game.theme);
  }
  draw_text(renderer, "HAND", kPanelX + 72, kPanelY + 60, 1, game.theme.text);
  for (int r = 0; r < 5; ++r) {
    draw_text(renderer, kResourceNames[r], kPanelX + 12, kPanelY + 92 + r * 14, 1, game.theme.muted);
    draw_text_right(renderer, std::to_string(player.resources[r]), kPanelX + kPanelW - 18, kPanelY + 92 + r * 14, 1, game.theme.text);
  }
  draw_text(renderer, "DECK", kPanelX + 12, kPanelY + 164, 1, game.theme.muted);
  draw_text_right(renderer, std::to_string(static_cast<int>(game.dev_deck.size())), kPanelX + kPanelW - 18, kPanelY + 164, 1,
                  game.theme.text);
  draw_text(renderer, "DEV", kPanelX + 12, kPanelY + 178, 1, game.theme.muted);
  draw_text_right(renderer,
                  std::to_string(std::accumulate(player.dev_cards.begin(), player.dev_cards.end(), 0) +
                                 std::accumulate(player.new_dev_cards.begin(), player.new_dev_cards.end(), 0)),
                  kPanelX + kPanelW - 18, kPanelY + 178, 1, game.theme.text);
  draw_text(renderer, seafarers_active(game) ? "SHIP" : "ROAD", kPanelX + 12, kPanelY + 192, 1, game.theme.muted);
  draw_text_right(renderer, seafarers_active(game) ? std::to_string(player.ships_left) : std::to_string(player.roads_left),
                  kPanelX + kPanelW - 18, kPanelY + 192, 1, game.theme.text);
  draw_text(renderer, seafarers_active(game) ? "ROUTE" : "ROAD", kPanelX + 12, kPanelY + 204, 1, game.theme.muted);
  draw_text(renderer, player.has_longest_road ? "GOLD" : "LOCKED", kPanelX + 52, kPanelY + 204, 1,
            player.has_longest_road ? game.theme.accent : game.theme.muted);
  draw_text(renderer, "ARMY", kPanelX + 12, kPanelY + 216, 1, game.theme.muted);
  draw_text(renderer, player.has_largest_army ? "GOLD" : "LOCKED", kPanelX + 52, kPanelY + 216, 1,
            player.has_largest_army ? game.theme.accent : game.theme.muted);
  fill_rect(renderer, {kPanelX + 10, kPanelY + 224, kPanelW - 20, 98}, game.theme.panel_hi);
  draw_text(renderer, "PLAYERS", kPanelX + 18, kPanelY + 238, 1, game.theme.text);
  for (int i = 0; i < static_cast<int>(game.players.size()); ++i) {
    const int y = kPanelY + 252 + i * 12;
    SDL_Color name_color = game.players[i].color;
    if (i != game.current_player) {
      name_color.r = static_cast<Uint8>((static_cast<int>(name_color.r) * 3 + static_cast<int>(game.theme.panel.r)) / 4);
      name_color.g = static_cast<Uint8>((static_cast<int>(name_color.g) * 3 + static_cast<int>(game.theme.panel.g)) / 4);
      name_color.b = static_cast<Uint8>((static_cast<int>(name_color.b) * 3 + static_cast<int>(game.theme.panel.b)) / 4);
    }
    draw_text(renderer, game.players[i].name, kPanelX + 18, y, 1, name_color);
    draw_text_right(renderer, std::to_string(game.players[i].victory_points), kPanelX + kPanelW - 18, y, 1, game.theme.text);
  }
  fill_rect(renderer, {kPanelX + 10, kPanelY + 330, kPanelW - 20, 48}, game.theme.panel_hi);
  draw_text(renderer, "STATUS", kPanelX + 18, kPanelY + 338, 1, game.theme.text);
  draw_wrapped_text(renderer, game.status, kPanelX + 18, kPanelY + 352, kPanelW - 36, 1, 2, game.theme.muted);
}

void render_action_menu(SDL_Renderer* renderer, const GameState& game) {
  if (game.screen != Screen::Action && game.screen != Screen::Roll) {
    return;
  }
  const auto actions = current_actions(game);
  fill_rect(renderer, {12, 362, 334, 134}, game.theme.panel);
  draw_rect(renderer, {12, 362, 334, 134}, game.theme.frame);
  draw_text(renderer, "TURN MENU", 24, 372, 1, game.theme.text);
  const int row_step = actions.size() > 8 ? 12 : 15;
  for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
    const SDL_Rect row{20, 388 + i * row_step, 318, row_step - 1};
    if (i == game.action_index) {
      fill_rect(renderer, row, game.theme.panel_hi);
    }
    draw_text(renderer, action_label(actions[i]), row.x + 4, row.y, row_step < 15 ? 1 : 2,
              i == game.action_index ? game.theme.accent : game.theme.text);
  }
}

void render_config(SDL_Renderer* renderer, const GameState& game) {
  fill_rect(renderer, {0, 0, kWindowWidth, kWindowHeight}, game.theme.bg);
  draw_text(renderer, "SETTLERS", kWindowWidth / 2, 20, 5, game.theme.text, true);
  draw_text(renderer, "CLASSIC OR SEAFARERS / CPU OR LOCAL", kWindowWidth / 2, 62, 1, game.theme.muted, true);
  fill_rect(renderer, {76, 122, 360, 208}, game.theme.panel);
  draw_rect(renderer, {76, 122, 360, 208}, game.theme.frame);
  if (game.screen == Screen::Variant) {
    const SDL_Rect classic{96, 196, 132, 72};
    const SDL_Rect seafarers{282, 196, 132, 72};
    draw_text(renderer, "CHOOSE MAP", kWindowWidth / 2, 144, 3, game.theme.text, true);
    fill_rect(renderer, classic, game.menu_index == 0 ? game.theme.accent : game.theme.panel_hi);
    fill_rect(renderer, seafarers, game.menu_index == 1 ? game.theme.accent : game.theme.panel_hi);
    draw_rect(renderer, classic, game.theme.frame);
    draw_rect(renderer, seafarers, game.theme.frame);
    draw_text(renderer, "CLASSIC", classic.x + classic.w / 2, classic.y + 14, 2,
              game.menu_index == 0 ? game.theme.bg : game.theme.text, true);
    draw_text(renderer, "BASE GAME", classic.x + classic.w / 2, classic.y + 40, 1,
              game.menu_index == 0 ? game.theme.bg : game.theme.text, true);
    draw_text(renderer, "SEAFARERS", seafarers.x + seafarers.w / 2, seafarers.y + 14, 2,
              game.menu_index == 1 ? game.theme.bg : game.theme.text, true);
    draw_text(renderer, "SHIPS + PIRATE", seafarers.x + seafarers.w / 2, seafarers.y + 40, 1,
              game.menu_index == 1 ? game.theme.bg : game.theme.text, true);
  } else if (game.screen == Screen::Mode) {
    const SDL_Rect cpu{112, 208, 116, 56};
    const SDL_Rect local{284, 208, 116, 56};
    draw_text(renderer, "CHOOSE MODE", kWindowWidth / 2, 144, 3, game.theme.text, true);
    fill_rect(renderer, cpu, game.menu_index == 0 ? game.theme.accent : game.theme.panel_hi);
    fill_rect(renderer, local, game.menu_index == 1 ? game.theme.accent : game.theme.panel_hi);
    draw_rect(renderer, cpu, game.theme.frame);
    draw_rect(renderer, local, game.theme.frame);
    draw_text(renderer, "CPU", cpu.x + cpu.w / 2, cpu.y + 12, 2, game.menu_index == 0 ? game.theme.bg : game.theme.text, true);
    draw_text(renderer, "OPPONENT", cpu.x + cpu.w / 2, cpu.y + 32, 1, game.menu_index == 0 ? game.theme.bg : game.theme.text, true);
    draw_text(renderer, "LOCAL", local.x + local.w / 2, local.y + 12, 2, game.menu_index == 1 ? game.theme.bg : game.theme.text, true);
    draw_text(renderer, "SAME DEVICE", local.x + local.w / 2, local.y + 32, 1, game.menu_index == 1 ? game.theme.bg : game.theme.text, true);
  } else if (game.screen == Screen::Humans) {
    draw_text(renderer, "LOCAL PLAYERS", kWindowWidth / 2, 144, 3, game.theme.text, true);
    draw_text(renderer, std::to_string(game.local_humans), kWindowWidth / 2, 216, 6, game.theme.accent, true);
  } else if (game.screen == Screen::Bots) {
    draw_text(renderer, game.mode == GameMode::Cpu ? "# OF BOTS" : "ADD CPU BOTS", kWindowWidth / 2, 144, 3, game.theme.text, true);
    draw_text(renderer, std::to_string(game.bots), kWindowWidth / 2, 216, 6, game.theme.accent, true);
    draw_text(renderer, "TOTAL " + std::to_string(game.local_humans + game.bots) + " PLAYERS", kWindowWidth / 2, 272, 1, game.theme.muted, true);
  }
  draw_text(renderer, "A CONFIRM  B BACK", kWindowWidth / 2, 344, 1, game.theme.muted, true);
}

void render_game(SDL_Renderer* renderer, GameState& game) {
  fill_rect(renderer, {0, 0, kWindowWidth, kWindowHeight}, game.theme.bg);
  draw_text(renderer, "SETTLERS", kWindowWidth / 2, 10, 4, game.theme.text, true);
  draw_text(renderer, seafarers_active(game) ? "SEAFARERS FRONTIER" : (game.board.expanded ? "EXPANDED FRONTIER" : "CLASSIC FRONTIER"),
            kWindowWidth / 2, 42, 1, game.theme.muted, true);
  const BoardView board_view = compute_board_view(game);
  fill_rect(renderer, {board_view.clip.x - 2, board_view.clip.y - 2, board_view.clip.w + 4, board_view.clip.h + 4}, game.theme.frame);
  fill_rect(renderer, board_view.clip, {12, 19, 29, 255});
  SDL_RenderSetClipRect(renderer, &board_view.clip);
  draw_board(renderer, game, board_view);
  SDL_RenderSetClipRect(renderer, nullptr);
  render_sidebar(renderer, game);
  render_action_menu(renderer, game);
  if (game.screen == Screen::Intro) {
    draw_overlay(renderer, game.theme, game.players[game.current_player].name + " TURN",
                 player_human(game, game.current_player) ? (needs_pass_device(game) ? "PASS DEVICE / PRESS A" : "PRESS A TO ROLL")
                                                         : "CPU THINKING");
  } else if (game.screen == Screen::SetupSettlement) {
    draw_overlay(renderer, game.theme, "PLACE SETTLEMENT", game.players[game.current_player].name);
  } else if (game.screen == Screen::SetupRoute) {
    draw_overlay(renderer, game.theme, game.route_mode == RouteKind::Ship ? "PLACE STARTING SHIP" : "PLACE STARTING ROAD",
                 game.players[game.current_player].name);
  } else if (game.screen == Screen::PlaceSettlement) {
    draw_overlay(renderer, game.theme, "BUILD SETTLEMENT", "SELECT A NODE");
  } else if (game.screen == Screen::PlaceRoad) {
    draw_overlay(renderer, game.theme, "BUILD ROAD", "SELECT AN EDGE");
  } else if (game.screen == Screen::PlaceShip) {
    draw_overlay(renderer, game.theme, game.selected_ship_edge >= 0 ? "MOVE SHIP" : "BUILD SHIP",
                 game.selected_ship_edge >= 0 ? "SELECT A NEW SEA ROUTE" : "SELECT A SEA ROUTE");
  } else if (game.screen == Screen::MoveShip) {
    draw_overlay(renderer, game.theme, "MOVE SHIP", "PICK AN OPEN SHIP");
  } else if (game.screen == Screen::PlaceCity) {
    draw_overlay(renderer, game.theme, "BUILD CITY", "UPGRADE A SETTLEMENT");
  } else if (game.screen == Screen::RaidChoice) {
    draw_overlay(renderer, game.theme, "ROBBER OR PIRATE", "UP / DOWN THEN A");
  } else if (game.screen == Screen::Robber) {
    draw_overlay(renderer, game.theme, "MOVE ROBBER", game.players[game.current_player].name);
  } else if (game.screen == Screen::Pirate) {
    draw_overlay(renderer, game.theme, "MOVE PIRATE", game.players[game.current_player].name);
  } else if (game.screen == Screen::TradeMenu) {
    draw_trade_menu_overlay(renderer, game);
  } else if (game.screen == Screen::TradePartner) {
    draw_trade_partner_overlay(renderer, game);
  } else if (game.screen == Screen::TradeGive) {
    draw_resource_picker_overlay(renderer, game, trade_mode_title(game.trade_mode),
                                 game.trade_mode == TradeMode::Player ? "OFFER ONE RESOURCE" : "CHOOSE RESOURCE TO GIVE");
  } else if (game.screen == Screen::TradeTake) {
    draw_resource_picker_overlay(renderer, game, trade_mode_title(game.trade_mode),
                                 game.trade_mode == TradeMode::Player ? "REQUEST ONE RESOURCE" : "CHOOSE RESOURCE TO TAKE");
  } else if (game.screen == Screen::TradeOfferPrompt) {
    draw_trade_offer_prompt(renderer, game);
  } else if (game.screen == Screen::DevCards) {
    draw_dev_card_overlay(renderer, game);
  } else if (game.screen == Screen::DevPlenty) {
    draw_resource_picker_overlay(renderer, game, "YEAR OF PLENTY",
                                 game.plenty_picks_left == 2 ? "CHOOSE FIRST RESOURCE" : "CHOOSE SECOND RESOURCE");
  } else if (game.screen == Screen::DevMonopoly) {
    draw_resource_picker_overlay(renderer, game, "MONOPOLY", "TAKE ALL OF ONE RESOURCE");
  } else if (game.screen == Screen::GameOver) {
    draw_overlay(renderer, game.theme, game.players[game.winner].name + " WINS", "START FOR NEW MATCH");
  } else if (game.screen == Screen::RobberSteal) {
    draw_overlay(renderer, game.theme, "STEAL A CARD", "CHOOSE AN ADJACENT PLAYER");
    const SDL_Rect box{18, 150, 322, 56};
    fill_rect(renderer, box, game.theme.overlay);
    draw_rect(renderer, box, game.theme.frame);
    for (int index = 0; index < static_cast<int>(game.robber_targets.size()); ++index) {
      const SDL_Rect row{box.x + 12 + index * 100, box.y + 16, 92, 24};
      if (index == game.robber_target_index) {
        fill_rect(renderer, row, game.theme.panel_hi);
      }
      draw_text(renderer, game.players[game.robber_targets[static_cast<std::size_t>(index)]].name, row.x + row.w / 2, row.y + 7, 1,
                index == game.robber_target_index ? game.theme.accent : game.theme.text, true);
    }
  } else if (game.screen == Screen::PirateSteal) {
    draw_overlay(renderer, game.theme, "RAID A SHIP", "CHOOSE A CAPTAIN");
    const SDL_Rect box{18, 150, 322, 56};
    fill_rect(renderer, box, game.theme.overlay);
    draw_rect(renderer, box, game.theme.frame);
    for (int index = 0; index < static_cast<int>(game.robber_targets.size()); ++index) {
      const SDL_Rect row{box.x + 12 + index * 100, box.y + 16, 92, 24};
      if (index == game.robber_target_index) {
        fill_rect(renderer, row, game.theme.panel_hi);
      }
      draw_text(renderer, game.players[game.robber_targets[static_cast<std::size_t>(index)]].name, row.x + row.w / 2, row.y + 7, 1,
                index == game.robber_target_index ? game.theme.accent : game.theme.text, true);
    }
  } else if (game.screen == Screen::GoldChoice && !game.gold_picks.empty()) {
    draw_resource_picker_overlay(renderer, game, "GOLD FIELD", game.players[game.gold_picks.front().first].name);
  }
}

Point target_point(const GameState& game, TargetKind kind, int target) {
  if (kind == TargetKind::Node) {
    return {game.board.nodes[target].x, game.board.nodes[target].y};
  }
  if (kind == TargetKind::Edge) {
    const Edge& edge = game.board.edges[target];
    return {(game.board.nodes[edge.a].x + game.board.nodes[edge.b].x) / 2,
            (game.board.nodes[edge.a].y + game.board.nodes[edge.b].y) / 2};
  }
  if (kind == TargetKind::Hex) {
    return {game.board.hexes[target].cx, game.board.hexes[target].cy};
  }
  return {0, 0};
}

void cycle_target(GameState& game, int delta) {
  if (game.valid_targets.empty()) {
    return;
  }
  const int count = static_cast<int>(game.valid_targets.size());
  game.target_index = (game.target_index + delta + count) % count;
}

void move_target_direction(GameState& game, int dx, int dy) {
  if (game.valid_targets.empty()) {
    return;
  }
  const int current_target = game.valid_targets[static_cast<std::size_t>(game.target_index)];
  const Point current = target_point(game, game.target_kind, current_target);
  int best_index = game.target_index;
  int best_score = 1 << 30;

  for (int index = 0; index < static_cast<int>(game.valid_targets.size()); ++index) {
    if (index == game.target_index) {
      continue;
    }
    const Point candidate = target_point(game, game.target_kind, game.valid_targets[static_cast<std::size_t>(index)]);
    const int diff_x = candidate.x - current.x;
    const int diff_y = candidate.y - current.y;

    if (dx < 0 && diff_x >= 0) {
      continue;
    }
    if (dx > 0 && diff_x <= 0) {
      continue;
    }
    if (dy < 0 && diff_y >= 0) {
      continue;
    }
    if (dy > 0 && diff_y <= 0) {
      continue;
    }

    const int primary = dx != 0 ? std::abs(diff_x) : std::abs(diff_y);
    const int secondary = dx != 0 ? std::abs(diff_y) : std::abs(diff_x);
    const int score = primary * 8 + secondary * 3 + (diff_x * diff_x + diff_y * diff_y) / 16;
    if (score < best_score) {
      best_score = score;
      best_index = index;
    }
  }

  if (best_index != game.target_index) {
    game.target_index = best_index;
  }
}

void confirm_target(GameState& game) {
  if (game.valid_targets.empty()) {
    game.screen = Screen::Action;
    return;
  }
  const int target = game.valid_targets[static_cast<std::size_t>(game.target_index)];
  if (game.screen == Screen::SetupSettlement) {
    place_settlement(game, target, true);
    game.screen = Screen::SetupRoute;
    game.status = "PLACE STARTING ROUTE";
    assign_targets(game, TargetKind::Edge);
  } else if (game.screen == Screen::SetupRoute) {
    if (game.route_mode == RouteKind::Ship && valid_ship_edge(game, game.current_player, target, true)) {
      place_ship(game, target, true, false);
      --game.players[game.current_player].ships_left;
    } else {
      place_road(game, target, true);
    }
    ++game.setup_index;
    if (game.setup_index >= game.total_players * 2) {
      game.current_player = 0;
      start_turn(game);
      game.screen = Screen::Roll;
      game.status = "ROLL DICE";
    } else {
      const bool reverse = game.setup_index >= game.total_players;
      game.current_player = reverse ? (game.total_players - 1 - (game.setup_index - game.total_players))
                                    : game.setup_index;
      game.screen = Screen::SetupSettlement;
      game.status = "PLACE STARTING SETTLEMENT";
      assign_targets(game, TargetKind::Node);
    }
  } else if (game.screen == Screen::PlaceSettlement) {
    place_settlement(game, target, false);
    game.screen = Screen::Action;
    game.status = "SETTLEMENT BUILT";
  } else if (game.screen == Screen::PlaceRoad) {
    if (game.free_roads_to_place > 0) {
      game.board.edges[target].owner = game.current_player;
      game.board.edges[target].route = RouteKind::Road;
      game.board.edges[target].built_turn = game.turn_number;
      --game.players[game.current_player].roads_left;
      --game.free_roads_to_place;
      update_victory(game);
      game.screen = game.free_roads_to_place > 0 ? Screen::PlaceRoad : Screen::Action;
      game.status = game.free_roads_to_place > 0 ? "PLACE NEXT FREE ROUTE" : "ROAD BUILDING COMPLETE";
      if (game.screen == Screen::PlaceRoad) {
        assign_targets(game, TargetKind::Edge);
      }
    } else {
      place_road(game, target, false);
      game.screen = Screen::Action;
      game.status = "ROAD BUILT";
    }
  } else if (game.screen == Screen::PlaceShip) {
    if (game.selected_ship_edge >= 0) {
      place_ship(game, target, false, false);
      game.selected_ship_edge = -1;
      game.screen = Screen::Action;
      game.status = "SHIP MOVED";
    } else if (game.free_roads_to_place > 0) {
      place_ship(game, target, false, false);
      --game.players[game.current_player].ships_left;
      --game.free_roads_to_place;
      game.screen = game.free_roads_to_place > 0 ? Screen::PlaceShip : Screen::Action;
      game.status = game.free_roads_to_place > 0 ? "PLACE NEXT FREE ROUTE" : "ROAD BUILDING COMPLETE";
      if (game.screen == Screen::PlaceShip) {
        assign_targets(game, TargetKind::Edge);
      }
    } else {
      place_ship(game, target, false, true);
      game.screen = Screen::Action;
      game.status = "SHIP LAUNCHED";
    }
  } else if (game.screen == Screen::MoveShip) {
    game.selected_ship_edge = target;
    game.board.edges[target].owner = -1;
    game.board.edges[target].route = RouteKind::None;
    game.board.edges[target].built_turn = -1;
    game.screen = Screen::PlaceShip;
    assign_targets(game, TargetKind::Edge);
    if (game.valid_targets.empty()) {
      game.board.edges[target].owner = game.current_player;
      game.board.edges[target].route = RouteKind::Ship;
      game.board.edges[target].built_turn = game.turn_number - 1;
      game.selected_ship_edge = -1;
      game.screen = Screen::Action;
      game.status = "NO OPEN SEA ROUTE";
    } else {
      game.status = "MOVE SHIP TO A NEW EDGE";
    }
  } else if (game.screen == Screen::PlaceCity) {
    place_city(game, target);
    game.screen = Screen::Action;
    game.status = "CITY BUILT";
  } else if (game.screen == Screen::Robber) {
    finish_robber_move(game, target);
  } else if (game.screen == Screen::Pirate) {
    finish_pirate_move(game, target);
  }
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  pp_context context{};
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_AudioDeviceID audio_device = 0U;
  pp_audio_spec audio_spec{};
  GameState game;
  pp_input_state input{};
  pp_input_state previous{};
  int width = 0;
  int height = 0;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    return 1;
  }
  if (pp_init(&context, "settlers") != 0) {
    SDL_Quit();
    return 1;
  }
  pp_get_framebuffer_size(&context, &width, &height);
  width = std::max(width, 512);
  height = std::max(height, 512);
  window = SDL_CreateWindow("Settlers", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_RenderSetLogicalSize(renderer, kWindowWidth, kWindowHeight);
  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &game.tone;
  if (pp_audio_open(&audio_spec, &audio_device) == 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }
  while (!pp_should_exit(&context)) {
    const Uint32 now = SDL_GetTicks();
    pp_poll_input(&context, &input);
    const bool up = input.up && !previous.up;
    const bool down = input.down && !previous.down;
    const bool left = input.left && !previous.left;
    const bool right = input.right && !previous.right;
    const bool a = input.a && !previous.a;
    const bool b = input.b && !previous.b;
    const bool start = input.start && !previous.start;

    if (game.screen == Screen::TradeOfferPrompt && game.pending_trade.from >= 0) {
      if (left || up) game.prompt_choice = 0;
      if (right || down) game.prompt_choice = 1;
      if (a || start) {
        resolve_trade_prompt(game, game.prompt_choice == 0);
      } else if (b) {
        resolve_trade_prompt(game, false);
      }
    } else if (!game.players.empty() && !player_human(game, game.current_player) && now >= game.ai_ready_at) {
      cpu_take_turn(game);
      game.ai_ready_at = now + kCpuDelayMs;
    } else if (game.screen == Screen::Variant || game.screen == Screen::Mode || game.screen == Screen::Humans || game.screen == Screen::Bots) {
      if (game.screen == Screen::Variant) {
        if (left || up) game.menu_index = 0;
        if (right || down) game.menu_index = 1;
        if (a || start) {
          game.variant = game.menu_index == 0 ? Variant::Classic : Variant::Seafarers;
          game.screen = Screen::Mode;
          game.menu_index = 0;
        }
      } else if (game.screen == Screen::Mode) {
        if (left || up) game.menu_index = 0;
        if (right || down) game.menu_index = 1;
        if (b) {
          game.screen = Screen::Variant;
          game.menu_index = game.variant == Variant::Classic ? 0 : 1;
        }
        if (a || start) {
          game.mode = game.menu_index == 0 ? GameMode::Cpu : GameMode::Local;
          game.local_humans = game.mode == GameMode::Cpu ? 1 : 2;
          game.bots = game.mode == GameMode::Cpu ? 1 : 0;
          game.screen = game.mode == GameMode::Cpu ? Screen::Bots : Screen::Humans;
        }
      } else if (game.screen == Screen::Humans) {
        if (up) game.local_humans = std::min(6, game.local_humans + 1);
        if (down) game.local_humans = std::max(2, game.local_humans - 1);
        if (b) game.screen = Screen::Mode;
        if (a || start) game.screen = Screen::Bots;
      } else if (game.screen == Screen::Bots) {
        const int max_bots = 6 - game.local_humans;
        const int min_bots = game.mode == GameMode::Cpu ? 1 : 0;
        if (up) game.bots = std::min(max_bots, game.bots + 1);
        if (down) game.bots = std::max(min_bots, game.bots - 1);
        if (b) game.screen = game.mode == GameMode::Cpu ? Screen::Mode : Screen::Humans;
        if (a || start) begin_game(game);
      }
    } else if (!game.players.empty() && player_human(game, game.current_player)) {
      if (game.screen == Screen::Intro) {
        if (a || start) {
          game.screen = Screen::Roll;
          game.status = "ROLL DICE";
        }
      } else if (game.screen == Screen::SetupSettlement || game.screen == Screen::SetupRoute ||
                 game.screen == Screen::PlaceSettlement || game.screen == Screen::PlaceRoad || game.screen == Screen::PlaceShip ||
                 game.screen == Screen::MoveShip ||
                 game.screen == Screen::PlaceCity || game.screen == Screen::Robber || game.screen == Screen::Pirate) {
        if (left) move_target_direction(game, -1, 0);
        if (right) move_target_direction(game, 1, 0);
        if (up) move_target_direction(game, 0, -1);
        if (down) move_target_direction(game, 0, 1);
        if (b && game.screen == Screen::SetupRoute && seafarers_active(game) && node_coastal(game.board, game.last_setup_node)) {
          game.route_mode = game.route_mode == RouteKind::Road ? RouteKind::Ship : RouteKind::Road;
          assign_targets(game, TargetKind::Edge);
        } else if (b && game.screen == Screen::PlaceShip && game.selected_ship_edge >= 0) {
          game.board.edges[game.selected_ship_edge].owner = game.current_player;
          game.board.edges[game.selected_ship_edge].route = RouteKind::Ship;
          game.board.edges[game.selected_ship_edge].built_turn = game.turn_number - 1;
          game.selected_ship_edge = -1;
          game.screen = Screen::Action;
          game.status = "SHIP MOVE CANCELED";
        } else if (b && game.free_roads_to_place > 0 && seafarers_active(game) &&
                    (game.screen == Screen::PlaceRoad || game.screen == Screen::PlaceShip)) {
          game.screen = game.screen == Screen::PlaceRoad ? Screen::PlaceShip : Screen::PlaceRoad;
          game.route_mode = game.screen == Screen::PlaceRoad ? RouteKind::Road : RouteKind::Ship;
          assign_targets(game, TargetKind::Edge);
        } else if (b && game.screen != Screen::SetupSettlement && game.screen != Screen::SetupRoute &&
                   game.screen != Screen::Robber && game.screen != Screen::Pirate) {
          game.screen = Screen::Action;
        }
        if (a) confirm_target(game);
      } else if (game.screen == Screen::RobberSteal || game.screen == Screen::PirateSteal) {
        if (left || up) {
          game.robber_target_index =
              (game.robber_target_index + static_cast<int>(game.robber_targets.size()) - 1) % static_cast<int>(game.robber_targets.size());
        }
        if (right || down) {
          game.robber_target_index = (game.robber_target_index + 1) % static_cast<int>(game.robber_targets.size());
        }
        if (a && !game.robber_targets.empty()) {
          steal_random_resource(game, game.robber_targets[static_cast<std::size_t>(game.robber_target_index)]);
          game.screen = Screen::Action;
        }
      } else if (game.screen == Screen::RaidChoice) {
        if (up || left) game.raid_choice = 0;
        if (down || right) game.raid_choice = 1;
        if (a || start) {
          game.screen = game.raid_choice == 0 ? Screen::Robber : Screen::Pirate;
          game.status = game.raid_choice == 0 ? "MOVE ROBBER" : "MOVE PIRATE";
          assign_targets(game, TargetKind::Hex);
        }
      } else if (game.screen == Screen::Roll) {
        if (a || start) {
          std::uniform_int_distribution<int> die(1, 6);
          game.dice_a = die(game.rng);
          game.dice_b = die(game.rng);
          game.last_roll = game.dice_a + game.dice_b;
          distribute_resources(game, game.last_roll);
        }
      } else if (game.screen == Screen::GoldChoice && !game.gold_picks.empty()) {
        if (up) game.menu_index = (game.menu_index + 4) % 5;
        if (down) game.menu_index = (game.menu_index + 1) % 5;
        if (a) {
          ++game.players[game.gold_picks.front().first].resources[game.menu_index];
          game.gold_picks.erase(game.gold_picks.begin());
          process_gold_queue(game);
        }
      } else if (game.screen == Screen::TradeMenu) {
        if (up) game.menu_index = (game.menu_index + 2) % 3;
        if (down) game.menu_index = (game.menu_index + 1) % 3;
        if (b) {
          reset_trade_state(game);
          game.screen = Screen::Action;
        }
        if (a) {
          if (game.menu_index == 0) {
            game.trade_partners = available_trade_partners(game, game.current_player);
            if (game.trade_partners.empty()) {
              game.status = "NO PLAYER TO TRADE WITH";
              game.screen = Screen::Action;
              reset_trade_state(game);
            } else {
              game.trade_mode = TradeMode::Player;
              game.menu_index = 0;
              game.screen = Screen::TradePartner;
            }
          } else if (game.menu_index == 1) {
            game.trade_mode = TradeMode::Bank;
            game.menu_index = 0;
            game.screen = Screen::TradeGive;
          } else {
            if (!has_port_access(game, game.current_player)) {
              game.status = "NO HARBOR ACCESS";
              game.screen = Screen::Action;
              reset_trade_state(game);
            } else {
              game.trade_mode = TradeMode::Port;
              game.menu_index = 0;
              game.screen = Screen::TradeGive;
            }
          }
        }
      } else if (game.screen == Screen::TradePartner) {
        if (up) game.menu_index = (game.menu_index + static_cast<int>(game.trade_partners.size()) - 1) %
                                  static_cast<int>(game.trade_partners.size());
        if (down) game.menu_index = (game.menu_index + 1) % static_cast<int>(game.trade_partners.size());
        if (b) {
          game.screen = Screen::TradeMenu;
          game.menu_index = 0;
        }
        if (a) {
          game.trade_partner = game.trade_partners[static_cast<std::size_t>(game.menu_index)];
          game.trade_give = -1;
          game.trade_want = -1;
          game.trade_rate = 1;
          game.menu_index = 0;
          game.screen = Screen::TradeGive;
        }
      } else if (game.screen == Screen::TradeGive || game.screen == Screen::TradeTake || game.screen == Screen::DevPlenty ||
                 game.screen == Screen::DevMonopoly) {
        if (up) game.menu_index = (game.menu_index + 4) % 5;
        if (down) game.menu_index = (game.menu_index + 1) % 5;
        if (b) {
          if (game.screen == Screen::TradeTake) {
            game.screen = Screen::TradeGive;
          } else if (game.screen == Screen::TradeGive) {
            game.screen = game.trade_mode == TradeMode::Player ? Screen::TradePartner : Screen::TradeMenu;
          } else {
            game.screen = Screen::Action;
          }
        }
        if (a) {
          Player& player = game.players[game.current_player];
          if (game.screen == Screen::TradeGive && (game.trade_mode == TradeMode::Player || game.trade_mode == TradeMode::Bank ||
                                                   game.trade_mode == TradeMode::Port)) {
            const int rate = trade_rate_for(game, game.current_player, game.trade_mode, game.menu_index);
            if (game.trade_mode == TradeMode::Port && rate >= 4) {
              game.status = "NO MATCHING HARBOR FOR " + std::string(kResourceNames[static_cast<std::size_t>(game.menu_index)]);
            } else if (player.resources[game.menu_index] >= rate && rate <= 4) {
              game.trade_give = game.menu_index;
              game.trade_rate = rate;
              game.trade_resource = game.menu_index;
              game.screen = Screen::TradeTake;
            }
          } else if (game.screen == Screen::TradeTake && (game.trade_mode == TradeMode::Player || game.trade_mode == TradeMode::Bank ||
                                                          game.trade_mode == TradeMode::Port) &&
                     game.menu_index != game.trade_resource) {
            if (game.trade_mode == TradeMode::Player) {
              if (game.trade_partner < 0 || game.players[game.trade_partner].resources[game.menu_index] <= 0) {
                game.status = "TARGET HAS NO " + std::string(kResourceNames[static_cast<std::size_t>(game.menu_index)]);
              } else if (player_human(game, game.trade_partner)) {
                game.pending_trade = {game.current_player, game.trade_partner, game.trade_give, game.menu_index, 1, 0.0};
                game.prompt_choice = 0;
                game.screen = Screen::TradeOfferPrompt;
                game.status = describe_trade_offer(game, game.pending_trade);
              } else if (ai_accepts_trade(game, game.trade_partner, game.menu_index, game.trade_give)) {
                apply_player_trade(game, game.current_player, game.trade_partner, game.trade_give, game.menu_index);
                game.status = "TRADE ACCEPTED";
                game.screen = Screen::Action;
                reset_trade_state(game);
              } else {
                game.status = "TRADE REJECTED";
                game.screen = Screen::Action;
                reset_trade_state(game);
              }
            } else {
              player.resources[game.trade_resource] -= game.trade_rate;
              player.resources[game.menu_index] += 1;
              game.screen = Screen::Action;
              game.status = std::string(game.trade_mode == TradeMode::Port ? "PORT" : "BANK") + " TRADE COMPLETE";
              reset_trade_state(game);
            }
          } else if (game.screen == Screen::DevPlenty) {
            ++player.resources[game.menu_index];
            --game.plenty_picks_left;
            if (game.plenty_picks_left <= 0) {
              game.screen = Screen::Action;
              game.status = "YEAR OF PLENTY COMPLETE";
            }
          } else if (game.screen == Screen::DevMonopoly) {
            int total = 0;
            for (int p = 0; p < game.total_players; ++p) {
              if (p == game.current_player) {
                continue;
              }
              total += game.players[p].resources[game.menu_index];
              game.players[p].resources[game.menu_index] = 0;
            }
            game.players[game.current_player].resources[game.menu_index] += total;
            game.screen = Screen::Action;
            game.status = "MONOPOLY TOOK " + std::string(kResourceNames[static_cast<std::size_t>(game.menu_index)]);
          }
        }
      } else if (game.screen == Screen::DevCards) {
        if (up) game.menu_index = (game.menu_index + 4) % 5;
        if (down) game.menu_index = (game.menu_index + 1) % 5;
        if (b) game.screen = Screen::Action;
        if (a) {
          play_selected_dev_card(game, game.menu_index);
        }
      } else if (game.screen == Screen::Action) {
        const auto actions = current_actions(game);
        if (up) game.action_index = (game.action_index + static_cast<int>(actions.size()) - 1) % static_cast<int>(actions.size());
        if (down) game.action_index = (game.action_index + 1) % static_cast<int>(actions.size());
        if (a) {
          switch (actions[game.action_index]) {
            case ActionId::Roll: {
              std::uniform_int_distribution<int> die(1, 6);
              game.dice_a = die(game.rng);
              game.dice_b = die(game.rng);
              game.last_roll = game.dice_a + game.dice_b;
              distribute_resources(game, game.last_roll);
              break;
            }
            case ActionId::BuildRoad:
              if (game.free_roads_to_place > 0 || can_afford_road(game.players[game.current_player])) {
                game.screen = Screen::PlaceRoad;
                game.route_mode = RouteKind::Road;
                assign_targets(game, TargetKind::Edge);
              } else {
                game.status = "NEED WOOD + BRICK";
              }
              break;
            case ActionId::BuildShip:
              if (game.free_roads_to_place > 0 || can_afford_ship(game.players[game.current_player])) {
                game.screen = Screen::PlaceShip;
                game.route_mode = RouteKind::Ship;
                assign_targets(game, TargetKind::Edge);
              } else {
                game.status = "NEED WOOD + SHEEP";
              }
              break;
            case ActionId::MoveShip:
              game.selected_ship_edge = -1;
              game.screen = Screen::MoveShip;
              assign_targets(game, TargetKind::Edge);
              if (game.valid_targets.empty()) {
                game.screen = Screen::Action;
                game.status = "NO OPEN SHIP TO MOVE";
              } else {
                game.status = "PICK AN OPEN SHIP";
              }
              break;
            case ActionId::BuildSettlement:
              if (can_afford_settlement(game.players[game.current_player])) {
                game.screen = Screen::PlaceSettlement;
                assign_targets(game, TargetKind::Node);
              } else {
                game.status = "NEED WOOD BRICK SHEEP WHEAT";
              }
              break;
            case ActionId::BuildCity:
              if (can_afford_city(game.players[game.current_player])) {
                game.screen = Screen::PlaceCity;
                assign_targets(game, TargetKind::Node);
              } else {
                game.status = "NEED 2 WHEAT + 3 ORE";
              }
              break;
            case ActionId::BuyDevCard:
              if (game.dev_deck.empty()) {
                game.status = "DEV DECK EMPTY";
              } else if (can_afford_dev_card(game.players[game.current_player])) {
                spend_dev_card(game.players[game.current_player]);
                const DevCard card = game.dev_deck.back();
                game.dev_deck.pop_back();
                ++game.players[game.current_player].new_dev_cards[static_cast<int>(card)];
                game.status = "BOUGHT DEV CARD";
                update_victory(game);
              } else {
                game.status = "NEED SHEEP WHEAT ORE";
              }
              break;
            case ActionId::ViewDevCards:
              open_dev_cards(game);
              break;
            case ActionId::Trade:
              reset_trade_state(game);
              game.screen = Screen::TradeMenu;
              game.menu_index = 0;
              break;
            case ActionId::EndTurn:
              next_player(game);
              break;
          }
        }
      } else if (game.screen == Screen::GameOver) {
        if (a || start) {
          game.players.clear();
          game.screen = Screen::Variant;
          game.menu_index = 0;
        }
      }
    }

    if (game.players.empty() || game.screen == Screen::Variant || game.screen == Screen::Mode ||
        game.screen == Screen::Humans || game.screen == Screen::Bots) {
      render_config(renderer, game);
    } else {
      render_game(renderer, game);
    }
    SDL_RenderPresent(renderer);
    previous = input;
    SDL_Delay(16);
  }
  if (audio_device != 0U) SDL_CloseAudioDevice(audio_device);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&context);
  SDL_Quit();
  return 0;
}
