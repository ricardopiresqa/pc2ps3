#pragma once

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

struct FidelitySample {
    std::uint64_t tick;
    std::uint32_t buttons;
    std::int16_t axis_x;
    std::int16_t axis_y;
    std::int32_t position_x_milli;
    std::int32_t position_y_milli;
    std::int32_t velocity_x_milli;
    std::int32_t velocity_y_milli;
    std::uint32_t collision_mask;
    std::uint32_t state_id;
    std::uint32_t animation_id;
    std::int32_t camera_x_milli;
    std::int32_t camera_y_milli;
    std::uint32_t checkpoint_id;
    std::uint32_t phase_id;
    std::uint32_t audio_event_mask;
};

class FidelityTrace {
public:
    enum { kMaxCapacity = 65536 };

    FidelityTrace(FidelitySample* storage, std::size_t capacity);
    FidelityTrace(const FidelityTrace&) = delete;
    FidelityTrace& operator=(const FidelityTrace&) = delete;

    bool valid() const;
    bool append(const FidelitySample& sample);
    void clear();
    std::size_t size() const;
    std::size_t capacity() const;
    const FidelitySample* sample_at(std::size_t index) const;

private:
    FidelitySample* storage_;
    std::size_t capacity_;
    std::size_t size_;
    std::uint64_t last_tick_;
};

} // namespace pc2ps3
