#include "pc2ps3/checkpoint.hpp"
#include "pc2ps3/trigger.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;

#define CHECK(expression)                                                     \
  do {                                                                        \
    if (!(expression)) {                                                      \
      std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": "            \
                << #expression << '\n';                                      \
      ++failures;                                                             \
    }                                                                         \
  } while (false)

bool same_position(pc2ps3::Vec2 first, pc2ps3::Vec2 second) {
  return first.x == second.x && first.y == second.y;
}

void test_activation_from_trigger_and_reset() {
  pc2ps3::CheckpointSystem checkpoints(2, pc2ps3::Vec2{4.0f, 8.0f});
  pc2ps3::TriggerSystem triggers(2);
  CHECK(checkpoints.valid());
  CHECK(checkpoints.add(10, pc2ps3::Vec2{20.0f, 30.0f}));
  CHECK(checkpoints.add(11, pc2ps3::Vec2{40.0f, 50.0f}));
  CHECK(triggers.add(10, pc2ps3::Aabb{10.0f, 10.0f, 16.0f, 16.0f}));
  CHECK(triggers.update(pc2ps3::Aabb{12.0f, 12.0f, 14.0f, 14.0f}));
  for (std::size_t i = 0; i < triggers.events().size(); ++i) {
    if (triggers.events()[i].type == pc2ps3::TriggerEventType::kEnter) {
      CHECK(checkpoints.activate(triggers.events()[i].trigger_id));
    }
  }
  pc2ps3::Vec2 spawn = {0.0f, 0.0f};
  CHECK(checkpoints.respawn_position(spawn));
  CHECK(same_position(spawn, pc2ps3::Vec2{20.0f, 30.0f}));
  std::uint32_t active_id = 0;
  CHECK(checkpoints.active_checkpoint_id(active_id));
  CHECK(active_id == 10);

  CHECK(checkpoints.activate(11));
  CHECK(checkpoints.respawn_position(spawn));
  CHECK(same_position(spawn, pc2ps3::Vec2{40.0f, 50.0f}));
  checkpoints.reset();
  CHECK(!checkpoints.active_checkpoint_id(active_id));
  CHECK(checkpoints.respawn_position(spawn));
  CHECK(same_position(spawn, pc2ps3::Vec2{4.0f, 8.0f}));
  CHECK(checkpoints.checkpoint_count() == 2);
}

void test_invalid_inputs_preserve_active_checkpoint() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  pc2ps3::CheckpointSystem checkpoints(1, pc2ps3::Vec2{1.0f, 2.0f});
  CHECK(checkpoints.add(7, pc2ps3::Vec2{3.0f, 4.0f}));
  CHECK(!checkpoints.add(7, pc2ps3::Vec2{5.0f, 6.0f}));
  CHECK(!checkpoints.add(8, pc2ps3::Vec2{nan, 0.0f}));
  CHECK(!checkpoints.add(8, pc2ps3::Vec2{5.0f, 6.0f}));
  CHECK(checkpoints.activate(7));
  CHECK(!checkpoints.activate(99));
  pc2ps3::Vec2 spawn = {0.0f, 0.0f};
  CHECK(checkpoints.respawn_position(spawn));
  CHECK(same_position(spawn, pc2ps3::Vec2{3.0f, 4.0f}));
  pc2ps3::CheckpointSystem invalid(0, pc2ps3::Vec2{0.0f, 0.0f});
  CHECK(!invalid.valid());
  CHECK(!invalid.respawn_position(spawn));
  pc2ps3::CheckpointSystem invalid_spawn(1, pc2ps3::Vec2{nan, 0.0f});
  CHECK(!invalid_spawn.valid());
}
}  // namespace

int main() {
  test_activation_from_trigger_and_reset();
  test_invalid_inputs_preserve_active_checkpoint();
  if (failures != 0) {
    std::cerr << failures << " checkpoint assertion(s) failed\n";
    return 1;
  }
  std::cout << "checkpoint tests passed\n";
  return 0;
}
