#include "pc2ps3/audio_callback.hpp"

#include <algorithm>
#include <limits>

namespace pc2ps3 {
namespace {

void silence(std::int16_t* output, std::size_t sample_count) {
  std::fill(output, output + sample_count, static_cast<std::int16_t>(0));
}

}  // namespace

bool render_audio_callback(AudioManager* manager,
                           std::int16_t* stereo_output,
                           std::size_t frame_count,
                           std::size_t max_chunk_frames) noexcept {
  if (frame_count == 0) return true;
  if (stereo_output == 0 ||
      frame_count > std::numeric_limits<std::size_t>::max() / 2) {
    return false;
  }

  const std::size_t sample_count = frame_count * 2;
  if (manager == 0 || max_chunk_frames == 0) {
    silence(stereo_output, sample_count);
    return false;
  }

  std::size_t rendered_frames = 0;
  try {
    while (rendered_frames < frame_count) {
      const std::size_t remaining = frame_count - rendered_frames;
      const std::size_t chunk_frames =
          remaining < max_chunk_frames ? remaining : max_chunk_frames;
      if (!manager->render(stereo_output + rendered_frames * 2,
                           chunk_frames)) {
        silence(stereo_output, sample_count);
        return false;
      }
      rendered_frames += chunk_frames;
    }
  } catch (...) {
    silence(stereo_output, sample_count);
    return false;
  }

  return true;
}

}  // namespace pc2ps3
