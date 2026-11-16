#include "pc2ps3/core.hpp"
#include "pc2ps3/jump_assist.hpp"

#include <cassert>
#include <iostream>

int main() {
  pc2ps3::JumpAssist buffered;
  assert(!buffered.update(true, false));
  for (int tick = 0; tick < 4; ++tick) {
    assert(!buffered.update(false, false));
  }
  assert(buffered.update(false, true));
  assert(!buffered.update(false, false));

  pc2ps3::JumpAssist expired_buffer;
  assert(!expired_buffer.update(true, false));
  for (int tick = 0; tick < 5; ++tick) {
    assert(!expired_buffer.update(false, false));
  }
  assert(!expired_buffer.update(false, true));

  pc2ps3::JumpAssist coyote;
  assert(!coyote.update(false, true));
  for (int tick = 0; tick < 5; ++tick) {
    assert(!coyote.update(false, false));
  }
  assert(coyote.update(true, false));
  assert(!coyote.update(false, false));

  pc2ps3::JumpAssist expired_coyote;
  assert(!expired_coyote.update(false, true));
  for (int tick = 0; tick < 6; ++tick) {
    assert(!expired_coyote.update(false, false));
  }
  assert(!expired_coyote.update(true, false));

  pc2ps3::JumpAssist reset;
  assert(!reset.update(true, false));
  reset.reset();
  assert(!reset.update(false, true));

  pc2ps3::JumpAssist reset_coyote;
  assert(!reset_coyote.update(false, true));
  reset_coyote.reset();
  assert(!reset_coyote.update(true, false));

  const float fixed_dt = 1.0f / 60.0f;
  const float gravity = 1800.0f;
  pc2ps3::World world(pc2ps3::WorldConfig{
      fixed_dt, gravity, 0.0f, 0.0f, 300.0f, 100.0f});
  world.step(pc2ps3::InputFrame{false, false, true, true});
  bool queued_before_landing = false;
  for (int tick = 0; tick < 60 && !world.player().grounded; ++tick) {
    const pc2ps3::Actor& player = world.player();
    const float next_y =
        player.position.y + (player.velocity.y + gravity * fixed_dt) * fixed_dt;
    const bool queue_jump = player.velocity.y > 0.0f && next_y >= 100.0f;
    queued_before_landing = queued_before_landing || queue_jump;
    world.step(pc2ps3::InputFrame{false, false, queue_jump, queue_jump});
  }
  assert(queued_before_landing && world.player().grounded);
  world.step(pc2ps3::InputFrame{false, false, false, false});
  assert(!world.player().grounded && world.player().velocity.y < 0.0f);

  std::cout << "jump assist tests passed\n";
  return 0;
}
