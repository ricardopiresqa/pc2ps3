#include "pc2ps3/boss_encounter.hpp"
#include "pc2ps3/boss_pattern.hpp"
#include "pc2ps3/combat_resolution.hpp"
#include "pc2ps3/hitbox.hpp"
#include "pc2ps3/parry_window.hpp"
#include "pc2ps3/projectile_pattern.hpp"
#include "pc2ps3/weapon_system.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>

int main() {
  const pc2ps3::WeaponProfile weapons[] = {{1, 1}};
  pc2ps3::WeaponSystem weapon_system(weapons, 1);
  assert(weapon_system.valid() && weapon_system.select(1));
  pc2ps3::WeaponFireEvent fire = {0};
  bool fired = false;
  assert(weapon_system.step(true, fire, fired) && fired);

  const pc2ps3::ProjectileSpawn authored_projectile = {
      fire.weapon_id, 0, 0, 8, 8, 0, 0, 1000,
      pc2ps3::Color{255, 255, 255, 255}};
  const pc2ps3::ProjectileStep projectile_steps[] = {
      {0, authored_projectile}};
  pc2ps3::ProjectilePattern projectile_pattern(projectile_steps, 1);
  pc2ps3::ProjectileSpawn spawned[1] = {};
  std::size_t spawn_count = 0;
  assert(projectile_pattern.valid());
  assert(projectile_pattern.advance(spawned, 1, spawn_count));
  assert(spawn_count == 1 && spawned[0].id == fire.weapon_id);

  pc2ps3::HitboxSystem hitboxes(1, 1);
  assert(hitboxes.valid());
  assert(hitboxes.add_hitbox(
      spawned[0].id, 1, pc2ps3::Aabb{0.0f, 0.0f, 8.0f, 8.0f}));
  assert(hitboxes.add_hurtbox(
      20, 2, pc2ps3::Aabb{2.0f, 2.0f, 6.0f, 6.0f}));
  assert(hitboxes.update());
  assert(hitboxes.events().size() == 1);

  const pc2ps3::AttackProfile attack_profiles[] = {{spawned[0].id, 6}};
  pc2ps3::DamageEvent damage_events[1] = {};
  std::size_t damage_count = 0;
  assert(pc2ps3::CombatResolver::resolve(
      hitboxes.events().data(), hitboxes.events().size(), attack_profiles, 1,
      damage_events, 1, damage_count));
  assert(damage_count == 1 && damage_events[0].damage == 6 &&
         damage_events[0].target_id == 2);

  const std::uint32_t phase_thresholds[] = {5};
  pc2ps3::BossEncounter boss(10, phase_thresholds, 1);
  assert(boss.valid());
  const pc2ps3::BossActionStep phase_zero_steps[] = {{0, 100}};
  const pc2ps3::BossActionStep phase_one_steps[] = {{0, 200}};
  const pc2ps3::BossPhaseSequence phases[] = {
      {0, phase_zero_steps, 1}, {1, phase_one_steps, 1}};
  pc2ps3::BossPatternTimeline boss_patterns(phases, 2);
  assert(boss_patterns.valid() && boss_patterns.begin_phase(0));
  std::uint32_t actions[1] = {0};
  std::size_t action_count = 0;
  assert(boss_patterns.advance(actions, 1, action_count));
  assert(action_count == 1 && actions[0] == 100);

  bool phase_changed = false;
  bool defeated = false;
  assert(boss.damage(damage_events[0].damage, phase_changed, defeated));
  assert(phase_changed && !defeated && boss.phase() == 1 &&
         boss.health() == 4);
  assert(boss_patterns.begin_phase(static_cast<std::uint32_t>(boss.phase())));
  assert(boss_patterns.advance(actions, 1, action_count));
  assert(action_count == 1 && actions[0] == 200);

  pc2ps3::ParryWindow parry(1);
  assert(parry.valid() && parry.step(true));
  assert(parry.accepts_contact(true));
  assert(!parry.accepts_contact(false));
  assert(!parry.step(false));
  assert(!parry.accepts_contact(true));
  return 0;
}
