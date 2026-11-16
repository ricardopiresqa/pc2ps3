#pragma once

#include "pc2ps3/hitbox.hpp"

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

struct AttackProfile {
 std::uint32_t attack_id;
 std::uint32_t damage;
};

struct DamageEvent {
 std::uint32_t attack_id;
 std::uint32_t attacker_id;
 std::uint32_t target_id;
 std::uint32_t damage;
};

// Converts one tick of contacts to caller-owned damage events. It never
// changes health or decides invulnerability, stun, or rewards.
class CombatResolver {
 public:
 enum {
  kMaxAttackProfiles = HitboxSystem::kMaxBoxesPerKind,
  kMaxContacts = HitboxSystem::kMaxBoxesPerKind *
                 HitboxSystem::kMaxBoxesPerKind
 };

 static bool resolve(const HitEvent* hits, std::size_t hit_count,
                     const AttackProfile* profiles,
                     std::size_t profile_count, DamageEvent* output,
                     std::size_t output_capacity,
                     std::size_t& output_count);
};

} // namespace pc2ps3
