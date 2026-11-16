#include "pc2ps3/sdl_runtime_platform.hpp"

#include "pc2ps3/audio_callback.hpp"
#include "pc2ps3/framebuffer_presenter.hpp"
#include "pc2ps3/platform_input.hpp"

#include <SDL.h>

#include <cstdio>

namespace pc2ps3 {
namespace {

// SDL2_PSL1GHT joystick button indices (src/joystick/psl1ght/SDL_sysjoystick.c).
enum Ps3Button {
    kLeft = 0, kDown = 1, kRight = 2, kUp = 3, kStart = 4, kSelect = 7,
    kSquare = 8, kCross = 9, kCircle = 10, kTriangle = 11, kR1 = 12, kL1 = 13
};

const int kDeadZone = 8000;

std::int16_t dead_zone(Sint16 value) {
    return (value > -kDeadZone && value < kDeadZone) ? 0 : value;
}

bool button(SDL_Joystick* joystick, int index) {
    return joystick != 0 && SDL_JoystickGetButton(joystick, index) != 0;
}

} // namespace

SdlRuntimePlatform::SdlRuntimePlatform(int output_width, int output_height)
    : output_width_(output_width), output_height_(output_height), window_(0),
      renderer_(0), texture_(0), joystick_(0), audio_device_(0),
      audio_manager_(0), counter_frequency_(0), previous_counter_(0),
      fps_window_start_(0), fps_frames_(0), fps_(0.0),
      sdl_initialized_(false) {}

SdlRuntimePlatform::~SdlRuntimePlatform() noexcept { shutdown(); }

bool SdlRuntimePlatform::initialize(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        std::printf("[pc2ps3] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    sdl_initialized_ = true;
    // PSL1GHT has no OpenGL: create the window without SDL_WINDOW_OPENGL.
    window_ = SDL_CreateWindow("PC2PS3", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, output_width_,
                               output_height_, SDL_WINDOW_FULLSCREEN);
    if (window_ == 0) {
        std::printf("[pc2ps3] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == 0) {
        renderer_ = SDL_CreateRenderer(window_, -1, 0);
    }
    if (renderer_ == 0) {
        std::printf("[pc2ps3] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    // ARGB8888 is the RSX-native layout; present_framebuffer writes BGRA bytes
    // on little endian, so pick the order matching the host byte order.
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, width, height);
    if (texture_ == 0) {
        std::printf("[pc2ps3] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_NumJoysticks() > 0) {
        joystick_ = SDL_JoystickOpen(0);
    }
    std::printf("[pc2ps3] video %dx%d -> %dx%d, joysticks=%d\n", width, height,
                output_width_, output_height_, SDL_NumJoysticks());
    counter_frequency_ = SDL_GetPerformanceFrequency();
    previous_counter_ = SDL_GetPerformanceCounter();
    fps_window_start_ = previous_counter_;
    return true;
}

RuntimePollStatus SdlRuntimePlatform::poll(InputSnapshot& snapshot,
                                           std::uint64_t& elapsed_ns) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            std::printf("[pc2ps3] quit requested\n");
            return RuntimePollStatus::kQuit;
        }
    }
    // PSL1GHT may not emit SDL_JOYDEVICEADDED, so pick up a pad connected after boot.
    if (joystick_ == 0 && SDL_NumJoysticks() > 0) {
        joystick_ = SDL_JoystickOpen(0);
        if (joystick_ != 0) std::printf("[pc2ps3] pad connected\n");
    }

    const Uint8* keys = SDL_GetKeyboardState(0);
    snapshot.keyboard_left = keys[SDL_SCANCODE_LEFT] != 0;
    snapshot.keyboard_right = keys[SDL_SCANCODE_RIGHT] != 0;
    snapshot.keyboard_up = keys[SDL_SCANCODE_UP] != 0;
    snapshot.keyboard_down = keys[SDL_SCANCODE_DOWN] != 0;
    snapshot.keyboard_jump = keys[SDL_SCANCODE_SPACE] != 0;
    snapshot.keyboard_confirm = keys[SDL_SCANCODE_RETURN] != 0;
    snapshot.keyboard_pause = keys[SDL_SCANCODE_ESCAPE] != 0;

    PlatformControllerSample pad;
    SDL_Joystick* joystick = joystick_;
    pad.dpad_left = button(joystick, kLeft);
    pad.dpad_right = button(joystick, kRight);
    pad.dpad_up = button(joystick, kUp);
    pad.dpad_down = button(joystick, kDown);
    pad.cross = button(joystick, kCross);
    pad.jump = pad.cross;
    pad.square = button(joystick, kSquare);
    pad.circle = button(joystick, kCircle);
    pad.triangle = button(joystick, kTriangle);
    pad.l1 = button(joystick, kL1);
    pad.r1 = button(joystick, kR1);
    pad.start = button(joystick, kStart);
    pad.select = button(joystick, kSelect);
    if (joystick != 0 && SDL_JoystickNumAxes(joystick) >= 2) {
        pad.axis_x = dead_zone(SDL_JoystickGetAxis(joystick, 0));
        pad.axis_y = dead_zone(SDL_JoystickGetAxis(joystick, 1));
    }
    apply_platform_controller_sample(pad, snapshot);
    if (joystick != 0) {  // TTY trace of raw pad changes, for RPCS3/console QA.
        std::uint32_t mask = 0;
        for (int i = 0; i < 16 && i < SDL_JoystickNumButtons(joystick); ++i) {
            if (SDL_JoystickGetButton(joystick, i)) mask |= 1u << i;
        }
        const int coarse_x = SDL_JoystickNumAxes(joystick) > 0 ? SDL_JoystickGetAxis(joystick, 0) / 8192 : 0;
        const int coarse_y = SDL_JoystickNumAxes(joystick) > 1 ? SDL_JoystickGetAxis(joystick, 1) / 8192 : 0;
        if (mask != last_mask_ || coarse_x != last_x_ || coarse_y != last_y_) {
            std::printf("[pc2ps3] pad buttons=0x%04x axis=%d,%d\n",
                        static_cast<unsigned>(mask), coarse_x, coarse_y);
            last_mask_ = mask;
            last_x_ = coarse_x;
            last_y_ = coarse_y;
        }
    }
    if (pad.start && pad.select) {
        std::printf("[pc2ps3] start+select quit\n");
        return RuntimePollStatus::kQuit;
    }

    const std::uint64_t now = SDL_GetPerformanceCounter();
    elapsed_ns = (now - previous_counter_) * 1000000000ull / counter_frequency_;
    previous_counter_ = now;
    ++fps_frames_;
    const std::uint64_t window = now - fps_window_start_;
    if (window >= counter_frequency_) {
        fps_ = static_cast<double>(fps_frames_) *
               static_cast<double>(counter_frequency_) /
               static_cast<double>(window);
        fps_frames_ = 0;
        fps_window_start_ = now;
    }
    return RuntimePollStatus::kFrame;
}

bool SdlRuntimePlatform::present(const Framebuffer& framebuffer) {
    void* pixels = 0;
    int pitch = 0;
    if (SDL_LockTexture(texture_, 0, &pixels, &pitch) != 0) {
        return false;
    }
    // ARGB8888 as a 32-bit word: bytes are B,G,R,A on LE and A,R,G,B on BE.
    // present_framebuffer only knows RGBA/BGRA byte orders, so on big endian
    // write BGRA then rotate each word in place.
    const std::size_t bytes =
        static_cast<std::size_t>(pitch) * framebuffer.height();
    bool ok = present_framebuffer(framebuffer, static_cast<std::uint8_t*>(pixels),
                                  bytes, static_cast<std::size_t>(pitch),
                                  SurfacePixelOrder::kBGRA8888);
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    if (ok) {
        for (int y = 0; y < framebuffer.height(); ++y) {
            Uint32* row = reinterpret_cast<Uint32*>(
                static_cast<std::uint8_t*>(pixels) + y * pitch);
            for (int x = 0; x < framebuffer.width(); ++x) {
                row[x] = SDL_Swap32(row[x]);  // B,G,R,A -> A,R,G,B
            }
        }
    }
#endif
    SDL_UnlockTexture(texture_);
    if (!ok) {
        return false;
    }
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, 0, 0);
    SDL_RenderPresent(renderer_);
    return true;
}

void SdlRuntimePlatform::audio_callback(void* user, std::uint8_t* stream,
                                        int bytes) {
    SdlRuntimePlatform* self = static_cast<SdlRuntimePlatform*>(user);
    const std::size_t frames = static_cast<std::size_t>(bytes) / 4u;
    if (!render_audio_callback(self->audio_manager_,
                               reinterpret_cast<std::int16_t*>(stream), frames,
                               1024u)) {
        SDL_memset(stream, 0, static_cast<std::size_t>(bytes));
    }
}

bool SdlRuntimePlatform::start_audio(AudioManager* manager) {
    if (manager == 0 || !manager->valid() || !sdl_initialized_) {
        return false;
    }
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = static_cast<int>(manager->output_rate());
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &SdlRuntimePlatform::audio_callback;
    want.userdata = this;
    audio_manager_ = manager;
    SDL_AudioSpec have;
    // allowed_changes = 0: SDL converts to the device format (F32MSB on PS3).
    audio_device_ = SDL_OpenAudioDevice(0, 0, &want, &have, 0);
    if (audio_device_ == 0) {
        std::printf("[pc2ps3] audio unavailable: %s\n", SDL_GetError());
        audio_manager_ = 0;
        return false;
    }
    SDL_PauseAudioDevice(audio_device_, 0);
    std::printf("[pc2ps3] audio %d Hz\n", have.freq);
    return true;
}

void SdlRuntimePlatform::lock_audio() {
    if (audio_device_ != 0) SDL_LockAudioDevice(audio_device_);
}

void SdlRuntimePlatform::unlock_audio() {
    if (audio_device_ != 0) SDL_UnlockAudioDevice(audio_device_);
}

void SdlRuntimePlatform::shutdown() {
    if (audio_device_ != 0) {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
        audio_manager_ = 0;
    }
    if (joystick_ != 0) {
        SDL_JoystickClose(joystick_);
        joystick_ = 0;
    }
    if (texture_ != 0) {
        SDL_DestroyTexture(texture_);
        texture_ = 0;
    }
    if (renderer_ != 0) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = 0;
    }
    if (window_ != 0) {
        SDL_DestroyWindow(window_);
        window_ = 0;
    }
    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }
}

} // namespace pc2ps3
