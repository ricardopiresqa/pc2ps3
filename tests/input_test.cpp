#include "pc2ps3/core.hpp"
#include "pc2ps3/input.hpp"

#include <cassert>
#include <cmath>

namespace {

bool near(float actual, float expected) {
  return std::fabs(actual - expected) < 0.0001f;
}

} // namespace

int main() {
  pc2ps3::InputMapper mapper;
  pc2ps3::InputSnapshot snapshot;
  pc2ps3::InputSnapshot keyboard_right;
  keyboard_right.keyboard_right = true;
  pc2ps3::InputFrame keyboard_frame = mapper.sample(keyboard_right);
  assert(keyboard_frame.right && !keyboard_frame.left);
  pc2ps3::InputSnapshot ds3_right;
  ds3_right.dualshock3_right = true;
  pc2ps3::InputFrame ds3_frame = mapper.sample(ds3_right);
  assert(ds3_frame.right && !ds3_frame.left);
  mapper.reset();  pc2ps3::InputFrame frame = mapper.sample(snapshot);
  assert(!frame.left && !frame.right && !frame.jump_pressed && !frame.jump_held);

  snapshot.keyboard_right = true;
  snapshot.keyboard_jump = true;
  frame = mapper.sample(snapshot);
  assert(!frame.left && frame.right && frame.jump_pressed && frame.jump_held);

  frame = mapper.sample(snapshot);
  assert(frame.right && !frame.jump_pressed && frame.jump_held);

  snapshot.keyboard_right = false;
  snapshot.keyboard_jump = false;
  snapshot.dualshock3_left = true;
  snapshot.dualshock3_jump = true;
  frame = mapper.sample(snapshot);
  assert(frame.left && !frame.right && !frame.jump_pressed && frame.jump_held);

  snapshot.dualshock3_jump = false;
  frame = mapper.sample(snapshot);
  assert(frame.left && !frame.jump_pressed && !frame.jump_held);

  snapshot.keyboard_left = true;
  snapshot.keyboard_right = true;
  frame = mapper.sample(snapshot);
  assert(!frame.left && !frame.right && frame.move_x == 0.0f);

  const pc2ps3::WorldConfig config = {
      1.0f / 60.0f, 1500.0f, 2200.0f, 280.0f, 560.0f, 1000.0f};
  pc2ps3::World world(config);
  world.step(frame);
  assert(near(world.player().position.x, 64.0f));

  snapshot = pc2ps3::InputSnapshot();
  snapshot.dualshock3_jump = true;
  frame = mapper.sample(snapshot);
  assert(frame.jump_pressed && frame.jump_held);

  mapper.reset();
  frame = mapper.sample(snapshot);
  assert(frame.jump_pressed && frame.jump_held);

  mapper.reset();
  snapshot = pc2ps3::InputSnapshot();
  frame = mapper.sample(snapshot);
  assert(!frame.left && !frame.right && !frame.jump_pressed && !frame.jump_held);
  return 0;
}
