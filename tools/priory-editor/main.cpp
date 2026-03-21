#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 920;
constexpr int kLaunchWindowWidth = 1024;
constexpr int kLaunchWindowHeight = 720;
constexpr int kNativeTileSize = 16;
constexpr int kSpritePixels = 16;
constexpr int kSpriteDirections = 4;
constexpr int kSpriteFramesPerDirection = 2;
constexpr int kHeaderHeight = 48;
constexpr int kStatusHeight = 30;
constexpr int kSidebarWidth = 252;
constexpr int kInspectorWidth = 372;
constexpr int kPanelGap = 12;
constexpr int kSectionGap = 10;
constexpr int kButtonHeight = 24;
constexpr int kFieldHeight = 28;
constexpr int kTextInset = 5;
constexpr int kDefaultAreaWidth = 24;
constexpr int kDefaultAreaHeight = 18;

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
constexpr char kHerb = 'h';
constexpr char kCrate = 'x';
constexpr char kBoard = 'b';
constexpr char kRug = 'r';
constexpr char kReed = 'q';
constexpr char kFence = 'f';
constexpr char kBench = 'p';
constexpr char kBarrel = 'a';
constexpr char kLaundry = 'i';
constexpr char kCartRail = 'j';
constexpr char kCartStep = 'm';
constexpr char kCartWheel = 'u';
constexpr char kHarness = 'y';
constexpr char kMuleBack = 'n';
constexpr char kMuleFore = 'z';
constexpr char kEmptyTile = ' ';

enum class Direction {
  None = 0,
  Up,
  Down,
  Left,
  Right,
};

enum class SpriteRole {
  Prior,
  Sister,
  Monk,
  Fisher,
  Merchant,
  Child,
  Elder,
  Watchman,
};

enum class ToolMode {
  Paint = 0,
  Warp,
  Npc,
  Monster,
  Quest,
  Sprite,
};

enum class BrushKind {
  Tile = 0,
  Stamp,
};

enum class PaintSurface {
  Ground = 0,
  Wall,
};

enum class SceneSpriteTarget {
  None = 0,
  Npc,
  Monster,
};

enum class QuestRequirementType {
  ReachArea = 0,
  TalkToNpc,
  DefeatMonster,
  CollectItem,
  Custom,
};

enum class TextAlign {
  Left,
  Center,
  Right,
};

enum class TextVerticalAlign {
  Top,
  Middle,
  Bottom,
};

struct TextPageLayout {
  std::vector<std::string> lines;
  int scale = 1;
};

struct TextBoxOptions {
  int preferred_scale = 1;
  int min_scale = 1;
  bool wrap = false;
  int line_gap = 2;
  TextAlign align = TextAlign::Left;
  TextVerticalAlign vertical_align = TextVerticalAlign::Top;
};

struct TileDefinition {
  char symbol = kGrass;
  const char* name = "";
  const char* description = "";
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

struct WarpData {
  int x = 0;
  int y = 0;
  int width = 1;
  int height = 1;
  std::string label;
  std::string target_area;
  int target_x = 0;
  int target_y = 0;
  Direction target_facing = Direction::Down;
};

struct NpcData {
  std::string id;
  std::string name;
  SpriteRole role = SpriteRole::Monk;
  std::string sprite_id;
  int x = 0;
  int y = 0;
  Direction facing = Direction::Down;
  bool solid = true;
  std::string dialogue;
};

struct MonsterData {
  std::string id;
  std::string name;
  std::string sprite_id;
  int x = 0;
  int y = 0;
  Direction facing = Direction::Down;
  int max_hp = 6;
  int attack = 1;
  bool aggressive = true;
};

struct QuestStage {
  std::string id;
  std::string text;
};

struct QuestRequirement {
  std::string id;
  QuestRequirementType type = QuestRequirementType::Custom;
  std::string target_id;
  std::string area_id;
  int x = 0;
  int y = 0;
  int quantity = 1;
  std::string description;
};

struct QuestReward {
  std::string item_id;
  int quantity = 1;
  int coins = 0;
  std::string unlock_sprite_id;
};

struct QuestData {
  std::string id;
  std::string title;
  std::string summary;
  std::string quest_giver_id;
  std::string start_dialogue;
  std::string completion_dialogue;
  std::vector<QuestStage> stages;
  std::vector<QuestRequirement> requirements;
  std::vector<QuestReward> rewards;
};

struct StampData {
  std::string id;
  std::string name;
  std::vector<std::string> tiles;
};

struct SpriteAsset {
  std::string id;
  std::string name;
  bool monster = false;
  std::array<std::vector<std::string>, kSpriteDirections * kSpriteFramesPerDirection> frames;
};

struct AreaData {
  std::string id;
  std::string name;
  bool indoor = false;
  bool player_tillable = false;
  std::vector<std::string> tiles;
  std::vector<std::string> wall_tiles;
  std::vector<WarpData> warps;
  std::vector<NpcData> npcs;
  std::vector<MonsterData> monsters;
};

struct ProjectData {
  std::string name;
  std::vector<AreaData> areas;
  std::vector<QuestData> quests;
  std::vector<StampData> stamps;
  std::vector<SpriteAsset> sprites;
};

struct EditorSnapshot {
  ProjectData project;
  int area_index = 0;
  int warp_index = -1;
  int npc_index = -1;
  int monster_index = -1;
  int quest_index = 0;
  int stage_index = 0;
  int requirement_index = 0;
  int reward_index = 0;
  int sprite_index = 0;
};

struct Layout {
  SDL_Rect header{};
  SDL_Rect status{};
  SDL_Rect sidebar{};
  SDL_Rect map{};
  SDL_Rect inspector{};
};

struct ResolutionOption {
  const char* label = "";
  int width = 1280;
  int height = 720;
};

struct InputFrame {
  int mouse_x = 0;
  int mouse_y = 0;
  bool left_down = false;
  bool left_pressed = false;
  bool left_released = false;
  bool right_down = false;
  bool right_pressed = false;
  bool right_released = false;
  bool shift = false;
  bool ctrl = false;
  int wheel_y = 0;
};

struct UiFrame {
  bool mouse_consumed = false;
};

struct EditorState {
  ProjectData project;
  ToolMode tool = ToolMode::Paint;
  BrushKind brush_kind = BrushKind::Tile;
  PaintSurface paint_surface = PaintSurface::Ground;
  char selected_ground_tile = kGrass;
  char selected_wall_tile = kWall;
  int selected_stamp = -1;
  int area_index = 0;
  int warp_index = -1;
  int npc_index = -1;
  int monster_index = -1;
  int quest_index = 0;
  int stage_index = 0;
  int requirement_index = 0;
  int reward_index = 0;
  int sprite_index = 0;
  float map_zoom = 2.0f;
  int camera_x = 0;
  int camera_y = 0;
  bool show_grid = true;
  bool show_overlays = true;
  bool picking_warp_target = false;
  bool selection_active = false;
  bool selection_dragging = false;
  SDL_Point selection_anchor{0, 0};
  SDL_Point selection_head{0, 0};
  std::string focus_id;
  std::string status_message = "READY TO AUTHOR THE SAINT CATHERINE CHAPTER.";
  std::string project_path = "sample-games/priory/editor-projects/saint-catherine-arrival.json";
  std::string pending_stamp_name = "MARKET STALL";
  std::string new_area_id = "new_area_1";
  std::string new_area_name = "NEW AREA";
  int new_area_width = kDefaultAreaWidth;
  int new_area_height = kDefaultAreaHeight;
  int sprite_direction_index = 0;
  int sprite_frame_index = 0;
  int sprite_brush_index = 1;
  SceneSpriteTarget scene_sprite_target = SceneSpriteTarget::None;
  std::string scene_sprite_area_id;
  int resolution_index = 2;
  bool fullscreen = false;
  bool video_dirty = false;
  bool launch_active = true;
  int launch_selection = 0;
  bool dirty = false;
  Uint32 now = 0;
  std::vector<EditorSnapshot> undo_stack;
};

std::string default_project_path() {
#ifdef PRIORY_EDITOR_DEFAULT_PROJECT_PATH
  return PRIORY_EDITOR_DEFAULT_PROJECT_PATH;
#else
  return "sample-games/priory/editor-projects/saint-catherine-arrival.json";
#endif
}

void normalize_sprite_asset(SpriteAsset* asset);
void push_undo_snapshot(EditorState* state);
NpcData* current_npc(EditorState* state);
MonsterData* current_monster(EditorState* state);

struct JsonValue {
  enum class Type {
    Null = 0,
    Bool,
    Number,
    String,
    Array,
    Object,
  };

  Type type = Type::Null;
  bool bool_value = false;
  double number_value = 0.0;
  std::string string_value;
  std::vector<JsonValue> array_value;
  std::map<std::string, JsonValue> object_value;
};

template <typename T>
T clamp_value(T value, T min_value, T max_value) {
  return std::max(min_value, std::min(max_value, value));
}

std::string uppercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
  return value;
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

int hash_text(const std::string& text, int seed = 0) {
  std::uint32_t value = 2166136261u ^ static_cast<std::uint32_t>(seed);
  for (unsigned char ch : text) {
    value ^= ch;
    value *= 16777619u;
  }
  return static_cast<int>(value & 0x7fffffff);
}

bool point_in_rect(const SDL_Rect& rect, int x, int y) {
  return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

SDL_Point scale_window_point_to_render(SDL_Renderer* renderer, SDL_Window* window, int window_x, int window_y) {
  int window_width = 1;
  int window_height = 1;
  int render_width = 1;
  int render_height = 1;
  SDL_GetWindowSize(window, &window_width, &window_height);
  SDL_GetRendererOutputSize(renderer, &render_width, &render_height);
  SDL_Point point;
  point.x = static_cast<int>(std::lround(window_x * (static_cast<double>(render_width) / std::max(1, window_width))));
  point.y = static_cast<int>(std::lround(window_y * (static_cast<double>(render_height) / std::max(1, window_height))));
  return point;
}

const std::array<ResolutionOption, 6>& resolution_options() {
  static const std::array<ResolutionOption, 6> kOptions = {{
      {"1280 X 720", 1280, 720},
      {"1366 X 768", 1366, 768},
      {"1440 X 900", 1440, 900},
      {"1600 X 900", 1600, 900},
      {"1920 X 1080", 1920, 1080},
      {"2560 X 1440", 2560, 1440},
  }};
  return kOptions;
}

int default_resolution_index() {
  SDL_DisplayMode mode{};
  if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
    return 2;
  }

  int best_index = 0;
  int best_area = 0;
  const auto& options = resolution_options();
  for (std::size_t index = 0; index < options.size(); ++index) {
    if (options[index].width <= mode.w && options[index].height <= mode.h) {
      const int area = options[index].width * options[index].height;
      if (area > best_area) {
        best_area = area;
        best_index = static_cast<int>(index);
      }
    }
  }
  return best_index;
}

const ResolutionOption& current_resolution(const EditorState& state) {
  const auto& options = resolution_options();
  return options[static_cast<std::size_t>(clamp_value(state.resolution_index, 0, static_cast<int>(options.size()) - 1))];
}

Layout compute_layout(int window_width, int window_height) {
  Layout layout;
  int min_map_width = 260;
  const int min_panel_width = 150;
  int sidebar_width = kSidebarWidth;
  int inspector_width = kInspectorWidth;
  int map_width = window_width - sidebar_width - inspector_width - kPanelGap * 4;
  if (map_width < min_map_width) {
    int overflow = min_map_width - map_width;
    const int inspector_reduction = std::min(overflow, std::max(0, inspector_width - min_panel_width));
    inspector_width -= inspector_reduction;
    overflow -= inspector_reduction;
    const int sidebar_reduction = std::min(overflow, std::max(0, sidebar_width - min_panel_width));
    sidebar_width -= sidebar_reduction;
    overflow -= sidebar_reduction;
    if (overflow > 0) {
      min_map_width = std::max(180, min_map_width - overflow);
    }
    map_width = std::max(min_map_width, window_width - sidebar_width - inspector_width - kPanelGap * 4);
  }
  const int content_height = std::max(120, window_height - kHeaderHeight - kStatusHeight - kPanelGap * 2);

  layout.header = {0, 0, window_width, kHeaderHeight};
  layout.status = {0, window_height - kStatusHeight, window_width, kStatusHeight};
  layout.sidebar = {kPanelGap, kHeaderHeight + kPanelGap, sidebar_width,
                    content_height};
  layout.inspector = {window_width - inspector_width - kPanelGap, kHeaderHeight + kPanelGap, inspector_width,
                      content_height};
  layout.map = {layout.sidebar.x + layout.sidebar.w + kPanelGap, kHeaderHeight + kPanelGap,
                std::max(min_map_width, window_width - layout.sidebar.w - layout.inspector.w - kPanelGap * 4),
                content_height};
  return layout;
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
    case '[': return {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E};
    case ']': return {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E};
    case '(': return {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
    case ')': return {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
    case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
    case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    default: return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
}

int text_width(const std::string& text, int scale) {
  if (text.empty()) {
    return 0;
  }
  return static_cast<int>(text.size()) * (6 * scale) - scale;
}

int text_height(int scale) {
  return 7 * scale;
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
          fill_rect(renderer, SDL_Rect{draw_x + col * scale, y + row * scale, scale, scale}, color);
        }
      }
    }
    draw_x += 6 * scale;
  }
}

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  if (lines.empty()) {
    lines.push_back("");
  }
  return lines;
}

std::vector<std::string> break_word_to_width(const std::string& word, int max_width, int scale) {
  std::vector<std::string> chunks;
  std::string current;
  for (char ch : word) {
    const std::string candidate = current + ch;
    if (!current.empty() && text_width(candidate, scale) > max_width) {
      chunks.push_back(current);
      current = std::string(1, ch);
    } else {
      current = candidate;
    }
  }
  if (!current.empty()) {
    chunks.push_back(current);
  }
  if (chunks.empty()) {
    chunks.push_back("");
  }
  return chunks;
}

std::vector<std::string> wrap_text(const std::string& text, int max_width, int scale) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string word;
  std::string current;

  while (stream >> word) {
    if (text_width(word, scale) > max_width) {
      const auto pieces = break_word_to_width(word, max_width, scale);
      for (std::size_t index = 0; index < pieces.size(); ++index) {
        const std::string candidate = current.empty() ? pieces[index] : current + " " + pieces[index];
        if (!current.empty() && text_width(candidate, scale) > max_width) {
          lines.push_back(current);
          current.clear();
        }
        if (index + 1 < pieces.size()) {
          if (!current.empty()) {
            lines.push_back(current);
            current.clear();
          }
          lines.push_back(pieces[index]);
        } else {
          current = pieces[index];
        }
      }
      continue;
    }

    const std::string candidate = current.empty() ? word : current + " " + word;
    if (!current.empty() && text_width(candidate, scale) > max_width) {
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

std::vector<std::string> wrap_text_block(const std::string& text, int max_width, int scale, bool wrap) {
  if (!wrap) {
    return split_lines(text);
  }
  std::vector<std::string> lines;
  for (const auto& line : split_lines(text)) {
    if (line.empty()) {
      lines.push_back("");
      continue;
    }
    const auto wrapped = wrap_text(line, max_width, scale);
    lines.insert(lines.end(), wrapped.begin(), wrapped.end());
  }
  if (lines.empty()) {
    lines.push_back("");
  }
  return lines;
}

TextPageLayout layout_text_box_single(const std::string& text, const SDL_Rect& box, const TextBoxOptions& options) {
  for (int scale = options.preferred_scale; scale >= options.min_scale; --scale) {
    const auto lines = wrap_text_block(text, std::max(1, box.w), scale, options.wrap);
    int total_height = 0;
    bool fits = true;
    for (std::size_t index = 0; index < lines.size(); ++index) {
      total_height += text_height(scale);
      if (index + 1 < lines.size()) {
        total_height += options.line_gap;
      }
      if (text_width(lines[index], scale) > box.w) {
        fits = false;
      }
    }
    if (fits && total_height <= box.h) {
      return {lines, scale};
    }
  }
  return {wrap_text_block(text, std::max(1, box.w), options.min_scale, options.wrap), options.min_scale};
}

void draw_text_layout(SDL_Renderer* renderer,
                      const TextPageLayout& page,
                      const SDL_Rect& box,
                      const TextBoxOptions& options,
                      SDL_Color color) {
  int total_height = 0;
  for (std::size_t index = 0; index < page.lines.size(); ++index) {
    total_height += text_height(page.scale);
    if (index + 1 < page.lines.size()) {
      total_height += options.line_gap;
    }
  }

  int draw_y = box.y;
  if (options.vertical_align == TextVerticalAlign::Middle) {
    draw_y = box.y + std::max(0, (box.h - total_height) / 2);
  } else if (options.vertical_align == TextVerticalAlign::Bottom) {
    draw_y = box.y + std::max(0, box.h - total_height);
  }

  for (const auto& line : page.lines) {
    int draw_x = box.x;
    bool centered = false;
    if (options.align == TextAlign::Center) {
      draw_x = box.x + box.w / 2;
      centered = true;
    } else if (options.align == TextAlign::Right) {
      draw_x = box.x + std::max(0, box.w - text_width(line, page.scale));
    }
    draw_text(renderer, line, draw_x, draw_y, page.scale, color, centered);
    draw_y += text_height(page.scale) + options.line_gap;
  }
}

void draw_text_box(SDL_Renderer* renderer,
                   const std::string& text,
                   const SDL_Rect& box,
                   const TextBoxOptions& options,
                   SDL_Color color) {
  draw_text_layout(renderer, layout_text_box_single(text, box, options), box, options, color);
}

int area_width(const AreaData& area) {
  return area.tiles.empty() ? 0 : static_cast<int>(area.tiles.front().size());
}

int area_height(const AreaData& area) {
  return static_cast<int>(area.tiles.size());
}

int stamp_width(const StampData& stamp) {
  return stamp.tiles.empty() ? 0 : static_cast<int>(stamp.tiles.front().size());
}

int stamp_height(const StampData& stamp) {
  return static_cast<int>(stamp.tiles.size());
}

char fill_tile_for(const AreaData& area) {
  return area.indoor ? kFloor : kGrass;
}

std::string paint_surface_name(PaintSurface surface) {
  switch (surface) {
    case PaintSurface::Wall:
      return "WALL TILE";
    default:
      return "GROUND TILE";
  }
}

std::string direction_name(Direction direction) {
  switch (direction) {
    case Direction::Up:
      return "UP";
    case Direction::Down:
      return "DOWN";
    case Direction::Left:
      return "LEFT";
    case Direction::Right:
      return "RIGHT";
    default:
      return "NONE";
  }
}

Direction next_direction(Direction direction, int delta) {
  static const std::array<Direction, 4> kDirections = {
      Direction::Up, Direction::Right, Direction::Down, Direction::Left};
  int index = 0;
  for (std::size_t scan = 0; scan < kDirections.size(); ++scan) {
    if (kDirections[scan] == direction) {
      index = static_cast<int>(scan);
      break;
    }
  }
  index = (index + delta + static_cast<int>(kDirections.size())) % static_cast<int>(kDirections.size());
  return kDirections[static_cast<std::size_t>(index)];
}

std::string sprite_role_name(SpriteRole role) {
  switch (role) {
    case SpriteRole::Prior:
      return "PRIOR";
    case SpriteRole::Sister:
      return "SISTER";
    case SpriteRole::Fisher:
      return "FISHER";
    case SpriteRole::Merchant:
      return "MERCHANT";
    case SpriteRole::Child:
      return "CHILD";
    case SpriteRole::Elder:
      return "ELDER";
    case SpriteRole::Watchman:
      return "WATCHMAN";
    default:
      return "MONK";
  }
}

SpriteRole next_role(SpriteRole role, int delta) {
  static const std::array<SpriteRole, 8> kRoles = {
      SpriteRole::Prior, SpriteRole::Sister, SpriteRole::Monk, SpriteRole::Fisher,
      SpriteRole::Merchant, SpriteRole::Child, SpriteRole::Elder, SpriteRole::Watchman};
  int index = 0;
  for (std::size_t scan = 0; scan < kRoles.size(); ++scan) {
    if (kRoles[scan] == role) {
      index = static_cast<int>(scan);
      break;
    }
  }
  index = (index + delta + static_cast<int>(kRoles.size())) % static_cast<int>(kRoles.size());
  return kRoles[static_cast<std::size_t>(index)];
}

std::string requirement_type_name(QuestRequirementType type) {
  switch (type) {
    case QuestRequirementType::ReachArea:
      return "GO HERE";
    case QuestRequirementType::TalkToNpc:
      return "TALK";
    case QuestRequirementType::DefeatMonster:
      return "SLAY";
    case QuestRequirementType::CollectItem:
      return "COLLECT";
    default:
      return "CUSTOM";
  }
}

QuestRequirementType next_requirement_type(QuestRequirementType type, int delta) {
  static const std::array<QuestRequirementType, 5> kTypes = {
      QuestRequirementType::ReachArea, QuestRequirementType::TalkToNpc, QuestRequirementType::DefeatMonster,
      QuestRequirementType::CollectItem, QuestRequirementType::Custom};
  int index = 0;
  for (std::size_t scan = 0; scan < kTypes.size(); ++scan) {
    if (kTypes[scan] == type) {
      index = static_cast<int>(scan);
      break;
    }
  }
  index = (index + delta + static_cast<int>(kTypes.size())) % static_cast<int>(kTypes.size());
  return kTypes[static_cast<std::size_t>(index)];
}

int direction_index(Direction direction) {
  switch (direction) {
    case Direction::Up:
      return 0;
    case Direction::Right:
      return 1;
    case Direction::Down:
      return 2;
    case Direction::Left:
      return 3;
    default:
      return 2;
  }
}

Direction direction_from_index(int index) {
  static const std::array<Direction, 4> kDirections = {
      Direction::Up, Direction::Right, Direction::Down, Direction::Left};
  return kDirections[static_cast<std::size_t>(clamp_value(index, 0, static_cast<int>(kDirections.size()) - 1))];
}

int sprite_frame_slot(int direction_index_value, int frame_index) {
  return clamp_value(direction_index_value, 0, kSpriteDirections - 1) * kSpriteFramesPerDirection +
         clamp_value(frame_index, 0, kSpriteFramesPerDirection - 1);
}

std::vector<std::string> blank_sprite_rows(char fill = '0') {
  return std::vector<std::string>(kSpritePixels, std::string(kSpritePixels, fill));
}

SpriteAsset make_sprite_asset(const std::string& id, const std::string& name, bool monster) {
  SpriteAsset asset;
  asset.id = id;
  asset.name = name;
  asset.monster = monster;
  for (auto& frame : asset.frames) {
    frame = blank_sprite_rows();
  }
  return asset;
}

SDL_Color sprite_brush_color(int index) {
  static const std::array<SDL_Color, 8> kColors = {
      SDL_Color{0, 0, 0, 0},
      SDL_Color{35, 30, 34, 255},
      SDL_Color{239, 232, 214, 255},
      SDL_Color{214, 176, 138, 255},
      SDL_Color{120, 80, 54, 255},
      SDL_Color{116, 137, 84, 255},
      SDL_Color{73, 113, 160, 255},
      SDL_Color{188, 78, 64, 255},
  };
  return kColors[static_cast<std::size_t>(clamp_value(index, 0, static_cast<int>(kColors.size()) - 1))];
}

char sprite_brush_symbol(int index) {
  return static_cast<char>('0' + clamp_value(index, 0, 7));
}

int sprite_brush_index_for(char symbol) {
  if (symbol < '0' || symbol > '7') {
    return 0;
  }
  return static_cast<int>(symbol - '0');
}

SpritePalette palette_for(SpriteRole role) {
  switch (role) {
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

void render_character_with_palette(SDL_Renderer* renderer,
                                   const SpritePalette& palette,
                                   SpriteRole role,
                                   Direction facing,
                                   int x,
                                   int y,
                                   bool blink) {
  const SDL_Color shadow = {0, 0, 0, 70};
  fill_rect(renderer, SDL_Rect{x + 4, y + 13, 8, 2}, shadow);
  fill_rect(renderer, SDL_Rect{x + 5, y + 12, 6, 1}, shadow);

  fill_rect(renderer, SDL_Rect{x + 3, y + 2, 10, 2}, palette.outline);
  fill_rect(renderer, SDL_Rect{x + 2, y + 4, 12, 3}, palette.outline);
  fill_rect(renderer, SDL_Rect{x + 3, y + 7, 10, 1}, palette.outline);
  fill_rect(renderer, SDL_Rect{x + 4, y + 4, 8, 3}, palette.skin);

  if (role == SpriteRole::Sister) {
    fill_rect(renderer, SDL_Rect{x + 4, y + 2, 8, 2}, palette.hair);
    fill_rect(renderer, SDL_Rect{x + 3, y + 4, 2, 4}, palette.secondary);
    fill_rect(renderer, SDL_Rect{x + 11, y + 4, 2, 4}, palette.secondary);
    fill_rect(renderer, SDL_Rect{x + 5, y + 3, 6, 1}, palette.secondary);
  } else {
    fill_rect(renderer, SDL_Rect{x + 4, y + 2, 8, 2}, palette.hair);
    fill_rect(renderer, SDL_Rect{x + 3, y + 4, 10, 1}, palette.hair);
  }

  if (facing == Direction::Up) {
    fill_rect(renderer, SDL_Rect{x + 4, y + 5, 8, 2}, palette.hair);
  } else if (facing == Direction::Down) {
    if (blink) {
      fill_rect(renderer, SDL_Rect{x + 5, y + 6, 2, 1}, palette.outline);
      fill_rect(renderer, SDL_Rect{x + 8, y + 6, 2, 1}, palette.outline);
    } else {
      draw_pixel(renderer, x + 6, y + 5, palette.outline);
      draw_pixel(renderer, x + 9, y + 5, palette.outline);
    }
  } else if (facing == Direction::Left) {
    if (blink) {
      fill_rect(renderer, SDL_Rect{x + 4, y + 6, 2, 1}, palette.outline);
    } else {
      draw_pixel(renderer, x + 5, y + 5, palette.outline);
    }
  } else if (facing == Direction::Right) {
    if (blink) {
      fill_rect(renderer, SDL_Rect{x + 10, y + 6, 2, 1}, palette.outline);
    } else {
      draw_pixel(renderer, x + 10, y + 5, palette.outline);
    }
  }

  fill_rect(renderer, SDL_Rect{x + 4, y + 7, 8, 1}, palette.accent);
  fill_rect(renderer, SDL_Rect{x + 3, y + 8, 10, 3}, palette.primary);
  fill_rect(renderer, SDL_Rect{x + 4, y + 11, 8, 1}, palette.secondary);
  fill_rect(renderer, SDL_Rect{x + 2, y + 8, 2, 4}, palette.primary);
  fill_rect(renderer, SDL_Rect{x + 12, y + 8, 2, 4}, palette.primary);
  fill_rect(renderer, SDL_Rect{x + 4, y + 12, 2, 3}, palette.boots);
  fill_rect(renderer, SDL_Rect{x + 10, y + 12, 2, 3}, palette.boots);
}

const std::vector<TileDefinition>& tile_definitions() {
  static const std::vector<TileDefinition> kTiles = {
      {kGrass, "GRASS", "OPEN GROUND"},
      {kFlower, "FLOWER", "GRASS WITH PETALS"},
      {kPath, "PATH", "DIRT TRACK OR ROAD"},
      {kStone, "STONE", "COURT OR STREET PAVING"},
      {kWater, "WATER", "POND, QUAY, OR CHANNEL"},
      {kTree, "TREE", "TREE CROWN AND TRUNK"},
      {kRoof, "ROOF", "BUILDING ROOF TILE"},
      {kWall, "WALL", "EXTERIOR WALL"},
      {kDoor, "DOOR", "DOORWAY TILE"},
      {kWindow, "WINDOW", "SMALL WINDOW"},
      {kGlass, "GLASS", "TALL GLASS WINDOW"},
      {kWood, "WOOD", "WOODEN DECK OR DOCK"},
      {kFloor, "FLOOR", "INDOOR FLOOR"},
      {kPew, "PEW", "CHAPEL FURNITURE"},
      {kAltar, "ALTAR", "ALTAR TILE"},
      {kDesk, "DESK", "WRITING DESK"},
      {kShelf, "SHELF", "BOOKS OR SUPPLIES"},
      {kCandle, "CANDLE", "CANDLE OR LAMP"},
      {kGarden, "GARDEN", "CULTIVATED BED"},
      {kFountain, "FOUNTAIN", "COURTYARD FOUNTAIN"},
      {kStall, "STALL", "MARKET STALL"},
      {kHerb, "HERB", "HERB PATCH"},
      {kCrate, "CRATE", "CRATE STACK"},
      {kBoard, "BOARD", "TASK BOARD"},
      {kRug, "RUG", "INTERIOR RUG"},
      {kReed, "REED", "WATER REEDS"},
      {kFence, "FENCE", "WOOD FENCE"},
      {kBench, "BENCH", "BENCH"},
      {kBarrel, "BARREL", "BARREL"},
      {kLaundry, "LAUNDRY", "CLOTH LINE"},
      {kCartRail, "CART RAIL", "CART BODY"},
      {kCartStep, "CART STEP", "BOARDING STEP"},
      {kCartWheel, "CART WHEEL", "WHEEL TILE"},
      {kHarness, "HARNESS", "HARNESS OR SHAFT"},
      {kMuleBack, "MULE BACK", "PACK ANIMAL REAR"},
      {kMuleFore, "MULE FORE", "PACK ANIMAL FRONT"},
  };
  return kTiles;
}

const TileDefinition* tile_definition(char symbol) {
  for (const auto& tile : tile_definitions()) {
    if (tile.symbol == symbol) {
      return &tile;
    }
  }
  return nullptr;
}

void draw_tile(SDL_Renderer* renderer, char tile, int tile_x, int tile_y, int screen_x, int screen_y, Uint32 now) {
  const SDL_Rect rect = {screen_x, screen_y, kNativeTileSize, kNativeTileSize};
  const int noise = hash_xy(tile_x, tile_y, static_cast<int>(now / 320U));

  switch (tile) {
    case kEmptyTile:
      break;
    case kGrass:
      fill_rect(renderer, rect, SDL_Color{96, 156, 92, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{76, 131, 72, 255});
      draw_pixel(renderer, screen_x + 2 + (noise & 1), screen_y + 4, SDL_Color{141, 194, 123, 255});
      draw_pixel(renderer, screen_x + 5, screen_y + 10, SDL_Color{59, 113, 61, 255});
      draw_pixel(renderer, screen_x + 11, screen_y + 6 + ((noise >> 2) & 1), SDL_Color{141, 194, 123, 255});
      break;
    case kFlower:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      draw_pixel(renderer, screen_x + 5, screen_y + 6, SDL_Color{244, 196, 206, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 7, SDL_Color{255, 236, 130, 255});
      draw_pixel(renderer, screen_x + 10, screen_y + 9, SDL_Color{238, 144, 170, 255});
      break;
    case kPath:
      fill_rect(renderer, rect, SDL_Color{191, 160, 109, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{172, 142, 97, 255});
      draw_pixel(renderer, screen_x + 3, screen_y + 4, SDL_Color{219, 194, 146, 255});
      draw_pixel(renderer, screen_x + 11, screen_y + 4, SDL_Color{149, 120, 76, 255});
      draw_pixel(renderer, screen_x + 8, screen_y + 10, SDL_Color{219, 194, 146, 255});
      break;
    case kStone:
      fill_rect(renderer, rect, SDL_Color{188, 181, 166, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{168, 160, 145, 255});
      for (int py = 4; py <= 12; py += 4) {
        fill_rect(renderer, SDL_Rect{screen_x, screen_y + py, kNativeTileSize, 1}, SDL_Color{145, 138, 124, 255});
      }
      for (int px = 4; px <= 12; px += 4) {
        fill_rect(renderer, SDL_Rect{screen_x + px, screen_y, 1, kNativeTileSize}, SDL_Color{158, 150, 135, 255});
      }
      break;
    case kWater:
      fill_rect(renderer, rect, SDL_Color{64, 123, 189, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{45, 95, 154, 255});
      for (int row = 3; row <= 12; row += 4) {
        const int wave = (static_cast<int>(now / 150U) + tile_x + row) % 4;
        fill_rect(renderer, SDL_Rect{screen_x + 2 + wave, screen_y + row, 7, 1}, SDL_Color{122, 188, 235, 255});
        fill_rect(renderer, SDL_Rect{screen_x + 9 - wave / 2, screen_y + row + 1, 5, 1}, SDL_Color{43, 86, 148, 255});
      }
      break;
    case kTree:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 8}, SDL_Color{50, 96, 52, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 3, 10, 6}, SDL_Color{87, 141, 80, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 10, 2, 4}, SDL_Color{110, 74, 45, 255});
      break;
    case kRoof:
      fill_rect(renderer, rect, SDL_Color{145, 74, 72, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y, kNativeTileSize, 2}, SDL_Color{177, 96, 93, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 13, kNativeTileSize, 3}, SDL_Color{104, 50, 48, 255});
      break;
    case kWall:
      fill_rect(renderer, rect, SDL_Color{220, 210, 188, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{184, 171, 145, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 5, 10, 1}, SDL_Color{201, 190, 168, 255});
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
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{102, 73, 47, 255});
      break;
    case kFloor:
      fill_rect(renderer, rect, SDL_Color{168, 134, 101, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{143, 112, 82, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 7, kNativeTileSize, 1}, SDL_Color{132, 102, 77, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y, 1, kNativeTileSize}, SDL_Color{132, 102, 77, 255});
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
      break;
    case kCandle:
      fill_rect(renderer, rect, SDL_Color{167, 149, 126, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 4, 4, 8}, SDL_Color{242, 229, 202, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 2 + ((now / 140U) % 2U), SDL_Color{255, 209, 96, 255}, 2);
      break;
    case kGarden:
      fill_rect(renderer, rect, SDL_Color{94, 114, 67, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{74, 86, 52, 255});
      break;
    case kFountain:
      fill_rect(renderer, rect, SDL_Color{170, 170, 177, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 1, 14, 14}, SDL_Color{134, 134, 142, 255});
      fill_circle(renderer, screen_x + 8, screen_y + 8, 5, SDL_Color{104, 159, 216, 255});
      break;
    case kStall:
      fill_rect(renderer, rect, SDL_Color{123, 86, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 3, 14, 3}, SDL_Color{191, 98, 74, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 8, 14, 6}, SDL_Color{224, 210, 161, 255});
      break;
    case kHerb:
      fill_rect(renderer, rect, SDL_Color{109, 130, 80, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kNativeTileSize, 4}, SDL_Color{80, 93, 58, 255});
      break;
    case kCrate:
      fill_rect(renderer, rect, SDL_Color{121, 88, 52, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 1, 14, 14}, SDL_Color{85, 59, 37, 255});
      break;
    case kBoard:
      draw_tile(renderer, kStone, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 2, 8, 9}, SDL_Color{124, 84, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 3, 6, 7}, SDL_Color{218, 202, 164, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 11, 2, 4}, SDL_Color{88, 60, 41, 255});
      break;
    case kRug:
      fill_rect(renderer, rect, SDL_Color{127, 63, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 12}, SDL_Color{169, 88, 69, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 12}, SDL_Color{217, 191, 126, 255});
      break;
    case kReed:
      draw_tile(renderer, kWater, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 5, 1, 8}, SDL_Color{184, 197, 102, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 3, 1, 9}, SDL_Color{209, 215, 127, 255});
      break;
    case kFence:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 3, 2, 11}, SDL_Color{112, 78, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 12, screen_y + 3, 2, 11}, SDL_Color{112, 78, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 6, 14, 2}, SDL_Color{163, 121, 77, 255});
      break;
    case kBench:
      draw_tile(renderer, kStone, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 5, 10, 2}, SDL_Color{154, 112, 70, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 8, 12, 2}, SDL_Color{123, 84, 51, 255});
      break;
    case kBarrel:
      draw_tile(renderer, kStone, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 3, 8, 10}, SDL_Color{138, 96, 56, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 2, 6, 2}, SDL_Color{174, 130, 83, 255});
      break;
    case kLaundry:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 2, 1, 12}, SDL_Color{118, 84, 50, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 14, screen_y + 2, 1, 12}, SDL_Color{118, 84, 50, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 4, 4, 6}, SDL_Color{228, 216, 190, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 8, screen_y + 4, 4, 5}, SDL_Color{151, 95, 80, 255});
      break;
    case kCartRail:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 5, 14, 7}, SDL_Color{122, 84, 49, 255});
      break;
    case kCartStep:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 4, 14, 8}, SDL_Color{134, 95, 57, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 10, 8, 3}, SDL_Color{163, 121, 73, 255});
      break;
    case kCartWheel:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 4, 14, 4}, SDL_Color{115, 80, 48, 255});
      fill_circle(renderer, screen_x + 8, screen_y + 11, 4, SDL_Color{90, 62, 39, 255});
      fill_circle(renderer, screen_x + 8, screen_y + 11, 2, SDL_Color{164, 122, 74, 255});
      break;
    case kHarness:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 8, 16, 2}, SDL_Color{104, 79, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 6, 10, 1}, SDL_Color{180, 149, 92, 255});
      break;
    case kMuleBack:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 5, 11, 6}, SDL_Color{122, 96, 76, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 6, 4, 3}, SDL_Color{170, 128, 83, 255});
      break;
    case kMuleFore:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 5, 8, 6}, SDL_Color{122, 96, 76, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 6, 6, 3}, SDL_Color{112, 84, 63, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 10, screen_y + 4, 3, 3}, SDL_Color{94, 72, 58, 255});
      break;
    default:
      fill_rect(renderer, rect, SDL_Color{255, 0, 255, 255});
      break;
  }
}

