#include "pc2ps3/weapon_system.hpp"

#include <cassert>
#include <cstddef>

namespace {

void test_weapon_capacity_cooldown_limit_and_reset() {
 pc2ps3::WeaponProfile profiles[pc2ps3::WeaponSystem::kMaxWeapons];
 for (std::size_t i = 0; i < pc2ps3::WeaponSystem::kMaxWeapons; ++i) {
  profiles[i].weapon_id = static_cast<std::uint32_t>(i + 1);
  profiles[i].cooldown_ticks = static_cast<std::uint32_t>(i);
 }
 pc2ps3::WeaponSystem bounded(profiles, pc2ps3::WeaponSystem::kMaxWeapons);
 assert(bounded.valid());

 pc2ps3::WeaponProfile too_many[pc2ps3::WeaponSystem::kMaxWeapons + 1];
 for (std::size_t i = 0; i < pc2ps3::WeaponSystem::kMaxWeapons + 1; ++i) {
  too_many[i].weapon_id = static_cast<std::uint32_t>(i + 1);
  too_many[i].cooldown_ticks = 0;
 }
 pc2ps3::WeaponSystem over_capacity(
     too_many, pc2ps3::WeaponSystem::kMaxWeapons + 1);
 assert(!over_capacity.valid());

 const pc2ps3::WeaponProfile max_cooldown[] = {{99, 65535}};
 pc2ps3::WeaponSystem max_cooldown_system(max_cooldown, 1);
 pc2ps3::WeaponFireEvent event = {0};
 bool fired = false;
 assert(max_cooldown_system.valid());
 assert(max_cooldown_system.step(true, event, fired) && fired);
 std::uint32_t remaining = 0;
 assert(max_cooldown_system.cooldown_remaining(99, remaining));
 assert(remaining == 65535);

 const pc2ps3::WeaponProfile reset_profiles[] = {{10, 3}, {11, 4}};
 pc2ps3::WeaponSystem reset_system(reset_profiles, 2);
 assert(reset_system.step(true, event, fired) && fired && event.weapon_id == 10);
 assert(reset_system.select(11));
 assert(reset_system.step(true, event, fired) && fired && event.weapon_id == 11);
 reset_system.reset();
 std::uint32_t selected = 0;
 assert(reset_system.selected_weapon(selected) && selected == 10);
 assert(reset_system.cooldown_remaining(10, remaining) && remaining == 0);
 assert(reset_system.cooldown_remaining(11, remaining) && remaining == 0);
}

} // namespace

int main() {
 const pc2ps3::WeaponProfile profiles[] = {{1, 2}, {2, 0}};
 pc2ps3::WeaponSystem weapons(profiles, 2);
 assert(weapons.valid());

 std::uint32_t selected = 99;
 assert(weapons.selected_weapon(selected) && selected == 1);
 pc2ps3::WeaponFireEvent event = {99};
 bool fired = false;
 assert(weapons.step(true, event, fired) && fired && event.weapon_id == 1);
 std::uint32_t remaining = 0;
 assert(weapons.cooldown_remaining(1, remaining) && remaining == 2);
 assert(weapons.step(false, event, fired) && !fired);
 assert(weapons.cooldown_remaining(1, remaining) && remaining == 1);
 assert(weapons.select(2));
 assert(weapons.cooldown_remaining(1, remaining) && remaining == 1);
 assert(weapons.step(true, event, fired) && fired && event.weapon_id == 2);
 assert(weapons.cooldown_remaining(1, remaining) && remaining == 0);
 assert(weapons.select(1));
 assert(weapons.step(true, event, fired) && fired && event.weapon_id == 1);
 assert(weapons.cooldown_remaining(1, remaining) && remaining == 2);
 assert(weapons.select(2));
 assert(weapons.step(true, event, fired) && fired && event.weapon_id == 2);
 assert(!weapons.select(44));
 assert(weapons.selected_weapon(selected) && selected == 2);

 weapons.reset();
 assert(weapons.selected_weapon(selected) && selected == 1);
 assert(weapons.cooldown_remaining(1, remaining) && remaining == 0);

 const pc2ps3::WeaponProfile duplicate[] = {{3, 1}, {3, 2}};
 const pc2ps3::WeaponProfile bad_cooldown[] = {{4, 65536}};
 pc2ps3::WeaponSystem invalid_duplicate(duplicate, 2);
 pc2ps3::WeaponSystem invalid_cooldown(bad_cooldown, 1);
 pc2ps3::WeaponSystem empty(0, 0);
 assert(!invalid_duplicate.valid() && !invalid_cooldown.valid() &&
        !empty.valid());
 assert(!invalid_duplicate.step(true, event, fired) && !fired);
 assert(!invalid_duplicate.selected_weapon(selected));
 assert(weapons.step(false, event, fired) && !fired);
 test_weapon_capacity_cooldown_limit_and_reset();
 return 0;
}
