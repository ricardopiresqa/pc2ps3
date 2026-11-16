#pragma once

#include "pc2ps3/audio.hpp"
#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

// Render interleaved stereo PCM16 into a device-owned buffer. Each call into
// AudioManager is capped at max_chunk_frames. The caller serializes this with
// AudioManager control calls, as required by AudioManager's contract.
// Returns true only if all requested frames were rendered.
bool render_audio_callback(AudioManager* manager,
                           std::int16_t* stereo_output,
                           std::size_t frame_count,
                           std::size_t max_chunk_frames) noexcept;

}  // namespace pc2ps3
