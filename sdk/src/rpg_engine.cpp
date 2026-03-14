#include "pixelpal/rpg_engine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace pixelpal::rpg {
namespace {

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

std::string trim_copy(std::string value) {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) { return !is_space(ch); }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) { return !is_space(ch); }).base(), value.end());
  return value;
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

void write_indent(std::ostream* output, int indent) {
  for (int index = 0; index < indent; ++index) {
    output->put(' ');
  }
}

void write_json_escaped(const std::string& text, std::ostream* output) {
  output->put('"');
  for (char ch : text) {
    switch (ch) {
      case '\\':
        *output << "\\\\";
        break;
      case '"':
        *output << "\\\"";
        break;
      case '\n':
        *output << "\\n";
        break;
      case '\r':
        *output << "\\r";
        break;
      case '\t':
        *output << "\\t";
        break;
      default:
        output->put(ch);
        break;
    }
  }
  output->put('"');
}

void write_json_value(const JsonValue& value, std::ostream* output, int indent) {
  switch (value.type) {
    case JsonValue::Type::Null:
      *output << "null";
      break;
    case JsonValue::Type::Bool:
      *output << (value.bool_value ? "true" : "false");
      break;
    case JsonValue::Type::Number: {
      std::ostringstream buffer;
      buffer.setf(std::ios::fixed);
      buffer.precision(4);
      buffer << value.number_value;
      std::string text = buffer.str();
      while (text.size() > 2 && text.find('.') != std::string::npos && text.back() == '0') {
        text.pop_back();
      }
      if (!text.empty() && text.back() == '.') {
        text.pop_back();
      }
      *output << text;
      break;
    }
    case JsonValue::Type::String:
      write_json_escaped(value.string_value, output);
      break;
    case JsonValue::Type::Array:
      *output << "[";
      if (!value.array_value.empty()) {
        *output << "\n";
        for (std::size_t index = 0; index < value.array_value.size(); ++index) {
          write_indent(output, indent + 2);
          write_json_value(value.array_value[index], output, indent + 2);
          if (index + 1 < value.array_value.size()) {
            *output << ",";
          }
          *output << "\n";
        }
        write_indent(output, indent);
      }
      *output << "]";
      break;
    case JsonValue::Type::Object:
      *output << "{";
      if (!value.object_value.empty()) {
        *output << "\n";
        std::size_t written = 0;
        for (const auto& entry : value.object_value) {
          write_indent(output, indent + 2);
          write_json_escaped(entry.first, output);
          *output << ": ";
          write_json_value(entry.second, output, indent + 2);
          if (written + 1 < value.object_value.size()) {
            *output << ",";
          }
          *output << "\n";
          ++written;
        }
        write_indent(output, indent);
      }
      *output << "}";
      break;
  }
}

class JsonParser {
 public:
  explicit JsonParser(std::string_view source) : source_(source) {}

  bool parse(JsonValue* out, std::string* error) {
    skip_ws();
    if (!parse_value(out, error)) {
      return false;
    }
    skip_ws();
    if (index_ != source_.size()) {
      if (error != nullptr) {
        *error = "Unexpected trailing characters after JSON document.";
      }
      return false;
    }
    return true;
  }

 private:
  std::string_view source_;
  std::size_t index_ = 0;

  void skip_ws() {
    while (index_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[index_])) != 0) {
      ++index_;
    }
  }

  bool parse_value(JsonValue* out, std::string* error) {
    skip_ws();
    if (index_ >= source_.size()) {
      if (error != nullptr) {
        *error = "Unexpected end of JSON input.";
      }
      return false;
    }
    const char ch = source_[index_];
    if (ch == '"') {
      return parse_string(out, error);
    }
    if (ch == '{') {
      return parse_object(out, error);
    }
    if (ch == '[') {
      return parse_array(out, error);
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
    return parse_number(out, error);
  }

  bool parse_literal(std::string_view token, const JsonValue& node, JsonValue* out, std::string* error) {
    if (source_.substr(index_, token.size()) != token) {
      if (error != nullptr) {
        *error = "Invalid JSON literal.";
      }
      return false;
    }
    index_ += token.size();
    *out = node;
    return true;
  }

  bool parse_string(JsonValue* out, std::string* error) {
    if (source_[index_] != '"') {
      if (error != nullptr) {
        *error = "Expected string.";
      }
      return false;
    }
    ++index_;
    std::string value;
    while (index_ < source_.size()) {
      const char ch = source_[index_++];
      if (ch == '"') {
        *out = make_json_string(std::move(value));
        return true;
      }
      if (ch == '\\') {
        if (index_ >= source_.size()) {
          if (error != nullptr) {
            *error = "Unterminated JSON escape sequence.";
          }
          return false;
        }
        const char escaped = source_[index_++];
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            value.push_back(escaped);
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
              *error = "Unsupported JSON escape sequence.";
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

  bool parse_number(JsonValue* out, std::string* error) {
    const std::size_t start = index_;
    if (index_ >= source_.size()) {
      if (error != nullptr) {
        *error = "Unexpected end of input while parsing number.";
      }
      return false;
    }
    if (source_[index_] == '-') {
      ++index_;
    }
    while (index_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[index_])) != 0) {
      ++index_;
    }
    if (index_ < source_.size() && source_[index_] == '.') {
      ++index_;
      while (index_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[index_])) != 0) {
        ++index_;
      }
    }
    const std::string token(source_.substr(start, index_ - start));
    char* end_ptr = nullptr;
    const double value = std::strtod(token.c_str(), &end_ptr);
    if (token.empty() || end_ptr == token.c_str() || *end_ptr != '\0') {
      if (error != nullptr) {
        std::ostringstream message;
        message << "Invalid JSON number near byte " << start;
        if (start < source_.size()) {
          message << " ('" << source_[start] << "')";
        }
        if (!token.empty()) {
          message << " token=" << token;
        }
        *error = message.str();
      }
      return false;
    }
    *out = make_json_number(value);
    return true;
  }

  bool parse_array(JsonValue* out, std::string* error) {
    if (source_[index_] != '[') {
      if (error != nullptr) {
        *error = "Expected array.";
      }
      return false;
    }
    ++index_;
    JsonValue array = make_json_array();
    skip_ws();
    if (index_ < source_.size() && source_[index_] == ']') {
      ++index_;
      *out = std::move(array);
      return true;
    }
    while (index_ < source_.size()) {
      JsonValue item;
      if (!parse_value(&item, error)) {
        return false;
      }
      array.array_value.push_back(std::move(item));
      skip_ws();
      if (index_ >= source_.size()) {
        break;
      }
      if (source_[index_] == ']') {
        ++index_;
        *out = std::move(array);
        return true;
      }
      if (source_[index_] != ',') {
        if (error != nullptr) {
          *error = "Expected ',' or ']' in JSON array.";
        }
        return false;
      }
      ++index_;
      skip_ws();
    }
    if (error != nullptr) {
      *error = "Unterminated JSON array.";
    }
    return false;
  }

  bool parse_object(JsonValue* out, std::string* error) {
    if (source_[index_] != '{') {
      if (error != nullptr) {
        *error = "Expected object.";
      }
      return false;
    }
    ++index_;
    JsonValue object = make_json_object();
    skip_ws();
    if (index_ < source_.size() && source_[index_] == '}') {
      ++index_;
      *out = std::move(object);
      return true;
    }
    while (index_ < source_.size()) {
      JsonValue key;
      if (!parse_string(&key, error)) {
        return false;
      }
      skip_ws();
      if (index_ >= source_.size() || source_[index_] != ':') {
        if (error != nullptr) {
          *error = "Expected ':' in JSON object.";
        }
        return false;
      }
      ++index_;
      skip_ws();
      JsonValue value;
      if (!parse_value(&value, error)) {
        return false;
      }
      object.object_value.emplace(key.string_value, std::move(value));
      skip_ws();
      if (index_ >= source_.size()) {
        break;
      }
      if (source_[index_] == '}') {
        ++index_;
        *out = std::move(object);
        return true;
      }
      if (source_[index_] != ',') {
        if (error != nullptr) {
          *error = "Expected ',' or '}' in JSON object.";
        }
        return false;
      }
      ++index_;
      skip_ws();
    }
    if (error != nullptr) {
      *error = "Unterminated JSON object.";
    }
    return false;
  }
};

const JsonValue* object_get(const JsonValue& object, const std::string& key) {
  if (object.type != JsonValue::Type::Object) {
    return nullptr;
  }
  const auto it = object.object_value.find(key);
  return it == object.object_value.end() ? nullptr : &it->second;
}

std::string json_as_string(const JsonValue* node, std::string fallback = {}) {
  return node != nullptr && node->type == JsonValue::Type::String ? node->string_value : std::move(fallback);
}

int json_as_int(const JsonValue* node, int fallback = 0) {
  return node != nullptr && node->type == JsonValue::Type::Number ? static_cast<int>(std::lround(node->number_value))
                                                                  : fallback;
}

bool json_as_bool(const JsonValue* node, bool fallback = false) {
  return node != nullptr && node->type == JsonValue::Type::Bool ? node->bool_value : fallback;
}

std::vector<std::string> json_as_string_array(const JsonValue* node) {
  std::vector<std::string> values;
  if (node == nullptr || node->type != JsonValue::Type::Array) {
    return values;
  }
  for (const auto& entry : node->array_value) {
    if (entry.type == JsonValue::Type::String) {
      values.push_back(entry.string_value);
    }
  }
  return values;
}

std::map<std::string, std::string> json_as_string_map(const JsonValue* node) {
  std::map<std::string, std::string> values;
  if (node == nullptr || node->type != JsonValue::Type::Object) {
    return values;
  }
  for (const auto& entry : node->object_value) {
    if (entry.second.type == JsonValue::Type::String) {
      values.emplace(entry.first, entry.second.string_value);
    } else if (entry.second.type == JsonValue::Type::Number) {
      values.emplace(entry.first, std::to_string(static_cast<int>(std::lround(entry.second.number_value))));
    } else if (entry.second.type == JsonValue::Type::Bool) {
      values.emplace(entry.first, entry.second.bool_value ? "true" : "false");
    }
  }
  return values;
}

