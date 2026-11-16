#include "pc2ps3/bundle_file_system.hpp"
#include "pc2ps3/texture_manager.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: bundle_texture_integration_test <package>\n";
    return 2;
  }

  pc2ps3::BundleFileSystem bundle(argv[1]);
  assert(bundle.valid());
  pc2ps3::TextureManager textures(bundle, 4, 1024, 1024);
  assert(textures.valid());

  pc2ps3::TextureHandle first;
  assert(textures.load("assets/red.png", first) == pc2ps3::TextureStatus::kOk);
  const pc2ps3::Texture* texture = textures.get(first);
  assert(texture != NULL);
  assert(texture->width() == 1 && texture->height() == 1);
  const pc2ps3::Color* pixel = texture->pixel_at(0, 0);
  assert(pixel != NULL);
  assert(pixel->r == 255 && pixel->g == 0 && pixel->b == 0 && pixel->a == 255);

  pc2ps3::TextureHandle second;
  assert(textures.load("assets/red.png", second) == pc2ps3::TextureStatus::kOk);
  assert(textures.loaded_count() == 1);
  assert(textures.unload(first) == pc2ps3::TextureStatus::kOk);
  assert(textures.get(second) != NULL);
  assert(textures.loaded_count() == 1);
  assert(textures.unload(second) == pc2ps3::TextureStatus::kOk);
  assert(textures.loaded_count() == 0);

  pc2ps3::TextureHandle reloaded;
  assert(textures.load("assets/red.png", reloaded) ==
         pc2ps3::TextureStatus::kOk);
  assert(textures.get(reloaded) != NULL);
  assert(textures.unload(reloaded) == pc2ps3::TextureStatus::kOk);

  pc2ps3::TextureHandle rejected;
  assert(textures.load("../assets/red.png", rejected) ==
         pc2ps3::TextureStatus::kInvalidArgument);
  assert(!rejected.valid());

  std::cout << "bundle texture integration scenarios passed\n";
  return 0;
}