char tile_at(const AreaData& area, int x, int y) {
  if (x < 0 || y < 0 || y >= area_height(area) || x >= area_width(area)) {
    return fill_tile_for(area);
  }
  return area.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

char wall_tile_at(const AreaData& area, int x, int y) {
  if (x < 0 || y < 0 || y >= static_cast<int>(area.wall_tiles.size())) {
    return kEmptyTile;
  }
  const auto& row = area.wall_tiles[static_cast<std::size_t>(y)];
  if (x >= static_cast<int>(row.size())) {
    return kEmptyTile;
  }
  return row[static_cast<std::size_t>(x)];
}

char visible_tile_at(const AreaData& area, int x, int y) {
  const char wall = wall_tile_at(area, x, y);
  return wall != kEmptyTile ? wall : tile_at(area, x, y);
}

void normalize_tiles(std::vector<std::string>* rows, int width, char fill) {
  if (rows == nullptr) {
    return;
  }
  for (auto& row : *rows) {
    if (static_cast<int>(row.size()) < width) {
      row.append(static_cast<std::size_t>(width - static_cast<int>(row.size())), fill);
    } else if (static_cast<int>(row.size()) > width) {
      row.resize(static_cast<std::size_t>(width));
    }
  }
}

AreaData make_area(std::string id, std::string name, int width, int height, char fill, bool indoor) {
  AreaData area;
  area.id = std::move(id);
  area.name = std::move(name);
  area.indoor = indoor;
  area.tiles.assign(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), fill));
  area.wall_tiles.assign(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), kEmptyTile));
  return area;
}

void set_tile(AreaData* area, int x, int y, char tile) {
  if (area == nullptr || x < 0 || y < 0 || x >= area_width(*area) || y >= area_height(*area)) {
    return;
  }
  area->tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
}

void set_wall_tile(AreaData* area, int x, int y, char tile) {
  if (area == nullptr || x < 0 || y < 0 || x >= area_width(*area) || y >= area_height(*area)) {
    return;
  }
  area->wall_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
}

void set_layer_tile(AreaData* area, PaintSurface surface, int x, int y, char tile) {
  if (surface == PaintSurface::Wall) {
    set_wall_tile(area, x, y, tile);
  } else {
    set_tile(area, x, y, tile);
  }
}

void clear_layer_tile(AreaData* area, PaintSurface surface, int x, int y) {
  if (area == nullptr) {
    return;
  }
  if (surface == PaintSurface::Wall) {
    set_wall_tile(area, x, y, kEmptyTile);
  } else {
    set_tile(area, x, y, fill_tile_for(*area));
  }
}

void fill_region(AreaData* area, int x, int y, int width, int height, char tile) {
  if (area == nullptr) {
    return;
  }
  for (int row = y; row < y + height; ++row) {
    for (int col = x; col < x + width; ++col) {
      set_tile(area, col, row, tile);
    }
  }
}

void stamp_pattern(AreaData* area, int x, int y, const std::vector<std::string>& rows) {
  if (area == nullptr) {
    return;
  }
  for (std::size_t row = 0; row < rows.size(); ++row) {
    for (std::size_t col = 0; col < rows[row].size(); ++col) {
      set_tile(area, x + static_cast<int>(col), y + static_cast<int>(row), rows[row][col]);
    }
  }
}

void stamp_house(AreaData* area, int x, int y, int width, int height) {
  if (area == nullptr || width < 3 || height < 3) {
    return;
  }
  fill_region(area, x, y, width, 2, kRoof);
  fill_region(area, x, y + 2, width, height - 2, kWall);
  for (int row = y + 2; row < y + height; ++row) {
    set_tile(area, x, row, kWall);
    set_tile(area, x + width - 1, row, kWall);
  }
  if (height >= 4) {
    set_tile(area, x + 1, y + 3, kWindow);
    set_tile(area, x + width - 2, y + 3, kWindow);
  }
  const int door_x = x + width / 2;
  set_tile(area, door_x, y + height - 1, kDoor);
  set_tile(area, door_x, y + height, kStone);
}

void ensure_area_size(AreaData* area, int width, int height) {
  if (area == nullptr) {
    return;
  }
  width = std::max(4, width);
  height = std::max(4, height);
  const char fill = fill_tile_for(*area);
  area->tiles.resize(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), fill));
  normalize_tiles(&area->tiles, width, fill);
  area->wall_tiles.resize(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), kEmptyTile));
  normalize_tiles(&area->wall_tiles, width, kEmptyTile);
  area->warps.erase(std::remove_if(area->warps.begin(), area->warps.end(), [width, height](const WarpData& warp) {
    return warp.x < 0 || warp.y < 0 || warp.x >= width || warp.y >= height;
  }), area->warps.end());
  for (auto& warp : area->warps) {
    warp.width = clamp_value(warp.width, 1, std::max(1, width - warp.x));
    warp.height = clamp_value(warp.height, 1, std::max(1, height - warp.y));
  }
  area->npcs.erase(std::remove_if(area->npcs.begin(), area->npcs.end(), [width, height](const NpcData& npc) {
    return npc.x < 0 || npc.y < 0 || npc.x >= width || npc.y >= height;
  }), area->npcs.end());
  area->monsters.erase(std::remove_if(area->monsters.begin(), area->monsters.end(), [width, height](const MonsterData& monster) {
    return monster.x < 0 || monster.y < 0 || monster.x >= width || monster.y >= height;
  }), area->monsters.end());
}

void replace_area_references(ProjectData* project, const std::string& old_id, const std::string& new_id) {
  if (project == nullptr || old_id == new_id) {
    return;
  }
  for (auto& area : project->areas) {
    for (auto& warp : area.warps) {
      if (warp.target_area == old_id) {
        warp.target_area = new_id;
      }
    }
  }
}

void remove_area_references(ProjectData* project, const std::string& removed_id, const std::string& fallback_id) {
  if (project == nullptr) {
    return;
  }
  for (auto& area : project->areas) {
    for (auto& warp : area.warps) {
      if (warp.target_area == removed_id) {
        warp.target_area = fallback_id;
        warp.target_x = 0;
        warp.target_y = 0;
      }
    }
  }
}

AreaData make_house_interior() {
  AreaData area = make_area("house", "FATHER'S HOUSE", 14, 10, kFloor, true);
  fill_region(&area, 0, 0, 14, 1, kWall);
  fill_region(&area, 0, 9, 14, 1, kWall);
  fill_region(&area, 0, 0, 1, 10, kWall);
  fill_region(&area, 13, 0, 1, 10, kWall);
  set_tile(&area, 7, 9, kDoor);
  fill_region(&area, 2, 2, 4, 2, kRug);
  set_tile(&area, 9, 2, kShelf);
  set_tile(&area, 10, 2, kShelf);
  set_tile(&area, 4, 6, kDesk);
  set_tile(&area, 5, 6, kDesk);
  set_tile(&area, 10, 6, kCandle);
  set_tile(&area, 2, 7, kCrate);
  area.warps.push_back({7, 9, 1, 1, "FRONT DOOR", "blackpine_lane", 5, 9, Direction::Down});
  area.npcs.push_back({"father", "Father Aldwyn", SpriteRole::Elder, "", 6, 4, Direction::Down, true,
                       "Saint Catherine will test patience before glory."});
  return area;
}

AreaData make_blackpine_lane() {
  AreaData area = make_area("blackpine_lane", "BLACKPINE LANE", 24, 18, kGrass, false);
  fill_region(&area, 0, 10, 24, 4, kPath);
  fill_region(&area, 3, 8, 5, 2, kPath);
  stamp_house(&area, 2, 3, 7, 5);
  fill_region(&area, 1, 15, 5, 2, kFlower);
  fill_region(&area, 12, 3, 2, 3, kTree);
  fill_region(&area, 20, 3, 2, 3, kTree);
  stamp_pattern(&area, 15, 10, {"jmu", "ynz"});
  fill_region(&area, 8, 2, 2, 2, kFence);
  fill_region(&area, 8, 5, 2, 2, kFence);
  area.warps.push_back({5, 9, 1, 1, "HOUSE RETURN", "house", 7, 8, Direction::Up});
  area.warps.push_back({23, 11, 1, 1, "TO SQUARE", "blackpine_square", 1, 11, Direction::Right});
  area.npcs.push_back({"driver", "Cart Driver Tomas", SpriteRole::Merchant, "", 14, 10, Direction::Left, true,
                       "The cart leaves when you climb aboard."});
  return area;
}

AreaData make_blackpine_square() {
  AreaData area = make_area("blackpine_square", "BLACKPINE SQUARE", 28, 18, kGrass, false);
  fill_region(&area, 2, 3, 24, 11, kStone);
  fill_region(&area, 0, 10, 28, 3, kPath);
  fill_region(&area, 12, 0, 4, 18, kPath);
  set_tile(&area, 14, 8, kFountain);
  set_tile(&area, 11, 8, kBench);
  set_tile(&area, 17, 8, kBench);
  fill_region(&area, 5, 4, 2, 2, kStall);
  fill_region(&area, 20, 4, 2, 2, kStall);
  stamp_house(&area, 2, 0, 6, 4);
  stamp_house(&area, 20, 0, 6, 4);
  fill_region(&area, 4, 15, 3, 2, kFlower);
  fill_region(&area, 20, 15, 3, 2, kLaundry);
  set_tile(&area, 9, 13, kBarrel);
  set_tile(&area, 18, 13, kCrate);
  area.warps.push_back({0, 11, 1, 1, "TO LANE", "blackpine_lane", 22, 11, Direction::Left});
  area.warps.push_back({14, 0, 1, 1, "TO PRIORY ROAD", "priory_road", 12, 16, Direction::Up});
  area.warps.push_back({27, 11, 1, 1, "TO HOSPICE", "saint_hilda_hospice", 1, 9, Direction::Right});
  area.npcs.push_back({"alisoun", "Alisoun Clothier", SpriteRole::Merchant, "", 6, 6, Direction::Down, true,
                       "Blackpine cloth keeps pilgrims respectable."});
  area.npcs.push_back({"clerk", "Market Clerk", SpriteRole::Monk, "", 18, 6, Direction::Left, true,
                       "Petitions and prices travel faster than weather."});
  return area;
}

AreaData make_priory_road() {
  AreaData area = make_area("priory_road", "PRIORY ROAD", 24, 18, kGrass, false);
  fill_region(&area, 10, 0, 4, 18, kPath);
  fill_region(&area, 0, 7, 24, 2, kPath);
  fill_region(&area, 2, 2, 3, 3, kTree);
  fill_region(&area, 18, 2, 3, 3, kTree);
  fill_region(&area, 3, 12, 3, 3, kHerb);
  fill_region(&area, 17, 12, 3, 3, kGarden);
  set_tile(&area, 8, 8, kFence);
  set_tile(&area, 15, 8, kFence);
  area.warps.push_back({12, 17, 1, 1, "TO SQUARE", "blackpine_square", 14, 1, Direction::Down});
  area.warps.push_back({12, 0, 1, 1, "TO GATE", "priory_gate", 10, 13, Direction::Up});
  return area;
}

AreaData make_priory_gate() {
  AreaData area = make_area("priory_gate", "PRIORY GATE", 22, 16, kGrass, false);
  fill_region(&area, 9, 0, 4, 16, kPath);
  fill_region(&area, 5, 3, 12, 3, kRoof);
  fill_region(&area, 5, 6, 12, 4, kWall);
  set_tile(&area, 10, 9, kDoor);
  set_tile(&area, 11, 9, kDoor);
  fill_region(&area, 6, 6, 2, 2, kWindow);
  fill_region(&area, 14, 6, 2, 2, kWindow);
  fill_region(&area, 3, 11, 2, 2, kTree);
  fill_region(&area, 17, 11, 2, 2, kTree);
  area.warps.push_back({10, 15, 1, 1, "TO ROAD", "priory_road", 12, 1, Direction::Down});
  area.warps.push_back({10, 9, 1, 1, "ENTER PRIORY", "priory_courtyard", 12, 16, Direction::Up});
  area.warps.push_back({11, 9, 1, 1, "ENTER PRIORY", "priory_courtyard", 13, 16, Direction::Up});
  area.npcs.push_back({"gate_friar", "Gate Friar", SpriteRole::Monk, "", 8, 10, Direction::Right, true,
                       "The gate is open to those who come ready for rule and labor."});
  return area;
}

AreaData make_priory_courtyard() {
  AreaData area = make_area("priory_courtyard", "SAINT CATHERINE COURTYARD", 26, 18, kGrass, false);
  fill_region(&area, 4, 2, 18, 12, kStone);
  fill_region(&area, 11, 12, 4, 6, kStone);
  fill_region(&area, 6, 1, 14, 3, kRoof);
  fill_region(&area, 6, 4, 14, 5, kWall);
  set_tile(&area, 12, 8, kDoor);
  set_tile(&area, 13, 8, kDoor);
  fill_region(&area, 8, 5, 2, 2, kGlass);
  fill_region(&area, 16, 5, 2, 2, kGlass);
  set_tile(&area, 9, 11, kBoard);
  set_tile(&area, 15, 11, kFountain);
  set_tile(&area, 7, 14, kBench);
  set_tile(&area, 17, 14, kBench);
  fill_region(&area, 2, 14, 3, 2, kGarden);
  fill_region(&area, 21, 14, 3, 2, kHerb);
  area.warps.push_back({12, 17, 1, 1, "TO GATE", "priory_gate", 10, 10, Direction::Down});
  area.npcs.push_back({"martin", "Brother Martin", SpriteRole::Monk, "", 9, 10, Direction::Down, true,
                       "The task board tells the truth before rumor does."});
  area.npcs.push_back({"prior", "Prior Conrad", SpriteRole::Prior, "", 13, 10, Direction::Left, true,
                       "Saint Catherine must be rebuilt in stone and discipline alike."});
  return area;
}

AreaData make_hospice() {
  AreaData area = make_area("saint_hilda_hospice", "SAINT HILDA HOSPICE", 22, 16, kGrass, false);
  fill_region(&area, 0, 8, 22, 3, kPath);
  stamp_house(&area, 7, 2, 8, 5);
  fill_region(&area, 2, 2, 3, 3, kHerb);
  fill_region(&area, 17, 2, 3, 3, kGarden);
  set_tile(&area, 5, 10, kBench);
  set_tile(&area, 16, 10, kCrate);
  area.warps.push_back({0, 9, 1, 1, "TO SQUARE", "blackpine_square", 26, 11, Direction::Left});
  area.npcs.push_back({"prioress", "Prioress Agnes", SpriteRole::Sister, "", 11, 8, Direction::Down, true,
                       "The hospice keeps roads human when politics do not."});
  return area;
}

std::vector<StampData> default_stamps() {
  return {
      {"cart_mule", "CART AND MULE", {"jmu", "ynz"}},
      {"market_stall", "MARKET STALL", {"sss", "sxs"}},
      {"cottage_front", "COTTAGE FRONT", {"^^^^^", "%%%%%", "%w+w%", ":::::"}},
      {"notice_corner", "NOTICE CORNER", {"bp", "ab"}},
  };
}

SpriteAsset make_seed_sprite(const std::string& id,
                             const std::string& name,
                             bool monster,
                             const std::vector<std::string>& down_frame_a,
                             const std::vector<std::string>& down_frame_b,
                             const std::vector<std::string>& side_frame_a,
                             const std::vector<std::string>& side_frame_b,
                             const std::vector<std::string>& up_frame_a,
                             const std::vector<std::string>& up_frame_b) {
  SpriteAsset sprite = make_sprite_asset(id, name, monster);
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(0, 0))] = up_frame_a;
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(0, 1))] = up_frame_b;
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(1, 0))] = side_frame_a;
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(1, 1))] = side_frame_b;
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(2, 0))] = down_frame_a;
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(2, 1))] = down_frame_b;
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(3, 0))] = side_frame_a;
  sprite.frames[static_cast<std::size_t>(sprite_frame_slot(3, 1))] = side_frame_b;
  normalize_sprite_asset(&sprite);
  return sprite;
}

