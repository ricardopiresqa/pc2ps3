#include "pc2ps3/knockback.hpp"

#include <cassert>
#include <limits>

int main() {
  pc2ps3::Actor actor = {pc2ps3::Vec2{0.0f, 0.0f},
                         pc2ps3::Vec2{3.0f, 4.0f}, true};
  assert(pc2ps3::apply_knockback_impulse(actor, pc2ps3::Vec2{-5.0f, 8.0f}));
  assert(actor.velocity.x == -2.0f);
  assert(actor.velocity.y == 12.0f);
  assert(!actor.grounded);

  actor.grounded = true;
  assert(pc2ps3::apply_knockback_impulse(actor, pc2ps3::Vec2{2.0f, 0.0f}));
  assert(actor.velocity.x == 0.0f);
  assert(actor.grounded);

  const pc2ps3::Vec2 before = actor.velocity;
  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(!pc2ps3::apply_knockback_impulse(actor, pc2ps3::Vec2{nan, 1.0f}));
  assert(actor.velocity.x == before.x && actor.velocity.y == before.y);
  actor.velocity.x = std::numeric_limits<float>::max();
  const float max_before = actor.velocity.x;
  assert(!pc2ps3::apply_knockback_impulse(
      actor, pc2ps3::Vec2{std::numeric_limits<float>::max(), 0.0f}));
  assert(actor.velocity.x == max_before);

  const pc2ps3::WorldConfig config = {1.0f / 60.0f, 1500.0f, 2200.0f,
                                      280.0f, 560.0f, 360.0f};
  pc2ps3::World world(config);
  assert(world.apply_knockback(pc2ps3::Vec2{10.0f, -5.0f}));
  assert(world.player().velocity.x == 10.0f);
  assert(world.player().velocity.y == -5.0f);
  assert(!world.player().grounded);
  return 0;
}
