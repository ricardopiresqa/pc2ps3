#include "pc2ps3/hitbox.hpp"

#include <cassert>
#include <limits>

int main() {
  pc2ps3::HitboxSystem combat(2, 3);
  assert(combat.valid());
  assert(combat.add_hitbox(10, 1, pc2ps3::Aabb{0.0f, 0.0f, 10.0f, 10.0f}));
  assert(combat.add_hitbox(11, 2, pc2ps3::Aabb{5.0f, 0.0f, 15.0f, 10.0f}));
  assert(combat.add_hurtbox(20, 1, pc2ps3::Aabb{1.0f, 1.0f, 4.0f, 4.0f}));
  assert(combat.add_hurtbox(21, 3, pc2ps3::Aabb{2.0f, 2.0f, 8.0f, 8.0f}));
  assert(combat.add_hurtbox(22, 4, pc2ps3::Aabb{7.0f, 2.0f, 12.0f, 8.0f}));
  assert(combat.update());
  assert(combat.events().size() == 4);
  assert(combat.events()[0].attack_id == 10);
  assert(combat.events()[0].hurtbox_id == 21);
  assert(combat.events()[1].attack_id == 10);
  assert(combat.events()[1].hurtbox_id == 22);
  assert(combat.events()[2].attack_id == 11);
  assert(combat.events()[2].hurtbox_id == 21);
  assert(combat.events()[3].attack_id == 11);
  assert(combat.events()[3].hurtbox_id == 22);
  assert(combat.update());
  assert(combat.events().size() == 4);

  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(!combat.add_hitbox(12, 5, pc2ps3::Aabb{nan, 0.0f, 1.0f, 1.0f}));
  assert(!combat.add_hitbox(12, 5, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}));
  assert(!combat.add_hitbox(10, 5, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}));
  assert(!combat.add_hurtbox(21, 5, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}));
  assert(!combat.add_hurtbox(23, 5, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}));
  assert(combat.hitbox_count() == 2);
  assert(combat.hurtbox_count() == 3);
  combat.begin_tick();
  assert(combat.events().empty());
  assert(combat.hitbox_count() == 0);
  assert(combat.add_hitbox(12, 5, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}));
  assert(combat.add_hurtbox(23, 6, pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}));
  assert(combat.update());
  assert(combat.events().size() == 1);
  assert(combat.events()[0].attack_id == 12);
  combat.reset();
  assert(combat.events().empty());
  assert(combat.hitbox_count() == 0);
  assert(combat.hurtbox_count() == 0);

  pc2ps3::HitboxSystem invalid(0, 1);
  assert(!invalid.valid());
  assert(!invalid.update());
  pc2ps3::HitboxSystem too_many(pc2ps3::HitboxSystem::kMaxBoxesPerKind + 1, 1);
  assert(!too_many.valid());
  return 0;
}
