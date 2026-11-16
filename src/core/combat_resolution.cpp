#include "pc2ps3/combat_resolution.hpp"

#include <algorithm>

namespace pc2ps3 {
namespace {

bool damage_event_less(const DamageEvent& left, const DamageEvent& right) {
 if (left.attack_id != right.attack_id) {
  return left.attack_id < right.attack_id;
 }
 if (left.target_id != right.target_id) {
  return left.target_id < right.target_id;
 }
 return left.attacker_id < right.attacker_id;
}

const AttackProfile* find_profile(const AttackProfile* profiles,
                                  std::size_t profile_count,
                                  std::uint32_t attack_id) {
 for (std::size_t i = 0; i < profile_count; ++i) {
  if (profiles[i].attack_id == attack_id) {
   return &profiles[i];
  }
 }
 return 0;
}

} // namespace

bool CombatResolver::resolve(const HitEvent* hits, std::size_t hit_count,
                             const AttackProfile* profiles,
                             std::size_t profile_count, DamageEvent* output,
                             std::size_t output_capacity,
                             std::size_t& output_count) {
 output_count = 0;
 if (profile_count > kMaxAttackProfiles || hit_count > kMaxContacts ||
     (profile_count > 0 && !profiles) || (hit_count > 0 && (!hits || !output)) ||
     output_capacity < hit_count) {
  return false;
 }

 for (std::size_t i = 0; i < profile_count; ++i) {
  if (profiles[i].damage == 0) {
   return false;
  }
  for (std::size_t j = 0; j < i; ++j) {
   if (profiles[j].attack_id == profiles[i].attack_id) {
    return false;
   }
  }
 }

 for (std::size_t i = 0; i < hit_count; ++i) {
  if (!find_profile(profiles, profile_count, hits[i].attack_id)) {
   return false;
  }
 }

 for (std::size_t i = 0; i < hit_count; ++i) {
  const AttackProfile* profile =
      find_profile(profiles, profile_count, hits[i].attack_id);
  output[i].attack_id = hits[i].attack_id;
  output[i].attacker_id = hits[i].attacker_id;
  output[i].target_id = hits[i].target_id;
  output[i].damage = profile->damage;
 }

 if (hit_count == 0) {
  return true;
 }

 std::sort(output, output + hit_count, damage_event_less);
 for (std::size_t i = 0; i < hit_count; ++i) {
  if (output_count > 0 &&
      output[output_count - 1].attack_id == output[i].attack_id &&
      output[output_count - 1].target_id == output[i].target_id) {
   continue;
  }
  if (output_count != i) {
   output[output_count] = output[i];
  }
  ++output_count;
 }
 return true;
}

} // namespace pc2ps3
