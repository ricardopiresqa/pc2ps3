#include "pc2ps3/enemy.hpp"

#include <cassert>
#include <limits>

int main() {
  pc2ps3::EnemySystem enemies(2);
  assert(enemies.valid());
  assert(enemies.spawn(10, pc2ps3::Aabb{0.0f, 0.0f, 10.0f, 10.0f}, 100));
  assert(enemies.spawn(11, pc2ps3::Aabb{20.0f, 0.0f, 30.0f, 10.0f}, 5));
  assert(!enemies.spawn(12, pc2ps3::Aabb{40.0f, 0.0f, 50.0f, 10.0f}, 5));
  assert(!enemies.spawn(10, pc2ps3::Aabb{40.0f, 0.0f, 50.0f, 10.0f}, 5));

  pc2ps3::EnemyState state = {0, pc2ps3::Aabb{0.0f, 0.0f, 0.0f, 0.0f}, 0};
  assert(enemies.get(10, state));
  assert(state.health == 100);
  bool defeated = true;
  assert(enemies.damage(10, 40, defeated));
  assert(!defeated);
  assert(enemies.get(10, state) && state.health == 60);
  assert(!enemies.damage(10, 0, defeated));
  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(!enemies.set_bounds(10, pc2ps3::Aabb{nan, 0.0f, 1.0f, 1.0f}));
  assert(enemies.get(10, state) && state.bounds.left == 0.0f);

  assert(enemies.damage(11, 5, defeated));
  assert(defeated);
  assert(!enemies.get(11, state));
  assert(enemies.active_count() == 1);
  assert(enemies.spawn(12, pc2ps3::Aabb{40.0f, 0.0f, 50.0f, 10.0f}, 5));
  assert(enemies.active_count() == 2);
  enemies.clear();
  assert(enemies.active_count() == 0);
  pc2ps3::EnemySystem invalid(0);
  assert(!invalid.valid());
  assert(!invalid.spawn(1, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}, 1));
  pc2ps3::EnemySystem invalid_health(1);
  assert(!invalid_health.spawn(1, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}, 0));
  return 0;
}
