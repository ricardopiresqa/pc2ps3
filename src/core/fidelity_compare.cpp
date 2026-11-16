#include "pc2ps3/fidelity_compare.hpp"

namespace pc2ps3 {
namespace {

std::int64_t absolute_delta(std::int32_t left, std::int32_t right) {
    const std::int64_t delta = static_cast<std::int64_t>(left) - right;
    return delta < 0 ? -delta : delta;
}

void record_error(std::int64_t x_error, std::int64_t y_error,
                  std::int32_t tolerance, std::size_t* mismatch_count,
                  std::int64_t* maximum_error) {
    const std::int64_t sample_error = x_error > y_error ? x_error : y_error;
    if (sample_error > *maximum_error) {
        *maximum_error = sample_error;
    }
    if (x_error > tolerance || y_error > tolerance) {
        ++(*mismatch_count);
    }
}

bool has_mismatches(const FidelityComparison& comparison) {
    return comparison.position_mismatches != 0 ||
           comparison.velocity_mismatches != 0 ||
           comparison.camera_mismatches != 0 ||
           comparison.input_mismatches != 0 ||
           comparison.collision_mismatches != 0 ||
           comparison.state_mismatches != 0 ||
           comparison.animation_mismatches != 0 ||
           comparison.progression_mismatches != 0 ||
           comparison.audio_mismatches != 0;
}

} // namespace

bool compare_fidelity_traces(const FidelityTrace& reference,
                             const FidelityTrace& candidate,
                             const FidelityTolerance& tolerance,
                             FidelityComparison* result) {
    if (result == 0 || !reference.valid() || !candidate.valid() ||
        reference.size() == 0 || reference.size() != candidate.size() ||
        tolerance.position_milli < 0 || tolerance.velocity_milli < 0 ||
        tolerance.camera_milli < 0) {
        return false;
    }

    FidelityComparison comparison = {};
    comparison.compared_samples = reference.size();
    for (std::size_t i = 0; i < reference.size(); ++i) {
        const FidelitySample* expected = reference.sample_at(i);
        const FidelitySample* actual = candidate.sample_at(i);
        if (expected == 0 || actual == 0 || expected->tick != actual->tick ||
            (i > 0 && expected->tick <= reference.sample_at(i - 1)->tick)) {
            return false;
        }

        record_error(absolute_delta(expected->position_x_milli,
                                    actual->position_x_milli),
                     absolute_delta(expected->position_y_milli,
                                    actual->position_y_milli),
                     tolerance.position_milli,
                     &comparison.position_mismatches,
                     &comparison.max_position_error_milli);
        record_error(absolute_delta(expected->velocity_x_milli,
                                    actual->velocity_x_milli),
                     absolute_delta(expected->velocity_y_milli,
                                    actual->velocity_y_milli),
                     tolerance.velocity_milli,
                     &comparison.velocity_mismatches,
                     &comparison.max_velocity_error_milli);
        record_error(absolute_delta(expected->camera_x_milli,
                                    actual->camera_x_milli),
                     absolute_delta(expected->camera_y_milli,
                                    actual->camera_y_milli),
                     tolerance.camera_milli,
                     &comparison.camera_mismatches,
                     &comparison.max_camera_error_milli);

        if (expected->buttons != actual->buttons ||
            expected->axis_x != actual->axis_x ||
            expected->axis_y != actual->axis_y) {
            ++comparison.input_mismatches;
        }
        if (expected->collision_mask != actual->collision_mask) {
            ++comparison.collision_mismatches;
        }
        if (expected->state_id != actual->state_id) {
            ++comparison.state_mismatches;
        }
        if (expected->animation_id != actual->animation_id) {
            ++comparison.animation_mismatches;
        }
        if (expected->checkpoint_id != actual->checkpoint_id ||
            expected->phase_id != actual->phase_id) {
            ++comparison.progression_mismatches;
        }
        if (expected->audio_event_mask != actual->audio_event_mask) {
            ++comparison.audio_mismatches;
        }
    }

    comparison.passed = !has_mismatches(comparison);
    *result = comparison;
    return true;
}

} // namespace pc2ps3
