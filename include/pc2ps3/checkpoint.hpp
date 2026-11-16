#pragma once

#include "pc2ps3/core.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

// Stores respawn points. Gameplay activates one after receiving a trigger enter.
class CheckpointSystem {
 public:
  enum { kMaxCapacity = 128 };

  CheckpointSystem(std::size_t capacity, Vec2 initial_spawn);
  CheckpointSystem(const CheckpointSystem&) = delete;
  CheckpointSystem& operator=(const CheckpointSystem&) = delete;

  bool valid() const;
  bool add(std::uint32_t id, Vec2 spawn_position);
  bool activate(std::uint32_t id);
  bool respawn_position(Vec2& output) const;
  bool active_checkpoint_id(std::uint32_t& output) const;
  void reset();
  std::size_t checkpoint_count() const;

 private:
  struct Entry {
    Entry(std::uint32_t entry_id, Vec2 entry_position)
        : id(entry_id), position(entry_position) {}
    std::uint32_t id;
    Vec2 position;
  };

  static bool valid_position(Vec2 position);

  std::size_t capacity_;
  Vec2 initial_spawn_;
  Vec2 active_spawn_;
  std::uint32_t active_id_;
  std::vector<Entry> entries_;
  bool has_active_;
  bool valid_;
};

}  // namespace pc2ps3
