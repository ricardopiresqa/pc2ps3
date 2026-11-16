#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cmath>

#include "pc2ps3/win32_runtime_platform.hpp"
#include "win32_framebuffer_internal.hpp"
#include "win32_frame_pacer_internal.hpp"

#include <limits>
#include <new>
#include <vector>

namespace pc2ps3 {

namespace win32_detail {
bool configure_frame_pacing(std::uint32_t max_fps, std::uint64_t frequency,
                            std::uint64_t current_counter,
                            std::uint64_t& period_ticks,
                            std::uint64_t& deadline_ticks) {
    period_ticks = 0;
    deadline_ticks = 0;
    if (max_fps > 240 || frequency == 0) return false;
    if (max_fps == 0) return true;
    period_ticks = frequency / max_fps;
    if (period_ticks == 0 ||
        current_counter >
            std::numeric_limits<std::uint64_t>::max() - period_ticks) {
        period_ticks = 0;
        return false;
    }
    deadline_ticks = current_counter + period_ticks;
    return true;
}

bool frame_wait_timeout_ms(std::uint64_t current_counter,
                           std::uint64_t frequency,
                           std::uint64_t period_ticks,
                           std::uint64_t& deadline_ticks,
                           std::uint32_t& timeout_ms) {
    timeout_ms = 0;
    if (frequency == 0) return false;
    if (period_ticks == 0) return true;
    if (current_counter >= deadline_ticks) {
        if (current_counter >
            std::numeric_limits<std::uint64_t>::max() - period_ticks) {
            return false;
        }
        deadline_ticks = current_counter + period_ticks;
        return true;
    }

    const std::uint64_t remaining = deadline_ticks - current_counter;
    const double wait_ms = std::ceil(
        1000.0 * static_cast<double>(remaining) /
        static_cast<double>(frequency));
    timeout_ms = wait_ms < 1.0
                     ? 1u
                     : (wait_ms > 1000.0 ? 1000u
                                         : static_cast<std::uint32_t>(wait_ms));
    return true;
}
}  // namespace win32_detail

struct Win32RuntimePlatformState {
    HWND window;
    bool quit;
    int framebuffer_width;
    int framebuffer_height;
    std::vector<std::uint8_t> bgra;

    Win32RuntimePlatformState()
        : window(0), quit(false), framebuffer_width(0), framebuffer_height(0),
          bgra() {}
};

namespace {
const wchar_t kWindowClassName[] = L"PC2PS3RuntimeWindow";
const wchar_t kWindowTitle[] = L"PC2PS3 Runtime Demo";

LRESULT CALLBACK runtime_window_proc(HWND window, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
    Win32RuntimePlatformState* state = reinterpret_cast<
        Win32RuntimePlatformState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = static_cast<Win32RuntimePlatformState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(state));
        state->window = window;
    }