SpriteAsset make_scene_starter_sprite(const std::string& id, const std::string& name, bool monster) {
  if (monster) {
    return make_seed_sprite(
        id,
        name,
        true,
        {"0000000000000000", "0000001111000000", "0000014444100000", "0000143223410000",
         "0001432222234100", "0001432772234100", "0000147777741000", "0000155555551000",
         "0001555515555100", "0001500000055100", "0000550000550000", "0005050005050000",
         "0000500000500000", "0000000000000000", "0000000000000000", "0000000000000000"},
        {"0000000000000000", "0000001111000000", "0000014444100000", "0000143223410000",
         "0001432222234100", "0001432772234100", "0000147777741000", "0000155555551000",
         "0001555515555100", "0001500000055100", "0000055005000000", "0000550000550000",
         "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
        {"0000000000000000", "0000000111000000", "0000001444410000", "0000014322234100",
         "0000143222223410", "0000143777773410", "0000017777777100", "0000155551555510",
         "0001555550155510", "0000150000005510", "0000055000550000", "0000505005050000",
         "0005000500500000", "0000000000000000", "0000000000000000", "0000000000000000"},
        {"0000000000000000", "0000000111000000", "0000001444410000", "0000014322234100",
         "0000143222223410", "0000143777773410", "0000017777777100", "0000155551555510",
         "0001555550155510", "0000150000005510", "0000005505000000", "0000055000550000",
         "0000500000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
        {"0000000000000000", "0000001111000000", "0000013333100000", "0000132222310000",
         "0001322222231000", "0001322222231000", "0000125552100000", "0000015551000000",
         "0000155555100000", "0001556615510000", "0000156006510000", "0000050005000000",
         "0005500000550000", "0000000000000000", "0000000000000000", "0000000000000000"},
        {"0000000000000000", "0000001111000000", "0000013333100000", "0000132222310000",
         "0001322222231000", "0001322222231000", "0000125552100000", "0000015551000000",
         "0000155555100000", "0001556615510000", "0000015001500000", "0000150000510000",
         "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"});
  }
  return make_seed_sprite(
      id,
      name,
      false,
      {"0000000000000000", "0000001111000000", "0000014444100000", "0000132222310000",
       "0001322222231000", "0000135555310000", "0000135555310000", "0000016661000000",
       "0000156666510000", "0001556615510000", "0000150065100000", "0000050005000000",
       "0005500000550000", "0000000000000000", "0000000000000000", "0000000000000000"},
      {"0000000000000000", "0000001111000000", "0000014444100000", "0000132222310000",
       "0001322222231000", "0000135555310000", "0000135555310000", "0000016661000000",
       "0000156666510000", "0001556615510000", "0000015001500000", "0000150000510000",
       "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
      {"0000000000000000", "0000000111000000", "0000001444410000", "0000013222231000",
       "0000132222223100", "0000013555531000", "0000013555531000", "0000016666610000",
       "0000155666155100", "0000015566150000", "0000005000500000", "0000050000050000",
       "0000500000005000", "0000000000000000", "0000000000000000", "0000000000000000"},
      {"0000000000000000", "0000000111000000", "0000001444410000", "0000013222231000",
       "0000132222223100", "0000013555531000", "0000013555531000", "0000016666610000",
       "0000155666155100", "0000015566150000", "0000000505000000", "0000005000500000",
       "0000050000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
      {"0000000000000000", "0000001111000000", "0000014444100000", "0000132222310000",
       "0001322222231000", "0001325552231000", "0000125552100000", "0000016661000000",
       "0000156666510000", "0001556615510000", "0000155005510000", "0000050005000000",
       "0005500000550000", "0000000000000000", "0000000000000000", "0000000000000000"},
      {"0000000000000000", "0000001111000000", "0000014444100000", "0000132222310000",
       "0001322222231000", "0001325552231000", "0000125552100000", "0000016661000000",
       "0000156666510000", "0001556615510000", "0000015001500000", "0000150000510000",
       "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"});
}

std::vector<SpriteAsset> default_sprites() {
  return {
      make_seed_sprite(
          "ratling",
          "Ratling",
          true,
          {"0000000000000000", "0000001111000000", "0000013333100000", "0000132222310000",
           "0001322222231000", "0001327772231000", "0000127772100000", "0000015551000000",
           "0000155555100000", "0001555515510000", "0000150055100000", "0000050005000000",
           "0005500000550000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000001111000000", "0000013333100000", "0000132222310000",
           "0001322222231000", "0001327772231000", "0000127772100000", "0000015551000000",
           "0000155555100000", "0001555515510000", "0000015001500000", "0000150000510000",
           "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000000111000000", "0000001332310000", "0000013222231000",
           "0000132227771000", "0000132227771000", "0000012222210000", "0000015555510000",
           "0000155551551000", "0000015555150000", "0000005000500000", "0000050000050000",
           "0000500000005000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000000111000000", "0000001332310000", "0000013222231000",
           "0000132227771000", "0000132227771000", "0000012222210000", "0000015555510000",
           "0000155551551000", "0000015555150000", "0000000505000000", "0000005000500000",
           "0000050000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000001111000000", "0000013333100000", "0000132222310000",
           "0001322222231000", "0001322222231000", "0000122772100000", "0000012771000000",
           "0000155555100000", "0001555515510000", "0000155005510000", "0000050005000000",
           "0005500000550000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000001111000000", "0000013333100000", "0000132222310000",
           "0001322222231000", "0001322222231000", "0000122772100000", "0000012771000000",
           "0000155555100000", "0001555515510000", "0000015001500000", "0000150000510000",
           "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"}),
      make_seed_sprite(
          "bog_wisp",
          "Bog Wisp",
          true,
          {"0000000000000000", "0000000066000000", "0000006677660000", "0000067777776000",
           "0000677666677600", "0006776666667760", "0006776666667760", "0000677666677600",
           "0000067777776000", "0000006677660000", "0000000666600000", "0000000060000000",
           "0000000606000000", "0000006000600000", "0000060000060000", "0000000000000000"},
          {"0000000000000000", "0000000066000000", "0000006677660000", "0000067777776000",
           "0000677666677600", "0006776666667760", "0000677666677600", "0000067777776000",
           "0000006677660000", "0000000666600000", "0000006000060000", "0000060000600000",
           "0000600000060000", "0006000000006000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000000066000000", "0000006677660000", "0000067777776000",
           "0000677666677600", "0006776666667760", "0006776666667760", "0000677666677600",
           "0000067777776000", "0000006677660000", "0000000666600000", "0000000060000000",
           "0000000606000000", "0000006000600000", "0000060000060000", "0000000000000000"},
          {"0000000000000000", "0000000066000000", "0000006677660000", "0000067777776000",
           "0000677666677600", "0006776666667760", "0000677666677600", "0000067777776000",
           "0000006677660000", "0000000666600000", "0000006000060000", "0000060000600000",
           "0000600000060000", "0006000000006000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000000066000000", "0000006677660000", "0000067777776000",
           "0000677666677600", "0006776666667760", "0006776666667760", "0000677666677600",
           "0000067777776000", "0000006677660000", "0000000666600000", "0000000060000000",
           "0000000606000000", "0000006000600000", "0000060000060000", "0000000000000000"},
          {"0000000000000000", "0000000066000000", "0000006677660000", "0000067777776000",
           "0000677666677600", "0006776666667760", "0000677666677600", "0000067777776000",
           "0000006677660000", "0000000666600000", "0000006000060000", "0000060000600000",
           "0000600000060000", "0006000000006000", "0000000000000000", "0000000000000000"}),
      make_seed_sprite(
          "dock_hound",
          "Dock Hound",
          true,
          {"0000000000000000", "0000001111000000", "0000014444100000", "0000143223410000",
           "0001432222234100", "0001432222234100", "0000145555541000", "0000155555551000",
           "0001555515555100", "0001500000055100", "0000550000550000", "0005050005050000",
           "0000500000500000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000001111000000", "0000014444100000", "0000143223410000",
           "0001432222234100", "0001432222234100", "0000145555541000", "0000155555551000",
           "0001555515555100", "0001500000055100", "0000055005000000", "0000550000550000",
           "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000000111000000", "0000001444410000", "0000014322234100",
           "0000143222223410", "0000143555555410", "0000015555555100", "0000155551555510",
           "0001555550155510", "0000150000005510", "0000055000550000", "0000505005050000",
           "0005000500500000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000000111000000", "0000001444410000", "0000014322234100",
           "0000143222223410", "0000143555555410", "0000015555555100", "0000155551555510",
           "0001555550155510", "0000150000005510", "0000005505000000", "0000055000550000",
           "0000500000050000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000001111000000", "0000014444100000", "0000143223410000",
           "0001432222234100", "0001432222234100", "0000145555541000", "0000155555551000",
           "0001555515555100", "0001500000055100", "0000550000550000", "0005050005050000",
           "0000500000500000", "0000000000000000", "0000000000000000", "0000000000000000"},
          {"0000000000000000", "0000001111000000", "0000014444100000", "0000143223410000",
           "0001432222234100", "0001432222234100", "0000145555541000", "0000155555551000",
           "0001555515555100", "0001500000055100", "0000055005000000", "0000550000550000",
           "0005000000050000", "0000000000000000", "0000000000000000", "0000000000000000"}),
  };
}

std::vector<QuestData> default_quests() {
  return {
      {"arrival_to_saint_catherine",
       "Arrival To Saint Catherine",
       "Leave your father's house, cross Blackpine, and enter the priory court.",
       "father",
       "Go first to the cart. Tomas will take you toward Saint Catherine.",
       "You reached Saint Catherine and crossed the first threshold.",
       {{"leave_house", "Leave the house and step onto Blackpine Lane."},
        {"board_cart", "Reach Tomas and the cart on the lane."},
        {"cross_square", "Pass through Blackpine Square toward Priory Road."},
        {"enter_gate", "Step through Saint Catherine's gate."}},
       {{"reach_lane", QuestRequirementType::ReachArea, "", "blackpine_lane", 5, 9, 1,
         "Reach Blackpine Lane outside your father's house."},
        {"talk_tomas", QuestRequirementType::TalkToNpc, "driver", "blackpine_lane", 14, 10, 1,
         "Speak to Tomas and board the cart."}},
       {{"sealed_letter", 1, 5, ""}}},
      {"task_board_docket",
       "The Rebuild Docket",
       "Brother Martin points you toward the task board and the labor behind the priory's recovery.",
       "martin",
       "Read the board and learn where Saint Catherine is breaking first.",
       "The docket is clear. Now the work can start.",
       {{"meet_martin", "Speak with Brother Martin in the courtyard."},
        {"read_board", "Read the task board and note Saint Catherine's needs."}},
       {{"meet_martin", QuestRequirementType::TalkToNpc, "martin", "priory_courtyard", 9, 10, 1,
         "Speak with Brother Martin in the court."},
        {"inspect_board", QuestRequirementType::ReachArea, "", "priory_courtyard", 9, 11, 1,
         "Stand before the task board."}},
       {{"parchment_bundle", 1, 0, ""}}},
      {"hospice_supplies",
       "Hospice Supply Run",
       "The Sisters of Saint Hilda need a clear route and steady stores.",
       "prioress",
       "Walk the hospice road and note what the sisters lack.",
       "The hospice route and needs are now logged.",
       {{"reach_hospice", "Walk to Saint Hilda Hospice from Blackpine Square."},
        {"review_supply", "Review the hospice corner and note the needed goods."}},
       {{"reach_hospice", QuestRequirementType::ReachArea, "", "saint_hilda_hospice", 11, 8, 1,
         "Reach Saint Hilda Hospice."},
        {"speak_prioress", QuestRequirementType::TalkToNpc, "prioress", "saint_hilda_hospice", 11, 8, 1,
         "Speak with the prioress about the route."}},
       {{"healing_herbs", 1, 2, ""}}},
  };
}

ProjectData make_seed_project() {
  ProjectData project;
  project.name = "Saint Catherine Arrival";
  project.areas = {
      make_house_interior(),
      make_blackpine_lane(),
      make_blackpine_square(),
      make_priory_road(),
      make_priory_gate(),
      make_priory_courtyard(),
      make_hospice(),
  };
  project.quests = default_quests();
  project.stamps = default_stamps();
  project.sprites = default_sprites();
  return project;
}

const AreaData* current_area(const EditorState& state) {
  if (state.project.areas.empty()) {
    return nullptr;
  }
  const int index = clamp_value(state.area_index, 0, static_cast<int>(state.project.areas.size()) - 1);
  return &state.project.areas[static_cast<std::size_t>(index)];
}

AreaData* current_area(EditorState* state) {
  if (state == nullptr || state->project.areas.empty()) {
    return nullptr;
  }
  state->area_index = clamp_value(state->area_index, 0, static_cast<int>(state->project.areas.size()) - 1);
  return &state->project.areas[static_cast<std::size_t>(state->area_index)];
}

char selected_tile_for(const EditorState& state) {
  return state.paint_surface == PaintSurface::Wall ? state.selected_wall_tile : state.selected_ground_tile;
}

void select_tile_for_surface(EditorState* state, char tile) {
  if (state == nullptr) {
    return;
  }
  if (state->paint_surface == PaintSurface::Wall) {
    state->selected_wall_tile = tile;
  } else {
    state->selected_ground_tile = tile;
  }
}

MonsterData* current_monster(EditorState* state) {
  AreaData* area = current_area(state);
  if (area == nullptr || state == nullptr || state->monster_index < 0 || area->monsters.empty()) {
    return nullptr;
  }
  state->monster_index = clamp_value(state->monster_index, 0, static_cast<int>(area->monsters.size()) - 1);
  return &area->monsters[static_cast<std::size_t>(state->monster_index)];
}

const MonsterData* current_monster(const EditorState& state) {
  const AreaData* area = current_area(state);
  if (area == nullptr || state.monster_index < 0 || area->monsters.empty()) {
    return nullptr;
  }
  const int index = clamp_value(state.monster_index, 0, static_cast<int>(area->monsters.size()) - 1);
  return &area->monsters[static_cast<std::size_t>(index)];
}

QuestData* current_quest(EditorState* state) {
  if (state == nullptr || state->project.quests.empty()) {
    return nullptr;
  }
  state->quest_index = clamp_value(state->quest_index, 0, static_cast<int>(state->project.quests.size()) - 1);
  return &state->project.quests[static_cast<std::size_t>(state->quest_index)];
}

const QuestData* current_quest(const EditorState& state) {
  if (state.project.quests.empty()) {
    return nullptr;
  }
  const int index = clamp_value(state.quest_index, 0, static_cast<int>(state.project.quests.size()) - 1);
  return &state.project.quests[static_cast<std::size_t>(index)];
}

QuestRequirement* current_requirement(EditorState* state) {
  QuestData* quest = current_quest(state);
  if (quest == nullptr || quest->requirements.empty()) {
    return nullptr;
  }
  state->requirement_index = clamp_value(state->requirement_index, 0, static_cast<int>(quest->requirements.size()) - 1);
  return &quest->requirements[static_cast<std::size_t>(state->requirement_index)];
}

QuestReward* current_reward(EditorState* state) {
  QuestData* quest = current_quest(state);
  if (quest == nullptr || quest->rewards.empty()) {
    return nullptr;
  }
  state->reward_index = clamp_value(state->reward_index, 0, static_cast<int>(quest->rewards.size()) - 1);
  return &quest->rewards[static_cast<std::size_t>(state->reward_index)];
}

SpriteAsset* current_sprite(EditorState* state) {
  if (state == nullptr || state->project.sprites.empty()) {
    return nullptr;
  }
  state->sprite_index = clamp_value(state->sprite_index, 0, static_cast<int>(state->project.sprites.size()) - 1);
  return &state->project.sprites[static_cast<std::size_t>(state->sprite_index)];
}

const SpriteAsset* current_sprite(const EditorState& state) {
  if (state.project.sprites.empty()) {
    return nullptr;
  }
  const int index = clamp_value(state.sprite_index, 0, static_cast<int>(state.project.sprites.size()) - 1);
  return &state.project.sprites[static_cast<std::size_t>(index)];
}

int find_sprite_asset_index(const ProjectData& project, const std::string& id) {
  if (id.empty()) {
    return -1;
  }
  for (std::size_t index = 0; index < project.sprites.size(); ++index) {
    if (project.sprites[index].id == id) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

SpriteAsset* find_sprite_asset(ProjectData* project, const std::string& id) {
  if (project == nullptr) {
    return nullptr;
  }
  const int index = find_sprite_asset_index(*project, id);
  if (index < 0) {
    return nullptr;
  }
  return &project->sprites[static_cast<std::size_t>(index)];
}

const SpriteAsset* find_sprite_asset(const ProjectData& project, const std::string& id) {
  const int index = find_sprite_asset_index(project, id);
  if (index < 0) {
    return nullptr;
  }
  return &project.sprites[static_cast<std::size_t>(index)];
}

std::string unique_sprite_id(const ProjectData& project, std::string base) {
  if (base.empty()) {
    base = "sprite";
  }
  for (char& ch : base) {
    if (!std::isalnum(static_cast<unsigned char>(ch))) {
      ch = '_';
    } else {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
  }
  std::string candidate = base;
  int suffix = 2;
  while (find_sprite_asset_index(project, candidate) >= 0) {
    candidate = base + "_" + std::to_string(suffix++);
  }
  return candidate;
}

void clear_scene_sprite_target(EditorState* state) {
  if (state == nullptr) {
    return;
  }
  state->scene_sprite_target = SceneSpriteTarget::None;
  state->scene_sprite_area_id.clear();
}

NpcData* current_scene_sprite_npc(EditorState* state) {
  if (state == nullptr || state->scene_sprite_target != SceneSpriteTarget::Npc) {
    return nullptr;
  }
  AreaData* area = current_area(state);
  if (area == nullptr || area->id != state->scene_sprite_area_id) {
    return nullptr;
  }
  return current_npc(state);
}

MonsterData* current_scene_sprite_monster(EditorState* state) {
  if (state == nullptr || state->scene_sprite_target != SceneSpriteTarget::Monster) {
    return nullptr;
  }
  AreaData* area = current_area(state);
  if (area == nullptr || area->id != state->scene_sprite_area_id) {
    return nullptr;
  }
  return current_monster(state);
}

std::string scene_sprite_target_label(const EditorState& state) {
  if (state.scene_sprite_target == SceneSpriteTarget::Npc) {
    const AreaData* area = current_area(state);
    if (area != nullptr && area->id == state.scene_sprite_area_id && state.npc_index >= 0 &&
        state.npc_index < static_cast<int>(area->npcs.size())) {
      return "NPC " + area->npcs[static_cast<std::size_t>(state.npc_index)].name;
    }
  } else if (state.scene_sprite_target == SceneSpriteTarget::Monster) {
    const AreaData* area = current_area(state);
    if (area != nullptr && area->id == state.scene_sprite_area_id && state.monster_index >= 0 &&
        state.monster_index < static_cast<int>(area->monsters.size())) {
      return "MONSTER " + area->monsters[static_cast<std::size_t>(state.monster_index)].name;
    }
  }
  return "NO SCENE TARGET";
}

bool bind_sprite_editor_to_npc(EditorState* state, int npc_index, bool create_if_missing) {
  AreaData* area = current_area(state);
  if (state == nullptr || area == nullptr || npc_index < 0 || npc_index >= static_cast<int>(area->npcs.size())) {
    return false;
  }
  state->npc_index = npc_index;
  state->monster_index = -1;
  state->scene_sprite_target = SceneSpriteTarget::Npc;
  state->scene_sprite_area_id = area->id;
  NpcData& npc = area->npcs[static_cast<std::size_t>(npc_index)];
  int sprite_index = find_sprite_asset_index(state->project, npc.sprite_id);
  if (sprite_index < 0 && create_if_missing) {
    push_undo_snapshot(state);
    npc.sprite_id = unique_sprite_id(state->project, npc.id + "_sprite");
    state->project.sprites.push_back(make_scene_starter_sprite(npc.sprite_id, npc.name + " Sprite", false));
    sprite_index = static_cast<int>(state->project.sprites.size()) - 1;
    state->dirty = true;
  }
  if (sprite_index >= 0) {
    state->sprite_index = sprite_index;
  }
  state->tool = ToolMode::Sprite;
  return sprite_index >= 0;
}

bool bind_sprite_editor_to_monster(EditorState* state, int monster_index, bool create_if_missing) {
  AreaData* area = current_area(state);
  if (state == nullptr || area == nullptr || monster_index < 0 || monster_index >= static_cast<int>(area->monsters.size())) {
    return false;
  }
  state->monster_index = monster_index;
  state->npc_index = -1;
  state->scene_sprite_target = SceneSpriteTarget::Monster;
  state->scene_sprite_area_id = area->id;
  MonsterData& monster = area->monsters[static_cast<std::size_t>(monster_index)];
  int sprite_index = find_sprite_asset_index(state->project, monster.sprite_id);
  if (sprite_index < 0 && create_if_missing) {
    push_undo_snapshot(state);
    monster.sprite_id = unique_sprite_id(state->project, monster.id + "_sprite");
    state->project.sprites.push_back(make_scene_starter_sprite(monster.sprite_id, monster.name + " Sprite", true));
    sprite_index = static_cast<int>(state->project.sprites.size()) - 1;
    state->dirty = true;
  }
  if (sprite_index >= 0) {
    state->sprite_index = sprite_index;
  }
  state->tool = ToolMode::Sprite;
  return sprite_index >= 0;
}

StampData* current_stamp(EditorState* state) {
  if (state == nullptr || state->selected_stamp < 0 || state->project.stamps.empty()) {
    return nullptr;
  }
  state->selected_stamp = clamp_value(state->selected_stamp, 0, static_cast<int>(state->project.stamps.size()) - 1);
  return &state->project.stamps[static_cast<std::size_t>(state->selected_stamp)];
}

const StampData* current_stamp(const EditorState& state) {
  if (state.selected_stamp < 0 || state.project.stamps.empty()) {
    return nullptr;
  }
  const int index = clamp_value(state.selected_stamp, 0, static_cast<int>(state.project.stamps.size()) - 1);
  return &state.project.stamps[static_cast<std::size_t>(index)];
}

WarpData* current_warp(EditorState* state) {
  AreaData* area = current_area(state);
  if (area == nullptr || state == nullptr || state->warp_index < 0 || area->warps.empty()) {
    return nullptr;
  }
  state->warp_index = clamp_value(state->warp_index, 0, static_cast<int>(area->warps.size()) - 1);
  return &area->warps[static_cast<std::size_t>(state->warp_index)];
}

NpcData* current_npc(EditorState* state) {
  AreaData* area = current_area(state);
  if (area == nullptr || state == nullptr || state->npc_index < 0 || area->npcs.empty()) {
    return nullptr;
  }
  state->npc_index = clamp_value(state->npc_index, 0, static_cast<int>(area->npcs.size()) - 1);
  return &area->npcs[static_cast<std::size_t>(state->npc_index)];
}

QuestStage* current_stage(EditorState* state) {
  QuestData* quest = current_quest(state);
  if (quest == nullptr || quest->stages.empty()) {
    return nullptr;
  }
  state->stage_index = clamp_value(state->stage_index, 0, static_cast<int>(quest->stages.size()) - 1);
  return &quest->stages[static_cast<std::size_t>(state->stage_index)];
}

void sanitize_selection(EditorState* state) {
  if (state == nullptr) {
    return;
  }
  if (!state->project.areas.empty()) {
    state->area_index = clamp_value(state->area_index, 0, static_cast<int>(state->project.areas.size()) - 1);
  } else {
    state->area_index = 0;
  }
  if (!state->project.quests.empty()) {
    state->quest_index = clamp_value(state->quest_index, 0, static_cast<int>(state->project.quests.size()) - 1);
  } else {
    state->quest_index = 0;
  }
  if (!state->project.sprites.empty()) {
    state->sprite_index = clamp_value(state->sprite_index, 0, static_cast<int>(state->project.sprites.size()) - 1);
  } else {
    state->sprite_index = 0;
  }
  if (state->selected_stamp >= 0 && !state->project.stamps.empty()) {
    state->selected_stamp = clamp_value(state->selected_stamp, 0, static_cast<int>(state->project.stamps.size()) - 1);
  } else if (state->project.stamps.empty()) {
    state->selected_stamp = -1;
  }

  AreaData* area = current_area(state);
  if (area == nullptr || area->warps.empty()) {
    state->warp_index = -1;
  } else if (state->warp_index >= 0) {
    state->warp_index = clamp_value(state->warp_index, 0, static_cast<int>(area->warps.size()) - 1);
  }
  if (area == nullptr || area->npcs.empty()) {
    state->npc_index = -1;
  } else if (state->npc_index >= 0) {
    state->npc_index = clamp_value(state->npc_index, 0, static_cast<int>(area->npcs.size()) - 1);
  }
  if (area == nullptr || area->monsters.empty()) {
    state->monster_index = -1;
  } else if (state->monster_index >= 0) {
    state->monster_index = clamp_value(state->monster_index, 0, static_cast<int>(area->monsters.size()) - 1);
  }

  QuestData* quest = current_quest(state);
  if (quest == nullptr || quest->stages.empty()) {
    state->stage_index = 0;
  } else {
    state->stage_index = clamp_value(state->stage_index, 0, static_cast<int>(quest->stages.size()) - 1);
  }
  if (quest == nullptr || quest->requirements.empty()) {
    state->requirement_index = 0;
  } else {
    state->requirement_index =
        clamp_value(state->requirement_index, 0, static_cast<int>(quest->requirements.size()) - 1);
  }
  if (quest == nullptr || quest->rewards.empty()) {
    state->reward_index = 0;
  } else {
    state->reward_index = clamp_value(state->reward_index, 0, static_cast<int>(quest->rewards.size()) - 1);
  }
}

EditorSnapshot make_snapshot(const EditorState& state) {
  return {state.project,
          state.area_index,
          state.warp_index,
          state.npc_index,
          state.monster_index,
          state.quest_index,
          state.stage_index,
          state.requirement_index,
          state.reward_index,
          state.sprite_index};
}

void push_undo_snapshot(EditorState* state) {
  if (state == nullptr) {
    return;
  }
  state->undo_stack.push_back(make_snapshot(*state));
  if (state->undo_stack.size() > 96U) {
    state->undo_stack.erase(state->undo_stack.begin());
  }
}

bool undo_last_change(EditorState* state) {
  if (state == nullptr || state->undo_stack.empty()) {
    return false;
  }
  const EditorSnapshot snapshot = std::move(state->undo_stack.back());
  state->undo_stack.pop_back();
  state->project = snapshot.project;
  state->area_index = snapshot.area_index;
  state->warp_index = snapshot.warp_index;
  state->npc_index = snapshot.npc_index;
  state->monster_index = snapshot.monster_index;
  state->quest_index = snapshot.quest_index;
  state->stage_index = snapshot.stage_index;
  state->requirement_index = snapshot.requirement_index;
  state->reward_index = snapshot.reward_index;
  state->sprite_index = snapshot.sprite_index;
  state->dirty = true;
  sanitize_selection(state);
  return true;
}

std::string json_escape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (unsigned char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(static_cast<char>(ch));
        break;
    }
  }
  return escaped;
}

void write_json_indent(std::ostream* output, int indent) {
  for (int index = 0; index < indent; ++index) {
    *output << ' ';
  }
}

JsonValue make_json_null() {
  return {};
}

JsonValue make_json_bool(bool value) {
  JsonValue node;
  node.type = JsonValue::Type::Bool;
  node.bool_value = value;
  return node;
}

JsonValue make_json_number(double value) {
  JsonValue node;
  node.type = JsonValue::Type::Number;
  node.number_value = value;
  return node;
}

JsonValue make_json_string(std::string value) {
  JsonValue node;
  node.type = JsonValue::Type::String;
  node.string_value = std::move(value);
  return node;
}

JsonValue make_json_array(std::vector<JsonValue> value = {}) {
  JsonValue node;
  node.type = JsonValue::Type::Array;
  node.array_value = std::move(value);
  return node;
}

JsonValue make_json_object(std::map<std::string, JsonValue> value = {}) {
  JsonValue node;
  node.type = JsonValue::Type::Object;
  node.object_value = std::move(value);
  return node;
}

void write_json_value(const JsonValue& value, std::ostream* output, int indent);

void write_json_value(const JsonValue& value, std::ostream* output, int indent) {
  switch (value.type) {
    case JsonValue::Type::Null:
      *output << "null";
      return;
    case JsonValue::Type::Bool:
      *output << (value.bool_value ? "true" : "false");
      return;
    case JsonValue::Type::Number: {
      std::ostringstream stream;
      stream << value.number_value;
      *output << stream.str();
      return;
    }
    case JsonValue::Type::String:
      *output << '"' << json_escape(value.string_value) << '"';
      return;
    case JsonValue::Type::Array:
      *output << "[";
      if (!value.array_value.empty()) {
        *output << "\n";
        for (std::size_t index = 0; index < value.array_value.size(); ++index) {
          write_json_indent(output, indent + 2);
          write_json_value(value.array_value[index], output, indent + 2);
          if (index + 1 < value.array_value.size()) {
            *output << ",";
          }
          *output << "\n";
        }
        write_json_indent(output, indent);
      }
      *output << "]";
      return;
    case JsonValue::Type::Object:
      *output << "{";
      if (!value.object_value.empty()) {
        *output << "\n";
        std::size_t index = 0;
        for (const auto& entry : value.object_value) {
          write_json_indent(output, indent + 2);
          *output << '"' << json_escape(entry.first) << "\": ";
          write_json_value(entry.second, output, indent + 2);
          if (index + 1 < value.object_value.size()) {
            *output << ",";
          }
          *output << "\n";
          ++index;
        }
        write_json_indent(output, indent);
      }
      *output << "}";
      return;
  }
}

class JsonParser {
 public:
  explicit JsonParser(std::string_view text) : text_(text) {}

  bool parse(JsonValue* out, std::string* error) {
    if (out == nullptr) {
      if (error != nullptr) {
        *error = "Null output node.";
      }
      return false;
    }
    skip_whitespace();
    if (!parse_value(out, error)) {
      return false;
    }
    skip_whitespace();
    if (pos_ != text_.size()) {
      if (error != nullptr) {
        *error = "Unexpected trailing JSON content.";
      }
      return false;
    }
    return true;
  }

 private:
  bool parse_value(JsonValue* out, std::string* error) {
    if (pos_ >= text_.size()) {
      if (error != nullptr) {
        *error = "Unexpected end of input.";
      }
      return false;
    }
    const char ch = text_[pos_];
    if (ch == '{') {
      return parse_object(out, error);
    }
    if (ch == '[') {
      return parse_array(out, error);
    }
    if (ch == '"') {
      std::string value;
      if (!parse_string(&value, error)) {
        return false;
      }
      *out = make_json_string(std::move(value));
      return true;
    }
    if (ch == 't') {
      return parse_literal("true", make_json_bool(true), out, error);
    }
    if (ch == 'f') {
      return parse_literal("false", make_json_bool(false), out, error);
    }
    if (ch == 'n') {
      return parse_literal("null", make_json_null(), out, error);
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
      return parse_number(out, error);
    }
    if (error != nullptr) {
      *error = "Unexpected JSON token.";
    }
    return false;
  }

  bool parse_literal(std::string_view token, JsonValue value, JsonValue* out, std::string* error) {
    if (text_.substr(pos_, token.size()) != token) {
      if (error != nullptr) {
        *error = "Invalid JSON literal.";
      }
      return false;
    }
    pos_ += token.size();
    *out = std::move(value);
    return true;
  }

  bool parse_number(JsonValue* out, std::string* error) {
    const std::size_t begin = pos_;
    if (text_[pos_] == '-') {
      ++pos_;
    }
    while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
      ++pos_;
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
        ++pos_;
      }
    }
    const std::string token(text_.substr(begin, pos_ - begin));
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (end == nullptr || *end != '\0') {
      if (error != nullptr) {
        *error = "Invalid JSON number.";
      }
      return false;
    }
    *out = make_json_number(value);
    return true;
  }

  bool parse_string(std::string* out, std::string* error);
  bool parse_array(JsonValue* out, std::string* error);
  bool parse_object(JsonValue* out, std::string* error);

  void skip_whitespace() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
      ++pos_;
    }
  }

  std::string_view text_;
  std::size_t pos_ = 0;
};

bool JsonParser::parse_string(std::string* out, std::string* error) {
  if (pos_ >= text_.size() || text_[pos_] != '"') {
    if (error != nullptr) {
      *error = "Expected string.";
    }
    return false;
  }
  ++pos_;
  std::string value;
  while (pos_ < text_.size()) {
    const char ch = text_[pos_++];
    if (ch == '"') {
      *out = std::move(value);
      return true;
    }
    if (ch == '\\') {
      if (pos_ >= text_.size()) {
        break;
      }
      const char escaped = text_[pos_++];
      switch (escaped) {
        case '"':
          value.push_back('"');
          break;
        case '\\':
          value.push_back('\\');
          break;
        case '/':
          value.push_back('/');
          break;
        case 'b':
          value.push_back('\b');
          break;
        case 'f':
          value.push_back('\f');
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          if (error != nullptr) {
            *error = "Unsupported JSON escape.";
          }
          return false;
      }
    } else {
      value.push_back(ch);
    }
  }
  if (error != nullptr) {
    *error = "Unterminated JSON string.";
  }
  return false;
}

bool JsonParser::parse_array(JsonValue* out, std::string* error) {
  ++pos_;
  JsonValue array = make_json_array();
  skip_whitespace();
  if (pos_ < text_.size() && text_[pos_] == ']') {
    ++pos_;
    *out = std::move(array);
    return true;
  }
  while (pos_ < text_.size()) {
    JsonValue item;
    if (!parse_value(&item, error)) {
      return false;
    }
    array.array_value.push_back(std::move(item));
    skip_whitespace();
    if (pos_ < text_.size() && text_[pos_] == ',') {
      ++pos_;
      skip_whitespace();
      continue;
    }
    if (pos_ < text_.size() && text_[pos_] == ']') {
      ++pos_;
      *out = std::move(array);
      return true;
    }
    break;
  }
  if (error != nullptr) {
    *error = "Unterminated JSON array.";
  }
  return false;
}

bool JsonParser::parse_object(JsonValue* out, std::string* error) {
  ++pos_;
  JsonValue object = make_json_object();
  skip_whitespace();
  if (pos_ < text_.size() && text_[pos_] == '}') {
    ++pos_;
    *out = std::move(object);
    return true;
  }
  while (pos_ < text_.size()) {
    std::string key;
    if (!parse_string(&key, error)) {
      return false;
    }
    skip_whitespace();
    if (pos_ >= text_.size() || text_[pos_] != ':') {
      if (error != nullptr) {
        *error = "Expected ':' in object.";
      }
      return false;
    }
    ++pos_;
    skip_whitespace();
    JsonValue value;
    if (!parse_value(&value, error)) {
      return false;
    }
    object.object_value.emplace(std::move(key), std::move(value));
    skip_whitespace();
    if (pos_ < text_.size() && text_[pos_] == ',') {
      ++pos_;
      skip_whitespace();
      continue;
    }
    if (pos_ < text_.size() && text_[pos_] == '}') {
      ++pos_;
      *out = std::move(object);
      return true;
    }
    break;
  }
  if (error != nullptr) {
    *error = "Unterminated JSON object.";
  }
  return false;
}

const JsonValue* object_get(const JsonValue& object, const std::string& key) {
  if (object.type != JsonValue::Type::Object) {
    return nullptr;
  }
  const auto found = object.object_value.find(key);
  return found == object.object_value.end() ? nullptr : &found->second;
}

std::string json_as_string(const JsonValue* node, std::string fallback = "") {
  return node != nullptr && node->type == JsonValue::Type::String ? node->string_value : fallback;
}

int json_as_int(const JsonValue* node, int fallback = 0) {
  return node != nullptr && node->type == JsonValue::Type::Number
             ? static_cast<int>(std::lround(node->number_value))
             : fallback;
}

bool json_as_bool(const JsonValue* node, bool fallback = false) {
  return node != nullptr && node->type == JsonValue::Type::Bool ? node->bool_value : fallback;
}

Direction direction_from_string(const std::string& value) {
  const std::string upper = uppercase(value);
  if (upper == "UP") {
    return Direction::Up;
  }
  if (upper == "DOWN") {
    return Direction::Down;
  }
  if (upper == "LEFT") {
    return Direction::Left;
  }
  if (upper == "RIGHT") {
    return Direction::Right;
  }
  return Direction::Down;
}

SpriteRole role_from_string(const std::string& value) {
  const std::string upper = uppercase(value);
  if (upper == "PRIOR") {
    return SpriteRole::Prior;
  }
  if (upper == "SISTER") {
    return SpriteRole::Sister;
  }
  if (upper == "FISHER") {
    return SpriteRole::Fisher;
  }
  if (upper == "MERCHANT") {
    return SpriteRole::Merchant;
  }
  if (upper == "CHILD") {
    return SpriteRole::Child;
  }
  if (upper == "ELDER") {
    return SpriteRole::Elder;
  }
  if (upper == "WATCHMAN") {
    return SpriteRole::Watchman;
  }
  return SpriteRole::Monk;
}

QuestRequirementType requirement_type_from_string(const std::string& value) {
  const std::string upper = uppercase(value);
  if (upper == "GO HERE" || upper == "REACHAREA" || upper == "REACH_AREA") {
    return QuestRequirementType::ReachArea;
  }
  if (upper == "TALK" || upper == "TALKTONPC" || upper == "TALK_TO_NPC") {
    return QuestRequirementType::TalkToNpc;
  }
  if (upper == "SLAY" || upper == "DEFEATMONSTER" || upper == "DEFEAT_MONSTER") {
    return QuestRequirementType::DefeatMonster;
  }
  if (upper == "COLLECT" || upper == "COLLECTITEM" || upper == "COLLECT_ITEM") {
    return QuestRequirementType::CollectItem;
  }
  return QuestRequirementType::Custom;
}

void normalize_sprite_asset(SpriteAsset* asset) {
  if (asset == nullptr) {
    return;
  }
  for (auto& frame : asset->frames) {
    if (frame.empty()) {
      frame = blank_sprite_rows();
    }
    frame.resize(kSpritePixels, std::string(kSpritePixels, '0'));
    for (auto& row : frame) {
      if (static_cast<int>(row.size()) < kSpritePixels) {
        row.append(static_cast<std::size_t>(kSpritePixels - static_cast<int>(row.size())), '0');
      } else if (static_cast<int>(row.size()) > kSpritePixels) {
        row.resize(kSpritePixels);
      }
      for (char& symbol : row) {
        if (symbol < '0' || symbol > '7') {
          symbol = '0';
        }
      }
    }
  }
}

JsonValue warp_to_json(const WarpData& warp) {
  return make_json_object({
      {"x", make_json_number(warp.x)},
      {"y", make_json_number(warp.y)},
      {"width", make_json_number(warp.width)},
      {"height", make_json_number(warp.height)},
      {"label", make_json_string(warp.label)},
      {"target_area", make_json_string(warp.target_area)},
      {"target_x", make_json_number(warp.target_x)},
      {"target_y", make_json_number(warp.target_y)},
      {"target_facing", make_json_string(direction_name(warp.target_facing))},
  });
}

JsonValue npc_to_json(const NpcData& npc) {
  return make_json_object({
      {"id", make_json_string(npc.id)},
      {"name", make_json_string(npc.name)},
      {"role", make_json_string(sprite_role_name(npc.role))},
      {"sprite_id", make_json_string(npc.sprite_id)},
      {"x", make_json_number(npc.x)},
      {"y", make_json_number(npc.y)},
      {"facing", make_json_string(direction_name(npc.facing))},
      {"solid", make_json_bool(npc.solid)},
      {"dialogue", make_json_string(npc.dialogue)},
  });
}

JsonValue monster_to_json(const MonsterData& monster) {
  return make_json_object({
      {"id", make_json_string(monster.id)},
      {"name", make_json_string(monster.name)},
      {"sprite_id", make_json_string(monster.sprite_id)},
      {"x", make_json_number(monster.x)},
      {"y", make_json_number(monster.y)},
      {"facing", make_json_string(direction_name(monster.facing))},
      {"max_hp", make_json_number(monster.max_hp)},
      {"attack", make_json_number(monster.attack)},
      {"aggressive", make_json_bool(monster.aggressive)},
  });
}

JsonValue stage_to_json(const QuestStage& stage) {
  return make_json_object({
      {"id", make_json_string(stage.id)},
      {"text", make_json_string(stage.text)},
  });
}

JsonValue requirement_to_json(const QuestRequirement& requirement) {
  return make_json_object({
      {"id", make_json_string(requirement.id)},
      {"type", make_json_string(requirement_type_name(requirement.type))},
      {"target_id", make_json_string(requirement.target_id)},
      {"area_id", make_json_string(requirement.area_id)},
      {"x", make_json_number(requirement.x)},
      {"y", make_json_number(requirement.y)},
      {"quantity", make_json_number(requirement.quantity)},
      {"description", make_json_string(requirement.description)},
  });
}

JsonValue reward_to_json(const QuestReward& reward) {
  return make_json_object({
      {"item_id", make_json_string(reward.item_id)},
      {"quantity", make_json_number(reward.quantity)},
      {"coins", make_json_number(reward.coins)},
      {"unlock_sprite_id", make_json_string(reward.unlock_sprite_id)},
  });
}

JsonValue quest_to_json(const QuestData& quest) {
  std::vector<JsonValue> stages;
  std::vector<JsonValue> requirements;
  std::vector<JsonValue> rewards;
  for (const auto& stage : quest.stages) {
    stages.push_back(stage_to_json(stage));
  }
  for (const auto& requirement : quest.requirements) {
    requirements.push_back(requirement_to_json(requirement));
  }
  for (const auto& reward : quest.rewards) {
    rewards.push_back(reward_to_json(reward));
  }
  return make_json_object({
      {"id", make_json_string(quest.id)},
      {"title", make_json_string(quest.title)},
      {"summary", make_json_string(quest.summary)},
      {"quest_giver_id", make_json_string(quest.quest_giver_id)},
      {"start_dialogue", make_json_string(quest.start_dialogue)},
      {"completion_dialogue", make_json_string(quest.completion_dialogue)},
      {"stages", make_json_array(std::move(stages))},
      {"requirements", make_json_array(std::move(requirements))},
      {"rewards", make_json_array(std::move(rewards))},
  });
}

JsonValue stamp_to_json(const StampData& stamp) {
  std::vector<JsonValue> rows;
  for (const auto& row : stamp.tiles) {
    rows.push_back(make_json_string(row));
  }
  return make_json_object({
      {"id", make_json_string(stamp.id)},
      {"name", make_json_string(stamp.name)},
      {"tiles", make_json_array(std::move(rows))},
  });
}

JsonValue sprite_to_json(const SpriteAsset& sprite) {
  std::map<std::string, JsonValue> frames;
  for (int direction_scan = 0; direction_scan < kSpriteDirections; ++direction_scan) {
    for (int frame_scan = 0; frame_scan < kSpriteFramesPerDirection; ++frame_scan) {
      std::vector<JsonValue> rows;
      for (const auto& row : sprite.frames[static_cast<std::size_t>(sprite_frame_slot(direction_scan, frame_scan))]) {
        rows.push_back(make_json_string(row));
      }
      const std::string key = direction_name(direction_from_index(direction_scan)) + "_" + std::to_string(frame_scan + 1);
      frames.emplace(key, make_json_array(std::move(rows)));
    }
  }
  return make_json_object({
      {"id", make_json_string(sprite.id)},
      {"name", make_json_string(sprite.name)},
      {"monster", make_json_bool(sprite.monster)},
      {"frames", make_json_object(std::move(frames))},
  });
}

JsonValue area_to_json(const AreaData& area) {
  std::vector<JsonValue> rows;
  std::vector<JsonValue> wall_rows;
  std::vector<JsonValue> warps;
  std::vector<JsonValue> npcs;
  std::vector<JsonValue> monsters;
  for (const auto& row : area.tiles) {
    rows.push_back(make_json_string(row));
  }
  for (const auto& row : area.wall_tiles) {
    wall_rows.push_back(make_json_string(row));
  }
  for (const auto& warp : area.warps) {
    warps.push_back(warp_to_json(warp));
  }
  for (const auto& npc : area.npcs) {
    npcs.push_back(npc_to_json(npc));
  }
  for (const auto& monster : area.monsters) {
    monsters.push_back(monster_to_json(monster));
  }
  return make_json_object({
      {"id", make_json_string(area.id)},
      {"name", make_json_string(area.name)},
      {"indoor", make_json_bool(area.indoor)},
      {"player_tillable", make_json_bool(area.player_tillable)},
      {"width", make_json_number(area_width(area))},
      {"height", make_json_number(area_height(area))},
      {"tiles", make_json_array(std::move(rows))},
      {"wall_tiles", make_json_array(std::move(wall_rows))},
      {"warps", make_json_array(std::move(warps))},
      {"npcs", make_json_array(std::move(npcs))},
      {"monsters", make_json_array(std::move(monsters))},
  });
}

JsonValue project_to_json(const ProjectData& project) {
  std::vector<JsonValue> areas;
  std::vector<JsonValue> quests;
  std::vector<JsonValue> stamps;
  std::vector<JsonValue> sprites;
  for (const auto& area : project.areas) {
    areas.push_back(area_to_json(area));
  }
  for (const auto& quest : project.quests) {
    quests.push_back(quest_to_json(quest));
  }
  for (const auto& stamp : project.stamps) {
    stamps.push_back(stamp_to_json(stamp));
  }
  for (const auto& sprite : project.sprites) {
    sprites.push_back(sprite_to_json(sprite));
  }
  return make_json_object({
      {"version", make_json_number(1)},
      {"project_name", make_json_string(project.name)},
      {"areas", make_json_array(std::move(areas))},
      {"quests", make_json_array(std::move(quests))},
      {"stamps", make_json_array(std::move(stamps))},
      {"sprites", make_json_array(std::move(sprites))},
  });
}

bool load_stamp_from_json(const JsonValue& node, StampData* stamp) {
  if (stamp == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  stamp->id = json_as_string(object_get(node, "id"), "stamp");
  stamp->name = json_as_string(object_get(node, "name"), "STAMP");
  stamp->tiles.clear();
  const JsonValue* rows = object_get(node, "tiles");
  if (rows != nullptr && rows->type == JsonValue::Type::Array) {
    for (const auto& row : rows->array_value) {
      if (row.type == JsonValue::Type::String) {
        stamp->tiles.push_back(row.string_value);
      }
    }
  }
  if (stamp->tiles.empty()) {
    stamp->tiles.push_back(std::string(1, kGrass));
  }
  normalize_tiles(&stamp->tiles, stamp_width(*stamp), kGrass);
  return true;
}

bool load_area_from_json(const JsonValue& node, AreaData* area) {
  if (area == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  area->id = json_as_string(object_get(node, "id"), "area");
  area->name = json_as_string(object_get(node, "name"), "AREA");
  area->indoor = json_as_bool(object_get(node, "indoor"), false);
  area->player_tillable = json_as_bool(object_get(node, "player_tillable"), false);
  area->tiles.clear();
  area->wall_tiles.clear();
  const JsonValue* rows = object_get(node, "tiles");
  if (rows != nullptr && rows->type == JsonValue::Type::Array) {
    for (const auto& row : rows->array_value) {
      if (row.type == JsonValue::Type::String) {
        area->tiles.push_back(row.string_value);
      }
    }
  }
  const int width_hint = json_as_int(object_get(node, "width"), kDefaultAreaWidth);
  const int height_hint = json_as_int(object_get(node, "height"), kDefaultAreaHeight);
  if (area->tiles.empty()) {
    area->tiles.assign(static_cast<std::size_t>(height_hint),
                       std::string(static_cast<std::size_t>(width_hint), fill_tile_for(*area)));
  }
  normalize_tiles(&area->tiles, std::max(width_hint, area_width(*area)), fill_tile_for(*area));
  const JsonValue* wall_rows = object_get(node, "wall_tiles");
  if (wall_rows != nullptr && wall_rows->type == JsonValue::Type::Array) {
    for (const auto& row : wall_rows->array_value) {
      if (row.type == JsonValue::Type::String) {
        area->wall_tiles.push_back(row.string_value);
      }
    }
  }
  if (area->wall_tiles.empty()) {
    area->wall_tiles.assign(area->tiles.size(), std::string(static_cast<std::size_t>(area_width(*area)), kEmptyTile));
  }
  normalize_tiles(&area->wall_tiles, std::max(width_hint, area_width(*area)), kEmptyTile);
  if (static_cast<int>(area->wall_tiles.size()) < area_height(*area)) {
    area->wall_tiles.resize(area->tiles.size(), std::string(static_cast<std::size_t>(area_width(*area)), kEmptyTile));
  } else if (static_cast<int>(area->wall_tiles.size()) > area_height(*area)) {
    area->wall_tiles.resize(area->tiles.size());
  }

  area->warps.clear();
  const JsonValue* warps = object_get(node, "warps");
  if (warps != nullptr && warps->type == JsonValue::Type::Array) {
    for (const auto& warp_node : warps->array_value) {
      if (warp_node.type != JsonValue::Type::Object) {
        continue;
      }
      WarpData warp;
      warp.x = json_as_int(object_get(warp_node, "x"), 0);
      warp.y = json_as_int(object_get(warp_node, "y"), 0);
      warp.width = json_as_int(object_get(warp_node, "width"), 1);
      warp.height = json_as_int(object_get(warp_node, "height"), 1);
      warp.label = json_as_string(object_get(warp_node, "label"), "WARP");
      warp.target_area = json_as_string(object_get(warp_node, "target_area"), area->id);
      warp.target_x = json_as_int(object_get(warp_node, "target_x"), 0);
      warp.target_y = json_as_int(object_get(warp_node, "target_y"), 0);
      warp.target_facing = direction_from_string(json_as_string(object_get(warp_node, "target_facing"), "DOWN"));
      warp.width = std::max(1, warp.width);
      warp.height = std::max(1, warp.height);
      area->warps.push_back(std::move(warp));
    }
  }

  area->npcs.clear();
  const JsonValue* npcs = object_get(node, "npcs");
  if (npcs != nullptr && npcs->type == JsonValue::Type::Array) {
    for (const auto& npc_node : npcs->array_value) {
      if (npc_node.type != JsonValue::Type::Object) {
        continue;
      }
      NpcData npc;
      npc.id = json_as_string(object_get(npc_node, "id"), "npc");
      npc.name = json_as_string(object_get(npc_node, "name"), "NPC");
      npc.role = role_from_string(json_as_string(object_get(npc_node, "role"), "MONK"));
      npc.sprite_id = json_as_string(object_get(npc_node, "sprite_id"), "");
      npc.x = json_as_int(object_get(npc_node, "x"), 0);
      npc.y = json_as_int(object_get(npc_node, "y"), 0);
      npc.facing = direction_from_string(json_as_string(object_get(npc_node, "facing"), "DOWN"));
      npc.solid = json_as_bool(object_get(npc_node, "solid"), true);
      npc.dialogue = json_as_string(object_get(npc_node, "dialogue"), "");
      area->npcs.push_back(std::move(npc));
    }
  }
  area->monsters.clear();
  const JsonValue* monsters = object_get(node, "monsters");
  if (monsters != nullptr && monsters->type == JsonValue::Type::Array) {
    for (const auto& monster_node : monsters->array_value) {
      if (monster_node.type != JsonValue::Type::Object) {
        continue;
      }
      MonsterData monster;
      monster.id = json_as_string(object_get(monster_node, "id"), "monster");
      monster.name = json_as_string(object_get(monster_node, "name"), "MONSTER");
      monster.sprite_id = json_as_string(object_get(monster_node, "sprite_id"), "ratling");
      monster.x = json_as_int(object_get(monster_node, "x"), 0);
      monster.y = json_as_int(object_get(monster_node, "y"), 0);
      monster.facing = direction_from_string(json_as_string(object_get(monster_node, "facing"), "DOWN"));
      monster.max_hp = std::max(1, json_as_int(object_get(monster_node, "max_hp"), 6));
      monster.attack = std::max(1, json_as_int(object_get(monster_node, "attack"), 1));
      monster.aggressive = json_as_bool(object_get(monster_node, "aggressive"), true);
      area->monsters.push_back(std::move(monster));
    }
  }
  return true;
}

bool load_quest_from_json(const JsonValue& node, QuestData* quest) {
  if (quest == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  quest->id = json_as_string(object_get(node, "id"), "quest");
  quest->title = json_as_string(object_get(node, "title"), "QUEST");
  quest->summary = json_as_string(object_get(node, "summary"), "");
  quest->quest_giver_id = json_as_string(object_get(node, "quest_giver_id"), "");
  quest->start_dialogue = json_as_string(object_get(node, "start_dialogue"), "");
  quest->completion_dialogue = json_as_string(object_get(node, "completion_dialogue"), "");
  quest->stages.clear();
  quest->requirements.clear();
  quest->rewards.clear();
  const JsonValue* stages = object_get(node, "stages");
  if (stages != nullptr && stages->type == JsonValue::Type::Array) {
    for (const auto& stage_node : stages->array_value) {
      if (stage_node.type != JsonValue::Type::Object) {
        continue;
      }
      QuestStage stage;
      stage.id = json_as_string(object_get(stage_node, "id"), "stage");
      stage.text = json_as_string(object_get(stage_node, "text"), "");
      quest->stages.push_back(std::move(stage));
    }
  }
  if (quest->stages.empty()) {
    quest->stages.push_back({"stage_1", "Describe the first stage."});
  }
  const JsonValue* requirements = object_get(node, "requirements");
  if (requirements != nullptr && requirements->type == JsonValue::Type::Array) {
    for (const auto& requirement_node : requirements->array_value) {
      if (requirement_node.type != JsonValue::Type::Object) {
        continue;
      }
      QuestRequirement requirement;
      requirement.id = json_as_string(object_get(requirement_node, "id"), "requirement");
      requirement.type = requirement_type_from_string(json_as_string(object_get(requirement_node, "type"), "CUSTOM"));
      requirement.target_id = json_as_string(object_get(requirement_node, "target_id"), "");
      requirement.area_id = json_as_string(object_get(requirement_node, "area_id"), "");
      requirement.x = json_as_int(object_get(requirement_node, "x"), 0);
      requirement.y = json_as_int(object_get(requirement_node, "y"), 0);
      requirement.quantity = std::max(1, json_as_int(object_get(requirement_node, "quantity"), 1));
      requirement.description = json_as_string(object_get(requirement_node, "description"), "");
      quest->requirements.push_back(std::move(requirement));
    }
  }
  const JsonValue* rewards = object_get(node, "rewards");
  if (rewards != nullptr && rewards->type == JsonValue::Type::Array) {
    for (const auto& reward_node : rewards->array_value) {
      if (reward_node.type != JsonValue::Type::Object) {
        continue;
      }
      QuestReward reward;
      reward.item_id = json_as_string(object_get(reward_node, "item_id"), "");
      reward.quantity = std::max(1, json_as_int(object_get(reward_node, "quantity"), 1));
      reward.coins = std::max(0, json_as_int(object_get(reward_node, "coins"), 0));
      reward.unlock_sprite_id = json_as_string(object_get(reward_node, "unlock_sprite_id"), "");
      quest->rewards.push_back(std::move(reward));
    }
  }
  return true;
}

bool load_sprite_from_json(const JsonValue& node, SpriteAsset* sprite) {
  if (sprite == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  *sprite = make_sprite_asset(json_as_string(object_get(node, "id"), "sprite"),
                              json_as_string(object_get(node, "name"), "SPRITE"),
                              json_as_bool(object_get(node, "monster"), false));
  const JsonValue* frames = object_get(node, "frames");
  if (frames != nullptr && frames->type == JsonValue::Type::Object) {
    for (int direction_scan = 0; direction_scan < kSpriteDirections; ++direction_scan) {
      for (int frame_scan = 0; frame_scan < kSpriteFramesPerDirection; ++frame_scan) {
        const std::string key = direction_name(direction_from_index(direction_scan)) + "_" + std::to_string(frame_scan + 1);
        const JsonValue* rows = object_get(*frames, key);
        if (rows == nullptr || rows->type != JsonValue::Type::Array) {
          continue;
        }
        std::vector<std::string> parsed_rows;
        for (const auto& row : rows->array_value) {
          if (row.type == JsonValue::Type::String) {
            parsed_rows.push_back(row.string_value);
          }
        }
        sprite->frames[static_cast<std::size_t>(sprite_frame_slot(direction_scan, frame_scan))] = std::move(parsed_rows);
      }
    }
  }
  normalize_sprite_asset(sprite);
  return true;
}

bool project_from_json(const JsonValue& node, ProjectData* project) {
  if (project == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  project->name = json_as_string(object_get(node, "project_name"), "Priory Project");
  project->areas.clear();
  project->quests.clear();
  project->stamps.clear();
  project->sprites.clear();

  const JsonValue* areas = object_get(node, "areas");
  if (areas != nullptr && areas->type == JsonValue::Type::Array) {
    for (const auto& area_node : areas->array_value) {
      AreaData area;
      if (load_area_from_json(area_node, &area)) {
        project->areas.push_back(std::move(area));
      }
    }
  }

  const JsonValue* quests = object_get(node, "quests");
  if (quests != nullptr && quests->type == JsonValue::Type::Array) {
    for (const auto& quest_node : quests->array_value) {
      QuestData quest;
      if (load_quest_from_json(quest_node, &quest)) {
        project->quests.push_back(std::move(quest));
      }
    }
  }

  const JsonValue* stamps = object_get(node, "stamps");
  if (stamps != nullptr && stamps->type == JsonValue::Type::Array) {
    for (const auto& stamp_node : stamps->array_value) {
      StampData stamp;
      if (load_stamp_from_json(stamp_node, &stamp)) {
        project->stamps.push_back(std::move(stamp));
      }
    }
  }
  const JsonValue* sprites = object_get(node, "sprites");
  if (sprites != nullptr && sprites->type == JsonValue::Type::Array) {
    for (const auto& sprite_node : sprites->array_value) {
      SpriteAsset sprite;
      if (load_sprite_from_json(sprite_node, &sprite)) {
        project->sprites.push_back(std::move(sprite));
      }
    }
  }

  if (project->areas.empty()) {
    project->areas = make_seed_project().areas;
  }
  if (project->quests.empty()) {
    project->quests = default_quests();
  }
  if (project->sprites.empty()) {
    project->sprites = make_seed_project().sprites;
  }
  return true;
}

bool save_project(const ProjectData& project, const std::filesystem::path& path, std::string* error) {
  std::error_code ec;
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
  }

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    if (error != nullptr) {
      *error = "Failed to open project file for writing.";
    }
    return false;
  }

  const JsonValue root = project_to_json(project);
  write_json_value(root, &output, 0);
  output << "\n";
  if (!output.good()) {
    if (error != nullptr) {
      *error = "Failed while writing project JSON.";
    }
    return false;
  }
  return true;
}

bool load_project(ProjectData* project, const std::filesystem::path& path, std::string* error) {
  if (project == nullptr) {
    if (error != nullptr) {
      *error = "Null project target.";
    }
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (error != nullptr) {
      *error = "Project file not found.";
    }
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string text = buffer.str();
  JsonValue root;
  JsonParser parser(text);
  if (!parser.parse(&root, error)) {
    return false;
  }
  if (!project_from_json(root, project)) {
    if (error != nullptr) {
      *error = "Project JSON shape is invalid.";
    }
    return false;
  }
  return true;
}

int find_warp_at(const AreaData& area, int x, int y) {
  for (std::size_t index = 0; index < area.warps.size(); ++index) {
    const auto& warp = area.warps[index];
    if (x >= warp.x && x < warp.x + std::max(1, warp.width) && y >= warp.y && y < warp.y + std::max(1, warp.height)) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

int find_npc_at(const AreaData& area, int x, int y) {
  for (std::size_t index = 0; index < area.npcs.size(); ++index) {
    if (area.npcs[index].x == x && area.npcs[index].y == y) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

int find_monster_at(const AreaData& area, int x, int y) {
  for (std::size_t index = 0; index < area.monsters.size(); ++index) {
    if (area.monsters[index].x == x && area.monsters[index].y == y) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

bool run_smoke_test() {
  ProjectData project = make_seed_project();
  if (project.areas.size() < 6 || project.quests.size() < 3 || project.stamps.empty()) {
    std::fprintf(stderr, "Seed project missing required content.\n");
    return false;
  }

  project.areas[1].warps.push_back({4, 4, 1, 1, "HOUSE DOOR", "house", 7, 8, Direction::Up});
  project.areas[1].player_tillable = true;
  set_wall_tile(&project.areas[1], 6, 10, kBench);
  set_wall_tile(&project.areas[1], 7, 10, kDesk);
  project.areas[2].npcs.push_back({"watch", "Town Watch", SpriteRole::Watchman, "", 10, 10, Direction::Left, true,
                                   "Stay clear of the guilds if you want peace."});
  project.areas[2].monsters.push_back({"rat_test", "Test Rat", "ratling", 12, 10, Direction::Left, 4, 1, true});
  project.quests[0].stages.push_back({"task_board", "Reach Brother Martin and review the board."});
  project.stamps.push_back({"test_stamp", "TEST STAMP", {"::", ";;"}});

  const std::filesystem::path smoke_path = std::filesystem::path("build") / "priory-editor-smoke.json";
  std::string error;
  if (!save_project(project, smoke_path, &error)) {
    std::fprintf(stderr, "Save failed: %s\n", error.c_str());
    return false;
  }

  ProjectData loaded;
  if (!load_project(&loaded, smoke_path, &error)) {
    std::fprintf(stderr, "Load failed: %s\n", error.c_str());
    return false;
  }
  if (loaded.areas.size() != project.areas.size()) {
    std::fprintf(stderr, "Area count mismatch after round trip.\n");
    return false;
  }
  if (loaded.quests.size() != project.quests.size()) {
    std::fprintf(stderr, "Quest count mismatch after round trip.\n");
    return false;
  }
  if (loaded.stamps.size() != project.stamps.size()) {
    std::fprintf(stderr, "Stamp count mismatch after round trip.\n");
    return false;
  }
  if (find_warp_at(loaded.areas[1], 4, 4) < 0) {
    std::fprintf(stderr, "Warp missing after round trip.\n");
    return false;
  }
  if (!loaded.areas[1].player_tillable) {
    std::fprintf(stderr, "Tillable area flag missing after round trip.\n");
    return false;
  }
  if (wall_tile_at(loaded.areas[1], 6, 10) != kBench || wall_tile_at(loaded.areas[1], 7, 10) != kDesk) {
    std::fprintf(stderr, "Wall tile layer missing after round trip.\n");
    return false;
  }
  if (find_npc_at(loaded.areas[2], 10, 10) < 0) {
    std::fprintf(stderr, "NPC missing after round trip.\n");
    return false;
  }

  EditorState state;
  state.project = loaded;
  state.area_index = 2;
  sanitize_selection(&state);
  if (!bind_sprite_editor_to_npc(&state, 0, true)) {
    std::fprintf(stderr, "Failed to bind NPC sprite editor target.\n");
    return false;
  }
  NpcData* bound_npc = current_scene_sprite_npc(&state);
  if (bound_npc == nullptr || bound_npc->sprite_id.empty() || find_sprite_asset(state.project, bound_npc->sprite_id) == nullptr) {
    std::fprintf(stderr, "NPC sprite binding did not produce a real sprite asset.\n");
    return false;
  }
  if (!bind_sprite_editor_to_monster(&state, 0, true)) {
    std::fprintf(stderr, "Failed to bind monster sprite editor target.\n");
    return false;
  }
  MonsterData* bound_monster = current_scene_sprite_monster(&state);
  if (bound_monster == nullptr || bound_monster->sprite_id.empty() ||
      find_sprite_asset(state.project, bound_monster->sprite_id) == nullptr) {
    std::fprintf(stderr, "Monster sprite binding did not produce a real sprite asset.\n");
    return false;
  }

  std::puts("PRIORY EDITOR SMOKE TEST OK");
  return true;
}

std::string* focused_text_target(EditorState* state) {
  if (state == nullptr) {
    return nullptr;
  }
  if (state->focus_id == "project_name") {
    return &state->project.name;
  }
  if (state->focus_id == "project_path") {
    return &state->project_path;
  }
  if (state->focus_id == "stamp_name") {
    return &state->pending_stamp_name;
  }
  if (state->focus_id == "new_area_id") {
    return &state->new_area_id;
  }
  if (state->focus_id == "new_area_name") {
    return &state->new_area_name;
  }
  AreaData* area = current_area(state);
  if (area != nullptr) {
    if (state->focus_id == "area_id") {
      return &area->id;
    }
    if (state->focus_id == "area_name") {
      return &area->name;
    }
  }
  WarpData* warp = current_warp(state);
  if (warp != nullptr && state->focus_id == "warp_label") {
    return &warp->label;
  }
  NpcData* npc = current_npc(state);
  if (npc != nullptr) {
    if (state->focus_id == "npc_id") {
      return &npc->id;
    }
    if (state->focus_id == "npc_name") {
      return &npc->name;
    }
    if (state->focus_id == "npc_dialogue") {
      return &npc->dialogue;
    }
    if (state->focus_id == "npc_sprite_id") {
      return &npc->sprite_id;
    }
  }
  MonsterData* monster = current_monster(state);
  if (monster != nullptr) {
    if (state->focus_id == "monster_id") {
      return &monster->id;
    }
    if (state->focus_id == "monster_name") {
      return &monster->name;
    }
    if (state->focus_id == "monster_sprite_id") {
      return &monster->sprite_id;
    }
  }
  QuestData* quest = current_quest(state);
  if (quest != nullptr) {
    if (state->focus_id == "quest_id") {
      return &quest->id;
    }
    if (state->focus_id == "quest_title") {
      return &quest->title;
    }
    if (state->focus_id == "quest_summary") {
      return &quest->summary;
    }
    if (state->focus_id == "quest_giver_id") {
      return &quest->quest_giver_id;
    }
    if (state->focus_id == "quest_start_dialogue") {
      return &quest->start_dialogue;
    }
    if (state->focus_id == "quest_completion_dialogue") {
      return &quest->completion_dialogue;
    }
  }
  QuestStage* stage = current_stage(state);
  if (stage != nullptr) {
    if (state->focus_id == "stage_id") {
      return &stage->id;
    }
    if (state->focus_id == "stage_text") {
      return &stage->text;
    }
  }
  QuestRequirement* requirement = current_requirement(state);
  if (requirement != nullptr) {
    if (state->focus_id == "requirement_id") {
      return &requirement->id;
    }
    if (state->focus_id == "requirement_target_id") {
      return &requirement->target_id;
    }
    if (state->focus_id == "requirement_area_id") {
      return &requirement->area_id;
    }
    if (state->focus_id == "requirement_description") {
      return &requirement->description;
    }
  }
  QuestReward* reward = current_reward(state);
  if (reward != nullptr) {
    if (state->focus_id == "reward_item_id") {
      return &reward->item_id;
    }
    if (state->focus_id == "reward_sprite_id") {
      return &reward->unlock_sprite_id;
    }
  }
  StampData* stamp = current_stamp(state);
  if (stamp != nullptr) {
    if (state->focus_id == "stamp_selected_id") {
      return &stamp->id;
    }
    if (state->focus_id == "stamp_selected_name") {
      return &stamp->name;
    }
  }
  SpriteAsset* sprite = current_sprite(state);
  if (sprite != nullptr) {
    if (state->focus_id == "sprite_id") {
      return &sprite->id;
    }
    if (state->focus_id == "sprite_name") {
      return &sprite->name;
    }
  }
  return nullptr;
}

bool focus_is_multiline(const EditorState& state) {
  return state.focus_id == "npc_dialogue" || state.focus_id == "quest_summary" || state.focus_id == "stage_text" ||
         state.focus_id == "quest_start_dialogue" || state.focus_id == "quest_completion_dialogue" ||
         state.focus_id == "requirement_description";
}

void append_text(EditorState* state, const char* text) {
  if (state == nullptr || text == nullptr) {
    return;
  }
  std::string* target = focused_text_target(state);
  if (target == nullptr) {
    return;
  }
  push_undo_snapshot(state);
  target->append(text);
  state->dirty = true;
}

void erase_last_text_character(EditorState* state) {
  if (state == nullptr) {
    return;
  }
  std::string* target = focused_text_target(state);
  if (target == nullptr || target->empty()) {
    return;
  }
  push_undo_snapshot(state);
  target->pop_back();
  state->dirty = true;
}

void set_status(EditorState* state, std::string message) {
  if (state == nullptr) {
    return;
  }
  state->status_message = std::move(message);
}

bool button(SDL_Renderer* renderer,
            const InputFrame& input,
            UiFrame* ui,
            const SDL_Rect& rect,
            const std::string& label,
            bool active = false,
            bool disabled = false) {
  const bool hovered = point_in_rect(rect, input.mouse_x, input.mouse_y);
  const bool pressed = !disabled && hovered && input.left_pressed && ui != nullptr && !ui->mouse_consumed;
  if (pressed && ui != nullptr) {
    ui->mouse_consumed = true;
  }

  SDL_Color fill = active ? SDL_Color{113, 137, 84, 255} : SDL_Color{232, 224, 206, 255};
  SDL_Color border = active ? SDL_Color{69, 91, 52, 255} : SDL_Color{120, 109, 89, 255};
  SDL_Color text = active ? SDL_Color{245, 245, 231, 255} : SDL_Color{58, 56, 50, 255};
  if (hovered && !disabled) {
    fill = lighten(fill, 10);
  }
  if (disabled) {
    fill = SDL_Color{212, 208, 198, 255};
    border = SDL_Color{151, 145, 132, 255};
    text = SDL_Color{132, 126, 116, 255};
  }

  fill_rect(renderer, rect, fill);
  draw_rect(renderer, rect, border);
  draw_text_box(renderer, label, SDL_Rect{rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 8},
                TextBoxOptions{1, 1, false, 1, TextAlign::Center, TextVerticalAlign::Middle}, text);
  return pressed;
}

void section_card(SDL_Renderer* renderer, const SDL_Rect& rect, const std::string& title) {
  fill_rect(renderer, rect, SDL_Color{247, 241, 228, 255});
  draw_rect(renderer, rect, SDL_Color{123, 111, 88, 255});
  fill_rect(renderer, SDL_Rect{rect.x, rect.y, rect.w, 18}, SDL_Color{222, 214, 198, 255});
  draw_text(renderer, title, rect.x + 8, rect.y + 5, 1, SDL_Color{70, 65, 57, 255});
}

void text_field(SDL_Renderer* renderer,
                const InputFrame& input,
                UiFrame* ui,
                EditorState* state,
                const SDL_Rect& rect,
                const std::string& focus_id,
                const std::string& value,
                const std::string& placeholder,
                bool multiline = false) {
  const bool hovered = point_in_rect(rect, input.mouse_x, input.mouse_y);
  if (hovered && input.left_pressed && ui != nullptr && !ui->mouse_consumed && state != nullptr) {
    state->focus_id = focus_id;
    ui->mouse_consumed = true;
  }
  const bool focused = state != nullptr && state->focus_id == focus_id;
  fill_rect(renderer, rect, focused ? SDL_Color{255, 252, 244, 255} : SDL_Color{240, 235, 224, 255});
  draw_rect(renderer, rect, focused ? SDL_Color{84, 104, 150, 255} : SDL_Color{141, 130, 109, 255});

  const SDL_Rect inner = {rect.x + kTextInset, rect.y + kTextInset, rect.w - kTextInset * 2, rect.h - kTextInset * 2};
  const bool empty = value.empty();
  draw_text_box(renderer,
                empty ? placeholder : value,
                inner,
                TextBoxOptions{multiline ? 1 : 2, 1, multiline, multiline ? 2 : 1, TextAlign::Left,
                               TextVerticalAlign::Top},
                empty ? SDL_Color{150, 144, 131, 255} : SDL_Color{48, 46, 42, 255});
  if (focused) {
    fill_rect(renderer, SDL_Rect{rect.x + rect.w - 8, rect.y + 6, 2, rect.h - 12}, SDL_Color{84, 104, 150, 255});
  }
}

void label(SDL_Renderer* renderer, int x, int y, const std::string& text) {
  draw_text(renderer, text, x, y, 1, SDL_Color{88, 81, 69, 255});
}

void cycle_buttons(SDL_Renderer* renderer,
                   const InputFrame& input,
                   UiFrame* ui,
                   const SDL_Rect& rect,
                   const std::string& value,
                   bool* decrement,
                   bool* increment) {
  if (decrement != nullptr) {
    *decrement = false;
  }
  if (increment != nullptr) {
    *increment = false;
  }
  const SDL_Rect left = {rect.x, rect.y, 24, rect.h};
  const SDL_Rect center = {rect.x + 26, rect.y, rect.w - 52, rect.h};
  const SDL_Rect right = {rect.x + rect.w - 24, rect.y, 24, rect.h};
  if (button(renderer, input, ui, left, "<")) {
    if (decrement != nullptr) {
      *decrement = true;
    }
  }
  fill_rect(renderer, center, SDL_Color{240, 235, 224, 255});
  draw_rect(renderer, center, SDL_Color{141, 130, 109, 255});
  draw_text_box(renderer, value, SDL_Rect{center.x + 4, center.y + 4, center.w - 8, center.h - 8},
                TextBoxOptions{1, 1, false, 1, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{48, 46, 42, 255});
  if (button(renderer, input, ui, right, ">")) {
    if (increment != nullptr) {
      *increment = true;
    }
  }
}

void clamp_camera(EditorState* state, const SDL_Rect& canvas) {
  const AreaData* area = current_area(*state);
  if (area == nullptr) {
    state->camera_x = 0;
    state->camera_y = 0;
    return;
  }
  const int native_width = area_width(*area) * kNativeTileSize;
  const int native_height = area_height(*area) * kNativeTileSize;
  const int visible_width = std::max(1, static_cast<int>(std::floor(canvas.w / std::max(0.25f, state->map_zoom))));
  const int visible_height = std::max(1, static_cast<int>(std::floor(canvas.h / std::max(0.25f, state->map_zoom))));
  state->camera_x = clamp_value(state->camera_x, 0, std::max(0, native_width - visible_width));
  state->camera_y = clamp_value(state->camera_y, 0, std::max(0, native_height - visible_height));
}

SDL_Rect map_source_rect(const EditorState& state, const SDL_Rect& canvas) {
  const AreaData* area = current_area(state);
  if (area == nullptr) {
    return {0, 0, 1, 1};
  }
  const int native_width = area_width(*area) * kNativeTileSize;
  const int native_height = area_height(*area) * kNativeTileSize;
  SDL_Rect src = {state.camera_x,
                  state.camera_y,
                  std::max(1, static_cast<int>(std::floor(canvas.w / std::max(0.25f, state.map_zoom)))),
                  std::max(1, static_cast<int>(std::floor(canvas.h / std::max(0.25f, state.map_zoom))))};
  src.w = std::min(src.w, native_width);
  src.h = std::min(src.h, native_height);
  src.x = clamp_value(src.x, 0, std::max(0, native_width - src.w));
  src.y = clamp_value(src.y, 0, std::max(0, native_height - src.h));
  return src;
}

struct MapViewport {
  SDL_Rect src{};
  SDL_Rect dest{};
  double scale_x = 1.0;
  double scale_y = 1.0;
};

MapViewport map_viewport(const EditorState& state, const SDL_Rect& canvas) {
  MapViewport view;
  view.src = map_source_rect(state, canvas);
  view.dest = canvas;
  view.scale_x = static_cast<double>(view.dest.w) / std::max(1, view.src.w);
  view.scale_y = static_cast<double>(view.dest.h) / std::max(1, view.src.h);
  return view;
}

bool map_tile_from_mouse(const EditorState& state,
                         const SDL_Rect& canvas,
                         int mouse_x,
                         int mouse_y,
                         int* tile_x,
                         int* tile_y) {
  const AreaData* area = current_area(state);
  if (area == nullptr || !point_in_rect(canvas, mouse_x, mouse_y)) {
    return false;
  }
  const MapViewport view = map_viewport(state, canvas);
  if (!point_in_rect(view.dest, mouse_x, mouse_y)) {
    return false;
  }
  const int local_x = mouse_x - view.dest.x;
  const int local_y = mouse_y - view.dest.y;
  const int world_x = view.src.x + static_cast<int>(std::floor(local_x / std::max(0.0001, view.scale_x)));
  const int world_y = view.src.y + static_cast<int>(std::floor(local_y / std::max(0.0001, view.scale_y)));
  const int tx = world_x / kNativeTileSize;
  const int ty = world_y / kNativeTileSize;
  if (tx < 0 || ty < 0 || tx >= area_width(*area) || ty >= area_height(*area)) {
    return false;
  }
  if (tile_x != nullptr) {
    *tile_x = tx;
  }
  if (tile_y != nullptr) {
    *tile_y = ty;
  }
  return true;
}

void draw_pattern_preview(SDL_Renderer* renderer,
                          const std::vector<std::string>& rows,
                          const SDL_Rect& dest,
                          Uint32 now) {
  const int width = rows.empty() ? 1 : static_cast<int>(rows.front().size());
  const int height = rows.empty() ? 1 : static_cast<int>(rows.size());
  SDL_Texture* texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           std::max(1, width * kNativeTileSize),
                                           std::max(1, height * kNativeTileSize));
  if (texture == nullptr) {
    return;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetRenderTarget(renderer, texture);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      draw_tile(renderer, rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)], x, y,
                x * kNativeTileSize, y * kNativeTileSize, now);
    }
  }
  SDL_SetRenderTarget(renderer, nullptr);
  SDL_RenderCopy(renderer, texture, nullptr, &dest);
  SDL_DestroyTexture(texture);
}

void draw_sprite_asset_preview(SDL_Renderer* renderer,
                               const SpriteAsset& sprite,
                               int direction_index_value,
                               int frame_index,
                               const SDL_Rect& rect) {
  fill_rect(renderer, rect, SDL_Color{245, 240, 229, 255});
  draw_rect(renderer, rect, SDL_Color{125, 115, 95, 255});
  const auto& rows = sprite.frames[static_cast<std::size_t>(sprite_frame_slot(direction_index_value, frame_index))];
  const int pixel_size = std::max(1, std::min((rect.w - 8) / kSpritePixels, (rect.h - 8) / kSpritePixels));
  const int draw_w = pixel_size * kSpritePixels;
  const int draw_h = pixel_size * kSpritePixels;
  const int origin_x = rect.x + (rect.w - draw_w) / 2;
  const int origin_y = rect.y + (rect.h - draw_h) / 2;
  for (int y = 0; y < kSpritePixels && y < static_cast<int>(rows.size()); ++y) {
    for (int x = 0; x < kSpritePixels && x < static_cast<int>(rows[static_cast<std::size_t>(y)].size()); ++x) {
      const int brush = sprite_brush_index_for(rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]);
      if (brush == 0) {
        continue;
      }
      fill_rect(renderer, SDL_Rect{origin_x + x * pixel_size, origin_y + y * pixel_size, pixel_size, pixel_size},
                sprite_brush_color(brush));
    }
  }
}

void render_sprite_asset_frame(SDL_Renderer* renderer,
                               const SpriteAsset& sprite,
                               Direction facing,
                               int frame_index,
                               int screen_x,
                               int screen_y) {
  const auto& rows = sprite.frames[static_cast<std::size_t>(sprite_frame_slot(direction_index(facing), frame_index))];
  for (int y = 0; y < kSpritePixels && y < static_cast<int>(rows.size()); ++y) {
    for (int x = 0; x < kSpritePixels && x < static_cast<int>(rows[static_cast<std::size_t>(y)].size()); ++x) {
      const int brush = sprite_brush_index_for(rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]);
      if (brush == 0) {
        continue;
      }
      fill_rect(renderer, SDL_Rect{screen_x + x, screen_y + y, 1, 1}, sprite_brush_color(brush));
    }
  }
}

void render_area_to_texture(SDL_Renderer* renderer,
                            const EditorState& state,
                            const AreaData& area,
                            SDL_Texture* texture) {
  SDL_SetRenderTarget(renderer, texture);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  for (int y = 0; y < area_height(area); ++y) {
    for (int x = 0; x < area_width(area); ++x) {
      draw_tile(renderer, tile_at(area, x, y), x, y, x * kNativeTileSize, y * kNativeTileSize, state.now);
      const char wall = wall_tile_at(area, x, y);
      if (wall != kEmptyTile) {
        draw_tile(renderer, wall, x, y, x * kNativeTileSize, y * kNativeTileSize, state.now);
      }
    }
  }
  if (state.show_overlays) {
    for (std::size_t index = 0; index < area.warps.size(); ++index) {
      const auto& warp = area.warps[index];
      const SDL_Rect frame = {warp.x * kNativeTileSize + 1,
                              warp.y * kNativeTileSize + 1,
                              std::max(1, warp.width * kNativeTileSize - 2),
                              std::max(1, warp.height * kNativeTileSize - 2)};
      const SDL_Color color = state.warp_index == static_cast<int>(index)
                                  ? SDL_Color{88, 204, 232, 255}
                                  : SDL_Color{73, 154, 183, 230};
      draw_rect(renderer, frame, color);
      draw_text(renderer, "W" + std::to_string(static_cast<int>(index) + 1), frame.x + 3, frame.y + 4, 1, color);
    }
    for (std::size_t index = 0; index < area.npcs.size(); ++index) {
      const auto& npc = area.npcs[index];
      const SpriteAsset* sprite = find_sprite_asset(state.project, npc.sprite_id);
      if (sprite != nullptr) {
        const int frame = static_cast<int>(((state.now / 280U) + static_cast<Uint32>(index)) % kSpriteFramesPerDirection);
        render_sprite_asset_frame(renderer, *sprite, npc.facing, frame, npc.x * kNativeTileSize, npc.y * kNativeTileSize);
      } else {
        const bool blink = ((state.now + static_cast<Uint32>(hash_text(npc.id))) % 5200U) < 150U;
        render_character_with_palette(renderer, palette_for(npc.role), npc.role, npc.facing,
                                      npc.x * kNativeTileSize, npc.y * kNativeTileSize, blink);
      }
      if (state.npc_index == static_cast<int>(index)) {
        draw_rect(renderer, SDL_Rect{npc.x * kNativeTileSize, npc.y * kNativeTileSize, kNativeTileSize, kNativeTileSize},
                  SDL_Color{238, 198, 102, 255});
      }
    }
    for (std::size_t index = 0; index < area.monsters.size(); ++index) {
      const auto& monster = area.monsters[index];
      const SpriteAsset* sprite = find_sprite_asset(state.project, monster.sprite_id);
      if (sprite != nullptr) {
        const int frame = static_cast<int>(((state.now / 280U) + static_cast<Uint32>(index)) % kSpriteFramesPerDirection);
        render_sprite_asset_frame(renderer, *sprite, monster.facing, frame,
                                  monster.x * kNativeTileSize, monster.y * kNativeTileSize);
      } else {
        fill_rect(renderer, SDL_Rect{monster.x * kNativeTileSize + 3, monster.y * kNativeTileSize + 4, 10, 8},
                  SDL_Color{108, 60, 52, 255});
        fill_rect(renderer, SDL_Rect{monster.x * kNativeTileSize + 4, monster.y * kNativeTileSize + 5, 8, 6},
                  SDL_Color{188, 78, 64, 255});
        draw_pixel(renderer, monster.x * kNativeTileSize + 5, monster.y * kNativeTileSize + 6, SDL_Color{248, 232, 214, 255});
        draw_pixel(renderer, monster.x * kNativeTileSize + 10, monster.y * kNativeTileSize + 6, SDL_Color{248, 232, 214, 255});
      }
      if (state.monster_index == static_cast<int>(index)) {
        draw_rect(renderer,
                  SDL_Rect{monster.x * kNativeTileSize, monster.y * kNativeTileSize, kNativeTileSize, kNativeTileSize},
                  SDL_Color{214, 102, 76, 255});
      }
    }
  }
  SDL_SetRenderTarget(renderer, nullptr);
}

bool save_selection_as_stamp(EditorState* state) {
  AreaData* area = current_area(state);
  if (state == nullptr || area == nullptr || !state->selection_active) {
    return false;
  }
  const int min_x = std::min(state->selection_anchor.x, state->selection_head.x);
  const int min_y = std::min(state->selection_anchor.y, state->selection_head.y);
  const int max_x = std::max(state->selection_anchor.x, state->selection_head.x);
  const int max_y = std::max(state->selection_anchor.y, state->selection_head.y);
  if (min_x < 0 || min_y < 0 || max_x >= area_width(*area) || max_y >= area_height(*area)) {
    return false;
  }
  StampData stamp;
  stamp.name = state->pending_stamp_name.empty() ? "CUSTOM STAMP" : state->pending_stamp_name;
  stamp.id = uppercase(stamp.name);
  std::replace(stamp.id.begin(), stamp.id.end(), ' ', '_');
  for (int y = min_y; y <= max_y; ++y) {
    std::string row;
    for (int x = min_x; x <= max_x; ++x) {
      row.push_back(state->paint_surface == PaintSurface::Wall ? wall_tile_at(*area, x, y) : tile_at(*area, x, y));
    }
    stamp.tiles.push_back(row);
  }
  state->project.stamps.push_back(std::move(stamp));
  state->selected_stamp = static_cast<int>(state->project.stamps.size()) - 1;
  state->brush_kind = BrushKind::Stamp;
  state->dirty = true;
  return true;
}

void apply_stamp(AreaData* area, PaintSurface surface, const StampData& stamp, int tile_x, int tile_y) {
  if (area == nullptr) {
    return;
  }
  for (int y = 0; y < stamp_height(stamp); ++y) {
    for (int x = 0; x < stamp_width(stamp); ++x) {
      set_layer_tile(area, surface, tile_x + x, tile_y + y,
                     stamp.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]);
    }
  }
}

void fill_area_with_brush(EditorState* state) {
  AreaData* area = current_area(state);
  if (state == nullptr || area == nullptr) {
    return;
  }
  if (state->brush_kind == BrushKind::Tile) {
    for (int y = 0; y < area_height(*area); ++y) {
      for (int x = 0; x < area_width(*area); ++x) {
        set_layer_tile(area, state->paint_surface, x, y, selected_tile_for(*state));
      }
    }
  } else {
    StampData* stamp = current_stamp(state);
    if (stamp == nullptr || stamp_width(*stamp) <= 0 || stamp_height(*stamp) <= 0) {
      return;
    }
    if (state->paint_surface == PaintSurface::Wall) {
      for (auto& row : area->wall_tiles) {
        std::fill(row.begin(), row.end(), kEmptyTile);
      }
    } else {
      const char fill = fill_tile_for(*area);
      for (auto& row : area->tiles) {
        std::fill(row.begin(), row.end(), fill);
      }
    }
    for (int y = 0; y < area_height(*area); y += std::max(1, stamp_height(*stamp))) {
      for (int x = 0; x < area_width(*area); x += std::max(1, stamp_width(*stamp))) {
        apply_stamp(area, state->paint_surface, *stamp, x, y);
      }
    }
  }
  state->dirty = true;
}

void delete_selected_for_tool(EditorState* state) {
  if (state == nullptr) {
    return;
  }
  if (state->tool == ToolMode::Warp) {
    AreaData* area = current_area(state);
    if (area != nullptr && state->warp_index >= 0 && state->warp_index < static_cast<int>(area->warps.size())) {
      push_undo_snapshot(state);
      area->warps.erase(area->warps.begin() + state->warp_index);
      state->warp_index = -1;
      state->dirty = true;
      set_status(state, "WARP REMOVED.");
    }
  } else if (state->tool == ToolMode::Npc) {
    AreaData* area = current_area(state);
    if (area != nullptr && state->npc_index >= 0 && state->npc_index < static_cast<int>(area->npcs.size())) {
      push_undo_snapshot(state);
      area->npcs.erase(area->npcs.begin() + state->npc_index);
      state->npc_index = -1;
      if (state->scene_sprite_target == SceneSpriteTarget::Npc && state->scene_sprite_area_id == area->id) {
        clear_scene_sprite_target(state);
      }
      state->dirty = true;
      set_status(state, "NPC REMOVED.");
    }
  } else if (state->tool == ToolMode::Monster) {
    AreaData* area = current_area(state);
    if (area != nullptr && state->monster_index >= 0 && state->monster_index < static_cast<int>(area->monsters.size())) {
      push_undo_snapshot(state);
      area->monsters.erase(area->monsters.begin() + state->monster_index);
      state->monster_index = -1;
      if (state->scene_sprite_target == SceneSpriteTarget::Monster && state->scene_sprite_area_id == area->id) {
        clear_scene_sprite_target(state);
      }
      state->dirty = true;
      set_status(state, "MONSTER REMOVED.");
    }
  } else if (state->tool == ToolMode::Quest) {
    QuestData* quest = current_quest(state);
    if (quest != nullptr && !quest->stages.empty()) {
      push_undo_snapshot(state);
      const int index = clamp_value(state->stage_index, 0, static_cast<int>(quest->stages.size()) - 1);
      quest->stages.erase(quest->stages.begin() + index);
      if (quest->stages.empty()) {
        quest->stages.push_back({"stage_1", "Describe the first stage."});
      }
      state->stage_index = 0;
      state->dirty = true;
      set_status(state, "QUEST STAGE REMOVED.");
    }
  } else if (state->tool == ToolMode::Sprite) {
    if (state->project.sprites.size() > 1 && state->sprite_index >= 0 &&
        state->sprite_index < static_cast<int>(state->project.sprites.size())) {
      push_undo_snapshot(state);
      state->project.sprites.erase(state->project.sprites.begin() + state->sprite_index);
      state->sprite_index = clamp_value(state->sprite_index, 0, static_cast<int>(state->project.sprites.size()) - 1);
      state->dirty = true;
      set_status(state, "SPRITE REMOVED.");
    }
  }
}

void draw_header(SDL_Renderer* renderer, const InputFrame& input, UiFrame* ui, EditorState* state, const SDL_Rect& rect) {
  fill_rect(renderer, rect, SDL_Color{49, 57, 74, 255});
  draw_text(renderer, "PRIORY EDITOR", 14, 15, 2, SDL_Color{245, 242, 232, 255});

  int button_x = rect.x + 176;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 58, 26}, "NEW")) {
    state->project = make_seed_project();
    state->selection_active = false;
    state->brush_kind = BrushKind::Tile;
    state->paint_surface = PaintSurface::Ground;
    state->selected_ground_tile = kGrass;
    state->selected_wall_tile = kWall;
    state->selected_stamp = -1;
    clear_scene_sprite_target(state);
    state->dirty = false;
    set_status(state, "SEEDED PRIORY PROJECT RESET.");
  }
  button_x += 66;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 58, 26}, "UNDO")) {
    if (undo_last_change(state)) {
      set_status(state, "UNDID LAST CHANGE.");
    }
  }
  button_x += 66;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 58, 26}, "LOAD")) {
    ProjectData project;
    std::string error;
    if (load_project(&project, state->project_path, &error)) {
      state->project = std::move(project);
      state->dirty = false;
      state->selection_active = false;
      state->brush_kind = BrushKind::Tile;
      state->paint_surface = PaintSurface::Ground;
      clear_scene_sprite_target(state);
      sanitize_selection(state);
      set_status(state, "PROJECT LOADED.");
    } else {
      set_status(state, "LOAD FAILED: " + error);
    }
  }
  button_x += 66;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 58, 26}, "SAVE")) {
    std::string error;
    if (save_project(state->project, state->project_path, &error)) {
      state->dirty = false;
      set_status(state, "PROJECT SAVED.");
    } else {
      set_status(state, "SAVE FAILED: " + error);
    }
  }
  button_x += 72;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 68, 26}, "PAINT", state->tool == ToolMode::Paint)) {
    state->tool = ToolMode::Paint;
  }
  button_x += 76;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 68, 26}, "WARP", state->tool == ToolMode::Warp)) {
    state->tool = ToolMode::Warp;
  }
  button_x += 76;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 68, 26}, "NPC", state->tool == ToolMode::Npc)) {
    state->tool = ToolMode::Npc;
  }
  button_x += 76;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 76, 26}, "MONSTER", state->tool == ToolMode::Monster)) {
    state->tool = ToolMode::Monster;
  }
  button_x += 84;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 68, 26}, "QUEST", state->tool == ToolMode::Quest)) {
    state->tool = ToolMode::Quest;
  }
  button_x += 80;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 76, 26}, "SPRITE", state->tool == ToolMode::Sprite)) {
    state->tool = ToolMode::Sprite;
  }
  button_x += 84;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 58, 26}, state->show_grid ? "GRID" : "NO GRID",
             state->show_grid)) {
    state->show_grid = !state->show_grid;
  }
  button_x += 66;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 76, 26}, state->show_overlays ? "MARKERS" : "PLAIN",
             state->show_overlays)) {
    state->show_overlays = !state->show_overlays;
  }
  button_x += 84;
  if (button(renderer, input, ui, SDL_Rect{button_x, 11, 76, 26}, state->fullscreen ? "WINDOW" : "FULL")) {
    state->fullscreen = !state->fullscreen;
    state->video_dirty = true;
  }

  draw_text(renderer, current_resolution(*state).label, rect.x + rect.w - 206, 15, 1, SDL_Color{214, 220, 228, 255});
  draw_text(renderer, state->dirty ? "UNSAVED" : "CLEAN", rect.x + rect.w - 76, 15, 1,
            state->dirty ? SDL_Color{255, 221, 132, 255} : SDL_Color{170, 224, 170, 255});
}

