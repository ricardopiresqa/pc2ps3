#pragma once

#include "pc2ps3/render.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

class TileMap {
 public:
  TileMap();
  TileMap(int width, int height, int tile_width, int tile_height);

  bool valid() const;
  int add_layer(int depth);
  bool set_tile(std::size_t layer, int x, int y, std::int32_t tile_index);
  std::int32_t tile_at(std::size_t layer, int x, int y) const;
  bool layer_depth(std::size_t layer, int& depth) const;

  int width() const;
  int height() const;
  int tile_width() const;
  int tile_height() const;
  std::size_t layer_count() const;
  std::size_t cell_storage_bytes() const;

 private:
  struct Layer {
    int depth;
    std::vector<std::int32_t> cells;
  };

  int width_;
  int height_;
  int tile_width_;
  int tile_height_;
  std::size_t cells_per_layer_;
  std::vector<Layer> layers_;
};

// Draws uniform atlas tiles in layer order. The camera offsets are world pixels.
bool draw_tilemap(Framebuffer& framebuffer, const Texture& atlas,
                  const TileMap& tilemap, int atlas_columns, int origin_x,
                  int origin_y, int camera_x = 0, int camera_y = 0);

}  // namespace pc2ps3
