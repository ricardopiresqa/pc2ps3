#include "win32_framebuffer_internal.hpp"

#include <limits>
#include <new>

namespace pc2ps3 {
namespace win32_detail {
namespace {
const std::size_t kMaxFramebufferBytes = 128u * 1024u * 1024u;
}

PcKeyState::PcKeyState()
    : left(false), right(false), up(false), down(false), jump(false),
      confirm(false), attack(false), cancel(false), ability(false), l1(false),
      r1(false), pause(false), menu(false) {}

void map_pc_keys(const PcKeyState& keys, InputSnapshot& snapshot) {
    snapshot.keyboard_left = keys.left;
    snapshot.keyboard_right = keys.right;
    snapshot.keyboard_up = keys.up;
    snapshot.keyboard_down = keys.down;
    snapshot.keyboard_jump = keys.jump;
    snapshot.keyboard_confirm = keys.confirm;
    snapshot.keyboard_attack = keys.attack;
    snapshot.keyboard_cancel = keys.cancel;
    snapshot.keyboard_ability = keys.ability;
    snapshot.keyboard_l1 = keys.l1;
    snapshot.keyboard_r1 = keys.r1;
    snapshot.keyboard_pause = keys.pause;
    snapshot.keyboard_menu = keys.menu;
}

bool framebuffer_to_bgra(const Framebuffer& framebuffer,
                         std::vector<std::uint8_t>& output) {
    const int width = framebuffer.width();
    const int height = framebuffer.height();
    if (width <= 0 || height <= 0 || framebuffer.pixel_data() == 0) {
        return false;
    }

    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h) {
        return false;
    }
    const std::size_t pixels = w * h;
    if (pixels > kMaxFramebufferBytes / 4u ||
        pixels > std::numeric_limits<std::size_t>::max() / 4u ||
        framebuffer.pixel_storage_bytes() != pixels * 4u) {
        return false;
    }

    std::size_t byte_count = pixels * 4u;
    try {
        output.resize(byte_count);
    } catch (const std::bad_alloc&) {
        return false;
    }

    const Color* source = framebuffer.pixel_data();
    for (std::size_t index = 0; index < pixels; ++index) {
        const std::size_t offset = index * 4u;
        output[offset] = source[index].b;
        output[offset + 1u] = source[index].g;
        output[offset + 2u] = source[index].r;
        output[offset + 3u] = 0xffu;
    }
    return true;
}

} // namespace win32_detail
} // namespace pc2ps3
