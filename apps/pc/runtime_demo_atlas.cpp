#include "runtime_demo_atlas.hpp"

#include <utility>

namespace pc2ps3 {
namespace {

const std::size_t kDemoAtlasSlots = 1;
const std::size_t kDemoAtlasEncodedLimit = 32u * 1024u * 1024u;
const std::size_t kDemoAtlasPixelLimit = 64u * 1024u * 1024u;

} // namespace

RuntimeDemoAtlas::RuntimeDemoAtlas(FileSystem& file_system)
    : texture_manager_(file_system, kDemoAtlasSlots, kDemoAtlasEncodedLimit,
                       kDemoAtlasPixelLimit),
      sprite_index_(0) {}

RuntimeDemoAtlas::~RuntimeDemoAtlas() { unload(); }

bool RuntimeDemoAtlas::load(const std::string& logical_index_path,
                            std::size_t sprite_index) {
  if (!unload() || !texture_manager_.valid() || logical_index_path.empty()) {
    return false;
  }

  TextureHandle loaded_handle;
  AtlasRuntimeIndex loaded_index;
  if (texture_manager_.load_atlas(logical_index_path, loaded_handle,
                                  loaded_index) != TextureStatus::kOk) {
    return false;
  }
  if (sprite_index >= loaded_index.sprites.size()) {
    if (texture_manager_.unload(loaded_handle) != TextureStatus::kOk) {
      texture_handle_ = loaded_handle;
    }
    return false;
  }

  using std::swap;
  swap(index_, loaded_index);
  texture_handle_ = loaded_handle;
  sprite_index_ = sprite_index;
  return true;
}

bool RuntimeDemoAtlas::unload() {
  if (texture_handle_.valid() &&
      texture_manager_.unload(texture_handle_) != TextureStatus::kOk) {
    return false;
  }
  AtlasRuntimeIndex empty;
  using std::swap;
  swap(index_, empty);
  sprite_index_ = 0;
  return true;
}

bool RuntimeDemoAtlas::valid() const {
  return texture_manager_.get(texture_handle_) != 0 &&
         sprite_index_ < index_.sprites.size();
}

const Texture* RuntimeDemoAtlas::texture() const {
  return valid() ? texture_manager_.get(texture_handle_) : 0;
}

const AtlasSpriteRegion* RuntimeDemoAtlas::sprite_region() const {
  return valid() ? &index_.sprites[sprite_index_] : 0;
}

RuntimeDemoPlayerRenderResult render_runtime_demo_player(
    Framebuffer& framebuffer, const RuntimeDemoAtlas* atlas,
    int player_center_x, int player_baseline_y, Color body, Color accent) {
  if (atlas != 0 && atlas->valid()) {
    const Texture* texture = atlas->texture();
    const AtlasSpriteRegion* region = atlas->sprite_region();
    if (texture != 0 && region != 0 &&
        framebuffer.draw_texture_region_scaled(
            *texture, static_cast<int>(region->x),
            static_cast<int>(region->y), static_cast<int>(region->width),
            static_cast<int>(region->height), player_center_x - 12,
            player_baseline_y - 38, 24, 38)) {
      return RuntimeDemoPlayerRenderResult::kAtlasSprite;
    }
  }

  framebuffer.draw_player(player_center_x, player_baseline_y, body, accent);
  return RuntimeDemoPlayerRenderResult::kVectorFallback;
}

} // namespace pc2ps3
