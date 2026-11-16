#pragma once

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

class FrameTimeTracker {
 public:
  enum { kWindowSize = 120 };
  static const std::uint64_t kMaxSampleNs = UINT64_C(10000000000);

  FrameTimeTracker();
  bool add_sample(std::uint64_t duration_ns);
  void reset();

  std::size_t sample_count() const;
  std::uint64_t last_sample_ns() const;
  std::uint64_t average_ns() const;
  std::uint64_t minimum_ns() const;
  std::uint64_t maximum_ns() const;

 private:
  std::uint64_t samples_[kWindowSize];
  std::size_t next_index_;
  std::size_t sample_count_;
  std::uint64_t total_ns_;
  std::uint64_t last_sample_ns_;
};

}  // namespace pc2ps3
