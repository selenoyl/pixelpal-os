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
constexpr Uint32 kSwordSwingMs = 180;
constexpr Uint32 kAttackCooldownMs = 240;
constexpr Uint32 kMonsterMoveMs = 480;
constexpr Uint32 kMonsterAttackMs = 860;
constexpr Uint32 kAreaBannerMs = 2200;
constexpr Uint32 kStatusMessageMs = 1800;
constexpr Uint32 kWarpCooldownMs = 250;
constexpr int kDialogHeight = 78;
constexpr int kDialogBodyWidth = kScreenWidth - 36;
constexpr int kDialogBodyHeight = 38;
constexpr int kPlayerAttackDamage = 3;

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
constexpr char kChest = 't';
constexpr char kBed = 'l';

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

enum class MonsterKind {
  Ratling = 0,
  BogWisp,
  DockHound,
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
  Uint32 ambient_turn_at = 0;
  Uint32 facing_hold_until = 0;
  int ambient_turn_count = 0;
};

struct Monster {
  std::string id;
  std::string name;
  MonsterKind kind = MonsterKind::Ratling;
  int x = 0;
  int y = 0;
  Direction facing = Direction::Down;
  int hp = 6;
  int max_hp = 6;
  int attack = 1;
  bool aggressive = true;
  Uint32 move_at = 0;
  Uint32 attack_at = 0;
  Uint32 hurt_until = 0;
};

struct Area {
  std::string id;
  std::string name;
  bool indoor = false;
  bool player_tillable = false;
  std::vector<std::string> tiles;
  std::vector<Warp> warps;
  std::vector<Npc> npcs;
  std::vector<Monster> monsters;
};

struct InventoryItemDefinition {
  std::string id;
  std::string name;
  std::string description;
  int heal_amount = 0;
  std::string crop_id;
  char decor_tile = '\0';
  bool decor_blocks = false;
};

struct CropDefinition {
  std::string id;
  std::string seed_item_id;
  std::string harvest_item_id;
  std::string name;
  int days_to_grow = 2;
  int yield_count = 1;
  SDL_Color sprout{118, 166, 88, 255};
  SDL_Color mature{156, 192, 104, 255};
};

struct CropPlot {
  std::string area_id;
  int x = 0;
  int y = 0;
  bool tilled = false;
  std::string crop_id;
  int stage = 0;
};

struct DecorPlacement {
  std::string area_id;
  int x = 0;
  int y = 0;
  std::string item_id;
};

struct LifePathDefinition {
  std::string id;
  std::string name;
  std::string description;
  int coin_min = 0;
  int coin_max = 0;
  std::vector<std::string> starter_items;
  std::string starter_outfit_id;
  std::array<std::string, 2> reflection_text{};
  std::array<std::string, 2> reflection_response{};
};

struct ToneDefinition {
  std::string name;
  SDL_Color color;
};

struct OutfitDefinition {
  std::string id;
  std::string name;
  std::string description;
  SDL_Color primary;
  SDL_Color secondary;
  SDL_Color accent;
  SDL_Color boots;
};

enum class ShopId {
  None = 0,
  BlackpineCloth,
  HospiceSupply,
  ScriptoriumSupply,
  PropertyCharter,
};

struct ShopOffer {
  std::string label;
  std::string description;
  int price = 0;
  std::string item_id;
  std::string outfit_id;
  std::string home_id;
  std::string status_message;
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
  bool paginate = false;
  std::size_t max_lines = 0;
  int line_gap = 4;
  TextAlign align = TextAlign::Left;
  TextVerticalAlign vertical_align = TextVerticalAlign::Top;
};

struct DialogueScene {
  bool active = false;
  std::string speaker;
  std::vector<TextPageLayout> pages;
  std::size_t page_index = 0;
  ShopId shop_after = ShopId::None;
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
  int hp = 12;
  int max_hp = 12;
  Uint32 swing_until = 0;
  Uint32 attack_cooldown_until = 0;
};

struct QuestState {
  bool started = false;
  bool wax = false;
  bool leaf = false;
  bool oil = false;
  bool complete = false;
};

struct ProgressState {
  bool father_counsel = false;
  bool mother_blessing = false;
  bool friar_blessing = false;
  bool gate_opened = false;
  bool task_board_read = false;
  bool met_martin = false;
  bool met_prioress = false;
  bool visited_blackpine = false;
  bool visited_village_green = false;
  bool visited_quay = false;
  int chapel_prayer_index = 0;
};

struct ProfileState {
  int life_path_index = 0;
  int reflection_index = 0;
  int complexion_index = 0;
  int hair_index = 0;
  int day = 1;
  int coins = 0;
  std::map<std::string, int> inventory;
  std::map<std::string, int> stored_items;
  std::map<std::string, int> virtues;
  std::vector<std::string> unlocked_outfits;
  std::vector<std::string> owned_homes;
  std::string active_home_id;
  std::string active_item_id;
  std::string equipped_outfit_id = "lay_tunic";
  int journal_page = 0;
  int pack_selection = 0;
  int wardrobe_selection = 0;
};

struct CreationState {
  bool active = false;
  int row = 0;
};

struct ShopState {
  bool active = false;
  ShopId id = ShopId::None;
  int selection = 0;
};

struct ChestState {
  bool active = false;
  bool browse_storage = false;
  int inventory_selection = 0;
  int storage_selection = 0;
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
  ProfileState profile;
  DialogueScene dialogue;
  QuestState quest;
  ProgressState progress;
  std::vector<CropPlot> crop_plots;
  std::vector<DecorPlacement> decor_placements;
  bool journal_open = false;
  bool on_title = true;
  bool has_save = false;
  int title_selection = 0;
  CreationState creation;
  ShopState shop;
  ChestState chest;
  std::string status_message;
  Uint32 status_until = 0;
  std::string area_banner = "SAINT CATHERINE COURTYARD";
  Uint32 area_banner_until = 0;
  Uint32 warp_cooldown_until = 0;
  Uint32 bells_until = 0;
  bool smoke_test = false;
};

const ToneDefinition& current_complexion(const GameState& state);
const ToneDefinition& current_hair(const GameState& state);
const OutfitDefinition* outfit_definition(const std::string& id);

const std::vector<InventoryItemDefinition>& inventory_definitions() {
  static const std::vector<InventoryItemDefinition> kDefinitions = {
      {"psalter_worn", "Psalter (worn)", "A thumbed psalter carried for the Hours."},
      {"small_wooden_rosary", "Small wooden rosary", "Simple beads for quiet prayer on the road."},
      {"plain_tunic", "Plain tunic", "A serviceable tunic suited to an aspirant's first months."},
      {"ink_stained_notebook", "Ink-stained notebook", "Notes, copied phrases, and half-finished arguments."},
      {"charcoal_stylus", "Charcoal and stylus", "Cheap writing tools kept wrapped in cloth."},
      {"worn_arming_dagger", "Worn arming dagger", "Steel memory from a harsher trade."},
      {"patched_wool_cloak", "Patched wool cloak", "A weathered cloak mended more than once."},
      {"sealed_letter", "Sealed letter with faded crest", "A letter bearing the old summons toward Saint Catherine."},
      {"ledger_scraps", "Ledger scraps", "Abandoned account slips worth more than they first seem."},
      {"small_scale_weights", "Small scale weights", "Merchant weights kept in a leather purse."},
      {"wax_tally_tablets", "Wax tally tablets", "Reusable tablets for reckoning debts and stores."},
      {"work_gloves", "Work gloves", "Good leather for cold soil and rough rope."},
      {"farm_knife", "Farm knife", "A narrow knife for work before and after dawn."},
      {"seed_pouch", "Seed pouch", "A reminder that survival begins with patient labor."},
      {"field_tools", "Field Tool Set", "Strong tools for earth, ditch, and priory ground."},
      {"worn_needle_kit", "Worn needle kit", "Needles and thread kept in oiled cloth by careful hands."},
      {"lantern_oil", "Lantern Oil Flask", "Oil enough to keep a lamp alive through a hard evening."},
      {"wax_candles", "Wax Candles", "Chapel candles fit for altar or study desk."},
      {"barley_sack", "Barley Sack", "A coarse sack of grain for kitchens or winter stores."},
      {"iron_nails", "Iron Nails", "Useful wherever Saint Catherine still needs rebuilding."},
      {"healing_herbs", "Healing Herbs", "Dried herbs the Sisters of Saint Hilda trust in fever season."},
      {"parchment_bundle", "Parchment Bundle", "A bound packet of clean sheets for records and copying."},
      {"ink_vial", "Ink Vial", "Dark ink logged out of the scriptorium stores."},
      {"way_marker_ribbon", "Way-marker ribbon", "A faded ribbon once tied to a safer road."},
      {"rosary_boxwood", "Boxwood Rosary", "A sturdy rosary favored by traveling clergy."},
      {"wool_cloak_fine", "Fine Wool Cloak", "Warm wool fit for winter travel and public errands."},
      {"rye_loaf", "Rye Loaf", "A dense market loaf still warm from Blackpine's ovens.", 4},
      {"leek_soup", "Leek Soup", "A clay bowl of broth and softened leeks.", 5},
      {"fish_stew", "Fish Stew", "Quay stew rich with broth, barley, and white fish.", 6},
      {"bean_pottage", "Bean Pottage", "A sustaining pot of beans, onions, and dripping.", 5},
      {"herb_broth", "Herb Broth", "Light broth steeped with mint and rosemary.", 3},
      {"smoked_herring", "Smoked Herring", "Salt fish wrapped in paper for the road.", 4},
      {"cheese_wedge", "Cheese Wedge", "A hard wedge cut for travel and work breaks.", 3},
      {"honeyed_apple", "Honeyed Apple", "A preserved apple glossed with a little honey.", 2},
      {"oat_cakes", "Oat Cakes", "Dry cakes that travel better than soft bread.", 3},
      {"abbey_ale_small", "Small Abbey Ale", "A weak ale safer than foul water after a long patrol.", 2},
      {"cabbage_seed", "Cabbage Seed Packet", "Seed wrapped in waxed linen for a heavy-headed brassica.", 0, "cabbage"},
      {"leek_seed", "Leek Seed Packet", "Black seed for kitchen leeks and broth greens.", 0, "leek"},
      {"bean_seed", "Bean Seed Packet", "Pole bean seed that climbs a simple stake.", 0, "bean"},
      {"turnip_seed", "Turnip Seed Packet", "Common turnip seed for quick roots.", 0, "turnip"},
      {"barley_seed", "Barley Seed Packet", "Barley fit for porridge, broth, and ale mashes.", 0, "barley"},
      {"onion_seed", "Onion Set Packet", "Small onion sets wrapped for planting.", 0, "onion"},
      {"cabbage_head", "Cabbage Head", "A tightly packed head cut fresh from good soil.", 5},
      {"leek_bundle", "Leek Bundle", "A tied bundle of leeks with pale shafts and green tops.", 4},
      {"bean_pod_basket", "Bean Pod Basket", "Broad beans gathered in a rush basket.", 4},
      {"turnip_root", "Turnip Root", "A quick-growing root with a peppered sweetness.", 3},
      {"barley_sheaf", "Barley Sheaf", "A cut sheaf that can be cooked down or milled.", 3},
      {"onion_bulb", "Onion Bulb", "A sharp onion fit for stews and frying pans.", 3},
      {"rush_bench_item", "Rush-Woven Bench", "A simple household bench for the cottage wall.", 0, "", kBench, true},
      {"oak_iconshelf_item", "Oak Icon Shelf", "A narrow oak shelf for small images and candles.", 0, "", kBoard},
      {"linen_hangings_item", "Linen Wall Hangings", "Worked linen that softens bare walls.", 0, "", kLaundry},
      {"herb_chest_item", "Herbal Cedar Chest", "A cedar chest for linens, herbs, and winter salves.", 0, "", kChest, true},
      {"guest_table_item", "Pilgrim Guest Table", "A broad table for bread, letters, and counsel.", 0, "", kDesk, true},
      {"hearth_tiles_item", "Glazed Hearth Tiles", "Painted tiles to brighten the hearth edge.", 0, "", kRug},
      {"prayer_stool_item", "Prayer Stool", "A small stool placed near an icon corner.", 0, "", kBench, true},
      {"wax_lantern_item", "Wax Lantern", "A hanging lantern for gentler evening light.", 0, "", kCandle},
      {"wool_tapestry_item", "Wool Tapestry", "A woven wall hanging that warms a narrow room.", 0, "", kLaundry},
  };
  return kDefinitions;
}

const std::vector<CropDefinition>& crop_definitions() {
  static const std::vector<CropDefinition> kDefinitions = {
      {"cabbage", "cabbage_seed", "cabbage_head", "Cabbage", 3, 1, SDL_Color{116, 166, 96, 255},
       SDL_Color{154, 190, 116, 255}},
      {"leek", "leek_seed", "leek_bundle", "Leek", 2, 1, SDL_Color{98, 154, 92, 255},
       SDL_Color{138, 180, 104, 255}},
      {"bean", "bean_seed", "bean_pod_basket", "Bean", 3, 2, SDL_Color{90, 148, 88, 255},
       SDL_Color{124, 176, 96, 255}},
      {"turnip", "turnip_seed", "turnip_root", "Turnip", 2, 1, SDL_Color{114, 160, 104, 255},
       SDL_Color{186, 177, 124, 255}},
      {"barley", "barley_seed", "barley_sheaf", "Barley", 3, 2, SDL_Color{122, 160, 92, 255},
       SDL_Color{209, 188, 108, 255}},
      {"onion", "onion_seed", "onion_bulb", "Onion", 3, 1, SDL_Color{96, 152, 96, 255},
       SDL_Color{202, 180, 134, 255}},
  };
  return kDefinitions;
}

const std::vector<LifePathDefinition>& life_path_definitions() {
  static const std::vector<LifePathDefinition> kDefinitions = {
      {"1_lay_aspirant",
       "Lay Aspirant",
       "You go to test a vocation among the Dominicans.",
       2,
       6,
       {"psalter_worn", "small_wooden_rosary", "plain_tunic"},
       "lay_tunic",
       {{"Confess fear of false zeal.", "Ask for a rule of life from day one."}},
       {{"You admit your fear and choose patience over display.",
         "You commit to discipline, even when unseen."}}},
      {"2_scholar_son",
       "Scholar's Son",
       "Literate, modest Latin, drawn to study.",
       4,
       8,
       {"ink_stained_notebook", "psalter_worn", "charcoal_stylus"},
       "scholar_wool",
       {{"Pride in your learning.", "Fear that study is useless to the poor."}},
       {{"You resolve to argue less and listen more.",
         "You vow to wed doctrine to mercy."}}},
      {"3_former_man_at_arms",
       "Former Man-at-Arms",
       "You have known steel and discipline.",
       5,
       9,
       {"worn_arming_dagger", "patched_wool_cloak", "small_wooden_rosary", "sealed_letter"},
       "guard_cloak",
       {{"Use force only as last defense.", "Keep readiness without apology."}},
       {{"You set a hard boundary against old habits.",
         "You accept that danger is real, and so is duty."}}},
      {"4_merchant_apprentice",
       "Merchant's Apprentice",
       "Trade and negotiation come naturally.",
       8,
       14,
       {"ledger_scraps", "small_scale_weights", "wax_tally_tablets"},
       "merchant_doublet",
       {{"Profit as tool, never master.", "Stability first, sentiment second."}},
       {{"You frame coin as stewardship, not devotion.",
         "You lean into hard prudence for hard times."}}},
      {"5_farmer_son",
       "Farmer's Son",
       "You know land, weather, and toil.",
       3,
       7,
       {"work_gloves", "farm_knife", "seed_pouch"},
       "field_tunic",
       {{"Feed people before winning arguments.", "Order first; chaos starves everyone."}},
       {{"You prioritize bread, then policy.",
         "You choose structure, hoping mercy can live inside it."}}},
  };
  return kDefinitions;
}

const std::vector<ToneDefinition>& complexion_definitions() {
  static const std::vector<ToneDefinition> kDefinitions = {
      {"Fen Fair", SDL_Color{245, 216, 182, 255}},
      {"North Sea Rose", SDL_Color{232, 198, 166, 255}},
      {"York Olive", SDL_Color{210, 176, 138, 255}},
      {"Pilgrim Umber", SDL_Color{168, 123, 86, 255}},
  };
  return kDefinitions;
}

const std::vector<ToneDefinition>& hair_definitions() {
  static const std::vector<ToneDefinition> kDefinitions = {
      {"Ash Brown", SDL_Color{96, 69, 47, 255}},
      {"Black Pine", SDL_Color{45, 38, 34, 255}},
      {"Harvest Gold", SDL_Color{150, 109, 64, 255}},
      {"Silvered", SDL_Color{160, 156, 149, 255}},
  };
  return kDefinitions;
}

const std::vector<OutfitDefinition>& outfit_definitions() {
  static const std::vector<OutfitDefinition> kDefinitions = {
      {"lay_tunic", "Plain Tunic", "A novice plainness proper to a lay aspirant.",
       SDL_Color{76, 98, 128, 255}, SDL_Color{225, 213, 188, 255},
       SDL_Color{214, 190, 132, 255}, SDL_Color{92, 68, 52, 255}},
      {"scholar_wool", "Ink-Stained Wool", "Scholar's wool with darker sleeves and candle marks.",
       SDL_Color{80, 91, 118, 255}, SDL_Color{210, 205, 187, 255},
       SDL_Color{179, 151, 103, 255}, SDL_Color{78, 60, 50, 255}},
      {"guard_cloak", "Patched Wool Cloak", "A repaired cloak fit for road duty and harsh weather.",
       SDL_Color{126, 72, 67, 255}, SDL_Color{110, 108, 115, 255},
       SDL_Color{204, 171, 112, 255}, SDL_Color{72, 52, 42, 255}},
      {"merchant_doublet", "Market Apprentice", "Practical layers cut for ledgers, stalls, and rain.",
       SDL_Color{82, 117, 76, 255}, SDL_Color{213, 196, 159, 255},
       SDL_Color{225, 184, 114, 255}, SDL_Color{86, 61, 41, 255}},
      {"field_tunic", "Field Tunic", "Hard-wearing cloth for cold mornings and rough priory labor.",
       SDL_Color{101, 121, 72, 255}, SDL_Color{203, 192, 150, 255},
       SDL_Color{215, 180, 109, 255}, SDL_Color{88, 64, 43, 255}},
      {"blackpine_trim", "Blackpine Trim", "Alisoun's wool trim for someone now seen around the green.",
       SDL_Color{73, 104, 135, 255}, SDL_Color{214, 202, 181, 255},
       SDL_Color{164, 68, 61, 255}, SDL_Color{82, 61, 46, 255}},
      {"waymarker_ribbon_outfit", "Way-marker Ribbon", "A faded ribbon and brighter trim for long errands.",
       SDL_Color{92, 102, 69, 255}, SDL_Color{212, 201, 172, 255},
       SDL_Color{195, 148, 66, 255}, SDL_Color{82, 61, 46, 255}},
      {"fine_wool_cloak_outfit", "Fine Wool Cloak", "Warm wool fit for York roads and winter visits.",
       SDL_Color{66, 84, 120, 255}, SDL_Color{230, 221, 198, 255},
       SDL_Color{206, 164, 103, 255}, SDL_Color{64, 49, 40, 255}},
      {"scriptorium_apron", "Scriptorium Apron", "A protective apron worn by the careful keeper of ink.",
       SDL_Color{88, 76, 112, 255}, SDL_Color{205, 197, 185, 255},
       SDL_Color{187, 146, 93, 255}, SDL_Color{78, 59, 45, 255}},
  };
  return kDefinitions;
}

const std::vector<ShopOffer>& blackpine_shop_offers() {
  static const std::vector<ShopOffer> kOffers = {
      {"Buy Blackpine Trim (4)", "A wool-trimmed mantle from Alisoun's cloth stall.", 4, "", "blackpine_trim", "",
       "ALISOUN FITS A NEW BLACKPINE TRIM."},
      {"Buy Way-marker Ribbon (1)", "A faded ribbon reworked into a brighter traveling trim.", 1, "way_marker_ribbon",
       "waymarker_ribbon_outfit", "", "ALISOUN ADDS THE WAY-MARKER RIBBON TO YOUR KIT."},
      {"Buy Field Tool Set (5)", "An iron hoe, mattock, and hand fork for a small holding.", 5, "field_tools", "", "",
       "YOU PURCHASE A WORKING FIELD TOOL SET."},
      {"Buy Cabbage Seed Packet (2)", "Reliable seed for a heavy kitchen crop.", 2, "cabbage_seed", "", "",
       "YOU PURCHASE CABBAGE SEED."},
      {"Buy Bean Seed Packet (2)", "Beans for broth and the poor table.", 2, "bean_seed", "", "",
       "YOU PURCHASE BEAN SEED."},
      {"Buy Turnip Seed Packet (1)", "Quick roots for poor soil and quick hunger.", 1, "turnip_seed", "", "",
       "YOU PURCHASE TURNIP SEED."},
      {"Buy Rye Loaf (2)", "Fresh bread from Margery's ovens.", 2, "rye_loaf", "", "",
       "YOU PURCHASE A HOT RYE LOAF."},
      {"Buy Smoked Herring (2)", "Salt fish wrapped for market travel.", 2, "smoked_herring", "", "",
       "YOU PURCHASE SMOKED HERRING."},
      {"Buy Lantern Oil (2)", "Useful for chapel light, hospice lamps, and long roads.", 2, "lantern_oil", "", "",
       "YOU PURCHASE LANTERN OIL."},
      {"Buy Wax Candles (2)", "Wax candles fit for altar, desk, or winter watch.", 2, "wax_candles", "", "",
       "YOU PURCHASE WAX CANDLES."},
      {"Buy Healing Herbs (3)", "Fresh herbs brought through Blackpine's rising market.", 3, "healing_herbs", "", "",
       "YOU PURCHASE HEALING HERBS."},
  };
  return kOffers;
}

const std::vector<ShopOffer>& hospice_shop_offers() {
  static const std::vector<ShopOffer> kOffers = {
      {"Buy Fine Wool Cloak (6)", "Warm wool fit for York roads and hospice winter rounds.", 6, "wool_cloak_fine",
       "fine_wool_cloak_outfit", "", "THE SISTERS SET ASIDE A FINE WOOL CLOAK FOR YOU."},
      {"Buy Herb Broth (2)", "Restorative broth for weakness after the road.", 2, "herb_broth", "", "",
       "THE SISTERS POUR YOU A POT OF HERB BROTH."},
      {"Buy Leek Soup (3)", "A warm bowl of leek soup from the sisters' kitchen.", 3, "leek_soup", "", "",
       "YOU RECEIVE A BOWL OF LEEK SOUP FOR THE ROAD."},
      {"Buy Healing Herbs (3)", "Bundled herbs the Sisters of Saint Hilda trust in fever season.", 3, "healing_herbs",
       "", "", "YOU PURCHASE HEALING HERBS FROM THE SISTERS."},
      {"Buy Lantern Oil (2)", "Practical lamp oil for wards, road chapels, or priory halls.", 2, "lantern_oil", "", "",
       "YOU PURCHASE HOSPICE LANTERN OIL."},
      {"Buy Boxwood Rosary (3)", "A sturdier rosary for long roads and longer duties.", 3, "rosary_boxwood", "", "",
       "YOU PURCHASE A BOXWOOD ROSARY."},
    };
  return kOffers;
}

const std::vector<ShopOffer>& scriptorium_shop_offers() {
  static const std::vector<ShopOffer> kOffers = {
      {"Acquire Parchment Bundle (4)", "Logged sheets for records, sermon notes, and copied leaves.", 4,
       "parchment_bundle", "", "", "YOU SECURE A PARCHMENT BUNDLE."},
      {"Acquire Ink Vial (2)", "A measured vial drawn from the prior's restricted stores.", 2, "ink_vial", "", "",
       "YOU SECURE WRITING INK."},
      {"Acquire Wax Candles (2)", "Extra study candles for late copying and catalog work.", 2, "wax_candles", "", "",
       "YOU SECURE EXTRA STUDY CANDLES."},
      {"Acquire Scriptorium Apron (4)", "A careful apron for ink, wax, and logged discipline.", 4, "",
       "scriptorium_apron", "", "BROTHER MARTIN RELEASES A SCRIPTORIUM APRON TO YOU."},
  };
  return kOffers;
}

const std::vector<ShopOffer>& property_shop_offers() {
  static const std::vector<ShopOffer> kOffers = {
      {"Buy Blackpine Cottage (24)", "A whitewashed cottage beyond the green with an icon corner and storage chest.", 24,
       "", "", "blackpine-cottage", "YSABEL SEALS THE BLACKPINE COTTAGE CHARTER."},
      {"Buy Rush-Woven Bench (6)", "A simple bench to anchor one wall of your cottage.", 6, "rush_bench_item", "", "",
       "A RUSH-WOVEN BENCH IS SENT TO YOUR COTTAGE."},
      {"Buy Oak Icon Shelf (8)", "A narrow oak shelf for icons, candles, and letters.", 8, "oak_iconshelf_item", "", "",
       "YSABEL ARRANGES DELIVERY OF AN OAK ICON SHELF."},
      {"Buy Linen Wall Hangings (7)", "Worked linen that softens bare boards and stone.", 7, "linen_hangings_item", "", "",
       "LINEN WALL HANGINGS ARE SET ASIDE FOR YOUR HOUSEHOLD."},
      {"Buy Herbal Cedar Chest (9)", "A cedar chest for linen, herbs, and household records.", 9, "herb_chest_item", "", "",
       "A CEDAR CHEST IS ENTERED INTO YOUR HOUSEHOLD ACCOUNT."},
      {"Buy Pilgrim Guest Table (10)", "A broad table for bread, alms, and counsel.", 10, "guest_table_item", "", "",
       "A PILGRIM GUEST TABLE IS DELIVERED TO YOUR COTTAGE."},
      {"Buy Glazed Hearth Tiles (11)", "Painted tiles to brighten your hearth edge.", 11, "hearth_tiles_item", "", "",
       "GLAZED HEARTH TILES ARE ENTERED ON YOUR HOUSEHOLD LEDGER."},
      {"Buy Prayer Stool (5)", "A small stool for the icon corner.", 5, "prayer_stool_item", "", "",
       "A PRAYER STOOL IS SET BY THE WALL."},
      {"Buy Wax Lantern (6)", "A hanging lantern for gentler evening light.", 6, "wax_lantern_item", "", "",
       "A WAX LANTERN IS HUNG FOR YOUR HOUSEHOLD."},
      {"Buy Wool Tapestry (9)", "A woven hanging to warm the room through damp weather.", 9, "wool_tapestry_item", "", "",
       "A WOOL TAPESTRY IS ENTERED IN YOUR HOUSEHOLD FURNISHINGS."},
  };
  return kOffers;
}

const std::vector<ShopOffer>& shop_offers(ShopId id) {
  switch (id) {
    case ShopId::BlackpineCloth:
      return blackpine_shop_offers();
    case ShopId::HospiceSupply:
      return hospice_shop_offers();
    case ShopId::ScriptoriumSupply:
      return scriptorium_shop_offers();
    case ShopId::PropertyCharter:
      return property_shop_offers();
    default: {
      static const std::vector<ShopOffer> kEmpty;
      return kEmpty;
    }
  }
}

const InventoryItemDefinition* inventory_definition(const std::string& id) {
  for (const auto& definition : inventory_definitions()) {
    if (definition.id == id) {
      return &definition;
    }
  }
  return nullptr;
}

const CropDefinition* crop_definition(const std::string& id) {
  for (const auto& definition : crop_definitions()) {
    if (definition.id == id) {
      return &definition;
    }
  }
  return nullptr;
}

const CropDefinition* crop_definition_for_seed(const std::string& item_id) {
  for (const auto& definition : crop_definitions()) {
    if (definition.seed_item_id == item_id) {
      return &definition;
    }
  }
  return nullptr;
}

const OutfitDefinition* outfit_definition(const std::string& id) {
  for (const auto& definition : outfit_definitions()) {
    if (definition.id == id) {
      return &definition;
    }
  }
  return nullptr;
}

const char* shop_title(ShopId id) {
  switch (id) {
    case ShopId::BlackpineCloth:
      return "BLACKPINE MARKET STALLS";
    case ShopId::HospiceSupply:
      return "SAINT HILDA SUPPLY TABLE";
    case ShopId::ScriptoriumSupply:
      return "SCRIPTORIUM CHEST";
    case ShopId::PropertyCharter:
      return "PROPERTY CHARTERS";
    default:
      return "";
  }
}

const char* shop_description(ShopId id) {
  switch (id) {
    case ShopId::BlackpineCloth:
      return "SEED, BREAD, TOOLS, CLOTH, AND THE SMALL COMFORTS BLACKPINE CAN AFFORD.";
    case ShopId::HospiceSupply:
      return "PRACTICAL SUPPLIES FOR NURSING, TRAVEL, AND WINTER CARE.";
    case ShopId::ScriptoriumSupply:
      return "PARCHMENT, INK, AND WAX RELEASED ONLY UNDER THE PRIOR'S RULE.";
    case ShopId::PropertyCharter:
      return "CHARTERS, HOUSE LOTS, AND THE FURNISHINGS THAT TURN A COTTAGE INTO A HOUSEHOLD.";
    default:
      return "";
  }
}

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

int hash_text(const std::string& text, int seed = 0) {
  std::uint32_t value = 2166136261u ^ static_cast<std::uint32_t>(seed);
  for (unsigned char ch : text) {
    value ^= ch;
    value *= 16777619u;
  }
  return static_cast<int>(value & 0x7fffffff);
}

int manhattan_distance(int x0, int y0, int x1, int y1) {
  return std::abs(x0 - x1) + std::abs(y0 - y1);
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
          fill_rect(renderer,
                    SDL_Rect{draw_x + col * scale, y + row * scale, scale, scale},
                    color);
        }
      }
    }
    draw_x += 6 * scale;
  }
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
      for (std::size_t piece_index = 0; piece_index < pieces.size(); ++piece_index) {
        const std::string& piece = pieces[piece_index];
        const std::string candidate = current.empty() ? piece : current + " " + piece;
        if (!current.empty() && text_width(candidate, scale) > max_width) {
          lines.push_back(current);
          current.clear();
        }
        if (piece_index + 1 < pieces.size()) {
          if (!current.empty()) {
            lines.push_back(current);
            current.clear();
          }
          lines.push_back(piece);
        } else {
          current = piece;
        }
      }
      continue;
    }

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

std::vector<std::string> wrap_text_block(const std::string& text, int max_width, int scale, bool wrap) {
  if (!wrap) {
    return split_lines(text);
  }

  std::vector<std::string> lines;
  for (const auto& raw_line : split_lines(text)) {
    if (raw_line.empty()) {
      lines.push_back("");
      continue;
    }
    const auto wrapped = wrap_text(raw_line, max_width, scale);
    lines.insert(lines.end(), wrapped.begin(), wrapped.end());
  }
  if (lines.empty()) {
    lines.push_back("");
  }
  return lines;
}

