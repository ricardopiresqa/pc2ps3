#include "pc2ps3/fidelity_compare.hpp"

#include <cassert>
#include <cstdint>

namespace {

pc2ps3::FidelitySample make_sample(std::uint64_t tick) {
    pc2ps3::FidelitySample sample = {};
    sample.tick = tick;
    sample.buttons = 1;
    sample.axis_x = 10;
    sample.axis_y = -10;
    sample.position_x_milli = 1000;
    sample.position_y_milli = 2000;
    sample.velocity_x_milli = 3000;
    sample.velocity_y_milli = 4000;
    sample.collision_mask = 2;
    sample.state_id = 3;
    sample.animation_id = 4;
    sample.camera_x_milli = 5000;
    sample.camera_y_milli = 6000;
    sample.checkpoint_id = 7;
    sample.phase_id = 8;
    sample.audio_event_mask = 16;
    return sample;
}

void test_equal_traces_pass() {
    pc2ps3::FidelitySample left_storage[2] = {};
    pc2ps3::FidelitySample right_storage[2] = {};
    pc2ps3::FidelityTrace left(left_storage, 2);
    pc2ps3::FidelityTrace right(right_storage, 2);
    assert(left.append(make_sample(0)));
    assert(left.append(make_sample(1)));
    assert(right.append(make_sample(0)));
    assert(right.append(make_sample(1)));

    const pc2ps3::FidelityTolerance tolerance = {0, 0, 0};
    pc2ps3::FidelityComparison result = {};
    assert(pc2ps3::compare_fidelity_traces(left, right, tolerance, &result));
    assert(result.passed);
    assert(result.compared_samples == 2);
}

void test_numeric_tolerances_are_inclusive() {
    pc2ps3::FidelitySample left_storage[1] = {};
    pc2ps3::FidelitySample right_storage[1] = {};
    pc2ps3::FidelityTrace left(left_storage, 1);
    pc2ps3::FidelityTrace right(right_storage, 1);
    pc2ps3::FidelitySample expected = make_sample(4);
    pc2ps3::FidelitySample actual = expected;
    actual.position_x_milli += 2;
    actual.velocity_y_milli -= 3;
    actual.camera_y_milli += 4;
    assert(left.append(expected));
    assert(right.append(actual));

    const pc2ps3::FidelityTolerance tolerance = {2, 3, 4};
    pc2ps3::FidelityComparison result = {};
    assert(pc2ps3::compare_fidelity_traces(left, right, tolerance, &result));
    assert(result.passed);
    assert(result.max_position_error_milli == 2);
    assert(result.max_velocity_error_milli == 3);
    assert(result.max_camera_error_milli == 4);
}

void test_numeric_threshold_exceeded_fails_comparison() {
    pc2ps3::FidelitySample left_storage[1] = {};
    pc2ps3::FidelitySample right_storage[1] = {};
    pc2ps3::FidelityTrace left(left_storage, 1);
    pc2ps3::FidelityTrace right(right_storage, 1);
    pc2ps3::FidelitySample expected = make_sample(0);
    pc2ps3::FidelitySample actual = expected;
    actual.position_x_milli += 3;
    assert(left.append(expected));
    assert(right.append(actual));

    const pc2ps3::FidelityTolerance tolerance = {2, 0, 0};
    pc2ps3::FidelityComparison result = {};
    assert(pc2ps3::compare_fidelity_traces(left, right, tolerance, &result));
    assert(!result.passed);
    assert(result.position_mismatches == 1);
    assert(result.max_position_error_milli == 3);
}

void test_categorical_fields_are_exact() {
    pc2ps3::FidelitySample left_storage[1] = {};
    pc2ps3::FidelitySample right_storage[1] = {};
    pc2ps3::FidelityTrace left(left_storage, 1);
    pc2ps3::FidelityTrace right(right_storage, 1);
    pc2ps3::FidelitySample expected = make_sample(0);
    pc2ps3::FidelitySample actual = expected;
    actual.buttons = 2;
    actual.collision_mask = 4;
    actual.state_id = 5;
    actual.animation_id = 6;
    actual.checkpoint_id = 9;
    actual.phase_id = 10;
    actual.audio_event_mask = 32;
    assert(left.append(expected));
    assert(right.append(actual));

    const pc2ps3::FidelityTolerance tolerance = {100, 100, 100};
    pc2ps3::FidelityComparison result = {};
    assert(pc2ps3::compare_fidelity_traces(left, right, tolerance, &result));
    assert(!result.passed);
    assert(result.input_mismatches == 1);
    assert(result.collision_mismatches == 1);
    assert(result.state_mismatches == 1);
    assert(result.animation_mismatches == 1);
    assert(result.progression_mismatches == 1);
    assert(result.audio_mismatches == 1);
}

void test_invalid_comparisons_leave_result_unchanged() {
    pc2ps3::FidelitySample left_storage[1] = {};
    pc2ps3::FidelitySample right_storage[1] = {};
    pc2ps3::FidelityTrace left(left_storage, 1);
    pc2ps3::FidelityTrace right(right_storage, 1);
    assert(left.append(make_sample(1)));
    assert(right.append(make_sample(2)));
    const pc2ps3::FidelityTolerance tolerance = {0, 0, 0};
    pc2ps3::FidelityComparison result = {};
    result.compared_samples = 77;

    assert(!pc2ps3::compare_fidelity_traces(left, right, tolerance, &result));
    assert(result.compared_samples == 77);
    assert(!pc2ps3::compare_fidelity_traces(left, right,
        pc2ps3::FidelityTolerance{-1, 0, 0}, &result));
    assert(result.compared_samples == 77);
    assert(!pc2ps3::compare_fidelity_traces(left, right, tolerance, 0));
}

void test_mutated_non_monotonic_storage_is_rejected() {
    pc2ps3::FidelitySample left_storage[2] = {};
    pc2ps3::FidelitySample right_storage[2] = {};
    pc2ps3::FidelityTrace left(left_storage, 2);
    pc2ps3::FidelityTrace right(right_storage, 2);
    assert(left.append(make_sample(0)));
    assert(left.append(make_sample(1)));
    assert(right.append(make_sample(0)));
    assert(right.append(make_sample(1)));

    left_storage[1].tick = 0;
    right_storage[1].tick = 0;
    const pc2ps3::FidelityTolerance tolerance = {0, 0, 0};
    pc2ps3::FidelityComparison result = {};
    result.compared_samples = 91;
    assert(!pc2ps3::compare_fidelity_traces(left, right, tolerance, &result));
    assert(result.compared_samples == 91);
}

} // namespace

int main() {
    test_equal_traces_pass();
    test_numeric_tolerances_are_inclusive();
    test_numeric_threshold_exceeded_fails_comparison();
    test_categorical_fields_are_exact();
    test_invalid_comparisons_leave_result_unchanged();
    test_mutated_non_monotonic_storage_is_rejected();
    return 0;
}
