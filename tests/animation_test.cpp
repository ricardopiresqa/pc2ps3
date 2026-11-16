#include "pc2ps3/animation.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

int main() {
  using pc2ps3::AnimationClip;
  using pc2ps3::AnimationFrame;
  using pc2ps3::AnimationPlayer;
  const std::vector<AnimationFrame> frames = {
      {0, 0, 16, 16, 0.25}, {16, 0, 16, 16, 0.5},
      {32, 0, 16, 16, 0.25}};
  const AnimationClip once(frames, false);
  const AnimationClip loop(frames, true);
  const AnimationClip empty(std::vector<AnimationFrame>(), false);
  const AnimationClip bad_rect({AnimationFrame{0, 0, 0, 16, 0.25}}, false);
  const AnimationClip zero_duration({AnimationFrame{0, 0, 16, 16, 0.0}}, false);
  const AnimationClip negative_duration({AnimationFrame{0, 0, 16, 16, -0.25}}, false);
  const AnimationClip nan_duration(
      {AnimationFrame{0, 0, 16, 16, std::numeric_limits<double>::quiet_NaN()}}, false);
  const AnimationClip infinite_duration(
      {AnimationFrame{0, 0, 16, 16, std::numeric_limits<double>::infinity()}}, false);
  assert(once.valid() && loop.valid());
  assert(!empty.valid() && !bad_rect.valid() && !zero_duration.valid());
  assert(!negative_duration.valid() && !nan_duration.valid() &&
         !infinite_duration.valid());

  AnimationPlayer player;
  assert(!player.play(empty));
  assert(player.current_frame() == 0 && !player.finished());
  assert(player.play(once));
  assert(player.current_frame() == &once.frame(0));
  player.update(0.125);
  player.update(0.0);
  player.update(-1.0);
  player.update(std::numeric_limits<double>::quiet_NaN());
  player.update(std::numeric_limits<double>::infinity());
  assert(player.current_index() == 0 && player.elapsed_in_frame() == 0.125);
  player.update(0.125);
  assert(player.current_index() == 1 && player.elapsed_in_frame() == 0.0);
  player.update(0.5);
  assert(player.current_index() == 2 && player.elapsed_in_frame() == 0.0);
  player.update(0.25);
  assert(player.current_index() == 2 && player.finished());
  player.update(10.0);
  assert(player.finished() && player.elapsed_in_frame() == 0.25);
  const AnimationFrame* completed_frame = player.current_frame();
  const std::size_t completed_index = player.current_index();
  const double completed_elapsed = player.elapsed_in_frame();
  assert(!player.play(empty));
  assert(player.current_frame() == completed_frame);
  assert(player.current_index() == completed_index);
  assert(player.elapsed_in_frame() == completed_elapsed && player.finished());
  assert(player.play(once));
  assert(player.current_frame() == &once.frame(0) && !player.finished());
  player.reset();
  assert(player.current_frame() == &once.frame(0));
  assert(player.current_index() == 0 && player.elapsed_in_frame() == 0.0);
  assert(!player.finished());

  assert(player.play(loop));
  player.update(0.25);
  assert(player.current_index() == 1 && player.elapsed_in_frame() == 0.0);
  player.update(0.5);
  assert(player.current_index() == 2 && player.elapsed_in_frame() == 0.0);
  player.update(0.25);
  assert(player.current_index() == 0 && player.elapsed_in_frame() == 0.0);
  player.update(1000000.25);
  assert(player.current_index() == 1 && player.elapsed_in_frame() == 0.0);
  player.reset();
  assert(player.current_frame() == &loop.frame(0));
  assert(player.elapsed_in_frame() == 0.0 && !player.finished());

  const AnimationClip decimal_loop(
      {AnimationFrame{0, 0, 16, 16, 0.1},
       AnimationFrame{16, 0, 16, 16, 0.2}}, true);
  assert(player.play(decimal_loop));
  player.update(0.1);
  assert(player.current_index() == 1);
  player.update(0.2);
  assert(player.current_index() == 0 && player.elapsed_in_frame() == 0.0);
  return 0;
}
