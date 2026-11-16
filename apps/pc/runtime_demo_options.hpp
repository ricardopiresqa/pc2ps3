#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace pc2ps3 {

static const std::size_t kMaxRuntimeDemoSpriteIndex = 1023;

struct RuntimeDemoOptions {
  std::uint32_t max_fps;
  std::string asset_root;
  std::string atlas_index;
  std::size_t sprite_index;

  RuntimeDemoOptions() : max_fps(60), sprite_index(0) {}
};

inline bool parse_runtime_demo_fps(const char* text,
                                   std::uint32_t& max_fps) {
  if (text == 0 || *text == '\0') return false;

  std::uint32_t value = 0;
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') return false;
    const std::uint32_t digit = static_cast<std::uint32_t>(*cursor - '0');
    if (value > 24 || (value == 24 && digit > 0)) return false;
    value = value * 10 + digit;
  }

  max_fps = value;
  return true;
}

inline bool parse_runtime_demo_sprite_index(const char* text,
                                            std::size_t& sprite_index) {
  if (text == 0 || *text == '\0') return false;

  std::size_t value = 0;
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') return false;
    const std::size_t digit = static_cast<std::size_t>(*cursor - '0');
    if (value > kMaxRuntimeDemoSpriteIndex / 10 ||
        (value == kMaxRuntimeDemoSpriteIndex / 10 &&
         digit > kMaxRuntimeDemoSpriteIndex % 10)) {
      return false;
    }
    value = value * 10 + digit;
  }

  sprite_index = value;
  return true;
}

inline bool is_runtime_demo_option_value(const char* value) {
  return value != 0 && value[0] != '\0' &&
         !(value[0] == '-' && value[1] == '-');
}

inline bool parse_runtime_demo_options(int argc, const char* const argv[],
                                       RuntimeDemoOptions& options) {
  if (argc < 1 || argv == 0) return false;

  RuntimeDemoOptions parsed;
  bool saw_max_fps = false;
  bool saw_asset_root = false;
  bool saw_atlas_index = false;
  bool saw_sprite_index = false;
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == 0) return false;
    if (std::strcmp(argv[i], "--max-fps") == 0) {
      if (saw_max_fps || i + 1 >= argc ||
          !parse_runtime_demo_fps(argv[++i], parsed.max_fps)) {
        return false;
      }
      saw_max_fps = true;
    } else if (std::strcmp(argv[i], "--asset-root") == 0) {
      if (saw_asset_root || i + 1 >= argc ||
          !is_runtime_demo_option_value(argv[i + 1])) {
        return false;
      }
      parsed.asset_root = argv[++i];
      saw_asset_root = true;
    } else if (std::strcmp(argv[i], "--atlas-index") == 0) {
      if (saw_atlas_index || i + 1 >= argc ||
          !is_runtime_demo_option_value(argv[i + 1])) {
        return false;
      }
      parsed.atlas_index = argv[++i];
      saw_atlas_index = true;
    } else if (std::strcmp(argv[i], "--sprite-index") == 0) {
      if (saw_sprite_index || i + 1 >= argc ||
          !parse_runtime_demo_sprite_index(argv[++i], parsed.sprite_index)) {
        return false;
      }
      saw_sprite_index = true;
    } else {
      return false;
    }
  }

  if (saw_asset_root != saw_atlas_index ||
      (saw_sprite_index && !saw_asset_root)) {
    return false;
  }
  options = parsed;
  return true;
}

} // namespace pc2ps3
