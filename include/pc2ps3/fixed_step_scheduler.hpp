#pragma once

#include <cstdint>

namespace pc2ps3 {

struct FixedStepConfig {
  std::uint64_t fixed_step_ns;
  std::uint64_t max_frame_ns;
  std::uint32_t max_steps_per_frame;

  FixedStepConfig();
  FixedStepConfig(std::uint64_t fixed_step_ns,
                  std::uint64_t max_frame_ns,
                  std::uint32_t max_steps_per_frame);
};

struct FixedStepAdvance {
  bool valid;
  std::uint32_t steps;
  std::uint64_t dropped_ns;
  double alpha;

  FixedStepAdvance();
};

class FixedStepScheduler {
public:
  explicit FixedStepScheduler(const FixedStepConfig& config);

  bool valid() const;
  void reset();
  FixedStepAdvance advance(std::uint64_t elapsed_ns);
  std::uint64_t accumulator_ns() const;

private:
  FixedStepConfig config_;
  bool valid_;
  std::uint64_t accumulator_ns_;
};

} // namespace pc2ps3
