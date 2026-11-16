#include "pc2ps3/projectile_pattern.hpp"

#include <cassert>
#include <cstddef>

namespace {
pc2ps3::ProjectileSpawn spawn(std::uint32_t id) {
 pc2ps3::ProjectileSpawn projectile = {
     id, 10, 20, 2, 2, 3, -4, 1000, {255, 128, 64, 255}};
 return projectile;
}
} // namespace

int main() {
 const pc2ps3::ProjectileStep steps[] = {
     {0, spawn(1)}, {0, spawn(2)}, {2, spawn(3)}};
 pc2ps3::ProjectilePattern pattern(steps, 3);
 assert(pattern.valid() && !pattern.finished());

 pc2ps3::ProjectileSpawn output[2] = {spawn(90), spawn(91)};
 std::size_t count = 9;
 assert(!pattern.advance(output, 1, count));
 assert(count == 0 && output[0].id == 90 && pattern.current_tick() == 0);

 assert(pattern.advance(output, 2, count));
 assert(count == 2 && output[0].id == 1 && output[1].id == 2);
 assert(pattern.current_tick() == 1);
 assert(pattern.advance(0, 0, count));
 assert(count == 0 && pattern.current_tick() == 2);
 assert(pattern.advance(output, 2, count));
 assert(count == 1 && output[0].id == 3 && pattern.finished());
 assert(pattern.advance(0, 0, count) && count == 0);

 pattern.reset();
 assert(!pattern.finished() && pattern.current_tick() == 0);
 assert(pattern.advance(output, 2, count));
 assert(count == 2 && output[0].id == 1 && output[1].id == 2);

 const pc2ps3::ProjectileStep descending[] = {
     {2, spawn(4)}, {1, spawn(5)}};
 pc2ps3::ProjectilePattern invalid_order(descending, 2);
 pc2ps3::ProjectilePattern null_steps(0, 1);
 pc2ps3::ProjectilePattern empty_steps(0, 0);
 pc2ps3::ProjectilePattern too_many(steps,
     static_cast<std::size_t>(pc2ps3::ProjectilePattern::kMaxSteps) + 1);
 assert(!invalid_order.valid() && !invalid_order.advance(output, 2, count));
 assert(!null_steps.valid() && !empty_steps.valid() && !too_many.valid());
 return 0;
}
