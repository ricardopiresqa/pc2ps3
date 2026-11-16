#include "pc2ps3/tilemap.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace pc2ps3 {
namespace {

const int kMaxMapDimension = 4096;
const std::size_t kMaxCellsPerMap = 1048576u;
const std::size_t kMaxCellsAcrossLayers = 1048576u;
const std::size_t kMaxLayers = 32u;

std::int64_t floor_div(std::int64_t value, std::int64_t divisor) {
  std::int64_t quotient = value / divisor;
  if (value % divisor < 0) --quotient;
  return quotient;
}

std::int64_t ceil_div(std::int64_t value, std::int64_t divisor) {
  return -floor_div(-value, divisor);
}

std::int64_t clamp_index(std::int64_t value, int maximum) {
  if (value < 0) return 0;
  if (value > maximum) return maximum;
  return value;
}

}  // namespace

TileMap::TileMap()
    : width_(0), height_(0), tile_width_(0), tile_height_(0),
      cells_per_layer_(0) {}

TileMap::TileMap(int width, int height, int tile_width, int tile_height)
    : width_(0), height_(0), tile_width_(0), tile_height_(0),
      cells_per_layer_(0) {
  if (width <= 0 || height <= 0 || width > kMaxMapDimension ||
      height > kMaxMapDimension || tile_width <= 0 || tile_height <= 0 ||
      tile_width > kMaxMapDimension || tile_height > kMaxMapDimension) {
    return;
  }
  const std::uint64_t cell_count =
      static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  if (cell_count > kMaxCellsPerMap) return;
  width_ = width;
  height_ = height;
  tile_width_ = tile_width;
  tile_height_ = tile_height;
  cells_per_layer_ = static_cast<std::size_t>(cell_count);
}

bool TileMap::valid() const {
  return width_ > 0 && height_ > 0 && tile_width_ > 0 && tile_height_ > 0 &&
         cells_per_layer_ > 0;
}

int TileMap::add_layer(int depth) {
  if (!valid() || layers_.size() >= kMaxLayers ||
      cells_per_layer_ > kMaxCellsAcrossLayers / (layers_.size() + 1u)) {
    return -1;
  }
  Layer layer;
  layer.depth = depth;
  layer.cells.assign(cells_per_layer_, -1);
  layers_.push_back(std::move(layer));
  return static_cast<int>(layers_.size() - 1u);
}

bool TileMap::set_tile(std::size_t layer, int x, int y,
                       std::int32_t tile_index) {
  if (!valid() || layer >= layers_.size() || x < 0 || y < 0 || x >= width_ ||
      y >= height_ || tile_index < -1) {
    return false;
  }
  const std::size_t index = static_cast<std::size_t>(y) * width_ + x;
  layers_[layer].cells[index] = tile_index;
  return true;
}

std::int32_t TileMap::tile_at(std::size_t layer, int x, int y) const {
  if (!valid() || layer >= layers_.size() || x < 0 || y < 0 || x >= width_ ||
      y >= height_) {
    return -1;
  }
  const std::size_t index = static_cast<std::size_t>(y) * width_ + x;
  return layers_[layer].cells[index];
}

bool TileMap::layer_depth(std::size_t layer, int& depth) const {
  if (layer >= layers_.size()) return false;
  depth = layers_[layer].depth;
  return true;
}

int TileMap::width() const { return width_; }
int TileMap::height() const { return height_; }
int TileMap::tile_width() const { return tile_width_; }
int TileMap::tile_height() const { return tile_height_; }
std::size_t TileMap::layer_count() const { return layers_.size(); }

std::size_t TileMap::cell_storage_bytes() const {
  std::size_t bytes = 0;
  for (std::size_t i = 0; i < layers_.size(); ++i) {
    bytes += layers_[i].cells.capacity() * sizeof(std::int32_t);
  }
  return bytes;
}

