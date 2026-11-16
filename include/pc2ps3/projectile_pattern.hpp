#pragma once

#include "pc2ps3/projectiles.hpp"

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

struct ProjectileStep {
 std::uint32_t tick_offset;
 ProjectileSpawn projectile;
};

// Emits borrowed, caller-authored projectile spawns in tick order.
class ProjectilePattern {
 public:
 enum { kMaxSteps = 128 };

 ProjectilePattern(const ProjectileStep* steps, std::size_t step_count);

 bool valid() const;
 bool finished() const;
 std::uint32_t current_tick() const;
 // On insufficient output capacity, the cursor and output buffer stay intact.
 bool advance(ProjectileSpawn* output, std::size_t output_capacity,
              std::size_t& output_count);
 void reset();

 private:
 const ProjectileStep* steps_;
 std::size_t step_count_;
 std::size_t cursor_;
 std::uint32_t current_tick_;
 bool valid_;
};

} // namespace pc2ps3
