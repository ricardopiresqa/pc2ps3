#pragma once

#include "pc2ps3/fidelity_trace.hpp"

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

struct FidelityTolerance {
    std::int32_t position_milli;
    std::int32_t velocity_milli;
    std::int32_t camera_milli;
};

struct FidelityComparison {
    std::size_t compared_samples;
    std::size_t position_mismatches;
    std::size_t velocity_mismatches;
    std::size_t camera_mismatches;
    std::size_t input_mismatches;
    std::size_t collision_mismatches;
    std::size_t state_mismatches;
    std::size_t animation_mismatches;
    std::size_t progression_mismatches;
    std::size_t audio_mismatches;
    std::int64_t max_position_error_milli;
    std::int64_t max_velocity_error_milli;
    std::int64_t max_camera_error_milli;
    bool passed;
};

// Returns false for incompatible traces or invalid arguments; result is unchanged.
bool compare_fidelity_traces(const FidelityTrace& reference,
                             const FidelityTrace& candidate,
                             const FidelityTolerance& tolerance,
                             FidelityComparison* result);

} // namespace pc2ps3