    if (state != 0) {
        switch (message) {
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            state->window = 0;
            state->quit = true;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_NCDESTROY:
            if (state->window == window) {
                state->window = 0;
            }
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;
        default:
            break;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool key_down(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

std::uint64_t counter_to_nanoseconds(std::uint64_t ticks,
                                    std::uint64_t frequency,
                                    bool& valid) {
    const std::uint64_t scale = 1000000000u;
    const std::uint64_t max_value = std::numeric_limits<std::uint64_t>::max();
    if (frequency == 0 || frequency > max_value / scale) {
        valid = false;
        return 0;
    }
    const std::uint64_t seconds = ticks / frequency;
    const std::uint64_t remainder = ticks % frequency;
    if (seconds > max_value / scale) {
        valid = false;
        return 0;
    }
    const std::uint64_t whole_ns = seconds * scale;
    const std::uint64_t fractional_ns = (remainder * scale) / frequency;
    if (fractional_ns > max_value - whole_ns) {
        valid = false;
        return 0;
    }
    valid = true;
    return whole_ns + fractional_ns;
}
} // namespace

Win32RuntimePlatform::Win32RuntimePlatform(bool show_window,
                                           std::uint32_t max_fps)
    : state_(new Win32RuntimePlatformState()), counter_frequency_(0),
      previous_counter_(0), frame_period_ticks_(0), frame_deadline_ticks_(0),
max_fps_(max_fps), show_window_(show_window), gamepad_() {}

Win32RuntimePlatform::~Win32RuntimePlatform() noexcept {
    shutdown();
    delete state_;
}

bool Win32RuntimePlatform::initialize(int width, int height) {
    const std::size_t max_bytes = 128u * 1024u * 1024u;
    if (max_fps_ > 240 || state_ == 0 || state_->window != 0 || width <= 0 || height <= 0 ||
        width > 8192 || height > 8192 ||
        static_cast<std::size_t>(width) >
            max_bytes / 4u / static_cast<std::size_t>(height)) {
        return false;
    }

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = runtime_window_proc;
    window_class.hInstance = GetModuleHandleW(0);
    window_class.hCursor = LoadCursorW(0, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(
        static_cast<INT_PTR>(COLOR_WINDOW + 1));
    window_class.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&window_class) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    LARGE_INTEGER frequency = {};
    LARGE_INTEGER counter = {};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&counter) || counter.QuadPart < 0) {
        return false;
    }
    counter_frequency_ = static_cast<std::uint64_t>(frequency.QuadPart);
    previous_counter_ = static_cast<std::uint64_t>(counter.QuadPart);
    if (!win32_detail::configure_frame_pacing(
            max_fps_, counter_frequency_, previous_counter_,
            frame_period_ticks_, frame_deadline_ticks_)) {
        counter_frequency_ = 0;
        previous_counter_ = 0;
        return false;
    }
    bool conversion_valid = false;
    counter_to_nanoseconds(0, counter_frequency_, conversion_valid);
    if (!conversion_valid) {
        counter_frequency_ = 0;
        previous_counter_ = 0;
        return false;
    }

    const std::size_t pixel_bytes =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) * 4u;
    try {
        state_->bgra.resize(pixel_bytes);
    } catch (const std::bad_alloc&) {
        counter_frequency_ = 0;
        previous_counter_ = 0;
        return false;
    }

    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT bounds = {0, 0, width, height};
    if (!AdjustWindowRect(&bounds, style, FALSE)) {
        state_->bgra.clear();
        return false;
    }
    HWND window = CreateWindowExW(
        0, kWindowClassName, kWindowTitle, style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top, 0, 0,
        GetModuleHandleW(0), state_);
    if (window == 0) {
        state_->bgra.clear();
        state_->window = 0;
        state_->quit = false;
        counter_frequency_ = 0;
        previous_counter_ = 0;
        return false;
    }
(void)gamepad_.initialize();
state_->window = window;
    state_->quit = false;
    state_->framebuffer_width = width;
    state_->framebuffer_height = height;
    if (show_window_) {
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
    }
    return true;
}

