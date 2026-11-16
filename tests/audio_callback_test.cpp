#include "pc2ps3/audio_callback.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace {

class ConstantStream : public pc2ps3::PcmStream {
 public:
  ConstantStream() : calls(0), largest_request(0), should_throw(false) {}

  std::size_t read_frames(std::int16_t* output,
                          std::size_t max_frames) override {
    if (should_throw) throw 1;
    ++calls;
    if (max_frames > largest_request) largest_request = max_frames;
    for (std::size_t i = 0; i < max_frames; ++i) {
      output[i * 2] = 1000;
      output[i * 2 + 1] = -1000;
    }
    return max_frames;
  }

  bool finished() const override { return false; }
  bool rewind() override { return true; }

  std::size_t calls;
  std::size_t largest_request;
  bool should_throw;
};

}  // namespace

int main() {
  std::int16_t silence[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  assert(pc2ps3::render_audio_callback(0, 0, 0, 0));
  assert(!pc2ps3::render_audio_callback(0, silence, 4, 2));
  for (std::size_t i = 0; i < 8; ++i) assert(silence[i] == 0);

  pc2ps3::AudioManager manager(48000, 1);
  std::shared_ptr<ConstantStream> stream(new ConstantStream());
  assert(manager.play_music(stream, false) == pc2ps3::AudioStatus::kOk);

  std::int16_t output[10] = {0};
  assert(pc2ps3::render_audio_callback(&manager, output, 5, 2));
  assert(stream->calls == 3);
  assert(stream->largest_request == 2);
  for (std::size_t i = 0; i < 5; ++i) {
    assert(output[i * 2] == 1000);
    assert(output[i * 2 + 1] == -1000);
  }

  manager.stop_music();
  std::shared_ptr<ConstantStream> throwing(new ConstantStream());
  throwing->should_throw = true;
  assert(manager.play_music(throwing, false) == pc2ps3::AudioStatus::kOk);
  for (std::size_t i = 0; i < 10; ++i) output[i] = 7;
  assert(!pc2ps3::render_audio_callback(&manager, output, 5, 2));
  for (std::size_t i = 0; i < 10; ++i) assert(output[i] == 0);

  for (std::size_t i = 0; i < 8; ++i) silence[i] = 9;
  assert(!pc2ps3::render_audio_callback(&manager, silence, 4, 0));
  for (std::size_t i = 0; i < 8; ++i) assert(silence[i] == 0);

  pc2ps3::AudioManager invalid_manager(0, 1);
  std::int16_t rejected_output[8] = {7, 7, 7, 7, 7, 7, 7, 7};
  assert(!pc2ps3::render_audio_callback(&invalid_manager, rejected_output, 4,
                                       2));
  for (std::size_t i = 0; i < 8; ++i) assert(rejected_output[i] == 0);

  std::int16_t overflow_output[2] = {5, 5};
  const std::size_t overflow_frames =
      std::numeric_limits<std::size_t>::max() / 2 + 1;
  assert(!pc2ps3::render_audio_callback(&manager, overflow_output,
                                        overflow_frames, 1));
  assert(overflow_output[0] == 5 && overflow_output[1] == 5);
  return 0;
}
