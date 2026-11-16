#pragma once

#include "pc2ps3/audio.hpp"
#include <cstdint>

namespace pc2ps3 {

struct Win32WaveOutFunctions;
struct Win32AudioOutputState;

// WinMM PCM output. The manager and injected function table are borrowed and
// must outlive the output. All manager operations, including pump(), must be
// serialized by the caller on the initializing thread.
class Win32AudioOutput {
 public:
  explicit Win32AudioOutput(const Win32WaveOutFunctions* functions = 0);
  ~Win32AudioOutput() noexcept;

  Win32AudioOutput(const Win32AudioOutput&) = delete;
  Win32AudioOutput& operator=(const Win32AudioOutput&) = delete;

  bool initialize(AudioManager* manager, std::uint32_t block_frames = 512,
                  std::uint32_t queue_depth = 3);
  bool pump();
  bool shutdown() noexcept;

  bool active() const noexcept;
  bool last_render_succeeded() const noexcept;
  std::uint32_t last_error() const noexcept;

 private:
  const Win32WaveOutFunctions* functions_;
  Win32AudioOutputState* state_;
  std::uint32_t last_error_;
  bool last_render_succeeded_;
};

}  // namespace pc2ps3
