#pragma once

#include <cstdint>

namespace pc2ps3 {
namespace win32_detail {

bool configure_frame_pacing(std::uint32_t max_fps, std::uint64_t frequency,
                            std::uint64_t current_counter,
                            std::uint64_t& period_ticks,
                            std::uint64_t& deadline_ticks);

bool frame_wait_timeout_ms(std::uint64_t current_counter,
                           std::uint64_t frequency,
                           std::uint64_t period_ticks,
                           std::uint64_t& deadline_ticks,
                           std::uint32_t& timeout_ms);

}  // namespace win32_detail
}  // namespace pc2ps3