void draw_sidebar(SDL_Renderer* renderer,
                  const InputFrame& input,
                  UiFrame* ui,
                  EditorState* state,
                  const SDL_Rect& rect) {
  fill_rect(renderer, rect, SDL_Color{233, 228, 217, 255});
  draw_rect(renderer, rect, SDL_Color{119, 109, 89, 255});
  draw_text(renderer, "TILE BANK", rect.x + 10, rect.y + 10, 2, SDL_Color{59, 56, 51, 255});
  draw_text(renderer, paint_surface_name(state->paint_surface), rect.x + rect.w - 108, rect.y + 14, 1,
            state->paint_surface == PaintSurface::Wall ? SDL_Color{143, 87, 72, 255} : SDL_Color{73, 112, 82, 255});

  const int columns = 4;
  const int tile_button = 48;
  const int tile_gap = 8;
  const int start_x = rect.x + 10;
  const int start_y = rect.y + 42;

  for (std::size_t index = 0; index < tile_definitions().size(); ++index) {
    const auto& tile = tile_definitions()[index];
    const int col = static_cast<int>(index % static_cast<std::size_t>(columns));
    const int row = static_cast<int>(index / static_cast<std::size_t>(columns));
    SDL_Rect cell = {start_x + col * (tile_button + tile_gap), start_y + row * (tile_button + tile_gap),
                     tile_button, tile_button};
    const bool active = state->brush_kind == BrushKind::Tile && selected_tile_for(*state) == tile.symbol;
    if (button(renderer, input, ui, cell, "", active)) {
      state->brush_kind = BrushKind::Tile;
      select_tile_for_surface(state, tile.symbol);
      set_status(state, paint_surface_name(state->paint_surface) + std::string(" BRUSH: ") + tile.name + ".");
    }
    draw_tile(renderer, tile.symbol, static_cast<int>(index), 0, cell.x + 16, cell.y + 16, state->now);
    draw_text(renderer, std::string(1, tile.symbol), cell.x + 5, cell.y + 5, 1, SDL_Color{79, 72, 60, 255});
  }

  const int tile_rows = static_cast<int>((tile_definitions().size() + columns - 1) / static_cast<std::size_t>(columns));
  int current_y = start_y + tile_rows * (tile_button + tile_gap) + 12;
  draw_text(renderer, "STAMPS", rect.x + 10, current_y, 2, SDL_Color{59, 56, 51, 255});
  current_y += 24;

  for (std::size_t index = 0; index < state->project.stamps.size(); ++index) {
    const auto& stamp = state->project.stamps[index];
    SDL_Rect row = {rect.x + 10, current_y, rect.w - 20, 54};
    const bool active = state->brush_kind == BrushKind::Stamp && state->selected_stamp == static_cast<int>(index);
    if (button(renderer, input, ui, row, "", active)) {
      state->brush_kind = BrushKind::Stamp;
      state->selected_stamp = static_cast<int>(index);
      set_status(state, "STAMP BRUSH SELECTED.");
    }
    draw_pattern_preview(renderer, stamp.tiles, SDL_Rect{row.x + 6, row.y + 6, 42, 42}, state->now);
    draw_text_box(renderer, stamp.name, SDL_Rect{row.x + 54, row.y + 8, row.w - 60, 14},
                  TextBoxOptions{1, 1, false, 1, TextAlign::Left, TextVerticalAlign::Middle},
                  SDL_Color{48, 46, 42, 255});
    draw_text(renderer,
              std::to_string(stamp_width(stamp)) + "X" + std::to_string(stamp_height(stamp)),
              row.x + 54, row.y + 28, 1, SDL_Color{116, 108, 96, 255});
    current_y += 62;
  }
}

