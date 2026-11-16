#pragma once

#include "pc2ps3/physics.hpp"
#include "pc2ps3/render.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

struct ProjectileSpawn {
  std::uint32_t id;
  int x;
  int y;
  int width;
  int height;
  std::int32_t velocity_x;
  std::int32_t velocity_y;
  std::uint32_t lifetime_ms;
  Color color;
};

struct ProjectileHit {
  std::uint32_t projectile_id;
};

// Bounded single-threaded pool. Collision checks use the current AABB per tick.
class ProjectileSystem {
 public:
  enum {
    kMaxCapacity = 1024,
    kMaxLifetimeMs = 600000,
    kMaxVelocityPixelsPerSecond = 1000000,
    kMaxSize = 64,
    kMaxUpdateMs = 1000,
    kMaxWorldCoordinate = 1000000
  };

  explicit ProjectileSystem(std::size_t capacity);
  ProjectileSystem(const ProjectileSystem&) = delete;
  ProjectileSystem& operator=(const ProjectileSystem&) = delete;

  bool valid() const;
  bool spawn(const ProjectileSpawn& projectile);
  bool update(std::uint32_t dt_ms);
  bool collide(const Aabb& target_bounds);
  void draw(Framebuffer& framebuffer, int camera_x = 0,
            int camera_y = 0) const;
  void clear();

  const std::vector<ProjectileHit>& hits() const;
  std::size_t capacity() const;
  std::size_t active_count() const;

 private:
  struct Slot {
    Slot();
    std::uint32_t id;
    std::int64_t x_q16;
    std::int64_t y_q16;
    std::int32_t velocity_x;
    std::int32_t velocity_y;
    std::uint32_t remaining_ms;
    int width;
    int height;
    Color color;
    bool active;
  };

  static bool valid_target(const Aabb& bounds);
  static std::int64_t floor_q16(std::int64_t value);
  static Aabb slot_bounds(const Slot& slot);

  bool valid_;
  std::vector<Slot> slots_;
  std::vector<ProjectileHit> hits_;
};

}  // namespace pc2ps3
