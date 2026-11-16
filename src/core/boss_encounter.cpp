#include "pc2ps3/boss_encounter.hpp"

namespace pc2ps3 {

BossEncounter::BossEncounter(std::uint32_t maximum_health,
                             const std::uint32_t* phase_thresholds,
                             std::size_t threshold_count)
    : maximum_health_(maximum_health),
      health_(maximum_health),
      threshold_count_(0),
      phase_(0),
      valid_(false) {
  for (std::size_t i = 0; i < kMaxThresholds; ++i) thresholds_[i] = 0;
  if (maximum_health == 0 || threshold_count > kMaxThresholds ||
      (threshold_count != 0 && phase_thresholds == 0)) {
    return;
  }
  for (std::size_t i = 0; i < threshold_count; ++i) {
    const std::uint32_t threshold = phase_thresholds[i];
    if (threshold == 0 || threshold >= maximum_health ||
        (i != 0 && threshold >= phase_thresholds[i - 1])) {
      return;
    }
    thresholds_[i] = threshold;
  }
  threshold_count_ = threshold_count;
  valid_ = true;
}

bool BossEncounter::valid() const { return valid_; }

bool BossEncounter::damage(std::uint32_t amount, bool& phase_changed,
                           bool& defeated_now) {
  phase_changed = false;
  defeated_now = false;
  if (!valid_ || amount == 0 || defeated()) return false;
  health_ = amount >= health_ ? 0 : health_ - amount;
  if (health_ == 0) {
    defeated_now = true;
    return true;
  }
  while (phase_ < threshold_count_ && health_ <= thresholds_[phase_]) {
    ++phase_;
    phase_changed = true;
  }
  return true;
}

void BossEncounter::reset() {
  if (!valid_) return;
  health_ = maximum_health_;
  phase_ = 0;
}

std::uint32_t BossEncounter::health() const { return health_; }

std::uint32_t BossEncounter::maximum_health() const {
  return maximum_health_;
}

std::size_t BossEncounter::phase() const { return phase_; }

std::size_t BossEncounter::phase_count() const {
  return valid_ ? threshold_count_ + 1 : 0;
}

bool BossEncounter::defeated() const { return valid_ && health_ == 0; }

}  // namespace pc2ps3
