#pragma once

#include "pc2ps3/platform_input.hpp"
#include <cstdint>

namespace pc2ps3 {

struct Win32GamepadState {
  std::uint16_t buttons;
  std::int16_t left_thumb_x;
  std::int16_t left_thumb_y;
  std::uint8_t left_trigger;
  std::uint8_t right_trigger;

  Win32GamepadState();
};

enum class Win32GamepadStatus {
  kConnected,
  kDisconnected,
  kUnavailable,
  kError
};

enum Win32GamepadButton {
  kWin32DpadUp = 0x0001,
  kWin32DpadDown = 0x0002,
  kWin32DpadLeft = 0x0004,
  kWin32DpadRight = 0x0008,
  kWin32Start = 0x0010,
  kWin32Back = 0x0020,
  kWin32LeftShoulder = 0x0100,
  kWin32RightShoulder = 0x0200,
  kWin32A = 0x1000,
  kWin32B = 0x2000,
  kWin32X = 0x4000,
  kWin32Y = 0x8000,
  kWin32TriggerThreshold = 30
};

typedef Win32GamepadStatus (*Win32GamepadReader)(
    void* context, std::uint32_t index, Win32GamepadState& output);

// Optional XInput adapter. The injected reader is a deterministic test seam;
// the default constructor loads the system XInput DLL at initialize().
class Win32GamepadAdapter {
 public:
  Win32GamepadAdapter();
  Win32GamepadAdapter(Win32GamepadReader reader, void* context);
  ~Win32GamepadAdapter() noexcept;

  Win32GamepadAdapter(const Win32GamepadAdapter&) = delete;
  Win32GamepadAdapter& operator=(const Win32GamepadAdapter&) = delete;

  bool initialize();
  Win32GamepadStatus poll(std::uint32_t index,
                          PlatformControllerSample& sample) noexcept;
  void shutdown() noexcept;

  static PlatformControllerSample map_state(const Win32GamepadState& state);

 private:
  static Win32GamepadStatus read_native(void* context, std::uint32_t index,
                                        Win32GamepadState& output);

  void* module_;
  void* get_state_;
  Win32GamepadReader reader_;
  void* reader_context_;
  bool initialized_;
};

}  // namespace pc2ps3
