#include "pc2ps3/performance_budget.hpp"

namespace pc2ps3 {

PerformanceBudgetTracker::PerformanceBudgetTracker(
    std::uint64_t ram_budget_bytes, std::uint64_t vram_budget_bytes)
    : frame_time_(),
      ram_budget_bytes_(ram_budget_bytes),
      vram_budget_bytes_(vram_budget_bytes),
      ram_current_known_(false),
      ram_current_bytes_(0),
      ram_peak_known_(false),
      ram_peak_bytes_(0),
      ram_budget_violations_(0),
      vram_current_known_(false),
      vram_current_bytes_(0),
      vram_peak_known_(false),
      vram_peak_bytes_(0),
      vram_budget_violations_(0) {}

bool PerformanceBudgetTracker::valid() const {
    return ram_budget_bytes_ > 0 && vram_budget_bytes_ > 0;
}

bool PerformanceBudgetTracker::add_sample(std::uint64_t elapsed_ns,
                                          bool ram_available,
                                          std::uint64_t ram_bytes,
                                          bool vram_available,
                                          std::uint64_t vram_bytes) {
    if (!valid() || elapsed_ns == 0 ||
        elapsed_ns > FrameTimeTracker::kMaxSampleNs) {
        return false;
    }
    if (!frame_time_.add_sample(elapsed_ns)) {
        return false;
    }

    ram_current_known_ = ram_available;
    if (ram_available) {
        ram_current_bytes_ = ram_bytes;
        if (!ram_peak_known_ || ram_bytes > ram_peak_bytes_) {
            ram_peak_bytes_ = ram_bytes;
            ram_peak_known_ = true;
        }
        if (ram_bytes > ram_budget_bytes_) {
            ++ram_budget_violations_;
        }
    }

    vram_current_known_ = vram_available;
    if (vram_available) {
        vram_current_bytes_ = vram_bytes;
        if (!vram_peak_known_ || vram_bytes > vram_peak_bytes_) {
            vram_peak_bytes_ = vram_bytes;
            vram_peak_known_ = true;
        }
        if (vram_bytes > vram_budget_bytes_) {
            ++vram_budget_violations_;
        }
    }
    return true;
}

void PerformanceBudgetTracker::reset() {
    frame_time_.reset();
    ram_current_known_ = false;
    ram_current_bytes_ = 0;
    ram_peak_known_ = false;
    ram_peak_bytes_ = 0;
    ram_budget_violations_ = 0;
    vram_current_known_ = false;
    vram_current_bytes_ = 0;
    vram_peak_known_ = false;
    vram_peak_bytes_ = 0;
    vram_budget_violations_ = 0;
}

std::size_t PerformanceBudgetTracker::sample_count() const {
    return frame_time_.sample_count();
}

std::uint64_t PerformanceBudgetTracker::average_frame_time_ns() const {
    return frame_time_.average_ns();
}

std::uint64_t PerformanceBudgetTracker::frames_per_second() const {
    const std::uint64_t average = average_frame_time_ns();
    return average == 0 ? 0 : UINT64_C(1000000000) / average;
}

bool PerformanceBudgetTracker::ram_current_known() const {
    return ram_current_known_;
}

std::uint64_t PerformanceBudgetTracker::ram_current_bytes() const {
    return ram_current_bytes_;
}

bool PerformanceBudgetTracker::ram_peak_known() const {
    return ram_peak_known_;
}

std::uint64_t PerformanceBudgetTracker::ram_peak_bytes() const {
    return ram_peak_bytes_;
}

std::size_t PerformanceBudgetTracker::ram_budget_violations() const {
    return ram_budget_violations_;
}

bool PerformanceBudgetTracker::vram_current_known() const {
    return vram_current_known_;
}

std::uint64_t PerformanceBudgetTracker::vram_current_bytes() const {
    return vram_current_bytes_;
}

bool PerformanceBudgetTracker::vram_peak_known() const {
    return vram_peak_known_;
}

std::uint64_t PerformanceBudgetTracker::vram_peak_bytes() const {
    return vram_peak_bytes_;
}

std::size_t PerformanceBudgetTracker::vram_budget_violations() const {
    return vram_budget_violations_;
}

std::uint64_t PerformanceBudgetTracker::ram_budget_bytes() const {
    return ram_budget_bytes_;
}

std::uint64_t PerformanceBudgetTracker::vram_budget_bytes() const {
    return vram_budget_bytes_;
}

} // namespace pc2ps3
