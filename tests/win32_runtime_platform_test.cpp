#include "pc2ps3/win32_runtime_platform.hpp"

#include <cassert>
#include <cstdint>

namespace {

void test_hidden_runtime_stays_available_without_focused_controller() {
  pc2ps3::Win32RuntimePlatform platform(false, 0);
  assert(platform.initialize(64, 64));

  pc2ps3::InputSnapshot snapshot;
  std::uint64_t elapsed_ns = 0;
  assert(platform.poll(snapshot, elapsed_ns) == pc2ps3::RuntimePollStatus::kFrame);
  platform.shutdown();
}

void test_controller_merge_preserves_keyboard() {
  pc2ps3::InputSnapshot snapshot;
  snapshot.keyboard_jump = true;
  pc2ps3::PlatformControllerSample controller;
  controller.jump = true;
  pc2ps3::apply_platform_controller_sample(controller, snapshot);
  assert(snapshot.keyboard_jump);
  assert(snapshot.dualshock3_jump);
}

}  // namespace

int main() {
  test_hidden_runtime_stays_available_without_focused_controller();
  test_controller_merge_preserves_keyboard();
  return 0;
}
