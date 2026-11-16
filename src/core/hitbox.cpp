#include "pc2ps3/hitbox.hpp"

#include <new>
#include <utility>

namespace pc2ps3 {

HitboxSystem::HitboxSystem(std::size_t hitbox_capacity,
                           std::size_t hurtbox_capacity)
    : hitbox_capacity_(0), hurtbox_capacity_(0), valid_(false) {
  if (hitbox_capacity == 0 || hitbox_capacity > kMaxBoxesPerKind ||
      hurtbox_capacity == 0 || hurtbox_capacity > kMaxBoxesPerKind) {
    return;
  }
  try {
    hitboxes_.reserve(hitbox_capacity);
    hurtboxes_.reserve(hurtbox_capacity);
    events_.reserve(hitbox_capacity * hurtbox_capacity);
  } catch (const std::bad_alloc&) {
    std::vector<Box>().swap(hitboxes_);
    std::vector<Box>().swap(hurtboxes_);
    std::vector<HitEvent>().swap(events_);
    return;
  }
  hitbox_capacity_ = hitbox_capacity;
  hurtbox_capacity_ = hurtbox_capacity;
  valid_ = true;
}

bool HitboxSystem::valid() const { return valid_; }

void HitboxSystem::begin_tick() {
  if (!valid_) return;
  hitboxes_.clear();
  hurtboxes_.clear();
  events_.clear();
}

bool HitboxSystem::add_hitbox(std::uint32_t attack_id, std::uint32_t owner_id,
                              const Aabb& bounds) {
  if (!valid_ || hitboxes_.size() >= hitbox_capacity_ ||
      !valid_bounds(bounds) || has_id(hitboxes_, attack_id)) {
    return false;
  }
  hitboxes_.push_back(Box(attack_id, owner_id, bounds));
  return true;
}

bool HitboxSystem::add_hurtbox(std::uint32_t hurtbox_id,
                               std::uint32_t owner_id, const Aabb& bounds) {
  if (!valid_ || hurtboxes_.size() >= hurtbox_capacity_ ||
      !valid_bounds(bounds) || has_id(hurtboxes_, hurtbox_id)) {
    return false;
  }
  hurtboxes_.push_back(Box(hurtbox_id, owner_id, bounds));
  return true;
}

bool HitboxSystem::update() {
  if (!valid_) return false;
  events_.clear();
  for (std::size_t i = 0; i < hitboxes_.size(); ++i) {
    for (std::size_t j = 0; j < hurtboxes_.size(); ++j) {
      const Box& attack = hitboxes_[i];
      const Box& hurt = hurtboxes_[j];
      if (attack.owner_id == hurt.owner_id ||
          !query_aabb(attack.bounds, hurt.bounds).overlapping) {
        continue;
      }
      events_.push_back(
          HitEvent{attack.id, attack.owner_id, hurt.id, hurt.owner_id});
    }
  }
  return true;
}

void HitboxSystem::reset() { begin_tick(); }

const std::vector<HitEvent>& HitboxSystem::events() const { return events_; }

std::size_t HitboxSystem::hitbox_count() const { return hitboxes_.size(); }

std::size_t HitboxSystem::hurtbox_count() const { return hurtboxes_.size(); }

bool HitboxSystem::valid_bounds(const Aabb& bounds) {
  return query_aabb(bounds, bounds).overlapping;
}

bool HitboxSystem::has_id(const std::vector<Box>& boxes, std::uint32_t id) {
  for (std::size_t i = 0; i < boxes.size(); ++i) {
    if (boxes[i].id == id) return true;
  }
  return false;
}

}  // namespace pc2ps3
