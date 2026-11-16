#pragma once

#include "pc2ps3/physics.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

struct EnemyState {
  std::uint32_t id;
  Aabb bounds;
  std::uint32_t health;
};

// Bounded enemy registry. Movement and AI remain caller-owned.
class EnemySystem {
 public:
  enum { kMaxCapacity = 128 };

  explicit EnemySystem(std::size_t capacity);
  EnemySystem(const EnemySystem&) = delete;
  EnemySystem& operator=(const EnemySystem&) = delete;

  bool valid() const;
  bool spawn(std::uint32_t id, const Aabb& bounds, std::uint32_t health);
  bool set_bounds(std::uint32_t id, const Aabb& bounds);
  bool damage(std::uint32_t id, std::uint32_t amount, bool& defeated);
  bool get(std::uint32_t id, EnemyState& output) const;
  void clear();
  std::size_t capacity() const;
  std::size_t active_count() const;

 private:
  struct Entry {
    Entry(std::uint32_t enemy_id, const Aabb& enemy_bounds,
          std::uint32_t enemy_health)
        : id(enemy_id), bounds(enemy_bounds), health(enemy_health), active(true) {}
    std::uint32_t id;
    Aabb bounds;
    std::uint32_t health;
    bool active;
  };

  static bool valid_bounds(const Aabb& bounds);
  std::size_t find(std::uint32_t id) const;

  std::size_t capacity_;
  std::vector<Entry> entries_;
  bool valid_;
};

}  // namespace pc2ps3
