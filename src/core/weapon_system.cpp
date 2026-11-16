#include "pc2ps3/weapon_system.hpp"

namespace pc2ps3 {

WeaponSystem::WeaponSystem(const WeaponProfile* profiles,
                           std::size_t profile_count)
    : profiles_(profiles), profile_count_(profile_count), selected_index_(0),
      valid_(profiles != 0 && profile_count > 0 &&
             profile_count <= kMaxWeapons) {
 for (std::size_t i = 0; i < kMaxWeapons; ++i) {
  cooldowns_[i] = 0;
 }
 if (!valid_) {
  return;
 }
 for (std::size_t i = 0; i < profile_count_; ++i) {
  if (profiles_[i].cooldown_ticks > kMaxCooldownTicks) {
   valid_ = false;
   return;
  }
  for (std::size_t j = 0; j < i; ++j) {
   if (profiles_[j].weapon_id == profiles_[i].weapon_id) {
    valid_ = false;
    return;
   }
  }
 }
}

bool WeaponSystem::valid() const { return valid_; }

bool WeaponSystem::select(std::uint32_t weapon_id) {
 if (!valid_) {
  return false;
 }
 for (std::size_t i = 0; i < profile_count_; ++i) {
  if (profiles_[i].weapon_id == weapon_id) {
   selected_index_ = i;
   return true;
  }
 }
 return false;
}

bool WeaponSystem::selected_weapon(std::uint32_t& weapon_id) const {
 if (!valid_) {
  return false;
 }
 weapon_id = profiles_[selected_index_].weapon_id;
 return true;
}

bool WeaponSystem::cooldown_remaining(std::uint32_t weapon_id,
                                      std::uint32_t& ticks) const {
 if (!valid_) {
  return false;
 }
 for (std::size_t i = 0; i < profile_count_; ++i) {
  if (profiles_[i].weapon_id == weapon_id) {
   ticks = cooldowns_[i];
   return true;
  }
 }
 return false;
}

bool WeaponSystem::step(bool fire_pressed, WeaponFireEvent& event,
                        bool& fired) {
 fired = false;
 if (!valid_) {
  return false;
 }
 for (std::size_t i = 0; i < profile_count_; ++i) {
  if (cooldowns_[i] > 0) {
   --cooldowns_[i];
  }
 }
 if (!fire_pressed || cooldowns_[selected_index_] > 0) {
  return true;
 }
 event.weapon_id = profiles_[selected_index_].weapon_id;
 cooldowns_[selected_index_] = profiles_[selected_index_].cooldown_ticks;
 fired = true;
 return true;
}

void WeaponSystem::reset() {
 selected_index_ = 0;
 for (std::size_t i = 0; i < kMaxWeapons; ++i) {
  cooldowns_[i] = 0;
 }
}

} // namespace pc2ps3
