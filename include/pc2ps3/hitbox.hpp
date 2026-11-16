#pragma once

#include "pc2ps3/physics.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

struct HitEvent {
  std::uint32_t attack_id;
  std::uint32_t attacker_id;
  std::uint32_t hurtbox_id;
  std::uint32_t target_id;
};

// Register current-tick boxes, then collect overlaps in insertion order.
class HitboxSystem {
 public:
  enum { kMaxBoxesPerKind = 128 };

  HitboxSystem(std::size_t hitbox_capacity, std::size_t hurtbox_capacity);
  HitboxSystem(const HitboxSystem&) = delete;
  HitboxSystem& operator=(const HitboxSystem&) = delete;

  bool valid() const;
  void begin_tick();
  bool add_hitbox(std::uint32_t attack_id, std::uint32_t owner_id,
                  const Aabb& bounds);
  bool add_hurtbox(std::uint32_t hurtbox_id, std::uint32_t owner_id,
                   const Aabb& bounds);
  bool update();
  void reset();

  const std::vector<HitEvent>& events() const;
  std::size_t hitbox_count() const;
  std::size_t hurtbox_count() const;

 private:
  struct Box {
    Box(std::uint32_t box_id, std::uint32_t box_owner, const Aabb& box_bounds)
        : id(box_id), owner_id(box_owner), bounds(box_bounds) {}
    std::uint32_t id;
    std::uint32_t owner_id;
    Aabb bounds;
  };

  static bool valid_bounds(const Aabb& bounds);
  static bool has_id(const std::vector<Box>& boxes, std::uint32_t id);

  std::size_t hitbox_capacity_;
  std::size_t hurtbox_capacity_;
  std::vector<Box> hitboxes_;
  std::vector<Box> hurtboxes_;
  std::vector<HitEvent> events_;
  bool valid_;
};

}  // namespace pc2ps3
