#include "pc2ps3/checkpoint.hpp"

#include <cmath>
#include <new>

namespace pc2ps3 {

CheckpointSystem::CheckpointSystem(std::size_t capacity, Vec2 initial_spawn)
    : capacity_(0),
      initial_spawn_(initial_spawn),
      active_spawn_(initial_spawn),
      active_id_(0),
      has_active_(false),
      valid_(false) {
  if (capacity == 0 || capacity > kMaxCapacity ||
      !valid_position(initial_spawn)) {
    return;
  }
  try {
    entries_.reserve(capacity);
  } catch (const std::bad_alloc&) {
    return;
  }
  capacity_ = capacity;
  valid_ = true;
}

bool CheckpointSystem::valid() const { return valid_; }

bool CheckpointSystem::add(std::uint32_t id, Vec2 spawn_position) {
  if (!valid_ || entries_.size() >= capacity_ ||
      !valid_position(spawn_position)) {
    return false;
  }
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].id == id) return false;
  }
  entries_.push_back(Entry(id, spawn_position));
  return true;
}

bool CheckpointSystem::activate(std::uint32_t id) {
  if (!valid_) return false;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].id == id) {
      active_id_ = id;
      active_spawn_ = entries_[i].position;
      has_active_ = true;
      return true;
    }
  }
  return false;
}

bool CheckpointSystem::respawn_position(Vec2& output) const {
  if (!valid_) return false;
  output = has_active_ ? active_spawn_ : initial_spawn_;
  return true;
}

bool CheckpointSystem::active_checkpoint_id(std::uint32_t& output) const {
  if (!valid_ || !has_active_) return false;
  output = active_id_;
  return true;
}

void CheckpointSystem::reset() {
  if (!valid_) return;
  active_spawn_ = initial_spawn_;
  active_id_ = 0;
  has_active_ = false;
}

std::size_t CheckpointSystem::checkpoint_count() const { return entries_.size(); }

bool CheckpointSystem::valid_position(Vec2 position) {
  return std::isfinite(position.x) && std::isfinite(position.y);
}

}  // namespace pc2ps3