void draw_area_canvas(SDL_Renderer* renderer, const InputFrame& input, const EditorState& state, const SDL_Rect& canvas) {
  fill_rect(renderer, canvas, SDL_Color{210, 223, 234, 255});
  draw_rect(renderer, canvas, SDL_Color{121, 128, 140, 255});
  const AreaData* area = current_area(state);
  if (area == nullptr) {
    return;
  }

  SDL_Texture* texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           std::max(1, area_width(*area) * kNativeTileSize),
                                           std::max(1, area_height(*area) * kNativeTileSize));
  if (texture == nullptr) {
    return;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  render_area_to_texture(renderer, state, *area, texture);
  const MapViewport view = map_viewport(state, canvas);
  SDL_RenderCopy(renderer, texture, &view.src, &view.dest);
  SDL_DestroyTexture(texture);

  if (state.show_grid) {
    SDL_SetRenderDrawColor(renderer, 120, 130, 143, 60);
    const int start_tile_x = view.src.x / kNativeTileSize;
    const int start_tile_y = view.src.y / kNativeTileSize;
    const int end_tile_x = (view.src.x + view.src.w) / kNativeTileSize + 1;
    const int end_tile_y = (view.src.y + view.src.h) / kNativeTileSize + 1;
    for (int x = start_tile_x; x <= end_tile_x; ++x) {
      const int screen_x = view.dest.x + static_cast<int>(std::lround((x * kNativeTileSize - view.src.x) * view.scale_x));
      SDL_RenderDrawLine(renderer, screen_x, view.dest.y, screen_x, view.dest.y + view.dest.h);
    }
    for (int y = start_tile_y; y <= end_tile_y; ++y) {
      const int screen_y = view.dest.y + static_cast<int>(std::lround((y * kNativeTileSize - view.src.y) * view.scale_y));
      SDL_RenderDrawLine(renderer, view.dest.x, screen_y, view.dest.x + view.dest.w, screen_y);
    }
  }

  if (state.selection_active) {
    const int min_x = std::min(state.selection_anchor.x, state.selection_head.x);
    const int min_y = std::min(state.selection_anchor.y, state.selection_head.y);
    const int max_x = std::max(state.selection_anchor.x, state.selection_head.x);
    const int max_y = std::max(state.selection_anchor.y, state.selection_head.y);
    SDL_Rect select = {
        view.dest.x + static_cast<int>(std::lround((min_x * kNativeTileSize - view.src.x) * view.scale_x)),
        view.dest.y + static_cast<int>(std::lround((min_y * kNativeTileSize - view.src.y) * view.scale_y)),
        static_cast<int>(std::lround((max_x - min_x + 1) * kNativeTileSize * view.scale_x)),
        static_cast<int>(std::lround((max_y - min_y + 1) * kNativeTileSize * view.scale_y)),
    };
    draw_rect(renderer, select, SDL_Color{255, 228, 116, 255});
  }

  int hover_x = 0;
  int hover_y = 0;
  if (map_tile_from_mouse(state, canvas, input.mouse_x, input.mouse_y, &hover_x, &hover_y)) {
    SDL_Rect hover = {
        view.dest.x + static_cast<int>(std::lround((hover_x * kNativeTileSize - view.src.x) * view.scale_x)),
        view.dest.y + static_cast<int>(std::lround((hover_y * kNativeTileSize - view.src.y) * view.scale_y)),
        static_cast<int>(std::lround(kNativeTileSize * view.scale_x)),
        static_cast<int>(std::lround(kNativeTileSize * view.scale_y)),
    };
    draw_rect(renderer, hover, SDL_Color{255, 248, 214, 255});
  }
}