std::size_t max_lines_for_box(const SDL_Rect& box, int scale, int line_gap, std::size_t max_lines_override = 0) {
  const int glyph_height = text_height(scale);
  const int stride = glyph_height + line_gap;
  std::size_t lines = 1;
  if (box.h > glyph_height) {
    lines = static_cast<std::size_t>(1 + (box.h - glyph_height) / stride);
  }
  if (max_lines_override != 0U) {
    lines = std::min(lines, max_lines_override);
  }
  return std::max<std::size_t>(1, lines);
}

bool lines_fit_width(const std::vector<std::string>& lines, int width, int scale) {
  for (const auto& line : lines) {
    if (text_width(line, scale) > width) {
      return false;
    }
  }
  return true;
}

std::vector<TextPageLayout> paginate_lines(const std::vector<std::string>& lines,
                                           int scale,
                                           std::size_t lines_per_page,
                                           bool paginate) {
  std::vector<TextPageLayout> pages;
  const std::size_t chunk_size = paginate ? lines_per_page : lines.size();
  for (std::size_t index = 0; index < lines.size(); index += chunk_size) {
    TextPageLayout page;
    page.scale = scale;
    const std::size_t end = std::min(lines.size(), index + chunk_size);
    for (std::size_t line_index = index; line_index < end; ++line_index) {
      page.lines.push_back(lines[line_index]);
    }
    pages.push_back(page);
    if (!paginate) {
      break;
    }
  }
  if (pages.empty()) {
    pages.push_back({{""}, scale});
  }
  return pages;
}

std::vector<TextPageLayout> layout_text_box(const std::string& text,
                                            const SDL_Rect& box,
                                            const TextBoxOptions& options) {
  for (int scale = options.preferred_scale; scale >= options.min_scale; --scale) {
    const auto lines = wrap_text_block(text, box.w, scale, options.wrap);
    if (!lines_fit_width(lines, box.w, scale)) {
      continue;
    }
    const std::size_t line_limit = max_lines_for_box(box, scale, options.line_gap, options.max_lines);
    if (options.paginate || lines.size() <= line_limit) {
      return paginate_lines(lines, scale, line_limit, options.paginate);
    }
  }

  const int fallback_scale = options.min_scale;
  std::vector<std::string> fallback_lines = wrap_text_block(text, box.w, fallback_scale, options.wrap);
  const std::size_t line_limit = max_lines_for_box(box, fallback_scale, options.line_gap, options.max_lines);
  if (!options.paginate && fallback_lines.size() > line_limit) {
    fallback_lines.resize(line_limit);
  }
  return paginate_lines(fallback_lines, fallback_scale, line_limit, options.paginate);
}

TextPageLayout layout_text_box_single(const std::string& text, const SDL_Rect& box, const TextBoxOptions& options) {
  const auto pages = layout_text_box(text, box, options);
  return pages.empty() ? TextPageLayout{{""}, options.min_scale} : pages.front();
}

void draw_text_layout(SDL_Renderer* renderer,
                      const TextPageLayout& page,
                      const SDL_Rect& box,
                      const TextBoxOptions& options,
                      SDL_Color color) {
  const int total_height = static_cast<int>(page.lines.size()) * text_height(page.scale) +
                           std::max(0, static_cast<int>(page.lines.size()) - 1) * options.line_gap;
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

SpritePalette player_palette(const GameState& state) {
  const OutfitDefinition* outfit = outfit_definition(state.profile.equipped_outfit_id);
  const SpritePalette fallback = palette_for(SpriteRole::Player);
  return {
      fallback.outline,
      current_complexion(state).color,
      current_hair(state).color,
      outfit != nullptr ? outfit->primary : fallback.primary,
      outfit != nullptr ? outfit->secondary : fallback.secondary,
      outfit != nullptr ? outfit->accent : fallback.accent,
      outfit != nullptr ? outfit->boots : fallback.boots,
  };
}

void render_character_with_palette(SDL_Renderer* renderer,
                                   const SpritePalette& palette,
                                   SpriteRole role,
                                   Direction facing,
                                   bool walking,
                                   int x,
                                   int y,
                                   Uint32 now,
                                   bool blink = false) {
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
      if (blink) {
        fill_rect(renderer, SDL_Rect{x + 5, y + 6 + offset_y, 2, 1}, palette.outline);
        fill_rect(renderer, SDL_Rect{x + 8, y + 6 + offset_y, 2, 1}, palette.outline);
      } else {
        draw_pixel(renderer, x + 6, y + 5 + offset_y, palette.outline);
        draw_pixel(renderer, x + 9, y + 5 + offset_y, palette.outline);
      }
    } else if (facing == Direction::Left) {
      if (blink) {
        fill_rect(renderer, SDL_Rect{x + 4, y + 6 + offset_y, 2, 1}, palette.outline);
      } else {
        draw_pixel(renderer, x + 5, y + 5 + offset_y, palette.outline);
      }
    } else if (facing == Direction::Right) {
      if (blink) {
        fill_rect(renderer, SDL_Rect{x + 10, y + 6 + offset_y, 2, 1}, palette.outline);
      } else {
        draw_pixel(renderer, x + 10, y + 5 + offset_y, palette.outline);
      }
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

void render_character(SDL_Renderer* renderer,
                      SpriteRole role,
                      Direction facing,
                      bool walking,
                      int x,
                      int y,
                      Uint32 now,
                      bool blink = false) {
  render_character_with_palette(renderer, palette_for(role), role, facing, walking, x, y, now, blink);
}

void render_monster(SDL_Renderer* renderer, const Monster& monster, int x, int y, Uint32 now) {
  const bool hurt = now < monster.hurt_until;
  const SDL_Color outline = hurt ? SDL_Color{248, 236, 220, 255} : SDL_Color{35, 38, 42, 255};

  if (monster.kind == MonsterKind::Ratling) {
    const SDL_Color fur = hurt ? SDL_Color{214, 144, 130, 255} : SDL_Color{116, 92, 82, 255};
    const SDL_Color belly = hurt ? SDL_Color{245, 201, 184, 255} : SDL_Color{189, 170, 152, 255};
    const SDL_Color eye = SDL_Color{225, 92, 72, 255};
    fill_rect(renderer, SDL_Rect{x + 3, y + 10, 10, 3}, SDL_Color{0, 0, 0, 70});
    fill_rect(renderer, SDL_Rect{x + 3, y + 5, 8, 6}, outline);
    fill_rect(renderer, SDL_Rect{x + 4, y + 6, 6, 4}, fur);
    fill_rect(renderer, SDL_Rect{x + 7, y + 4, 5, 5}, outline);
    fill_rect(renderer, SDL_Rect{x + 8, y + 5, 3, 3}, fur);
    fill_rect(renderer, SDL_Rect{x + 9, y + 7, 2, 1}, belly);
    draw_pixel(renderer, x + 10, y + 6, eye);
    draw_pixel(renderer, x + 7, y + 5, outline);
    draw_pixel(renderer, x + 11, y + 5, outline);
    fill_rect(renderer, SDL_Rect{x + 5, y + 11, 2, 2}, fur);
    fill_rect(renderer, SDL_Rect{x + 9, y + 11, 2, 2}, fur);
    draw_pixel(renderer, x + 2, y + 9, SDL_Color{186, 152, 138, 255}, 1);
    draw_pixel(renderer, x + 1, y + 10, SDL_Color{186, 152, 138, 255}, 1);
    return;
  }

  if (monster.kind == MonsterKind::BogWisp) {
    const int bob = static_cast<int>((now / 220U) % 2U);
    const SDL_Color core = hurt ? SDL_Color{255, 212, 205, 255} : SDL_Color{178, 233, 204, 255};
    const SDL_Color aura = hurt ? SDL_Color{244, 168, 160, 160} : SDL_Color{104, 191, 156, 130};
    fill_circle(renderer, x + 8, y + 9 + bob, 5, aura);
    fill_rect(renderer, SDL_Rect{x + 5, y + 5 + bob, 6, 6}, outline);
    fill_rect(renderer, SDL_Rect{x + 6, y + 6 + bob, 4, 4}, core);
    draw_pixel(renderer, x + 7, y + 4 + bob, outline);
    draw_pixel(renderer, x + 5, y + 8 + bob, outline);
    draw_pixel(renderer, x + 10, y + 8 + bob, outline);
    draw_pixel(renderer, x + 7, y + 11 + bob, outline);
    return;
  }

  const SDL_Color fur = hurt ? SDL_Color{222, 154, 132, 255} : SDL_Color{148, 102, 68, 255};
  const SDL_Color mane = hurt ? SDL_Color{244, 207, 187, 255} : SDL_Color{203, 176, 130, 255};
  const SDL_Color eye = SDL_Color{242, 226, 195, 255};
  fill_rect(renderer, SDL_Rect{x + 3, y + 11, 10, 2}, SDL_Color{0, 0, 0, 70});
  fill_rect(renderer, SDL_Rect{x + 4, y + 6, 8, 6}, outline);
  fill_rect(renderer, SDL_Rect{x + 5, y + 7, 6, 4}, fur);
  fill_rect(renderer, SDL_Rect{x + 10, y + 5, 4, 5}, outline);
  fill_rect(renderer, SDL_Rect{x + 11, y + 6, 2, 3}, fur);
  fill_rect(renderer, SDL_Rect{x + 6, y + 5, 3, 2}, mane);
  draw_pixel(renderer, x + 12, y + 7, eye);
  fill_rect(renderer, SDL_Rect{x + 5, y + 11, 2, 2}, fur);
  fill_rect(renderer, SDL_Rect{x + 9, y + 11, 2, 2}, fur);
  draw_pixel(renderer, x + 3, y + 9, outline);
  draw_pixel(renderer, x + 2, y + 8, outline);
}

void render_monster_hp_bar(SDL_Renderer* renderer, const Monster& monster, int screen_x, int screen_y) {
  if (monster.hp <= 0 || monster.max_hp <= 0) {
    return;
  }
  const SDL_Rect back = {screen_x - 1, screen_y - 7, 18, 4};
  fill_rect(renderer, back, SDL_Color{34, 34, 40, 220});
  draw_rect(renderer, back, SDL_Color{242, 236, 220, 255});
  const int fill_width = std::max(1, (16 * monster.hp) / std::max(1, monster.max_hp));
  fill_rect(renderer, SDL_Rect{screen_x, screen_y - 6, fill_width, 2},
            monster.hp * 3 > monster.max_hp ? SDL_Color{100, 187, 118, 255}
                                            : SDL_Color{196, 113, 84, 255});
}

void render_player_hp_panel(SDL_Renderer* renderer, const GameState& state) {
  const SDL_Rect panel = {6, 6, 74, 18};
  fill_rect(renderer, panel, SDL_Color{248, 242, 226, 230});
  draw_rect(renderer, panel, SDL_Color{95, 86, 67, 255});
  draw_text(renderer, "HP", panel.x + 6, panel.y + 5, 1, SDL_Color{72, 84, 58, 255});
  fill_rect(renderer, SDL_Rect{panel.x + 22, panel.y + 6, 46, 6}, SDL_Color{60, 62, 68, 255});
  const int fill_width = std::clamp((44 * state.player.hp) / std::max(1, state.player.max_hp), 0, 44);
  fill_rect(renderer, SDL_Rect{panel.x + 23, panel.y + 7, fill_width, 4},
            state.player.hp * 2 > state.player.max_hp ? SDL_Color{100, 187, 118, 255}
                                                      : SDL_Color{196, 113, 84, 255});
}

void render_player_swing(SDL_Renderer* renderer, const GameState& state, int screen_x, int screen_y, Uint32 now) {
  if (now >= state.player.swing_until) {
    return;
  }
  const Uint32 elapsed = kSwordSwingMs - (state.player.swing_until - now);
  const int phase = elapsed > kSwordSwingMs / 2 ? 1 : 0;
  SDL_Color blade = phase == 0 ? SDL_Color{240, 240, 246, 255} : SDL_Color{213, 219, 232, 255};
  SDL_Color accent = SDL_Color{233, 199, 126, 255};

  switch (state.player.facing) {
    case Direction::Up:
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y - 6 + phase, 2, 8}, blade);
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 1, 4, 1}, accent);
      break;
    case Direction::Down:
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 11 - phase, 2, 8}, blade);
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 11, 4, 1}, accent);
      break;
    case Direction::Left:
      fill_rect(renderer, SDL_Rect{screen_x - 6 + phase, screen_y + 7, 8, 2}, blade);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 6, 1, 4}, accent);
      break;
    case Direction::Right:
      fill_rect(renderer, SDL_Rect{screen_x + 11 - phase, screen_y + 7, 8, 2}, blade);
      fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + 6, 1, 4}, accent);
      break;
    default:
      break;
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
  const int roof_height = std::max(2, height - 2);
  fill_tiles(rows, x, y, width, roof_height, kRoof);
  fill_tiles(rows, x, y + roof_height, width, height - roof_height, kWall);
  set_tile(rows, x + width / 2, y + height - 1, kDoor);
  if (width >= 6) {
    set_tile(rows, x + 1, y + height - 1, kWindow);
    set_tile(rows, x + width - 2, y + height - 1, kWindow);
  }
}

bool tile_in_bounds(const Area& area, int x, int y);
char tile_at(const Area& area, int x, int y);

bool is_threshold_tile(char tile) {
  switch (tile) {
    case kPath:
    case kStone:
    case kWood:
    case kFloor:
    case kCartStep:
      return true;
    default:
      return false;
  }
}

bool is_facade_support_tile(char tile) {
  switch (tile) {
    case kRoof:
    case kWall:
    case kDoor:
    case kWindow:
    case kGlass:
      return true;
    default:
      return false;
  }
}

void finalize_outdoor_art(Area* area) {
  if (area == nullptr || area->indoor) {
    return;
  }
  for (int y = 0; y < static_cast<int>(area->tiles.size()); ++y) {
    for (int x = 0; x < static_cast<int>(area->tiles[y].size()); ++x) {
      if (tile_at(*area, x, y) != kDoor || !tile_in_bounds(*area, x, y + 1)) {
        continue;
      }
      const char below = tile_at(*area, x, y + 1);
      if (below == kDoor || is_threshold_tile(below)) {
        continue;
      }
      if (below == kGrass || below == kFlower || below == kGarden || below == kHerb) {
        set_tile(&area->tiles, x, y + 1, kStone);
      }
    }
  }
}

Area make_house() {
  Area area;
  area.id = "house";
  area.name = "YOUR FATHER'S HOUSE";
  area.indoor = true;
  area.tiles = blank_rows(16, 12, kFloor);

  fill_tiles(&area.tiles, 0, 0, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 11, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 0, 1, 12, kWall);
  fill_tiles(&area.tiles, 15, 0, 1, 12, kWall);
  set_tile(&area.tiles, 7, 11, kDoor);
  set_tile(&area.tiles, 8, 11, kDoor);

  fill_tiles(&area.tiles, 2, 2, 4, 1, kDesk);
  fill_tiles(&area.tiles, 10, 2, 3, 1, kShelf);
  fill_tiles(&area.tiles, 4, 6, 8, 3, kRug);
  set_tile(&area.tiles, 4, 2, kCandle);
  set_tile(&area.tiles, 11, 2, kCandle);
  set_tile(&area.tiles, 6, 2, kBoard);

  area.warps.push_back({7, 11, "lane", 4, 8, Direction::Down});
  area.warps.push_back({8, 11, "lane", 5, 8, Direction::Down});

  area.npcs.push_back({"father", "YOUR FATHER", SpriteRole::Elder, 5, 4, Direction::Down, true});
  area.npcs.push_back({"mother", "YOUR MOTHER", SpriteRole::Elder, 10, 5, Direction::Left, true});
  return area;
}

Area make_lane() {
  Area area;
  area.id = "lane";
  area.name = "BLACKPINE LANE";
  area.tiles = blank_rows(24, 14, kGrass);
  draw_tree_border(&area.tiles, -1, -1, -1, -1);

  fill_tiles(&area.tiles, 4, 7, 18, 3, kPath);
  fill_tiles(&area.tiles, 2, 5, 5, 5, kStone);
  stamp_house(&area.tiles, 1, 2, 7, 5);
  set_tile(&area.tiles, 0, 8, kPath);
  set_tile(&area.tiles, 13, 6, kCrate);
  set_tile(&area.tiles, 17, 10, kFlower);
  set_tile(&area.tiles, 16, 7, kCartRail);
  set_tile(&area.tiles, 17, 7, kCartRail);
  set_tile(&area.tiles, 18, 7, kHarness);
  set_tile(&area.tiles, 19, 7, kHarness);
  set_tile(&area.tiles, 16, 8, kCartWheel);
  set_tile(&area.tiles, 17, 8, kCartStep);
  set_tile(&area.tiles, 18, 8, kCartWheel);
  set_tile(&area.tiles, 19, 8, kMuleBack);
  set_tile(&area.tiles, 20, 8, kMuleFore);

  area.warps.push_back({4, 7, "house", 7, 10, Direction::Down});
  area.warps.push_back({5, 7, "house", 8, 10, Direction::Down});
  area.warps.push_back({17, 8, "square", 1, 10, Direction::Right});
  area.npcs.push_back({"driver", "CART DRIVER", SpriteRole::Merchant, 15, 8, Direction::Right, true});
  return area;
}

Area make_square() {
  Area area;
  area.id = "square";
  area.name = "BLACKPINE MARKET SQUARE";
  area.tiles = blank_rows(32, 20, kGrass);
  draw_tree_border(&area.tiles, 13, 18, -1, -1);

  fill_tiles(&area.tiles, 0, 9, 32, 4, kPath);
  fill_tiles(&area.tiles, 13, 0, 6, 10, kPath);
  fill_tiles(&area.tiles, 22, 7, 5, 6, kStone);

  stamp_house(&area.tiles, 2, 4, 7, 5);
  fill_tiles(&area.tiles, 10, 13, 4, 3, kStall);
  fill_tiles(&area.tiles, 23, 13, 4, 3, kStall);
  fill_tiles(&area.tiles, 18, 13, 4, 3, kStall);
  set_tile(&area.tiles, 16, 11, kFountain);
  set_tile(&area.tiles, 0, 10, kPath);
  set_tile(&area.tiles, 0, 11, kPath);
  fill_tiles(&area.tiles, 11, 3, 4, 1, kLaundry);
  set_tile(&area.tiles, 22, 8, kBench);
  set_tile(&area.tiles, 25, 8, kBench);
  set_tile(&area.tiles, 10, 12, kBarrel);
  set_tile(&area.tiles, 13, 12, kBarrel);
  set_tile(&area.tiles, 18, 12, kBarrel);
  set_tile(&area.tiles, 26, 12, kBarrel);
  set_tile(&area.tiles, 3, 2, kFlower);
  set_tile(&area.tiles, 7, 2, kFlower);
  set_tile(&area.tiles, 2, 10, kCrate);

  area.warps.push_back({0, 10, "lane", 17, 9, Direction::Left});
  area.warps.push_back({0, 11, "lane", 18, 9, Direction::Left});
  area.warps.push_back({13, 0, "priory-road", 11, 16, Direction::Up});
  area.warps.push_back({14, 0, "priory-road", 12, 16, Direction::Up});
  area.warps.push_back({15, 0, "priory-road", 13, 16, Direction::Up});
  area.warps.push_back({16, 0, "priory-road", 14, 16, Direction::Up});
  area.warps.push_back({17, 0, "priory-road", 15, 16, Direction::Up});
  area.warps.push_back({18, 0, "priory-road", 16, 16, Direction::Up});

  area.npcs.push_back({"friar", "FRANCISCAN FRIAR", SpriteRole::Monk, 8, 11, Direction::Right, true});
  area.npcs.push_back({"margery", "MARGERY BAKER", SpriteRole::Merchant, 12, 16, Direction::Up, true});
  area.npcs.push_back({"godric", "GODRIC COBBLER", SpriteRole::Elder, 24, 7, Direction::Left, true});
  return area;
}

Area make_priory_road() {
  Area area;
  area.id = "priory-road";
  area.name = "PRIORY ROAD";
  area.tiles = blank_rows(28, 18, kGrass);
  draw_tree_border(&area.tiles, 11, 16, -1, -1);

  fill_tiles(&area.tiles, 11, 0, 6, 18, kPath);
  fill_tiles(&area.tiles, 10, 5, 8, 3, kStone);
  fill_tiles(&area.tiles, 3, 4, 3, 10, kTree);
  fill_tiles(&area.tiles, 22, 4, 3, 10, kTree);
  set_tile(&area.tiles, 13, 6, kCrate);
  set_tile(&area.tiles, 14, 10, kFlower);

  area.warps.push_back({11, 17, "square", 13, 1, Direction::Down});
  area.warps.push_back({12, 17, "square", 14, 1, Direction::Down});
  area.warps.push_back({13, 17, "square", 15, 1, Direction::Down});
  area.warps.push_back({14, 17, "square", 16, 1, Direction::Down});
  area.warps.push_back({15, 17, "square", 17, 1, Direction::Down});
  area.warps.push_back({16, 17, "square", 18, 1, Direction::Down});
  area.warps.push_back({11, 0, "priory-gate", 7, 12, Direction::Up});
  area.warps.push_back({12, 0, "priory-gate", 8, 12, Direction::Up});
  area.warps.push_back({13, 0, "priory-gate", 9, 12, Direction::Up});
  area.warps.push_back({14, 0, "priory-gate", 10, 12, Direction::Up});
  area.warps.push_back({15, 0, "priory-gate", 11, 12, Direction::Up});
  area.warps.push_back({16, 0, "priory-gate", 12, 12, Direction::Up});
  area.monsters.push_back({"road-ratling", "Ratling", MonsterKind::Ratling, 8, 8, Direction::Right, 6, 6, 1, true});
  return area;
}

Area make_priory_gate() {
  Area area;
  area.id = "priory-gate";
  area.name = "SAINT CATHERINE GATE";
  area.tiles = blank_rows(20, 14, kGrass);
  draw_tree_border(&area.tiles, 7, 12, -1, -1);

  fill_tiles(&area.tiles, 7, 0, 6, 5, kRoof);
  fill_tiles(&area.tiles, 8, 5, 4, 1, kWall);
  fill_tiles(&area.tiles, 8, 6, 4, 2, kDoor);
  fill_tiles(&area.tiles, 7, 8, 6, 6, kStone);
  fill_tiles(&area.tiles, 8, 8, 4, 6, kPath);
  set_tile(&area.tiles, 8, 4, kGlass);
  set_tile(&area.tiles, 11, 4, kGlass);

  area.warps.push_back({8, 13, "priory-road", 12, 1, Direction::Down});
  area.warps.push_back({9, 13, "priory-road", 13, 1, Direction::Down});
  area.warps.push_back({10, 13, "priory-road", 14, 1, Direction::Down});
  area.warps.push_back({11, 13, "priory-road", 15, 1, Direction::Down});
  area.warps.push_back({8, 6, "priory-court", 14, 18, Direction::Up});
  area.warps.push_back({9, 6, "priory-court", 15, 18, Direction::Up});
  area.warps.push_back({10, 6, "priory-court", 16, 18, Direction::Up});
  area.warps.push_back({11, 6, "priory-court", 17, 18, Direction::Up});
  area.warps.push_back({8, 7, "priory-court", 14, 18, Direction::Up});
  area.warps.push_back({9, 7, "priory-court", 15, 18, Direction::Up});
  area.warps.push_back({10, 7, "priory-court", 16, 18, Direction::Up});
  area.warps.push_back({11, 7, "priory-court", 17, 18, Direction::Up});

  area.npcs.push_back({"gate-friar", "GATE FRIAR", SpriteRole::Prior, 12, 9, Direction::Left, true});
  return area;
}

Area make_priory_court() {
  Area area;
  area.id = "priory-court";
  area.name = "SAINT CATHERINE COURTYARD";
  area.tiles = blank_rows(28, 20, kGrass);
  draw_tree_border(&area.tiles, 12, 15, -1, -1);

  fill_tiles(&area.tiles, 10, 6, 9, 10, kStone);
  fill_tiles(&area.tiles, 7, 8, 15, 6, kStone);
  fill_tiles(&area.tiles, 9, 9, 11, 4, kPath);
  fill_tiles(&area.tiles, 0, 11, 8, 3, kPath);
  fill_tiles(&area.tiles, 12, 15, 5, 5, kPath);
  fill_tiles(&area.tiles, 11, 18, 7, 1, kPath);

  fill_tiles(&area.tiles, 10, 1, 10, 6, kRoof);
  fill_tiles(&area.tiles, 11, 6, 8, 1, kWall);
  set_tile(&area.tiles, 14, 6, kDoor);
  set_tile(&area.tiles, 12, 6, kGlass);
  set_tile(&area.tiles, 16, 6, kGlass);

  stamp_house(&area.tiles, 21, 3, 5, 5);
  stamp_house(&area.tiles, 2, 3, 6, 5);

  fill_tiles(&area.tiles, 4, 10, 3, 3, kGarden);
  fill_tiles(&area.tiles, 21, 10, 3, 3, kGarden);
  fill_tiles(&area.tiles, 5, 10, 1, 3, kHerb);
  fill_tiles(&area.tiles, 22, 10, 1, 3, kHerb);
  set_tile(&area.tiles, 14, 10, kFountain);
  set_tile(&area.tiles, 7, 12, kBoard);
  set_tile(&area.tiles, 4, 9, kCrate);
  set_tile(&area.tiles, 23, 9, kCrate);
  set_tile(&area.tiles, 10, 14, kBench);
  set_tile(&area.tiles, 18, 14, kBench);
  fill_tiles(&area.tiles, 2, 2, 4, 1, kLaundry);
  fill_tiles(&area.tiles, 21, 2, 4, 1, kLaundry);
  set_tile(&area.tiles, 0, 12, kPath);
  set_tile(&area.tiles, 0, 13, kPath);

  area.warps.push_back({14, 6, "chapel", 7, 11, Direction::Up});
  area.warps.push_back({23, 7, "scriptorium", 6, 10, Direction::Up});
  area.warps.push_back({12, 19, "bellfield", 13, 1, Direction::Down});
  area.warps.push_back({13, 19, "bellfield", 14, 1, Direction::Down});
  area.warps.push_back({14, 19, "bellfield", 15, 1, Direction::Down});
  area.warps.push_back({15, 19, "bellfield", 16, 1, Direction::Down});
  area.warps.push_back({16, 19, "bellfield", 17, 1, Direction::Down});
  area.warps.push_back({0, 12, "saint-hilda-hospice", 7, 12, Direction::Up});
  area.warps.push_back({0, 13, "saint-hilda-hospice", 8, 12, Direction::Up});

  area.npcs.push_back({"steward-helena", "STEWARD HELENA", SpriteRole::Sister, 18, 15, Direction::Left, true});
  area.npcs.push_back({"porter", "BROTHER PORTER", SpriteRole::Monk, 5, 14, Direction::Right, true});
  area.npcs.push_back({"novice-paul", "NOVICE PAUL", SpriteRole::Child, 9, 15, Direction::Up, true});
  return area;
}

Area make_bellfield() {
  Area area;
  area.id = "bellfield";
  area.name = "BLACKPINE VILLAGE GREEN";
  area.tiles = blank_rows(32, 20, kGrass);
  draw_tree_border(&area.tiles, 14, 17, -1, -1);

  fill_tiles(&area.tiles, 13, 0, 5, 7, kPath);
  fill_tiles(&area.tiles, 10, 6, 12, 6, kStone);
  fill_tiles(&area.tiles, 0, 8, 5, 3, kPath);
  fill_tiles(&area.tiles, 20, 8, 12, 3, kPath);
  fill_tiles(&area.tiles, 14, 11, 4, 9, kPath);
  fill_tiles(&area.tiles, 7, 14, 18, 3, kPath);
  set_tile(&area.tiles, 16, 8, kFountain);

  stamp_house(&area.tiles, 2, 3, 6, 5);
  stamp_house(&area.tiles, 2, 11, 6, 5);
  stamp_house(&area.tiles, 24, 3, 6, 5);
  stamp_house(&area.tiles, 24, 11, 6, 5);

  fill_tiles(&area.tiles, 10, 12, 4, 3, kStall);
  fill_tiles(&area.tiles, 19, 12, 4, 3, kStall);
  fill_tiles(&area.tiles, 1, 2, 8, 1, kFence);
  fill_tiles(&area.tiles, 23, 2, 8, 1, kFence);
  fill_tiles(&area.tiles, 1, 18, 8, 1, kFence);
  fill_tiles(&area.tiles, 23, 18, 8, 1, kFence);
  fill_tiles(&area.tiles, 8, 4, 4, 1, kLaundry);
  fill_tiles(&area.tiles, 20, 4, 4, 1, kLaundry);
  set_tile(&area.tiles, 10, 11, kBench);
  set_tile(&area.tiles, 21, 11, kBench);
  set_tile(&area.tiles, 9, 14, kBarrel);
  set_tile(&area.tiles, 23, 14, kBarrel);
  fill_tiles(&area.tiles, 1, 8, 2, 3, kPath);
  set_tile(&area.tiles, 2, 1, kFlower);
  set_tile(&area.tiles, 5, 1, kFlower);
  set_tile(&area.tiles, 25, 1, kFlower);
  set_tile(&area.tiles, 28, 1, kFlower);
  set_tile(&area.tiles, 22, 2, kBench);
  set_tile(&area.tiles, 23, 7, kBench);
  fill_tiles(&area.tiles, 24, 8, 3, 2, kHerb);
  set_tile(&area.tiles, 28, 8, kCandle);
  set_tile(&area.tiles, 9, 17, kFlower);
  set_tile(&area.tiles, 22, 17, kFlower);
  set_tile(&area.tiles, 12, 17, kFlower);
  set_tile(&area.tiles, 19, 17, kFlower);
  set_tile(&area.tiles, 4, 17, kFlower);
  set_tile(&area.tiles, 27, 17, kFlower);

  area.warps.push_back({13, 0, "priory-court", 12, 18, Direction::Up});
  area.warps.push_back({14, 0, "priory-court", 13, 18, Direction::Up});
  area.warps.push_back({15, 0, "priory-court", 14, 18, Direction::Up});
  area.warps.push_back({16, 0, "priory-court", 15, 18, Direction::Up});
  area.warps.push_back({17, 0, "priory-court", 16, 18, Direction::Up});
  area.warps.push_back({31, 8, "candlewharf", 1, 8, Direction::Right});
  area.warps.push_back({31, 9, "candlewharf", 1, 9, Direction::Right});
  area.warps.push_back({31, 10, "candlewharf", 1, 10, Direction::Right});
  area.warps.push_back({0, 8, "saint-hilda-hospice", 1, 7, Direction::Right});
  area.warps.push_back({0, 9, "saint-hilda-hospice", 1, 8, Direction::Right});
  area.warps.push_back({0, 10, "saint-hilda-hospice", 1, 9, Direction::Right});
  area.warps.push_back({27, 7, "blackpine-hospital", 8, 12, Direction::Up});

  area.npcs.push_back({"alisoun", "ALISOUN WEAVER", SpriteRole::Merchant, 6, 9, Direction::Down, true});
  area.npcs.push_back({"oswin", "WATCHMAN OSWIN", SpriteRole::Watchman, 25, 9, Direction::Left, true});
  area.npcs.push_back({"piers", "PIERS", SpriteRole::Child, 15, 10, Direction::Up, true});
  area.npcs.push_back({"widow-joan", "WIDOW JOAN", SpriteRole::Elder, 11, 17, Direction::Up, true});
  area.npcs.push_back({"agnes", "AGNES MILLER", SpriteRole::Merchant, 8, 17, Direction::Up, true});
  area.npcs.push_back({"colm", "COLM DROVER", SpriteRole::Merchant, 24, 17, Direction::Left, true});
  area.npcs.push_back({"ysabel", "YSABEL NOTARY", SpriteRole::Merchant, 20, 16, Direction::Left, true});
  area.monsters.push_back({"green-wisp", "Bog Wisp", MonsterKind::BogWisp, 28, 17, Direction::Left, 7, 7, 2, true});
  return area;
}

