#include "pc2ps3/frame_time.hpp"

namespace pc2ps3 {

FrameTimeTracker::FrameTimeTracker()
    : samples_(),
      next_index_(0),
      sample_count_(0),
      total_ns_(0),
      last_sample_ns_(0) {}

bool FrameTimeTracker::add_sample(std::uint64_t duration_ns) {
  if (duration_ns == 0 || duration_ns > kMaxSampleNs) return false;

  if (sample_count_ == kWindowSize) {
    total_ns_ -= samples_[next_index_];
  } else {
    ++sample_count_;
  }
  samples_[next_index_] = duration_ns;
  total_ns_ += duration_ns;
  last_sample_ns_ = duration_ns;
  next_index_ = (next_index_ + 1) % kWindowSize;
  return true;
}

void FrameTimeTracker::reset() {
  for (std::size_t i = 0; i < kWindowSize; ++i) samples_[i] = 0;
  next_index_ = 0;
  sample_count_ = 0;
  total_ns_ = 0;
  last_sample_ns_ = 0;
}

std::size_t FrameTimeTracker::sample_count() const { return sample_count_; }

std::uint64_t FrameTimeTracker::last_sample_ns() const {
  return last_sample_ns_;
}

std::uint64_t FrameTimeTracker::average_ns() const {
  return sample_count_ == 0 ? 0 : total_ns_ / sample_count_;
}

std::uint64_t FrameTimeTracker::minimum_ns() const {
  if (sample_count_ == 0) return 0;
  std::uint64_t minimum = samples_[0];
  for (std::size_t i = 1; i < sample_count_; ++i) {
    if (samples_[i] < minimum) minimum = samples_[i];
  }
  return minimum;
}

std::uint64_t FrameTimeTracker::maximum_ns() const {
  if (sample_count_ == 0) return 0;
  std::uint64_t maximum = samples_[0];
  for (std::size_t i = 1; i < sample_count_; ++i) {
    if (samples_[i] > maximum) maximum = samples_[i];
  }
  return maximum;
}

}  // namespace pc2ps3
