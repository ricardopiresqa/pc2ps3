#pragma once

#include "pc2ps3/audio.hpp"
#include "pc2ps3/runtime_loop.hpp"

#include <cstdint>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct _SDL_Joystick;

namespace pc2ps3 {

// SDL2 backend for RuntimeLoop. On PS3 it runs over SDL2_PSL1GHT (RSX video,
// cellPad joystick, cellAudio), which also turns the XMB "Quit Game" sysutil
// event into SDL_QUIT. The framebuffer is scaled to the output resolution.
class SdlRuntimePlatform : public RuntimePlatform {
public:
    SdlRuntimePlatform(int output_width, int output_height);
    ~SdlRuntimePlatform() noexcept override;

    bool initialize(int width, int height) override;
    RuntimePollStatus poll(InputSnapshot& snapshot,
                           std::uint64_t& elapsed_ns) override;
    bool present(const Framebuffer& framebuffer) override;
    void shutdown() override;

    // Opens the audio device and pulls PCM from manager on the SDL audio
    // thread. Callers must hold lock_audio() around AudioManager control calls.
    bool start_audio(AudioManager* manager);
    void lock_audio();
    void unlock_audio();
    bool audio_running() const { return audio_device_ != 0; }

    // Start + Select held together also quits (useful on emulators).
    double measured_fps() const { return fps_; }

private:
    static void audio_callback(void* user, std::uint8_t* stream, int bytes);

    int output_width_;
    int output_height_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    _SDL_Joystick* joystick_;
    std::uint32_t audio_device_;
    AudioManager* audio_manager_;
    std::uint64_t counter_frequency_;
    std::uint64_t previous_counter_;
    std::uint64_t fps_window_start_;
    std::uint32_t fps_frames_;
    double fps_;
    std::uint32_t last_mask_ = 0;
    int last_x_ = 0;
    int last_y_ = 0;
    bool sdl_initialized_;
};

} // namespace pc2ps3