Area make_candlewharf() {
  Area area;
  area.id = "candlewharf";
  area.name = "RAVENSCAR QUAY";
  area.tiles = blank_rows(28, 18, kGrass);
  draw_tree_border(&area.tiles, 0, 0, -1, -1);

  fill_tiles(&area.tiles, 0, 8, 18, 3, kPath);
  fill_tiles(&area.tiles, 11, 5, 7, 8, kStone);
  fill_tiles(&area.tiles, 18, 0, 10, 18, kWater);
  fill_tiles(&area.tiles, 16, 6, 9, 4, kWood);
  fill_tiles(&area.tiles, 21, 3, 3, 10, kWood);
  stamp_house(&area.tiles, 4, 3, 6, 5);
  stamp_house(&area.tiles, 4, 11, 6, 5);
  fill_tiles(&area.tiles, 10, 2, 4, 1, kLaundry);
  set_tile(&area.tiles, 24, 9, kWood);
  set_tile(&area.tiles, 24, 10, kWood);
  set_tile(&area.tiles, 11, 12, kBarrel);
  set_tile(&area.tiles, 15, 5, kBarrel);
  set_tile(&area.tiles, 13, 13, kCrate);
  set_tile(&area.tiles, 15, 13, kCrate);
  set_tile(&area.tiles, 17, 5, kCrate);
  set_tile(&area.tiles, 17, 12, kReed);
  set_tile(&area.tiles, 17, 2, kReed);
  set_tile(&area.tiles, 17, 15, kReed);

  area.warps.push_back({0, 8, "bellfield", 30, 8, Direction::Left});
  area.warps.push_back({0, 9, "bellfield", 30, 9, Direction::Left});
  area.warps.push_back({0, 10, "bellfield", 30, 10, Direction::Left});

  area.npcs.push_back({"tomas", "TOMAS FERRYMAN", SpriteRole::Fisher, 22, 9, Direction::Left, true});
  area.npcs.push_back({"dock-clerk", "DOCK CLERK ELSWYTH", SpriteRole::Elder, 12, 13, Direction::Up, true});
  area.npcs.push_back({"hanse-factor", "HANSE FACTOR", SpriteRole::Merchant, 8, 9, Direction::Right, true});
  area.npcs.push_back({"rowan", "ROWAN ROPER", SpriteRole::Fisher, 6, 16, Direction::Right, true});
  area.monsters.push_back({"dock-hound", "Dock Hound", MonsterKind::DockHound, 14, 14, Direction::Left, 9, 9, 2, true});
  return area;
}

Area make_scriptorium() {
  Area area;
  area.id = "scriptorium";
  area.name = "SAINT CATHERINE SCRIPTORIUM";
  area.indoor = true;
  area.tiles = blank_rows(14, 12, kFloor);

  fill_tiles(&area.tiles, 0, 0, 14, 1, kWall);
  fill_tiles(&area.tiles, 0, 11, 14, 1, kWall);
  fill_tiles(&area.tiles, 0, 0, 1, 12, kWall);
  fill_tiles(&area.tiles, 13, 0, 1, 12, kWall);
  set_tile(&area.tiles, 6, 11, kDoor);

  fill_tiles(&area.tiles, 2, 2, 10, 1, kShelf);
  fill_tiles(&area.tiles, 4, 7, 6, 2, kRug);
  fill_tiles(&area.tiles, 2, 4, 3, 1, kDesk);
  fill_tiles(&area.tiles, 8, 4, 3, 1, kDesk);
  fill_tiles(&area.tiles, 10, 6, 2, 3, kShelf);
  set_tile(&area.tiles, 5, 4, kCandle);
  set_tile(&area.tiles, 8, 5, kCandle);
  set_tile(&area.tiles, 11, 2, kCandle);
  set_tile(&area.tiles, 2, 2, kCandle);
  set_tile(&area.tiles, 10, 2, kBoard);

  area.warps.push_back({6, 11, "priory-court", 23, 8, Direction::Down});
  area.npcs.push_back({"brother-martin", "BROTHER MARTIN", SpriteRole::Monk, 6, 5, Direction::Down, true});
  return area;
}

Area make_chapel() {
  Area area;
  area.id = "chapel";
  area.name = "CHAPEL OF SAINT CATHERINE";
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
  fill_tiles(&area.tiles, 7, 3, 2, 9, kRug);
  fill_tiles(&area.tiles, 4, 6, 3, 1, kPew);
  fill_tiles(&area.tiles, 9, 6, 3, 1, kPew);
  fill_tiles(&area.tiles, 4, 8, 3, 1, kPew);
  fill_tiles(&area.tiles, 9, 8, 3, 1, kPew);

  area.warps.push_back({7, 13, "priory-court", 14, 7, Direction::Down});
  area.npcs.push_back({"prior", "THE PRIOR", SpriteRole::Prior, 7, 4, Direction::Down, true});
  return area;
}

Area make_hospice() {
  Area area;
  area.id = "saint-hilda-hospice";
  area.name = "SAINT HILDA HOSPICE";
  area.indoor = true;
  area.tiles = blank_rows(16, 14, kFloor);

  fill_tiles(&area.tiles, 0, 0, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 13, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 0, 1, 14, kWall);
  fill_tiles(&area.tiles, 15, 0, 1, 14, kWall);
  set_tile(&area.tiles, 0, 7, kDoor);
  set_tile(&area.tiles, 0, 8, kDoor);
  set_tile(&area.tiles, 7, 13, kDoor);
  set_tile(&area.tiles, 8, 13, kDoor);

  fill_tiles(&area.tiles, 3, 2, 3, 1, kDesk);
  fill_tiles(&area.tiles, 9, 2, 4, 1, kShelf);
  fill_tiles(&area.tiles, 3, 5, 3, 1, kPew);
  fill_tiles(&area.tiles, 9, 5, 3, 1, kPew);
  fill_tiles(&area.tiles, 3, 8, 3, 1, kPew);
  fill_tiles(&area.tiles, 9, 8, 3, 1, kPew);
  fill_tiles(&area.tiles, 6, 10, 4, 2, kRug);
  fill_tiles(&area.tiles, 12, 10, 2, 2, kHerb);
  set_tile(&area.tiles, 4, 2, kCandle);
  set_tile(&area.tiles, 10, 2, kCandle);
  set_tile(&area.tiles, 13, 10, kFlower);

  area.warps.push_back({0, 7, "bellfield", 1, 9, Direction::Right});
  area.warps.push_back({0, 8, "bellfield", 1, 10, Direction::Right});
  area.warps.push_back({7, 12, "priory-court", 1, 12, Direction::Right});
  area.warps.push_back({8, 12, "priory-court", 1, 13, Direction::Right});
  area.warps.push_back({7, 13, "priory-court", 1, 12, Direction::Right});
  area.warps.push_back({8, 13, "priory-court", 1, 13, Direction::Right});

  area.npcs.push_back({"hospice-prioress", "PRIORESS HILDA", SpriteRole::Sister, 5, 4, Direction::Down, true});
  area.npcs.push_back({"sister-beatrice", "SISTER BEATRICE", SpriteRole::Sister, 11, 9, Direction::Left, true});
  return area;
}

Area make_blackpine_cottage() {
  Area area;
  area.id = "blackpine-cottage";
  area.name = "BLACKPINE COTTAGE";
  area.indoor = true;
  area.tiles = blank_rows(16, 12, kFloor);

  fill_tiles(&area.tiles, 0, 0, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 11, 16, 1, kWall);
  fill_tiles(&area.tiles, 0, 0, 1, 12, kWall);
  fill_tiles(&area.tiles, 15, 0, 1, 12, kWall);
  set_tile(&area.tiles, 7, 0, kDoor);
  set_tile(&area.tiles, 8, 0, kDoor);
  set_tile(&area.tiles, 7, 11, kDoor);
  set_tile(&area.tiles, 8, 11, kDoor);
  fill_tiles(&area.tiles, 3, 3, 5, 2, kRug);
  fill_tiles(&area.tiles, 10, 2, 3, 1, kDesk);
  fill_tiles(&area.tiles, 10, 4, 3, 2, kShelf);
  set_tile(&area.tiles, 3, 7, kBed);
  set_tile(&area.tiles, 4, 7, kBed);
  set_tile(&area.tiles, 11, 7, kChest);
  set_tile(&area.tiles, 12, 7, kChest);
  set_tile(&area.tiles, 4, 2, kCandle);
  set_tile(&area.tiles, 5, 2, kBoard);
  set_tile(&area.tiles, 10, 2, kCandle);
  fill_tiles(&area.tiles, 9, 9, 4, 1, kRug);
  area.warps.push_back({7, 0, "blackpine-cottage-yard", 8, 3, Direction::Down});
  area.warps.push_back({8, 0, "blackpine-cottage-yard", 9, 3, Direction::Down});
  area.warps.push_back({7, 11, "bellfield", 5, 16, Direction::Down});
  area.warps.push_back({8, 11, "bellfield", 6, 16, Direction::Down});
  return area;
}

Area make_blackpine_cottage_yard() {
  Area area;
  area.id = "blackpine-cottage-yard";
  area.name = "BLACKPINE COTTAGE YARD";
  area.player_tillable = true;
  area.tiles = blank_rows(18, 14, kGrass);
  draw_tree_border(&area.tiles, -1, -1, -1, -1);

  fill_tiles(&area.tiles, 6, 0, 6, 1, kRoof);
  fill_tiles(&area.tiles, 6, 1, 6, 1, kWall);
  set_tile(&area.tiles, 8, 2, kDoor);
  set_tile(&area.tiles, 9, 2, kDoor);
  fill_tiles(&area.tiles, 2, 3, 13, 1, kFence);
  fill_tiles(&area.tiles, 2, 3, 1, 8, kFence);
  fill_tiles(&area.tiles, 14, 3, 1, 8, kFence);
  fill_tiles(&area.tiles, 7, 3, 4, 1, kStone);
  fill_tiles(&area.tiles, 3, 10, 4, 1, kFence);
  fill_tiles(&area.tiles, 10, 10, 4, 1, kFence);
  fill_tiles(&area.tiles, 4, 5, 8, 4, kGarden);
  fill_tiles(&area.tiles, 12, 5, 1, 4, kHerb);
  set_tile(&area.tiles, 3, 4, kBench);
  set_tile(&area.tiles, 13, 4, kBarrel);
  set_tile(&area.tiles, 13, 9, kCrate);
  set_tile(&area.tiles, 5, 4, kFlower);
  set_tile(&area.tiles, 6, 4, kFlower);

  area.warps.push_back({8, 2, "blackpine-cottage", 7, 1, Direction::Down});
  area.warps.push_back({9, 2, "blackpine-cottage", 8, 1, Direction::Down});
  return area;
}

Area make_blackpine_hospital() {
  Area area;
  area.id = "blackpine-hospital";
  area.name = "SAINT LUKE INFIRMARY";
  area.indoor = true;
  area.tiles = blank_rows(18, 14, kFloor);

  fill_tiles(&area.tiles, 0, 0, 18, 1, kWall);
  fill_tiles(&area.tiles, 0, 13, 18, 1, kWall);
  fill_tiles(&area.tiles, 0, 0, 1, 14, kWall);
  fill_tiles(&area.tiles, 17, 0, 1, 14, kWall);
  set_tile(&area.tiles, 8, 13, kDoor);
  set_tile(&area.tiles, 9, 13, kDoor);
  fill_tiles(&area.tiles, 2, 2, 4, 1, kDesk);
  fill_tiles(&area.tiles, 11, 2, 4, 1, kShelf);
  set_tile(&area.tiles, 3, 2, kCandle);
  set_tile(&area.tiles, 12, 2, kCandle);
  fill_tiles(&area.tiles, 2, 4, 3, 1, kBed);
  fill_tiles(&area.tiles, 7, 4, 3, 1, kBed);
  fill_tiles(&area.tiles, 12, 4, 3, 1, kBed);
  fill_tiles(&area.tiles, 2, 8, 3, 1, kBed);
  fill_tiles(&area.tiles, 7, 8, 3, 1, kBed);
  fill_tiles(&area.tiles, 12, 8, 3, 1, kBed);
  fill_tiles(&area.tiles, 6, 11, 6, 1, kRug);
  set_tile(&area.tiles, 8, 10, kCandle);
  set_tile(&area.tiles, 14, 11, kHerb);
  set_tile(&area.tiles, 15, 11, kHerb);

  area.warps.push_back({8, 13, "bellfield", 27, 8, Direction::Down});
  area.warps.push_back({9, 13, "bellfield", 27, 8, Direction::Down});
  area.npcs.push_back({"sister-miriam", "SISTER MIRIAM", SpriteRole::Sister, 8, 6, Direction::Down, true});
  area.npcs.push_back({"apothecary-eda", "APOTHECARY EDA", SpriteRole::Elder, 13, 3, Direction::Left, true});
  return area;
}

std::vector<Area> build_world() {
  std::vector<Area> world;
  world.push_back(make_house());
  world.push_back(make_lane());
  world.push_back(make_square());
  world.push_back(make_priory_road());
  world.push_back(make_priory_gate());
  world.push_back(make_priory_court());
  world.push_back(make_bellfield());
  world.push_back(make_candlewharf());
  world.push_back(make_scriptorium());
  world.push_back(make_chapel());
  world.push_back(make_hospice());
  world.push_back(make_blackpine_cottage());
  world.push_back(make_blackpine_cottage_yard());
  world.push_back(make_blackpine_hospital());
  for (auto& area : world) {
    finalize_outdoor_art(&area);
  }
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

CropPlot* mutable_crop_plot(GameState* state, const std::string& area_id, int x, int y) {
  for (auto& plot : state->crop_plots) {
    if (plot.area_id == area_id && plot.x == x && plot.y == y) {
      return &plot;
    }
  }
  return nullptr;
}

const CropPlot* crop_plot_at(const GameState& state, const std::string& area_id, int x, int y) {
  for (const auto& plot : state.crop_plots) {
    if (plot.area_id == area_id && plot.x == x && plot.y == y) {
      return &plot;
    }
  }
  return nullptr;
}

DecorPlacement* mutable_decor_placement(GameState* state, const std::string& area_id, int x, int y) {
  for (auto& placement : state->decor_placements) {
    if (placement.area_id == area_id && placement.x == x && placement.y == y) {
      return &placement;
    }
  }
  return nullptr;
}

const DecorPlacement* decor_placement_at(const GameState& state, const std::string& area_id, int x, int y) {
  for (const auto& placement : state.decor_placements) {
    if (placement.area_id == area_id && placement.x == x && placement.y == y) {
      return &placement;
    }
  }
  return nullptr;
}

const LifePathDefinition& current_life_path(const GameState& state) {
  const auto& paths = life_path_definitions();
  const int index = std::clamp(state.profile.life_path_index, 0, static_cast<int>(paths.size()) - 1);
  return paths[static_cast<std::size_t>(index)];
}

const ToneDefinition& current_complexion(const GameState& state) {
  const auto& tones = complexion_definitions();
  const int index = std::clamp(state.profile.complexion_index, 0, static_cast<int>(tones.size()) - 1);
  return tones[static_cast<std::size_t>(index)];
}

const ToneDefinition& current_hair(const GameState& state) {
  const auto& tones = hair_definitions();
  const int index = std::clamp(state.profile.hair_index, 0, static_cast<int>(tones.size()) - 1);
  return tones[static_cast<std::size_t>(index)];
}

void set_status_message(GameState* state, const std::string& message, Uint32 now);

std::map<std::string, int> default_virtues() {
  return {
      {"fortitude", 0},
      {"temperance", 0},
      {"faith", 0},
      {"hope", 0},
      {"charity", 0},
      {"humility", 0},
  };
}

std::string canonical_virtue_key(const std::string& key) {
  const std::string lower = uppercase(key);
  if (lower == "PRUDENCE") {
    return "humility";
  }
  if (lower == "JUSTICE") {
    return "charity";
  }
  std::string canonical = key;
  std::transform(canonical.begin(), canonical.end(), canonical.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return canonical;
}

const char* virtue_label(const std::string& key) {
  if (key == "fortitude") {
    return "FORTITUDE";
  }
  if (key == "temperance") {
    return "TEMPERANCE";
  }
  if (key == "faith") {
    return "FAITH";
  }
  if (key == "hope") {
    return "HOPE";
  }
  if (key == "charity") {
    return "CHARITY";
  }
  if (key == "humility") {
    return "HUMILITY";
  }
  return "VIRTUE";
}

int virtue_value(const GameState& state, const std::string& key) {
  const auto canonical = canonical_virtue_key(key);
  const auto found = state.profile.virtues.find(canonical);
  return found == state.profile.virtues.end() ? 0 : found->second;
}

void ensure_virtues(ProfileState* profile) {
  for (const auto& [key, value] : default_virtues()) {
    if (profile->virtues.count(key) == 0U) {
      profile->virtues[key] = value;
    }
  }
}

void add_virtue(GameState* state, const std::string& key, int delta, Uint32 now) {
  const std::string canonical = canonical_virtue_key(key);
  if (canonical.empty() || delta == 0) {
    return;
  }
  ensure_virtues(&state->profile);
  state->profile.virtues[canonical] += delta;
  set_status_message(state, std::string(virtue_label(canonical)) + " +" + std::to_string(delta) + ".", now);
}

std::vector<std::string> owned_homes(const GameState& state) {
  return state.profile.owned_homes;
}

bool owns_home(const GameState& state, const std::string& home_id) {
  return std::find(state.profile.owned_homes.begin(), state.profile.owned_homes.end(), home_id) !=
         state.profile.owned_homes.end();
}

void unlock_home(GameState* state, const std::string& home_id) {
  if (home_id.empty() || owns_home(*state, home_id)) {
    return;
  }
  state->profile.owned_homes.push_back(home_id);
  if (state->profile.active_home_id.empty()) {
    state->profile.active_home_id = home_id;
  }
}

std::string home_name(const std::string& home_id) {
  if (home_id == "blackpine-cottage") {
    return "BLACKPINE COTTAGE";
  }
  return uppercase(home_id);
}

bool area_belongs_to_home(const std::string& area_id, const std::string& home_id) {
  if (home_id.empty()) {
    return false;
  }
  if (area_id == home_id) {
    return true;
  }
  return area_id == home_id + "-yard";
}

char decor_tile_for_item(const std::string& item_id) {
  if (const InventoryItemDefinition* definition = inventory_definition(item_id)) {
    return definition->decor_tile;
  }
  return '\0';
}

bool decor_blocks_movement(const std::string& item_id) {
  if (const InventoryItemDefinition* definition = inventory_definition(item_id)) {
    return definition->decor_blocks;
  }
  return false;
}

bool tile_is_tillable_ground(char tile) {
  return tile == kGrass || tile == kGarden || tile == kHerb || tile == kFlower;
}

std::string inventory_name(const std::string& id);

std::string active_item_label(const GameState& state) {
  if (state.profile.active_item_id.empty()) {
    return "NONE";
  }
  return inventory_name(state.profile.active_item_id);
}

bool opening_square_reached(const GameState& state) {
  return state.current_area == "square" || state.current_area == "priory-road" || state.current_area == "priory-gate" ||
         state.current_area == "priory-court" || state.current_area == "chapel" || state.current_area == "scriptorium" ||
         state.current_area == "saint-hilda-hospice" || state.current_area == "bellfield" ||
         state.current_area == "candlewharf" || state.current_area == "blackpine-cottage" ||
         state.current_area == "blackpine-cottage-yard" || state.current_area == "blackpine-hospital";
}

bool village_green_reached(const GameState& state) {
  return state.progress.visited_village_green || state.current_area == "bellfield" ||
         state.current_area == "blackpine-cottage" || state.current_area == "blackpine-cottage-yard" ||
         state.current_area == "blackpine-hospital" || state.progress.met_prioress || state.progress.visited_quay;
}

bool priory_arrival_complete(const GameState& state) {
  return state.progress.task_board_read && state.progress.met_martin && state.progress.met_prioress &&
         village_green_reached(state) && state.progress.visited_quay;
}

std::vector<std::string> priory_arrival_missing(const GameState& state) {
  std::vector<std::string> missing;
  if (!state.progress.task_board_read) {
    missing.push_back("THE TASK BOARD");
  }
  if (!state.progress.met_martin) {
    missing.push_back("BROTHER MARTIN");
  }
  if (!state.progress.met_prioress) {
    missing.push_back("SAINT HILDA HOSPICE");
  }
  if (!village_green_reached(state)) {
    missing.push_back("BLACKPINE VILLAGE GREEN");
  }
  if (!state.progress.visited_quay) {
    missing.push_back("RAVENSCAR QUAY");
  }
  return missing;
}

std::string joined_missing_list(const std::vector<std::string>& missing) {
  std::string text;
  for (std::size_t index = 0; index < missing.size(); ++index) {
    if (index != 0U) {
      text += index + 1U == missing.size() ? " AND " : ", ";
    }
    text += missing[index];
  }
  return text;
}

std::string martin_vocation_line(const GameState& state) {
  switch (std::clamp(state.profile.life_path_index, 0, 4)) {
    case 1:
      return "'THEN LEARN TO READ SOULS AS CLOSELY AS TEXTS,' HE SAYS.";
    case 2:
      return "'THEN GUARD WITHOUT BECOMING WHAT YOU RESIST,' HE SAYS.";
    default:
      return "'THEN YOUR WORDS MUST BE LEAN AND TRUE,' HE SAYS.";
  }
}

void apply_intro_virtues(GameState* state) {
  ensure_virtues(&state->profile);
  switch (std::clamp(state->profile.life_path_index, 0, 4)) {
    case 0:
      if (state->profile.reflection_index == 0) {
        state->profile.virtues["temperance"] += 1;
        state->profile.virtues["humility"] += 1;
      } else {
        state->profile.virtues["charity"] += 1;
        state->profile.virtues["fortitude"] += 1;
      }
      break;
    case 1:
      if (state->profile.reflection_index == 0) {
        state->profile.virtues["temperance"] += 1;
      } else {
        state->profile.virtues["charity"] += 1;
        state->profile.virtues["humility"] += 1;
      }
      break;
    case 2:
      if (state->profile.reflection_index == 0) {
        state->profile.virtues["temperance"] += 1;
        state->profile.virtues["charity"] += 1;
      } else {
        state->profile.virtues["fortitude"] += 1;
      }
      break;
    case 3:
      if (state->profile.reflection_index == 0) {
        state->profile.virtues["charity"] += 1;
        state->profile.virtues["temperance"] += 1;
      } else {
        state->profile.virtues["humility"] += 1;
        state->profile.virtues["fortitude"] += 1;
      }
      break;
    case 4:
    default:
      if (state->profile.reflection_index == 0) {
        state->profile.virtues["charity"] += 1;
        state->profile.virtues["temperance"] += 1;
      } else {
        state->profile.virtues["fortitude"] += 1;
        state->profile.virtues["humility"] += 1;
      }
      break;
  }
}

bool has_outfit(const GameState& state, const std::string& id) {
  return std::find(state.profile.unlocked_outfits.begin(), state.profile.unlocked_outfits.end(), id) !=
         state.profile.unlocked_outfits.end();
}

void unlock_outfit(GameState* state, const std::string& id) {
  if (id.empty() || has_outfit(*state, id)) {
    return;
  }
  state->profile.unlocked_outfits.push_back(id);
}

void add_inventory_item(GameState* state, const std::string& id, int count = 1) {
  if (id.empty() || count <= 0) {
    return;
  }
  state->profile.inventory[id] += count;
}

void remove_inventory_item(GameState* state, const std::string& id, int count = 1) {
  if (id.empty() || count <= 0) {
    return;
  }
  auto found = state->profile.inventory.find(id);
  if (found == state->profile.inventory.end()) {
    return;
  }
  found->second -= count;
  if (found->second <= 0) {
    state->profile.inventory.erase(found);
  }
}

void add_stored_item(GameState* state, const std::string& id, int count = 1) {
  if (id.empty() || count <= 0) {
    return;
  }
  state->profile.stored_items[id] += count;
}

void remove_stored_item(GameState* state, const std::string& id, int count = 1) {
  if (id.empty() || count <= 0) {
    return;
  }
  auto found = state->profile.stored_items.find(id);
  if (found == state->profile.stored_items.end()) {
    return;
  }
  found->second -= count;
  if (found->second <= 0) {
    state->profile.stored_items.erase(found);
  }
}

int inventory_count(const GameState& state, const std::string& id) {
  const auto found = state.profile.inventory.find(id);
  return found == state.profile.inventory.end() ? 0 : found->second;
}

int stored_count(const GameState& state, const std::string& id) {
  const auto found = state.profile.stored_items.find(id);
  return found == state.profile.stored_items.end() ? 0 : found->second;
}

std::string coin_text(int coins) {
  return std::to_string(coins) + "D";
}

bool item_is_food(const std::string& id) {
  if (const InventoryItemDefinition* definition = inventory_definition(id)) {
    return definition->heal_amount > 0;
  }
  return false;
}

bool item_is_seed(const std::string& id) {
  if (const InventoryItemDefinition* definition = inventory_definition(id)) {
    return !definition->crop_id.empty();
  }
  return false;
}

bool item_is_decor(const std::string& id) {
  if (const InventoryItemDefinition* definition = inventory_definition(id)) {
    return definition->decor_tile != '\0';
  }
  return false;
}

std::string inventory_name(const std::string& id) {
  if (const InventoryItemDefinition* definition = inventory_definition(id)) {
    return definition->name;
  }
  return uppercase(id);
}

std::string inventory_name_list(const std::vector<std::string>& ids) {
  std::string joined;
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (index != 0U) {
      joined += ", ";
    }
    joined += inventory_name(ids[index]);
  }
  return joined;
}

std::vector<std::string> inventory_entry_lines(const std::map<std::string, int>& items) {
  std::vector<std::string> lines;
  for (const auto& definition : inventory_definitions()) {
    const auto found = items.find(definition.id);
    if (found == items.end() || found->second <= 0) {
      continue;
    }
    lines.push_back(definition.name + " x" + std::to_string(found->second));
  }
  for (const auto& [id, count] : items) {
    if (count <= 0 || inventory_definition(id) != nullptr) {
      continue;
    }
    lines.push_back(uppercase(id) + " x" + std::to_string(count));
  }
  return lines;
}

std::vector<std::string> sorted_item_ids(const std::map<std::string, int>& items) {
  std::vector<std::string> ids;
  for (const auto& definition : inventory_definitions()) {
    const auto found = items.find(definition.id);
    if (found != items.end() && found->second > 0) {
      ids.push_back(definition.id);
    }
  }
  for (const auto& [id, count] : items) {
    if (count <= 0 || inventory_definition(id) != nullptr) {
      continue;
    }
    ids.push_back(id);
  }
  return ids;
}

std::string join_strings(const std::vector<std::string>& values, const char* separator) {
  std::string joined;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0U) {
      joined += separator;
    }
    joined += values[index];
  }
  return joined;
}

std::vector<std::string> split_string(const std::string& value, char separator) {
  std::vector<std::string> items;
  std::istringstream stream(value);
  std::string item;
  while (std::getline(stream, item, separator)) {
    if (!item.empty()) {
      items.push_back(item);
    }
  }
  return items;
}

std::string serialize_inventory(const std::map<std::string, int>& inventory) {
  std::vector<std::string> entries;
  for (const auto& [id, count] : inventory) {
    entries.push_back(id + ":" + std::to_string(count));
  }
  return join_strings(entries, ",");
}

std::map<std::string, int> deserialize_inventory(const std::string& value) {
  std::map<std::string, int> inventory;
  for (const auto& entry : split_string(value, ',')) {
    const std::size_t split = entry.find(':');
    if (split == std::string::npos) {
      continue;
    }
    const int count = std::atoi(entry.substr(split + 1).c_str());
    if (count > 0) {
      inventory[entry.substr(0, split)] = count;
    }
  }
  return inventory;
}

std::string serialize_crop_plots(const std::vector<CropPlot>& plots) {
  std::vector<std::string> entries;
  for (const auto& plot : plots) {
    entries.push_back(plot.area_id + ":" + std::to_string(plot.x) + ":" + std::to_string(plot.y) + ":" +
                      (plot.tilled ? "1" : "0") + ":" + plot.crop_id + ":" + std::to_string(plot.stage));
  }
  return join_strings(entries, ",");
}

std::vector<CropPlot> deserialize_crop_plots(const std::string& value) {
  std::vector<CropPlot> plots;
  for (const auto& entry : split_string(value, ',')) {
    const std::vector<std::string> parts = split_string(entry, ':');
    if (parts.size() < 6) {
      continue;
    }
    CropPlot plot;
    plot.area_id = parts[0];
    plot.x = std::atoi(parts[1].c_str());
    plot.y = std::atoi(parts[2].c_str());
    plot.tilled = parts[3] == "1";
    plot.crop_id = parts[4];
    plot.stage = std::atoi(parts[5].c_str());
    plots.push_back(plot);
  }
  return plots;
}

std::string serialize_decor_placements(const std::vector<DecorPlacement>& placements) {
  std::vector<std::string> entries;
  for (const auto& placement : placements) {
    entries.push_back(placement.area_id + ":" + std::to_string(placement.x) + ":" + std::to_string(placement.y) +
                      ":" + placement.item_id);
  }
  return join_strings(entries, ",");
}

