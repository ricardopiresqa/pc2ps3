#include "pc2ps3/combat_resolution.hpp"

#include <cassert>
#include <cstddef>

int main() {
 using pc2ps3::AttackProfile;
 using pc2ps3::CombatResolver;
 using pc2ps3::DamageEvent;
 using pc2ps3::HitEvent;

 const AttackProfile profiles[] = {{11, 5}, {10, 3}};
 const HitEvent hits[] = {
     {11, 2, 21, 4}, {10, 1, 20, 4}, {10, 1, 21, 4}, {10, 1, 22, 5}};
 DamageEvent output[4] = {};
 std::size_t count = 99;
 assert(CombatResolver::resolve(hits, 4, profiles, 2, output, 4, count));
 assert(count == 3);
 assert(output[0].attack_id == 10 && output[0].target_id == 4);
 assert(output[0].attacker_id == 1 && output[0].damage == 3);
 assert(output[1].attack_id == 10 && output[1].target_id == 5);
 assert(output[2].attack_id == 11 && output[2].target_id == 4);

 DamageEvent unchanged[2] = {{91, 92, 93, 94}, {81, 82, 83, 84}};
 const HitEvent unknown[] = {{77, 1, 2, 3}};
 count = 7;
 assert(!CombatResolver::resolve(unknown, 1, profiles, 2, unchanged, 2,
                                count));
 assert(count == 0 && unchanged[0].attack_id == 91);

 const AttackProfile duplicate[] = {{10, 1}, {10, 2}};
 count = 7;
 assert(!CombatResolver::resolve(hits, 1, duplicate, 2, unchanged, 2, count));
 assert(count == 0 && unchanged[0].attack_id == 91);

 count = 7;
 assert(!CombatResolver::resolve(hits, 2, profiles, 2, unchanged, 1, count));
 assert(count == 0 && unchanged[0].attack_id == 91);

 count = 7;
 assert(CombatResolver::resolve(0, 0, profiles, 2, 0, 0, count));
 assert(count == 0);
 assert(!CombatResolver::resolve(0, 1, profiles, 2, unchanged, 2, count));
 assert(count == 0 && unchanged[0].attack_id == 91);
 return 0;
}