void draw_map_overlay_labels(SDL_Renderer* renderer, const EditorState& state, const SDL_Rect& canvas) {
  const AreaData* area = current_area(state);
  if (area == nullptr) {
    return;
  }
  fill_rect(renderer, SDL_Rect{canvas.x + 8, canvas.y + 8, 224, 24}, SDL_Color{247, 241, 228, 230});
  draw_rect(renderer, SDL_Rect{canvas.x + 8, canvas.y + 8, 224, 24}, SDL_Color{116, 107, 91, 255});
  draw_text(renderer, area->name, canvas.x + 16, canvas.y + 16, 1, SDL_Color{55, 52, 48, 255});

  const std::string tool_name = state.tool == ToolMode::Paint
                                    ? "PAINT"
                                    : state.tool == ToolMode::Warp
                                          ? "WARP"
                                          : state.tool == ToolMode::Npc
                                                ? "NPC"
                                                : state.tool == ToolMode::Monster
                                                      ? "MONSTER"
                                                      : state.tool == ToolMode::Quest ? "QUEST" : "SPRITE";
  const int zoom_percent = static_cast<int>(std::lround(state.map_zoom * 100.0f));
  const std::string zoom = "ZOOM " + std::to_string(zoom_percent) + "%";
  draw_text(renderer, tool_name + "  " + zoom, canvas.x + canvas.w - 184, canvas.y + 16, 1,
            SDL_Color{248, 243, 230, 255});
}

void draw_area_thumbnail(SDL_Renderer* renderer,
                         const EditorState& state,
                         const AreaData& area,
                         const SDL_Rect& rect,
                         bool highlight) {
  fill_rect(renderer, rect, highlight ? SDL_Color{251, 246, 236, 255} : SDL_Color{232, 226, 214, 255});
  draw_rect(renderer, rect, highlight ? SDL_Color{97, 132, 88, 255} : SDL_Color{125, 116, 96, 255});
  SDL_Texture* texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           std::max(1, area_width(area) * kNativeTileSize),
                                           std::max(1, area_height(area) * kNativeTileSize));
  if (texture == nullptr) {
    return;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  render_area_to_texture(renderer, state, area, texture);
  SDL_Rect preview = {rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 18};
  SDL_RenderCopy(renderer, texture, nullptr, &preview);
  SDL_DestroyTexture(texture);
  draw_text_box(renderer, area.name, SDL_Rect{rect.x + 4, rect.y + rect.h - 14, rect.w - 8, 10},
                TextBoxOptions{1, 1, false, 1, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{61, 58, 52, 255});
}

void draw_world_overview(SDL_Renderer* renderer, const EditorState& state, const SDL_Rect& canvas) {
  const AreaData* area = current_area(state);
  if (area == nullptr) {
    return;
  }
  SDL_Rect panel = {canvas.x + canvas.w - 214, canvas.y + canvas.h - 134, 206, 126};
  fill_rect(renderer, panel, SDL_Color{248, 242, 229, 238});
  draw_rect(renderer, panel, SDL_Color{116, 106, 86, 255});
  draw_text(renderer, "LINKED AREAS", panel.x + 8, panel.y + 8, 1, SDL_Color{74, 68, 59, 255});

  draw_area_thumbnail(renderer, state, *area, SDL_Rect{panel.x + 8, panel.y + 24, 88, 54}, true);
  std::vector<const AreaData*> linked;
  for (const auto& warp : area->warps) {
    for (const auto& candidate : state.project.areas) {
      if (candidate.id == warp.target_area) {
        const bool seen = std::any_of(linked.begin(), linked.end(), [&](const AreaData* existing) {
          return existing->id == candidate.id;
        });
        if (!seen) {
          linked.push_back(&candidate);
        }
        break;
      }
    }
    if (linked.size() >= 3U) {
      break;
    }
  }
  for (std::size_t index = 0; index < linked.size(); ++index) {
    const int col = static_cast<int>(index % 2U);
    const int row = static_cast<int>(index / 2U);
    draw_area_thumbnail(renderer, state, *linked[index],
                        SDL_Rect{panel.x + 104 + col * 46, panel.y + 24 + row * 48, 42, 42}, false);
  }
}

void draw_status(SDL_Renderer* renderer, const EditorState& state, const SDL_Rect& rect) {
  fill_rect(renderer, rect, SDL_Color{46, 53, 67, 255});
  draw_text(renderer, state.status_message, rect.x + 12, rect.y + 10, 1, SDL_Color{239, 235, 222, 255});
  draw_text(renderer, "1 PAINT 2 WARP 3 NPC 4 MON 5 QUEST 6 SPRITE CTRL+S SAVE CTRL+Z UNDO F11 FULL",
            rect.x + rect.w - 470, rect.y + 10, 1, SDL_Color{193, 203, 214, 255});
}

void apply_video_settings(SDL_Window* window, EditorState* state) {
  if (window == nullptr || state == nullptr) {
    return;
  }

  const ResolutionOption& resolution = current_resolution(*state);
  if (state->fullscreen) {
    SDL_DisplayMode mode{};
    mode.w = resolution.width;
    mode.h = resolution.height;
    if (SDL_SetWindowDisplayMode(window, &mode) != 0 || SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) != 0) {
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
  } else {
    SDL_SetWindowFullscreen(window, 0);
    SDL_SetWindowSize(window, resolution.width, resolution.height);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  }
  state->video_dirty = false;
}

void draw_launch_menu(SDL_Renderer* renderer, const EditorState& state, int width, int height) {
  fill_rect(renderer, SDL_Rect{0, 0, width, height}, SDL_Color{194, 209, 219, 255});
  fill_rect(renderer, SDL_Rect{0, height / 2, width, height / 2}, SDL_Color{178, 189, 164, 255});

  const SDL_Rect panel = {width / 2 - 220, height / 2 - 150, 440, 300};
  fill_rect(renderer, panel, SDL_Color{245, 240, 229, 250});
  draw_rect(renderer, panel, SDL_Color{120, 109, 89, 255});
  draw_text(renderer, "PRIORY EDITOR", width / 2, panel.y + 24, 4, SDL_Color{63, 78, 91, 255}, true);
  draw_text_box(renderer, "VIDEO SETUP", SDL_Rect{panel.x + 104, panel.y + 62, 232, 16},
                TextBoxOptions{2, 1, false, 2, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{75, 86, 66, 255});

  const std::array<std::string, 4> labels = {"RESOLUTION", "DISPLAY", "OPEN EDITOR", "QUIT"};
  const std::array<std::string, 4> values = {
      current_resolution(state).label,
      state.fullscreen ? "FULLSCREEN" : "WINDOWED",
      "ENTER TO LAUNCH",
      "ESC OR ENTER",
  };

  int y = panel.y + 96;
  for (std::size_t index = 0; index < labels.size(); ++index) {
    const bool selected = static_cast<int>(index) == state.launch_selection;
    fill_rect(renderer, SDL_Rect{panel.x + 28, y - 4, panel.w - 56, 40},
              selected ? SDL_Color{224, 216, 191, 255} : SDL_Color{236, 231, 219, 255});
    draw_rect(renderer, SDL_Rect{panel.x + 28, y - 4, panel.w - 56, 40},
              selected ? SDL_Color{82, 104, 67, 255} : SDL_Color{129, 119, 101, 255});
    draw_text_box(renderer, labels[index], SDL_Rect{panel.x + 42, y + 2, 150, 10},
                  TextBoxOptions{1, 1, false, 1, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{86, 79, 60, 255});
    draw_text_box(renderer, values[index], SDL_Rect{panel.x + 196, y + 2, panel.w - 238, 18},
                  TextBoxOptions{1, 1, true, 2, TextAlign::Right, TextVerticalAlign::Middle},
                  selected ? SDL_Color{61, 91, 54, 255} : SDL_Color{58, 56, 50, 255});
    y += 48;
  }

  draw_text_box(renderer, "UP DOWN CHOOSE   LEFT RIGHT CHANGE   ENTER CONFIRM", SDL_Rect{panel.x + 24, panel.y + 262, panel.w - 48, 12},
                TextBoxOptions{1, 1, false, 1, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{96, 88, 73, 255});
}

void mark_project_edit(EditorState* state) {
  if (state == nullptr) {
    return;
  }
  push_undo_snapshot(state);
  state->dirty = true;
}

void draw_project_panel(SDL_Renderer* renderer,
                        const InputFrame& input,
                        UiFrame* ui,
                        EditorState* state,
                        const SDL_Rect& rect) {
  section_card(renderer, rect, "PROJECT");
  label(renderer, rect.x + 8, rect.y + 24, "NAME");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 34, rect.w - 16, kFieldHeight}, "project_name",
             state->project.name, "PROJECT NAME");
  label(renderer, rect.x + 8, rect.y + 67, "PATH");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 77, rect.w - 16, kFieldHeight}, "project_path",
             state->project_path, "PATH TO JSON");
  const int button_width = std::max(66, (rect.w - 32) / 3);
  const int button_gap = 8;
  const int button_y = rect.y + 112;
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, button_y, button_width, 24}, "LOAD")) {
    ProjectData project;
    std::string error;
    if (load_project(&project, state->project_path, &error)) {
      state->project = std::move(project);
      state->dirty = false;
      state->selection_active = false;
      state->brush_kind = BrushKind::Tile;
      state->paint_surface = PaintSurface::Ground;
      clear_scene_sprite_target(state);
      sanitize_selection(state);
      set_status(state, "PROJECT LOADED.");
    } else {
      set_status(state, "LOAD FAILED: " + error);
    }
  }
  if (button(renderer, input, ui,
             SDL_Rect{rect.x + 8 + button_width + button_gap, button_y, button_width, 24}, "SAVE")) {
    std::string error;
    if (save_project(state->project, state->project_path, &error)) {
      state->dirty = false;
      set_status(state, "PROJECT SAVED.");
    } else {
      set_status(state, "SAVE FAILED: " + error);
    }
  }
  if (button(renderer, input, ui,
             SDL_Rect{rect.x + 8 + (button_width + button_gap) * 2, button_y, button_width, 24}, "UNDO")) {
    if (undo_last_change(state)) {
      set_status(state, "UNDID LAST CHANGE.");
    }
  }
  draw_text(renderer, "CTRL+L LOAD  CTRL+S SAVE  CTRL+Z UNDO", rect.x + 8, rect.y + 142, 1,
            SDL_Color{108, 99, 84, 255});
}