std::vector<DecorPlacement> deserialize_decor_placements(const std::string& value) {
  std::vector<DecorPlacement> placements;
  for (const auto& entry : split_string(value, ',')) {
    const std::vector<std::string> parts = split_string(entry, ':');
    if (parts.size() < 4) {
      continue;
    }
    placements.push_back({parts[0], std::atoi(parts[1].c_str()), std::atoi(parts[2].c_str()), parts[3]});
  }
  return placements;
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

bool tile_in_bounds(const Area& area, int x, int y);
char tile_at(const Area& area, int x, int y);
bool tile_passable(char tile);
bool tile_occupied_by_npc(const GameState& state, const std::string& area_id, int x, int y);
bool tile_locked_by_progress(const GameState& state, const std::string& area_id, int x, int y);
const Warp* warp_at(const Area& area, int x, int y);

Monster* mutable_monster_at(GameState* state, const std::string& area_id, int x, int y) {
  Area* area = mutable_area(state, area_id);
  if (area == nullptr) {
    return nullptr;
  }
  for (auto& monster : area->monsters) {
    if (monster.hp > 0 && monster.x == x && monster.y == y) {
      return &monster;
    }
  }
  return nullptr;
}

const Monster* monster_at(const GameState& state, const std::string& area_id, int x, int y) {
  const Area* area = area_for(state, area_id);
  if (area == nullptr) {
    return nullptr;
  }
  for (const auto& monster : area->monsters) {
    if (monster.hp > 0 && monster.x == x && monster.y == y) {
      return &monster;
    }
  }
  return nullptr;
}

bool monster_can_occupy(const GameState& state,
                        const std::string& area_id,
                        int x,
                        int y,
                        const std::string& self_id = "") {
  const Area* area = area_for(state, area_id);
  if (area == nullptr || !tile_in_bounds(*area, x, y)) {
    return false;
  }
  if (!tile_passable(tile_at(*area, x, y)) || tile_locked_by_progress(state, area_id, x, y)) {
    return false;
  }
  if (tile_occupied_by_npc(state, area_id, x, y)) {
    return false;
  }
  if (warp_at(*area, x, y) != nullptr) {
    return false;
  }
  for (const auto& monster : area->monsters) {
    if (monster.hp <= 0 || monster.id == self_id) {
      continue;
    }
    if (monster.x == x && monster.y == y) {
      return false;
    }
  }
  if (state.current_area == area_id && state.player.tile_x == x && state.player.tile_y == y) {
    return false;
  }
  return true;
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
    case kHerb:
    case kRug:
    case kReed:
    case kCartStep:
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

bool tile_occupied_by_monster(const GameState& state, const std::string& area_id, int x, int y) {
  const Area* area = area_for(state, area_id);
  if (area == nullptr) {
    return false;
  }
  for (const auto& monster : area->monsters) {
    if (monster.hp > 0 && monster.x == x && monster.y == y) {
      return true;
    }
  }
  return false;
}

bool tile_blocked_by_decor(const GameState& state, const std::string& area_id, int x, int y) {
  if (const DecorPlacement* placement = decor_placement_at(state, area_id, x, y)) {
    return decor_blocks_movement(placement->item_id);
  }
  return false;
}

bool tile_locked_by_progress(const GameState& state, const std::string& area_id, int x, int y) {
  if (area_id == "priory-gate" && !state.progress.gate_opened && x >= 8 && x <= 11 && y >= 6 && y <= 7) {
    return true;
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
  output << "hp=" << state.player.hp << "\n";
  output << "max_hp=" << state.player.max_hp << "\n";
  output << "life_path=" << state.profile.life_path_index << "\n";
  output << "reflection=" << state.profile.reflection_index << "\n";
  output << "complexion=" << state.profile.complexion_index << "\n";
  output << "hair=" << state.profile.hair_index << "\n";
  output << "day=" << state.profile.day << "\n";
  output << "coins=" << state.profile.coins << "\n";
  output << "equipped_outfit=" << state.profile.equipped_outfit_id << "\n";
  output << "unlocked_outfits=" << join_strings(state.profile.unlocked_outfits, ",") << "\n";
  output << "inventory=" << serialize_inventory(state.profile.inventory) << "\n";
  output << "stored=" << serialize_inventory(state.profile.stored_items) << "\n";
  output << "virtues=" << serialize_inventory(state.profile.virtues) << "\n";
  output << "owned_homes=" << join_strings(state.profile.owned_homes, ",") << "\n";
  output << "active_home=" << state.profile.active_home_id << "\n";
  output << "active_item=" << state.profile.active_item_id << "\n";
  output << "crops=" << serialize_crop_plots(state.crop_plots) << "\n";
  output << "decor=" << serialize_decor_placements(state.decor_placements) << "\n";
  output << "started=" << (state.quest.started ? 1 : 0) << "\n";
  output << "wax=" << (state.quest.wax ? 1 : 0) << "\n";
  output << "leaf=" << (state.quest.leaf ? 1 : 0) << "\n";
  output << "oil=" << (state.quest.oil ? 1 : 0) << "\n";
  output << "complete=" << (state.quest.complete ? 1 : 0) << "\n";
  output << "father_counsel=" << (state.progress.father_counsel ? 1 : 0) << "\n";
  output << "mother_blessing=" << (state.progress.mother_blessing ? 1 : 0) << "\n";
  output << "friar_blessing=" << (state.progress.friar_blessing ? 1 : 0) << "\n";
  output << "gate_opened=" << (state.progress.gate_opened ? 1 : 0) << "\n";
  output << "task_board_read=" << (state.progress.task_board_read ? 1 : 0) << "\n";
  output << "met_martin=" << (state.progress.met_martin ? 1 : 0) << "\n";
  output << "met_prioress=" << (state.progress.met_prioress ? 1 : 0) << "\n";
  output << "visited_blackpine=" << (state.progress.visited_blackpine ? 1 : 0) << "\n";
  output << "visited_village_green=" << (state.progress.visited_village_green ? 1 : 0) << "\n";
  output << "visited_quay=" << (state.progress.visited_quay ? 1 : 0) << "\n";
  output << "chapel_prayer_index=" << state.progress.chapel_prayer_index << "\n";
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
  state->player.max_hp =
      values.count("max_hp") != 0U ? std::max(1, std::atoi(values["max_hp"].c_str())) : 12;
  state->player.hp = values.count("hp") != 0U ? std::clamp(std::atoi(values["hp"].c_str()), 1, state->player.max_hp)
                                               : state->player.max_hp;
  state->profile.life_path_index = std::atoi(values["life_path"].c_str());
  state->profile.reflection_index = std::atoi(values["reflection"].c_str());
  state->profile.complexion_index = std::atoi(values["complexion"].c_str());
  state->profile.hair_index = std::atoi(values["hair"].c_str());
  state->profile.day = values.count("day") != 0U ? std::max(1, std::atoi(values["day"].c_str())) : 1;
  state->profile.coins = std::atoi(values["coins"].c_str());
  state->profile.equipped_outfit_id =
      values.count("equipped_outfit") != 0U ? values["equipped_outfit"] : current_life_path(*state).starter_outfit_id;
  state->profile.unlocked_outfits =
      values.count("unlocked_outfits") != 0U ? split_string(values["unlocked_outfits"], ',')
                                             : std::vector<std::string>{state->profile.equipped_outfit_id};
  if (state->profile.unlocked_outfits.empty()) {
    state->profile.unlocked_outfits.push_back(state->profile.equipped_outfit_id);
  }
  if (!has_outfit(*state, state->profile.equipped_outfit_id)) {
    state->profile.unlocked_outfits.push_back(state->profile.equipped_outfit_id);
  }
  state->profile.inventory =
      values.count("inventory") != 0U ? deserialize_inventory(values["inventory"]) : std::map<std::string, int>{};
  state->profile.stored_items =
      values.count("stored") != 0U ? deserialize_inventory(values["stored"]) : std::map<std::string, int>{};
  state->profile.virtues =
      values.count("virtues") != 0U ? deserialize_inventory(values["virtues"]) : default_virtues();
  ensure_virtues(&state->profile);
  state->profile.owned_homes =
      values.count("owned_homes") != 0U ? split_string(values["owned_homes"], ',') : std::vector<std::string>{};
  state->profile.active_home_id = values.count("active_home") != 0U ? values["active_home"] : "";
  state->profile.active_item_id = values.count("active_item") != 0U ? values["active_item"] : "";
  state->crop_plots =
      values.count("crops") != 0U ? deserialize_crop_plots(values["crops"]) : std::vector<CropPlot>{};
  state->decor_placements =
      values.count("decor") != 0U ? deserialize_decor_placements(values["decor"]) : std::vector<DecorPlacement>{};
  state->quest.started = values["started"] == "1";
  state->quest.wax = values["wax"] == "1";
  state->quest.leaf = (values.count("leaf") != 0U && values["leaf"] == "1") ||
                      (values.count("hymn") != 0U && values["hymn"] == "1");
  state->quest.oil = values["oil"] == "1";
  state->quest.complete = values["complete"] == "1";
  state->progress.father_counsel = values.count("father_counsel") != 0U && values["father_counsel"] == "1";
  state->progress.mother_blessing = values.count("mother_blessing") != 0U && values["mother_blessing"] == "1";
  state->progress.friar_blessing = values.count("friar_blessing") != 0U && values["friar_blessing"] == "1";
  state->progress.gate_opened = values.count("gate_opened") != 0U && values["gate_opened"] == "1";
  state->progress.task_board_read = values.count("task_board_read") != 0U && values["task_board_read"] == "1";
  state->progress.met_martin = values.count("met_martin") != 0U && values["met_martin"] == "1";
  state->progress.met_prioress = values.count("met_prioress") != 0U && values["met_prioress"] == "1";
  state->progress.visited_blackpine = values.count("visited_blackpine") != 0U && values["visited_blackpine"] == "1";
  state->progress.visited_village_green =
      values.count("visited_village_green") != 0U && values["visited_village_green"] == "1";
  state->progress.visited_quay = values.count("visited_quay") != 0U && values["visited_quay"] == "1";
  state->progress.chapel_prayer_index =
      values.count("chapel_prayer_index") != 0U ? std::atoi(values["chapel_prayer_index"].c_str()) : 0;
  if (state->current_area == "square" || state->current_area == "bellfield") {
    state->progress.visited_blackpine = true;
  }
  if (state->current_area == "bellfield" || state->current_area == "blackpine-cottage" ||
      state->current_area == "blackpine-cottage-yard" || state->current_area == "blackpine-hospital" ||
      state->progress.met_prioress || state->progress.visited_quay) {
    state->progress.visited_village_green = true;
  }
  if (!state->profile.active_home_id.empty() && !owns_home(*state, state->profile.active_home_id)) {
    unlock_home(state, state->profile.active_home_id);
  }
  return true;
}

void open_shop(GameState* state, ShopId id);

void start_dialogue(GameState* state,
                    const std::string& speaker,
                    std::vector<std::string> pages,
                    ShopId shop_after = ShopId::None) {
  state->dialogue.active = true;
  state->dialogue.speaker = speaker;
  state->dialogue.pages.clear();
  state->dialogue.shop_after = shop_after;
  const SDL_Rect dialogue_body_box = {0, 0, kDialogBodyWidth, kDialogBodyHeight};
  const TextBoxOptions dialogue_body_options = {
      1, 1, true, true, 0, 2, TextAlign::Left, TextVerticalAlign::Top};
  for (const auto& page : pages) {
    const auto paginated = layout_text_box(page, dialogue_body_box, dialogue_body_options);
    state->dialogue.pages.insert(state->dialogue.pages.end(), paginated.begin(), paginated.end());
  }
  if (state->dialogue.pages.empty()) {
    state->dialogue.pages.push_back({{""}, 2});
  }
  state->dialogue.page_index = 0;
}

std::string objective_text(const GameState& state) {
  if (!state.progress.gate_opened) {
    if (state.current_area == "house") {
      return "LEAVE YOUR FATHER'S HOUSE AND STEP INTO BLACKPINE.";
    }
    if (state.current_area == "lane") {
      return "CLIMB INTO THE DRIVER'S CART FOR BLACKPINE MARKET SQUARE.";
    }
    if (state.current_area == "square") {
      return state.progress.friar_blessing ? "TAKE THE PRIORY ROAD TOWARD SAINT CATHERINE."
                                           : "SPEAK TO THE FRANCISCAN FRIAR OR TAKE THE PRIORY ROAD NORTH.";
    }
    if (state.current_area == "priory-road") {
      return "REACH SAINT CATHERINE'S GATE.";
    }
    if (state.current_area == "priory-gate") {
      return "PRESENT YOUR LETTER AND ENTER SAINT CATHERINE.";
    }
  }
  if (!state.progress.task_board_read) {
    return "READ THE TASK BOARD IN SAINT CATHERINE'S COURTYARD.";
  }
  if (!priory_arrival_complete(state)) {
    return "WALK THE HOUSE OF SAINT CATHERINE: VISIT " + joined_missing_list(priory_arrival_missing(state)) + ".";
  }
  return "TAKE A DUTY FROM THE TASK BOARD OR SPEAK WITH THE PRIOR IN THE CHAPEL.";
}

void open_shop(GameState* state, ShopId id) {
  state->chest = {};
  state->shop.active = true;
  state->shop.id = id;
  state->shop.selection = 0;
}

bool shop_offer_owned(const GameState& state, const ShopOffer& offer) {
  if (!offer.outfit_id.empty() && has_outfit(state, offer.outfit_id)) {
    return true;
  }
  if (!offer.home_id.empty() && owns_home(state, offer.home_id)) {
    return true;
  }
  return false;
}

bool purchase_shop_offer(GameState* state, Uint32 now) {
  const auto& offers = shop_offers(state->shop.id);
  if (offers.empty()) {
    return false;
  }
  state->shop.selection = std::clamp(state->shop.selection, 0, static_cast<int>(offers.size()) - 1);
  const ShopOffer& offer = offers[static_cast<std::size_t>(state->shop.selection)];
  if (!offer.outfit_id.empty() && has_outfit(*state, offer.outfit_id)) {
    state->profile.equipped_outfit_id = offer.outfit_id;
    set_status_message(state, "YOU EQUIP A KNOWN GARMENT.", now);
    save_game(*state);
    return true;
  }
  if (!offer.home_id.empty() && owns_home(*state, offer.home_id)) {
    state->profile.active_home_id = offer.home_id;
    set_status_message(state, home_name(offer.home_id) + " IS NOW YOUR ACTIVE HOME.", now);
    save_game(*state);
    return true;
  }
  if (state->profile.coins < offer.price) {
    set_status_message(state, "NOT ENOUGH COIN.", now);
    return false;
  }

  state->profile.coins -= offer.price;
  if (!offer.item_id.empty()) {
    add_inventory_item(state, offer.item_id);
  }
  if (!offer.outfit_id.empty()) {
    unlock_outfit(state, offer.outfit_id);
    state->profile.equipped_outfit_id = offer.outfit_id;
  }
  if (!offer.home_id.empty()) {
    unlock_home(state, offer.home_id);
    state->profile.active_home_id = offer.home_id;
  }
  set_status_message(state, offer.status_message, now);
  save_game(*state);
  return true;
}

std::vector<std::string> inventory_lines(const GameState& state) {
  std::vector<std::string> lines = inventory_entry_lines(state.profile.inventory);
  if (lines.empty()) {
    lines.push_back("NO PACK ITEMS YET.");
  }
  return lines;
}

std::vector<std::string> stored_lines(const GameState& state) {
  std::vector<std::string> lines = inventory_entry_lines(state.profile.stored_items);
  if (lines.empty()) {
    lines.push_back("CHEST IS EMPTY.");
  }
  return lines;
}

void clear_active_item_if_missing(GameState* state);

bool store_selected_item(GameState* state, Uint32 now) {
  const std::vector<std::string> ids = sorted_item_ids(state->profile.inventory);
  if (ids.empty()) {
    set_status_message(state, "NOTHING TO STORE.", now);
    return false;
  }
  state->chest.inventory_selection =
      std::clamp(state->chest.inventory_selection, 0, static_cast<int>(ids.size()) - 1);
  const std::string& item_id = ids[static_cast<std::size_t>(state->chest.inventory_selection)];
  remove_inventory_item(state, item_id);
  add_stored_item(state, item_id);
  clear_active_item_if_missing(state);
  set_status_message(state, inventory_name(item_id) + " STORED.", now);
  save_game(*state);
  return true;
}

bool retrieve_selected_item(GameState* state, Uint32 now) {
  const std::vector<std::string> ids = sorted_item_ids(state->profile.stored_items);
  if (ids.empty()) {
    set_status_message(state, "CHEST IS EMPTY.", now);
    return false;
  }
  state->chest.storage_selection =
      std::clamp(state->chest.storage_selection, 0, static_cast<int>(ids.size()) - 1);
  const std::string& item_id = ids[static_cast<std::size_t>(state->chest.storage_selection)];
  remove_stored_item(state, item_id);
  add_inventory_item(state, item_id);
  set_status_message(state, inventory_name(item_id) + " RETRIEVED.", now);
  save_game(*state);
  return true;
}

void clear_active_item_if_missing(GameState* state) {
  if (!state->profile.active_item_id.empty() && inventory_count(*state, state->profile.active_item_id) <= 0) {
    state->profile.active_item_id.clear();
  }
}

bool use_selected_pack_item(GameState* state, Uint32 now) {
  const std::vector<std::string> ids = sorted_item_ids(state->profile.inventory);
  if (ids.empty()) {
    set_status_message(state, "YOUR PACK IS EMPTY.", now);
    return false;
  }

  state->profile.pack_selection = std::clamp(state->profile.pack_selection, 0, static_cast<int>(ids.size()) - 1);
  const std::string& item_id = ids[static_cast<std::size_t>(state->profile.pack_selection)];
  const InventoryItemDefinition* definition = inventory_definition(item_id);
  if (definition == nullptr) {
    return false;
  }

  if (definition->heal_amount > 0) {
    if (state->player.hp >= state->player.max_hp) {
      set_status_message(state, "YOU DO NOT NEED FOOD JUST NOW.", now);
      return false;
    }
    remove_inventory_item(state, item_id);
    state->player.hp = std::min(state->player.max_hp, state->player.hp + definition->heal_amount);
    state->profile.active_item_id.clear();
    set_status_message(state, "YOU EAT " + uppercase(definition->name) + " AND RECOVER.", now);
    save_game(*state);
    return true;
  }

  if (!definition->crop_id.empty() || definition->decor_tile != '\0') {
    state->profile.active_item_id = item_id;
    set_status_message(state, uppercase(definition->name) + " IS NOW ACTIVE.", now);
    save_game(*state);
    return true;
  }

  set_status_message(state, "THIS ITEM IS CARRIED, NOT USED HERE.", now);
  return false;
}

void advance_farm_day(GameState* state) {
  for (auto& plot : state->crop_plots) {
    if (!plot.tilled || plot.crop_id.empty()) {
      continue;
    }
    if (const CropDefinition* crop = crop_definition(plot.crop_id)) {
      plot.stage = std::min(crop->days_to_grow, plot.stage + 1);
    }
  }
}

void advance_day(GameState* state, Uint32 now) {
  ++state->profile.day;
  advance_farm_day(state);
  state->player.hp = std::min(state->player.max_hp, state->player.hp + 3);
  set_status_message(state, "DAY " + std::to_string(state->profile.day) + " AT PRIME.", now);
  save_game(*state);
}

bool place_active_decor(GameState* state, const Area& area, int x, int y, Uint32 now) {
  if (state->profile.active_item_id.empty() || !item_is_decor(state->profile.active_item_id) ||
      !area_belongs_to_home(area.id, state->profile.active_home_id)) {
    return false;
  }
  if (decor_placement_at(*state, area.id, x, y) != nullptr || warp_at(area, x, y) != nullptr ||
      tile_occupied_by_npc(*state, area.id, x, y) || tile_occupied_by_monster(*state, area.id, x, y)) {
    return false;
  }
  const char tile = tile_at(area, x, y);
  if (area.indoor) {
    if (!(tile == kFloor || tile == kRug || tile == kWood || tile == kStone)) {
      return false;
    }
  } else {
    if (!(tile == kGrass || tile == kStone || tile == kPath || tile == kWood || tile == kFlower)) {
      return false;
    }
    if (crop_plot_at(*state, area.id, x, y) != nullptr) {
      return false;
    }
  }
  state->decor_placements.push_back({area.id, x, y, state->profile.active_item_id});
  remove_inventory_item(state, state->profile.active_item_id);
  set_status_message(state, uppercase(inventory_name(state->profile.active_item_id)) + " PLACED.", now);
  clear_active_item_if_missing(state);
  save_game(*state);
  return true;
}

bool farm_tile_action(GameState* state, const Area& area, int x, int y, Uint32 now) {
  if (!area.player_tillable || !tile_is_tillable_ground(tile_at(area, x, y))) {
    return false;
  }
  CropPlot* plot = mutable_crop_plot(state, area.id, x, y);
  if (plot != nullptr && plot->tilled && !plot->crop_id.empty()) {
    const CropDefinition* crop = crop_definition(plot->crop_id);
    if (crop != nullptr && plot->stage >= crop->days_to_grow) {
      add_inventory_item(state, crop->harvest_item_id, crop->yield_count);
      set_status_message(state, "YOU HARVEST " + uppercase(crop->name) + ".", now);
      plot->crop_id.clear();
      plot->stage = 0;
      save_game(*state);
      return true;
    }
  }

  if (plot == nullptr) {
    if (inventory_count(*state, "field_tools") <= 0) {
      set_status_message(state, "YOU NEED FIELD TOOLS TO BREAK THE GROUND.", now);
      return false;
    }
    state->crop_plots.push_back({area.id, x, y, true, "", 0});
    set_status_message(state, "YOU TILL A NEW PLOT.", now);
    save_game(*state);
    return true;
  }

  if (plot->tilled && plot->crop_id.empty()) {
    const CropDefinition* crop = crop_definition_for_seed(state->profile.active_item_id);
    if (crop == nullptr || inventory_count(*state, state->profile.active_item_id) <= 0) {
      set_status_message(state, "SET A SEED PACK ACTIVE TO SOW THIS PLOT.", now);
      return false;
    }
    plot->crop_id = crop->id;
    plot->stage = 0;
    remove_inventory_item(state, state->profile.active_item_id);
    set_status_message(state, "YOU SOW " + uppercase(crop->name) + ".", now);
    clear_active_item_if_missing(state);
    save_game(*state);
    return true;
  }

  return false;
}

std::string equipped_outfit_name(const GameState& state) {
  if (const OutfitDefinition* outfit = outfit_definition(state.profile.equipped_outfit_id)) {
    return outfit->name;
  }
  return "UNKNOWN GARMENT";
}

void start_new_game(GameState* state, Uint32 now) {
  const LifePathDefinition& life_path = current_life_path(*state);
  state->current_area = "house";
  state->player.tile_x = 8;
  state->player.tile_y = 8;
  state->player.start_x = 8;
  state->player.start_y = 8;
  state->player.target_x = 8;
  state->player.target_y = 8;
  state->player.moving = false;
  state->player.facing = Direction::Down;
  state->player.max_hp = 12;
  state->player.hp = state->player.max_hp;
  state->player.swing_until = 0;
  state->player.attack_cooldown_until = 0;
  state->quest = {};
  state->progress = {};
  state->dialogue = {};
  state->journal_open = false;
  state->on_title = false;
  state->creation.active = false;
  state->shop = {};
  state->chest = {};
  state->title_selection = 0;
  state->bells_until = 0;
  state->profile.coins = (life_path.coin_min + life_path.coin_max) / 2;
  state->profile.day = 1;
  state->profile.inventory.clear();
  state->profile.stored_items.clear();
  state->profile.virtues = default_virtues();
  state->profile.unlocked_outfits.clear();
  state->profile.owned_homes.clear();
  state->profile.active_home_id.clear();
  state->profile.active_item_id.clear();
  for (const auto& item_id : life_path.starter_items) {
    add_inventory_item(state, item_id);
  }
  if (inventory_count(*state, "sealed_letter") == 0) {
    add_inventory_item(state, "sealed_letter");
  }
  unlock_outfit(state, life_path.starter_outfit_id);
  state->profile.equipped_outfit_id = life_path.starter_outfit_id;
  apply_intro_virtues(state);
  state->profile.virtues["faith"] += 1;
  state->profile.journal_page = 0;
  state->profile.pack_selection = 0;
  state->profile.wardrobe_selection = 0;
  state->crop_plots.clear();
  state->decor_placements.clear();
  set_area_banner(state, "YOUR FATHER'S HOUSE", now);
  start_dialogue(
      state,
      "CHRONICLE",
      {
          "DAWN MIST HANGS OVER BLACKPINE AS BELLS CALL PRIME. SAINT CATHERINE PRIORY STILL BEARS OLD FIRE-SCARS.",
          life_path.reflection_response[static_cast<std::size_t>(std::clamp(state->profile.reflection_index, 0, 1))],
          "YOUR FATHER'S HOUSE IS POOR BUT ORDERLY: RUSHLIGHT, WORN PSALTER, PATCHED CLOAK, AND A SUMMONS TO MERCY-WORK.",
          "THE SEALED LETTER BEARS A FADED LION CREST AND THE WORDS: TO SAINT CATHERINE PRIORY.",
          "STEP INTO BLACKPINE, TAKE THE PRIORY ROAD, AND ENTER SAINT CATHERINE.",
      });
  save_game(*state);
}

void apply_warp(GameState* state, const Warp& warp, Uint32 now) {
  const std::string from_area = state->current_area;
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
  if (state->current_area == "square" || state->current_area == "bellfield") {
    state->progress.visited_blackpine = true;
  }
  if (state->current_area == "bellfield" || state->current_area == "blackpine-cottage" ||
      state->current_area == "blackpine-cottage-yard" || state->current_area == "blackpine-hospital") {
    state->progress.visited_village_green = true;
  } else if (state->current_area == "candlewharf") {
    state->progress.visited_quay = true;
  }
  if (from_area == "priory-gate" && state->current_area == "priory-court") {
    state->progress.gate_opened = true;
  }
  save_game(*state);
}

void maybe_trigger_warp(GameState* state, Uint32 now) {
  if (!state->smoke_test && now < state->warp_cooldown_until) {
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
  if (tile_locked_by_progress(state, area_id, x, y)) {
    return false;
  }
  if (tile_occupied_by_npc(state, area_id, x, y)) {
    return false;
  }
  if (tile_occupied_by_monster(state, area_id, x, y)) {
    return false;
  }
  if (tile_blocked_by_decor(state, area_id, x, y)) {
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

struct RespawnPoint {
  const char* area_id = "house";
  int x = 8;
  int y = 8;
  const char* message = "YOU WITHDRAW TO YOUR FATHER'S HOUSE.";
};

RespawnPoint respawn_point_for(const GameState& state) {
  if (state.current_area == "priory-court" || state.current_area == "priory-gate" || state.current_area == "chapel" ||
      state.current_area == "scriptorium" || state.current_area == "saint-hilda-hospice") {
    return {"saint-hilda-hospice", 8, 10, "YOU WAKE UNDER SAINT HILDA'S CARE."};
  }
  return {"blackpine-hospital", 8, 11, "YOU WAKE IN SAINT LUKE'S INFIRMARY."};
}

void return_player_to_safety(GameState* state, Uint32 now) {
  state->player.hp = state->player.max_hp;
  state->player.swing_until = 0;
  state->player.attack_cooldown_until = 0;

  const RespawnPoint respawn = respawn_point_for(*state);
  state->current_area = respawn.area_id;
  state->player.tile_x = respawn.x;
  state->player.tile_y = respawn.y;
  set_status_message(state, respawn.message, now);

  state->player.start_x = state->player.tile_x;
  state->player.start_y = state->player.tile_y;
  state->player.target_x = state->player.tile_x;
  state->player.target_y = state->player.tile_y;
  state->player.facing = Direction::Down;
  state->player.moving = false;
  if (const Area* area = current_area(*state)) {
    set_area_banner(state, area->name, now);
  }
  save_game(*state);
}

int player_attack_damage(const GameState& state) {
  int bonus = 0;
  if (inventory_count(state, "worn_arming_dagger") > 0) {
    bonus = 1;
  }
  return kPlayerAttackDamage + bonus;
}

int monster_coin_reward(const Monster& monster) {
  switch (monster.kind) {
    case MonsterKind::Ratling:
      return 3;
    case MonsterKind::BogWisp:
      return 4;
    case MonsterKind::DockHound:
      return 5;
    default:
      return 2;
  }
}

bool player_attack(GameState* state, Uint32 now) {
  if (state->player.moving || now < state->player.attack_cooldown_until) {
    return false;
  }

  state->player.swing_until = now + kSwordSwingMs;
  state->player.attack_cooldown_until = now + kAttackCooldownMs;

  int dx = 0;
  int dy = 0;
  direction_delta(state->player.facing, &dx, &dy);
  const int target_x = state->player.tile_x + dx;
  const int target_y = state->player.tile_y + dy;
  Monster* monster = mutable_monster_at(state, state->current_area, target_x, target_y);
  if (monster == nullptr) {
    set_status_message(state, "YOU CUT THE AIR.", now);
    return false;
  }

  monster->hurt_until = now + 180U;
  monster->move_at = now + 220U;
  monster->attack_at = now + 420U;
  monster->hp = std::max(0, monster->hp - player_attack_damage(*state));
  if (monster->hp == 0) {
    const int reward = monster_coin_reward(*monster);
    state->profile.coins += reward;
    set_status_message(state, uppercase(monster->name) + " FALLS. +" + std::to_string(reward) + "D.", now);
  } else {
    set_status_message(state, "YOU STRIKE " + uppercase(monster->name) + ".", now);
  }
  save_game(*state);
  return true;
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
    begin_player_step(state, Direction::Up, now, buttons.select);
  } else if (buttons.down) {
    begin_player_step(state, Direction::Down, now, buttons.select);
  } else if (buttons.left) {
    begin_player_step(state, Direction::Left, now, buttons.select);
  } else if (buttons.right) {
    begin_player_step(state, Direction::Right, now, buttons.select);
  }
}

void update_monsters(GameState* state, Uint32 now) {
  Area* area = mutable_area(state, state->current_area);
  if (area == nullptr) {
    return;
  }

  for (auto& monster : area->monsters) {
    if (monster.hp <= 0) {
      continue;
    }

    const int distance = manhattan_distance(monster.x, monster.y, state->player.tile_x, state->player.tile_y);
    if (distance <= 1) {
      if (now >= monster.attack_at) {
        monster.attack_at = now + kMonsterAttackMs;
        state->player.hp = std::max(0, state->player.hp - std::max(1, monster.attack));
        monster.hurt_until = std::max(monster.hurt_until, now + 120U);
        set_status_message(state, uppercase(monster.name) + " HITS FOR " + std::to_string(monster.attack) + ".", now);
        if (state->player.hp <= 0) {
          return_player_to_safety(state, now);
          return;
        }
        save_game(*state);
      }
      continue;
    }

    if (!monster.aggressive || distance > 6 || now < monster.move_at) {
      continue;
    }

    std::array<Direction, 4> choices = {Direction::Up, Direction::Down, Direction::Left, Direction::Right};
    if (std::abs(state->player.tile_x - monster.x) >= std::abs(state->player.tile_y - monster.y)) {
      choices = {
          state->player.tile_x < monster.x ? Direction::Left : Direction::Right,
          state->player.tile_y < monster.y ? Direction::Up : Direction::Down,
          state->player.tile_y < monster.y ? Direction::Down : Direction::Up,
          state->player.tile_x < monster.x ? Direction::Right : Direction::Left,
      };
    } else {
      choices = {
          state->player.tile_y < monster.y ? Direction::Up : Direction::Down,
          state->player.tile_x < monster.x ? Direction::Left : Direction::Right,
          state->player.tile_x < monster.x ? Direction::Right : Direction::Left,
          state->player.tile_y < monster.y ? Direction::Down : Direction::Up,
      };
    }

    bool moved = false;
    for (Direction direction : choices) {
      int dx = 0;
      int dy = 0;
      direction_delta(direction, &dx, &dy);
      const int next_x = monster.x + dx;
      const int next_y = monster.y + dy;
      if (!monster_can_occupy(*state, state->current_area, next_x, next_y, monster.id)) {
        continue;
      }
      monster.x = next_x;
      monster.y = next_y;
      monster.facing = direction;
      moved = true;
      break;
    }

    if (!moved) {
      monster.facing = opposite(monster.facing);
    }
    monster.move_at = now + kMonsterMoveMs;
  }
}

bool npc_is_blinking(const Npc& npc, Uint32 now) {
  const int seed = hash_text(npc.id, npc.x * 31 + npc.y * 17);
  const Uint32 cycle = 4400U + static_cast<Uint32>(seed % 1700);
  const Uint32 phase = now % cycle;
  return phase >= cycle - 140U || (phase >= cycle - 320U && phase < cycle - 280U);
}

Direction ambient_npc_direction(const Npc& npc) {
  const int seed = hash_text(npc.id, npc.ambient_turn_count + npc.x * 13 + npc.y * 7);
  switch (seed % 4) {
    case 0:
      return Direction::Down;
    case 1:
      return Direction::Left;
    case 2:
      return Direction::Right;
    default:
      return Direction::Up;
  }
}

void update_npcs(GameState* state, Uint32 now) {
  Area* area = mutable_area(state, state->current_area);
  if (area == nullptr) {
    return;
  }

  for (auto& npc : area->npcs) {
    if (npc.facing_hold_until != 0 && now < npc.facing_hold_until) {
      continue;
    }
    if (std::abs(state->player.tile_x - npc.x) + std::abs(state->player.tile_y - npc.y) <= 1) {
      continue;
    }
    if (npc.ambient_turn_at == 0) {
      npc.ambient_turn_at = now + 3200U + static_cast<Uint32>(hash_text(npc.id, npc.x * 19 + npc.y * 23) % 2600);
      continue;
    }
    if (now < npc.ambient_turn_at) {
      continue;
    }

    ++npc.ambient_turn_count;
    const int pause_roll = hash_text(npc.id, npc.ambient_turn_count * 37 + 11);
    if (pause_roll % 5 != 0) {
      Direction next = ambient_npc_direction(npc);
      if (next == npc.facing) {
        next = static_cast<Direction>((static_cast<int>(next) % 4) + 1);
      }
      npc.facing = next;
    }
    npc.ambient_turn_at = now + 3400U +
                          static_cast<Uint32>(hash_text(npc.id, npc.ambient_turn_count * 53 + 29) % 3100);
  }
}

void turn_npc_toward(GameState* state,
                     const std::string& area_id,
                     const std::string& npc_id,
                     Direction facing,
                     Uint32 now) {
  if (Npc* npc = mutable_npc(state, area_id, npc_id)) {
    npc->facing = facing;
    npc->facing_hold_until = now + 2400U;
    npc->ambient_turn_at = npc->facing_hold_until + 2200U;
  }
}

std::vector<std::string> dialogue_for_npc(GameState* state, const Npc& npc, Uint32 now) {
  if (npc.id == "father") {
    if (!state->progress.father_counsel) {
      state->progress.father_counsel = true;
      state->profile.coins += 5;
      add_virtue(state, "prudence", 1, now);
      save_game(*state);
      set_status_message(state, "FATHER'S POUCH: +5 COIN.", now);
      return {
          "YOUR FATHER STUDIES YOU ACROSS THE TABLE.",
          "'DO NOT BE RULED BY ANGER. LEARN WHERE POWER TRULY SITS.'",
          "HE PLACES A LEATHER POUCH BY YOUR HAND. YOU ACCEPT FIVE ADDITIONAL PENNIES.",
      };
    }
    return {
        "HE NODS TOWARD THE LETTER. 'STAND WHERE YOU CHOOSE, AND STAND THERE CLEANLY.'",
    };
  }

  if (npc.id == "mother") {
    if (!state->progress.mother_blessing) {
      state->progress.mother_blessing = true;
      state->profile.virtues["faith"] += 1;
      state->profile.virtues["hope"] += 1;
      save_game(*state);
      return {
          "YOUR MOTHER TURNS FROM THE HEARTH AND HOLDS YOU FOR A MOMENT.",
          "'DO NOT LOSE WHAT IS GOOD IN YOU,' SHE SAYS.",
          "SHE TRACES THE CROSS OVER YOU AND WHISPERS, 'CHRIST KEEP YOUR MIND CLEAR.'",
      };
    }
    return {
        "SHE STRAIGHTENS YOUR CLOAK AND TELLS YOU TO KEEP YOUR MIND CLEAR.",
    };
  }

  if (npc.id == "friar") {
    if (!state->progress.friar_blessing) {
      state->progress.friar_blessing = true;
      state->profile.virtues["charity"] += 1;
      state->profile.virtues["fortitude"] += 1;
      save_game(*state);
      return {
          "THE FRANCISCAN GREETS YOU: 'PEACE TO YOU. YOU TRAVEL?'",
          "HE RAISES A HAND AND PRAYS BRIEFLY FOR COURAGE AND RESTRAINT.",
      };
    }
    return {
        "'KEEP YOUR PROMISES, AND KEEP SHORT ACCOUNTS WITH GOD,' THE FRIAR SAYS.",
        "'BLACKPINE WATCHES WHAT MEN DO MORE THAN WHAT THEY CLAIM.'",
    };
  }

  if (npc.id == "driver") {
    return {
        "'MOST MEN CHOOSE THEIR ROAD. OTHERS WAKE TO FIND THE ROAD HAS CHOSEN THEM,' THE DRIVER SAYS.",
        "THE MULE STAMPS AND THE CART CREAKS AGAINST ITS LEATHER STRAPS.",
        "'SAINT CATHERINE? THEN CLIMB INTO THE CART. BRUNE KNOWS THE TURN BETTER THAN I DO.'",
    };
  }

  if (npc.id == "gate-friar") {
    if (!state->progress.gate_opened) {
      state->progress.gate_opened = true;
      save_game(*state);
      return {
          "THE ELDER DOMINICAN COMES TO THE GATE AND READS THE CREST ON YOUR LETTER.",
          "'THE PRIOR WILL SEE YOU,' HE SAYS, OPENING SAINT CATHERINE'S GATE.",
      };
    }
    return {
        "THE GATE FRIAR DIPS HIS HEAD. 'THE HOUSE IS THIN, BUT ITS RULE STILL HOLDS.'",
    };
  }

  if (npc.id == "prior") {
    if (!state->progress.task_board_read) {
      return {
          "WITHIN SAINT CATHERINE: WELL, CLOISTER, CHAPEL DOOR, REFECTORY ARCH, AND THE RHYTHM OF BELLS.",
          "READ THE TASK BOARD IN THE COURTYARD. HERE YOUR LONG LABOR BEGINS.",
      };
    }
    if (!priory_arrival_complete(*state)) {
      return {
          "WALK THE HOUSE AND ITS OBLIGATIONS. SEE BROTHER MARTIN, SAINT HILDA HOSPICE, BLACKPINE VILLAGE GREEN, AND RAVENSCAR QUAY.",
          "A PRIORY HOLDS BY ORDER, BUT IT SERVES BY KNOWING THE PEOPLE AROUND IT.",
      };
    }
    return {
        "GOOD. NOW TAKE A DUTY FROM THE TASK BOARD AND LET SAINT CATHERINE BE KNOWN BY STEADY WORK.",
    };
  }

  if (npc.id == "hospice-prioress") {
    const bool first_meet = !state->progress.met_prioress;
    state->progress.met_prioress = true;
    save_game(*state);
    if (first_meet) {
      return {
          "THE PRIORESS OF SAINT HILDA SAYS: 'WE NEED STEADY SUPPLY, NOT HEROIC VISITS.'",
          "'WEEKLY HERBS AND LAMP OIL WILL EARN MORE TRUST THAN GRAND SPEECHES.'",
      };
    }
    return {
        "CARE IMPROVES ONLY WHEN PROMISES BECOME HABIT. SAINT HILDA REMEMBERS WHO RETURNS.",
    };
  }

  if (npc.id == "brother-martin") {
    const bool first_meet = !state->progress.met_martin;
    state->progress.met_martin = true;
    if (first_meet) {
      if (state->profile.life_path_index == 1) {
        state->profile.virtues["humility"] += 1;
      } else if (state->profile.life_path_index == 2) {
        state->profile.virtues["fortitude"] += 1;
        state->profile.virtues["temperance"] += 1;
      } else {
        state->profile.virtues["charity"] += 1;
      }
    }
    save_game(*state);
    if (first_meet) {
      return {
          "BROTHER MARTIN ASKS WHAT SORT OF DOMINICAN YOU INTEND TO BECOME.",
          martin_vocation_line(*state),
          "AT THE SCRIPTORIUM DESKS, INK AND BEESWAX MINGLE, AND MARGINS CARRY DEBATES IN CRAMPED LATIN HANDS.",
      };
    }
    return {
        "STUDY, PREACHING, AND GUARDING THE ROAD ALL FAIL IF THE SOUL BEHIND THEM TURNS CROOKED.",
        "THE LOCKED SHELVES TO THE SOUTH HOLD PARCHMENT, INK, AND CANDLES RELEASED ONLY UNDER RULE.",
    };
  }

  if (npc.id == "tomas") {
    const bool first_visit = !state->progress.visited_quay;
    state->progress.visited_quay = true;
    save_game(*state);
    if (first_visit) {
      return {
          "RAVENSCAR QUAY REEKS OF WET ROPE, FISH BRINE, AND PINE TAR.",
          "HANSE FACTORS STAND BY SCALES AND SEAL-CASES WHILE PRIESTS AND CLERKS WATCH EVERY LEDGER LINE.",
      };
    }
    return {
        "THE QUAY TEACHES MEN TO COUNT WEIGHT, WIND, AND OBLIGATION TOGETHER.",
    };
  }

  if (npc.id == "steward-helena") {
    return {
        priory_arrival_complete(*state)
            ? "WITH THE CHAPEL IN ORDER, WE CAN THINK ABOUT GATEHOUSE, HOSPICE, WALLS, AND A REAL SCRIPTORIUM ROW."
            : "THE REBUILD LEDGER NAMES SIX PILLARS: CHAPEL, GATEHOUSE, REFECTORY, SCRIPTORIUM, HOSPICE, AND WALLS.",
    };
  }

  if (npc.id == "porter") {
    return {
        state->progress.gate_opened
            ? "THE ROAD IS BUSIER THAN THE PRIORY LIKES TO ADMIT. PETITIONS, CARTS, AND TROUBLES ALL ARRIVE BY THE SAME GATE."
            : "THE GATE IS QUIET. ENJOY IT WHILE YOU CAN.",
    };
  }

  if (npc.id == "novice-paul") {
    return {
        priory_arrival_complete(*state)
            ? "BROTHER MARTIN SAYS EVEN CARRYING WATER COUNTS, IF YOU DO IT WITHOUT GRUMBLING."
            : "I COPIED THE TASK BOARD TITLES ONTO SCRAP. THEY LOOK MORE IMPORTANT IN BIG LETTERS.",
    };
  }

  if (npc.id == "alisoun") {
    return {
        priory_arrival_complete(*state)
            ? "GOOD. WHEN THE BELL HOLDS, PEOPLE ARGUE LESS ABOUT WHETHER SAINT CATHERINE STILL DESERVES THEIR TRADE."
            : "BLACKPINE BRINGS PETITIONS WITH ITS WOOL, ITS BREAD, AND ITS ADVANTAGE. THE PRIORY HAS TO HEAR ALL THREE.",
        "MY STALL SOUTH OF THE FOUNTAIN CARRIES CLOTH TRIM, RIBBONS, AND PRACTICAL MARKET GOODS.",
    };
  }

  if (npc.id == "ysabel") {
    return {
        "YSABEL SPREADS CHARTERS, RENT NOTES, AND GUILD SEALS OVER A PORTABLE DESK.",
        owns_home(*state, "blackpine-cottage")
            ? "YOUR BLACKPINE COTTAGE IS ENTERED CLEANLY IN THE GUILD ROLL. IF YOU KEEP IT WELL, IT WILL KEEP YOUR RECORDS WELL."
            : "A BLACKPINE COTTAGE STANDS JUST BEYOND THE GREEN: WHITEWASHED WALLS, AN ICON CORNER, AND A CHEST FOR HOUSEHOLD RECORDS.",
    };
  }

  if (npc.id == "margery") {
    return {
        "MARGERY DUSTS FLOUR FROM HER APRON. 'THE OVENS START BEFORE PRIME IF BLACKPINE MEANS TO EAT.'",
        "SHE NODS TOWARD THE PRIORY ROAD. 'MONKS, CARTERS, AND HOSPICE SISTERS ALL TAKE THEIR LOAVES HOT, SO I PLAN FOR ALL OF THEM.'",
    };
  }

  if (npc.id == "godric") {
    return {
        "GODRIC HOLDS UP A HALF-SOLED BOOT. 'BLACKPINE MUD, QUAY TAR, AND PRIORY STONE EACH EAT LEATHER DIFFERENTLY.'",
        "HE GRUNTS. 'A TOWN FEELS HEALTHY WHEN MEN REPAIR THINGS BEFORE THEY FAIL.'",
    };
  }

  if (npc.id == "oswin") {
    return {
        priory_arrival_complete(*state)
            ? "ALL CLEAR FROM THE GREEN TO RAVENSCAR. A GOOD BELL MAKES THE WATCH SHORTER."
            : "MILL ROAD IS TENSE. TOO MANY MEN ARE COUNTING SHORTAGES AND TOO FEW ARE COUNTING COST.",
    };
  }

  if (npc.id == "agnes") {
    return {
        "AGNES MILLER TAPS A SACK WITH HER FOOT. 'FLOUR, BRAN, AND TOLL DUST. THAT IS HALF OF BLACKPINE'S ARGUMENT IN ONE BUNDLE.'",
        "SHE LOWERS HER VOICE. 'SAINT HILDA GETS THE CLEANEST MEAL. THE HOSPICE HAS NO USE FOR PRIDE.'",
    };
  }

  if (npc.id == "colm") {
    return {
        "COLM KEEPS ONE HAND ON A DROVER'S STAFF. 'WOOL MOVES SLOWER THAN MEN THINK AND FASTER THAN TAXMEN ADMIT.'",
        "HE GLANCES TOWARD THE QUAY ROAD. 'KEEP THE ROAD OPEN AND THE GREEN STAYS CIVIL.'",
    };
  }

  if (npc.id == "piers") {
    return {
        priory_arrival_complete(*state)
            ? "THE FOUNTAIN SHIVERS WHEN THE BELL RINGS. I TOLD THEM IT WOULD."
            : "THE GREEN FEELS SMALLER WHEN THE CHAPEL IS DARK.",
    };
  }

  if (npc.id == "widow-joan") {
    return {
        "SAINT CATHERINE STOOD HERE BEFORE THIS MARKET WAS STONE. SOME THINGS SHOULD OUTLAST OUR TEMPERS.",
    };
  }

  if (npc.id == "dock-clerk") {
    return {
        "EVERY LEDGER LINE AT RAVENSCAR IS A SMALL ARGUMENT. THE HANSE MEN JUST PREFER INK TO SHOUTING.",
        "THE QUAY TAUGHT US THAT CLOTH, WAX, AND GRAIN ALL CHANGE PRICE BEFORE THE TIDE DOES.",
    };
  }

  if (npc.id == "hanse-factor") {
    return {
        priory_arrival_complete(*state)
            ? "A HOUSE THAT KEEPS ITS OFFICE USUALLY KEEPS ITS ACCOUNTS. THAT IS GOOD FOR TRADE."
            : "ROPE, TAR, SEALS, RATES. THIS QUAY IS HOLY ONLY WHEN SOMEONE FAILS TO CHECK A WEIGHT.",
    };
  }

  if (npc.id == "rowan") {
    return {
        "ROWAN PULLS COIL AFTER COIL OF LINE STRAIGHT. 'SALT EATS ROPE, WIND TESTS IT, AND MEN BLAME THE ROPER LAST.'",
        "HE JERKS HIS CHIN TOWARD RAVENSCAR'S WATER. 'IF THE TIDE TURNS HARD, EVERY GOOD KNOT IN TOWN EARNS ITS KEEP.'",
    };
  }

  if (npc.id == "sister-beatrice") {
    return {
        priory_arrival_complete(*state)
            ? "THE SISTERS HEARD THE BELL EVEN OVER THE FEVER COUGHS. IT HELPED."
            : "THE COTS FILL FAST. HERBS, OIL, AND HANDS DISAPPEAR EVEN FASTER.",
    };
  }

  if (npc.id == "sister-miriam") {
    return {
        "SISTER MIRIAM FOLDS A CLEAN BANDAGE. 'BLACKPINE KEEPS A SMALL INFIRMARY NOW. CART WRECKS, FEVERS, AND QUAY CUTS MAKE IT NECESSARY.'",
        "SHE GLANCES AT THE COTS. 'REST HERE IF YOU MUST, BUT EAT WELL BEFORE YOU LET YOUR STRENGTH RUN OUT AGAIN.'",
    };
  }

  if (npc.id == "apothecary-eda") {
    return {
        "EDA LAYS OUT HERB PACKETS, BROTH JARS, AND SALVES IN TIDY RANKS.",
        "SHE TAPS A seed pouch with one finger. 'BLACKPINE'S sick recover faster when their yards stay planted and their chests stay orderly.'",
    };
  }

  return {"PEACE BE WITH YOU."};
}

ShopId shop_after_npc_dialogue(const GameState& state, const std::string& npc_id) {
  if (npc_id == "alisoun") {
    return ShopId::BlackpineCloth;
  }
  if (npc_id == "brother-martin" && state.progress.met_martin) {
    return ShopId::ScriptoriumSupply;
  }
  if (npc_id == "hospice-prioress" && state.progress.met_prioress) {
    return ShopId::HospiceSupply;
  }
  if (npc_id == "ysabel") {
    return ShopId::PropertyCharter;
  }
  if (npc_id == "apothecary-eda") {
    return ShopId::HospiceSupply;
  }
  return ShopId::None;
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
      const ProgressState previous_progress = state->progress;
      turn_npc_toward(state, state->current_area, npc.id, opposite(state->player.facing), now);
      const std::vector<std::string> pages = dialogue_for_npc(state, npc, now);
      start_dialogue(state, npc.name, pages, shop_after_npc_dialogue(*state, npc.id));
      if ((npc.id == "gate-friar" && !previous_progress.gate_opened && state->progress.gate_opened) ||
          (npc.id == "hospice-prioress" && !previous_progress.met_prioress && state->progress.met_prioress) ||
          (npc.id == "brother-martin" && !previous_progress.met_martin && state->progress.met_martin) ||
          (npc.id == "tomas" && !previous_progress.visited_quay && state->progress.visited_quay)) {
        trigger_bell(audio, 880.0f, 240);
      }
      if (npc.id == "prior" && priory_arrival_complete(*state)) {
        trigger_bell(audio, 523.25f, 520);
      }
      return;
    }
  }

  if (state->current_area == "blackpine-cottage" && tile_at(*area, target_x, target_y) == kBed) {
    advance_day(state, now);
    return;
  }

  if (state->current_area == "bellfield" && target_x == 5 && target_y == 15) {
    if (owns_home(*state, "blackpine-cottage")) {
      apply_warp(state, Warp{5, 15, "blackpine-cottage", 7, 10, Direction::Up}, now);
    } else {
      start_dialogue(
          state,
          "COTTAGE",
          {"A MODEST COTTAGE STANDS JUST OFF THE GREEN. YSABEL THE NOTARY HOLDS ITS CHARTER."});
    }
    return;
  }

  if (area_belongs_to_home(area->id, state->profile.active_home_id) &&
      (tile_at(*area, target_x, target_y) == kChest || decor_placement_at(*state, area->id, target_x, target_y) != nullptr)) {
    const DecorPlacement* placement = decor_placement_at(*state, area->id, target_x, target_y);
    if (placement == nullptr || decor_tile_for_item(placement->item_id) == kChest || tile_at(*area, target_x, target_y) == kChest) {
      state->shop = {};
      state->chest.active = true;
      state->chest.browse_storage = false;
      state->chest.inventory_selection = 0;
      state->chest.storage_selection = 0;
      return;
    }
  }

  if (farm_tile_action(state, *area, target_x, target_y, now)) {
    return;
  }

  if (place_active_decor(state, *area, target_x, target_y, now)) {
    return;
  }

  if (state->current_area == "bellfield" && target_x == 16 && target_y == 8) {
    start_dialogue(
        state,
        "FOUNTAIN",
        {"BLACKPINE'S FOUNTAIN CATCHES MARKET GOSSIP, CHILDREN, AND EVERY PRIORY BELL IN THE SAME BASIN."});
    return;
  }

  if (state->current_area == "house" && target_x == 6 && target_y == 2) {
    start_dialogue(
        state,
        "LETTER",
        {"THE SEALED LETTER BEARS A FADED LION CREST AND THE WORDS: TO SAINT CATHERINE PRIORY."});
    return;
  }

  if (state->current_area == "lane" && target_x == 13 && target_y == 6) {
    start_dialogue(
        state,
        "TRACKS",
        {"BOOT-MARKS AND CART GROOVES OVERLAP. TRAFFIC IS HEAVIER THAN THE PRIORY LIKES TO ADMIT."});
    return;
  }

  if (state->current_area == "square" && target_x == 16 && target_y == 11) {
    start_dialogue(
        state,
        "WELL",
        {"A RING OF VILLAGERS TRADES REPORTS BESIDE THE WELL."});
    return;
  }

  if (state->current_area == "square" && target_x >= 18 && target_x <= 26 && target_y >= 13 && target_y <= 15) {
    start_dialogue(
        state,
        "STALLS",
        {"ROWS OF STALLS: WAX, NAILS, GRAIN, CLOTH, AND MERCHANT CHATTER ABOUT RISK AND WEATHER."});
    return;
  }

  if (state->current_area == "bellfield" && target_y >= 12 && target_y <= 14 &&
      ((target_x >= 10 && target_x <= 13) || (target_x >= 19 && target_x <= 22))) {
    open_shop(state, ShopId::BlackpineCloth);
    return;
  }

  if (state->current_area == "priory-court" && target_x == 14 && target_y == 10) {
    start_dialogue(state, "WELL", {"THE COURTYARD WELL RUNS COLD AND CLEAR. EVERYTHING IN THIS HOUSE STARTS WITH WATER CARRIED BY HAND."});
    return;
  }

  if (state->current_area == "priory-court" && target_x == 7 && target_y == 12) {
    state->progress.task_board_read = true;
    save_game(*state);
    start_dialogue(
        state,
        "TASK BOARD",
        {
            "SAINT CATHERINE TASK BOARD: EACH DUTY STRENGTHENS ONE PILLAR OF THE PRIORY WHILE ANOTHER WAITS.",
            "PREACH IN BLACKPINE AND ROADSIDE CHAPELS, STUDY AND COPY TEXTS, PATROL ROADS, WORK FIELDS AND STORES, OR RUN CHARITY ROUNDS.",
            "THE REBUILD DOCKET BELOW IT NAMES THE GREAT WORKS: CHAPEL, GATEHOUSE, REFECTORY, SCRIPTORIUM, HOSPICE, AND WALLS.",
        });
    return;
  }

  if (state->current_area == "scriptorium" && target_x == 10 && target_y == 2) {
    start_dialogue(
        state,
        "DOCKET",
        {
            "SAINT CATHERINE REBUILD ARC: THE RUINED CLOISTER SURVEY, CLAIM OF THE STONE, REBUILD THE GATEHOUSE, AND THE CHAPEL BEFORE THE HALL.",
            "LATER PAGES SPEAK OF THE WINTER SOUP LINE, THE HIDDEN ROOM, THEFT AT NIGHT, AND THE CONSECRATION OF THE RESTORED CHAPEL.",
        });
    return;
  }

  if (state->current_area == "scriptorium" && target_x >= 10 && target_x <= 11 && target_y >= 6 && target_y <= 8) {
    open_shop(state, ShopId::ScriptoriumSupply);
    return;
  }

  if (state->current_area == "chapel" && target_x == 6 && target_y == 2) {
    if (state->progress.chapel_prayer_index == 0) {
      state->progress.chapel_prayer_index = 1;
      state->profile.virtues["humility"] += 1;
      state->profile.virtues["faith"] += 1;
      save_game(*state);
      start_dialogue(state, "CHAPEL RAIL",
                     {"YOU ASK FOR CLEAR SIGHT BEFORE CLEAR SPEECH.", "HUMILITY +1. FAITH +1."});
    } else if (state->progress.chapel_prayer_index == 1) {
      state->progress.chapel_prayer_index = 2;
      state->profile.virtues["fortitude"] += 1;
      state->profile.virtues["faith"] += 1;
      save_game(*state);
      start_dialogue(state, "CHAPEL RAIL",
                     {"YOU ASK FOR COURAGE WITHOUT HARDNESS.", "FORTITUDE +1. FAITH +1."});
    } else if (state->progress.chapel_prayer_index == 2) {
      state->progress.chapel_prayer_index = 3;
      state->profile.virtues["temperance"] += 1;
      state->profile.virtues["charity"] += 1;
      state->profile.virtues["faith"] += 1;
      save_game(*state);
      start_dialogue(state, "CHAPEL RAIL",
                     {"YOU ASK FOR PATIENCE WHEN OFFENDED.", "TEMPERANCE +1. CHARITY +1. FAITH +1."});
    } else {
      start_dialogue(
          state,
          "ALTAR",
          {
              "THE ALTAR CLOTH IS REPAIRED IN THREE PLACES. POVERTY, YES, BUT NOT NEGLECT.",
          });
    }
    return;
  }

  if (state->current_area == "candlewharf" && target_x == 13 && target_y == 13) {
    start_dialogue(
        state,
        "CRATES",
        {
            "ROPE, TAR, WEIGHTS, AND SEAL-CASES. RAVENSCAR'S COMMERCE IS MOST HOLY WHEN SOMEONE ELSE IS CHECKING IT.",
        });
    return;
  }

  if (state->current_area == "saint-hilda-hospice" && target_x == 12 && target_y == 10) {
    open_shop(state, ShopId::HospiceSupply);
    return;
  }
}

void advance_dialogue(GameState* state) {
  if (!state->dialogue.active) {
    return;
  }
  if (state->dialogue.page_index + 1 < state->dialogue.pages.size()) {
    ++state->dialogue.page_index;
  } else {
    const ShopId shop_after = state->dialogue.shop_after;
    state->dialogue = {};
    if (shop_after != ShopId::None) {
      open_shop(state, shop_after);
    }
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
      const SDL_Color dark = {59, 113, 61, 255};
      const SDL_Color light = {141, 194, 123, 255};
      fill_rect(renderer, rect, base);
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{76, 131, 72, 255});
      draw_pixel(renderer, screen_x + 2 + (noise & 1), screen_y + 4, light);
      draw_pixel(renderer, screen_x + 5, screen_y + 10, dark);
      draw_pixel(renderer, screen_x + 8, screen_y + 11, dark);
      draw_pixel(renderer, screen_x + 11, screen_y + 6 + ((noise >> 2) & 1), light);
      draw_pixel(renderer, screen_x + 13, screen_y + 9, dark);
      draw_pixel(renderer, screen_x + 4, screen_y + 13, SDL_Color{154, 203, 133, 255});
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
      const SDL_Color dark = {149, 120, 76, 255};
      const SDL_Color light = {219, 194, 146, 255};
      fill_rect(renderer, rect, base);
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{172, 142, 97, 255});
      draw_pixel(renderer, screen_x + 3, screen_y + 4, light);
      draw_pixel(renderer, screen_x + 11, screen_y + 4, dark);
      draw_pixel(renderer, screen_x + 5, screen_y + 8, dark);
      draw_pixel(renderer, screen_x + 8, screen_y + 10, light);
      draw_pixel(renderer, screen_x + 13, screen_y + 12, light);
      draw_pixel(renderer, screen_x + 1, screen_y + 14, dark);
      break;
    }
    case kStone:
      fill_rect(renderer, rect, SDL_Color{188, 181, 166, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{168, 160, 145, 255});
      for (int py = 4; py <= 12; py += 4) {
        fill_rect(renderer, SDL_Rect{screen_x, screen_y + py, kTileSize, 1}, SDL_Color{145, 138, 124, 255});
      }
      for (int px = 4; px <= 12; px += 4) {
        fill_rect(renderer, SDL_Rect{screen_x + px, screen_y, 1, kTileSize}, SDL_Color{158, 150, 135, 255});
      }
      draw_pixel(renderer, screen_x + 8, screen_y + 3, SDL_Color{108, 132, 93, 255});
      draw_pixel(renderer, screen_x + 3, screen_y + 11, SDL_Color{145, 138, 124, 255});
      break;
    case kWater:
      fill_rect(renderer, rect, SDL_Color{64, 123, 189, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{45, 95, 154, 255});
      for (int row = 3; row <= 12; row += 4) {
        const int wave = (static_cast<int>(now / 150U) + tile_x + row) % 4;
        fill_rect(renderer, SDL_Rect{screen_x + 2 + wave, screen_y + row, 7, 1},
                  SDL_Color{122, 188, 235, 255});
        fill_rect(renderer, SDL_Rect{screen_x + 9 - wave / 2, screen_y + row + 1, 5, 1},
                  SDL_Color{43, 86, 148, 255});
      }
      draw_pixel(renderer, screen_x + 11, screen_y + 5, SDL_Color{191, 230, 248, 255});
      break;
    case kTree:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 8}, SDL_Color{50, 96, 52, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 3, 10, 6}, SDL_Color{87, 141, 80, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 4, 8, 3}, SDL_Color{116, 171, 104, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 10, 2, 4}, SDL_Color{110, 74, 45, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 5, SDL_Color{136, 184, 118, 255});
      break;
    case kRoof:
      fill_rect(renderer, rect, SDL_Color{145, 74, 72, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y, kTileSize, 2}, SDL_Color{177, 96, 93, 255});
      for (int row = 1; row < kTileSize; row += 4) {
        fill_rect(renderer, SDL_Rect{screen_x, screen_y + row, kTileSize, 1},
                  SDL_Color{170, 95, 92, 255});
      }
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 13, kTileSize, 3},
                SDL_Color{104, 50, 48, 255});
      draw_pixel(renderer, screen_x + 8, screen_y + 4, SDL_Color{196, 118, 112, 255});
      break;
    case kWall:
      fill_rect(renderer, rect, SDL_Color{220, 210, 188, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4},
                SDL_Color{184, 171, 145, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 5, 10, 1}, SDL_Color{201, 190, 168, 255});
      draw_pixel(renderer, screen_x + 4, screen_y + 9, SDL_Color{170, 159, 136, 255});
      draw_pixel(renderer, screen_x + 11, screen_y + 7, SDL_Color{170, 159, 136, 255});
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
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 3, 6, 1}, SDL_Color{148, 112, 71, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 7, SDL_Color{219, 188, 124, 255});
      break;
    case kWood:
      fill_rect(renderer, rect, SDL_Color{132, 96, 61, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{102, 73, 47, 255});
      for (int x = 0; x < kTileSize; x += 4) {
        fill_rect(renderer, SDL_Rect{screen_x + x, screen_y, 1, kTileSize}, SDL_Color{90, 62, 39, 255});
      }
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 7, kTileSize, 1}, SDL_Color{168, 126, 82, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 4, SDL_Color{186, 145, 96, 255});
      break;
    case kFloor:
      fill_rect(renderer, rect, SDL_Color{168, 134, 101, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{143, 112, 82, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 7, kTileSize, 1}, SDL_Color{132, 102, 77, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y, 1, kTileSize}, SDL_Color{132, 102, 77, 255});
      draw_pixel(renderer, screen_x + 3, screen_y + 4, SDL_Color{188, 156, 123, 255});
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
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 4, 12, 1}, SDL_Color{182, 139, 92, 255});
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
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{74, 86, 52, 255});
      draw_pixel(renderer, screen_x + 4, screen_y + 5, SDL_Color{131, 175, 86, 255});
      draw_pixel(renderer, screen_x + 8, screen_y + 9, SDL_Color{168, 212, 110, 255});
      draw_pixel(renderer, screen_x + 11, screen_y + 4, SDL_Color{131, 175, 86, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 12, SDL_Color{122, 150, 80, 255});
      break;
    case kFountain:
      fill_rect(renderer, rect, SDL_Color{170, 170, 177, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 1, 14, 14}, SDL_Color{134, 134, 142, 255});
      fill_circle(renderer, screen_x + 8, screen_y + 8, 5, SDL_Color{104, 159, 216, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 3, SDL_Color{245, 245, 255, 255}, 2);
      break;
    case kStall:
      fill_rect(renderer, rect, SDL_Color{123, 86, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 3, 14, 3}, SDL_Color{191, 98, 74, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 4, 14, 1}, SDL_Color{235, 207, 175, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 8, 14, 6}, SDL_Color{224, 210, 161, 255});
      draw_pixel(renderer, screen_x + 5, screen_y + 10, SDL_Color{180, 134, 78, 255});
      draw_pixel(renderer, screen_x + 10, screen_y + 11, SDL_Color{191, 98, 74, 255});
      break;
    case kHerb:
      fill_rect(renderer, rect, SDL_Color{109, 130, 80, 255});
      fill_rect(renderer, SDL_Rect{screen_x, screen_y + 12, kTileSize, 4}, SDL_Color{80, 93, 58, 255});
      draw_pixel(renderer, screen_x + 4, screen_y + 4, SDL_Color{173, 211, 112, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 7, SDL_Color{131, 175, 86, 255});
      draw_pixel(renderer, screen_x + 10, screen_y + 5, SDL_Color{173, 211, 112, 255});
      draw_pixel(renderer, screen_x + 12, screen_y + 9, SDL_Color{131, 175, 86, 255});
      break;
    case kCrate:
      fill_rect(renderer, rect, SDL_Color{121, 88, 52, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 1, 14, 14}, SDL_Color{85, 59, 37, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 1, 1, 14}, SDL_Color{154, 118, 72, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + 1, 1, 14}, SDL_Color{154, 118, 72, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 7, 14, 1}, SDL_Color{154, 118, 72, 255});
      break;
    case kChest:
      fill_rect(renderer, rect, SDL_Color{88, 58, 37, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 4, 12, 9}, SDL_Color{140, 95, 55, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 4, 12, 9}, SDL_Color{64, 42, 28, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 7, 12, 1}, SDL_Color{198, 160, 98, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 7, 2, 4}, SDL_Color{223, 193, 124, 255});
      break;
    case kBed:
      fill_rect(renderer, rect, SDL_Color{132, 96, 74, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 2, 14, 11}, SDL_Color{210, 198, 186, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 6, 12, 7}, SDL_Color{142, 92, 88, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 3, 5, 3}, SDL_Color{239, 232, 220, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 2, 14, 11}, SDL_Color{96, 68, 50, 255});
      break;
    case kBoard:
      draw_tile(renderer, kStone, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 2, 8, 9}, SDL_Color{124, 84, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 3, 6, 7}, SDL_Color{218, 202, 164, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 5, 6, 1}, SDL_Color{141, 121, 95, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 11, 2, 4}, SDL_Color{88, 60, 41, 255});
      break;
    case kRug:
      fill_rect(renderer, rect, SDL_Color{127, 63, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 12}, SDL_Color{169, 88, 69, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 12}, SDL_Color{217, 191, 126, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 7, SDL_Color{232, 211, 149, 255}, 2);
      break;
    case kReed:
      draw_tile(renderer, kWater, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 5, 1, 8}, SDL_Color{184, 197, 102, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 3, 1, 9}, SDL_Color{209, 215, 127, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 10, screen_y + 6, 1, 7}, SDL_Color{184, 197, 102, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 12, screen_y + 4, 1, 8}, SDL_Color{209, 215, 127, 255});
      break;
    case kFence:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 3, 2, 11}, SDL_Color{112, 78, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 12, screen_y + 3, 2, 11}, SDL_Color{112, 78, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 6, 14, 2}, SDL_Color{163, 121, 77, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 10, 14, 2}, SDL_Color{147, 108, 69, 255});
      draw_pixel(renderer, screen_x + 3, screen_y + 4, SDL_Color{197, 161, 109, 255});
      draw_pixel(renderer, screen_x + 12, screen_y + 4, SDL_Color{197, 161, 109, 255});
      break;
    case kBench:
      draw_tile(renderer, kStone, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 5, 10, 2}, SDL_Color{154, 112, 70, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 8, 12, 2}, SDL_Color{123, 84, 51, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 10, 2, 4}, SDL_Color{83, 56, 36, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 10, screen_y + 10, 2, 4}, SDL_Color{83, 56, 36, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 6, SDL_Color{194, 156, 102, 255});
      break;
    case kBarrel:
      draw_tile(renderer, kStone, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 3, 8, 10}, SDL_Color{138, 96, 56, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 2, 6, 2}, SDL_Color{174, 130, 83, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 12, 6, 2}, SDL_Color{109, 73, 43, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 5, 8, 1}, SDL_Color{94, 64, 39, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 9, 8, 1}, SDL_Color{94, 64, 39, 255});
      draw_pixel(renderer, screen_x + 8, screen_y + 7, SDL_Color{208, 171, 114, 255});
      break;
    case kLaundry:
      draw_tile(renderer, kGrass, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 2, 1, 12}, SDL_Color{118, 84, 50, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 14, screen_y + 2, 1, 12}, SDL_Color{118, 84, 50, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 3, 14, 1}, SDL_Color{87, 59, 38, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 4, 4, 6}, SDL_Color{228, 216, 190, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 8, screen_y + 4, 4, 5}, SDL_Color{151, 95, 80, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 12, screen_y + 4, 2, 6}, SDL_Color{83, 121, 143, 255});
      draw_pixel(renderer, screen_x + 4, screen_y + 5, SDL_Color{246, 236, 211, 255});
      draw_pixel(renderer, screen_x + 9, screen_y + 5, SDL_Color{193, 132, 111, 255});
      break;
    case kCartRail:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 5, 14, 7}, SDL_Color{122, 84, 49, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 5, 14, 2}, SDL_Color{171, 126, 77, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 7, 1, 5}, SDL_Color{83, 56, 36, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 13, screen_y + 7, 1, 5}, SDL_Color{83, 56, 36, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 8, 8, 1}, SDL_Color{202, 166, 108, 255});
      break;
    case kCartStep:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 4, 14, 8}, SDL_Color{134, 95, 57, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 5, 12, 2}, SDL_Color{178, 132, 82, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 8, 12, 1}, SDL_Color{96, 66, 41, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 10, 8, 3}, SDL_Color{163, 121, 73, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + 5, 2, 7}, SDL_Color{89, 60, 39, 255});
      draw_pixel(renderer, screen_x + 5, screen_y + 11, SDL_Color{212, 182, 122, 255});
      break;
    case kCartWheel:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 4, 14, 4}, SDL_Color{115, 80, 48, 255});
      fill_circle(renderer, screen_x + 8, screen_y + 11, 4, SDL_Color{90, 62, 39, 255});
      fill_circle(renderer, screen_x + 8, screen_y + 11, 2, SDL_Color{164, 122, 74, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 7, 2, 8}, SDL_Color{187, 149, 95, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 10, 8, 1}, SDL_Color{187, 149, 95, 255});
      break;
    case kHarness:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 0, screen_y + 8, 16, 2}, SDL_Color{104, 79, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 6, 10, 1}, SDL_Color{180, 149, 92, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + 7, 2, 4}, SDL_Color{91, 64, 40, 255});
      draw_pixel(renderer, screen_x + 6, screen_y + 7, SDL_Color{212, 180, 114, 255});
      break;
    case kMuleBack:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 5, 11, 6}, SDL_Color{122, 96, 76, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 4, 6, 3}, SDL_Color{94, 72, 58, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 6, 4, 3}, SDL_Color{170, 128, 83, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 11, 1, 4}, SDL_Color{73, 53, 36, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 10, screen_y + 11, 1, 4}, SDL_Color{73, 53, 36, 255});
      break;
    case kMuleFore:
      draw_tile(renderer, kPath, tile_x, tile_y, screen_x, screen_y, now);
      fill_rect(renderer, SDL_Rect{screen_x + 0, screen_y + 5, 8, 6}, SDL_Color{122, 96, 76, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 6, screen_y + 6, 6, 3}, SDL_Color{112, 84, 63, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 10, screen_y + 4, 3, 3}, SDL_Color{94, 72, 58, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + 3, 1, 2}, SDL_Color{94, 72, 58, 255});
      draw_pixel(renderer, screen_x + 12, screen_y + 6, SDL_Color{42, 34, 29, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 11, 1, 4}, SDL_Color{73, 53, 36, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 11, 1, 4}, SDL_Color{73, 53, 36, 255});
      break;
    default:
      fill_rect(renderer, rect, SDL_Color{255, 0, 255, 255});
      break;
  }
}

