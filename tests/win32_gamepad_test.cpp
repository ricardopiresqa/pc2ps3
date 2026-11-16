#include "pc2ps3/win32_gamepad.hpp"

#include <cassert>
#include <cstdint>

namespace {
struct FakeReader {
  pc2ps3::Win32GamepadStatus status;
  pc2ps3::Win32GamepadState state;
  std::uint32_t calls;
};

pc2ps3::Win32GamepadStatus read_fake(void* context, std::uint32_t index,
                                     pc2ps3::Win32GamepadState& output) {
  FakeReader* const reader = static_cast<FakeReader*>(context);
  ++reader->calls;
  if (index != 0) return pc2ps3::Win32GamepadStatus::kError;
  output = reader->state;
  return reader->status;
}

void test_map_axes_buttons_and_triggers() {
  pc2ps3::Win32GamepadState raw;
  raw.buttons = pc2ps3::kWin32DpadRight | pc2ps3::kWin32DpadUp |
                pc2ps3::kWin32A | pc2ps3::kWin32B | pc2ps3::kWin32X |
                pc2ps3::kWin32Y | pc2ps3::kWin32LeftShoulder |
                pc2ps3::kWin32Start | pc2ps3::kWin32Back;
  raw.left_thumb_x = 16000;
  raw.left_thumb_y = -32768;
  raw.right_trigger = pc2ps3::kWin32TriggerThreshold;

  pc2ps3::PlatformControllerSample sample =
      pc2ps3::Win32GamepadAdapter::map_state(raw);
  assert(sample.dpad_right && sample.dpad_up && !sample.dpad_left &&
         !sample.dpad_down);
  assert(sample.axis_x > 0 && sample.axis_x < 32767);
  assert(sample.axis_y == 32767);
  assert(sample.cross && sample.jump && sample.square && sample.circle &&
         sample.triangle);
  assert(sample.l1 && sample.r1 && sample.start && sample.select);

  raw.buttons = 0;
  raw.left_trigger = pc2ps3::kWin32TriggerThreshold;
  raw.right_trigger = pc2ps3::kWin32TriggerThreshold - 1;
  sample = pc2ps3::Win32GamepadAdapter::map_state(raw);
  assert(sample.l1 && !sample.r1);
}

void test_deadzone_and_negative_axis() {
  pc2ps3::Win32GamepadState raw;
  raw.left_thumb_x = 7000;
  raw.left_thumb_y = 0;
  pc2ps3::PlatformControllerSample sample =
      pc2ps3::Win32GamepadAdapter::map_state(raw);
  assert(sample.axis_x == 0 && sample.axis_y == 0);

  raw.left_thumb_x = -32768;
  sample = pc2ps3::Win32GamepadAdapter::map_state(raw);
  assert(sample.axis_x == -32768);
  raw.left_thumb_x = 32767;
  sample = pc2ps3::Win32GamepadAdapter::map_state(raw);
  assert(sample.axis_x == 32767);
}

void test_disconnect_error_and_reconnect_are_neutral_or_mapped() {
  FakeReader reader = {pc2ps3::Win32GamepadStatus::kConnected,
                       pc2ps3::Win32GamepadState(), 0};
  reader.state.buttons = pc2ps3::kWin32A;
  pc2ps3::Win32GamepadAdapter adapter(&read_fake, &reader);
  assert(adapter.initialize());

  pc2ps3::PlatformControllerSample sample;
  assert(adapter.poll(0, sample) == pc2ps3::Win32GamepadStatus::kConnected);
  assert(sample.jump);

  reader.status = pc2ps3::Win32GamepadStatus::kDisconnected;
  sample.jump = true;
  sample.dpad_left = true;
  assert(adapter.poll(0, sample) == pc2ps3::Win32GamepadStatus::kDisconnected);
  assert(!sample.jump && !sample.dpad_left && sample.axis_x == 0);

  reader.status = pc2ps3::Win32GamepadStatus::kError;
  sample.square = true;
  assert(adapter.poll(0, sample) == pc2ps3::Win32GamepadStatus::kError);
  assert(!sample.square && !sample.cross);

  reader.status = pc2ps3::Win32GamepadStatus::kConnected;
  reader.state.buttons = pc2ps3::kWin32B;
  assert(adapter.poll(0, sample) == pc2ps3::Win32GamepadStatus::kConnected);
  assert(sample.circle && !sample.jump);
  assert(reader.calls == 4);
  assert(adapter.poll(4, sample) == pc2ps3::Win32GamepadStatus::kError);
  assert(!sample.circle);
}

void test_apply_preserves_keyboard_and_shutdown_is_idempotent() {
  FakeReader reader = {pc2ps3::Win32GamepadStatus::kConnected,
                       pc2ps3::Win32GamepadState(), 0};
  reader.state.buttons = pc2ps3::kWin32A;
  pc2ps3::Win32GamepadAdapter adapter(&read_fake, &reader);
  assert(adapter.initialize());
  pc2ps3::PlatformControllerSample sample;
  assert(adapter.poll(0, sample) == pc2ps3::Win32GamepadStatus::kConnected);
  pc2ps3::InputSnapshot snapshot;
  snapshot.keyboard_left = true;
  pc2ps3::apply_platform_controller_sample(sample, snapshot);
  assert(snapshot.keyboard_left && snapshot.dualshock3_jump);
  adapter.shutdown();
  adapter.shutdown();
  assert(adapter.initialize());
}
}  // namespace

int main() {
  test_map_axes_buttons_and_triggers();
  test_deadzone_and_negative_axis();
  test_disconnect_error_and_reconnect_are_neutral_or_mapped();
  test_apply_preserves_keyboard_and_shutdown_is_idempotent();
  return 0;
}
