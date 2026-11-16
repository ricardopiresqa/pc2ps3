#include "pc2ps3/physics.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

bool near(float actual, float expected) {
  return std::fabs(actual - expected) < 0.0001f;
}

void expect_no_contact(const pc2ps3::Aabb& first, const pc2ps3::Aabb& second) {
  const pc2ps3::AabbContact contact = pc2ps3::query_aabb(first, second);
  assert(!contact.overlapping);
  assert(contact.normal.x == 0.0f);
  assert(contact.normal.y == 0.0f);
  assert(contact.penetration == 0.0f);
}

}  // namespace

int main() {
  using pc2ps3::Aabb;
  using pc2ps3::AabbContact;
  const Aabb base = {0.0f, 0.0f, 10.0f, 10.0f};

  AabbContact contact = pc2ps3::query_aabb(base, Aabb{8.0f, 2.0f, 14.0f, 8.0f});
  assert(contact.overlapping);
  assert(contact.normal.x == 1.0f && contact.normal.y == 0.0f);
  assert(near(contact.penetration, 2.0f));

  contact = pc2ps3::query_aabb(base, Aabb{2.0f, 8.0f, 8.0f, 14.0f});
  assert(contact.overlapping);
  assert(contact.normal.x == 0.0f && contact.normal.y == 1.0f);
  assert(near(contact.penetration, 2.0f));

  contact = pc2ps3::query_aabb(base, Aabb{2.0f, 3.0f, 4.0f, 4.0f});
  assert(contact.overlapping);
  assert(contact.normal.x == -1.0f && contact.normal.y == 0.0f);
  assert(near(contact.penetration, 4.0f));

  contact = pc2ps3::query_aabb(base, base);
  assert(contact.overlapping);
  assert(contact.normal.x == 1.0f && contact.normal.y == 0.0f);
  assert(near(contact.penetration, 10.0f));

  contact = pc2ps3::query_aabb(Aabb{8.0f, 0.0f, 14.0f, 10.0f},
                               Aabb{0.0f, 2.0f, 10.0f, 8.0f});
  assert(contact.overlapping);
  assert(contact.normal.x == -1.0f && contact.normal.y == 0.0f);
  assert(near(contact.penetration, 2.0f));

  expect_no_contact(base, Aabb{10.0f, 0.0f, 12.0f, 10.0f});
  expect_no_contact(base, Aabb{0.0f, 10.0f, 10.0f, 12.0f});
  expect_no_contact(base, Aabb{1.0f, 1.0f, 1.0f, 4.0f});
  expect_no_contact(Aabb{std::numeric_limits<float>::quiet_NaN(), 0.0f, 2.0f, 2.0f}, base);
  expect_no_contact(Aabb{0.0f, 0.0f, 16777216.0f, 10.0f},
                    Aabb{16777216.0f, 0.0f, 16777218.0f, 10.0f});

  const pc2ps3::WorldConfig wall_config = {1.0f/60.0f, 1500.0f, 2200.0f, 280.0f, 560.0f, 1000.0f};
pc2ps3::World wall_world(wall_config);
wall_world.add_solid(Aabb{90.0f, 900.0f, 100.0f, 1000.0f});
pc2ps3::InputFrame move_right = {false, true, false, false};
for (int frame = 0; frame < 60; ++frame) {
wall_world.step(move_right);
}
assert(near(wall_world.player().position.x, 78.0f));
assert(wall_world.player().velocity.x == 0.0f);
pc2ps3::World platform_world(wall_config);
platform_world.add_solid(Aabb{90.0f, 950.0f, 200.0f, 990.0f});
pc2ps3::InputFrame approach = {false, true, true, true};
platform_world.step(approach);
pc2ps3::InputFrame move_toward_platform = {false, true, false, true};
for (int frame = 0; frame < 14; ++frame) {
platform_world.step(move_toward_platform);
}
pc2ps3::InputFrame idle = {false, false, false, false};
for (int frame = 15; frame < 60; ++frame) {
platform_world.step(idle);
}
assert(near(platform_world.player().position.y, 950.0f));
assert(platform_world.player().grounded);
pc2ps3::InputFrame jump = {false, false, true, false};
platform_world.step(jump);
assert(platform_world.player().position.y < 950.0f);
assert(!platform_world.player().grounded);
return 0;
}