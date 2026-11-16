#pragma once

#include "pc2ps3/atlas_runtime_index.hpp"
#include "pc2ps3/render.hpp"
#include "pc2ps3/texture_manager.hpp"

#include <cstddef>
#include <string>

namespace pc2ps3 {

class RuntimeDemoAtlas {
 public:
  explicit RuntimeDemoAtlas(FileSystem& file_system);
  ~RuntimeDemoAtlas();

  RuntimeDemoAtlas(const RuntimeDemoAtlas&) = delete;
  RuntimeDemoAtlas& operator=(const RuntimeDemoAtlas&) = delete;

  bool load(const std::string& logical_index_path, std::size_t sprite_index);
  bool unload();
  bool valid() const;
  const Texture* texture() const;
  const AtlasSpriteRegion* sprite_region() const;
  const AtlasRuntimeIndex& index() const { return index_; }

 private:
  TextureManager texture_manager_;
  TextureHandle texture_handle_;
  AtlasRuntimeIndex index_;
  std::size_t sprite_index_;
};

enum class RuntimeDemoPlayerRenderResult { kAtlasSprite, kVectorFallback };

RuntimeDemoPlayerRenderResult render_runtime_demo_player(
    Framebuffer& framebuffer, const RuntimeDemoAtlas* atlas,
    int player_center_x, int player_baseline_y, Color body, Color accent);

} // namespace pc2ps3