void draw_objective_tokens(SDL_Renderer* renderer, const GameState& state) {
  const SDL_Rect panel = {146, 8, 102, 20};
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

  if (!state.progress.gate_opened) {
    draw_token(151, "H", state.current_area != "house");
    draw_token(172, "S", opening_square_reached(state));
    draw_token(193, "F", state.progress.friar_blessing);
    draw_token(214, "G", state.progress.gate_opened);
    return;
  }

  draw_token(151, "T", state.progress.task_board_read);
  draw_token(172, "M", state.progress.met_martin);
  draw_token(193, "H", state.progress.met_prioress);
  draw_token(214, "Q", state.progress.visited_quay);
}

void render_crop_plot(SDL_Renderer* renderer, const CropPlot& plot, int screen_x, int screen_y) {
  fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 2, 14, 12}, SDL_Color{120, 86, 56, 255});
  fill_rect(renderer, SDL_Rect{screen_x + 1, screen_y + 12, 14, 2}, SDL_Color{92, 62, 40, 255});
  fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 4, 1, 8}, SDL_Color{146, 108, 76, 255});
  fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 4, 1, 8}, SDL_Color{146, 108, 76, 255});
  fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + 4, 1, 8}, SDL_Color{146, 108, 76, 255});

  if (plot.crop_id.empty()) {
    return;
  }

  const CropDefinition* crop = crop_definition(plot.crop_id);
  if (crop == nullptr) {
    return;
  }

  const bool mature = plot.stage >= crop->days_to_grow;
  const SDL_Color leaf = mature ? crop->mature : crop->sprout;
  const SDL_Color stem = SDL_Color{74, 116, 53, 255};
  const int plant_top = mature ? 4 : 7;
  fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + plant_top, 1, 6}, stem);
  fill_rect(renderer, SDL_Rect{screen_x + 8, screen_y + plant_top - 1, 1, 7}, stem);
  fill_rect(renderer, SDL_Rect{screen_x + 12, screen_y + plant_top, 1, 6}, stem);
  fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + plant_top + 2, 3, 2}, leaf);
  fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + plant_top + 1, 3, 2}, leaf);
  fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + plant_top + 2, 3, 2}, leaf);
  if (mature) {
    draw_pixel(renderer, screen_x + 4, screen_y + plant_top, SDL_Color{226, 222, 142, 255});
    draw_pixel(renderer, screen_x + 8, screen_y + plant_top - 1, SDL_Color{226, 222, 142, 255});
    draw_pixel(renderer, screen_x + 12, screen_y + plant_top, SDL_Color{226, 222, 142, 255});
  }
}