void draw_area_panel(SDL_Renderer* renderer,
                     const InputFrame& input,
                     UiFrame* ui,
                     EditorState* state,
                     const SDL_Rect& rect) {
  section_card(renderer, rect, "AREA");
  AreaData* area = current_area(state);
  bool left = false;
  bool right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 24, rect.w - 16, 24},
                area != nullptr ? area->name : "NO AREA", &left, &right);
  if (left && !state->project.areas.empty()) {
    state->area_index = (state->area_index - 1 + static_cast<int>(state->project.areas.size())) %
                        static_cast<int>(state->project.areas.size());
    state->warp_index = -1;
    state->npc_index = -1;
    state->monster_index = -1;
    clear_scene_sprite_target(state);
    area = current_area(state);
  }
  if (right && !state->project.areas.empty()) {
    state->area_index = (state->area_index + 1) % static_cast<int>(state->project.areas.size());
    state->warp_index = -1;
    state->npc_index = -1;
    state->monster_index = -1;
    clear_scene_sprite_target(state);
    area = current_area(state);
  }

  label(renderer, rect.x + 8, rect.y + 56, "NEW AREA ID");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 66, 114, kFieldHeight}, "new_area_id",
             state->new_area_id, "AREA ID");
  label(renderer, rect.x + 130, rect.y + 56, "NEW AREA NAME");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 130, rect.y + 66, rect.w - 138, kFieldHeight}, "new_area_name",
             state->new_area_name, "AREA NAME");
  draw_text(renderer, "W " + std::to_string(state->new_area_width), rect.x + 8, rect.y + 104, 1, SDL_Color{88, 81, 69, 255});
  if (button(renderer, input, ui, SDL_Rect{rect.x + 34, rect.y + 98, 24, 20}, "-")) {
    state->new_area_width = std::max(4, state->new_area_width - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 62, rect.y + 98, 24, 20}, "+")) {
    state->new_area_width += 1;
  }
  draw_text(renderer, "H " + std::to_string(state->new_area_height), rect.x + 98, rect.y + 104, 1, SDL_Color{88, 81, 69, 255});
  if (button(renderer, input, ui, SDL_Rect{rect.x + 124, rect.y + 98, 24, 20}, "-")) {
    state->new_area_height = std::max(4, state->new_area_height - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 152, rect.y + 98, 24, 20}, "+")) {
    state->new_area_height += 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 188, rect.y + 98, 82, 20}, "CREATE")) {
    mark_project_edit(state);
    AreaData fresh = make_area(state->new_area_id.empty() ? "new_area" : state->new_area_id,
                               state->new_area_name.empty() ? "NEW AREA" : state->new_area_name,
                               state->new_area_width, state->new_area_height, kGrass, false);
    state->project.areas.push_back(std::move(fresh));
    state->area_index = static_cast<int>(state->project.areas.size()) - 1;
    state->warp_index = -1;
    state->npc_index = -1;
    state->monster_index = -1;
    clear_scene_sprite_target(state);
    set_status(state, "AREA CREATED.");
    area = current_area(state);
  }

  if (button(renderer, input, ui, SDL_Rect{rect.x + rect.w - 260, rect.y + 124, 78, 24}, "DELETE",
             !state->project.areas.empty(), state->project.areas.size() <= 1)) {
    if (state->project.areas.size() > 1 && area != nullptr) {
      mark_project_edit(state);
      const std::string removed_id = area->id;
      state->project.areas.erase(state->project.areas.begin() + state->area_index);
      state->area_index = clamp_value(state->area_index, 0, static_cast<int>(state->project.areas.size()) - 1);
      remove_area_references(&state->project, removed_id, state->project.areas.front().id);
      set_status(state, "AREA REMOVED.");
      clear_scene_sprite_target(state);
      area = current_area(state);
    }
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + rect.w - 174, rect.y + 124, 78, 24},
             area != nullptr && area->indoor ? "INDOOR" : "OUTDOOR", area != nullptr && area->indoor)) {
    if (area != nullptr) {
      mark_project_edit(state);
      area->indoor = !area->indoor;
    }
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + rect.w - 88, rect.y + 124, 78, 24},
             area != nullptr && area->player_tillable ? "TILLABLE" : "UNTILLED", area != nullptr && area->player_tillable)) {
    if (area != nullptr) {
      mark_project_edit(state);
      area->player_tillable = !area->player_tillable;
    }
  }

  label(renderer, rect.x + 8, rect.y + 154, "AREA ID");
  const std::string previous_id = area != nullptr ? area->id : "";
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 164, 140, kFieldHeight}, "area_id",
             area != nullptr ? area->id : "", "AREA ID");
  if (area != nullptr && previous_id != area->id) {
    replace_area_references(&state->project, previous_id, area->id);
    state->dirty = true;
  }
  label(renderer, rect.x + 156, rect.y + 154, "AREA NAME");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 156, rect.y + 164, rect.w - 164, kFieldHeight}, "area_name",
             area != nullptr ? area->name : "", "AREA NAME");
  if (area == nullptr) {
    return;
  }
  draw_text(renderer, "SIZE " + std::to_string(area_width(*area)) + " X " + std::to_string(area_height(*area)),
            rect.x + 8, rect.y + 200, 1, SDL_Color{88, 81, 69, 255});
  if (button(renderer, input, ui, SDL_Rect{rect.x + 96, rect.y + 194, 24, 20}, "-")) {
    mark_project_edit(state);
    ensure_area_size(area, area_width(*area) - 1, area_height(*area));
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 124, rect.y + 194, 24, 20}, "+")) {
    mark_project_edit(state);
    ensure_area_size(area, area_width(*area) + 1, area_height(*area));
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 160, rect.y + 194, 24, 20}, "-")) {
    mark_project_edit(state);
    ensure_area_size(area, area_width(*area), area_height(*area) - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 188, rect.y + 194, 24, 20}, "+")) {
    mark_project_edit(state);
    ensure_area_size(area, area_width(*area), area_height(*area) + 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 226, rect.y + 194, 96, 20}, "FILL")) {
    mark_project_edit(state);
    fill_area_with_brush(state);
    set_status(state, "AREA FILLED.");
  }
}

void draw_paint_panel(SDL_Renderer* renderer,
                      const InputFrame& input,
                      UiFrame* ui,
                      EditorState* state,
                      const SDL_Rect& rect) {
  section_card(renderer, rect, "PAINT AND STAMPS");
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 26, 112, 24}, "GROUND TILE",
             state->paint_surface == PaintSurface::Ground)) {
    state->paint_surface = PaintSurface::Ground;
    state->brush_kind = BrushKind::Tile;
    set_status(state, "GROUND TILE LAYER IS ACTIVE.");
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 128, rect.y + 26, 104, 24}, "WALL TILE",
             state->paint_surface == PaintSurface::Wall)) {
    state->paint_surface = PaintSurface::Wall;
    state->brush_kind = BrushKind::Tile;
    set_status(state, "WALL TILE LAYER IS ACTIVE.");
  }
  draw_text(renderer, state->brush_kind == BrushKind::Tile ? "TILE BRUSH" : "STAMP BRUSH", rect.x + 8, rect.y + 60, 1,
            SDL_Color{75, 70, 63, 255});
  if (state->brush_kind == BrushKind::Tile) {
    const TileDefinition* tile = tile_definition(selected_tile_for(*state));
    draw_text(renderer, tile != nullptr ? tile->name : "UNKNOWN", rect.x + 8, rect.y + 76, 2, SDL_Color{51, 48, 45, 255});
  } else {
    const StampData* stamp = current_stamp(*state);
    draw_text(renderer, stamp != nullptr ? stamp->name : "NO STAMP", rect.x + 8, rect.y + 76, 2, SDL_Color{51, 48, 45, 255});
  }
  draw_text(renderer, state->paint_surface == PaintSurface::Wall ? "WALL LAYER BLOCKS WALKING." : "GROUND LAYER IS WALKABLE.",
            rect.x + 8, rect.y + 100, 1, state->paint_surface == PaintSurface::Wall ? SDL_Color{143, 87, 72, 255}
                                                                                      : SDL_Color{73, 112, 82, 255});
  label(renderer, rect.x + 8, rect.y + 116, "NEW STAMP NAME");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 126, rect.w - 16, kFieldHeight}, "stamp_name",
             state->pending_stamp_name, "STAMP NAME");
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 160, 134, 24}, "SAVE SELECTION", state->selection_active,
             !state->selection_active)) {
    if (save_selection_as_stamp(state)) {
      set_status(state, "STAMP CAPTURED.");
    }
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 150, rect.y + 160, 124, 24}, "CLEAR SELECT")) {
    state->selection_active = false;
    state->selection_dragging = false;
  }
  StampData* stamp = current_stamp(state);
  if (stamp != nullptr) {
    label(renderer, rect.x + 8, rect.y + 196, "STAMP ID");
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 206, rect.w - 16, kFieldHeight}, "stamp_selected_id",
               stamp->id, "STAMP ID");
    label(renderer, rect.x + 8, rect.y + 238, "STAMP NAME");
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 248, rect.w - 16, kFieldHeight},
               "stamp_selected_name", stamp->name, "STAMP NAME");
  }
}

void draw_warp_panel(SDL_Renderer* renderer,
                     const InputFrame& input,
                     UiFrame* ui,
                     EditorState* state,
                     const SDL_Rect& rect) {
  section_card(renderer, rect, "PATHWAYS");
  AreaData* area = current_area(state);
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 24, 78, 24}, "ADD") && area != nullptr) {
    mark_project_edit(state);
    area->warps.push_back({0, 0, 1, 1, "NEW WARP", area->id, 0, 0, Direction::Down});
    state->warp_index = static_cast<int>(area->warps.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 92, rect.y + 24, 78, 24}, "DELETE", state->warp_index >= 0,
             state->warp_index < 0)) {
    delete_selected_for_tool(state);
  }
  WarpData* selected = current_warp(state);
  draw_text(renderer, selected != nullptr ? "SELECTED PATH" : "NO PATH", rect.x + 186, rect.y + 31, 1, SDL_Color{88, 81, 69, 255});
  if (selected == nullptr) {
    return;
  }
  label(renderer, rect.x + 8, rect.y + 58, "LABEL");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 68, rect.w - 16, kFieldHeight}, "warp_label",
             selected->label, "PATH LABEL");
  draw_text(renderer,
            "SOURCE " + std::to_string(selected->x) + "," + std::to_string(selected->y) + "  " +
                std::to_string(selected->width) + "X" + std::to_string(selected->height),
            rect.x + 8, rect.y + 106, 1, SDL_Color{88, 81, 69, 255});
  if (button(renderer, input, ui, SDL_Rect{rect.x + 180, rect.y + 100, 78, 20}, "BOUND", state->selection_active,
             !state->selection_active)) {
    const int min_x = std::min(state->selection_anchor.x, state->selection_head.x);
    const int min_y = std::min(state->selection_anchor.y, state->selection_head.y);
    const int max_x = std::max(state->selection_anchor.x, state->selection_head.x);
    const int max_y = std::max(state->selection_anchor.y, state->selection_head.y);
    mark_project_edit(state);
    selected->x = min_x;
    selected->y = min_y;
    selected->width = max_x - min_x + 1;
    selected->height = max_y - min_y + 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 120, 24, 20}, "W-")) {
    mark_project_edit(state);
    selected->width = std::max(1, selected->width - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 36, rect.y + 120, 24, 20}, "W+")) {
    mark_project_edit(state);
    selected->width += 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 68, rect.y + 120, 24, 20}, "H-")) {
    mark_project_edit(state);
    selected->height = std::max(1, selected->height - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 96, rect.y + 120, 24, 20}, "H+")) {
    mark_project_edit(state);
    selected->height += 1;
  }

  std::vector<std::string> area_ids;
  std::vector<std::string> area_names;
  for (const auto& candidate : state->project.areas) {
    area_ids.push_back(candidate.id);
    area_names.push_back(candidate.name);
  }
  int target_area_index = 0;
  for (std::size_t index = 0; index < area_ids.size(); ++index) {
    if (area_ids[index] == selected->target_area) {
      target_area_index = static_cast<int>(index);
      break;
    }
  }
  bool area_left = false;
  bool area_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 150, rect.w - 16, 24},
                area_names.empty() ? "NO TARGET AREA" : area_names[static_cast<std::size_t>(target_area_index)], &area_left, &area_right);
  if ((area_left || area_right) && !area_ids.empty()) {
    mark_project_edit(state);
    target_area_index = (target_area_index + (area_right ? 1 : -1) + static_cast<int>(area_ids.size())) %
                        static_cast<int>(area_ids.size());
    selected->target_area = area_ids[static_cast<std::size_t>(target_area_index)];
  }
  const AreaData* target_area = nullptr;
  for (const auto& candidate : state->project.areas) {
    if (candidate.id == selected->target_area) {
      target_area = &candidate;
      break;
    }
  }
  std::vector<std::string> target_paths;
  int target_path_index = -1;
  if (target_area != nullptr) {
    for (std::size_t index = 0; index < target_area->warps.size(); ++index) {
      const auto& warp = target_area->warps[index];
      target_paths.push_back("W" + std::to_string(static_cast<int>(index) + 1) + " " + warp.label);
      if (selected->target_x == warp.x && selected->target_y == warp.y) {
        target_path_index = static_cast<int>(index);
      }
    }
  }
  bool path_left = false;
  bool path_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 180, rect.w - 16, 24},
                target_paths.empty() ? "DIRECT TILE" : target_paths[static_cast<std::size_t>(std::max(0, target_path_index))],
                &path_left, &path_right);
  if ((path_left || path_right) && !target_paths.empty() && target_area != nullptr) {
    mark_project_edit(state);
    int index = target_path_index < 0 ? 0 : target_path_index;
    index = (index + (path_right ? 1 : -1) + static_cast<int>(target_paths.size())) % static_cast<int>(target_paths.size());
    selected->target_x = target_area->warps[static_cast<std::size_t>(index)].x;
    selected->target_y = target_area->warps[static_cast<std::size_t>(index)].y;
  }
  draw_text(renderer, "TARGET " + std::to_string(selected->target_x) + "," + std::to_string(selected->target_y), rect.x + 8,
            rect.y + 214, 1, SDL_Color{88, 81, 69, 255});
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 226, 28, 22}, "X-")) {
    mark_project_edit(state);
    selected->target_x = std::max(0, selected->target_x - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 40, rect.y + 226, 28, 22}, "X+")) {
    mark_project_edit(state);
    selected->target_x += 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 84, rect.y + 226, 28, 22}, "Y-")) {
    mark_project_edit(state);
    selected->target_y = std::max(0, selected->target_y - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 116, rect.y + 226, 28, 22}, "Y+")) {
    mark_project_edit(state);
    selected->target_y += 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 160, rect.y + 226, 110, 22},
             state->picking_warp_target ? "PICKING..." : "PICK TILE", state->picking_warp_target)) {
    state->picking_warp_target = !state->picking_warp_target;
  }
  bool dir_left = false;
  bool dir_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 256, rect.w - 16, 24}, direction_name(selected->target_facing),
                &dir_left, &dir_right);
  if (dir_left) {
    mark_project_edit(state);
    selected->target_facing = next_direction(selected->target_facing, -1);
  }
  if (dir_right) {
    mark_project_edit(state);
    selected->target_facing = next_direction(selected->target_facing, 1);
  }
}

void draw_npc_panel(SDL_Renderer* renderer,
                    const InputFrame& input,
                    UiFrame* ui,
                    EditorState* state,
                    const SDL_Rect& rect) {
  section_card(renderer, rect, "NPC");
  AreaData* area = current_area(state);
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 24, 78, 24}, "ADD") && area != nullptr) {
    mark_project_edit(state);
    const int number = static_cast<int>(area->npcs.size()) + 1;
    area->npcs.push_back({"npc_" + std::to_string(number), "New NPC", SpriteRole::Monk, "", 1, 1, Direction::Down, true,
                          "Write dialogue here."});
    state->npc_index = static_cast<int>(area->npcs.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 92, rect.y + 24, 78, 24}, "DELETE", state->npc_index >= 0,
             state->npc_index < 0)) {
    delete_selected_for_tool(state);
  }
  NpcData* npc = current_npc(state);
  if (npc == nullptr) {
    return;
  }
  label(renderer, rect.x + 8, rect.y + 58, "ID");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 68, 146, kFieldHeight}, "npc_id", npc->id, "NPC ID");
  label(renderer, rect.x + 162, rect.y + 58, "NAME");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 162, rect.y + 68, rect.w - 170, kFieldHeight}, "npc_name",
             npc->name, "NPC NAME");
  draw_text(renderer, "POS " + std::to_string(npc->x) + "," + std::to_string(npc->y), rect.x + 8, rect.y + 104, 1,
            SDL_Color{88, 81, 69, 255});
  bool role_left = false;
  bool role_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 118, rect.w - 16, 24}, sprite_role_name(npc->role), &role_left,
                &role_right);
  if (role_left) {
    mark_project_edit(state);
    npc->role = next_role(npc->role, -1);
  }
  if (role_right) {
    mark_project_edit(state);
    npc->role = next_role(npc->role, 1);
  }
  bool face_left = false;
  bool face_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 148, rect.w - 16, 24}, direction_name(npc->facing), &face_left,
                &face_right);
  if (face_left) {
    mark_project_edit(state);
    npc->facing = next_direction(npc->facing, -1);
  }
  if (face_right) {
    mark_project_edit(state);
    npc->facing = next_direction(npc->facing, 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 178, 110, 22}, npc->solid ? "SOLID" : "NON-SOLID",
             npc->solid)) {
    mark_project_edit(state);
    npc->solid = !npc->solid;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 126, rect.y + 178, 96, 22}, "EDIT SPRITE")) {
    if (bind_sprite_editor_to_npc(state, state->npc_index, true)) {
      set_status(state, "NPC SPRITE OPENED IN STUDIO.");
    } else {
      set_status(state, "NPC SPRITE COULD NOT BE OPENED.");
    }
  }
  label(renderer, rect.x + 230, rect.y + 180, "SPRITE OVERRIDE");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 230, rect.y + 190, rect.w - 238, kFieldHeight}, "npc_sprite_id",
             npc->sprite_id, "OPTIONAL SPRITE ID");
  label(renderer, rect.x + 8, rect.y + 226, "DIALOGUE");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 236, rect.w - 16, 54}, "npc_dialogue", npc->dialogue,
             "NPC DIALOGUE", true);
}

void draw_monster_panel(SDL_Renderer* renderer,
                        const InputFrame& input,
                        UiFrame* ui,
                        EditorState* state,
                        const SDL_Rect& rect) {
  section_card(renderer, rect, "MONSTER");
  AreaData* area = current_area(state);
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 24, 78, 24}, "ADD") && area != nullptr) {
    mark_project_edit(state);
    const int number = static_cast<int>(area->monsters.size()) + 1;
    area->monsters.push_back({"monster_" + std::to_string(number), "New Monster", "ratling", 1, 1, Direction::Down, 6, 1, true});
    state->monster_index = static_cast<int>(area->monsters.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 92, rect.y + 24, 78, 24}, "DELETE", state->monster_index >= 0,
             state->monster_index < 0)) {
    delete_selected_for_tool(state);
  }
  MonsterData* monster = current_monster(state);
  if (monster == nullptr) {
    return;
  }
  label(renderer, rect.x + 8, rect.y + 58, "ID");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 68, 146, kFieldHeight}, "monster_id", monster->id,
             "MONSTER ID");
  label(renderer, rect.x + 162, rect.y + 58, "NAME");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 162, rect.y + 68, rect.w - 170, kFieldHeight}, "monster_name",
             monster->name, "MONSTER NAME");
  label(renderer, rect.x + 8, rect.y + 102, "SPRITE");
  if (button(renderer, input, ui, SDL_Rect{rect.x + rect.w - 94, rect.y + 102, 86, 22}, "EDIT")) {
    if (bind_sprite_editor_to_monster(state, state->monster_index, true)) {
      set_status(state, "MONSTER SPRITE OPENED IN STUDIO.");
    } else {
      set_status(state, "MONSTER SPRITE COULD NOT BE OPENED.");
    }
  }
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 112, rect.w - 104, kFieldHeight}, "monster_sprite_id",
             monster->sprite_id, "SPRITE ASSET ID");
  draw_text(renderer, "POS " + std::to_string(monster->x) + "," + std::to_string(monster->y), rect.x + 8, rect.y + 148, 1,
            SDL_Color{88, 81, 69, 255});
  bool face_left = false;
  bool face_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 162, rect.w - 16, 24}, direction_name(monster->facing),
                &face_left, &face_right);
  if (face_left) {
    mark_project_edit(state);
    monster->facing = next_direction(monster->facing, -1);
  }
  if (face_right) {
    mark_project_edit(state);
    monster->facing = next_direction(monster->facing, 1);
  }
  draw_text(renderer, "HP " + std::to_string(monster->max_hp), rect.x + 8, rect.y + 202, 1, SDL_Color{88, 81, 69, 255});
  if (button(renderer, input, ui, SDL_Rect{rect.x + 46, rect.y + 196, 24, 20}, "-")) {
    mark_project_edit(state);
    monster->max_hp = std::max(1, monster->max_hp - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 74, rect.y + 196, 24, 20}, "+")) {
    mark_project_edit(state);
    monster->max_hp += 1;
  }
  draw_text(renderer, "ATK " + std::to_string(monster->attack), rect.x + 114, rect.y + 202, 1, SDL_Color{88, 81, 69, 255});
  if (button(renderer, input, ui, SDL_Rect{rect.x + 156, rect.y + 196, 24, 20}, "-")) {
    mark_project_edit(state);
    monster->attack = std::max(1, monster->attack - 1);
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 184, rect.y + 196, 24, 20}, "+")) {
    mark_project_edit(state);
    monster->attack += 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 224, rect.y + 196, 112, 20}, monster->aggressive ? "AGGRESSIVE" : "PASSIVE",
             monster->aggressive)) {
    mark_project_edit(state);
    monster->aggressive = !monster->aggressive;
  }
}

