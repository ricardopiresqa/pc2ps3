#include "pc2ps3/enemy.hpp"

#include <new>

namespace pc2ps3 {

EnemySystem::EnemySystem(std::size_t capacity)
    : capacity_(0), valid_(false) {
  if (capacity == 0 || capacity > kMaxCapacity) return;
  try {
    entries_.reserve(capacity);
  } catch (const std::bad_alloc&) {
    return;
  }
  capacity_ = capacity;
  valid_ = true;
}

bool EnemySystem::valid() const { return valid_; }

bool EnemySystem::spawn(std::uint32_t id, const Aabb& bounds,
                        std::uint32_t health) {
  if (!valid_ || health == 0 || !valid_bounds(bounds)) return false;
  const std::size_t existing = find(id);
  if (existing != entries_.size()) return false;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (!entries_[i].active) {
      entries_[i] = Entry(id, bounds, health);
      return true;
    }
  }
  if (entries_.size() >= capacity_) return false;
  entries_.push_back(Entry(id, bounds, health));
  return true;
}

bool EnemySystem::set_bounds(std::uint32_t id, const Aabb& bounds) {
  if (!valid_ || !valid_bounds(bounds)) return false;
  const std::size_t index = find(id);
  if (index == entries_.size()) return false;
  entries_[index].bounds = bounds;
  return true;
}

bool EnemySystem::damage(std::uint32_t id, std::uint32_t amount,
                         bool& defeated) {
  if (!valid_ || amount == 0) return false;
  const std::size_t index = find(id);
  if (index == entries_.size()) return false;
  Entry& entry = entries_[index];
  const bool will_defeat = amount >= entry.health;
  if (will_defeat) {
    entry.health = 0;
    entry.active = false;
  } else {
    entry.health -= amount;
  }
  defeated = will_defeat;
  return true;
}

bool EnemySystem::get(std::uint32_t id, EnemyState& output) const {
  if (!valid_) return false;
  const std::size_t index = find(id);
  if (index == entries_.size()) return false;
  output = EnemyState{entries_[index].id, entries_[index].bounds,
                      entries_[index].health};
  return true;
}

void EnemySystem::clear() {
  if (!valid_) return;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    entries_[i].active = false;
    entries_[i].health = 0;
  }
}

std::size_t EnemySystem::capacity() const { return capacity_; }

std::size_t EnemySystem::active_count() const {
  std::size_t count = 0;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].active) ++count;
  }
  return count;
}

bool EnemySystem::valid_bounds(const Aabb& bounds) {
  return query_aabb(bounds, bounds).overlapping;
}

std::size_t EnemySystem::find(std::uint32_t id) const {
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].active && entries_[i].id == id) return i;
  }
  return entries_.size();
}

}  // namespace pc2ps3
