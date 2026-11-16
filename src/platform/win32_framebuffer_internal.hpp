#pragma once

#include "pc2ps3/input.hpp"
#include "pc2ps3/render.hpp"

#include <cstdint>
#include <vector>

namespace pc2ps3 {
namespace win32_detail {

struct PcKeyState {
    bool left;
    bool right;
    bool up;
    bool down;
    bool jump;
    bool confirm;
    bool attack;
    bool cancel;
    bool ability;
    bool l1;
    bool r1;
    bool pause;
    bool menu;

    PcKeyState();
};

void map_pc_keys(const PcKeyState& keys, InputSnapshot& snapshot);
bool framebuffer_to_bgra(const Framebuffer& framebuffer,
                         std::vector<std::uint8_t>& output);

} // namespace win32_detail
} // namespace pc2ps3