bool draw_tilemap(Framebuffer& framebuffer, const Texture& atlas,
                  const TileMap& tilemap, int atlas_columns, int origin_x,
                  int origin_y, int camera_x, int camera_y) {
  if (!tilemap.valid() || !atlas.valid() || atlas_columns <= 0 ||
      framebuffer.width() <= 0 || framebuffer.height() <= 0) {
    return false;
  }
  const int tile_width = tilemap.tile_width();
  const int tile_height = tilemap.tile_height();
  const int available_columns = atlas.width() / tile_width;
  const int available_rows = atlas.height() / tile_height;
  if (atlas_columns > available_columns || available_rows <= 0) return false;
  const std::uint64_t atlas_tile_count =
      static_cast<std::uint64_t>(atlas_columns) *
      static_cast<std::uint64_t>(available_rows);

  const std::int64_t first_column = clamp_index(
      floor_div(static_cast<std::int64_t>(camera_x) - origin_x, tile_width),
      tilemap.width());
  const std::int64_t end_column = clamp_index(
      ceil_div(static_cast<std::int64_t>(camera_x) + framebuffer.width() -
                   origin_x,
               tile_width),
      tilemap.width());
  const std::int64_t first_row = clamp_index(
      floor_div(static_cast<std::int64_t>(camera_y) - origin_y, tile_height),
      tilemap.height());
  const std::int64_t end_row = clamp_index(
      ceil_div(static_cast<std::int64_t>(camera_y) + framebuffer.height() -
                   origin_y,
               tile_height),
      tilemap.height());
  if (first_column >= end_column || first_row >= end_row) return true;

  std::size_t order[32];
  for (std::size_t i = 0; i < tilemap.layer_count(); ++i) {
    order[i] = i;
    std::size_t position = i;
    while (position > 0) {
      int previous_depth = 0;
      int current_depth = 0;
      tilemap.layer_depth(order[position - 1], previous_depth);
      tilemap.layer_depth(order[position], current_depth);
      if (previous_depth <= current_depth) break;
      std::swap(order[position - 1], order[position]);
      --position;
    }
  }

  for (std::size_t i = 0; i < tilemap.layer_count(); ++i) {
    for (std::int64_t y = first_row; y < end_row; ++y) {
      for (std::int64_t x = first_column; x < end_column; ++x) {
        const std::int32_t tile = tilemap.tile_at(
            i, static_cast<int>(x), static_cast<int>(y));
        if (tile >= 0 && static_cast<std::uint64_t>(tile) >= atlas_tile_count) {
          return false;
        }
      }
    }
  }

  for (std::size_t layer_order = 0; layer_order < tilemap.layer_count();
       ++layer_order) {
    const std::size_t layer = order[layer_order];
    for (std::int64_t y = first_row; y < end_row; ++y) {
      const std::int64_t screen_y =
          static_cast<std::int64_t>(origin_y) + y * tile_height - camera_y;
      for (std::int64_t x = first_column; x < end_column; ++x) {
        const std::int32_t tile = tilemap.tile_at(
            layer, static_cast<int>(x), static_cast<int>(y));
        if (tile < 0) continue;
        const std::int64_t screen_x =
            static_cast<std::int64_t>(origin_x) + x * tile_width - camera_x;
        const int source_x = (tile % atlas_columns) * tile_width;
        const int source_y = (tile / atlas_columns) * tile_height;
        if (screen_x < std::numeric_limits<int>::min() ||
            screen_x > std::numeric_limits<int>::max() ||
            screen_y < std::numeric_limits<int>::min() ||
            screen_y > std::numeric_limits<int>::max()) {
          return false;
        }
        if (!framebuffer.draw_texture_region(
                atlas, source_x, source_y, tile_width, tile_height,
                static_cast<int>(screen_x), static_cast<int>(screen_y))) {
          return false;
        }
      }
    }
  }
  return true;
}

}  // namespace pc2ps3
