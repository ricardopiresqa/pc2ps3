#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>

#include "pc2ps3/win32_gamepad.hpp"

#include <cstddef>
#include <cwchar>

namespace pc2ps3 {
namespace {

typedef DWORD(WINAPI* XInputGetStateFunction)(DWORD, XINPUT_STATE*);

std::int16_t normalize_axis(std::int16_t value, int deadzone) {
  const int axis = value;
  if (axis >= -deadzone && axis <= deadzone) return 0;
  if (axis > deadzone) {
    const long long scaled =
        static_cast<long long>(axis - deadzone) * 32767 / (32767 - deadzone);
    return static_cast<std::int16_t>(scaled);
  }
  const long long scaled =
      static_cast<long long>(axis + deadzone) * 32768 / (32768 - deadzone);
  return static_cast<std::int16_t>(scaled);
}

bool has_button(const Win32GamepadState& state, std::uint16_t button) {
  return (state.buttons & button) != 0;
}

HMODULE load_system_xinput(const wchar_t* filename) {
  wchar_t path[MAX_PATH + 32] = {};
  const UINT system_length = GetSystemDirectoryW(path, MAX_PATH);
  if (system_length == 0 || system_length >= MAX_PATH) return 0;

  std::size_t length = system_length;
  if (length == 0 || path[length - 1] != L'\\') path[length++] = L'\\';
  const std::size_t filename_length = std::wcslen(filename);
  if (length + filename_length + 1 > sizeof(path) / sizeof(path[0])) return 0;
  for (std::size_t i = 0; i <= filename_length; ++i) {
    path[length + i] = filename[i];
  }
  return LoadLibraryW(path);
}

}  // namespace

Win32GamepadState::Win32GamepadState()
    : buttons(0),
      left_thumb_x(0),
      left_thumb_y(0),
      left_trigger(0),
      right_trigger(0) {}

Win32GamepadAdapter::Win32GamepadAdapter()
    : module_(0),
      get_state_(0),
      reader_(0),
      reader_context_(0),
      initialized_(false) {}

Win32GamepadAdapter::Win32GamepadAdapter(Win32GamepadReader reader,
                                         void* context)
    : module_(0),
      get_state_(0),
      reader_(reader),
      reader_context_(context),
      initialized_(false) {}

Win32GamepadAdapter::~Win32GamepadAdapter() noexcept { shutdown(); }

bool Win32GamepadAdapter::initialize() {
  if (initialized_) return true;
  if (reader_ != 0) {
    initialized_ = true;
    return true;
  }

  const wchar_t* const dlls[] = {L"xinput1_4.dll", L"xinput1_3.dll",
                                 L"xinput9_1_0.dll"};
  for (std::size_t i = 0; i < sizeof(dlls) / sizeof(dlls[0]); ++i) {
    HMODULE module = load_system_xinput(dlls[i]);
    if (module == 0) continue;
    FARPROC symbol = GetProcAddress(module, "XInputGetState");
    if (symbol == 0) {
      FreeLibrary(module);
      continue;
    }
    module_ = module;
    get_state_ = reinterpret_cast<void*>(symbol);
    reader_ = &Win32GamepadAdapter::read_native;
    reader_context_ = this;
    initialized_ = true;
    return true;
  }
  return false;
}

Win32GamepadStatus Win32GamepadAdapter::poll(
    std::uint32_t index, PlatformControllerSample& sample) noexcept {
  sample = PlatformControllerSample();
  if (!initialized_ || reader_ == 0) return Win32GamepadStatus::kUnavailable;
  if (index >= XUSER_MAX_COUNT) return Win32GamepadStatus::kError;

  Win32GamepadState state;
  Win32GamepadStatus status = Win32GamepadStatus::kError;
  try {
    status = reader_(reader_context_, index, state);
  } catch (...) {
    return Win32GamepadStatus::kError;
  }
  if (status != Win32GamepadStatus::kConnected) return status;
  sample = map_state(state);
  return Win32GamepadStatus::kConnected;
}

void Win32GamepadAdapter::shutdown() noexcept {
  if (module_ != 0) FreeLibrary(static_cast<HMODULE>(module_));
  module_ = 0;
  get_state_ = 0;
  if (reader_ == &Win32GamepadAdapter::read_native) {
    reader_ = 0;
    reader_context_ = 0;
  }
  initialized_ = false;
}

PlatformControllerSample Win32GamepadAdapter::map_state(
    const Win32GamepadState& state) {
  PlatformControllerSample sample;
  sample.dpad_up = has_button(state, kWin32DpadUp);
  sample.dpad_down = has_button(state, kWin32DpadDown);
  sample.dpad_left = has_button(state, kWin32DpadLeft);
  sample.dpad_right = has_button(state, kWin32DpadRight);
  sample.axis_x = normalize_axis(state.left_thumb_x,
                                 XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  const std::int16_t y = normalize_axis(state.left_thumb_y,
                                       XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  sample.axis_y = y == -32768 ? 32767 : static_cast<std::int16_t>(-y);
  sample.cross = has_button(state, kWin32A);
  sample.jump = sample.cross;
  sample.circle = has_button(state, kWin32B);
  sample.square = has_button(state, kWin32X);
  sample.triangle = has_button(state, kWin32Y);
  sample.l1 = has_button(state, kWin32LeftShoulder) ||
              state.left_trigger >= kWin32TriggerThreshold;
  sample.r1 = has_button(state, kWin32RightShoulder) ||
              state.right_trigger >= kWin32TriggerThreshold;
  sample.start = has_button(state, kWin32Start);
  sample.select = has_button(state, kWin32Back);
  return sample;
}

Win32GamepadStatus Win32GamepadAdapter::read_native(
    void* context, std::uint32_t index, Win32GamepadState& output) {
  Win32GamepadAdapter* const adapter =
      static_cast<Win32GamepadAdapter*>(context);
  if (adapter == 0 || adapter->get_state_ == 0 || index >= XUSER_MAX_COUNT) {
    return Win32GamepadStatus::kError;
  }
  const XInputGetStateFunction get_state =
      reinterpret_cast<XInputGetStateFunction>(adapter->get_state_);
  XINPUT_STATE state = {};
  const DWORD result = get_state(index, &state);
  if (result == ERROR_DEVICE_NOT_CONNECTED) {
    return Win32GamepadStatus::kDisconnected;
  }
  if (result != ERROR_SUCCESS) return Win32GamepadStatus::kError;

  output.buttons = state.Gamepad.wButtons;
  output.left_thumb_x = state.Gamepad.sThumbLX;
  output.left_thumb_y = state.Gamepad.sThumbLY;
  output.left_trigger = state.Gamepad.bLeftTrigger;
  output.right_trigger = state.Gamepad.bRightTrigger;
  return Win32GamepadStatus::kConnected;
}

}  // namespace pc2ps3
