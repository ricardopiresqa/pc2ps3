#include "pc2ps3/particles.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace pc2ps3 {
namespace {

const std::int64_t kQ16One = 65536;

std::int64_t floor_q16(std::int64_t value) {
  std::int64_t pixel = value / kQ16One;
  if (value % kQ16One < 0) --pixel;
  return pixel;
}

}  // namespace

ParticleSystem::Slot::Slot()
    : x_q16(0), y_q16(0), velocity_x(0), velocity_y(0), remaining_ms(0),
      size(0), color{0, 0, 0, 0}, active(false) {}

ParticleSystem::ParticleSystem(std::size_t capacity)
    : valid_(capacity > 0 && capacity <= kMaxCapacity),
      slots_(valid_ ? capacity : 0) {}

bool ParticleSystem::valid() const { return valid_; }

bool ParticleSystem::spawn(const ParticleSpawn& particle) {
  if (!valid_ || particle.lifetime_ms == 0 ||
      particle.lifetime_ms > kMaxLifetimeMs || particle.size < 1 ||
      particle.size > kMaxSize ||
      particle.velocity_x < -kMaxVelocityPixelsPerSecond ||
      particle.velocity_x > kMaxVelocityPixelsPerSecond ||
      particle.velocity_y < -kMaxVelocityPixelsPerSecond ||
      particle.velocity_y > kMaxVelocityPixelsPerSecond) {
    return false;
  }
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    Slot& slot = slots_[i];
    if (slot.active) continue;
    slot.x_q16 = static_cast<std::int64_t>(particle.x) * kQ16One;
    slot.y_q16 = static_cast<std::int64_t>(particle.y) * kQ16One;
    slot.velocity_x = particle.velocity_x;
    slot.velocity_y = particle.velocity_y;
    slot.remaining_ms = particle.lifetime_ms;
    slot.size = particle.size;
    slot.color = particle.color;
    slot.active = true;
    return true;
  }
  return false;
}

bool ParticleSystem::update(std::uint32_t dt_ms) {
  if (!valid_ || dt_ms > 1000u) return false;
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    Slot& slot = slots_[i];
    if (!slot.active) continue;
    if (dt_ms >= slot.remaining_ms) {
      slot.remaining_ms = 0;
      slot.active = false;
      continue;
    }
    slot.x_q16 +=
        (static_cast<std::int64_t>(slot.velocity_x) * dt_ms * kQ16One) /
        1000;
    slot.y_q16 +=
        (static_cast<std::int64_t>(slot.velocity_y) * dt_ms * kQ16One) /
        1000;
    slot.remaining_ms -= dt_ms;
  }
  return true;
}

void ParticleSystem::draw(Framebuffer& framebuffer, int camera_x,
                          int camera_y) const {
  if (!valid_ || framebuffer.width() <= 0 || framebuffer.height() <= 0) return;
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    const Slot& slot = slots_[i];
    if (!slot.active) continue;
    const std::int64_t x = floor_q16(slot.x_q16) - camera_x;
    const std::int64_t y = floor_q16(slot.y_q16) - camera_y;
    if (x + slot.size <= 0 || y + slot.size <= 0 ||
        x >= framebuffer.width() || y >= framebuffer.height()) {
      continue;
    }
    if (x < std::numeric_limits<int>::min() ||
        x > std::numeric_limits<int>::max() ||
        y < std::numeric_limits<int>::min() ||
        y > std::numeric_limits<int>::max()) {
      continue;
    }
    framebuffer.blend_rect(static_cast<int>(x), static_cast<int>(y), slot.size,
                           slot.size, slot.color);
  }
}

void ParticleSystem::clear() {
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    slots_[i].active = false;
    slots_[i].remaining_ms = 0;
  }
}

std::size_t ParticleSystem::capacity() const { return slots_.size(); }

std::size_t ParticleSystem::active_count() const {
  std::size_t count = 0;
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    if (slots_[i].active) ++count;
  }
  return count;
}

}  // namespace pc2ps3
