#ifndef PIXELPAL_RPG_ENGINE_HPP
#define PIXELPAL_RPG_ENGINE_HPP

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace pixelpal::rpg {

enum class Facing {
  Up = 0,
  Right,
  Down,
  Left,
};

enum class SpriteRole {
  Prior = 0,
  Sister,
  Monk,
  Fisher,
  Merchant,
  Child,
  Elder,
  Watchman,
};

enum class LayerKind {
  Ground = 0,
  Wall,
  Fringe,
  Object,
  Trigger,
};

enum class EntityKind {
  Npc = 0,
  Monster,
  Prop,
  Pickup,
  Chest,
  Shopkeeper,
  Crop,
};

enum class RequirementKind {
  ReachArea = 0,
  TalkToEntity,
  DefeatEntity,
  CollectItem,
  HasVirtue,
  Custom,
};

enum class ActionKind {
  GiveItem = 0,
  GiveCoins,
  SetFlag,
  CompleteQuest,
  UnlockArea,
  RunDialogue,
  Custom,
};

struct Rect {
  int x = 0;
  int y = 0;
  int width = 1;
  int height = 1;
};

struct TileDefinition {
  std::string id;
  std::string name;
  std::string description;
  char symbol = '.';
  bool walkable = true;
  bool opaque = false;
  LayerKind default_layer = LayerKind::Ground;
  std::vector<std::string> tags;
  std::map<std::string, std::string> properties;
};

struct TileLayer {
  std::string id;
  std::string name;
  LayerKind kind = LayerKind::Ground;
  int width = 0;
  int height = 0;
  bool visible = true;
  std::vector<std::string> rows;
  std::map<std::string, std::string> properties;
};

struct Warp {
  std::string id;
  std::string label;
  Rect bounds;
  std::string target_area;
  std::string target_warp;
  int target_x = 0;
  int target_y = 0;
  Facing target_facing = Facing::Down;
};

struct SpriteAsset {
  std::string id;
  std::string name;
  bool monster = false;
  std::map<std::string, std::vector<std::string>> frames;
};

struct PatternStamp {
  std::string id;
  std::string name;
  std::vector<std::string> rows;
};

struct EntityArchetype {
  std::string id;
  std::string name;
  EntityKind kind = EntityKind::Npc;
  std::string sprite_id;
  SpriteRole role = SpriteRole::Monk;
  bool solid = true;
  bool aggressive = false;
  int max_hp = 0;
  int attack = 0;
  std::string dialogue_id;
  std::string shop_id;
  std::vector<std::string> tags;
  std::map<std::string, std::string> properties;
};

struct SceneEntity {
  std::string id;
  std::string name;
  EntityKind kind = EntityKind::Npc;
  std::string archetype_id;
  std::string sprite_id;
  SpriteRole role = SpriteRole::Monk;
  int x = 0;
  int y = 0;
  Facing facing = Facing::Down;
  bool solid = true;
  bool aggressive = false;
  int max_hp = 0;
  int attack = 0;
  std::string dialogue_id;
  std::string dialogue_text;
  std::string shop_id;
  std::vector<std::string> tags;
  std::map<std::string, std::string> properties;
};

struct DialogueChoice {
  std::string text;
  std::string next_node_id;
  std::string action_id;
};

struct DialogueNode {
  std::string id;
  std::string speaker;
  std::string text;
  std::vector<DialogueChoice> choices;
  bool ends_conversation = false;
};

struct DialogueGraph {
  std::string id;
  std::string name;
  std::string start_node_id;
  std::vector<DialogueNode> nodes;
};

struct QuestRequirement {
  std::string id;
  RequirementKind kind = RequirementKind::Custom;
  std::string target_id;
  std::string area_id;
  int x = 0;
  int y = 0;
  int quantity = 1;
  std::string description;
};

struct QuestAction {
  std::string id;
  ActionKind kind = ActionKind::Custom;
  std::string target_id;
  int amount = 0;
  std::string payload;
  std::string description;
};

struct QuestStage {
  std::string id;
  std::string text;
  std::vector<QuestRequirement> requirements;
  std::vector<QuestAction> on_complete;
};

struct Quest {
  std::string id;
  std::string title;
  std::string summary;
  std::string giver_entity_id;
  std::string start_dialogue;
  std::string completion_dialogue;
  std::vector<QuestStage> stages;
  std::vector<QuestAction> rewards;
};

struct ItemDefinition {
  std::string id;
  std::string name;
  std::string description;
  int base_price = 0;
  int heal_amount = 0;
  bool consumable = false;
  bool placeable = false;
  std::string placement_archetype_id;
  std::vector<std::string> tags;
};

struct ShopListing {
  std::string item_id;
  int price = 0;
  int stock = -1;
};

struct ShopDefinition {
  std::string id;
  std::string name;
  std::string keeper_entity_id;
  std::vector<ShopListing> listings;
};

struct CropDefinition {
  std::string id;
  std::string name;
  std::string seed_item_id;
  std::string produce_item_id;
  int growth_days = 1;
  std::vector<std::string> stage_tiles;
};

struct Area {
  std::string id;
  std::string name;
  bool indoor = false;
  bool player_tillable = false;
  int width = 0;
  int height = 0;
  std::vector<TileLayer> layers;
  std::vector<Warp> warps;
  std::vector<SceneEntity> entities;
  std::map<std::string, std::string> properties;
};

struct ProjectMeta {
  std::string id;
  std::string name;
  std::string author;
  std::string version = "0.1.0";
  std::string starting_area_id;
  std::string starting_warp_id;
};

struct Project {
  ProjectMeta meta;
  std::vector<TileDefinition> tiles;
  std::vector<PatternStamp> stamps;
  std::vector<SpriteAsset> sprites;
  std::vector<EntityArchetype> archetypes;
  std::vector<DialogueGraph> dialogues;
  std::vector<ItemDefinition> items;
  std::vector<ShopDefinition> shops;
  std::vector<CropDefinition> crops;
  std::vector<Quest> quests;
  std::vector<Area> areas;
};

struct ValidationIssue {
  enum class Severity {
    Warning = 0,
    Error,
  };

  Severity severity = Severity::Warning;
  std::string path;
  std::string message;
};

struct ValidationReport {
  std::vector<ValidationIssue> issues;

  [[nodiscard]] bool has_errors() const;
  [[nodiscard]] int error_count() const;
  [[nodiscard]] int warning_count() const;
};

struct ProjectSummary {
  int area_count = 0;
  int layer_count = 0;
  int warp_count = 0;
  int entity_count = 0;
  int dialogue_count = 0;
  int quest_count = 0;
  int item_count = 0;
  int shop_count = 0;
  int crop_count = 0;
  int sprite_count = 0;
  int archetype_count = 0;
};

std::string facing_name(Facing facing);
Facing facing_from_string(std::string_view value);

std::string sprite_role_name(SpriteRole role);
SpriteRole sprite_role_from_string(std::string_view value);

std::string layer_kind_name(LayerKind kind);
LayerKind layer_kind_from_string(std::string_view value);

std::string entity_kind_name(EntityKind kind);
EntityKind entity_kind_from_string(std::string_view value);

std::string requirement_kind_name(RequirementKind kind);
RequirementKind requirement_kind_from_string(std::string_view value);

std::string action_kind_name(ActionKind kind);
ActionKind action_kind_from_string(std::string_view value);

Project make_starter_project();
ValidationReport validate_project(const Project& project);
ProjectSummary summarize_project(const Project& project);

bool load_project(Project* project, const std::filesystem::path& path, std::string* error);
bool save_project(const Project& project, const std::filesystem::path& path, std::string* error);

const Area* find_area(const Project& project, std::string_view id);
const SceneEntity* find_entity(const Project& project, std::string_view id);
const DialogueGraph* find_dialogue(const Project& project, std::string_view id);
const ItemDefinition* find_item(const Project& project, std::string_view id);
const ShopDefinition* find_shop(const Project& project, std::string_view id);
const CropDefinition* find_crop(const Project& project, std::string_view id);
const EntityArchetype* find_archetype(const Project& project, std::string_view id);
const SpriteAsset* find_sprite(const Project& project, std::string_view id);

}  // namespace pixelpal::rpg

#endif
