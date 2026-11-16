#include "pc2ps3/animation.hpp"
#include "pc2ps3/render.hpp"

#include <cassert>
#include <vector>

namespace {

bool same_color(const pc2ps3::Color& left, const pc2ps3::Color& right) {
  return left.r == right.r && left.g == right.g && left.b == right.b &&
         left.a == right.a;
}

void expect_pixel(const pc2ps3::Framebuffer& framebuffer, int x, int y,
                  const pc2ps3::Color& expected) {
  pc2ps3::Color actual = {0, 0, 0, 0};
  assert(framebuffer.read_pixel(x, y, actual));
  assert(same_color(actual, expected));
}

}  // namespace

int main() {
  const pc2ps3::Color background = {8, 12, 20, 255};
  const pc2ps3::Color red = {220, 30, 40, 255};
  const pc2ps3::Color blue = {35, 80, 230, 255};
  const pc2ps3::Texture atlas(2, 1, std::vector<pc2ps3::Color>{red, blue});
  const pc2ps3::AnimationClip clip(
      std::vector<pc2ps3::AnimationFrame>{
          {0, 0, 1, 1, 0.1}, {1, 0, 1, 1, 0.2}},
      false);
  assert(atlas.valid() && clip.valid());

  pc2ps3::Framebuffer framebuffer(5, 2);
  framebuffer.clear(background);
  pc2ps3::AnimationPlayer player;
  assert(player.play(clip));
  assert(framebuffer.draw_animation_frame(atlas, player, 1, 0));
  assert(player.current_index() == 0);
  expect_pixel(framebuffer, 1, 0, red);

  player.update(0.1);
  assert(player.current_index() == 1);
  assert(framebuffer.draw_animation_frame(atlas, player, 2, 0));
  assert(player.current_index() == 1);
  expect_pixel(framebuffer, 2, 0, blue);

  player.update(0.2);
  assert(player.finished());
  assert(framebuffer.draw_animation_frame(atlas, player, 3, 0));
  expect_pixel(framebuffer, 3, 0, blue);

  pc2ps3::AnimationPlayer empty_player;
  assert(!framebuffer.draw_animation_frame(atlas, empty_player, 0, 0));
  expect_pixel(framebuffer, 0, 0, background);

  const pc2ps3::AnimationClip invalid_region(
      std::vector<pc2ps3::AnimationFrame>{{2, 0, 1, 1, 0.1}}, false);
  pc2ps3::AnimationPlayer invalid_player;
  assert(invalid_player.play(invalid_region));
  assert(!framebuffer.draw_animation_frame(atlas, invalid_player, 0, 0));
  expect_pixel(framebuffer, 0, 0, background);
  return 0;
}