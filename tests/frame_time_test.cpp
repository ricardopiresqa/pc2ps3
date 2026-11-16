#include "pc2ps3/frame_time.hpp"

#include <cassert>
#include <cstdint>

namespace {

void test_empty_and_partial_window() {
  pc2ps3::FrameTimeTracker tracker;
  assert(tracker.sample_count() == 0);
  assert(tracker.last_sample_ns() == 0);
  assert(tracker.average_ns() == 0);
  assert(tracker.minimum_ns() == 0);
  assert(tracker.maximum_ns() == 0);

  assert(tracker.add_sample(100));
  assert(tracker.add_sample(300));
  assert(tracker.add_sample(200));
  assert(tracker.sample_count() == 3);
  assert(tracker.last_sample_ns() == 200);
  assert(tracker.average_ns() == 200);
  assert(tracker.minimum_ns() == 100);
  assert(tracker.maximum_ns() == 300);
}

void test_window_rollover() {
  pc2ps3::FrameTimeTracker tracker;
  for (std::uint64_t sample = 1;
       sample <= pc2ps3::FrameTimeTracker::kWindowSize; ++sample) {
    assert(tracker.add_sample(sample));
  }
  assert(tracker.sample_count() == pc2ps3::FrameTimeTracker::kWindowSize);
  assert(tracker.average_ns() == 60);
  assert(tracker.minimum_ns() == 1);
  assert(tracker.maximum_ns() == 120);

  assert(tracker.add_sample(121));
  assert(tracker.sample_count() == pc2ps3::FrameTimeTracker::kWindowSize);
  assert(tracker.last_sample_ns() == 121);
  assert(tracker.average_ns() == 61);
  assert(tracker.minimum_ns() == 2);
  assert(tracker.maximum_ns() == 121);
}

void test_limits_rejection_and_reset() {
  pc2ps3::FrameTimeTracker tracker;
  assert(tracker.add_sample(1));
  assert(tracker.add_sample(pc2ps3::FrameTimeTracker::kMaxSampleNs));
  assert(tracker.average_ns() == UINT64_C(5000000000));
  assert(tracker.minimum_ns() == 1);
  assert(tracker.maximum_ns() == pc2ps3::FrameTimeTracker::kMaxSampleNs);
  assert(tracker.last_sample_ns() ==
         pc2ps3::FrameTimeTracker::kMaxSampleNs);

  assert(!tracker.add_sample(0));
  assert(!tracker.add_sample(pc2ps3::FrameTimeTracker::kMaxSampleNs + 1));
  assert(tracker.sample_count() == 2);
  assert(tracker.average_ns() == UINT64_C(5000000000));
  assert(tracker.minimum_ns() == 1);
  assert(tracker.maximum_ns() == pc2ps3::FrameTimeTracker::kMaxSampleNs);
  assert(tracker.last_sample_ns() ==
         pc2ps3::FrameTimeTracker::kMaxSampleNs);

  tracker.reset();
  assert(tracker.sample_count() == 0);
  assert(tracker.last_sample_ns() == 0);
  assert(tracker.average_ns() == 0);
  assert(tracker.minimum_ns() == 0);
  assert(tracker.maximum_ns() == 0);
}

}  // namespace

int main() {
  test_empty_and_partial_window();
  test_window_rollover();
  test_limits_rejection_and_reset();
  return 0;
}
