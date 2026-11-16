#include "pc2ps3/projectiles.hpp"

#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace pc2ps3 {
namespace {
const std::int64_t kQ16One = 65536;
const std::int64_t kWorldLimitQ16 =
    static_cast<std::int64_t>(ProjectileSystem::kMaxWorldCoordinate) *
    kQ16One;

bool valid_position(int coordinate) {
  return coordinate >= -ProjectileSystem::kMaxWorldCoordinate &&
         coordinate <= ProjectileSystem::kMaxWorldCoordinate;
}
}  // namespace

ProjectileSystem::Slot::Slot()
    : id(0),
      x_q16(0),
      y_q16(0),
      velocity_x(0),
      velocity_y(0),
      remaining_ms(0),
      width(0),
      height(0),
      color{0, 0, 0, 0},
      active(false) {}

ProjectileSystem::ProjectileSystem(std::size_t capacity) : valid_(false) {
  if (capacity == 0 || capacity > kMaxCapacity) return;
  try {
    slots_.resize(capacity);
    hits_.reserve(capacity);
  } catch (const std::bad_alloc&) {
    std::vector<Slot>().swap(slots_);
    std::vector<ProjectileHit>().swap(hits_);
    return;
  }
  valid_ = true;
}

bool ProjectileSystem::valid() const { return valid_; }

bool ProjectileSystem::spawn(const ProjectileSpawn& projectile) {
  if (!valid_ || projectile.width < 1 || projectile.width > kMaxSize ||
      projectile.height < 1 || projectile.height > kMaxSize ||
      !valid_position(projectile.x) || !valid_position(projectile.y) ||
      projectile.velocity_x < -kMaxVelocityPixelsPerSecond ||
      projectile.velocity_x > kMaxVelocityPixelsPerSecond ||
      projectile.velocity_y < -kMaxVelocityPixelsPerSecond ||
      projectile.velocity_y > kMaxVelocityPixelsPerSecond ||
      projectile.lifetime_ms == 0 ||
      projectile.lifetime_ms > kMaxLifetimeMs) {
    return false;
  }

  std::size_t free_slot = slots_.size();
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    if (slots_[i].active && slots_[i].id == projectile.id) return false;
    if (!slots_[i].active && free_slot == slots_.size()) free_slot = i;
  }
  if (free_slot == slots_.size()) return false;

  Slot& slot = slots_[free_slot];
  slot.id = projectile.id;
  slot.x_q16 = static_cast<std::int64_t>(projectile.x) * kQ16One;
  slot.y_q16 = static_cast<std::int64_t>(projectile.y) * kQ16One;
  slot.velocity_x = projectile.velocity_x;
  slot.velocity_y = projectile.velocity_y;
  slot.remaining_ms = projectile.lifetime_ms;
  slot.width = projectile.width;
  slot.height = projectile.height;
  slot.color = projectile.color;
  slot.active = true;
  return true;
}

bool ProjectileSystem::update(std::uint32_t dt_ms) {
  if (!valid_ || dt_ms > kMaxUpdateMs) return false;
  if (dt_ms == 0) return true;

  for (std::size_t i = 0; i < slots_.size(); ++i) {
    Slot& slot = slots_[i];
    if (!slot.active) continue;
    if (slot.remaining_ms <= dt_ms) {
      slot.remaining_ms = 0;
      slot.active = false;
      continue;
    }

    slot.x_q16 += static_cast<std::int64_t>(slot.velocity_x) * dt_ms *
                  kQ16One / 1000;
    slot.y_q16 += static_cast<std::int64_t>(slot.velocity_y) * dt_ms *
                  kQ16One / 1000;
    slot.remaining_ms -= dt_ms;
    if (slot.x_q16 < -kWorldLimitQ16 || slot.x_q16 > kWorldLimitQ16 ||
        slot.y_q16 < -kWorldLimitQ16 || slot.y_q16 > kWorldLimitQ16) {
      slot.remaining_ms = 0;
      slot.active = false;
    }
  }
  return true;
}

bool ProjectileSystem::collide(const Aabb& target_bounds) {
  if (!valid_ || !valid_target(target_bounds)) return false;
  hits_.clear();
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    Slot& slot = slots_[i];
    if (!slot.active || !query_aabb(slot_bounds(slot), target_bounds).overlapping)
      continue;
    hits_.push_back(ProjectileHit{slot.id});
    slot.active = false;
    slot.remaining_ms = 0;
  }
  return true;
}

void ProjectileSystem::draw(Framebuffer& framebuffer, int camera_x,
                            int camera_y) const {
  if (!valid_ || framebuffer.width() <= 0 || framebuffer.height() <= 0) return;
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    const Slot& slot = slots_[i];
    if (!slot.active) continue;
    const std::int64_t x = floor_q16(slot.x_q16) - camera_x;
    const std::int64_t y = floor_q16(slot.y_q16) - camera_y;
    if (x + slot.width <= 0 || y + slot.height <= 0 ||
        x >= framebuffer.width() || y >= framebuffer.height() ||
        x < std::numeric_limits<int>::min() ||
        x > std::numeric_limits<int>::max() ||
        y < std::numeric_limits<int>::min() ||
        y > std::numeric_limits<int>::max()) {
      continue;
    }
    framebuffer.blend_rect(static_cast<int>(x), static_cast<int>(y),
                           slot.width, slot.height, slot.color);
  }
}

void ProjectileSystem::clear() {
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    slots_[i].active = false;
    slots_[i].remaining_ms = 0;
  }
  hits_.clear();
}

const std::vector<ProjectileHit>& ProjectileSystem::hits() const {
  return hits_;
}

std::size_t ProjectileSystem::capacity() const { return slots_.size(); }

std::size_t ProjectileSystem::active_count() const {
  std::size_t count = 0;
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    if (slots_[i].active) ++count;
  }
  return count;
}

bool ProjectileSystem::valid_target(const Aabb& bounds) {
  return query_aabb(bounds, bounds).overlapping;
}

std::int64_t ProjectileSystem::floor_q16(std::int64_t value) {
  std::int64_t pixel = value / kQ16One;
  if (value % kQ16One < 0) --pixel;
  return pixel;
}

Aabb ProjectileSystem::slot_bounds(const Slot& slot) {
  const float left = static_cast<float>(slot.x_q16) / kQ16One;
  const float top = static_cast<float>(slot.y_q16) / kQ16One;
  return Aabb{left, top, left + slot.width, top + slot.height};
}

}  // namespace pc2ps3
