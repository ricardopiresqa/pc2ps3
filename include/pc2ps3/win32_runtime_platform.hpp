#pragma once

#include "pc2ps3/runtime_loop.hpp"
#include "pc2ps3/win32_gamepad.hpp"

#include <cstdint>

namespace pc2ps3 {

struct Win32RuntimePlatformState;

class Win32RuntimePlatform : public RuntimePlatform {
public:
    explicit Win32RuntimePlatform(bool show_window = true,
                                  std::uint32_t max_fps = 60);
    ~Win32RuntimePlatform() noexcept override;

    bool initialize(int width, int height) override;
    RuntimePollStatus poll(InputSnapshot& snapshot,
                          std::uint64_t& elapsed_ns) override;
    bool present(const Framebuffer& framebuffer) override;
    void shutdown() override;

private:
    Win32RuntimePlatformState* state_;
    std::uint64_t counter_frequency_;
    std::uint64_t previous_counter_;
    std::uint64_t frame_period_ticks_;
    std::uint64_t frame_deadline_ticks_;
    std::uint32_t max_fps_;
    bool show_window_;
    Win32GamepadAdapter gamepad_;
};

} // namespace pc2ps3