JsonValue string_array_to_json(const std::vector<std::string>& values) {
  std::vector<JsonValue> array;
  for (const auto& value : values) {
    array.push_back(make_json_string(value));
  }
  return make_json_array(std::move(array));
}

JsonValue string_map_to_json(const std::map<std::string, std::string>& values) {
  std::map<std::string, JsonValue> object;
  for (const auto& entry : values) {
    object.emplace(entry.first, make_json_string(entry.second));
  }
  return make_json_object(std::move(object));
}

bool is_valid_id(std::string_view id) {
  return !trim_copy(std::string(id)).empty();
}

const TileLayer* find_layer(const Area& area, LayerKind kind) {
  for (const auto& layer : area.layers) {
    if (layer.kind == kind) {
      return &layer;
    }
  }
  return nullptr;
}

TileLayer make_layer(std::string id, std::string name, LayerKind kind, int width, int height, char fill) {
  TileLayer layer;
  layer.id = std::move(id);
  layer.name = std::move(name);
  layer.kind = kind;
  layer.width = width;
  layer.height = height;
  layer.rows.assign(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), fill));
  return layer;
}

std::map<std::string, std::vector<std::string>> blank_sprite_frames() {
  std::map<std::string, std::vector<std::string>> frames;
  for (std::string direction : {"up", "right", "down", "left"}) {
    for (int frame = 1; frame <= 2; ++frame) {
      frames.emplace(direction + "_" + std::to_string(frame), std::vector<std::string>(16, std::string(16, '0')));
    }
  }
  return frames;
}

SpriteAsset make_sprite(std::string id, std::string name, bool monster) {
  SpriteAsset sprite;
  sprite.id = std::move(id);
  sprite.name = std::move(name);
  sprite.monster = monster;
  sprite.frames = blank_sprite_frames();
  return sprite;
}

JsonValue rect_to_json(const Rect& rect) {
  return make_json_object({
      {"x", make_json_number(rect.x)},
      {"y", make_json_number(rect.y)},
      {"width", make_json_number(rect.width)},
      {"height", make_json_number(rect.height)},
  });
}

Rect rect_from_json(const JsonValue& node) {
  Rect rect;
  rect.x = json_as_int(object_get(node, "x"), 0);
  rect.y = json_as_int(object_get(node, "y"), 0);
  rect.width = std::max(1, json_as_int(object_get(node, "width"), 1));
  rect.height = std::max(1, json_as_int(object_get(node, "height"), 1));
  return rect;
}

void push_issue(ValidationReport* report, ValidationIssue::Severity severity, std::string path, std::string message) {
  if (report == nullptr) {
    return;
  }
  report->issues.push_back({severity, std::move(path), std::move(message)});
}

template <typename T, typename Accessor>
void validate_unique_ids(const std::vector<T>& items,
                         std::string_view group_name,
                         ValidationReport* report,
                         Accessor accessor) {
  std::set<std::string> seen;
  for (const auto& item : items) {
    const std::string id = accessor(item);
    if (!is_valid_id(id)) {
      push_issue(report, ValidationIssue::Severity::Error, std::string(group_name), "Encountered an empty or invalid id.");
      continue;
    }
    if (!seen.insert(id).second) {
      push_issue(report, ValidationIssue::Severity::Error, std::string(group_name) + "." + id, "Duplicate id detected.");
    }
  }
}

}  // namespace

bool ValidationReport::has_errors() const {
  return std::any_of(issues.begin(), issues.end(), [](const ValidationIssue& issue) {
    return issue.severity == ValidationIssue::Severity::Error;
  });
}

int ValidationReport::error_count() const {
  return static_cast<int>(std::count_if(issues.begin(), issues.end(), [](const ValidationIssue& issue) {
    return issue.severity == ValidationIssue::Severity::Error;
  }));
}

int ValidationReport::warning_count() const {
  return static_cast<int>(std::count_if(issues.begin(), issues.end(), [](const ValidationIssue& issue) {
    return issue.severity == ValidationIssue::Severity::Warning;
  }));
}

std::string facing_name(Facing facing) {
  switch (facing) {
    case Facing::Up: return "UP";
    case Facing::Right: return "RIGHT";
    case Facing::Left: return "LEFT";
    default: return "DOWN";
  }
}

Facing facing_from_string(std::string_view value) {
  const std::string upper = uppercase(std::string(value));
  if (upper == "UP") {
    return Facing::Up;
  }
  if (upper == "RIGHT") {
    return Facing::Right;
  }
  if (upper == "LEFT") {
    return Facing::Left;
  }
  return Facing::Down;
}

std::string sprite_role_name(SpriteRole role) {
  switch (role) {
    case SpriteRole::Prior: return "PRIOR";
    case SpriteRole::Sister: return "SISTER";
    case SpriteRole::Fisher: return "FISHER";
    case SpriteRole::Merchant: return "MERCHANT";
    case SpriteRole::Child: return "CHILD";
    case SpriteRole::Elder: return "ELDER";
    case SpriteRole::Watchman: return "WATCHMAN";
    default: return "MONK";
  }
}

