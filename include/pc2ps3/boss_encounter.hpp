#pragma once

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

// Health-threshold phase controller; attacks and visuals remain caller-owned.
class BossEncounter {
 public:
  enum { kMaxThresholds = 7 };

  BossEncounter(std::uint32_t maximum_health,
                const std::uint32_t* phase_thresholds,
                std::size_t threshold_count);

  bool valid() const;
  bool damage(std::uint32_t amount, bool& phase_changed, bool& defeated);
  void reset();

  std::uint32_t health() const;
  std::uint32_t maximum_health() const;
  std::size_t phase() const;
  std::size_t phase_count() const;
  bool defeated() const;

 private:
  std::uint32_t maximum_health_;
  std::uint32_t health_;
  std::uint32_t thresholds_[kMaxThresholds];
  std::size_t threshold_count_;
  std::size_t phase_;
  bool valid_;
};

}  // namespace pc2ps3
