#include "pc2ps3/fixed_step_scheduler.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

void test_threshold_and_fraction() {
  pc2ps3::FixedStepScheduler scheduler(
      pc2ps3::FixedStepConfig(10, 30, 2));
  pc2ps3::FixedStepAdvance result = scheduler.advance(9);
  assert(result.valid && result.steps == 0 && result.dropped_ns == 0);
  assert(std::fabs(result.alpha - 0.9) < 0.000001);

  result = scheduler.advance(1);
  assert(result.valid && result.steps == 1 && result.alpha == 0.0);
  assert(scheduler.accumulator_ns() == 0);
}

void test_catch_up_and_dropped_time() {
  pc2ps3::FixedStepScheduler scheduler(
      pc2ps3::FixedStepConfig(10, 30, 2));
  pc2ps3::FixedStepAdvance result = scheduler.advance(25);
  assert(result.valid && result.steps == 2);
  assert(result.dropped_ns == 0);
  assert(std::fabs(result.alpha - 0.5) < 0.000001);

  scheduler.reset();
  result = scheduler.advance(35);
  assert(result.valid && result.steps == 2);
  assert(result.dropped_ns == 15);
  assert(result.alpha == 0.0);

  scheduler.reset();
  result = scheduler.advance(45);
  assert(result.valid && result.steps == 2);
  assert(result.dropped_ns == 25);
  assert(result.alpha == 0.0);
}

void test_reset_and_invalid_configuration() {
  pc2ps3::FixedStepScheduler scheduler(
      pc2ps3::FixedStepConfig(10, 30, 2));
  scheduler.advance(7);
  pc2ps3::FixedStepAdvance first = scheduler.advance(13);
  scheduler.reset();
  scheduler.advance(7);
  pc2ps3::FixedStepAdvance after_reset = scheduler.advance(13);
  assert(first.steps == after_reset.steps);
  assert(first.dropped_ns == after_reset.dropped_ns);
  assert(first.alpha == after_reset.alpha);

  const pc2ps3::FixedStepConfig invalid[] = {
      pc2ps3::FixedStepConfig(0, 30, 2),
      pc2ps3::FixedStepConfig(10, 9, 2),
      pc2ps3::FixedStepConfig(10, 30, 0)};
  for (std::size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    pc2ps3::FixedStepScheduler bad(invalid[i]);
    const pc2ps3::FixedStepAdvance result = bad.advance(100);
    assert(!bad.valid() && !result.valid);
    assert(result.steps == 0 && result.dropped_ns == 0 && result.alpha == 0.0);
    assert(bad.accumulator_ns() == 0);
  }
}

void test_uint64_elapsed_does_not_overflow() {
  const std::uint64_t maximum =
      (std::numeric_limits<std::uint64_t>::max)();
  pc2ps3::FixedStepScheduler scheduler(
      pc2ps3::FixedStepConfig(7, maximum, 1));
  pc2ps3::FixedStepAdvance result = scheduler.advance(6);
  assert(result.valid && result.steps == 0 && result.alpha < 1.0);

  result = scheduler.advance(maximum);
  assert(result.valid && result.steps == 1);
  assert(result.dropped_ns <= maximum);
  assert(result.alpha >= 0.0 && result.alpha < 1.0);
  assert(scheduler.accumulator_ns() < 7);
}

} // namespace

int main() {
  test_threshold_and_fraction();
  test_catch_up_and_dropped_time();
  test_reset_and_invalid_configuration();
  test_uint64_elapsed_does_not_overflow();
  return 0;
}