SpriteRole sprite_role_from_string(std::string_view value) {
  const std::string upper = uppercase(std::string(value));
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

std::string layer_kind_name(LayerKind kind) {
  switch (kind) {
    case LayerKind::Wall: return "WALL";
    case LayerKind::Fringe: return "FRINGE";
    case LayerKind::Object: return "OBJECT";
    case LayerKind::Trigger: return "TRIGGER";
    default: return "GROUND";
  }
}

LayerKind layer_kind_from_string(std::string_view value) {
  const std::string upper = uppercase(std::string(value));
  if (upper == "WALL") {
    return LayerKind::Wall;
  }
  if (upper == "FRINGE") {
    return LayerKind::Fringe;
  }
  if (upper == "OBJECT") {
    return LayerKind::Object;
  }
  if (upper == "TRIGGER") {
    return LayerKind::Trigger;
  }
  return LayerKind::Ground;
}

std::string entity_kind_name(EntityKind kind) {
  switch (kind) {
    case EntityKind::Monster: return "MONSTER";
    case EntityKind::Prop: return "PROP";
    case EntityKind::Pickup: return "PICKUP";
    case EntityKind::Chest: return "CHEST";
    case EntityKind::Shopkeeper: return "SHOPKEEPER";
    case EntityKind::Crop: return "CROP";
    default: return "NPC";
  }
}

EntityKind entity_kind_from_string(std::string_view value) {
  const std::string upper = uppercase(std::string(value));
  if (upper == "MONSTER") {
    return EntityKind::Monster;
  }
  if (upper == "PROP") {
    return EntityKind::Prop;
  }
  if (upper == "PICKUP") {
    return EntityKind::Pickup;
  }
  if (upper == "CHEST") {
    return EntityKind::Chest;
  }
  if (upper == "SHOPKEEPER") {
    return EntityKind::Shopkeeper;
  }
  if (upper == "CROP") {
    return EntityKind::Crop;
  }
  return EntityKind::Npc;
}

std::string requirement_kind_name(RequirementKind kind) {
  switch (kind) {
    case RequirementKind::ReachArea: return "REACH_AREA";
    case RequirementKind::TalkToEntity: return "TALK_TO_ENTITY";
    case RequirementKind::DefeatEntity: return "DEFEAT_ENTITY";
    case RequirementKind::CollectItem: return "COLLECT_ITEM";
    case RequirementKind::HasVirtue: return "HAS_VIRTUE";
    default: return "CUSTOM";
  }
}

RequirementKind requirement_kind_from_string(std::string_view value) {
  const std::string upper = uppercase(std::string(value));
  if (upper == "REACH_AREA" || upper == "GO HERE") {
    return RequirementKind::ReachArea;
  }
  if (upper == "TALK_TO_ENTITY" || upper == "TALK") {
    return RequirementKind::TalkToEntity;
  }
  if (upper == "DEFEAT_ENTITY" || upper == "SLAY") {
    return RequirementKind::DefeatEntity;
  }
  if (upper == "COLLECT_ITEM" || upper == "COLLECT") {
    return RequirementKind::CollectItem;
  }
  if (upper == "HAS_VIRTUE") {
    return RequirementKind::HasVirtue;
  }
  return RequirementKind::Custom;
}

std::string action_kind_name(ActionKind kind) {
  switch (kind) {
    case ActionKind::GiveItem: return "GIVE_ITEM";
    case ActionKind::GiveCoins: return "GIVE_COINS";
    case ActionKind::SetFlag: return "SET_FLAG";
    case ActionKind::CompleteQuest: return "COMPLETE_QUEST";
    case ActionKind::UnlockArea: return "UNLOCK_AREA";
    case ActionKind::RunDialogue: return "RUN_DIALOGUE";
    default: return "CUSTOM";
  }
}

ActionKind action_kind_from_string(std::string_view value) {
  const std::string upper = uppercase(std::string(value));
  if (upper == "GIVE_ITEM") {
    return ActionKind::GiveItem;
  }
  if (upper == "GIVE_COINS") {
    return ActionKind::GiveCoins;
  }
  if (upper == "SET_FLAG") {
    return ActionKind::SetFlag;
  }
  if (upper == "COMPLETE_QUEST") {
    return ActionKind::CompleteQuest;
  }
  if (upper == "UNLOCK_AREA") {
    return ActionKind::UnlockArea;
  }
  if (upper == "RUN_DIALOGUE") {
    return ActionKind::RunDialogue;
  }
  return ActionKind::Custom;
}

const Area* find_area(const Project& project, std::string_view id) {
  for (const auto& area : project.areas) {
    if (area.id == id) {
      return &area;
    }
  }
  return nullptr;
}

const SceneEntity* find_entity(const Project& project, std::string_view id) {
  for (const auto& area : project.areas) {
    for (const auto& entity : area.entities) {
      if (entity.id == id) {
        return &entity;
      }
    }
  }
  return nullptr;
}

const DialogueGraph* find_dialogue(const Project& project, std::string_view id) {
  for (const auto& dialogue : project.dialogues) {
    if (dialogue.id == id) {
      return &dialogue;
    }
  }
  return nullptr;
}

const ItemDefinition* find_item(const Project& project, std::string_view id) {
  for (const auto& item : project.items) {
    if (item.id == id) {
      return &item;
    }
  }
  return nullptr;
}

const ShopDefinition* find_shop(const Project& project, std::string_view id) {
  for (const auto& shop : project.shops) {
    if (shop.id == id) {
      return &shop;
    }
  }
  return nullptr;
}

const CropDefinition* find_crop(const Project& project, std::string_view id) {
  for (const auto& crop : project.crops) {
    if (crop.id == id) {
      return &crop;
    }
  }
  return nullptr;
}

const EntityArchetype* find_archetype(const Project& project, std::string_view id) {
  for (const auto& archetype : project.archetypes) {
    if (archetype.id == id) {
      return &archetype;
    }
  }
  return nullptr;
}

const SpriteAsset* find_sprite(const Project& project, std::string_view id) {
  for (const auto& sprite : project.sprites) {
    if (sprite.id == id) {
      return &sprite;
    }
  }
  return nullptr;
}

namespace {

JsonValue tile_definition_to_json(const TileDefinition& tile) {
  return make_json_object({
      {"id", make_json_string(tile.id)},
      {"name", make_json_string(tile.name)},
      {"description", make_json_string(tile.description)},
      {"symbol", make_json_string(std::string(1, tile.symbol))},
      {"walkable", make_json_bool(tile.walkable)},
      {"opaque", make_json_bool(tile.opaque)},
      {"default_layer", make_json_string(layer_kind_name(tile.default_layer))},
      {"tags", string_array_to_json(tile.tags)},
      {"properties", string_map_to_json(tile.properties)},
  });
}

bool load_tile_definition_from_json(const JsonValue& node, TileDefinition* tile) {
  if (tile == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  tile->id = json_as_string(object_get(node, "id"), "tile");
  tile->name = json_as_string(object_get(node, "name"), "TILE");
  tile->description = json_as_string(object_get(node, "description"), "");
  const std::string symbol = json_as_string(object_get(node, "symbol"), ".");
  tile->symbol = symbol.empty() ? '.' : symbol.front();
  tile->walkable = json_as_bool(object_get(node, "walkable"), true);
  tile->opaque = json_as_bool(object_get(node, "opaque"), false);
  tile->default_layer = layer_kind_from_string(json_as_string(object_get(node, "default_layer"), "GROUND"));
  tile->tags = json_as_string_array(object_get(node, "tags"));
  tile->properties = json_as_string_map(object_get(node, "properties"));
  return true;
}

JsonValue stamp_to_json(const PatternStamp& stamp) {
  std::vector<JsonValue> rows;
  for (const auto& row : stamp.rows) {
    rows.push_back(make_json_string(row));
  }
  return make_json_object({
      {"id", make_json_string(stamp.id)},
      {"name", make_json_string(stamp.name)},
      {"rows", make_json_array(std::move(rows))},
  });
}

bool load_stamp_from_json(const JsonValue& node, PatternStamp* stamp) {
  if (stamp == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  stamp->id = json_as_string(object_get(node, "id"), "stamp");
  stamp->name = json_as_string(object_get(node, "name"), "STAMP");
  stamp->rows = json_as_string_array(object_get(node, "rows"));
  if (stamp->rows.empty()) {
    stamp->rows = json_as_string_array(object_get(node, "tiles"));
  }
  if (stamp->rows.empty()) {
    stamp->rows.push_back(".");
  }
  return true;
}

JsonValue sprite_to_json(const SpriteAsset& sprite) {
  std::map<std::string, JsonValue> frames;
  for (const auto& entry : sprite.frames) {
    std::vector<JsonValue> rows;
    for (const auto& row : entry.second) {
      rows.push_back(make_json_string(row));
    }
    frames.emplace(entry.first, make_json_array(std::move(rows)));
  }
  return make_json_object({
      {"id", make_json_string(sprite.id)},
      {"name", make_json_string(sprite.name)},
      {"monster", make_json_bool(sprite.monster)},
      {"frames", make_json_object(std::move(frames))},
  });
}

bool load_sprite_from_json(const JsonValue& node, SpriteAsset* sprite) {
  if (sprite == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  *sprite = make_sprite(json_as_string(object_get(node, "id"), "sprite"),
                        json_as_string(object_get(node, "name"), "SPRITE"),
                        json_as_bool(object_get(node, "monster"), false));
  const JsonValue* frames = object_get(node, "frames");
  if (frames != nullptr && frames->type == JsonValue::Type::Object) {
    for (auto& entry : sprite->frames) {
      entry.second = json_as_string_array(object_get(*frames, entry.first));
      if (entry.second.empty()) {
        entry.second.assign(16, std::string(16, '0'));
      }
    }
  }
  return true;
}

JsonValue archetype_to_json(const EntityArchetype& archetype) {
  return make_json_object({
      {"id", make_json_string(archetype.id)},
      {"name", make_json_string(archetype.name)},
      {"kind", make_json_string(entity_kind_name(archetype.kind))},
      {"sprite_id", make_json_string(archetype.sprite_id)},
      {"role", make_json_string(sprite_role_name(archetype.role))},
      {"solid", make_json_bool(archetype.solid)},
      {"aggressive", make_json_bool(archetype.aggressive)},
      {"max_hp", make_json_number(archetype.max_hp)},
      {"attack", make_json_number(archetype.attack)},
      {"dialogue_id", make_json_string(archetype.dialogue_id)},
      {"shop_id", make_json_string(archetype.shop_id)},
      {"tags", string_array_to_json(archetype.tags)},
      {"properties", string_map_to_json(archetype.properties)},
  });
}

bool load_archetype_from_json(const JsonValue& node, EntityArchetype* archetype) {
  if (archetype == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  archetype->id = json_as_string(object_get(node, "id"), "archetype");
  archetype->name = json_as_string(object_get(node, "name"), "ARCHETYPE");
  archetype->kind = entity_kind_from_string(json_as_string(object_get(node, "kind"), "NPC"));
  archetype->sprite_id = json_as_string(object_get(node, "sprite_id"), "");
  archetype->role = sprite_role_from_string(json_as_string(object_get(node, "role"), "MONK"));
  archetype->solid = json_as_bool(object_get(node, "solid"), true);
  archetype->aggressive = json_as_bool(object_get(node, "aggressive"), false);
  archetype->max_hp = std::max(0, json_as_int(object_get(node, "max_hp"), 0));
  archetype->attack = std::max(0, json_as_int(object_get(node, "attack"), 0));
  archetype->dialogue_id = json_as_string(object_get(node, "dialogue_id"), "");
  archetype->shop_id = json_as_string(object_get(node, "shop_id"), "");
  archetype->tags = json_as_string_array(object_get(node, "tags"));
  archetype->properties = json_as_string_map(object_get(node, "properties"));
  return true;
}

JsonValue entity_to_json(const SceneEntity& entity) {
  return make_json_object({
      {"id", make_json_string(entity.id)},
      {"name", make_json_string(entity.name)},
      {"kind", make_json_string(entity_kind_name(entity.kind))},
      {"archetype_id", make_json_string(entity.archetype_id)},
      {"sprite_id", make_json_string(entity.sprite_id)},
      {"role", make_json_string(sprite_role_name(entity.role))},
      {"x", make_json_number(entity.x)},
      {"y", make_json_number(entity.y)},
      {"facing", make_json_string(facing_name(entity.facing))},
      {"solid", make_json_bool(entity.solid)},
      {"aggressive", make_json_bool(entity.aggressive)},
      {"max_hp", make_json_number(entity.max_hp)},
      {"attack", make_json_number(entity.attack)},
      {"dialogue_id", make_json_string(entity.dialogue_id)},
      {"dialogue_text", make_json_string(entity.dialogue_text)},
      {"shop_id", make_json_string(entity.shop_id)},
      {"tags", string_array_to_json(entity.tags)},
      {"properties", string_map_to_json(entity.properties)},
  });
}

bool load_entity_from_json(const JsonValue& node, SceneEntity* entity) {
  if (entity == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  entity->id = json_as_string(object_get(node, "id"), "entity");
  entity->name = json_as_string(object_get(node, "name"), "ENTITY");
  entity->kind = entity_kind_from_string(json_as_string(object_get(node, "kind"), "NPC"));
  entity->archetype_id = json_as_string(object_get(node, "archetype_id"), "");
  entity->sprite_id = json_as_string(object_get(node, "sprite_id"), "");
  entity->role = sprite_role_from_string(json_as_string(object_get(node, "role"), "MONK"));
  entity->x = json_as_int(object_get(node, "x"), 0);
  entity->y = json_as_int(object_get(node, "y"), 0);
  entity->facing = facing_from_string(json_as_string(object_get(node, "facing"), "DOWN"));
  entity->solid = json_as_bool(object_get(node, "solid"), true);
  entity->aggressive = json_as_bool(object_get(node, "aggressive"), false);
  entity->max_hp = std::max(0, json_as_int(object_get(node, "max_hp"), 0));
  entity->attack = std::max(0, json_as_int(object_get(node, "attack"), 0));
  entity->dialogue_id = json_as_string(object_get(node, "dialogue_id"), "");
  entity->dialogue_text = json_as_string(object_get(node, "dialogue_text"), "");
  entity->shop_id = json_as_string(object_get(node, "shop_id"), "");
  entity->tags = json_as_string_array(object_get(node, "tags"));
  entity->properties = json_as_string_map(object_get(node, "properties"));
  return true;
}

JsonValue dialogue_choice_to_json(const DialogueChoice& choice) {
  return make_json_object({
      {"text", make_json_string(choice.text)},
      {"next_node_id", make_json_string(choice.next_node_id)},
      {"action_id", make_json_string(choice.action_id)},
  });
}

bool load_dialogue_choice_from_json(const JsonValue& node, DialogueChoice* choice) {
  if (choice == nullptr || node.type != JsonValue::Type::Object) {
    return false;
  }
  choice->text = json_as_string(object_get(node, "text"), "");
  choice->next_node_id = json_as_string(object_get(node, "next_node_id"), "");
  choice->action_id = json_as_string(object_get(node, "action_id"), "");
  return true;
}

JsonValue dialogue_node_to_json(const DialogueNode& node) {
  std::vector<JsonValue> choices;
  for (const auto& choice : node.choices) {
    choices.push_back(dialogue_choice_to_json(choice));
  }
  return make_json_object({
      {"id", make_json_string(node.id)},
      {"speaker", make_json_string(node.speaker)},
      {"text", make_json_string(node.text)},
      {"choices", make_json_array(std::move(choices))},
      {"ends_conversation", make_json_bool(node.ends_conversation)},
  });
}

bool load_dialogue_node_from_json(const JsonValue& json, DialogueNode* node) {
  if (node == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  node->id = json_as_string(object_get(json, "id"), "node");
  node->speaker = json_as_string(object_get(json, "speaker"), "");
  node->text = json_as_string(object_get(json, "text"), "");
  node->choices.clear();
  const JsonValue* choices = object_get(json, "choices");
  if (choices != nullptr && choices->type == JsonValue::Type::Array) {
    for (const auto& choice_json : choices->array_value) {
      DialogueChoice choice;
      if (load_dialogue_choice_from_json(choice_json, &choice)) {
        node->choices.push_back(std::move(choice));
      }
    }
  }
  node->ends_conversation = json_as_bool(object_get(json, "ends_conversation"), false);
  return true;
}

JsonValue dialogue_graph_to_json(const DialogueGraph& graph) {
  std::vector<JsonValue> nodes;
  for (const auto& node : graph.nodes) {
    nodes.push_back(dialogue_node_to_json(node));
  }
  return make_json_object({
      {"id", make_json_string(graph.id)},
      {"name", make_json_string(graph.name)},
      {"start_node_id", make_json_string(graph.start_node_id)},
      {"nodes", make_json_array(std::move(nodes))},
  });
}

bool load_dialogue_graph_from_json(const JsonValue& json, DialogueGraph* graph) {
  if (graph == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  graph->id = json_as_string(object_get(json, "id"), "dialogue");
  graph->name = json_as_string(object_get(json, "name"), "DIALOGUE");
  graph->start_node_id = json_as_string(object_get(json, "start_node_id"), "");
  graph->nodes.clear();
  const JsonValue* nodes = object_get(json, "nodes");
  if (nodes != nullptr && nodes->type == JsonValue::Type::Array) {
    for (const auto& node_json : nodes->array_value) {
      DialogueNode node;
      if (load_dialogue_node_from_json(node_json, &node)) {
        graph->nodes.push_back(std::move(node));
      }
    }
  }
  return true;
}

JsonValue requirement_to_json(const QuestRequirement& requirement) {
  return make_json_object({
      {"id", make_json_string(requirement.id)},
      {"kind", make_json_string(requirement_kind_name(requirement.kind))},
      {"target_id", make_json_string(requirement.target_id)},
      {"area_id", make_json_string(requirement.area_id)},
      {"x", make_json_number(requirement.x)},
      {"y", make_json_number(requirement.y)},
      {"quantity", make_json_number(requirement.quantity)},
      {"description", make_json_string(requirement.description)},
  });
}

bool load_requirement_from_json(const JsonValue& json, QuestRequirement* requirement) {
  if (requirement == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  requirement->id = json_as_string(object_get(json, "id"), "requirement");
  requirement->kind = requirement_kind_from_string(
      json_as_string(object_get(json, "kind"), json_as_string(object_get(json, "type"), "CUSTOM")));
  requirement->target_id = json_as_string(object_get(json, "target_id"), "");
  requirement->area_id = json_as_string(object_get(json, "area_id"), "");
  requirement->x = json_as_int(object_get(json, "x"), 0);
  requirement->y = json_as_int(object_get(json, "y"), 0);
  requirement->quantity = std::max(1, json_as_int(object_get(json, "quantity"), 1));
  requirement->description = json_as_string(object_get(json, "description"), "");
  return true;
}

JsonValue action_to_json(const QuestAction& action) {
  return make_json_object({
      {"id", make_json_string(action.id)},
      {"kind", make_json_string(action_kind_name(action.kind))},
      {"target_id", make_json_string(action.target_id)},
      {"amount", make_json_number(action.amount)},
      {"payload", make_json_string(action.payload)},
      {"description", make_json_string(action.description)},
  });
}

bool load_action_from_json(const JsonValue& json, QuestAction* action) {
  if (action == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  action->id = json_as_string(object_get(json, "id"), "action");
  action->kind = action_kind_from_string(json_as_string(object_get(json, "kind"), "CUSTOM"));
  action->target_id = json_as_string(object_get(json, "target_id"), json_as_string(object_get(json, "item_id"), ""));
  action->amount = json_as_int(object_get(json, "amount"), json_as_int(object_get(json, "coins"), 0));
  action->payload = json_as_string(object_get(json, "payload"), json_as_string(object_get(json, "unlock_sprite_id"), ""));
  action->description = json_as_string(object_get(json, "description"), "");
  return true;
}

JsonValue quest_stage_to_json(const QuestStage& stage) {
  std::vector<JsonValue> requirements;
  std::vector<JsonValue> actions;
  for (const auto& requirement : stage.requirements) {
    requirements.push_back(requirement_to_json(requirement));
  }
  for (const auto& action : stage.on_complete) {
    actions.push_back(action_to_json(action));
  }
  return make_json_object({
      {"id", make_json_string(stage.id)},
      {"text", make_json_string(stage.text)},
      {"requirements", make_json_array(std::move(requirements))},
      {"on_complete", make_json_array(std::move(actions))},
  });
}

bool load_quest_stage_from_json(const JsonValue& json, QuestStage* stage) {
  if (stage == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  stage->id = json_as_string(object_get(json, "id"), "stage");
  stage->text = json_as_string(object_get(json, "text"), "");
  stage->requirements.clear();
  const JsonValue* requirements = object_get(json, "requirements");
  if (requirements != nullptr && requirements->type == JsonValue::Type::Array) {
    for (const auto& requirement_json : requirements->array_value) {
      QuestRequirement requirement;
      if (load_requirement_from_json(requirement_json, &requirement)) {
        stage->requirements.push_back(std::move(requirement));
      }
    }
  }
  stage->on_complete.clear();
  const JsonValue* actions = object_get(json, "on_complete");
  if (actions != nullptr && actions->type == JsonValue::Type::Array) {
    for (const auto& action_json : actions->array_value) {
      QuestAction action;
      if (load_action_from_json(action_json, &action)) {
        stage->on_complete.push_back(std::move(action));
      }
    }
  }
  return true;
}

JsonValue quest_to_json(const Quest& quest) {
  std::vector<JsonValue> stages;
  std::vector<JsonValue> rewards;
  for (const auto& stage : quest.stages) {
    stages.push_back(quest_stage_to_json(stage));
  }
  for (const auto& reward : quest.rewards) {
    rewards.push_back(action_to_json(reward));
  }
  return make_json_object({
      {"id", make_json_string(quest.id)},
      {"title", make_json_string(quest.title)},
      {"summary", make_json_string(quest.summary)},
      {"giver_entity_id", make_json_string(quest.giver_entity_id)},
      {"start_dialogue", make_json_string(quest.start_dialogue)},
      {"completion_dialogue", make_json_string(quest.completion_dialogue)},
      {"stages", make_json_array(std::move(stages))},
      {"rewards", make_json_array(std::move(rewards))},
  });
}

bool load_quest_from_json(const JsonValue& json, Quest* quest) {
  if (quest == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  quest->id = json_as_string(object_get(json, "id"), "quest");
  quest->title = json_as_string(object_get(json, "title"), "QUEST");
  quest->summary = json_as_string(object_get(json, "summary"), "");
  quest->giver_entity_id = json_as_string(object_get(json, "giver_entity_id"),
                                          json_as_string(object_get(json, "quest_giver_id"), ""));
  quest->start_dialogue = json_as_string(object_get(json, "start_dialogue"), "");
  quest->completion_dialogue = json_as_string(object_get(json, "completion_dialogue"), "");
  quest->stages.clear();
  const JsonValue* stages = object_get(json, "stages");
  if (stages != nullptr && stages->type == JsonValue::Type::Array) {
    for (const auto& stage_json : stages->array_value) {
      QuestStage stage;
      if (load_quest_stage_from_json(stage_json, &stage)) {
        quest->stages.push_back(std::move(stage));
      }
    }
  }
  if (quest->stages.empty()) {
    QuestStage legacy_stage;
    legacy_stage.id = "stage_1";
    legacy_stage.text = quest->summary;
    const JsonValue* requirements = object_get(json, "requirements");
    if (requirements != nullptr && requirements->type == JsonValue::Type::Array) {
      for (const auto& requirement_json : requirements->array_value) {
        QuestRequirement requirement;
        if (load_requirement_from_json(requirement_json, &requirement)) {
          legacy_stage.requirements.push_back(std::move(requirement));
        }
      }
    }
    if (!legacy_stage.text.empty() || !legacy_stage.requirements.empty()) {
      quest->stages.push_back(std::move(legacy_stage));
    }
  }
  quest->rewards.clear();
  const JsonValue* rewards = object_get(json, "rewards");
  if (rewards != nullptr && rewards->type == JsonValue::Type::Array) {
    for (const auto& reward_json : rewards->array_value) {
      QuestAction action;
      if (load_action_from_json(reward_json, &action)) {
        quest->rewards.push_back(std::move(action));
      }
    }
  }
  return true;
}

JsonValue item_to_json(const ItemDefinition& item) {
  return make_json_object({
      {"id", make_json_string(item.id)},
      {"name", make_json_string(item.name)},
      {"description", make_json_string(item.description)},
      {"base_price", make_json_number(item.base_price)},
      {"heal_amount", make_json_number(item.heal_amount)},
      {"consumable", make_json_bool(item.consumable)},
      {"placeable", make_json_bool(item.placeable)},
      {"placement_archetype_id", make_json_string(item.placement_archetype_id)},
      {"tags", string_array_to_json(item.tags)},
  });
}

bool load_item_from_json(const JsonValue& json, ItemDefinition* item) {
  if (item == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  item->id = json_as_string(object_get(json, "id"), "item");
  item->name = json_as_string(object_get(json, "name"), "ITEM");
  item->description = json_as_string(object_get(json, "description"), "");
  item->base_price = std::max(0, json_as_int(object_get(json, "base_price"), json_as_int(object_get(json, "price"), 0)));
  item->heal_amount = std::max(0, json_as_int(object_get(json, "heal_amount"), 0));
  item->consumable = json_as_bool(object_get(json, "consumable"), false);
  item->placeable = json_as_bool(object_get(json, "placeable"), false);
  item->placement_archetype_id = json_as_string(object_get(json, "placement_archetype_id"), "");
  item->tags = json_as_string_array(object_get(json, "tags"));
  return true;
}

JsonValue shop_listing_to_json(const ShopListing& listing) {
  return make_json_object({
      {"item_id", make_json_string(listing.item_id)},
      {"price", make_json_number(listing.price)},
      {"stock", make_json_number(listing.stock)},
  });
}

bool load_shop_listing_from_json(const JsonValue& json, ShopListing* listing) {
  if (listing == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  listing->item_id = json_as_string(object_get(json, "item_id"), "");
  listing->price = std::max(0, json_as_int(object_get(json, "price"), 0));
  listing->stock = json_as_int(object_get(json, "stock"), -1);
  return true;
}

JsonValue shop_to_json(const ShopDefinition& shop) {
  std::vector<JsonValue> listings;
  for (const auto& listing : shop.listings) {
    listings.push_back(shop_listing_to_json(listing));
  }
  return make_json_object({
      {"id", make_json_string(shop.id)},
      {"name", make_json_string(shop.name)},
      {"keeper_entity_id", make_json_string(shop.keeper_entity_id)},
      {"listings", make_json_array(std::move(listings))},
  });
}

bool load_shop_from_json(const JsonValue& json, ShopDefinition* shop) {
  if (shop == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  shop->id = json_as_string(object_get(json, "id"), "shop");
  shop->name = json_as_string(object_get(json, "name"), "SHOP");
  shop->keeper_entity_id = json_as_string(object_get(json, "keeper_entity_id"), "");
  shop->listings.clear();
  const JsonValue* listings = object_get(json, "listings");
  if (listings != nullptr && listings->type == JsonValue::Type::Array) {
    for (const auto& listing_json : listings->array_value) {
      ShopListing listing;
      if (load_shop_listing_from_json(listing_json, &listing)) {
        shop->listings.push_back(std::move(listing));
      }
    }
  }
  return true;
}

JsonValue crop_to_json(const CropDefinition& crop) {
  return make_json_object({
      {"id", make_json_string(crop.id)},
      {"name", make_json_string(crop.name)},
      {"seed_item_id", make_json_string(crop.seed_item_id)},
      {"produce_item_id", make_json_string(crop.produce_item_id)},
      {"growth_days", make_json_number(crop.growth_days)},
      {"stage_tiles", string_array_to_json(crop.stage_tiles)},
  });
}

bool load_crop_from_json(const JsonValue& json, CropDefinition* crop) {
  if (crop == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  crop->id = json_as_string(object_get(json, "id"), "crop");
  crop->name = json_as_string(object_get(json, "name"), "CROP");
  crop->seed_item_id = json_as_string(object_get(json, "seed_item_id"), "");
  crop->produce_item_id = json_as_string(object_get(json, "produce_item_id"), "");
  crop->growth_days = std::max(1, json_as_int(object_get(json, "growth_days"), 1));
  crop->stage_tiles = json_as_string_array(object_get(json, "stage_tiles"));
  return true;
}

JsonValue layer_to_json(const TileLayer& layer) {
  std::vector<JsonValue> rows;
  for (const auto& row : layer.rows) {
    rows.push_back(make_json_string(row));
  }
  return make_json_object({
      {"id", make_json_string(layer.id)},
      {"name", make_json_string(layer.name)},
      {"kind", make_json_string(layer_kind_name(layer.kind))},
      {"width", make_json_number(layer.width)},
      {"height", make_json_number(layer.height)},
      {"visible", make_json_bool(layer.visible)},
      {"rows", make_json_array(std::move(rows))},
      {"properties", string_map_to_json(layer.properties)},
  });
}

bool load_layer_from_json(const JsonValue& json, TileLayer* layer) {
  if (layer == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  layer->id = json_as_string(object_get(json, "id"), "layer");
  layer->name = json_as_string(object_get(json, "name"), "LAYER");
  layer->kind = layer_kind_from_string(json_as_string(object_get(json, "kind"), "GROUND"));
  layer->width = std::max(1, json_as_int(object_get(json, "width"), 1));
  layer->height = std::max(1, json_as_int(object_get(json, "height"), 1));
  layer->visible = json_as_bool(object_get(json, "visible"), true);
  layer->rows = json_as_string_array(object_get(json, "rows"));
  if (layer->rows.empty()) {
    layer->rows = json_as_string_array(object_get(json, "tiles"));
  }
  if (layer->rows.empty()) {
    layer->rows.assign(static_cast<std::size_t>(layer->height), std::string(static_cast<std::size_t>(layer->width), '.'));
  }
  if (static_cast<int>(layer->rows.size()) < layer->height) {
    layer->rows.resize(static_cast<std::size_t>(layer->height), std::string(static_cast<std::size_t>(layer->width), '.'));
  } else if (static_cast<int>(layer->rows.size()) > layer->height) {
    layer->rows.resize(static_cast<std::size_t>(layer->height));
  }
  for (auto& row : layer->rows) {
    if (static_cast<int>(row.size()) < layer->width) {
      row.append(static_cast<std::size_t>(layer->width - static_cast<int>(row.size())), '.');
    } else if (static_cast<int>(row.size()) > layer->width) {
      row.resize(static_cast<std::size_t>(layer->width));
    }
  }
  layer->properties = json_as_string_map(object_get(json, "properties"));
  return true;
}

JsonValue warp_to_json(const Warp& warp) {
  return make_json_object({
      {"id", make_json_string(warp.id)},
      {"label", make_json_string(warp.label)},
      {"bounds", rect_to_json(warp.bounds)},
      {"target_area", make_json_string(warp.target_area)},
      {"target_warp", make_json_string(warp.target_warp)},
      {"target_x", make_json_number(warp.target_x)},
      {"target_y", make_json_number(warp.target_y)},
      {"target_facing", make_json_string(facing_name(warp.target_facing))},
  });
}

bool load_warp_from_json(const JsonValue& json, Warp* warp) {
  if (warp == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  warp->id = json_as_string(object_get(json, "id"), "warp");
  warp->label = json_as_string(object_get(json, "label"), "WARP");
  const JsonValue* bounds = object_get(json, "bounds");
  if (bounds != nullptr && bounds->type == JsonValue::Type::Object) {
    warp->bounds = rect_from_json(*bounds);
  } else {
    warp->bounds.x = json_as_int(object_get(json, "x"), 0);
    warp->bounds.y = json_as_int(object_get(json, "y"), 0);
    warp->bounds.width = std::max(1, json_as_int(object_get(json, "width"), 1));
    warp->bounds.height = std::max(1, json_as_int(object_get(json, "height"), 1));
  }
  warp->target_area = json_as_string(object_get(json, "target_area"), "");
  warp->target_warp = json_as_string(object_get(json, "target_warp"), "");
  warp->target_x = json_as_int(object_get(json, "target_x"), 0);
  warp->target_y = json_as_int(object_get(json, "target_y"), 0);
  warp->target_facing = facing_from_string(json_as_string(object_get(json, "target_facing"), "DOWN"));
  return true;
}

JsonValue area_to_json(const Area& area) {
  std::vector<JsonValue> layers;
  std::vector<JsonValue> warps;
  std::vector<JsonValue> entities;
  for (const auto& layer : area.layers) {
    layers.push_back(layer_to_json(layer));
  }
  for (const auto& warp : area.warps) {
    warps.push_back(warp_to_json(warp));
  }
  for (const auto& entity : area.entities) {
    entities.push_back(entity_to_json(entity));
  }
  return make_json_object({
      {"id", make_json_string(area.id)},
      {"name", make_json_string(area.name)},
      {"indoor", make_json_bool(area.indoor)},
      {"player_tillable", make_json_bool(area.player_tillable)},
      {"width", make_json_number(area.width)},
      {"height", make_json_number(area.height)},
      {"layers", make_json_array(std::move(layers))},
      {"warps", make_json_array(std::move(warps))},
      {"entities", make_json_array(std::move(entities))},
      {"properties", string_map_to_json(area.properties)},
  });
}

bool load_legacy_area_from_json(const JsonValue& json, Area* area) {
  if (area == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  area->id = json_as_string(object_get(json, "id"), "area");
  area->name = json_as_string(object_get(json, "name"), "AREA");
  area->indoor = json_as_bool(object_get(json, "indoor"), false);
  area->player_tillable = json_as_bool(object_get(json, "player_tillable"), false);
  area->width = std::max(1, json_as_int(object_get(json, "width"), 1));
  area->height = std::max(1, json_as_int(object_get(json, "height"), 1));
  area->properties.clear();
  area->layers.clear();
  const std::vector<std::string> tiles = json_as_string_array(object_get(json, "tiles"));
  if (!tiles.empty()) {
    area->height = static_cast<int>(tiles.size());
    area->width = static_cast<int>(tiles.front().size());
    TileLayer ground = make_layer("ground", "Ground", LayerKind::Ground, area->width, area->height, '.');
    ground.rows = tiles;
    area->layers.push_back(std::move(ground));
  }
  const std::vector<std::string> wall_tiles = json_as_string_array(object_get(json, "wall_tiles"));
  if (!wall_tiles.empty()) {
    TileLayer wall = make_layer("wall", "Wall", LayerKind::Wall, area->width, area->height, ' ');
    wall.rows = wall_tiles;
    area->layers.push_back(std::move(wall));
  }
  area->warps.clear();
  const JsonValue* warps = object_get(json, "warps");
  if (warps != nullptr && warps->type == JsonValue::Type::Array) {
    for (const auto& warp_json : warps->array_value) {
      Warp warp;
      if (load_warp_from_json(warp_json, &warp)) {
        area->warps.push_back(std::move(warp));
      }
    }
  }
  area->entities.clear();
  const JsonValue* npcs = object_get(json, "npcs");
  if (npcs != nullptr && npcs->type == JsonValue::Type::Array) {
    for (const auto& npc_json : npcs->array_value) {
      SceneEntity entity;
      entity.kind = EntityKind::Npc;
      entity.id = json_as_string(object_get(npc_json, "id"), "npc");
      entity.name = json_as_string(object_get(npc_json, "name"), "NPC");
      entity.role = sprite_role_from_string(json_as_string(object_get(npc_json, "role"), "MONK"));
      entity.sprite_id = json_as_string(object_get(npc_json, "sprite_id"), "");
      entity.x = json_as_int(object_get(npc_json, "x"), 0);
      entity.y = json_as_int(object_get(npc_json, "y"), 0);
      entity.facing = facing_from_string(json_as_string(object_get(npc_json, "facing"), "DOWN"));
      entity.solid = json_as_bool(object_get(npc_json, "solid"), true);
      entity.dialogue_text = json_as_string(object_get(npc_json, "dialogue"), "");
      area->entities.push_back(std::move(entity));
    }
  }
  const JsonValue* monsters = object_get(json, "monsters");
  if (monsters != nullptr && monsters->type == JsonValue::Type::Array) {
    for (const auto& monster_json : monsters->array_value) {
      SceneEntity entity;
      entity.kind = EntityKind::Monster;
      entity.id = json_as_string(object_get(monster_json, "id"), "monster");
      entity.name = json_as_string(object_get(monster_json, "name"), "MONSTER");
      entity.sprite_id = json_as_string(object_get(monster_json, "sprite_id"), "");
      entity.x = json_as_int(object_get(monster_json, "x"), 0);
      entity.y = json_as_int(object_get(monster_json, "y"), 0);
      entity.facing = facing_from_string(json_as_string(object_get(monster_json, "facing"), "DOWN"));
      entity.max_hp = std::max(1, json_as_int(object_get(monster_json, "max_hp"), 6));
      entity.attack = std::max(1, json_as_int(object_get(monster_json, "attack"), 1));
      entity.aggressive = json_as_bool(object_get(monster_json, "aggressive"), true);
      area->entities.push_back(std::move(entity));
    }
  }
  return true;
}

bool load_area_from_json(const JsonValue& json, Area* area) {
  if (area == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  const JsonValue* layers = object_get(json, "layers");
  if (layers == nullptr || layers->type != JsonValue::Type::Array) {
    return load_legacy_area_from_json(json, area);
  }

  area->id = json_as_string(object_get(json, "id"), "area");
  area->name = json_as_string(object_get(json, "name"), "AREA");
  area->indoor = json_as_bool(object_get(json, "indoor"), false);
  area->player_tillable = json_as_bool(object_get(json, "player_tillable"), false);
  area->width = std::max(1, json_as_int(object_get(json, "width"), 1));
  area->height = std::max(1, json_as_int(object_get(json, "height"), 1));
  area->properties = json_as_string_map(object_get(json, "properties"));
  area->layers.clear();
  for (const auto& layer_json : layers->array_value) {
    TileLayer layer;
    if (load_layer_from_json(layer_json, &layer)) {
      area->width = std::max(area->width, layer.width);
      area->height = std::max(area->height, layer.height);
      area->layers.push_back(std::move(layer));
    }
  }
  area->warps.clear();
  const JsonValue* warps = object_get(json, "warps");
  if (warps != nullptr && warps->type == JsonValue::Type::Array) {
    for (const auto& warp_json : warps->array_value) {
      Warp warp;
      if (load_warp_from_json(warp_json, &warp)) {
        area->warps.push_back(std::move(warp));
      }
    }
  }
  area->entities.clear();
  const JsonValue* entities = object_get(json, "entities");
  if (entities != nullptr && entities->type == JsonValue::Type::Array) {
    for (const auto& entity_json : entities->array_value) {
      SceneEntity entity;
      if (load_entity_from_json(entity_json, &entity)) {
        area->entities.push_back(std::move(entity));
      }
    }
  }
  return true;
}

JsonValue project_to_json(const Project& project) {
  std::vector<JsonValue> tiles;
  std::vector<JsonValue> stamps;
  std::vector<JsonValue> sprites;
  std::vector<JsonValue> archetypes;
  std::vector<JsonValue> dialogues;
  std::vector<JsonValue> items;
  std::vector<JsonValue> shops;
  std::vector<JsonValue> crops;
  std::vector<JsonValue> quests;
  std::vector<JsonValue> areas;
  for (const auto& tile : project.tiles) {
    tiles.push_back(tile_definition_to_json(tile));
  }
  for (const auto& stamp : project.stamps) {
    stamps.push_back(stamp_to_json(stamp));
  }
  for (const auto& sprite : project.sprites) {
    sprites.push_back(sprite_to_json(sprite));
  }
  for (const auto& archetype : project.archetypes) {
    archetypes.push_back(archetype_to_json(archetype));
  }
  for (const auto& dialogue : project.dialogues) {
    dialogues.push_back(dialogue_graph_to_json(dialogue));
  }
  for (const auto& item : project.items) {
    items.push_back(item_to_json(item));
  }
  for (const auto& shop : project.shops) {
    shops.push_back(shop_to_json(shop));
  }
  for (const auto& crop : project.crops) {
    crops.push_back(crop_to_json(crop));
  }
  for (const auto& quest : project.quests) {
    quests.push_back(quest_to_json(quest));
  }
  for (const auto& area : project.areas) {
    areas.push_back(area_to_json(area));
  }
  return make_json_object({
      {"version", make_json_number(2)},
      {"meta", make_json_object({
          {"id", make_json_string(project.meta.id)},
          {"name", make_json_string(project.meta.name)},
          {"author", make_json_string(project.meta.author)},
          {"version", make_json_string(project.meta.version)},
          {"starting_area_id", make_json_string(project.meta.starting_area_id)},
          {"starting_warp_id", make_json_string(project.meta.starting_warp_id)},
      })},
      {"tiles", make_json_array(std::move(tiles))},
      {"stamps", make_json_array(std::move(stamps))},
      {"sprites", make_json_array(std::move(sprites))},
      {"archetypes", make_json_array(std::move(archetypes))},
      {"dialogues", make_json_array(std::move(dialogues))},
      {"items", make_json_array(std::move(items))},
      {"shops", make_json_array(std::move(shops))},
      {"crops", make_json_array(std::move(crops))},
      {"quests", make_json_array(std::move(quests))},
      {"areas", make_json_array(std::move(areas))},
  });
}

bool project_from_json(const JsonValue& json, Project* project) {
  if (project == nullptr || json.type != JsonValue::Type::Object) {
    return false;
  }
  project->tiles.clear();
  project->stamps.clear();
  project->sprites.clear();
  project->archetypes.clear();
  project->dialogues.clear();
  project->items.clear();
  project->shops.clear();
  project->crops.clear();
  project->quests.clear();
  project->areas.clear();

  const JsonValue* meta = object_get(json, "meta");
  if (meta != nullptr && meta->type == JsonValue::Type::Object) {
    project->meta.id = json_as_string(object_get(*meta, "id"), "pixelpal_rpg");
    project->meta.name = json_as_string(object_get(*meta, "name"), "PixelPal RPG Project");
    project->meta.author = json_as_string(object_get(*meta, "author"), "");
    project->meta.version = json_as_string(object_get(*meta, "version"), "0.1.0");
    project->meta.starting_area_id = json_as_string(object_get(*meta, "starting_area_id"), "");
    project->meta.starting_warp_id = json_as_string(object_get(*meta, "starting_warp_id"), "");
  } else {
    project->meta.id = "legacy_priory_project";
    project->meta.name = json_as_string(object_get(json, "project_name"), "Priory Project");
    project->meta.author.clear();
    project->meta.version = "1.0";
    project->meta.starting_area_id.clear();
    project->meta.starting_warp_id.clear();
  }

  const JsonValue* tiles = object_get(json, "tiles");
  if (tiles != nullptr && tiles->type == JsonValue::Type::Array) {
    for (const auto& tile_json : tiles->array_value) {
      TileDefinition tile;
      if (load_tile_definition_from_json(tile_json, &tile)) {
        project->tiles.push_back(std::move(tile));
      }
    }
  }
  const JsonValue* stamps = object_get(json, "stamps");
  if (stamps != nullptr && stamps->type == JsonValue::Type::Array) {
    for (const auto& stamp_json : stamps->array_value) {
      PatternStamp stamp;
      if (load_stamp_from_json(stamp_json, &stamp)) {
        project->stamps.push_back(std::move(stamp));
      }
    }
  }
  const JsonValue* sprites = object_get(json, "sprites");
  if (sprites != nullptr && sprites->type == JsonValue::Type::Array) {
    for (const auto& sprite_json : sprites->array_value) {
      SpriteAsset sprite;
      if (load_sprite_from_json(sprite_json, &sprite)) {
        project->sprites.push_back(std::move(sprite));
      }
    }
  }
  const JsonValue* archetypes = object_get(json, "archetypes");
  if (archetypes != nullptr && archetypes->type == JsonValue::Type::Array) {
    for (const auto& archetype_json : archetypes->array_value) {
      EntityArchetype archetype;
      if (load_archetype_from_json(archetype_json, &archetype)) {
        project->archetypes.push_back(std::move(archetype));
      }
    }
  }
  const JsonValue* dialogues = object_get(json, "dialogues");
  if (dialogues != nullptr && dialogues->type == JsonValue::Type::Array) {
    for (const auto& dialogue_json : dialogues->array_value) {
      DialogueGraph dialogue;
      if (load_dialogue_graph_from_json(dialogue_json, &dialogue)) {
        project->dialogues.push_back(std::move(dialogue));
      }
    }
  }
  const JsonValue* items = object_get(json, "items");
  if (items != nullptr && items->type == JsonValue::Type::Array) {
    for (const auto& item_json : items->array_value) {
      ItemDefinition item;
      if (load_item_from_json(item_json, &item)) {
        project->items.push_back(std::move(item));
      }
    }
  }
  const JsonValue* shops = object_get(json, "shops");
  if (shops != nullptr && shops->type == JsonValue::Type::Array) {
    for (const auto& shop_json : shops->array_value) {
      ShopDefinition shop;
      if (load_shop_from_json(shop_json, &shop)) {
        project->shops.push_back(std::move(shop));
      }
    }
  }
  const JsonValue* crops = object_get(json, "crops");
  if (crops != nullptr && crops->type == JsonValue::Type::Array) {
    for (const auto& crop_json : crops->array_value) {
      CropDefinition crop;
      if (load_crop_from_json(crop_json, &crop)) {
        project->crops.push_back(std::move(crop));
      }
    }
  }
  const JsonValue* quests = object_get(json, "quests");
  if (quests != nullptr && quests->type == JsonValue::Type::Array) {
    for (const auto& quest_json : quests->array_value) {
      Quest quest;
      if (load_quest_from_json(quest_json, &quest)) {
        project->quests.push_back(std::move(quest));
      }
    }
  }
  const JsonValue* areas = object_get(json, "areas");
  if (areas != nullptr && areas->type == JsonValue::Type::Array) {
    for (const auto& area_json : areas->array_value) {
      Area area;
      if (load_area_from_json(area_json, &area)) {
        project->areas.push_back(std::move(area));
      }
    }
  }
  return true;
}

}  // namespace

Project make_starter_project() {
  Project project;
  project.meta.id = "saint_catherine_engine";
  project.meta.name = "Saint Catherine Engine Starter";
  project.meta.author = "PixelPal";
  project.meta.version = "0.1.0";
  project.meta.starting_area_id = "blackpine_lane";
  project.meta.starting_warp_id = "house_return";
  project.tiles = {
      {"grass", "Grass", "Open ground", '.', true, false, LayerKind::Ground, {"nature"}, {}},
      {"flower", "Flower", "Flowered grass", ',', true, false, LayerKind::Ground, {"nature"}, {}},
      {"path", "Path", "Roadway or lane", ':', true, false, LayerKind::Ground, {"road"}, {}},
      {"stone", "Stone", "Paved court", ';', true, false, LayerKind::Ground, {"road"}, {}},
      {"floor", "Floor", "Interior floor", '_', true, false, LayerKind::Ground, {"indoor"}, {}},
      {"roof", "Roof", "Roofline", '^', false, true, LayerKind::Wall, {"building"}, {}},
      {"wall", "Wall", "Wall tile", '%', false, true, LayerKind::Wall, {"building"}, {}},
      {"door", "Door", "Doorway", '+', true, false, LayerKind::Wall, {"door"}, {}},
      {"window", "Window", "Window tile", 'w', false, true, LayerKind::Wall, {"building"}, {}},
      {"bench", "Bench", "Bench", 'p', false, false, LayerKind::Object, {"furniture"}, {}},
      {"crate", "Crate", "Crate stack", 'x', false, false, LayerKind::Object, {"storage"}, {}},
      {"board", "Board", "Notice board", 'b', false, false, LayerKind::Object, {"quest"}, {}},
      {"garden", "Garden", "Tilled garden", 'g', true, false, LayerKind::Ground, {"farm"}, {}},
  };
  project.stamps = {{"cart_mule", "Cart And Mule", {"jmu", "ynz"}}, {"cottage_front", "Cottage Front", {"^^^^^", "%%%%%", "%w+w%", ":::::"}}};
  project.sprites.push_back(make_sprite("novice", "Novice", false));
  project.sprites.push_back(make_sprite("ratling", "Ratling", true));
  project.archetypes = {
      {"prior", "Prior", EntityKind::Npc, "novice", SpriteRole::Prior, true, false, 0, 0, "prior_greeting", "", {"clergy"}, {}},
      {"merchant", "Merchant", EntityKind::Shopkeeper, "novice", SpriteRole::Merchant, true, false, 0, 0, "merchant_greeting", "blackpine_goods", {"shop"}, {}},
      {"ratling_pack", "Ratling Pack", EntityKind::Monster, "ratling", SpriteRole::Monk, true, true, 8, 2, "", "", {"monster"}, {}},
      {"wood_chest", "Wood Chest", EntityKind::Chest, "", SpriteRole::Monk, true, false, 0, 0, "", "", {"storage"}, {}},
  };
  project.dialogues = {
      {"prior_greeting", "Prior Greeting", "start", {{"start", "Prior Conrad", "Saint Catherine must be rebuilt in stone and discipline alike.", {}, true}}},
      {"merchant_greeting", "Merchant Greeting", "start", {{"start", "Alisoun", "Cloth, candles, and seed grain. Blackpine breathes through trade.", {{"Browse the stock.", "", "open_shop"}, {"Leave.", "", ""}}, false}}},
  };
  project.items = {
      {"barley_seed", "Barley Seed", "A hardy seed for priory gardens.", 4, 0, false, false, "", {"seed"}},
      {"leek_seed", "Leek Seed", "A kitchen staple that suits wet ground.", 5, 0, false, false, "", {"seed"}},
      {"cottage_table", "Cottage Table", "A heavy table for a purchased cottage.", 24, 0, false, true, "wood_chest", {"decor"}},
      {"travel_bread", "Travel Bread", "Dense bread that restores health.", 3, 6, true, false, "", {"food"}},
  };
  project.shops = {{"blackpine_goods", "Blackpine Goods", "alisoun", {{"barley_seed", 4, -1}, {"leek_seed", 5, -1}, {"travel_bread", 3, -1}, {"cottage_table", 24, 4}}}};
  project.crops = {{"barley", "Barley", "barley_seed", "travel_bread", 3, {"g", "g", "g"}}, {"leek", "Leek", "leek_seed", "travel_bread", 4, {"g", "g", "g", "g"}}};
  project.quests = {
      {"arrival_to_saint_catherine", "Arrival To Saint Catherine", "Leave your father's house, board the cart, and pass through the priory gate.", "driver_tomas", "Go first to the cart. Tomas will take you toward Saint Catherine.", "You reached Saint Catherine and crossed the first threshold.",
       {{"leave_house", "Leave the house and step onto Blackpine Lane.", {{"reach_lane", RequirementKind::ReachArea, "", "blackpine_lane", 5, 9, 1, "Reach Blackpine Lane."}}, {}},
        {"board_cart", "Speak to Tomas and board the cart.", {{"talk_tomas", RequirementKind::TalkToEntity, "driver_tomas", "blackpine_lane", 14, 10, 1, "Speak to Tomas."}}, {}}},
       {{"arrival_reward", ActionKind::GiveCoins, "", 5, "", "Grant five coins."}}},
  };

  Area lane;
  lane.id = "blackpine_lane";
  lane.name = "Blackpine Lane";
  lane.width = 24;
  lane.height = 18;
  lane.layers.push_back(make_layer("ground", "Ground", LayerKind::Ground, 24, 18, '.'));
  lane.layers.push_back(make_layer("wall", "Wall", LayerKind::Wall, 24, 18, ' '));
  lane.layers[0].rows[10] = "::::::::::::::::::::::::";
  lane.layers[0].rows[11] = "::::::::::::::::::::::::";
  lane.layers[0].rows[12] = "::::::::::::::::::::::::";
  lane.layers[1].rows[3] = "  ^^^^^^^               ";
  lane.layers[1].rows[4] = "  %%%%%%%               ";
  lane.layers[1].rows[5] = "  %w%+%w%               ";
  lane.layers[1].rows[6] = "  %%%%%%%               ";
  lane.warps.push_back({"house_return", "House Return", {5, 9, 1, 1}, "fathers_house", "front_door", 7, 8, Facing::Up});
  lane.warps.push_back({"to_square", "To Square", {23, 11, 1, 1}, "blackpine_square", "from_lane", 1, 11, Facing::Right});
  lane.entities.push_back({"driver_tomas", "Driver Tomas", EntityKind::Npc, "merchant", "", SpriteRole::Merchant, 14, 10, Facing::Left, true, false, 0, 0, "", "The cart leaves when you climb aboard.", "", {"transport"}, {}});
  lane.entities.push_back({"alisoun", "Alisoun", EntityKind::Shopkeeper, "merchant", "", SpriteRole::Merchant, 18, 8, Facing::Down, true, false, 0, 0, "merchant_greeting", "", "blackpine_goods", {"shop"}, {}});

  Area house;
  house.id = "fathers_house";
  house.name = "Father's House";
  house.indoor = true;
  house.width = 14;
  house.height = 10;
  house.layers.push_back(make_layer("ground", "Ground", LayerKind::Ground, 14, 10, '_'));
  house.layers.push_back(make_layer("wall", "Wall", LayerKind::Wall, 14, 10, ' '));
  house.layers[1].rows[0] = "%%%%%%%%%%%%%%";
  house.layers[1].rows[1] = "%            %";
  house.layers[1].rows[2] = "%            %";
  house.layers[1].rows[3] = "%     p      %";
  house.layers[1].rows[4] = "%            %";
  house.layers[1].rows[5] = "%   t        %";
  house.layers[1].rows[6] = "%            %";
  house.layers[1].rows[7] = "%            %";
  house.layers[1].rows[8] = "%     ++     %";
  house.layers[1].rows[9] = "%%%%%%%%%%%%%%";
  house.warps.push_back({"front_door", "Front Door", {5, 8, 2, 1}, "blackpine_lane", "house_return", 5, 10, Facing::Down});
  house.entities.push_back({"father_aldwyn", "Father Aldwyn", EntityKind::Npc, "", "novice", SpriteRole::Elder, 8, 4, Facing::Left, true, false, 0, 0, "", "Saint Catherine receives you with prayer, not haste.", "", {"family"}, {}});

  Area square;
  square.id = "blackpine_square";
  square.name = "Blackpine Square";
  square.width = 28;
  square.height = 18;
  square.layers.push_back(make_layer("ground", "Ground", LayerKind::Ground, 28, 18, '.'));
  square.layers.push_back(make_layer("wall", "Wall", LayerKind::Wall, 28, 18, ' '));
  for (int row = 4; row <= 13; ++row) {
    square.layers[0].rows[static_cast<std::size_t>(row)] = "..;;;;;;;;;;;;;;;;;;;;;;;;..";
  }
  square.layers[1].rows[4] = "  ^^^^^       b        ^^^^^";
  square.layers[1].rows[5] = "  %%%%%                %%%%%";
  square.layers[1].rows[6] = "  %w+w%                %w+w%";
  square.layers[1].rows[7] = "  %%%%%                %%%%%";
  square.warps.push_back({"from_lane", "From Lane", {0, 11, 1, 1}, "blackpine_lane", "to_square", 22, 11, Facing::Left});
  square.warps.push_back({"to_gate", "To Gate", {27, 10, 1, 2}, "priory_gate", "road_entry", 1, 10, Facing::Right});
  square.entities.push_back({"town_watch", "Town Watch", EntityKind::Npc, "", "", SpriteRole::Watchman, 13, 10, Facing::Left, true, false, 0, 0, "", "Stay clear of the guilds if you want peace.", "", {"watch"}, {}});
  square.entities.push_back({"ratling_1", "Ratling", EntityKind::Monster, "ratling_pack", "", SpriteRole::Monk, 20, 13, Facing::Left, true, true, 8, 2, "", "", "", {"encounter"}, {}});

  Area gate;
  gate.id = "priory_gate";
  gate.name = "Priory Gate";
  gate.width = 20;
  gate.height = 16;
  gate.layers.push_back(make_layer("ground", "Ground", LayerKind::Ground, 20, 16, '.'));
  gate.layers.push_back(make_layer("wall", "Wall", LayerKind::Wall, 20, 16, ' '));
  for (int row = 4; row <= 13; ++row) {
    gate.layers[0].rows[static_cast<std::size_t>(row)] = ".....::::::::::.....";
  }
  gate.layers[1].rows[3] = "      ^^^^^^^^      ";
  gate.layers[1].rows[4] = "      %%%%%%%%      ";
  gate.layers[1].rows[5] = "      %%%++%%%      ";
  gate.layers[1].rows[6] = "      %%%%%%%%      ";
  gate.warps.push_back({"road_entry", "Road Entry", {0, 10, 1, 2}, "blackpine_square", "to_gate", 26, 10, Facing::Left});
  gate.warps.push_back({"courtyard_entry", "Courtyard Entry", {9, 5, 2, 1}, "blackpine_square", "from_lane", 2, 11, Facing::Down});
  gate.entities.push_back({"gate_friar", "Gate Friar", EntityKind::Npc, "prior", "", SpriteRole::Monk, 7, 9, Facing::Right, true, false, 0, 0, "", "When the register is in order, Saint Catherine opens her gate.", "", {"gate"}, {}});

  project.areas.push_back(std::move(house));
  project.areas.push_back(std::move(lane));
  project.areas.push_back(std::move(square));
  project.areas.push_back(std::move(gate));
  return project;
}

ValidationReport validate_project(const Project& project) {
  ValidationReport report;
  if (!is_valid_id(project.meta.id)) {
    push_issue(&report, ValidationIssue::Severity::Error, "meta.id", "Project id must be present.");
  }
  if (!project.meta.starting_area_id.empty() && find_area(project, project.meta.starting_area_id) == nullptr) {
    push_issue(&report, ValidationIssue::Severity::Error, "meta.starting_area_id", "Starting area id does not exist.");
  }

  validate_unique_ids(project.tiles, "tiles", &report, [](const TileDefinition& tile) { return tile.id; });
  validate_unique_ids(project.sprites, "sprites", &report, [](const SpriteAsset& sprite) { return sprite.id; });
  validate_unique_ids(project.stamps, "stamps", &report, [](const PatternStamp& stamp) { return stamp.id; });
  validate_unique_ids(project.archetypes, "archetypes", &report, [](const EntityArchetype& archetype) { return archetype.id; });
  validate_unique_ids(project.dialogues, "dialogues", &report, [](const DialogueGraph& graph) { return graph.id; });
  validate_unique_ids(project.items, "items", &report, [](const ItemDefinition& item) { return item.id; });
  validate_unique_ids(project.shops, "shops", &report, [](const ShopDefinition& shop) { return shop.id; });
  validate_unique_ids(project.crops, "crops", &report, [](const CropDefinition& crop) { return crop.id; });
  validate_unique_ids(project.quests, "quests", &report, [](const Quest& quest) { return quest.id; });
  validate_unique_ids(project.areas, "areas", &report, [](const Area& area) { return area.id; });

  for (const auto& archetype : project.archetypes) {
    if (!archetype.sprite_id.empty() && find_sprite(project, archetype.sprite_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Warning, "archetypes." + archetype.id, "Archetype references a missing sprite.");
    }
    if (!archetype.dialogue_id.empty() && find_dialogue(project, archetype.dialogue_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Warning, "archetypes." + archetype.id, "Archetype references a missing dialogue graph.");
    }
    if (!archetype.shop_id.empty() && find_shop(project, archetype.shop_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Warning, "archetypes." + archetype.id, "Archetype references a missing shop.");
    }
  }

  for (const auto& item : project.items) {
    if (!item.placement_archetype_id.empty() && find_archetype(project, item.placement_archetype_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Warning, "items." + item.id, "Placeable item references a missing placement archetype.");
    }
  }

  for (const auto& crop : project.crops) {
    if (!crop.seed_item_id.empty() && find_item(project, crop.seed_item_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Error, "crops." + crop.id, "Crop seed item is missing.");
    }
    if (!crop.produce_item_id.empty() && find_item(project, crop.produce_item_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Error, "crops." + crop.id, "Crop produce item is missing.");
    }
  }

  for (const auto& shop : project.shops) {
    if (!shop.keeper_entity_id.empty() && find_entity(project, shop.keeper_entity_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Warning, "shops." + shop.id, "Shop keeper entity id does not exist in any area.");
    }
    for (const auto& listing : shop.listings) {
      if (find_item(project, listing.item_id) == nullptr) {
        push_issue(&report, ValidationIssue::Severity::Error, "shops." + shop.id, "Shop listing references a missing item: " + listing.item_id);
      }
    }
  }

  for (const auto& quest : project.quests) {
    if (!quest.giver_entity_id.empty() && find_entity(project, quest.giver_entity_id) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Warning, "quests." + quest.id, "Quest giver entity id does not exist in any area.");
    }
    for (const auto& stage : quest.stages) {
      for (const auto& requirement : stage.requirements) {
        if (requirement.kind == RequirementKind::ReachArea && !requirement.area_id.empty() &&
            find_area(project, requirement.area_id) == nullptr) {
          push_issue(&report, ValidationIssue::Severity::Error, "quests." + quest.id + "." + stage.id, "Stage requirement references a missing area.");
        }
        if ((requirement.kind == RequirementKind::TalkToEntity || requirement.kind == RequirementKind::DefeatEntity) &&
            !requirement.target_id.empty() && find_entity(project, requirement.target_id) == nullptr) {
          push_issue(&report, ValidationIssue::Severity::Error, "quests." + quest.id + "." + stage.id, "Stage requirement references a missing entity.");
        }
        if (requirement.kind == RequirementKind::CollectItem && !requirement.target_id.empty() &&
            find_item(project, requirement.target_id) == nullptr) {
          push_issue(&report, ValidationIssue::Severity::Error, "quests." + quest.id + "." + stage.id, "Stage requirement references a missing item.");
        }
      }
    }
  }

  for (const auto& area : project.areas) {
    if (area.width <= 0 || area.height <= 0) {
      push_issue(&report, ValidationIssue::Severity::Error, "areas." + area.id, "Area dimensions must be positive.");
    }
    if (find_layer(area, LayerKind::Ground) == nullptr) {
      push_issue(&report, ValidationIssue::Severity::Warning, "areas." + area.id, "Area has no ground layer.");
    }
    std::set<std::string> warp_ids;
    for (const auto& layer : area.layers) {
      if (static_cast<int>(layer.rows.size()) != layer.height) {
        push_issue(&report, ValidationIssue::Severity::Error, "areas." + area.id + ".layers." + layer.id, "Layer row count does not match declared height.");
      }
      for (const auto& row : layer.rows) {
        if (static_cast<int>(row.size()) != layer.width) {
          push_issue(&report, ValidationIssue::Severity::Error, "areas." + area.id + ".layers." + layer.id, "Layer row width does not match declared width.");
          break;
        }
      }
    }
    for (const auto& warp : area.warps) {
      if (!warp_ids.insert(warp.id).second) {
        push_issue(&report, ValidationIssue::Severity::Error, "areas." + area.id, "Duplicate warp id detected.");
      }
      if (!warp.target_area.empty() && find_area(project, warp.target_area) == nullptr) {
        push_issue(&report, ValidationIssue::Severity::Error, "areas." + area.id + ".warps." + warp.id, "Warp target area does not exist.");
      }
    }
    std::set<std::string> entity_ids;
    for (const auto& entity : area.entities) {
      if (!entity_ids.insert(entity.id).second) {
        push_issue(&report, ValidationIssue::Severity::Error, "areas." + area.id, "Duplicate entity id detected.");
      }
      if (!entity.archetype_id.empty() && find_archetype(project, entity.archetype_id) == nullptr) {
        push_issue(&report, ValidationIssue::Severity::Warning, "areas." + area.id + ".entities." + entity.id, "Entity archetype id does not exist.");
      }
      if (!entity.sprite_id.empty() && find_sprite(project, entity.sprite_id) == nullptr) {
        push_issue(&report, ValidationIssue::Severity::Warning, "areas." + area.id + ".entities." + entity.id, "Entity sprite id does not exist.");
      }
    }
  }
  return report;
}

ProjectSummary summarize_project(const Project& project) {
  ProjectSummary summary;
  summary.area_count = static_cast<int>(project.areas.size());
  summary.dialogue_count = static_cast<int>(project.dialogues.size());
  summary.quest_count = static_cast<int>(project.quests.size());
  summary.item_count = static_cast<int>(project.items.size());
  summary.shop_count = static_cast<int>(project.shops.size());
  summary.crop_count = static_cast<int>(project.crops.size());
  summary.sprite_count = static_cast<int>(project.sprites.size());
  summary.archetype_count = static_cast<int>(project.archetypes.size());
  for (const auto& area : project.areas) {
    summary.layer_count += static_cast<int>(area.layers.size());
    summary.warp_count += static_cast<int>(area.warps.size());
    summary.entity_count += static_cast<int>(area.entities.size());
  }
  return summary;
}

bool load_project(Project* project, const std::filesystem::path& path, std::string* error) {
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
  const std::string source = buffer.str();
  JsonValue root;
  JsonParser parser(source);
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

bool save_project(const Project& project, const std::filesystem::path& path, std::string* error) {
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

}  // namespace pixelpal::rpg