void render_decor_overlay(SDL_Renderer* renderer, char tile, int screen_x, int screen_y, Uint32 now) {
  switch (tile) {
    case kBench:
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 7, 10, 2}, SDL_Color{154, 112, 70, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 10, 12, 2}, SDL_Color{123, 84, 51, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 12, 2, 2}, SDL_Color{83, 56, 36, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 10, screen_y + 12, 2, 2}, SDL_Color{83, 56, 36, 255});
      break;
    case kBoard:
      fill_rect(renderer, SDL_Rect{screen_x + 4, screen_y + 2, 8, 8}, SDL_Color{124, 84, 54, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 3, 6, 6}, SDL_Color{218, 202, 164, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 5, 6, 1}, SDL_Color{141, 121, 95, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 10, 2, 4}, SDL_Color{88, 60, 41, 255});
      break;
    case kLaundry:
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 2}, SDL_Color{122, 89, 64, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 4, 4, 7}, SDL_Color{221, 214, 198, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 4, 3, 6}, SDL_Color{181, 112, 94, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 11, screen_y + 4, 3, 7}, SDL_Color{196, 177, 118, 255});
      break;
    case kChest:
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 6, 12, 7}, SDL_Color{140, 95, 55, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 6, 12, 7}, SDL_Color{64, 42, 28, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 8, 12, 1}, SDL_Color{198, 160, 98, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 8, 2, 3}, SDL_Color{223, 193, 124, 255});
      break;
    case kDesk:
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 5, 12, 7}, SDL_Color{153, 108, 65, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 5, 12, 1}, SDL_Color{182, 139, 92, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 3, screen_y + 12, 1, 2}, SDL_Color{95, 68, 42, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 12, screen_y + 12, 1, 2}, SDL_Color{95, 68, 42, 255});
      break;
    case kRug:
      fill_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 12}, SDL_Color{169, 88, 69, 255});
      draw_rect(renderer, SDL_Rect{screen_x + 2, screen_y + 2, 12, 12}, SDL_Color{217, 191, 126, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 7, SDL_Color{232, 211, 149, 255}, 2);
      break;
    case kCandle:
      fill_rect(renderer, SDL_Rect{screen_x + 5, screen_y + 6, 6, 6}, SDL_Color{104, 82, 66, 255});
      fill_rect(renderer, SDL_Rect{screen_x + 7, screen_y + 3, 2, 6}, SDL_Color{242, 229, 202, 255});
      draw_pixel(renderer, screen_x + 7, screen_y + 1 + static_cast<int>((now / 140U) % 2U), SDL_Color{255, 209, 96, 255}, 2);
      break;
    default:
      draw_tile(renderer, tile, 0, 0, screen_x, screen_y, now);
      break;
  }
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

  for (const auto& plot : state.crop_plots) {
    if (plot.area_id != area->id || !plot.tilled) {
      continue;
    }
    render_crop_plot(renderer, plot, plot.x * kTileSize - camera_x, plot.y * kTileSize - camera_y);
  }

  for (const auto& placement : state.decor_placements) {
    if (placement.area_id != area->id) {
      continue;
    }
    const char tile = decor_tile_for_item(placement.item_id);
    if (tile == '\0') {
      continue;
    }
    render_decor_overlay(renderer, tile, placement.x * kTileSize - camera_x, placement.y * kTileSize - camera_y, now);
  }

  struct Drawable {
    int sort_y;
    int screen_x;
    int screen_y;
    SpriteRole role;
    Direction facing;
    bool walking;
    bool blink = false;
    bool is_player = false;
    const Monster* monster = nullptr;
  };

  std::vector<Drawable> drawables;
  for (const auto& npc : area->npcs) {
    drawables.push_back({npc.y * kTileSize,
                         npc.x * kTileSize - camera_x,
                         npc.y * kTileSize - camera_y,
                         npc.role,
                         npc.facing,
                         false,
                         npc_is_blinking(npc, now),
                         false,
                         nullptr});
  }
  for (const auto& monster : area->monsters) {
    if (monster.hp <= 0) {
      continue;
    }
    drawables.push_back({monster.y * kTileSize,
                         monster.x * kTileSize - camera_x,
                         monster.y * kTileSize - camera_y,
                         SpriteRole::Player,
                         monster.facing,
                         false,
                         false,
                         false,
                         &monster});
  }
  drawables.push_back({static_cast<int>(player_y), static_cast<int>(player_x) - camera_x,
                       static_cast<int>(player_y) - camera_y, SpriteRole::Player, state.player.facing,
                       state.player.moving, false, true, nullptr});

  std::sort(drawables.begin(), drawables.end(), [](const Drawable& left, const Drawable& right) {
    return left.sort_y < right.sort_y;
  });

  for (const auto& drawable : drawables) {
    if (drawable.monster != nullptr) {
      render_monster(renderer, *drawable.monster, drawable.screen_x, drawable.screen_y, now);
      continue;
    }
    if (drawable.is_player) {
      render_character_with_palette(renderer, player_palette(state), drawable.role, drawable.facing, drawable.walking,
                                    drawable.screen_x, drawable.screen_y, now, drawable.blink);
      render_player_swing(renderer, state, drawable.screen_x, drawable.screen_y, now);
    } else {
      render_character(renderer, drawable.role, drawable.facing, drawable.walking, drawable.screen_x,
                       drawable.screen_y, now, drawable.blink);
    }
  }

  for (const auto& monster : area->monsters) {
    if (monster.hp <= 0) {
      continue;
    }
    const int distance = manhattan_distance(monster.x, monster.y, state.player.tile_x, state.player.tile_y);
    if (distance <= 2 || monster.hp < monster.max_hp || now < monster.hurt_until + 300U) {
      render_monster_hp_bar(renderer, monster, monster.x * kTileSize - camera_x, monster.y * kTileSize - camera_y);
    }
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
  render_player_hp_panel(renderer, state);
}

void render_area_banner(SDL_Renderer* renderer, const GameState& state, Uint32 now) {
  if (now >= state.area_banner_until) {
    return;
  }
  const SDL_Rect plaque = {36, 10, 184, 24};
  const SDL_Rect text_box = {plaque.x + 6, plaque.y + 3, plaque.w - 12, plaque.h - 6};
  fill_rect(renderer, plaque, SDL_Color{239, 232, 214, 245});
  draw_rect(renderer, plaque, SDL_Color{117, 103, 74, 255});
  draw_text_box(renderer, state.area_banner, text_box,
                TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{64, 78, 56, 255});
}

void render_status_message(SDL_Renderer* renderer, const GameState& state, Uint32 now) {
  if (state.status_message.empty() || now >= state.status_until) {
    return;
  }
  const SDL_Rect plaque = {44, 36, 168, 18};
  const SDL_Rect text_box = {plaque.x + 6, plaque.y + 2, plaque.w - 12, plaque.h - 4};
  fill_rect(renderer, plaque, SDL_Color{247, 239, 214, 230});
  draw_rect(renderer, plaque, SDL_Color{122, 110, 84, 255});
  draw_text_box(renderer, state.status_message, text_box,
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{72, 89, 58, 255});
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
  draw_text_box(renderer, state.dialogue.speaker,
                SDL_Rect{22, kScreenHeight - kDialogHeight - 5, 84, 7},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                SDL_Color{58, 56, 41, 255});

  if (state.dialogue.page_index < state.dialogue.pages.size()) {
    draw_text_layout(renderer, state.dialogue.pages[state.dialogue.page_index],
                     SDL_Rect{box.x + 10, box.y + 10, box.w - 20, kDialogBodyHeight},
                     TextBoxOptions{1, 1, true, true, 0, 2, TextAlign::Left, TextVerticalAlign::Top},
                     SDL_Color{42, 46, 52, 255});
  }
  const bool has_next = state.dialogue.page_index + 1 < state.dialogue.pages.size();
  draw_text(renderer, has_next ? "A NEXT" : "A CLOSE", box.x + box.w - 46, box.y + box.h - 15, 1,
            SDL_Color{95, 86, 67, 255});
  const std::string page_counter =
      std::to_string(state.dialogue.page_index + 1) + "/" + std::to_string(state.dialogue.pages.size());
  draw_text(renderer, page_counter, box.x + 10, box.y + box.h - 15, 1, SDL_Color{95, 86, 67, 255});
}

