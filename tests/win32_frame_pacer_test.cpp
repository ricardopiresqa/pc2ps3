#include "pc2ps3/win32_runtime_platform.hpp"
#include "win32_frame_pacer_internal.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <cstdint>
#include <limits>

namespace {
void test_schedule_and_ceil_timeout() {
  std::uint64_t period = 0;
  std::uint64_t deadline = 0;
  assert(pc2ps3::win32_detail::configure_frame_pacing(
      60, 1000000, 100, period, deadline));
  assert(period == 16666 && deadline == 16766);

  std::uint32_t timeout = 0;
  assert(pc2ps3::win32_detail::frame_wait_timeout_ms(
      100, 1000000, period, deadline, timeout));
  assert(timeout == 17);
  assert(pc2ps3::win32_detail::frame_wait_timeout_ms(
      deadline - 1, 1000000, period, deadline, timeout));
  assert(timeout == 1);
}

void test_late_frame_resets_deadline_without_catch_up() {
  std::uint64_t period = 0;
  std::uint64_t deadline = 0;
  assert(pc2ps3::win32_detail::configure_frame_pacing(
      60, 1000, 0, period, deadline));
  std::uint32_t timeout = 99;
  const std::uint64_t late = deadline + 500;
  assert(pc2ps3::win32_detail::frame_wait_timeout_ms(
      late, 1000, period, deadline, timeout));
  assert(timeout == 0 && deadline == late + period);
  assert(pc2ps3::win32_detail::frame_wait_timeout_ms(
      late + 1, 1000, period, deadline, timeout));
  assert(timeout == 15);
}

void test_disabled_invalid_and_overflow() {
  std::uint64_t period = 55;
  std::uint64_t deadline = 77;
  assert(pc2ps3::win32_detail::configure_frame_pacing(
      0, 1000, 100, period, deadline));
  assert(period == 0 && deadline == 0);
  std::uint32_t timeout = 99;
  assert(pc2ps3::win32_detail::frame_wait_timeout_ms(
      100, 1000, period, deadline, timeout));
  assert(timeout == 0);

  assert(!pc2ps3::win32_detail::configure_frame_pacing(
      241, 1000, 0, period, deadline));
  assert(!pc2ps3::win32_detail::configure_frame_pacing(
      60, 0, 0, period, deadline));
  assert(!pc2ps3::win32_detail::configure_frame_pacing(
      240, 100, 0, period, deadline));
  assert(!pc2ps3::win32_detail::configure_frame_pacing(
      60, 1000, std::numeric_limits<std::uint64_t>::max() - 10,
      period, deadline));

  assert(pc2ps3::win32_detail::configure_frame_pacing(
      60, 1000, 0, period, deadline));
  assert(!pc2ps3::win32_detail::frame_wait_timeout_ms(
      std::numeric_limits<std::uint64_t>::max() - 5, 1000, period,
      deadline, timeout));
}

void test_hidden_window_waits_and_still_quits() {
  pc2ps3::Win32RuntimePlatform platform(false, 60);
  assert(platform.initialize(16, 16));
  pc2ps3::InputSnapshot snapshot;
  std::uint64_t elapsed_ns = 0;
  assert(platform.poll(snapshot, elapsed_ns) == pc2ps3::RuntimePollStatus::kFrame);
  assert(elapsed_ns >= 8000000u);
  HWND window = FindWindowW(L"PC2PS3RuntimeWindow", L"PC2PS3 Runtime Demo");
  assert(window != 0);
  assert(PostMessageW(window, WM_CLOSE, 0, 0));
  assert(platform.poll(snapshot, elapsed_ns) == pc2ps3::RuntimePollStatus::kQuit);
  platform.shutdown();
}

void test_invalid_fps_rejected_before_window_creation() {
  pc2ps3::Win32RuntimePlatform platform(false, 241);
  assert(!platform.initialize(16, 16));
  HWND window = FindWindowW(L"PC2PS3RuntimeWindow", L"PC2PS3 Runtime Demo");
  assert(window == 0);
  platform.shutdown();
}
}  // namespace

int main() {
  test_schedule_and_ceil_timeout();
  test_late_frame_resets_deadline_without_catch_up();
  test_disabled_invalid_and_overflow();
  test_hidden_window_waits_and_still_quits();
  test_invalid_fps_rejected_before_window_creation();
  return 0;
}
