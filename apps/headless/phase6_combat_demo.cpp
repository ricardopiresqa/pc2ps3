#include "pc2ps3/boss_encounter.hpp"
#include "pc2ps3/boss_pattern.hpp"
#include "pc2ps3/combat_resolution.hpp"
#include "pc2ps3/hitbox.hpp"
#include "pc2ps3/parry_window.hpp"
#include "pc2ps3/projectile_pattern.hpp"
#include "pc2ps3/weapon_system.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

int fail(const char* message) {
  std::cerr << "phase6_combat_demo: " << message << '\n';
  return 1;
}

}  // namespace

int main() {
  const pc2ps3::WeaponProfile weapons[] = {{1, 1}};
  pc2ps3::WeaponSystem weapon_system(weapons, 1);
  if (!weapon_system.valid() || !weapon_system.select(1)) {
    return fail("weapon setup failed");
  }

  pc2ps3::WeaponFireEvent fire = {0};
  bool fired = false;
  if (!weapon_system.step(true, fire, fired) || !fired) {
    return fail("weapon did not fire");
  }

  const pc2ps3::ProjectileSpawn authored_projectile = {
      fire.weapon_id, 0, 0, 8, 8, 0, 0, 1000,
      pc2ps3::Color{255, 255, 255, 255}};
  const pc2ps3::ProjectileStep projectile_steps[] = {
      {0, authored_projectile}};
  pc2ps3::ProjectilePattern projectile_pattern(projectile_steps, 1);
  pc2ps3::ProjectileSpawn spawned[1] = {};
  std::size_t spawn_count = 0;
  if (!projectile_pattern.valid() ||
      !projectile_pattern.advance(spawned, 1, spawn_count) ||
      spawn_count != 1 || spawned[0].id != fire.weapon_id) {
    return fail("authored projectile spawn failed");
  }

  pc2ps3::HitboxSystem hitboxes(1, 1);
  if (!hitboxes.valid() ||
      !hitboxes.add_hitbox(
          spawned[0].id, 1, pc2ps3::Aabb{0.0f, 0.0f, 8.0f, 8.0f}) ||
      !hitboxes.add_hurtbox(
          20, 2, pc2ps3::Aabb{2.0f, 2.0f, 6.0f, 6.0f}) ||
      !hitboxes.update() || hitboxes.events().size() != 1) {
    return fail("synthetic hit event failed");
  }

  const pc2ps3::AttackProfile attack_profiles[] = {{spawned[0].id, 6}};
  pc2ps3::DamageEvent damage_events[1] = {};
  std::size_t damage_count = 0;
  if (!pc2ps3::CombatResolver::resolve(
          hitboxes.events().data(), hitboxes.events().size(), attack_profiles,
          1, damage_events, 1, damage_count) ||
      damage_count != 1 || damage_events[0].damage != 6 ||
      damage_events[0].target_id != 2) {
    return fail("damage resolution failed");
  }

  const std::uint32_t phase_thresholds[] = {5};
  pc2ps3::BossEncounter boss(10, phase_thresholds, 1);
  const pc2ps3::BossActionStep phase_zero_steps[] = {{0, 100}};
  const pc2ps3::BossActionStep phase_one_steps[] = {{0, 200}};
  const pc2ps3::BossPhaseSequence phases[] = {
      {0, phase_zero_steps, 1}, {1, phase_one_steps, 1}};
  pc2ps3::BossPatternTimeline boss_patterns(phases, 2);
  if (!boss.valid() || !boss_patterns.valid() ||
      !boss_patterns.begin_phase(0)) {
    return fail("boss setup failed");
  }

  std::uint32_t actions[1] = {0};
  std::size_t action_count = 0;
  if (!boss_patterns.advance(actions, 1, action_count) ||
      action_count != 1 || actions[0] != 100) {
    return fail("phase zero action failed");
  }

  bool phase_changed = false;
  bool defeated = false;
  if (!boss.damage(damage_events[0].damage, phase_changed, defeated) ||
      !phase_changed || defeated || boss.phase() != 1 || boss.health() != 4 ||
      !boss_patterns.begin_phase(static_cast<std::uint32_t>(boss.phase())) ||
      !boss_patterns.advance(actions, 1, action_count) || action_count != 1 ||
      actions[0] != 200) {
    return fail("boss phase transition failed");
  }

  pc2ps3::ParryWindow parry(1);
  const bool parry_accepted = parry.valid() && parry.step(true) &&
                              parry.accepts_contact(true);
  const bool non_parryable_rejected = !parry.accepts_contact(false);
  const bool expired_contact_rejected = !parry.step(false) &&
                                        !parry.accepts_contact(true);
  if (!parry_accepted || !non_parryable_rejected ||
      !expired_contact_rejected) {
    return fail("parry window behavior failed");
  }

  std::cout << "{\"fired\":true,\"spawn_count\":1,\"hit_count\":1,"
               "\"damage_count\":1,\"boss_health\":4,\"boss_phase\":1,"
               "\"phase0_action\":100,\"phase1_action\":200,"
               "\"parry_accepted\":true,\"non_parryable_rejected\":true,"
               "\"expired_contact_rejected\":true}\n";
  return 0;
}