RuntimePollStatus Win32RuntimePlatform::poll(InputSnapshot& snapshot,
                                              std::uint64_t& elapsed_ns) {
    elapsed_ns = 0;
    snapshot = InputSnapshot();
    if (state_ == 0) {
        return RuntimePollStatus::kError;
    }
    if (state_->quit) {
        return RuntimePollStatus::kQuit;
    }
    if (state_->window == 0 || counter_frequency_ == 0) {
        return RuntimePollStatus::kError;
    }

    MSG message = {};
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            state_->quit = true;
            return RuntimePollStatus::kQuit;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (state_->quit) {
            return RuntimePollStatus::kQuit;
        }
    }

    LARGE_INTEGER counter = {};
    if (!QueryPerformanceCounter(&counter) || counter.QuadPart < 0) {
        return RuntimePollStatus::kError;
    }
    std::uint64_t current = static_cast<std::uint64_t>(counter.QuadPart);
    for (;;) {
        std::uint32_t timeout_ms = 0;
        if (!win32_detail::frame_wait_timeout_ms(
                current, counter_frequency_, frame_period_ticks_,
                frame_deadline_ticks_, timeout_ms)) {
            return RuntimePollStatus::kError;
        }
        if (timeout_ms == 0) break;
        const DWORD wait_result = MsgWaitForMultipleObjectsEx(
            0, 0, timeout_ms, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wait_result != WAIT_TIMEOUT && wait_result != WAIT_OBJECT_0) {
            return RuntimePollStatus::kError;
        }
        if (wait_result == WAIT_OBJECT_0) {
            while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    state_->quit = true;
                    return RuntimePollStatus::kQuit;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
                if (state_->quit) return RuntimePollStatus::kQuit;
            }
        }
        if (!QueryPerformanceCounter(&counter) || counter.QuadPart < 0) {
            return RuntimePollStatus::kError;
        }
        current = static_cast<std::uint64_t>(counter.QuadPart);
    }
    if (current < previous_counter_) {
        return RuntimePollStatus::kError;
    }
    bool conversion_valid = false;
    elapsed_ns = counter_to_nanoseconds(current - previous_counter_,
                                       counter_frequency_, conversion_valid);
    if (!conversion_valid) {
        elapsed_ns = 0;
        return RuntimePollStatus::kError;
    }
    previous_counter_ = current;

    if (GetForegroundWindow() == state_->window) {
        win32_detail::PcKeyState keys;
        keys.left = key_down(VK_LEFT) || key_down('A');
        keys.right = key_down(VK_RIGHT) || key_down('D');
        keys.up = key_down(VK_UP) || key_down('W');
        keys.down = key_down(VK_DOWN) || key_down('S');
        keys.jump = key_down(VK_SPACE) || key_down('Z');
        keys.confirm = key_down(VK_RETURN);
        keys.attack = key_down('J') || key_down('X');
        keys.cancel = key_down(VK_BACK);
        keys.ability = key_down('K') || key_down('C');
        keys.l1 = key_down('Q');
        keys.r1 = key_down('E');
        keys.pause = key_down('P');
        keys.menu = key_down(VK_TAB);
        win32_detail::map_pc_keys(keys, snapshot);
        PlatformControllerSample controller;
        gamepad_.poll(0, controller);
        apply_platform_controller_sample(controller, snapshot);
        if (key_down(VK_ESCAPE)) {
            state_->quit = true;
            return RuntimePollStatus::kQuit;
        }
    }
    return RuntimePollStatus::kFrame;
}

bool Win32RuntimePlatform::present(const Framebuffer& framebuffer) {
    if (state_ == 0 || state_->window == 0 ||
        framebuffer.width() != state_->framebuffer_width ||
        framebuffer.height() != state_->framebuffer_height) {
        return false;
    }
    if (!win32_detail::framebuffer_to_bgra(framebuffer, state_->bgra)) {
        return false;
    }

    RECT client = {};
    if (!GetClientRect(state_->window, &client)) {
        return false;
    }
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    if (client_width <= 0 || client_height <= 0) {
        return true;
    }

    BITMAPINFO bitmap = {};
    bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
    bitmap.bmiHeader.biWidth = framebuffer.width();
    bitmap.bmiHeader.biHeight = -framebuffer.height();
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    bitmap.bmiHeader.biSizeImage = static_cast<DWORD>(state_->bgra.size());

    HDC device = GetDC(state_->window);
    if (device == 0) {
        return false;
    }
    const int copied = StretchDIBits(
        device, 0, 0, client_width, client_height, 0, 0, framebuffer.width(),
        framebuffer.height(), state_->bgra.data(), &bitmap, DIB_RGB_COLORS,
        SRCCOPY);
    const int released = ReleaseDC(state_->window, device);
    return copied != GDI_ERROR && copied > 0 && released == 1;
}

void Win32RuntimePlatform::shutdown() {
    gamepad_.shutdown();
    if (state_ == 0) {
        return;
    }
    if (state_->window != 0) {
        DestroyWindow(state_->window);
        state_->window = 0;
    }
    state_->quit = true;
    state_->framebuffer_width = 0;
    state_->framebuffer_height = 0;
    std::vector<std::uint8_t>().swap(state_->bgra);
    counter_frequency_ = 0;
    previous_counter_ = 0;
    frame_period_ticks_ = 0;
    frame_deadline_ticks_ = 0;
}

} // namespace pc2ps3
