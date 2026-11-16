#include "pc2ps3/fidelity_trace.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace {

pc2ps3::FidelitySample make_sample(std::uint64_t tick, std::int32_t x) {
    pc2ps3::FidelitySample sample = {};
    sample.tick = tick;
    sample.buttons = UINT32_C(0x25);
    sample.axis_x = -12;
    sample.axis_y = 34;
    sample.position_x_milli = x;
    sample.position_y_milli = -200;
    sample.velocity_x_milli = 300;
    sample.velocity_y_milli = -400;
    sample.collision_mask = UINT32_C(0x3);
    sample.state_id = 5;
    sample.animation_id = 7;
    sample.camera_x_milli = 800;
    sample.camera_y_milli = -900;
    sample.checkpoint_id = 2;
    sample.phase_id = 4;
    sample.audio_event_mask = UINT32_C(0x10);
    return sample;
}

void assert_sample_equal(const pc2ps3::FidelitySample& actual,
                         const pc2ps3::FidelitySample& expected) {
    assert(actual.tick == expected.tick);
    assert(actual.buttons == expected.buttons);
    assert(actual.axis_x == expected.axis_x);
    assert(actual.axis_y == expected.axis_y);
    assert(actual.position_x_milli == expected.position_x_milli);
    assert(actual.position_y_milli == expected.position_y_milli);
    assert(actual.velocity_x_milli == expected.velocity_x_milli);
    assert(actual.velocity_y_milli == expected.velocity_y_milli);
    assert(actual.collision_mask == expected.collision_mask);
    assert(actual.state_id == expected.state_id);
    assert(actual.animation_id == expected.animation_id);
    assert(actual.camera_x_milli == expected.camera_x_milli);
    assert(actual.camera_y_milli == expected.camera_y_milli);
    assert(actual.checkpoint_id == expected.checkpoint_id);
    assert(actual.phase_id == expected.phase_id);
    assert(actual.audio_event_mask == expected.audio_event_mask);
}

void test_records_samples_in_tick_order() {
    pc2ps3::FidelitySample storage[2] = {};
    pc2ps3::FidelityTrace trace(storage, 2);
    const pc2ps3::FidelitySample first = make_sample(0, -250);
    const pc2ps3::FidelitySample second = make_sample(1, 1250);

    assert(trace.valid());
    assert(trace.capacity() == 2);
    assert(trace.size() == 0);
    assert(trace.sample_at(0) == 0);
    assert(trace.append(first));
    assert(trace.append(second));
    assert(trace.size() == 2);
    assert_sample_equal(*trace.sample_at(0), first);
    assert_sample_equal(*trace.sample_at(1), second);
}

void test_rejects_non_monotonic_ticks_without_mutation() {
    pc2ps3::FidelitySample storage[3] = {};
    pc2ps3::FidelityTrace trace(storage, 3);
    assert(trace.append(make_sample(4, 4000)));
    assert(trace.append(make_sample(5, 5000)));

    assert(!trace.append(make_sample(5, 9000)));
    assert(!trace.append(make_sample(3, 9000)));
    assert(trace.size() == 2);
    assert(trace.sample_at(1)->tick == 5);
    assert(trace.sample_at(1)->position_x_milli == 5000);
}

void test_rejects_full_storage_without_mutation() {
    pc2ps3::FidelitySample storage[1] = {};
    pc2ps3::FidelityTrace trace(storage, 1);
    assert(trace.append(make_sample(0, 10)));
    assert(!trace.append(make_sample(1, 20)));
    assert(trace.size() == 1);
    assert(trace.sample_at(0)->position_x_milli == 10);
}

void test_rejects_invalid_storage() {
    pc2ps3::FidelitySample storage[1] = {};
    pc2ps3::FidelityTrace null_trace(0, 1);
    pc2ps3::FidelityTrace empty_trace(storage, 0);
    pc2ps3::FidelityTrace oversized_trace(
        storage, static_cast<std::size_t>(pc2ps3::FidelityTrace::kMaxCapacity) + 1);

    assert(!null_trace.valid());
    assert(!empty_trace.valid());
    assert(!oversized_trace.valid());
    assert(!null_trace.append(make_sample(0, 0)));
    assert(null_trace.size() == 0);
}

void test_clear_allows_a_new_trace() {
    pc2ps3::FidelitySample storage[2] = {};
    pc2ps3::FidelityTrace trace(storage, 2);
    assert(trace.append(make_sample(8, 800)));
    trace.clear();
    assert(trace.size() == 0);
    assert(trace.sample_at(0) == 0);
    assert(trace.append(make_sample(0, -100)));
    assert(trace.sample_at(0)->position_x_milli == -100);
}

} // namespace

int main() {
    test_records_samples_in_tick_order();
    test_rejects_non_monotonic_ticks_without_mutation();
    test_rejects_full_storage_without_mutation();
    test_rejects_invalid_storage();
    test_clear_allows_a_new_trace();
    return 0;
}
