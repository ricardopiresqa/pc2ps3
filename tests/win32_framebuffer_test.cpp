#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "pc2ps3/render.hpp"
#include "pc2ps3/win32_runtime_platform.hpp"
#include "win32_framebuffer_internal.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

bool test_pixel_conversion() {
    pc2ps3::Framebuffer framebuffer(2, 2);
    framebuffer.clear(pc2ps3::Color{0, 0, 0, 255});
    framebuffer.fill_rect(0, 0, 1, 1, pc2ps3::Color{255, 0, 0, 255});
    framebuffer.fill_rect(1, 0, 1, 1, pc2ps3::Color{0, 255, 0, 255});
    framebuffer.fill_rect(0, 1, 1, 1, pc2ps3::Color{0, 0, 255, 255});
    framebuffer.fill_rect(1, 1, 1, 1, pc2ps3::Color{10, 20, 30, 40});

    std::vector<std::uint8_t> bgra;
    if (!expect(pc2ps3::win32_detail::framebuffer_to_bgra(framebuffer, bgra),
                "valid framebuffer conversion failed") ||
        !expect(bgra.size() == 16u, "unexpected converted byte count")) {
        return false;
    }
    const std::uint8_t expected[] = {
        0, 0, 255, 255, 0, 255, 0, 255,
        255, 0, 0, 255, 30, 20, 10, 255};
    for (std::size_t index = 0; index < sizeof(expected); ++index) {
        if (!expect(bgra[index] == expected[index],
                    "channel order or top-down row order changed")) {
            return false;
        }
    }
    const std::size_t initial_capacity = bgra.capacity();
    if (!expect(pc2ps3::win32_detail::framebuffer_to_bgra(framebuffer, bgra),
                "repeated framebuffer conversion failed") ||
        !expect(bgra.capacity() == initial_capacity,
                "repeated conversion unnecessarily grew its buffer")) {
        return false;
    }

    pc2ps3::Framebuffer invalid(0, 1);
    std::vector<std::uint8_t> unchanged(1u, 0x5au);
    return expect(!pc2ps3::win32_detail::framebuffer_to_bgra(invalid,
                                                             unchanged),
                  "zero-width framebuffer was accepted") &&
           expect(unchanged.size() == 1u && unchanged[0] == 0x5au,
                  "failed conversion changed the output buffer");
}

bool test_keyboard_mapping() {
    pc2ps3::win32_detail::PcKeyState keys;
    keys.left = true;
    keys.jump = true;
    keys.attack = true;
    keys.pause = true;
    pc2ps3::InputSnapshot snapshot;
    snapshot.dualshock3_right = true;
    pc2ps3::win32_detail::map_pc_keys(keys, snapshot);
    return expect(snapshot.keyboard_left && snapshot.keyboard_jump &&
                      snapshot.keyboard_attack && snapshot.keyboard_pause,
                  "keyboard actions were not mapped") &&
           expect(!snapshot.keyboard_right && snapshot.dualshock3_right,
                  "keyboard mapping changed an unrelated field");
}

bool test_hidden_window_lifecycle() {
    pc2ps3::Win32RuntimePlatform platform(false);
    if (!expect(!platform.initialize(0, 32),
                "invalid window dimensions were accepted") ||
        !expect(platform.initialize(64, 32),
                "hidden window initialization failed")) {
        return false;
    }

    pc2ps3::Framebuffer mismatched_framebuffer(2, 2);
    mismatched_framebuffer.clear(pc2ps3::Color{20, 40, 60, 255});
    if (!expect(!platform.present(mismatched_framebuffer),
                "framebuffer dimensions different from initialization were accepted")) {
        platform.shutdown();
        return false;
    }
    pc2ps3::Framebuffer framebuffer(64, 32);
    framebuffer.clear(pc2ps3::Color{20, 40, 60, 255});
    if (!expect(platform.present(framebuffer),
                "hidden window framebuffer presentation failed")) {
        platform.shutdown();
        return false;
    }

    pc2ps3::InputSnapshot snapshot;
    std::uint64_t elapsed_ns = 0;
    if (!expect(platform.poll(snapshot, elapsed_ns) ==
                    pc2ps3::RuntimePollStatus::kFrame,
                "poll did not return a frame")) {
        platform.shutdown();
        return false;
    }
    if (!expect(elapsed_ns < 10000000000ull,
                "monotonic timer conversion produced an unreasonable frame")) {
        platform.shutdown();
        return false;
    }

    HWND window = FindWindowW(L"PC2PS3RuntimeWindow", L"PC2PS3 Runtime Demo");
    if (!expect(window != 0, "hidden test window was not registered")) {
        platform.shutdown();
        return false;
    }
    PostMessageW(window, WM_CLOSE, 0, 0);
    if (!expect(platform.poll(snapshot, elapsed_ns) ==
                    pc2ps3::RuntimePollStatus::kQuit,
                "window close did not return kQuit")) {
        platform.shutdown();
        return false;
    }
    platform.shutdown();
    if (!expect(platform.initialize(64, 32),
                "platform could not be initialized again after shutdown")) {
        return false;
    }
    platform.shutdown();
    return true;
}

} // namespace

int main() {
    return test_pixel_conversion() && test_keyboard_mapping() &&
                   test_hidden_window_lifecycle()
               ? 0
               : 1;
}