void render_journal(SDL_Renderer* renderer, const GameState& state) {
  if (!state.journal_open) {
    return;
  }
  fill_rect(renderer, SDL_Rect{20, 18, 216, 188}, SDL_Color{242, 236, 220, 250});
  draw_rect(renderer, SDL_Rect{20, 18, 216, 188}, SDL_Color{86, 79, 60, 255});
  static const std::array<const char*, 6> kPages = {
      "SAINT CATHERINE LEDGER", "PILGRIM PROFILE", "PACK & STORES", "WARDROBE", "VIRTUES", "HOLDINGS"};
  const int page = std::clamp(state.profile.journal_page, 0, static_cast<int>(kPages.size()) - 1);
  draw_text_box(renderer, kPages[static_cast<std::size_t>(page)], SDL_Rect{26, 24, 204, 16},
                TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{72, 84, 58, 255});

  if (page == 0) {
    draw_text(renderer, "OBJECTIVE", 32, 52, 1, SDL_Color{86, 79, 60, 255});

    const SDL_Rect objective_box = {32, 64, 184, 44};
    const TextBoxOptions objective_options = {
        2, 1, true, false, 0, 4, TextAlign::Left, TextVerticalAlign::Top};
    const TextPageLayout objective_layout = layout_text_box_single(objective_text(state), objective_box, objective_options);
    draw_text_layout(renderer, objective_layout, objective_box, objective_options, SDL_Color{42, 46, 52, 255});
    const int objective_height =
        static_cast<int>(objective_layout.lines.size()) * text_height(objective_layout.scale) +
        std::max(0, static_cast<int>(objective_layout.lines.size()) - 1) * objective_options.line_gap;
    const int y = objective_box.y + objective_height + 4;

    const int tracker_y = std::max(120, y + 6);
    if (!state.progress.gate_opened) {
      draw_text_box(renderer, "ARRIVAL TO SAINT CATHERINE", SDL_Rect{32, tracker_y, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    SDL_Color{86, 79, 60, 255});
      draw_text_box(renderer, state.current_area == "house" ? "FATHER'S HOUSE NOT YET LEFT" : "LEFT FATHER'S HOUSE",
                    SDL_Rect{32, tracker_y + 12, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    state.current_area == "house" ? SDL_Color{112, 88, 72, 255} : SDL_Color{62, 103, 57, 255});
      draw_text_box(renderer, opening_square_reached(state) ? "BLACKPINE SQUARE REACHED" : "BLACKPINE SQUARE AHEAD",
                    SDL_Rect{32, tracker_y + 25, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    opening_square_reached(state) ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
      draw_text_box(renderer, state.progress.friar_blessing ? "FRIAR'S BLESSING RECEIVED" : "FRANCISCAN FRIAR UNMET",
                    SDL_Rect{32, tracker_y + 38, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    state.progress.friar_blessing ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
      draw_text_box(renderer, state.progress.gate_opened ? "PRIORY GATE OPENED" : "PRIORY GATE CLOSED",
                    SDL_Rect{32, tracker_y + 51, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    state.progress.gate_opened ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
    } else {
      draw_text_box(renderer, "THE LONG LABOR", SDL_Rect{32, tracker_y, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    SDL_Color{86, 79, 60, 255});
      draw_text_box(renderer, state.progress.task_board_read ? "TASK BOARD READ" : "TASK BOARD UNREAD",
                    SDL_Rect{32, tracker_y + 12, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    state.progress.task_board_read ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
      draw_text_box(renderer, state.progress.met_martin ? "BROTHER MARTIN MET" : "BROTHER MARTIN UNMET",
                    SDL_Rect{32, tracker_y + 25, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    state.progress.met_martin ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
      draw_text_box(renderer, state.progress.met_prioress ? "SAINT HILDA VISITED" : "SAINT HILDA AHEAD",
                    SDL_Rect{32, tracker_y + 38, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    state.progress.met_prioress ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
      draw_text_box(renderer, village_green_reached(state) ? "VILLAGE GREEN WALKED" : "VILLAGE GREEN AHEAD",
                    SDL_Rect{32, tracker_y + 51, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    village_green_reached(state) ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
      draw_text_box(renderer, state.progress.visited_quay ? "RAVENSCAR QUAY WALKED" : "RAVENSCAR QUAY AHEAD",
                    SDL_Rect{32, tracker_y + 64, 184, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    state.progress.visited_quay ? SDL_Color{62, 103, 57, 255} : SDL_Color{112, 88, 72, 255});
    }
  } else if (page == 1) {
    const LifePathDefinition& life_path = current_life_path(state);
    draw_text(renderer, "LIFEPATH", 32, 52, 1, SDL_Color{86, 79, 60, 255});
    draw_text_box(renderer, life_path.name, SDL_Rect{32, 64, 184, 14},
                  TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{42, 46, 52, 255});
    draw_text_box(renderer, life_path.description, SDL_Rect{32, 82, 184, 20},
                  TextBoxOptions{1, 1, true, false, 2, 2, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{72, 72, 68, 255});
    draw_text(renderer, "REFLECTION", 32, 108, 1, SDL_Color{86, 79, 60, 255});
    draw_text_box(renderer, life_path.reflection_text[static_cast<std::size_t>(std::clamp(state.profile.reflection_index, 0, 1))],
                  SDL_Rect{32, 120, 184, 18},
                  TextBoxOptions{1, 1, true, false, 2, 2, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{42, 46, 52, 255});
    draw_text_box(renderer, "COIN: " + coin_text(state.profile.coins), SDL_Rect{32, 146, 184, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{62, 103, 57, 255});
    draw_text_box(renderer, "GARMENT: " + equipped_outfit_name(state), SDL_Rect{32, 160, 184, 16},
                  TextBoxOptions{1, 1, true, false, 2, 2, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{42, 46, 52, 255});
    draw_text_box(renderer, "ORDERS: FRANCISCANS, CARMELITES, SAINT HILDA, AND THE BENEDICTINE GRANGE ALL PRESS INTO BLACKPINE'S STORY.",
                  SDL_Rect{32, 178, 184, 18},
                  TextBoxOptions{1, 1, true, false, 2, 2, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{72, 72, 68, 255});
  } else if (page == 2) {
    draw_text(renderer, "PACK", 32, 52, 1, SDL_Color{86, 79, 60, 255});
    draw_text_box(renderer, "DAY " + std::to_string(state.profile.day), SDL_Rect{32, 64, 54, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{62, 103, 57, 255});
    draw_text_box(renderer, "READY: " + uppercase(active_item_label(state)), SDL_Rect{90, 64, 126, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Right, TextVerticalAlign::Top},
                  SDL_Color{72, 72, 68, 255});

    const std::vector<std::string> pack_ids = sorted_item_ids(state.profile.inventory);
    if (pack_ids.empty()) {
      draw_text_box(renderer, "NO PACK ITEMS YET.", SDL_Rect{32, 84, 184, 10},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    SDL_Color{112, 88, 72, 255});
    } else {
      const int selection = std::clamp(state.profile.pack_selection, 0, static_cast<int>(pack_ids.size()) - 1);
      const int visible_count = 6;
      const int start = std::clamp(selection - 2, 0, std::max(0, static_cast<int>(pack_ids.size()) - visible_count));
      int y = 82;
      for (int row = 0; row < visible_count && start + row < static_cast<int>(pack_ids.size()); ++row) {
        const std::string& item_id = pack_ids[static_cast<std::size_t>(start + row)];
        const bool selected = start + row == selection;
        const std::string label =
            inventory_name(item_id) + " X" + std::to_string(inventory_count(state, item_id));
        fill_rect(renderer, SDL_Rect{30, y - 2, 188, 10},
                  selected ? SDL_Color{228, 220, 192, 255} : SDL_Color{0, 0, 0, 0});
        draw_text_box(renderer, label, SDL_Rect{32, y, 184, 8},
                      TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                      selected ? SDL_Color{62, 91, 54, 255} : SDL_Color{42, 46, 52, 255});
        y += 12;
      }

      if (const InventoryItemDefinition* definition =
              inventory_definition(pack_ids[static_cast<std::size_t>(selection)])) {
        std::string action = "CARRY";
        if (definition->heal_amount > 0) {
          action = "EAT FOR +" + std::to_string(definition->heal_amount) + " HP";
        } else if (!definition->crop_id.empty()) {
          action = "READY TO SOW";
        } else if (definition->decor_tile != '\0') {
          action = "READY TO PLACE";
        }
        draw_text_box(renderer, action, SDL_Rect{32, 158, 184, 8},
                      TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                      SDL_Color{62, 103, 57, 255});
        draw_text_box(renderer, definition->description, SDL_Rect{32, 170, 184, 20},
                      TextBoxOptions{1, 1, true, false, 3, 2, TextAlign::Left, TextVerticalAlign::Top},
                      SDL_Color{72, 72, 68, 255});
      }
    }
  } else if (page == 3) {
    draw_text(renderer, "EQUIPPED GARMENT", 32, 52, 1, SDL_Color{86, 79, 60, 255});
    fill_rect(renderer, SDL_Rect{154, 62, 52, 72}, SDL_Color{233, 225, 205, 255});
    draw_rect(renderer, SDL_Rect{154, 62, 52, 72}, SDL_Color{122, 110, 84, 255});
    render_character_with_palette(renderer, player_palette(state), SpriteRole::Player, Direction::Down, false, 172, 90, 0);

    int y = 68;
    for (std::size_t index = 0; index < state.profile.unlocked_outfits.size() && index < 7; ++index) {
      const bool selected = static_cast<int>(index) == state.profile.wardrobe_selection;
      const bool equipped = state.profile.unlocked_outfits[index] == state.profile.equipped_outfit_id;
      const OutfitDefinition* outfit = outfit_definition(state.profile.unlocked_outfits[index]);
      const std::string label = std::string(equipped ? "* " : "  ") + (outfit != nullptr ? outfit->name : "UNKNOWN");
      fill_rect(renderer, SDL_Rect{30, y - 2, 114, 10},
                selected ? SDL_Color{228, 220, 192, 255} : SDL_Color{242, 236, 220, 0});
      draw_text_box(renderer, label, SDL_Rect{32, y, 110, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    selected ? SDL_Color{62, 91, 54, 255} : SDL_Color{42, 46, 52, 255});
      y += 12;
    }
    if (state.profile.wardrobe_selection >= 0 &&
        state.profile.wardrobe_selection < static_cast<int>(state.profile.unlocked_outfits.size())) {
      if (const OutfitDefinition* outfit =
              outfit_definition(state.profile.unlocked_outfits[static_cast<std::size_t>(state.profile.wardrobe_selection)])) {
        draw_text_box(renderer, outfit->description, SDL_Rect{32, 152, 184, 24},
                      TextBoxOptions{1, 1, true, false, 3, 2, TextAlign::Left, TextVerticalAlign::Top},
                      SDL_Color{72, 72, 68, 255});
      }
    }
  } else if (page == 4) {
    static const std::array<std::string, 6> kVirtueKeys = {
        "fortitude", "temperance", "faith", "hope", "charity", "humility"};
    int y = 56;
    for (const auto& key : kVirtueKeys) {
      fill_rect(renderer, SDL_Rect{32, y - 2, 184, 18}, SDL_Color{236, 229, 211, 255});
      draw_rect(renderer, SDL_Rect{32, y - 2, 184, 18}, SDL_Color{122, 110, 84, 255});
      draw_text_box(renderer, virtue_label(key), SDL_Rect{38, y + 1, 68, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    SDL_Color{86, 79, 60, 255});
      const int value = virtue_value(state, key);
      draw_text_box(renderer, std::to_string(value), SDL_Rect{108, y + 1, 20, 8},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Right, TextVerticalAlign::Top},
                    SDL_Color{62, 91, 54, 255});
      fill_rect(renderer, SDL_Rect{136, y + 2, 70, 6}, SDL_Color{208, 201, 186, 255});
      const int bars = std::clamp(value, 0, 10);
      fill_rect(renderer, SDL_Rect{136, y + 2, bars * 7, 6},
                SDL_Color{122, 154, 77, 255});
      y += 24;
    }
    draw_text_box(renderer,
                  "PRUDENCE IS COUNTED AS HUMILITY, AND JUSTICE AS CHARITY, ACCORDING TO THE TEXT GAME'S CANONICAL VIRTUE LEDGER.",
                  SDL_Rect{32, 176, 184, 18},
                  TextBoxOptions{1, 1, true, false, 2, 2, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{72, 72, 68, 255});
  } else {
    draw_text(renderer, "ACTIVE HOME", 32, 52, 1, SDL_Color{86, 79, 60, 255});
    const std::string active_home = state.profile.active_home_id.empty() ? "NONE" : home_name(state.profile.active_home_id);
    draw_text_box(renderer, active_home, SDL_Rect{32, 64, 184, 14},
                  TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{42, 46, 52, 255});
    draw_text_box(renderer,
                  owns_home(state, "blackpine-cottage")
                      ? "YOUR BLACKPINE COTTAGE STANDS JUST OFF THE GREEN. ITS CHEST CAN HOLD HOUSEHOLD RECORDS AND TRAVEL SUPPLIES."
                      : "YSABEL THE NOTARY HANDLES PROPERTY CHARTERS FOR THE HOUSES AROUND BLACKPINE GREEN.",
                  SDL_Rect{32, 84, 184, 30},
                  TextBoxOptions{1, 1, true, false, 3, 2, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{72, 72, 68, 255});
    draw_text(renderer, "OWNED HOUSES", 32, 122, 1, SDL_Color{86, 79, 60, 255});
    if (state.profile.owned_homes.empty()) {
      draw_text_box(renderer, "NO CHARTERS YET.", SDL_Rect{32, 134, 184, 10},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                    SDL_Color{112, 88, 72, 255});
    } else {
      int y = 134;
      for (std::size_t index = 0; index < state.profile.owned_homes.size() && index < 4; ++index) {
        draw_text_box(renderer, home_name(state.profile.owned_homes[index]), SDL_Rect{32, y, 184, 10},
                      TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                      SDL_Color{42, 46, 52, 255});
        y += 12;
      }
    }
    draw_text_box(renderer, "HOUSE LOT: " + std::string(owns_home(state, "blackpine-cottage") ? "TILLABLE YARD OPEN" : "BUY LAND TO FARM"),
                  SDL_Rect{32, 170, 184, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{62, 91, 54, 255});
    draw_text_box(renderer, "CHEST ITEMS: " + std::to_string(static_cast<int>(sorted_item_ids(state.profile.stored_items).size())),
                  SDL_Rect{32, 182, 184, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{62, 91, 54, 255});
  }

  draw_text_box(renderer, "LEFT RIGHT PAGES", SDL_Rect{30, 189, 96, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});
  draw_text_box(renderer,
                page == 2 ? "A USE  UP DOWN SELECT"
                          : (page == 3 ? "A EQUIP  B CLOSE" : "START OR B TO CLOSE"),
                SDL_Rect{120, 189, 98, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Right, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});
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
  draw_text_box(renderer, "SAINT CATHERINE", SDL_Rect{34, 56, 188, 14},
                TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{70, 84, 62, 255});
  draw_text_box(renderer, "THE BLACKPINE CHAPTER", SDL_Rect{34, 72, 188, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{70, 84, 62, 255});

  const std::vector<std::string> options =
      state.has_save ? std::vector<std::string>{"CONTINUE", "NEW CHAPTER"}
                     : std::vector<std::string>{"BEGIN REBUILD"};
  for (std::size_t index = 0; index < options.size(); ++index) {
    const bool selected = static_cast<int>(index) == state.title_selection;
    fill_rect(renderer, SDL_Rect{66, 124 + static_cast<int>(index) * 22, 124, 18},
              selected ? SDL_Color{246, 239, 221, 255} : SDL_Color{194, 184, 157, 220});
    draw_rect(renderer, SDL_Rect{66, 124 + static_cast<int>(index) * 22, 124, 18},
              SDL_Color{96, 83, 61, 255});
    draw_text_box(renderer, options[index], SDL_Rect{72, 129 + static_cast<int>(index) * 22, 112, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Top},
                  selected ? SDL_Color{59, 87, 54, 255} : SDL_Color{83, 74, 54, 255});
  }

  draw_text(renderer, "A TO SELECT", 128, 202, 1, SDL_Color{245, 236, 215, 255}, true);
}

void render_creation(SDL_Renderer* renderer, const GameState& state) {
  const LifePathDefinition& life_path = current_life_path(state);
  GameState preview = state;
  preview.profile.equipped_outfit_id = life_path.starter_outfit_id;

  fill_rect(renderer, SDL_Rect{0, 0, kScreenWidth, kScreenHeight}, SDL_Color{115, 161, 181, 255});
  fill_rect(renderer, SDL_Rect{16, 14, 224, 196}, SDL_Color{243, 237, 221, 250});
  draw_rect(renderer, SDL_Rect{16, 14, 224, 196}, SDL_Color{86, 79, 60, 255});
  draw_text_box(renderer, "NEW CHAPTER", SDL_Rect{28, 22, 200, 16},
                TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{72, 84, 58, 255});

  const std::array<std::string, 5> labels = {"LIFEPATH", "REFLECTION", "COMPLEXION", "HAIR", "BEGIN CHAPTER"};
  const std::array<std::string, 5> values = {
      life_path.name,
      life_path.reflection_text[static_cast<std::size_t>(std::clamp(state.profile.reflection_index, 0, 1))],
      current_complexion(state).name,
      current_hair(state).name,
      "",
  };

  int y = 52;
  for (std::size_t index = 0; index < labels.size(); ++index) {
    const bool selected = static_cast<int>(index) == state.creation.row;
    fill_rect(renderer, SDL_Rect{28, y - 4, 124, 22}, selected ? SDL_Color{226, 217, 186, 255}
                                                                : SDL_Color{236, 229, 211, 255});
    draw_rect(renderer, SDL_Rect{28, y - 4, 124, 22}, SDL_Color{122, 110, 84, 255});
    draw_text_box(renderer, labels[index], SDL_Rect{34, y - 1, 112, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{86, 79, 60, 255});
    draw_text_box(renderer, values[index], SDL_Rect{34, y + 8, 112, 12},
                  TextBoxOptions{1, 1, true, false, 2, 1, TextAlign::Left, TextVerticalAlign::Top},
                  selected ? SDL_Color{62, 91, 54, 255} : SDL_Color{42, 46, 52, 255});
    y += 28;
  }

  fill_rect(renderer, SDL_Rect{164, 50, 54, 80}, SDL_Color{233, 225, 205, 255});
  draw_rect(renderer, SDL_Rect{164, 50, 54, 80}, SDL_Color{122, 110, 84, 255});
  render_character_with_palette(renderer, player_palette(preview), SpriteRole::Player, Direction::Down, false, 183, 80, 0);
  draw_text_box(renderer, coin_text((life_path.coin_min + life_path.coin_max) / 2), SDL_Rect{170, 118, 42, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Top},
                SDL_Color{62, 103, 57, 255});

  draw_text_box(renderer, life_path.description, SDL_Rect{164, 138, 56, 28},
                TextBoxOptions{1, 1, true, false, 4, 2, TextAlign::Left, TextVerticalAlign::Top},
                SDL_Color{72, 72, 68, 255});
  draw_text_box(renderer, "STARTER: " + inventory_name_list(life_path.starter_items), SDL_Rect{28, 188, 196, 14},
                TextBoxOptions{1, 1, true, false, 2, 1, TextAlign::Left, TextVerticalAlign::Top},
                SDL_Color{72, 72, 68, 255});
}

void render_shop(SDL_Renderer* renderer, const GameState& state) {
  if (!state.shop.active) {
    return;
  }
  const auto& offers = shop_offers(state.shop.id);
  fill_rect(renderer, SDL_Rect{18, 16, 220, 192}, SDL_Color{247, 241, 226, 250});
  draw_rect(renderer, SDL_Rect{18, 16, 220, 192}, SDL_Color{86, 79, 60, 255});
  draw_text_box(renderer, shop_title(state.shop.id), SDL_Rect{26, 22, 204, 16},
                TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{72, 84, 58, 255});
  draw_text_box(renderer, shop_description(state.shop.id), SDL_Rect{28, 40, 180, 18},
                TextBoxOptions{1, 1, true, false, 2, 1, TextAlign::Left, TextVerticalAlign::Top},
                SDL_Color{72, 72, 68, 255});
  draw_text_box(renderer, "COIN " + coin_text(state.profile.coins), SDL_Rect{186, 42, 42, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Right, TextVerticalAlign::Top},
                SDL_Color{62, 103, 57, 255});

  const int visible_count = 6;
  const int selection = offers.empty() ? 0 : std::clamp(state.shop.selection, 0, static_cast<int>(offers.size()) - 1);
  const int start = std::clamp(selection - 2, 0, std::max(0, static_cast<int>(offers.size()) - visible_count));
  int y = 66;
  for (int row = 0; row < visible_count && start + row < static_cast<int>(offers.size()); ++row) {
    const ShopOffer& offer = offers[static_cast<std::size_t>(start + row)];
    const bool selected = start + row == selection;
    const bool owned = shop_offer_owned(state, offer);
    fill_rect(renderer, SDL_Rect{28, y - 3, 200, 14}, selected ? SDL_Color{228, 220, 192, 255}
                                                                : SDL_Color{243, 237, 221, 0});
    draw_text_box(renderer, offer.label + (owned ? " OWNED" : ""), SDL_Rect{32, y, 192, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  selected ? SDL_Color{62, 91, 54, 255} : SDL_Color{42, 46, 52, 255});
    y += 16;
  }

  if (!offers.empty()) {
    const ShopOffer& offer = offers[static_cast<std::size_t>(selection)];
    draw_text_box(renderer, offer.description, SDL_Rect{32, 154, 192, 24},
                  TextBoxOptions{1, 1, true, false, 3, 2, TextAlign::Left, TextVerticalAlign::Top},
                  SDL_Color{72, 72, 68, 255});
  }

  draw_text_box(renderer, "A BUY OR EQUIP", SDL_Rect{32, 186, 72, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});
  draw_text_box(renderer, "B CLOSE", SDL_Rect{154, 186, 70, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Right, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});
}

void render_chest(SDL_Renderer* renderer, const GameState& state) {
  if (!state.chest.active) {
    return;
  }

  fill_rect(renderer, SDL_Rect{16, 18, 224, 188}, SDL_Color{245, 239, 225, 250});
  draw_rect(renderer, SDL_Rect{16, 18, 224, 188}, SDL_Color{86, 79, 60, 255});
  draw_text_box(renderer, "HOUSEHOLD CHEST", SDL_Rect{24, 24, 208, 16},
                TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                SDL_Color{72, 84, 58, 255});
  draw_text_box(renderer, home_name(state.profile.active_home_id.empty() ? "blackpine-cottage" : state.profile.active_home_id),
                SDL_Rect{24, 40, 208, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});

  const SDL_Rect pack_box = {28, 58, 92, 112};
  const SDL_Rect chest_box = {136, 58, 92, 112};
  fill_rect(renderer, pack_box, state.chest.browse_storage ? SDL_Color{238, 232, 217, 255} : SDL_Color{227, 219, 189, 255});
  fill_rect(renderer, chest_box, state.chest.browse_storage ? SDL_Color{227, 219, 189, 255} : SDL_Color{238, 232, 217, 255});
  draw_rect(renderer, pack_box, SDL_Color{122, 110, 84, 255});
  draw_rect(renderer, chest_box, SDL_Color{122, 110, 84, 255});
  draw_text(renderer, "PACK", pack_box.x + pack_box.w / 2, pack_box.y - 10, 1, SDL_Color{86, 79, 60, 255}, true);
  draw_text(renderer, "CHEST", chest_box.x + chest_box.w / 2, chest_box.y - 10, 1, SDL_Color{86, 79, 60, 255}, true);

  const std::vector<std::string> pack_lines = inventory_lines(state);
  const std::vector<std::string> chest_lines = stored_lines(state);
  int y = pack_box.y + 6;
  for (std::size_t index = 0; index < pack_lines.size() && index < 8; ++index) {
    const bool selected = !state.chest.browse_storage && static_cast<int>(index) == state.chest.inventory_selection;
    fill_rect(renderer, SDL_Rect{pack_box.x + 3, y - 2, pack_box.w - 6, 10},
              selected ? SDL_Color{220, 211, 181, 255} : SDL_Color{0, 0, 0, 0});
    draw_text_box(renderer, pack_lines[index], SDL_Rect{pack_box.x + 6, y, pack_box.w - 12, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  selected ? SDL_Color{62, 91, 54, 255} : SDL_Color{42, 46, 52, 255});
    y += 12;
  }

  y = chest_box.y + 6;
  for (std::size_t index = 0; index < chest_lines.size() && index < 8; ++index) {
    const bool selected = state.chest.browse_storage && static_cast<int>(index) == state.chest.storage_selection;
    fill_rect(renderer, SDL_Rect{chest_box.x + 3, y - 2, chest_box.w - 6, 10},
              selected ? SDL_Color{220, 211, 181, 255} : SDL_Color{0, 0, 0, 0});
    draw_text_box(renderer, chest_lines[index], SDL_Rect{chest_box.x + 6, y, chest_box.w - 12, 8},
                  TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                  selected ? SDL_Color{62, 91, 54, 255} : SDL_Color{42, 46, 52, 255});
    y += 12;
  }

  draw_text_box(renderer, state.chest.browse_storage ? "A RETRIEVE" : "A STORE", SDL_Rect{28, 182, 78, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});
  draw_text_box(renderer, "LEFT RIGHT SWITCH", SDL_Rect{84, 182, 88, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});
  draw_text_box(renderer, "B CLOSE", SDL_Rect{168, 182, 60, 8},
                TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Right, TextVerticalAlign::Top},
                SDL_Color{86, 79, 60, 255});
}

bool travel_to_area(GameState* state, const std::string& area_id);

void render_frame(SDL_Renderer* renderer, const GameState& state, Uint32 now) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  if (state.on_title) {
    render_title(renderer, state, now);
  } else if (state.creation.active) {
    render_creation(renderer, state);
  } else {
    render_world(renderer, state, now);
    render_area_banner(renderer, state, now);
    render_status_message(renderer, state, now);
    render_journal(renderer, state);
    render_dialogue(renderer, state);
    render_shop(renderer, state);
    render_chest(renderer, state);
    if (!state.dialogue.active && !state.journal_open && !state.shop.active && !state.chest.active) {
      draw_text_box(renderer, "A ACT  B SWING  SELECT RUN  START LEDGER", SDL_Rect{20, 204, 216, 10},
                    TextBoxOptions{1, 1, false, false, 1, 0, TextAlign::Center, TextVerticalAlign::Middle},
                    SDL_Color{248, 239, 214, 220});
    }
  }
}

bool save_renderer_bmp(SDL_Renderer* renderer, const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenWidth, kScreenHeight, 32, SDL_PIXELFORMAT_ARGB8888);
  if (surface == nullptr) {
    std::fprintf(stderr, "SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError());
    return false;
  }

  if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) != 0) {
    std::fprintf(stderr, "SDL_RenderReadPixels failed: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return false;
  }

  const std::string path_string = path.string();
  const int save_result = SDL_SaveBMP(surface, path_string.c_str());
  SDL_FreeSurface(surface);
  if (save_result != 0) {
    std::fprintf(stderr, "SDL_SaveBMP failed for %s: %s\n", path_string.c_str(), SDL_GetError());
    return false;
  }
  return true;
}

bool capture_preview_scene(SDL_Renderer* renderer,
                           const GameState& state,
                           Uint32 now,
                           const std::filesystem::path& path) {
  render_frame(renderer, state, now);
  SDL_RenderPresent(renderer);
  return save_renderer_bmp(renderer, path);
}

bool capture_previews(SDL_Renderer* renderer, pp_context* context) {
  const std::filesystem::path output_dir = std::filesystem::current_path() / "build" / "priory-previews";

  GameState title_state;
  title_state.context = context;
  title_state.world = build_world();
  title_state.on_title = true;
  title_state.has_save = true;
  title_state.title_selection = 1;
  if (!capture_preview_scene(renderer, title_state, 1200, output_dir / "title.bmp")) {
    return false;
  }

  GameState creation_state;
  creation_state.context = context;
  creation_state.world = build_world();
  creation_state.on_title = false;
  creation_state.creation.active = true;
  creation_state.creation.row = 2;
  creation_state.profile.life_path_index = 3;
  creation_state.profile.reflection_index = 1;
  creation_state.profile.complexion_index = 2;
  creation_state.profile.hair_index = 1;
  if (!capture_preview_scene(renderer, creation_state, 0, output_dir / "creation.bmp")) {
    return false;
  }

  GameState world_state;
  world_state.context = context;
  world_state.world = build_world();
  world_state.profile.life_path_index = 3;
  world_state.profile.reflection_index = 1;
  world_state.profile.complexion_index = 2;
  world_state.profile.hair_index = 1;
  start_new_game(&world_state, 0);
  while (world_state.dialogue.active) {
    advance_dialogue(&world_state);
  }
  world_state.current_area = "lane";
  world_state.player.tile_x = 8;
  world_state.player.tile_y = 8;
  world_state.player.start_x = 8;
  world_state.player.start_y = 8;
  world_state.player.target_x = 8;
  world_state.player.target_y = 8;
  world_state.player.moving = false;
  world_state.player.facing = Direction::Right;
  set_area_banner(&world_state, "BLACKPINE LANE", 0);
  world_state.area_banner_until = 2200;
  if (!capture_preview_scene(renderer, world_state, 400, output_dir / "lane-cart.bmp")) {
    return false;
  }
  if (!travel_to_area(&world_state, "priory-court")) {
    std::fprintf(stderr, "Preview capture failed: could not route to Saint Catherine courtyard.\n");
    return false;
  }
  world_state.area_banner_until = 2200;
  if (!capture_preview_scene(renderer, world_state, 400, output_dir / "courtyard.bmp")) {
    return false;
  }

  GameState shop_state = world_state;
  if (!travel_to_area(&shop_state, "bellfield")) {
    std::fprintf(stderr, "Preview capture failed: could not route to Blackpine.\n");
    return false;
  }
  open_shop(&shop_state, ShopId::BlackpineCloth);
  shop_state.shop.selection = 1;
  if (!capture_preview_scene(renderer, shop_state, 650, output_dir / "blackpine-shop.bmp")) {
    return false;
  }

  GameState dialogue_state = world_state;
  if (!travel_to_area(&dialogue_state, "chapel")) {
    std::fprintf(stderr, "Preview capture failed: could not route to chapel.\n");
    return false;
  }
  start_dialogue(
      &dialogue_state,
      "THE PRIOR",
      {"THE REBUILD LEDGER NAMES SIX PILLARS: CHAPEL, GATEHOUSE, REFECTORY, SCRIPTORIUM, HOSPICE, AND WALLS.",
       "IF THIS HOUSE IS TO FEEL REAL AGAIN, EVERY ROAD MUST LEAD TO SERVICE, TRADE, STUDY, AND PRAYER."});
  if (!capture_preview_scene(renderer, dialogue_state, 900, output_dir / "chapel-dialogue.bmp")) {
    return false;
  }

  std::printf("PRIORY PREVIEWS CAPTURED: %s\n", output_dir.string().c_str());
  return true;
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
  if (tile_locked_by_progress(state, area.id, x, y)) {
    return false;
  }
  return !tile_occupied_by_npc(state, area.id, x, y) && !tile_occupied_by_monster(state, area.id, x, y) &&
         !tile_blocked_by_decor(state, area.id, x, y);
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

bool move_to_tile(GameState* state, const std::string& area_id, int x, int y, bool allow_area_change = false) {
  if (state->current_area != area_id) {
    return false;
  }
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
  if (allow_area_change && state->current_area != area_id) {
    return true;
  }
  return state->current_area == area_id && state->player.tile_x == x && state->player.tile_y == y;
}

bool move_to_destination(GameState* state, const std::string& area_id, int x, int y) {
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

bool validate_outdoor_art(const GameState& state) {
  for (const auto& area : state.world) {
    if (area.indoor) {
      continue;
    }
    for (int y = 0; y < static_cast<int>(area.tiles.size()); ++y) {
      for (int x = 0; x < static_cast<int>(area.tiles[y].size()); ++x) {
        const char tile = area.tiles[y][x];
        if (tile != kDoor && tile != kWindow && tile != kGlass) {
          continue;
        }
        if (y <= 0 || !is_facade_support_tile(tile_at(area, x, y - 1))) {
          std::fprintf(stderr,
                       "Art audit failed: unsupported facade tile '%c' in %s at %d,%d.\n",
                       tile, area.id.c_str(), x, y);
          return false;
        }
        if (tile == kDoor && tile_at(area, x, y + 1) != kDoor && !is_threshold_tile(tile_at(area, x, y + 1))) {
          std::fprintf(stderr,
                       "Art audit failed: detached exterior door in %s at %d,%d.\n",
                       area.id.c_str(), x, y);
          return false;
        }
      }
    }
  }
  return true;
}

bool travel_to_area(GameState* state, const std::string& area_id) {
  if (state->current_area == area_id) {
    return true;
  }

  if (area_id == "house") {
    return move_to_destination(state, "house", 8, 8);
  }
  if (area_id == "lane") {
    return move_to_destination(state, "lane", 12, 8);
  }
  if (area_id == "square") {
    return move_to_destination(state, "square", 16, 11);
  }
  if (area_id == "priory-road") {
    return move_to_destination(state, "priory-road", 14, 8);
  }
  if (area_id == "priory-gate") {
    return move_to_destination(state, "priory-gate", 10, 9);
  }
  if (area_id == "priory-court") {
    return move_to_destination(state, "priory-court", 13, 12);
  }
  if (area_id == "bellfield") {
    return move_to_destination(state, "bellfield", 14, 8);
  }
  if (area_id == "candlewharf") {
    return move_to_destination(state, "candlewharf", 14, 9);
  }
  if (area_id == "scriptorium") {
    return move_to_destination(state, "scriptorium", 6, 10);
  }
  if (area_id == "chapel") {
    return move_to_destination(state, "chapel", 7, 11);
  }
  if (area_id == "saint-hilda-hospice") {
    return move_to_destination(state, "saint-hilda-hospice", 7, 12);
  }
  if (area_id == "blackpine-cottage") {
    if (!owns_home(*state, "blackpine-cottage")) {
      return false;
    }
    if (!move_to_destination(state, "bellfield", 5, 16)) {
      return false;
    }
    AudioState audio;
    state->player.facing = Direction::Up;
    interact(state, 0, &audio);
    return state->current_area == "blackpine-cottage";
  }
  if (area_id == "blackpine-cottage-yard") {
    if (!travel_to_area(state, "blackpine-cottage")) {
      return false;
    }
    if (!move_to_tile(state, "blackpine-cottage", 7, 1)) {
      return false;
    }
    return step_player_instant(state, Direction::Up, 0) && state->current_area == "blackpine-cottage-yard";
  }
  if (area_id == "blackpine-hospital") {
    return move_to_destination(state, "blackpine-hospital", 8, 11);
  }
  return false;
}

bool talk_to_npc(GameState* state, const std::string& area_id, const std::string& npc_id, AudioState* audio) {
  const Npc* npc = find_npc(*state, area_id, npc_id);
  if (npc == nullptr) {
    return false;
  }
  if (!travel_to_area(state, area_id)) {
    if (state->smoke_test) {
      std::fprintf(stderr, "talk_to_npc failed to travel to %s from %s\n",
                   area_id.c_str(), state->current_area.c_str());
    }
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

bool interact_with_tile(GameState* state, const std::string& area_id, int target_x, int target_y, AudioState* audio) {
  if (!travel_to_area(state, area_id)) {
    return false;
  }

  static const std::array<std::pair<int, int>, 4> kOffsets = {{{0, 1}, {0, -1}, {1, 0}, {-1, 0}}};
  for (const auto& offset : kOffsets) {
    const int tile_x = target_x + offset.first;
    const int tile_y = target_y + offset.second;
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
    return state->shop.active || state->dialogue.active;
  }
  return false;
}

bool run_smoke_test(GameState* state) {
  AudioState audio;
  if (!validate_outdoor_art(*state)) {
    return false;
  }
  Npc animation_probe;
  animation_probe.id = "gate-friar";
  animation_probe.x = 12;
  animation_probe.y = 9;
  if (!npc_is_blinking(animation_probe, 6000U) && !npc_is_blinking(animation_probe, 12000U)) {
    std::fprintf(stderr, "Smoke test failed: NPC blink timing never triggered in sample windows.\n");
    return false;
  }
  animation_probe.ambient_turn_count = 1;
  if (ambient_npc_direction(animation_probe) == Direction::None) {
    std::fprintf(stderr, "Smoke test failed: NPC ambient direction resolved to none.\n");
    return false;
  }
  const TextPageLayout fitted_label = layout_text_box_single(
      "BLACKPINE", SDL_Rect{0, 0, 60, 8},
      TextBoxOptions{2, 1, false, false, 1, 0, TextAlign::Left, TextVerticalAlign::Top});
  if (fitted_label.scale != 1) {
    std::fprintf(stderr, "Smoke test failed: text box system did not shrink label to fit.\n");
    return false;
  }
  const TextPageLayout long_word = layout_text_box_single(
      "SUPERSCRIPTORIUMLEDGER", SDL_Rect{0, 0, 60, 18},
      TextBoxOptions{1, 1, true, false, 0, 2, TextAlign::Left, TextVerticalAlign::Top});
  for (const auto& line : long_word.lines) {
    if (text_width(line, long_word.scale) > 60) {
      std::fprintf(stderr, "Smoke test failed: long word still overflowed text box.\n");
      return false;
    }
  }

  state->profile.life_path_index = 3;
  state->profile.reflection_index = 1;
  state->profile.complexion_index = 2;
  state->profile.hair_index = 1;
  start_new_game(state, 0);
  if (state->current_area != "house" || state->profile.coins != 11 || inventory_count(*state, "ledger_scraps") != 1 ||
      inventory_count(*state, "sealed_letter") != 1 || state->profile.equipped_outfit_id != "merchant_doublet" ||
      virtue_value(*state, "humility") != 1 || virtue_value(*state, "fortitude") != 1 || virtue_value(*state, "faith") != 1) {
    std::fprintf(stderr, "Smoke test failed: life path setup did not seed merchant profile.\n");
    return false;
  }
  while (state->dialogue.active) {
    advance_dialogue(state);
  }

  if (!talk_to_npc(state, "house", "father", &audio) || !state->progress.father_counsel || state->profile.coins != 16) {
    std::fprintf(stderr, "Smoke test failed: father counsel did not grant the pouch.\n");
    return false;
  }
  if (!talk_to_npc(state, "house", "mother", &audio) || !state->progress.mother_blessing) {
    std::fprintf(stderr, "Smoke test failed: mother blessing did not register.\n");
    return false;
  }
  if (virtue_value(*state, "hope") < 1 || virtue_value(*state, "faith") < 2) {
    std::fprintf(stderr, "Smoke test failed: mother blessing did not adjust the theological virtues.\n");
    return false;
  }
  if (!move_to_tile(state, "house", 7, 11, true) || state->current_area != "lane") {
    std::fprintf(stderr, "Smoke test failed: house door did not warp to Blackpine lane.\n");
    return false;
  }
  if (const Area* lane = area_for(*state, "lane")) {
    if (tile_at(*lane, 4, 7) == kDoor) {
      std::fprintf(stderr, "Smoke test failed: stray exterior house door still blocks the lane frontage.\n");
      return false;
    }
    if (!tile_passable(tile_at(*lane, 17, 8))) {
      std::fprintf(stderr, "Smoke test failed: cart boarding tile is not passable.\n");
      return false;
    }
  }
  if (const Area* square = area_for(*state, "square")) {
    if (tile_at(*square, 6, 9) == kDoor) {
      std::fprintf(stderr, "Smoke test failed: Blackpine square house still has a detached extra door tile.\n");
      return false;
    }
  }
  if (!talk_to_npc(state, "lane", "driver", &audio)) {
    std::fprintf(stderr, "Smoke test failed: lane driver interaction was unreachable.\n");
    return false;
  }
  if (!move_to_tile(state, "lane", 17, 8, true) || state->current_area != "square") {
    std::fprintf(stderr, "Smoke test failed: cart boarding tile did not warp to Blackpine square.\n");
    return false;
  }
  if (!opening_square_reached(*state) || !state->progress.visited_blackpine) {
    std::fprintf(stderr, "Smoke test failed: arrival at Blackpine square was not tracked.\n");
    return false;
  }
  if (!talk_to_npc(state, "square", "margery", &audio)) {
    std::fprintf(stderr, "Smoke test failed: Blackpine baker was unreachable after market densification.\n");
    return false;
  }
  if (!talk_to_npc(state, "square", "friar", &audio) || !state->progress.friar_blessing) {
    std::fprintf(stderr, "Smoke test failed: friar blessing did not register.\n");
    return false;
  }
  if (virtue_value(*state, "charity") < 1 || virtue_value(*state, "fortitude") < 2) {
    std::fprintf(stderr, "Smoke test failed: friar blessing did not adjust virtues.\n");
    return false;
  }
  GameState square_exit_probe = *state;
  if (!move_to_tile(&square_exit_probe, "square", 18, 0, true) || square_exit_probe.current_area != "priory-road") {
    std::fprintf(stderr, "Smoke test failed: right edge of square north road did not warp to priory road.\n");
    return false;
  }
  if (!move_to_tile(state, "square", 13, 0, true) || state->current_area != "priory-road") {
    std::fprintf(stderr, "Smoke test failed: left edge of square north road did not warp to priory road.\n");
    return false;
  }
  GameState combat_probe = *state;
  if (!move_to_tile(&combat_probe, "priory-road", 7, 8)) {
    std::fprintf(stderr, "Smoke test failed: could not route to the road ratling for combat.\n");
    return false;
  }
  combat_probe.player.facing = Direction::Right;
  const Monster* road_ratling = monster_at(combat_probe, "priory-road", 8, 8);
  if (road_ratling == nullptr) {
    std::fprintf(stderr, "Smoke test failed: priory road ratling was not seeded.\n");
    return false;
  }
  if (!player_attack(&combat_probe, 100U)) {
    std::fprintf(stderr, "Smoke test failed: player swing did not connect with the road ratling.\n");
    return false;
  }
  road_ratling = monster_at(combat_probe, "priory-road", 8, 8);
  if (road_ratling == nullptr || road_ratling->hp >= road_ratling->max_hp) {
    std::fprintf(stderr, "Smoke test failed: player swing did not reduce ratling hp.\n");
    return false;
  }
  const int hp_before_counter = combat_probe.player.hp;
  update_monsters(&combat_probe, 1000U);
  if (combat_probe.player.hp >= hp_before_counter) {
    std::fprintf(stderr, "Smoke test failed: adjacent monster did not counterattack.\n");
    return false;
  }
  player_attack(&combat_probe, 1400U);
  if (monster_at(combat_probe, "priory-road", 8, 8) != nullptr) {
    std::fprintf(stderr, "Smoke test failed: second sword swing did not finish the road ratling.\n");
    return false;
  }
  GameState gate_entry_probe = *state;
  if (!move_to_tile(&gate_entry_probe, "priory-road", 16, 0, true) || gate_entry_probe.current_area != "priory-gate") {
    std::fprintf(stderr, "Smoke test failed: right edge of priory road did not warp into the gate.\n");
    return false;
  }
  if (!move_to_tile(state, "priory-road", 11, 0, true) || state->current_area != "priory-gate") {
    std::fprintf(stderr, "Smoke test failed: left edge of priory road did not warp into the gate.\n");
    return false;
  }
  if (can_occupy(*state, "priory-gate", 10, 7)) {
    std::fprintf(stderr, "Smoke test failed: priory gate door was passable before the gate friar opened it.\n");
    return false;
  }
  if (!talk_to_npc(state, "priory-gate", "gate-friar", &audio) || !state->progress.gate_opened) {
    std::fprintf(stderr, "Smoke test failed: gate friar did not open Saint Catherine.\n");
    return false;
  }
  if (!move_to_tile(state, "priory-gate", 10, 7, true) || state->current_area != "priory-court") {
    std::fprintf(stderr, "Smoke test failed: priory gate did not warp into the courtyard.\n");
    return false;
  }
  if (objective_text(*state) != "READ THE TASK BOARD IN SAINT CATHERINE'S COURTYARD.") {
    std::fprintf(stderr, "Smoke test failed: priory arrival objective was incorrect.\n");
    return false;
  }
  if (!interact_with_tile(state, "priory-court", 7, 12, &audio) || !state->progress.task_board_read) {
    std::fprintf(stderr, "Smoke test failed: courtyard task board did not open or register.\n");
    return false;
  }
  while (state->dialogue.active) {
    advance_dialogue(state);
  }
  if (!talk_to_npc(state, "scriptorium", "brother-martin", &audio) || !state->progress.met_martin) {
    std::fprintf(stderr, "Smoke test failed: Brother Martin did not register.\n");
    return false;
  }
  if (!talk_to_npc(state, "saint-hilda-hospice", "hospice-prioress", &audio) || !state->progress.met_prioress) {
    std::fprintf(stderr, "Smoke test failed: Saint Hilda prioress did not register.\n");
    return false;
  }
  if (!travel_to_area(state, "bellfield") || !village_green_reached(*state)) {
    std::fprintf(stderr, "Smoke test failed: village green route did not register.\n");
    return false;
  }
  if (!talk_to_npc(state, "bellfield", "agnes", &audio)) {
    std::fprintf(stderr, "Smoke test failed: village green miller was unreachable after town pass.\n");
    return false;
  }
  if (!travel_to_area(state, "candlewharf") || !state->progress.visited_quay) {
    std::fprintf(stderr, "Smoke test failed: Ravenscar route did not register.\n");
    return false;
  }
  if (!talk_to_npc(state, "candlewharf", "rowan", &audio)) {
    std::fprintf(stderr, "Smoke test failed: Ravenscar roper was unreachable after quay pass.\n");
    return false;
  }
  if (!talk_to_npc(state, "chapel", "prior", &audio)) {
    std::fprintf(stderr, "Smoke test failed: could not talk to prior after the arrival circuit.\n");
    return false;
  }
  if (state->quest.started) {
    std::fprintf(stderr, "Smoke test failed: prior still triggered the deprecated fetch quest.\n");
    return false;
  }
  if (!priory_arrival_complete(*state)) {
    std::fprintf(stderr, "Smoke test failed: the canonical Priory arrival circuit did not complete.\n");
    return false;
  }

  GameState route_probe = *state;
  if (!travel_to_area(&route_probe, "priory-court")) {
    std::fprintf(stderr, "Smoke test failed: could not route back to the courtyard for road-edge checks.\n");
    return false;
  }
  if (!move_to_tile(&route_probe, "priory-court", 12, 19, true) || route_probe.current_area != "bellfield") {
    std::fprintf(stderr, "Smoke test failed: priory south exit did not warp from visible road edge.\n");
    return false;
  }
  if (!move_to_tile(&route_probe, "bellfield", 31, 8, true) || route_probe.current_area != "candlewharf") {
    std::fprintf(stderr, "Smoke test failed: Blackpine east exit did not warp from visible road edge.\n");
    return false;
  }

  GameState hospice_route_probe = *state;
  if (!travel_to_area(&hospice_route_probe, "bellfield")) {
    std::fprintf(stderr, "Smoke test failed: could not reach Blackpine for hospice exit check.\n");
    return false;
  }
  if (!move_to_tile(&hospice_route_probe, "bellfield", 0, 8, true) ||
      hospice_route_probe.current_area != "saint-hilda-hospice") {
    std::fprintf(stderr, "Smoke test failed: Blackpine west exit did not warp from visible road edge.\n");
    return false;
  }

  if (!interact_with_tile(state, "bellfield", 10, 12, &audio) || state->shop.id != ShopId::BlackpineCloth) {
    std::fprintf(stderr, "Smoke test failed: Blackpine stall did not open from world interaction.\n");
    return false;
  }
  if (!purchase_shop_offer(state, 0) || !has_outfit(*state, "blackpine_trim") ||
      state->profile.equipped_outfit_id != "blackpine_trim" || state->profile.coins != 12) {
    std::fprintf(stderr, "Smoke test failed: Blackpine clothing purchase did not unlock outfit.\n");
    return false;
  }
  state->shop = {};

  if (!interact_with_tile(state, "scriptorium", 10, 6, &audio) || state->shop.id != ShopId::ScriptoriumSupply) {
    std::fprintf(stderr, "Smoke test failed: scriptorium supply chest did not open from world interaction.\n");
    return false;
  }
  state->shop = {};

  if (!interact_with_tile(state, "saint-hilda-hospice", 12, 10, &audio) || state->shop.id != ShopId::HospiceSupply) {
    std::fprintf(stderr, "Smoke test failed: hospice supply table did not open from world interaction.\n");
    return false;
  }
  state->shop = {};

  if (!talk_to_npc(state, "bellfield", "alisoun", &audio) || state->shop.id != ShopId::BlackpineCloth) {
    std::fprintf(stderr, "Smoke test failed: Alisoun did not open her stall from NPC interaction.\n");
    return false;
  }
  state->shop = {};

  const int faith_before_prayer = virtue_value(*state, "faith");
  if (!interact_with_tile(state, "chapel", 6, 2, &audio)) {
    std::fprintf(stderr, "Smoke test failed: chapel rail interaction did not open.\n");
    return false;
  }
  while (state->dialogue.active) {
    advance_dialogue(state);
  }
  if (virtue_value(*state, "faith") <= faith_before_prayer) {
    std::fprintf(stderr, "Smoke test failed: chapel prayer did not improve the virtue ledger.\n");
    return false;
  }

  state->profile.coins = std::max(state->profile.coins, 30);
  if (!talk_to_npc(state, "bellfield", "ysabel", &audio) || state->shop.id != ShopId::PropertyCharter) {
    std::fprintf(stderr, "Smoke test failed: Ysabel did not open the property charter menu.\n");
    return false;
  }
  if (!purchase_shop_offer(state, 0) || !owns_home(*state, "blackpine-cottage") ||
      state->profile.active_home_id != "blackpine-cottage") {
    std::fprintf(stderr, "Smoke test failed: Blackpine cottage purchase did not complete.\n");
    return false;
  }
  state->shop = {};
  if (!travel_to_area(state, "bellfield") || !move_to_tile(state, "bellfield", 5, 16)) {
    std::fprintf(stderr, "Smoke test failed: could not stand before the owned cottage door.\n");
    return false;
  }
  state->player.facing = Direction::Up;
  interact(state, 0, &audio);
  if (state->current_area != "blackpine-cottage") {
    std::fprintf(stderr, "Smoke test failed: owned cottage door did not enter the purchased home.\n");
    return false;
  }
  if (!move_to_tile(state, "blackpine-cottage", 11, 8)) {
    std::fprintf(stderr, "Smoke test failed: could not stand before the house chest.\n");
    return false;
  }
  state->player.facing = Direction::Up;
  interact(state, 0, &audio);
  if (!state->chest.active) {
    std::fprintf(stderr, "Smoke test failed: house chest did not open.\n");
    return false;
  }
  const std::vector<std::string> pack_ids = sorted_item_ids(state->profile.inventory);
  const auto ledger_it = std::find(pack_ids.begin(), pack_ids.end(), "ledger_scraps");
  if (ledger_it == pack_ids.end()) {
    std::fprintf(stderr, "Smoke test failed: ledger scraps were unavailable for chest storage test.\n");
    return false;
  }
  state->chest.inventory_selection = static_cast<int>(std::distance(pack_ids.begin(), ledger_it));
  const int stored_before = stored_count(*state, "ledger_scraps");
  if (!store_selected_item(state, 0) || stored_count(*state, "ledger_scraps") <= stored_before) {
    std::fprintf(stderr, "Smoke test failed: storing an item in the home chest failed.\n");
    return false;
  }
  state->chest.browse_storage = true;
  const std::vector<std::string> chest_ids = sorted_item_ids(state->profile.stored_items);
  const auto stored_ledger_it = std::find(chest_ids.begin(), chest_ids.end(), "ledger_scraps");
  if (stored_ledger_it == chest_ids.end()) {
    std::fprintf(stderr, "Smoke test failed: stored ledger scraps were unavailable for retrieval test.\n");
    return false;
  }
  state->chest.storage_selection = static_cast<int>(std::distance(chest_ids.begin(), stored_ledger_it));
  if (!retrieve_selected_item(state, 0) || stored_count(*state, "ledger_scraps") != stored_before) {
    std::fprintf(stderr, "Smoke test failed: retrieving an item from the home chest failed.\n");
    return false;
  }
  state->chest = {};

  state->profile.coins = std::max(state->profile.coins, 80);
  if (!talk_to_npc(state, "bellfield", "ysabel", &audio) || state->shop.id != ShopId::PropertyCharter) {
    std::fprintf(stderr, "Smoke test failed: Ysabel did not reopen the property ledger for furnishings.\n");
    return false;
  }
  state->shop.selection = 1;
  if (!purchase_shop_offer(state, 0) || inventory_count(*state, "rush_bench_item") <= 0) {
    std::fprintf(stderr, "Smoke test failed: household decor purchase did not add a placeable item.\n");
    return false;
  }
  state->shop = {};

  if (!talk_to_npc(state, "bellfield", "alisoun", &audio) || state->shop.id != ShopId::BlackpineCloth) {
    std::fprintf(stderr, "Smoke test failed: Alisoun did not reopen the market menu for farm goods.\n");
    return false;
  }
  state->shop.selection = 2;
  if (!purchase_shop_offer(state, 0) || inventory_count(*state, "field_tools") <= 0) {
    std::fprintf(stderr, "Smoke test failed: field tools purchase did not complete.\n");
    return false;
  }
  state->shop.selection = 3;
  if (!purchase_shop_offer(state, 0) || inventory_count(*state, "cabbage_seed") <= 0) {
    std::fprintf(stderr, "Smoke test failed: cabbage seed purchase did not complete.\n");
    return false;
  }
  state->shop.selection = 6;
  if (!purchase_shop_offer(state, 0) || inventory_count(*state, "rye_loaf") <= 0) {
    std::fprintf(stderr, "Smoke test failed: food purchase did not complete.\n");
    return false;
  }
  state->shop = {};

  state->player.hp = std::max(1, state->player.max_hp - 5);
  {
    const std::vector<std::string> ids = sorted_item_ids(state->profile.inventory);
    const auto loaf_it = std::find(ids.begin(), ids.end(), "rye_loaf");
    if (loaf_it == ids.end()) {
      std::fprintf(stderr, "Smoke test failed: rye loaf missing before food-heal check.\n");
      return false;
    }
    state->profile.pack_selection = static_cast<int>(std::distance(ids.begin(), loaf_it));
  }
  const int hp_before_meal = state->player.hp;
  if (!use_selected_pack_item(state, 0) || state->player.hp <= hp_before_meal) {
    std::fprintf(stderr, "Smoke test failed: food use did not restore hp.\n");
    return false;
  }

  {
    const std::vector<std::string> ids = sorted_item_ids(state->profile.inventory);
    const auto seed_it = std::find(ids.begin(), ids.end(), "cabbage_seed");
    if (seed_it == ids.end()) {
      std::fprintf(stderr, "Smoke test failed: cabbage seed missing before sowing.\n");
      return false;
    }
    state->profile.pack_selection = static_cast<int>(std::distance(ids.begin(), seed_it));
  }
  if (!use_selected_pack_item(state, 0) || state->profile.active_item_id != "cabbage_seed") {
    std::fprintf(stderr, "Smoke test failed: seed packet did not become the active sowing item.\n");
    return false;
  }

  if (!travel_to_area(state, "blackpine-cottage") || !move_to_tile(state, "blackpine-cottage", 7, 1) ||
      !step_player_instant(state, Direction::Up, 0) ||
      state->current_area != "blackpine-cottage-yard") {
    std::fprintf(stderr, "Smoke test failed: cottage back door did not reach the tillable yard. area=%s x=%d y=%d\n",
                 state->current_area.c_str(), state->player.tile_x, state->player.tile_y);
    return false;
  }
  if (!move_to_tile(state, "blackpine-cottage-yard", 6, 9)) {
    std::fprintf(stderr, "Smoke test failed: could not reach the cottage plot edge.\n");
    return false;
  }
  state->player.facing = Direction::Up;
  interact(state, 0, &audio);
  const CropPlot* plot = crop_plot_at(*state, "blackpine-cottage-yard", 6, 8);
  if (plot == nullptr || !plot->tilled || !plot->crop_id.empty()) {
    std::fprintf(stderr, "Smoke test failed: field tools did not till the cottage yard.\n");
    return false;
  }
  interact(state, 0, &audio);
  plot = crop_plot_at(*state, "blackpine-cottage-yard", 6, 8);
  if (plot == nullptr || plot->crop_id != "cabbage") {
    std::fprintf(stderr, "Smoke test failed: active seed did not sow the tilled yard.\n");
    return false;
  }
  if (!move_to_tile(state, "blackpine-cottage-yard", 8, 3) || !step_player_instant(state, Direction::Up, 0) ||
      state->current_area != "blackpine-cottage") {
    std::fprintf(stderr, "Smoke test failed: yard return door did not re-enter the cottage.\n");
    return false;
  }
  if (!move_to_tile(state, "blackpine-cottage", 3, 8)) {
    std::fprintf(stderr, "Smoke test failed: could not stand at the cottage bed.\n");
    return false;
  }
  state->player.facing = Direction::Up;
  const int day_before_sleep = state->profile.day;
  interact(state, 0, &audio);
  interact(state, 0, &audio);
  interact(state, 0, &audio);
  if (state->profile.day != day_before_sleep + 3) {
    std::fprintf(stderr, "Smoke test failed: cottage bed did not advance the day counter.\n");
    return false;
  }
  if (!move_to_tile(state, "blackpine-cottage", 7, 1) || !step_player_instant(state, Direction::Up, 0) ||
      state->current_area != "blackpine-cottage-yard" ||
      !move_to_tile(state, "blackpine-cottage-yard", 6, 9)) {
    std::fprintf(stderr, "Smoke test failed: could not return to the matured crop plot.\n");
    return false;
  }
  state->player.facing = Direction::Up;
  const int cabbage_before_harvest = inventory_count(*state, "cabbage_head");
  interact(state, 0, &audio);
  if (inventory_count(*state, "cabbage_head") <= cabbage_before_harvest) {
    std::fprintf(stderr, "Smoke test failed: mature cottage crop did not harvest into inventory.\n");
    return false;
  }
  if (!move_to_tile(state, "blackpine-cottage-yard", 8, 3) || !step_player_instant(state, Direction::Up, 0) ||
      state->current_area != "blackpine-cottage") {
    std::fprintf(stderr, "Smoke test failed: yard return route broke after harvesting.\n");
    return false;
  }
  {
    const std::vector<std::string> ids = sorted_item_ids(state->profile.inventory);
    const auto bench_it = std::find(ids.begin(), ids.end(), "rush_bench_item");
    if (bench_it == ids.end()) {
      std::fprintf(stderr, "Smoke test failed: decor item missing before placement.\n");
      return false;
    }
    state->profile.pack_selection = static_cast<int>(std::distance(ids.begin(), bench_it));
  }
  if (!use_selected_pack_item(state, 0) || state->profile.active_item_id != "rush_bench_item") {
    std::fprintf(stderr, "Smoke test failed: decor item did not become the active placement item.\n");
    return false;
  }
  if (!move_to_tile(state, "blackpine-cottage", 6, 9)) {
    std::fprintf(stderr, "Smoke test failed: could not stand at the cottage decor placement tile.\n");
    return false;
  }
  state->player.facing = Direction::Right;
  interact(state, 0, &audio);
  if (decor_placement_at(*state, "blackpine-cottage", 7, 9) == nullptr) {
    std::fprintf(stderr, "Smoke test failed: household decor did not place inside the owned cottage.\n");
    return false;
  }

  GameState hospital_probe = *state;
  if (!travel_to_area(&hospital_probe, "candlewharf") || !move_to_tile(&hospital_probe, "candlewharf", 14, 15)) {
    std::fprintf(stderr, "Smoke test failed: could not position the hospital respawn probe by a Blackpine-side monster.\n");
    return false;
  }
  hospital_probe.player.hp = 1;
  update_monsters(&hospital_probe, 5000U);
  if (hospital_probe.current_area != "blackpine-hospital" || hospital_probe.player.hp != hospital_probe.player.max_hp) {
    std::fprintf(stderr, "Smoke test failed: zero hp did not return the player to Blackpine's hospital.\n");
    return false;
  }
  if (!talk_to_npc(&hospital_probe, "blackpine-hospital", "apothecary-eda", &audio) ||
      hospital_probe.shop.id != ShopId::HospiceSupply) {
    std::fprintf(stderr, "Smoke test failed: Blackpine infirmary apothecary did not open the care supplies menu.\n");
    return false;
  }

  GameState priory_respawn_probe = *state;
  if (!travel_to_area(&priory_respawn_probe, "chapel")) {
    std::fprintf(stderr, "Smoke test failed: could not position the priory-side recovery probe.\n");
    return false;
  }
  return_player_to_safety(&priory_respawn_probe, 0);
  if (priory_respawn_probe.current_area != "saint-hilda-hospice") {
    std::fprintf(stderr, "Smoke test failed: priory-side recovery did not route to Saint Hilda hospice.\n");
    return false;
  }

  GameState hospice_exit_probe = *state;
  if (!travel_to_area(&hospice_exit_probe, "priory-court")) {
    std::fprintf(stderr, "Smoke test failed: hospice priory door did not return to the courtyard. area=%s x=%d y=%d\n",
                 hospice_exit_probe.current_area.c_str(), hospice_exit_probe.player.tile_x,
                 hospice_exit_probe.player.tile_y);
    return false;
  }
  if (!travel_to_area(&hospice_exit_probe, "chapel")) {
    std::fprintf(stderr, "Smoke test failed: hospice return route could not continue on to the chapel.\n");
    return false;
  }

  start_dialogue(
      state,
      "TASK BOARD",
      {"SAINT CATHERINE TASK BOARD: EACH DUTY STRENGTHENS ONE PILLAR OF THE PRIORY WHILE ANOTHER WAITS. PREACH IN BLACKPINE, COPY TEXTS, PATROL ROADS, TEND STORES, AND RUN CHARITY ROUNDS."});
  if (state->dialogue.pages.size() < 2) {
    std::fprintf(stderr, "Smoke test failed: long dialogue did not paginate.\n");
    return false;
  }
  while (state->dialogue.active) {
    advance_dialogue(state);
  }

  state->journal_open = true;
  state->profile.journal_page = 3;
  state->profile.wardrobe_selection = 1;
  if (!has_outfit(*state, "blackpine_trim")) {
    std::fprintf(stderr, "Smoke test failed: wardrobe unlock state missing before equip check.\n");
    return false;
  }
  state->profile.equipped_outfit_id = state->profile.unlocked_outfits[state->profile.wardrobe_selection];
  if (state->profile.equipped_outfit_id != "blackpine_trim") {
    std::fprintf(stderr, "Smoke test failed: wardrobe equip selection did not land on purchased outfit.\n");
    return false;
  }
  state->journal_open = false;

  std::printf("PRIORY SMOKE TEST OK\n");
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  bool smoke_test = false;
  bool capture_previews_mode = false;
  bool force_software_renderer = false;
  bool force_accelerated_renderer = false;
  for (int index = 1; index < argc; ++index) {
    if (std::string(argv[index]) == "--smoke-test") {
      smoke_test = true;
    } else if (std::string(argv[index]) == "--capture-previews") {
      capture_previews_mode = true;
    } else if (std::string(argv[index]) == "--software-renderer") {
      force_software_renderer = true;
    } else if (std::string(argv[index]) == "--accelerated-renderer") {
      force_accelerated_renderer = true;
    }
  }

  const Uint32 sdl_flags = smoke_test
                               ? SDL_INIT_GAMECONTROLLER
                               : (SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER |
                                  (capture_previews_mode ? 0 : SDL_INIT_AUDIO));
  if (SDL_Init(sdl_flags) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  pp_context context;
  if (pp_init(&context, "priory") != 0) {
    std::fprintf(stderr, "pp_init failed\n");
    SDL_Quit();
    return 1;
  }

  GameState state;
  state.context = &context;
  state.world = build_world();
  state.smoke_test = smoke_test;
  state.has_save = load_game(&state);

  if (smoke_test) {
    const bool ok = run_smoke_test(&state);
    pp_shutdown(&context);
    SDL_Quit();
    return ok ? 0 : 1;
  }

  int width = kScreenWidth * 2;
  int height = kScreenHeight * 2;
  pp_get_framebuffer_size(&context, &width, &height);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  SDL_Window* window =
      SDL_CreateWindow("Priory", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }

  std::vector<Uint32> renderer_attempts;
#if defined(_WIN32)
  const bool prefer_software_renderer = !force_accelerated_renderer;
#else
  const bool prefer_software_renderer = force_software_renderer;
#endif
  if (prefer_software_renderer) {
    renderer_attempts.push_back(SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
    renderer_attempts.push_back(SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  } else {
    renderer_attempts.push_back(SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    renderer_attempts.push_back(SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
  }
  renderer_attempts.push_back(0);

  SDL_Renderer* renderer = nullptr;
  for (Uint32 flags : renderer_attempts) {
    renderer = SDL_CreateRenderer(window, -1, flags);
    if (renderer != nullptr) {
      break;
    }
  }
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    pp_shutdown(&context);
    SDL_Quit();
    return 1;
  }

  SDL_RenderSetLogicalSize(renderer, kScreenWidth, kScreenHeight);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  if (capture_previews_mode) {
    const bool ok = capture_previews(renderer, &context);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    pp_shutdown(&context);
    SDL_Quit();
    return ok ? 0 : 1;
  }

  AudioState audio;
  pp_audio_spec audio_spec{};
  SDL_AudioDeviceID device = 0;
  audio_spec.freq = 48000;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 2048;
  audio_spec.callback = audio_callback;
  audio_spec.userdata = &audio;
  if (pp_audio_open(&audio_spec, &device) == 0) {
    SDL_PauseAudioDevice(device, 0);
  }

  if (state.has_save) {
    if (const Area* area = current_area(state)) {
      state.area_banner = area->name;
      state.area_banner_until = SDL_GetTicks() + kAreaBannerMs;
    }
  }

  pp_input_state raw_input{};
  pp_input_state previous_input{};

  while (!pp_should_exit(&context)) {
    const Uint32 now = SDL_GetTicks();
    pp_poll_input(&context, &raw_input);
    const ButtonState buttons = make_buttons(raw_input, previous_input);
    previous_input = raw_input;

    if (state.on_title) {
      const int option_count = state.has_save ? 2 : 1;
      if (buttons.pressed_up || buttons.pressed_down) {
        state.title_selection =
            (state.title_selection + option_count + (buttons.pressed_down ? 1 : -1)) % option_count;
      }
      if (buttons.pressed_a || buttons.pressed_start) {
        if (state.has_save && state.title_selection == 0) {
          state.on_title = false;
          state.area_banner_until = now + kAreaBannerMs;
        } else {
          state.profile = {};
          state.creation.active = true;
          state.creation.row = 0;
          state.shop = {};
          state.dialogue = {};
          state.journal_open = false;
          state.on_title = false;
        }
      }
    } else if (state.creation.active) {
      if (buttons.pressed_up) {
        state.creation.row = (state.creation.row + 4) % 5;
      } else if (buttons.pressed_down) {
        state.creation.row = (state.creation.row + 1) % 5;
      }
      if (buttons.pressed_left || buttons.pressed_right) {
        const int delta = buttons.pressed_right ? 1 : -1;
        if (state.creation.row == 0) {
          const int count = static_cast<int>(life_path_definitions().size());
          state.profile.life_path_index = (state.profile.life_path_index + count + delta) % count;
          state.profile.reflection_index = 0;
        } else if (state.creation.row == 1) {
          state.profile.reflection_index = (state.profile.reflection_index + 2 + delta) % 2;
        } else if (state.creation.row == 2) {
          const int count = static_cast<int>(complexion_definitions().size());
          state.profile.complexion_index = (state.profile.complexion_index + count + delta) % count;
        } else if (state.creation.row == 3) {
          const int count = static_cast<int>(hair_definitions().size());
          state.profile.hair_index = (state.profile.hair_index + count + delta) % count;
        }
      }
      if (buttons.pressed_a || buttons.pressed_start) {
        if (state.creation.row == 4) {
          start_new_game(&state, now);
          state.has_save = true;
        } else {
          state.creation.row = std::min(4, state.creation.row + 1);
        }
      }
      if (buttons.pressed_b || buttons.pressed_select) {
        state.creation.active = false;
        state.on_title = true;
      }
    } else if (state.shop.active) {
      const int count = static_cast<int>(shop_offers(state.shop.id).size());
      if (count > 0 && (buttons.pressed_up || buttons.pressed_down)) {
        state.shop.selection = (state.shop.selection + count + (buttons.pressed_down ? 1 : -1)) % count;
      }
      if (buttons.pressed_a) {
        purchase_shop_offer(&state, now);
      }
      if (buttons.pressed_b || buttons.pressed_start || buttons.pressed_select) {
        state.shop = {};
      }
    } else if (state.chest.active) {
      if (buttons.pressed_left || buttons.pressed_right) {
        state.chest.browse_storage = !state.chest.browse_storage;
      } else if (buttons.pressed_up) {
        if (state.chest.browse_storage) {
          const int count = std::max(1, static_cast<int>(sorted_item_ids(state.profile.stored_items).size()));
          state.chest.storage_selection = (state.chest.storage_selection + count - 1) % count;
        } else {
          const int count = std::max(1, static_cast<int>(sorted_item_ids(state.profile.inventory).size()));
          state.chest.inventory_selection = (state.chest.inventory_selection + count - 1) % count;
        }
      } else if (buttons.pressed_down) {
        if (state.chest.browse_storage) {
          const int count = std::max(1, static_cast<int>(sorted_item_ids(state.profile.stored_items).size()));
          state.chest.storage_selection = (state.chest.storage_selection + 1) % count;
        } else {
          const int count = std::max(1, static_cast<int>(sorted_item_ids(state.profile.inventory).size()));
          state.chest.inventory_selection = (state.chest.inventory_selection + 1) % count;
        }
      } else if (buttons.pressed_a) {
        if (state.chest.browse_storage) {
          retrieve_selected_item(&state, now);
        } else {
          store_selected_item(&state, now);
        }
      }
      if (buttons.pressed_b || buttons.pressed_start || buttons.pressed_select) {
        state.chest = {};
      }
    } else if (state.dialogue.active) {
      if (buttons.pressed_a || buttons.pressed_start || buttons.pressed_b) {
        advance_dialogue(&state);
      }
    } else if (state.journal_open) {
      if (buttons.pressed_left) {
        state.profile.journal_page = (state.profile.journal_page + 5) % 6;
      } else if (buttons.pressed_right) {
        state.profile.journal_page = (state.profile.journal_page + 1) % 6;
      } else if (state.profile.journal_page == 2 && !state.profile.inventory.empty() &&
                 (buttons.pressed_up || buttons.pressed_down)) {
        const int count = static_cast<int>(sorted_item_ids(state.profile.inventory).size());
        state.profile.pack_selection =
            (state.profile.pack_selection + count + (buttons.pressed_down ? 1 : -1)) % count;
      } else if (state.profile.journal_page == 2 && buttons.pressed_a) {
        use_selected_pack_item(&state, now);
      } else if (state.profile.journal_page == 3 &&
                 !state.profile.unlocked_outfits.empty() && (buttons.pressed_up || buttons.pressed_down)) {
        const int count = static_cast<int>(state.profile.unlocked_outfits.size());
        state.profile.wardrobe_selection =
            (state.profile.wardrobe_selection + count + (buttons.pressed_down ? 1 : -1)) % count;
      } else if (state.profile.journal_page == 3 && buttons.pressed_a &&
                 !state.profile.unlocked_outfits.empty()) {
        state.profile.equipped_outfit_id =
            state.profile.unlocked_outfits[static_cast<std::size_t>(state.profile.wardrobe_selection)];
        set_status_message(&state, "GARMENT EQUIPPED.", now);
        save_game(state);
      }
      if (buttons.pressed_start || buttons.pressed_b || buttons.pressed_select) {
        state.journal_open = false;
      }
    } else {
      if (buttons.pressed_start) {
        state.journal_open = true;
      } else if (!state.player.moving && buttons.pressed_b) {
        player_attack(&state, now);
      } else if (!state.player.moving && buttons.pressed_a) {
        interact(&state, now, &audio);
      }
      update_player(&state, buttons, now);
      if (!state.dialogue.active && !state.shop.active && !state.journal_open) {
        update_monsters(&state, now);
      }
      if (!state.player.moving && !state.dialogue.active && !state.shop.active && !state.journal_open) {
        update_npcs(&state, now);
      }
    }

    render_frame(renderer, state, now);
    SDL_RenderPresent(renderer);
  }

  save_game(state);
  if (device != 0U) {
    SDL_CloseAudioDevice(device);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  pp_shutdown(&context);
  SDL_Quit();
  return 0;
}
