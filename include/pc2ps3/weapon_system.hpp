#pragma once

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

struct WeaponProfile {
 std::uint32_t weapon_id;
 std::uint32_t cooldown_ticks;
};

struct WeaponFireEvent {
 std::uint32_t weapon_id;
};

// Bounded weapon selection and fire-event timing. Damage, ammo, and projectiles
// remain caller-owned.
class WeaponSystem {
 public:
 enum { kMaxWeapons = 16, kMaxCooldownTicks = 65535 };

 WeaponSystem(const WeaponProfile* profiles, std::size_t profile_count);

 bool valid() const;
 bool select(std::uint32_t weapon_id);
 bool selected_weapon(std::uint32_t& weapon_id) const;
 bool cooldown_remaining(std::uint32_t weapon_id,
                         std::uint32_t& ticks) const;
 bool step(bool fire_pressed, WeaponFireEvent& event, bool& fired);
 void reset();

 private:
 const WeaponProfile* profiles_;
 std::size_t profile_count_;
 std::uint32_t cooldowns_[kMaxWeapons];
 std::size_t selected_index_;
 bool valid_;
};

} // namespace pc2ps3
