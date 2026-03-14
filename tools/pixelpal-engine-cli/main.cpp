#include "pixelpal/rpg_engine.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

void print_usage() {
  std::puts("pixelpal-engine-cli");
  std::puts("  --smoke-test");
  std::puts("  --emit-starter <output.json>");
  std::puts("  --validate <project.json>");
  std::puts("  --inspect <project.json>");
  std::puts("  --upgrade <input.json> <output.json>");
}

void print_summary(const pixelpal::rpg::ProjectSummary& summary) {
  std::printf("areas=%d layers=%d warps=%d entities=%d dialogues=%d quests=%d items=%d shops=%d crops=%d sprites=%d archetypes=%d\n",
              summary.area_count,
              summary.layer_count,
              summary.warp_count,
              summary.entity_count,
              summary.dialogue_count,
              summary.quest_count,
              summary.item_count,
              summary.shop_count,
              summary.crop_count,
              summary.sprite_count,
              summary.archetype_count);
}

void print_validation(const pixelpal::rpg::ValidationReport& report) {
  for (const auto& issue : report.issues) {
    const char* severity = issue.severity == pixelpal::rpg::ValidationIssue::Severity::Error ? "ERROR" : "WARN";
    std::printf("[%s] %s: %s\n", severity, issue.path.c_str(), issue.message.c_str());
  }
}

int emit_starter(const std::filesystem::path& output_path) {
  const pixelpal::rpg::Project project = pixelpal::rpg::make_starter_project();
  std::string error;
  if (!pixelpal::rpg::save_project(project, output_path, &error)) {
    std::fprintf(stderr, "Failed to write starter project: %s\n", error.c_str());
    return 1;
  }
  std::printf("WROTE %s\n", output_path.string().c_str());
  return 0;
}

int validate_file(const std::filesystem::path& path, bool print_summary_too) {
  pixelpal::rpg::Project project;
  std::string error;
  if (!pixelpal::rpg::load_project(&project, path, &error)) {
    std::fprintf(stderr, "Failed to load project: %s\n", error.c_str());
    return 1;
  }
  const auto report = pixelpal::rpg::validate_project(project);
  if (print_summary_too) {
    print_summary(pixelpal::rpg::summarize_project(project));
  }
  print_validation(report);
  std::printf("validation: %d error(s), %d warning(s)\n", report.error_count(), report.warning_count());
  return report.has_errors() ? 1 : 0;
}

int upgrade_file(const std::filesystem::path& input_path, const std::filesystem::path& output_path) {
  pixelpal::rpg::Project project;
  std::string error;
  if (!pixelpal::rpg::load_project(&project, input_path, &error)) {
    std::fprintf(stderr, "Failed to load input project: %s\n", error.c_str());
    return 1;
  }
  if (!pixelpal::rpg::save_project(project, output_path, &error)) {
    std::fprintf(stderr, "Failed to save upgraded project: %s\n", error.c_str());
    return 1;
  }
  std::printf("UPGRADED %s -> %s\n", input_path.string().c_str(), output_path.string().c_str());
  return 0;
}

int smoke_test() {
  const auto smoke_path = std::filesystem::path("build") / "pixelpal-engine-smoke.json";
  const pixelpal::rpg::Project starter = pixelpal::rpg::make_starter_project();
  const auto starter_report = pixelpal::rpg::validate_project(starter);
  if (starter_report.has_errors()) {
    std::fprintf(stderr, "Starter project validation failed before save.\n");
    print_validation(starter_report);
    return 1;
  }

  std::string error;
  if (!pixelpal::rpg::save_project(starter, smoke_path, &error)) {
    std::fprintf(stderr, "Starter save failed: %s\n", error.c_str());
    return 1;
  }

  pixelpal::rpg::Project loaded;
  if (!pixelpal::rpg::load_project(&loaded, smoke_path, &error)) {
    std::fprintf(stderr, "Starter load failed: %s\n", error.c_str());
    return 1;
  }

  const auto loaded_report = pixelpal::rpg::validate_project(loaded);
  if (loaded_report.has_errors()) {
    std::fprintf(stderr, "Loaded starter project validation failed.\n");
    print_validation(loaded_report);
    return 1;
  }

  const auto summary = pixelpal::rpg::summarize_project(loaded);
  if (summary.area_count < 2 || summary.entity_count < 3 || summary.quest_count < 1 || summary.item_count < 3) {
    std::fprintf(stderr, "Starter project summary is thinner than expected.\n");
    print_summary(summary);
    return 1;
  }

  std::puts("PIXELPAL ENGINE SMOKE TEST OK");
  print_summary(summary);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  const std::string command = argv[1];
  if (command == "--smoke-test") {
    return smoke_test();
  }
  if (command == "--emit-starter" && argc >= 3) {
    return emit_starter(argv[2]);
  }
  if (command == "--validate" && argc >= 3) {
    return validate_file(argv[2], false);
  }
  if (command == "--inspect" && argc >= 3) {
    return validate_file(argv[2], true);
  }
  if (command == "--upgrade" && argc >= 4) {
    return upgrade_file(argv[2], argv[3]);
  }

  print_usage();
  return 1;
}
