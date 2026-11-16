#include "pc2ps3/performance_budget.hpp"

#include <cassert>
#include <cstdint>

namespace {

void test_fps_and_current_peaks() {
    pc2ps3::PerformanceBudgetTracker tracker(1000, 500);
    assert(tracker.valid());
    assert(tracker.add_sample(20000000, true, 800, true, 300));
    assert(tracker.sample_count() == 1);
    assert(tracker.average_frame_time_ns() == 20000000);
    assert(tracker.frames_per_second() == 50);
    assert(tracker.ram_current_known());
    assert(tracker.ram_current_bytes() == 800);
    assert(tracker.ram_peak_known());
    assert(tracker.ram_peak_bytes() == 800);
    assert(tracker.vram_current_known());
    assert(tracker.vram_current_bytes() == 300);
    assert(tracker.vram_peak_bytes() == 300);
}

void test_budget_violations_and_peak_updates() {
    pc2ps3::PerformanceBudgetTracker tracker(1000, 500);
    assert(tracker.add_sample(10000000, true, 1200, true, 600));
    assert(tracker.ram_budget_violations() == 1);
    assert(tracker.vram_budget_violations() == 1);
    assert(tracker.add_sample(20000000, true, 900, true, 400));
    assert(tracker.ram_peak_bytes() == 1200);
    assert(tracker.vram_peak_bytes() == 600);
    assert(tracker.ram_budget_violations() == 1);
    assert(tracker.vram_budget_violations() == 1);
}

void test_unavailable_memory_is_not_zero() {
    pc2ps3::PerformanceBudgetTracker tracker(1000, 500);
    assert(tracker.add_sample(10000000, true, 700, true, 300));
    assert(tracker.add_sample(10000000, false, 0, false, 0));
    assert(!tracker.ram_current_known());
    assert(!tracker.vram_current_known());
    assert(tracker.ram_peak_known());
    assert(tracker.ram_peak_bytes() == 700);
    assert(tracker.vram_peak_known());
    assert(tracker.vram_peak_bytes() == 300);
}

void test_rejected_sample_does_not_update_partial_state() {
    pc2ps3::PerformanceBudgetTracker tracker(1000, 500);
    assert(tracker.add_sample(20000000, true, 700, true, 300));
    assert(!tracker.add_sample(0, true, 1500, true, 700));
    assert(!tracker.add_sample(pc2ps3::FrameTimeTracker::kMaxSampleNs + 1,
                               true, 1500, true, 700));
    assert(tracker.sample_count() == 1);
    assert(tracker.ram_current_bytes() == 700);
    assert(tracker.vram_current_bytes() == 300);
    assert(tracker.ram_budget_violations() == 0);
    assert(tracker.vram_budget_violations() == 0);
}

void test_invalid_budgets_and_reset() {
    pc2ps3::PerformanceBudgetTracker invalid(0, 500);
    assert(!invalid.valid());
    assert(!invalid.add_sample(10000000, true, 1, true, 1));

    pc2ps3::PerformanceBudgetTracker tracker(1000, 500);
    assert(tracker.add_sample(10000000, true, 1200, true, 600));
    tracker.reset();
    assert(tracker.valid());
    assert(tracker.sample_count() == 0);
    assert(tracker.frames_per_second() == 0);
    assert(!tracker.ram_current_known());
    assert(!tracker.ram_peak_known());
    assert(tracker.ram_budget_violations() == 0);
    assert(!tracker.vram_current_known());
    assert(!tracker.vram_peak_known());
    assert(tracker.vram_budget_violations() == 0);
}

} // namespace

int main() {
    test_fps_and_current_peaks();
    test_budget_violations_and_peak_updates();
    test_unavailable_memory_is_not_zero();
    test_rejected_sample_does_not_update_partial_state();
    test_invalid_budgets_and_reset();
    return 0;
}
