#include "pc2ps3/fixed_step_scheduler.hpp"

#include <cmath>
#include <limits>

namespace pc2ps3 {

FixedStepConfig::FixedStepConfig()
    : fixed_step_ns(0), max_frame_ns(0), max_steps_per_frame(0) {}

FixedStepConfig::FixedStepConfig(std::uint64_t step_ns,
                                 std::uint64_t frame_limit_ns,
                                 std::uint32_t step_limit)
    : fixed_step_ns(step_ns),
      max_frame_ns(frame_limit_ns),
      max_steps_per_frame(step_limit) {}

FixedStepAdvance::FixedStepAdvance()
    : valid(false), steps(0), dropped_ns(0), alpha(0.0) {}

FixedStepScheduler::FixedStepScheduler(const FixedStepConfig& config)
    : config_(config),
      valid_(config.fixed_step_ns > 0 && config.max_steps_per_frame > 0 &&
             config.max_frame_ns >= config.fixed_step_ns),
      accumulator_ns_(0) {}

bool FixedStepScheduler::valid() const { return valid_; }

void FixedStepScheduler::reset() { accumulator_ns_ = 0; }

FixedStepAdvance FixedStepScheduler::advance(std::uint64_t elapsed_ns) {
  FixedStepAdvance result;
  if (!valid_) {
    return result;
  }

  const std::uint64_t dropped_frame_ns =
      elapsed_ns > config_.max_frame_ns
          ? elapsed_ns - config_.max_frame_ns
          : 0;
  const std::uint64_t accepted_ns =
      elapsed_ns > config_.max_frame_ns ? config_.max_frame_ns : elapsed_ns;
  const std::uint64_t whole_steps = accepted_ns / config_.fixed_step_ns;
  const std::uint64_t frame_remainder = accepted_ns % config_.fixed_step_ns;

  std::uint64_t available_steps = whole_steps;
  std::uint64_t remainder_ns = 0;
  if (accumulator_ns_ >= config_.fixed_step_ns - frame_remainder) {
    ++available_steps;
    remainder_ns = accumulator_ns_ -
                    (config_.fixed_step_ns - frame_remainder);
  } else {
    remainder_ns = accumulator_ns_ + frame_remainder;
  }

  const std::uint32_t steps =
      available_steps > config_.max_steps_per_frame
          ? config_.max_steps_per_frame
          : static_cast<std::uint32_t>(available_steps);
  const std::uint64_t dropped_steps = available_steps - steps;
  const std::uint64_t max_value =
      (std::numeric_limits<std::uint64_t>::max)();
  if (dropped_steps > (max_value - dropped_frame_ns) /
                          config_.fixed_step_ns) {
    return result;
  }

  result.valid = true;
  result.steps = steps;
  result.dropped_ns =
      dropped_frame_ns + dropped_steps * config_.fixed_step_ns;
  result.alpha = static_cast<double>(remainder_ns) /
                 static_cast<double>(config_.fixed_step_ns);
  if (result.alpha >= 1.0) {
    result.alpha = std::nextafter(1.0, 0.0);
  }
  accumulator_ns_ = remainder_ns;
  return result;
}

std::uint64_t FixedStepScheduler::accumulator_ns() const {
  return accumulator_ns_;
}

} // namespace pc2ps3
