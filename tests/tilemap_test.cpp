#include "pc2ps3/tilemap.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": "             \
                << #expression << '\n';                                       \
      ++failures;                                                              \
    }                                                                          \
  } while (false)

pc2ps3::Texture make_atlas() {
  std::vector<pc2ps3::Color> pixels(16);
  pixels[0] = pc2ps3::Color{255, 0, 0, 255};
  pixels[1] = pc2ps3::Color{255, 255, 0, 255};
  pixels[2] = pc2ps3::Color{0, 0, 255, 255};
  pixels[3] = pc2ps3::Color{0, 255, 0, 255};
  pixels[4] = pc2ps3::Color{255, 255, 255, 255};
  pixels[5] = pc2ps3::Color{0, 0, 0, 255};
  pixels[6] = pc2ps3::Color{0, 255, 255, 255};
  pixels[7] = pc2ps3::Color{255, 0, 255, 255};
  pixels[8] = pixels[0];
  pixels[9] = pixels[1];
  pixels[10] = pixels[2];
  pixels[11] = pixels[3];
  pixels[12] = pixels[4];
  pixels[13] = pixels[5];
  pixels[14] = pixels[6];
  pixels[15] = pixels[7];
  return pc2ps3::Texture(4, 4, pixels);
}

bool pixel_is(const pc2ps3::Framebuffer& framebuffer, int x, int y,
              std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  pc2ps3::Color color = {0, 0, 0, 0};
  return framebuffer.read_pixel(x, y, color) && color.r == r && color.g == g &&
         color.b == b && color.a == 255;
}

void test_layers_atlas_and_camera() {
  pc2ps3::TileMap map(2, 1, 2, 2);
  CHECK(map.valid());
  const int foreground = map.add_layer(10);
  const int background = map.add_layer(0);
  const int same_depth = map.add_layer(10);
  CHECK(foreground == 0);
  CHECK(background == 1);
  CHECK(same_depth == 2);
  CHECK(map.set_tile(static_cast<std::size_t>(foreground), 0, 0, 1));
  CHECK(map.set_tile(static_cast<std::size_t>(foreground), 1, 0, 0));
  CHECK(map.set_tile(static_cast<std::size_t>(background), 0, 0, 0));
  CHECK(map.set_tile(static_cast<std::size_t>(background), 1, 0, 1));
  CHECK(map.set_tile(static_cast<std::size_t>(same_depth), 1, 0, 1));

  const pc2ps3::Texture atlas = make_atlas();
  pc2ps3::Framebuffer framebuffer(3, 1);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  CHECK(pc2ps3::draw_tilemap(framebuffer, atlas, map, 2, 0, 0, 1, 1));
  CHECK(pixel_is(framebuffer, 0, 0, 255, 0, 255));
  CHECK(pixel_is(framebuffer, 1, 0, 0, 255, 255));
  CHECK(pixel_is(framebuffer, 2, 0, 255, 0, 255));
}

void test_empty_and_invalid_cells() {
  const pc2ps3::Texture atlas = make_atlas();
  pc2ps3::TileMap map(1, 1, 2, 2);
  const int layer = map.add_layer(0);
  CHECK(layer == 0);
  CHECK(map.tile_at(0, 0, 0) == -1);
  CHECK(map.tile_at(0, -1, 0) == -1);
  CHECK(map.tile_at(4, 0, 0) == -1);
  CHECK(!map.set_tile(0, 1, 0, 0));
  CHECK(!map.set_tile(0, 0, 0, -2));
  int depth = 0;
  CHECK(map.layer_depth(0, depth));
  CHECK(depth == 0);
  CHECK(!map.layer_depth(1, depth));

  pc2ps3::Framebuffer framebuffer(2, 2);
  framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
  CHECK(pc2ps3::draw_tilemap(framebuffer, atlas, map, 2, 0, 0));
  CHECK(pixel_is(framebuffer, 0, 0, 0, 0, 0));
  CHECK(!pc2ps3::draw_tilemap(framebuffer, atlas, map, 0, 0, 0));

  CHECK(map.set_tile(0, 0, 0, 4));
  CHECK(!pc2ps3::draw_tilemap(framebuffer, atlas, map, 2, 0, 0));
  CHECK(pixel_is(framebuffer, 0, 0, 0, 0, 0));
  CHECK(!pc2ps3::draw_tilemap(framebuffer, pc2ps3::Texture(), map, 2, 0, 0));
}

void test_limits() {
  CHECK(!pc2ps3::TileMap(0, 1, 1, 1).valid());
  CHECK(!pc2ps3::TileMap(4097, 1, 1, 1).valid());
  CHECK(!pc2ps3::TileMap(1024, 1025, 1, 1).valid());

  pc2ps3::TileMap one_cell(1, 1, 1, 1);
  for (int i = 0; i < 32; ++i) CHECK(one_cell.add_layer(i) == i);
  CHECK(one_cell.add_layer(32) == -1);

  pc2ps3::TileMap bounded_layers(1024, 512, 1, 1);
  CHECK(bounded_layers.add_layer(0) == 0);
  CHECK(bounded_layers.add_layer(1) == 1);
  CHECK(bounded_layers.add_layer(2) == -1);
  CHECK(bounded_layers.cell_storage_bytes() <= 1048576u * sizeof(std::int32_t));
}

}  // namespace

int main() {
  test_layers_atlas_and_camera();
  test_empty_and_invalid_cells();
  test_limits();
  if (failures != 0) {
    std::cerr << failures << " tilemap assertion(s) failed\n";
    return 1;
  }
  std::cout << "tilemap tests passed\n";
  return 0;
}