void draw_quest_panel(SDL_Renderer* renderer,
                      const InputFrame& input,
                      UiFrame* ui,
                      EditorState* state,
                      const SDL_Rect& rect) {
  section_card(renderer, rect, "QUEST");
  bool quest_left = false;
  bool quest_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 24, rect.w - 16, 24},
                current_quest(*state) != nullptr ? current_quest(*state)->title : "NO QUEST", &quest_left, &quest_right);
  if (quest_left && !state->project.quests.empty()) {
    state->quest_index = (state->quest_index - 1 + static_cast<int>(state->project.quests.size())) %
                         static_cast<int>(state->project.quests.size());
  }
  if (quest_right && !state->project.quests.empty()) {
    state->quest_index = (state->quest_index + 1) % static_cast<int>(state->project.quests.size());
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 54, 78, 24}, "ADD")) {
    mark_project_edit(state);
    state->project.quests.push_back({"quest_" + std::to_string(state->project.quests.size() + 1),
                                     "New Quest",
                                     "Summarize the quest arc here.",
                                     "",
                                     "",
                                     "",
                                     {{"stage_1", "Describe the first stage."}},
                                     {{"requirement_1", QuestRequirementType::Custom, "", "", 0, 0, 1, "Describe the requirement."}},
                                     {{"", 1, 0, ""}}});
    state->quest_index = static_cast<int>(state->project.quests.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 92, rect.y + 54, 78, 24}, "DELETE", !state->project.quests.empty(),
             state->project.quests.size() <= 1)) {
    if (state->project.quests.size() > 1) {
      mark_project_edit(state);
      state->project.quests.erase(state->project.quests.begin() + state->quest_index);
      state->quest_index = clamp_value(state->quest_index, 0, static_cast<int>(state->project.quests.size()) - 1);
    }
  }
  QuestData* quest = current_quest(state);
  if (quest == nullptr) {
    return;
  }
  label(renderer, rect.x + 8, rect.y + 88, "ID");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 98, 120, kFieldHeight}, "quest_id", quest->id, "QUEST ID");
  label(renderer, rect.x + 136, rect.y + 88, "TITLE");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 136, rect.y + 98, rect.w - 144, kFieldHeight}, "quest_title",
             quest->title, "QUEST TITLE");
  label(renderer, rect.x + 8, rect.y + 132, "QUEST GIVER");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 142, rect.w - 16, kFieldHeight}, "quest_giver_id",
             quest->quest_giver_id, "NPC ID");
  label(renderer, rect.x + 8, rect.y + 176, "SUMMARY");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 186, rect.w - 16, 34}, "quest_summary", quest->summary,
             "QUEST SUMMARY", true);
  label(renderer, rect.x + 8, rect.y + 226, "START DIALOGUE");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 236, rect.w - 16, 28}, "quest_start_dialogue",
             quest->start_dialogue, "START DIALOGUE", true);
  label(renderer, rect.x + 8, rect.y + 270, "COMPLETION DIALOGUE");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 280, rect.w - 16, 28}, "quest_completion_dialogue",
             quest->completion_dialogue, "COMPLETION DIALOGUE", true);

  bool stage_left = false;
  bool stage_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 314, rect.w - 16, 24},
                current_stage(state) != nullptr ? current_stage(state)->id : "NO STAGE", &stage_left, &stage_right);
  if (stage_left && !quest->stages.empty()) {
    state->stage_index = (state->stage_index - 1 + static_cast<int>(quest->stages.size())) % static_cast<int>(quest->stages.size());
  }
  if (stage_right && !quest->stages.empty()) {
    state->stage_index = (state->stage_index + 1) % static_cast<int>(quest->stages.size());
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 344, 78, 22}, "STAGE+")) {
    mark_project_edit(state);
    quest->stages.push_back({"stage_" + std::to_string(quest->stages.size() + 1), "Describe this stage."});
    state->stage_index = static_cast<int>(quest->stages.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 92, rect.y + 344, 78, 22}, "STAGE-", !quest->stages.empty(),
             quest->stages.size() <= 1)) {
    delete_selected_for_tool(state);
  }
  QuestStage* stage = current_stage(state);
  if (stage != nullptr) {
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 176, rect.y + 344, 110, 22}, "stage_id", stage->id, "STAGE ID");
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 372, rect.w - 16, 30}, "stage_text", stage->text,
               "STAGE TEXT", true);
  }

  bool req_left = false;
  bool req_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 408, 188, 24},
                current_requirement(state) != nullptr ? current_requirement(state)->id : "NO REQUIREMENT", &req_left, &req_right);
  if (req_left && !quest->requirements.empty()) {
    state->requirement_index =
        (state->requirement_index - 1 + static_cast<int>(quest->requirements.size())) % static_cast<int>(quest->requirements.size());
  }
  if (req_right && !quest->requirements.empty()) {
    state->requirement_index = (state->requirement_index + 1) % static_cast<int>(quest->requirements.size());
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 202, rect.y + 408, 42, 24}, "+REQ")) {
    mark_project_edit(state);
    quest->requirements.push_back({"requirement_" + std::to_string(quest->requirements.size() + 1), QuestRequirementType::Custom,
                                   "", "", 0, 0, 1, "Describe the requirement."});
    state->requirement_index = static_cast<int>(quest->requirements.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 248, rect.y + 408, 42, 24}, "-REQ", !quest->requirements.empty(),
             quest->requirements.empty())) {
    if (!quest->requirements.empty()) {
      mark_project_edit(state);
      quest->requirements.erase(quest->requirements.begin() + state->requirement_index);
      state->requirement_index = 0;
    }
  }
  QuestRequirement* requirement = current_requirement(state);
  if (requirement != nullptr) {
    bool type_left = false;
    bool type_right = false;
    cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 438, 132, 24}, requirement_type_name(requirement->type),
                  &type_left, &type_right);
    if (type_left) {
      mark_project_edit(state);
      requirement->type = next_requirement_type(requirement->type, -1);
    }
    if (type_right) {
      mark_project_edit(state);
      requirement->type = next_requirement_type(requirement->type, 1);
    }
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 146, rect.y + 438, 92, 24}, "requirement_target_id",
               requirement->target_id, "TARGET");
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 244, rect.y + 438, 92, 24}, "requirement_area_id",
               requirement->area_id, "AREA");
    if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 466, 24, 20}, "Q-")) {
      mark_project_edit(state);
      requirement->quantity = std::max(1, requirement->quantity - 1);
    }
    if (button(renderer, input, ui, SDL_Rect{rect.x + 36, rect.y + 466, 24, 20}, "Q+")) {
      mark_project_edit(state);
      requirement->quantity += 1;
    }
    draw_text(renderer, "QTY " + std::to_string(requirement->quantity), rect.x + 68, rect.y + 472, 1,
              SDL_Color{88, 81, 69, 255});
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 146, rect.y + 466, rect.w - 154, 24}, "requirement_description",
               requirement->description, "REQUIREMENT TEXT", true);
  }
  bool reward_left = false;
  bool reward_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 494, 188, 24},
                current_reward(state) != nullptr ? current_reward(state)->item_id : "NO REWARD", &reward_left, &reward_right);
  if (reward_left && !quest->rewards.empty()) {
    state->reward_index = (state->reward_index - 1 + static_cast<int>(quest->rewards.size())) % static_cast<int>(quest->rewards.size());
  }
  if (reward_right && !quest->rewards.empty()) {
    state->reward_index = (state->reward_index + 1) % static_cast<int>(quest->rewards.size());
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 202, rect.y + 494, 42, 24}, "+RWD")) {
    mark_project_edit(state);
    quest->rewards.push_back({"", 1, 0, ""});
    state->reward_index = static_cast<int>(quest->rewards.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 248, rect.y + 494, 42, 24}, "-RWD", !quest->rewards.empty(),
             quest->rewards.empty())) {
    if (!quest->rewards.empty()) {
      mark_project_edit(state);
      quest->rewards.erase(quest->rewards.begin() + state->reward_index);
      state->reward_index = 0;
    }
  }
  QuestReward* reward = current_reward(state);
  if (reward != nullptr) {
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 522, 120, 24}, "reward_item_id", reward->item_id,
               "ITEM ID");
    text_field(renderer, input, ui, state, SDL_Rect{rect.x + 136, rect.y + 522, 120, 24}, "reward_sprite_id",
               reward->unlock_sprite_id, "SPRITE ID");
    if (button(renderer, input, ui, SDL_Rect{rect.x + 262, rect.y + 522, 20, 20}, "-")) {
      mark_project_edit(state);
      reward->quantity = std::max(1, reward->quantity - 1);
    }
    if (button(renderer, input, ui, SDL_Rect{rect.x + 286, rect.y + 522, 20, 20}, "+")) {
      mark_project_edit(state);
      reward->quantity += 1;
    }
    draw_text(renderer, "Q" + std::to_string(reward->quantity), rect.x + 310, rect.y + 529, 1, SDL_Color{88, 81, 69, 255});
  }
}

void draw_sprite_panel(SDL_Renderer* renderer,
                       const InputFrame& input,
                       UiFrame* ui,
                       EditorState* state,
                       const SDL_Rect& rect) {
  section_card(renderer, rect, "SPRITE STUDIO");
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 24, 78, 24}, "ADD")) {
    mark_project_edit(state);
    state->project.sprites.push_back(make_sprite_asset("sprite_" + std::to_string(state->project.sprites.size() + 1), "NEW SPRITE",
                                                       false));
    state->sprite_index = static_cast<int>(state->project.sprites.size()) - 1;
  }
  if (button(renderer, input, ui, SDL_Rect{rect.x + 92, rect.y + 24, 78, 24}, "DELETE", !state->project.sprites.empty(),
             state->project.sprites.size() <= 1)) {
    delete_selected_for_tool(state);
  }
  SpriteAsset* sprite = current_sprite(state);
  if (sprite == nullptr) {
    return;
  }
  bool sprite_left = false;
  bool sprite_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 176, rect.y + 24, rect.w - 184, 24}, sprite->name, &sprite_left,
                &sprite_right);
  if (sprite_left && !state->project.sprites.empty()) {
    state->sprite_index = (state->sprite_index - 1 + static_cast<int>(state->project.sprites.size())) %
                          static_cast<int>(state->project.sprites.size());
    sprite = current_sprite(state);
  }
  if (sprite_right && !state->project.sprites.empty()) {
    state->sprite_index = (state->sprite_index + 1) % static_cast<int>(state->project.sprites.size());
    sprite = current_sprite(state);
  }
  draw_text(renderer, scene_sprite_target_label(*state), rect.x + 8, rect.y + 58, 1, SDL_Color{88, 81, 69, 255});
  draw_text(renderer, "CLICK AN NPC OR MONSTER IN THE SCENE TO BIND THIS STUDIO.", rect.x + 8, rect.y + 72, 1,
            SDL_Color{116, 108, 96, 255});
  const bool has_scene_target = state->scene_sprite_target != SceneSpriteTarget::None;
  const char* apply_label = state->scene_sprite_target == SceneSpriteTarget::Monster ? "APPLY TO MON" : "APPLY TO NPC";
  if (button(renderer, input, ui, SDL_Rect{rect.x + rect.w - 118, rect.y + 54, 110, 22}, apply_label, has_scene_target,
             !has_scene_target)) {
    if (NpcData* npc = current_scene_sprite_npc(state)) {
      if (npc->sprite_id != sprite->id) {
        mark_project_edit(state);
        npc->sprite_id = sprite->id;
      }
      set_status(state, "CURRENT SPRITE APPLIED TO NPC.");
    } else if (MonsterData* monster = current_scene_sprite_monster(state)) {
      if (monster->sprite_id != sprite->id) {
        mark_project_edit(state);
        monster->sprite_id = sprite->id;
      }
      set_status(state, "CURRENT SPRITE APPLIED TO MONSTER.");
    }
  }
  label(renderer, rect.x + 8, rect.y + 90, "ID");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 8, rect.y + 100, 146, kFieldHeight}, "sprite_id", sprite->id, "SPRITE ID");
  label(renderer, rect.x + 162, rect.y + 90, "NAME");
  text_field(renderer, input, ui, state, SDL_Rect{rect.x + 162, rect.y + 100, rect.w - 170, kFieldHeight}, "sprite_name",
             sprite->name, "SPRITE NAME");
  if (button(renderer, input, ui, SDL_Rect{rect.x + 8, rect.y + 134, 98, 22}, sprite->monster ? "MONSTER" : "HUMANOID",
             sprite->monster)) {
    mark_project_edit(state);
    sprite->monster = !sprite->monster;
  }
  bool dir_left = false;
  bool dir_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 114, rect.y + 134, 120, 24},
                direction_name(direction_from_index(state->sprite_direction_index)), &dir_left, &dir_right);
  if (dir_left) {
    state->sprite_direction_index = (state->sprite_direction_index + 3) % 4;
  }
  if (dir_right) {
    state->sprite_direction_index = (state->sprite_direction_index + 1) % 4;
  }
  bool frame_left = false;
  bool frame_right = false;
  cycle_buttons(renderer, input, ui, SDL_Rect{rect.x + 242, rect.y + 134, 94, 24},
                "FRAME " + std::to_string(state->sprite_frame_index + 1), &frame_left, &frame_right);
  if (frame_left) {
    state->sprite_frame_index = (state->sprite_frame_index + kSpriteFramesPerDirection - 1) % kSpriteFramesPerDirection;
  }
  if (frame_right) {
    state->sprite_frame_index = (state->sprite_frame_index + 1) % kSpriteFramesPerDirection;
  }
  draw_sprite_asset_preview(renderer, *sprite, state->sprite_direction_index, state->sprite_frame_index,
                            SDL_Rect{rect.x + rect.w - 92, rect.y + 166, 84, 84});
  const SDL_Rect canvas = {rect.x + 8, rect.y + 166, 208, 208};
  fill_rect(renderer, canvas, SDL_Color{250, 246, 238, 255});
  draw_rect(renderer, canvas, SDL_Color{117, 108, 91, 255});
  auto& rows = sprite->frames[static_cast<std::size_t>(sprite_frame_slot(state->sprite_direction_index, state->sprite_frame_index))];
  const int pixel = 12;
  for (int py = 0; py < kSpritePixels; ++py) {
    for (int px = 0; px < kSpritePixels; ++px) {
      SDL_Rect pixel_rect = {canvas.x + px * pixel, canvas.y + py * pixel, pixel, pixel};
      fill_rect(renderer, pixel_rect, ((px + py) & 1) == 0 ? SDL_Color{232, 228, 220, 255} : SDL_Color{242, 238, 230, 255});
      const int brush = sprite_brush_index_for(rows[static_cast<std::size_t>(py)][static_cast<std::size_t>(px)]);
      if (brush != 0) {
        fill_rect(renderer, SDL_Rect{pixel_rect.x + 1, pixel_rect.y + 1, pixel_rect.w - 2, pixel_rect.h - 2},
                  sprite_brush_color(brush));
      }
      draw_rect(renderer, pixel_rect, SDL_Color{213, 204, 186, 255});
      if (point_in_rect(pixel_rect, input.mouse_x, input.mouse_y) && !ui->mouse_consumed && (input.left_down || input.right_down)) {
        if (input.left_pressed || input.right_pressed) {
          mark_project_edit(state);
        }
        rows[static_cast<std::size_t>(py)][static_cast<std::size_t>(px)] =
            input.right_down ? '0' : sprite_brush_symbol(state->sprite_brush_index);
        ui->mouse_consumed = true;
      }
    }
  }
  for (int index = 0; index < 8; ++index) {
    SDL_Rect swatch = {rect.x + 226 + (index % 2) * 54, rect.y + 166 + (index / 2) * 28, 44, 22};
    fill_rect(renderer, swatch, SDL_Color{241, 236, 227, 255});
    draw_rect(renderer, swatch, state->sprite_brush_index == index ? SDL_Color{88, 122, 152, 255} : SDL_Color{128, 119, 102, 255});
    if (index == 0) {
      draw_text(renderer, "ERASE", swatch.x + 5, swatch.y + 8, 1, SDL_Color{84, 79, 72, 255});
    } else {
      fill_rect(renderer, SDL_Rect{swatch.x + 4, swatch.y + 4, 14, 14}, sprite_brush_color(index));
    }
    if (point_in_rect(swatch, input.mouse_x, input.mouse_y) && input.left_pressed && !ui->mouse_consumed) {
      state->sprite_brush_index = index;
      ui->mouse_consumed = true;
    }
  }
}

void draw_inspector(SDL_Renderer* renderer,
                    const InputFrame& input,
                    UiFrame* ui,
                    EditorState* state,
                    const SDL_Rect& rect) {
  fill_rect(renderer, rect, SDL_Color{233, 228, 217, 255});
  draw_rect(renderer, rect, SDL_Color{119, 109, 89, 255});
  int y = rect.y + 10;
  draw_project_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 156});
  y += 156 + kSectionGap;
  draw_area_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 232});
  y += 232 + kSectionGap;
  switch (state->tool) {
    case ToolMode::Paint:
      draw_paint_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 286});
      break;
    case ToolMode::Warp:
      draw_warp_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 288});
      break;
    case ToolMode::Npc:
      draw_npc_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 300});
      break;
    case ToolMode::Monster:
      draw_monster_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 244});
      break;
    case ToolMode::Quest:
      draw_quest_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 560});
      break;
    case ToolMode::Sprite:
      draw_sprite_panel(renderer, input, ui, state, SDL_Rect{rect.x + 8, y, rect.w - 16, 390});
      break;
  }
}

void handle_map_interaction(EditorState* state,
                            const InputFrame& input,
                            UiFrame* ui,
                            const SDL_Rect& canvas) {
  if (state == nullptr || ui == nullptr) {
    return;
  }
  AreaData* area = current_area(state);
  if (area == nullptr) {
    return;
  }

  if (point_in_rect(canvas, input.mouse_x, input.mouse_y) && input.wheel_y != 0 && !ui->mouse_consumed) {
    state->map_zoom = clamp_value(state->map_zoom + static_cast<float>(input.wheel_y) * 0.25f, 0.5f, 8.0f);
    clamp_camera(state, canvas);
  }

  int tile_x = 0;
  int tile_y = 0;
  if (!map_tile_from_mouse(*state, canvas, input.mouse_x, input.mouse_y, &tile_x, &tile_y)) {
    return;
  }

  if (input.shift && input.left_pressed && !ui->mouse_consumed) {
    state->selection_dragging = true;
    state->selection_active = true;
    state->selection_anchor = {tile_x, tile_y};
    state->selection_head = {tile_x, tile_y};
    ui->mouse_consumed = true;
    return;
  }
  if (state->selection_dragging && input.left_down) {
    state->selection_head = {tile_x, tile_y};
    ui->mouse_consumed = true;
    return;
  }
  if (state->selection_dragging && input.left_released) {
    state->selection_head = {tile_x, tile_y};
    state->selection_dragging = false;
    ui->mouse_consumed = true;
    return;
  }

  if (ui->mouse_consumed) {
    return;
  }

  if (state->tool == ToolMode::Paint) {
    if (input.left_pressed) {
      push_undo_snapshot(state);
    }
    if (input.left_down) {
      if (state->brush_kind == BrushKind::Tile) {
        set_layer_tile(area, state->paint_surface, tile_x, tile_y, selected_tile_for(*state));
      } else {
        StampData* stamp = current_stamp(state);
        if (stamp != nullptr) {
          apply_stamp(area, state->paint_surface, *stamp, tile_x, tile_y);
        }
      }
      state->dirty = true;
    } else if (input.right_pressed) {
      push_undo_snapshot(state);
    }
    if (input.right_down) {
      clear_layer_tile(area, state->paint_surface, tile_x, tile_y);
      state->dirty = true;
    }
    return;
  }

  if (state->tool == ToolMode::Warp) {
    if (input.left_pressed) {
      const int existing = find_warp_at(*area, tile_x, tile_y);
      if (state->picking_warp_target) {
        WarpData* warp = current_warp(state);
        if (warp != nullptr) {
          push_undo_snapshot(state);
          warp->target_x = tile_x;
          warp->target_y = tile_y;
          state->picking_warp_target = false;
          state->dirty = true;
          set_status(state, "WARP TARGET TILE SET.");
        }
      } else if (existing >= 0) {
        state->warp_index = existing;
      } else {
        push_undo_snapshot(state);
        area->warps.push_back({tile_x, tile_y, 1, 1, "NEW WARP", area->id, tile_x, tile_y, Direction::Down});
        state->warp_index = static_cast<int>(area->warps.size()) - 1;
        state->dirty = true;
        set_status(state, "WARP CREATED.");
      }
    } else if (input.right_pressed) {
      const int existing = find_warp_at(*area, tile_x, tile_y);
      if (existing >= 0) {
        push_undo_snapshot(state);
        area->warps.erase(area->warps.begin() + existing);
        state->warp_index = -1;
        state->dirty = true;
        set_status(state, "WARP REMOVED.");
      }
    }
    return;
  }

  if (state->tool == ToolMode::Npc) {
    if (input.left_pressed) {
      const int existing = find_npc_at(*area, tile_x, tile_y);
      if (existing >= 0) {
        state->npc_index = existing;
      } else {
        push_undo_snapshot(state);
        const int number = static_cast<int>(area->npcs.size()) + 1;
        area->npcs.push_back({"npc_" + std::to_string(number), "New NPC", SpriteRole::Monk, "", tile_x, tile_y,
                              Direction::Down, true, "Write dialogue here."});
        state->npc_index = static_cast<int>(area->npcs.size()) - 1;
        state->dirty = true;
        set_status(state, "NPC CREATED.");
      }
    } else if (input.right_pressed) {
      const int existing = find_npc_at(*area, tile_x, tile_y);
      if (existing >= 0) {
        push_undo_snapshot(state);
        area->npcs.erase(area->npcs.begin() + existing);
        state->npc_index = -1;
        state->dirty = true;
        set_status(state, "NPC REMOVED.");
      }
    }
    return;
  }

  if (state->tool == ToolMode::Monster) {
    if (input.left_pressed) {
      const int existing = find_monster_at(*area, tile_x, tile_y);
      if (existing >= 0) {
        state->monster_index = existing;
      } else {
        push_undo_snapshot(state);
        const int number = static_cast<int>(area->monsters.size()) + 1;
        area->monsters.push_back({"monster_" + std::to_string(number), "New Monster", "ratling", tile_x, tile_y,
                                  Direction::Down, 6, 1, true});
        state->monster_index = static_cast<int>(area->monsters.size()) - 1;
        state->dirty = true;
        set_status(state, "MONSTER CREATED.");
      }
    } else if (input.right_pressed) {
      const int existing = find_monster_at(*area, tile_x, tile_y);
      if (existing >= 0) {
        push_undo_snapshot(state);
        area->monsters.erase(area->monsters.begin() + existing);
        state->monster_index = -1;
        state->dirty = true;
        set_status(state, "MONSTER REMOVED.");
      }
    }
    return;
  }

  if (state->tool == ToolMode::Sprite) {
    if (input.left_pressed) {
      const int npc_index = find_npc_at(*area, tile_x, tile_y);
      if (npc_index >= 0) {
        if (bind_sprite_editor_to_npc(state, npc_index, true)) {
          set_status(state, "NPC SPRITE BOUND TO STUDIO.");
        } else {
          set_status(state, "NPC SPRITE COULD NOT BE BOUND.");
        }
        return;
      }
      const int monster_index = find_monster_at(*area, tile_x, tile_y);
      if (monster_index >= 0) {
        if (bind_sprite_editor_to_monster(state, monster_index, true)) {
          set_status(state, "MONSTER SPRITE BOUND TO STUDIO.");
        } else {
          set_status(state, "MONSTER SPRITE COULD NOT BE BOUND.");
        }
        return;
      }
      set_status(state, "CLICK AN NPC OR MONSTER TO EDIT ITS SPRITE.");
    } else if (input.right_pressed) {
      clear_scene_sprite_target(state);
      set_status(state, "SCENE SPRITE TARGET CLEARED.");
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  for (int index = 1; index < argc; ++index) {
    if (std::string(argv[index]) == "--smoke-test") {
      return run_smoke_test() ? 0 : 1;
    }
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("Priory Editor",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        kLaunchWindowWidth,
                                        kLaunchWindowHeight,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  if (renderer == nullptr) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
  }
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_StartTextInput();

  EditorState state;
  state.project = make_seed_project();
  state.project_path = default_project_path();
  state.resolution_index = default_resolution_index();
  std::string load_error;
  ProjectData loaded_project;
  if (load_project(&loaded_project, state.project_path, &load_error)) {
    state.project = std::move(loaded_project);
    state.dirty = false;
  }
  sanitize_selection(&state);

  bool running = true;
  while (running) {
    InputFrame input;
    int raw_mouse_x = 0;
    int raw_mouse_y = 0;
    SDL_GetMouseState(&raw_mouse_x, &raw_mouse_y);
    const SDL_Point initial_mouse = scale_window_point_to_render(renderer, window, raw_mouse_x, raw_mouse_y);
    input.mouse_x = initial_mouse.x;
    input.mouse_y = initial_mouse.y;

    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      switch (event.type) {
        case SDL_QUIT:
          running = false;
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON_LEFT) {
            input.left_pressed = true;
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            input.right_pressed = true;
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (event.button.button == SDL_BUTTON_LEFT) {
            input.left_released = true;
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            input.right_released = true;
          }
          break;
        case SDL_MOUSEWHEEL:
          input.wheel_y += event.wheel.y;
          break;
        case SDL_TEXTINPUT:
          if (!state.launch_active) {
            append_text(&state, event.text.text);
          }
          break;
        case SDL_KEYDOWN:
          if (state.launch_active) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
              if (state.launch_selection == 3) {
                running = false;
              } else {
                state.launch_selection = 3;
              }
            } else if (event.key.keysym.sym == SDLK_UP) {
              state.launch_selection = (state.launch_selection + 3) % 4;
            } else if (event.key.keysym.sym == SDLK_DOWN) {
              state.launch_selection = (state.launch_selection + 1) % 4;
            } else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT) {
              const int delta = event.key.keysym.sym == SDLK_RIGHT ? 1 : -1;
              if (state.launch_selection == 0) {
                const int count = static_cast<int>(resolution_options().size());
                state.resolution_index = (state.resolution_index + count + delta) % count;
              } else if (state.launch_selection == 1) {
                state.fullscreen = !state.fullscreen;
              }
            } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER ||
                       event.key.keysym.sym == SDLK_SPACE) {
              if (state.launch_selection == 2) {
                state.launch_active = false;
                state.video_dirty = true;
                set_status(&state, "VIDEO SETTINGS APPLIED.");
              } else if (state.launch_selection == 3) {
                running = false;
              } else if (state.launch_selection == 1) {
                state.fullscreen = !state.fullscreen;
              }
            }
            break;
          }

          if (event.key.keysym.sym == SDLK_ESCAPE) {
            if (!state.focus_id.empty()) {
              state.focus_id.clear();
            } else {
              running = false;
            }
          } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
            erase_last_text_character(&state);
          } else if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) &&
                     focus_is_multiline(state)) {
            append_text(&state, "\n");
          } else if (event.key.keysym.sym == SDLK_1) {
            state.tool = ToolMode::Paint;
          } else if (event.key.keysym.sym == SDLK_2) {
            state.tool = ToolMode::Warp;
          } else if (event.key.keysym.sym == SDLK_3) {
            state.tool = ToolMode::Npc;
          } else if (event.key.keysym.sym == SDLK_4) {
            state.tool = ToolMode::Monster;
          } else if (event.key.keysym.sym == SDLK_5) {
            state.tool = ToolMode::Quest;
          } else if (event.key.keysym.sym == SDLK_6) {
            state.tool = ToolMode::Sprite;
          } else if (event.key.keysym.sym == SDLK_g) {
            state.show_grid = !state.show_grid;
          } else if (event.key.keysym.sym == SDLK_DELETE) {
            delete_selected_for_tool(&state);
          } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {
            state.map_zoom = clamp_value(state.map_zoom - 0.25f, 0.5f, 8.0f);
          } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {
            state.map_zoom = clamp_value(state.map_zoom + 0.25f, 0.5f, 8.0f);
          } else if (event.key.keysym.sym == SDLK_LEFT) {
            state.camera_x = std::max(0, state.camera_x - 16);
          } else if (event.key.keysym.sym == SDLK_RIGHT) {
            state.camera_x += 16;
          } else if (event.key.keysym.sym == SDLK_UP) {
            state.camera_y = std::max(0, state.camera_y - 16);
          } else if (event.key.keysym.sym == SDLK_DOWN) {
            state.camera_y += 16;
          } else if (event.key.keysym.sym == SDLK_F11) {
            state.fullscreen = !state.fullscreen;
            state.video_dirty = true;
          } else if ((SDL_GetModState() & KMOD_CTRL) != 0 && event.key.keysym.sym == SDLK_s) {
            std::string error;
            if (save_project(state.project, state.project_path, &error)) {
              state.dirty = false;
              set_status(&state, "PROJECT SAVED.");
            } else {
              set_status(&state, "SAVE FAILED: " + error);
            }
          } else if ((SDL_GetModState() & KMOD_CTRL) != 0 && event.key.keysym.sym == SDLK_z) {
            if (undo_last_change(&state)) {
              set_status(&state, "UNDID LAST CHANGE.");
            }
          } else if ((SDL_GetModState() & KMOD_CTRL) != 0 && event.key.keysym.sym == SDLK_l) {
            ProjectData project;
            std::string error;
            if (load_project(&project, state.project_path, &error)) {
              state.project = std::move(project);
              state.dirty = false;
              state.paint_surface = PaintSurface::Ground;
              state.brush_kind = BrushKind::Tile;
              state.selection_active = false;
              clear_scene_sprite_target(&state);
              sanitize_selection(&state);
              set_status(&state, "PROJECT LOADED.");
            } else {
              set_status(&state, "LOAD FAILED: " + error);
            }
          }
          break;
        default:
          break;
      }
    }

    const Uint32 mouse_state = SDL_GetMouseState(&raw_mouse_x, &raw_mouse_y);
    const SDL_Point scaled_mouse = scale_window_point_to_render(renderer, window, raw_mouse_x, raw_mouse_y);
    input.mouse_x = scaled_mouse.x;
    input.mouse_y = scaled_mouse.y;
    input.left_down = (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    input.right_down = (mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    const SDL_Keymod mods = SDL_GetModState();
    input.shift = (mods & KMOD_SHIFT) != 0;
    input.ctrl = (mods & KMOD_CTRL) != 0;

    state.now = SDL_GetTicks();
    sanitize_selection(&state);

    if (state.video_dirty) {
      apply_video_settings(window, &state);
    }

    int window_width = 0;
    int window_height = 0;
    SDL_GetRendererOutputSize(renderer, &window_width, &window_height);

    SDL_SetRenderDrawColor(renderer, 196, 214, 226, 255);
    SDL_RenderClear(renderer);

    if (state.launch_active) {
      draw_launch_menu(renderer, state, window_width, window_height);
    } else {
      const Layout layout = compute_layout(window_width, window_height);
      clamp_camera(&state, layout.map);

      UiFrame ui;
      draw_header(renderer, input, &ui, &state, layout.header);
      draw_sidebar(renderer, input, &ui, &state, layout.sidebar);
      draw_inspector(renderer, input, &ui, &state, layout.inspector);
      handle_map_interaction(&state, input, &ui, layout.map);
      draw_area_canvas(renderer, input, state, layout.map);
      draw_map_overlay_labels(renderer, state, layout.map);
      draw_world_overview(renderer, state, layout.map);
      draw_status(renderer, state, layout.status);
    }

    SDL_RenderPresent(renderer);
  }

  SDL_StopTextInput();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
