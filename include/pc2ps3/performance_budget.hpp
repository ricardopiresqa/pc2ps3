#pragma once

#include "pc2ps3/frame_time.hpp"

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

class PerformanceBudgetTracker {
public:
    PerformanceBudgetTracker(std::uint64_t ram_budget_bytes,
                             std::uint64_t vram_budget_bytes);

    bool valid() const;
    bool add_sample(std::uint64_t elapsed_ns,
                    bool ram_available, std::uint64_t ram_bytes,
                    bool vram_available, std::uint64_t vram_bytes);
    void reset();

    std::size_t sample_count() const;
    std::uint64_t average_frame_time_ns() const;
    std::uint64_t frames_per_second() const;
    bool ram_current_known() const;
    std::uint64_t ram_current_bytes() const;
    bool ram_peak_known() const;
    std::uint64_t ram_peak_bytes() const;
    std::size_t ram_budget_violations() const;
    bool vram_current_known() const;
    std::uint64_t vram_current_bytes() const;
    bool vram_peak_known() const;
    std::uint64_t vram_peak_bytes() const;
    std::size_t vram_budget_violations() const;
    std::uint64_t ram_budget_bytes() const;
    std::uint64_t vram_budget_bytes() const;

private:
    FrameTimeTracker frame_time_;
    std::uint64_t ram_budget_bytes_;
    std::uint64_t vram_budget_bytes_;
    bool ram_current_known_;
    std::uint64_t ram_current_bytes_;
    bool ram_peak_known_;
    std::uint64_t ram_peak_bytes_;
    std::size_t ram_budget_violations_;
    bool vram_current_known_;
    std::uint64_t vram_current_bytes_;
    bool vram_peak_known_;
    std::uint64_t vram_peak_bytes_;
    std::size_t vram_budget_violations_;
};

} // namespace pc2ps3
