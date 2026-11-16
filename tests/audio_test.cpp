#include "pc2ps3/audio.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace {

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

void append_fourcc(std::vector<std::uint8_t>& bytes, const char* text) {
  for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(text[i]));
}

std::vector<std::uint8_t> make_wav(std::uint16_t format,
                                   std::uint16_t channels,
                                   std::uint32_t sample_rate,
                                   std::uint16_t bits_per_sample,
                                   const std::vector<std::uint8_t>& data,
                                   bool odd_unknown_chunk) {
  const std::uint16_t block_align =
      static_cast<std::uint16_t>(channels * (bits_per_sample / 8));
  std::vector<std::uint8_t> body;
  append_fourcc(body, "WAVE");
  append_fourcc(body, "fmt ");
  append_u32(body, 16);
  append_u16(body, format);
  append_u16(body, channels);
  append_u32(body, sample_rate);
  append_u32(body, sample_rate * block_align);
  append_u16(body, block_align);
  append_u16(body, bits_per_sample);
  if (odd_unknown_chunk) {
    append_fourcc(body, "JUNK");
    append_u32(body, 1);
    body.push_back(0x7fu);
    body.push_back(0);
  }
  append_fourcc(body, "data");
  append_u32(body, static_cast<std::uint32_t>(data.size()));
  body.insert(body.end(), data.begin(), data.end());
  if ((data.size() & 1u) != 0u) body.push_back(0);

  std::vector<std::uint8_t> wav;
  append_fourcc(wav, "RIFF");
  append_u32(wav, static_cast<std::uint32_t>(body.size()));
  wav.insert(wav.end(), body.begin(), body.end());
  return wav;
}

class MemoryPcmStream : public pc2ps3::PcmStream {
 public:
  explicit MemoryPcmStream(const std::vector<std::int16_t>& stereo,
                           std::size_t max_chunk = 1000)
      : samples(stereo), cursor(0), max_chunk_frames(max_chunk), rewind_count(0) {}

  std::size_t read_frames(std::int16_t* output,
                          std::size_t max_frames) override {
    const std::size_t total_frames = samples.size() / 2;
    const std::size_t available = total_frames - cursor;
    const std::size_t count =
        std::min(max_frames, std::min(available, max_chunk_frames));
    for (std::size_t i = 0; i < count * 2; ++i) {
      output[i] = samples[cursor * 2 + i];
    }
    cursor += count;
    return count;
  }

  bool finished() const override { return cursor >= samples.size() / 2; }

  bool rewind() override {
    ++rewind_count;
    cursor = 0;
    return true;
  }

  std::vector<std::int16_t> samples;
  std::size_t cursor;
  std::size_t max_chunk_frames;
  std::size_t rewind_count;
};

}  // namespace

int main() {
  pc2ps3::PcmClip invalid_clip;
  assert(!invalid_clip.valid());
  pc2ps3::PcmClip bad_rate(7999, 1, std::vector<std::int16_t>(1, 0));
  pc2ps3::PcmClip bad_channels(44100, 3, std::vector<std::int16_t>(3, 0));
  pc2ps3::PcmClip incomplete_frame(44100, 2, std::vector<std::int16_t>(1, 0));
  assert(!bad_rate.valid() && !bad_channels.valid() && !incomplete_frame.valid());

  const std::vector<std::uint8_t> pcm8 = {0, 128, 255, 64};
  const std::vector<std::uint8_t> wav8 =
      make_wav(1, 1, 22050, 8, pcm8, true);
  pc2ps3::PcmClip decoded;
  assert(pc2ps3::decode_pcm_wav(wav8, 4, decoded) ==
         pc2ps3::WavDecodeStatus::kOk);
  assert(decoded.valid() && decoded.sample_rate() == 22050 &&
         decoded.channels() == 1 && decoded.frame_count() == 4);
  assert(decoded.samples()[0] == -32768);
  assert(decoded.samples()[1] == 0);
  assert(decoded.samples()[2] == 32512);
  assert(decoded.samples()[3] == -16384);

  std::vector<std::uint8_t> pcm16;
  append_u16(pcm16, 0x8000u);
  append_u16(pcm16, 0x7fffu);
  append_u16(pcm16, 0xffffu);
  append_u16(pcm16, 0);
  const std::vector<std::uint8_t> wav16 =
      make_wav(1, 2, 44100, 16, pcm16, false);
  assert(pc2ps3::decode_pcm_wav(wav16, 2, decoded) ==
         pc2ps3::WavDecodeStatus::kOk);
  assert(decoded.channels() == 2 && decoded.frame_count() == 2);
  assert(decoded.samples()[0] == -32768 && decoded.samples()[1] == 32767);
  assert(decoded.samples()[2] == -1 && decoded.samples()[3] == 0);

  const pc2ps3::PcmClip old_clip = decoded;
  assert(pc2ps3::decode_pcm_wav(wav8, 3, decoded) ==
         pc2ps3::WavDecodeStatus::kTooLarge);
  assert(decoded.samples() == old_clip.samples());
  const std::vector<std::uint8_t> oversized_wav(
      32u * 1024u * 1024u + 1u, 0);
  assert(pc2ps3::decode_pcm_wav(oversized_wav, 100, decoded) ==
         pc2ps3::WavDecodeStatus::kTooLarge);
  assert(decoded.samples() == old_clip.samples());

  std::vector<std::uint8_t> malformed = wav8;
  malformed[0] = 'X';
  assert(pc2ps3::decode_pcm_wav(malformed, 10, decoded) ==
         pc2ps3::WavDecodeStatus::kMalformed);
  assert(decoded.samples() == old_clip.samples());
  malformed = wav8;
  malformed[20] = 3;  // compressed/non-PCM format code
  assert(pc2ps3::decode_pcm_wav(malformed, 10, decoded) ==
         pc2ps3::WavDecodeStatus::kUnsupportedFormat);
  malformed = wav8;
  malformed.pop_back();
  assert(pc2ps3::decode_pcm_wav(malformed, 10, decoded) ==
         pc2ps3::WavDecodeStatus::kMalformed);

  pc2ps3::AudioManager manager(44100, 2);
  assert(manager.valid() && manager.output_rate() == 44100);
  pc2ps3::AudioManager invalid_manager(7000, 2);
  assert(!invalid_manager.valid());
  assert(!manager.set_master_volume(std::numeric_limits<float>::quiet_NaN()));
  assert(!manager.set_category_volume(pc2ps3::AudioCategory::kEffects, 1.1f));

  std::shared_ptr<const pc2ps3::PcmClip> resampled_clip(
      new pc2ps3::PcmClip(22050, 1,
                          std::vector<std::int16_t>{10000, -10000}));
  pc2ps3::AudioVoiceHandle first_voice;
  assert(manager.play_effect(resampled_clip, pc2ps3::AudioCategory::kEffects,
                             1.0f, false, first_voice) ==
         pc2ps3::AudioStatus::kOk);
  assert(first_voice.valid() && manager.active_voice_count() == 1);
  std::int16_t output[8] = {};
  assert(manager.render(output, 4));
  assert(output[0] == 10000 && output[1] == 10000);
  assert(output[2] == 0 && output[3] == 0);
  assert(output[4] == -10000 && output[5] == -10000);
  assert(output[6] == -10000 && output[7] == -10000);
  assert(manager.active_voice_count() == 0);

  std::shared_ptr<const pc2ps3::PcmClip> loud_clip(
      new pc2ps3::PcmClip(44100, 1,
                          std::vector<std::int16_t>{20000, 20000, 20000}));
  pc2ps3::AudioVoiceHandle second_voice;
  pc2ps3::AudioVoiceHandle third_voice;
  assert(manager.play_effect(loud_clip, pc2ps3::AudioCategory::kEffects,
                             1.0f, true, second_voice) ==
         pc2ps3::AudioStatus::kOk);
  assert(manager.play_effect(loud_clip, pc2ps3::AudioCategory::kUi,
                             1.0f, true, third_voice) ==
         pc2ps3::AudioStatus::kOk);
  pc2ps3::AudioVoiceHandle no_voice;
  assert(manager.play_effect(loud_clip, pc2ps3::AudioCategory::kEffects,
                             1.0f, false, no_voice) ==
         pc2ps3::AudioStatus::kNoVoice);
  assert(!no_voice.valid());
  assert(manager.set_category_volume(pc2ps3::AudioCategory::kUi, 0.5f));
  assert(manager.render(output, 1));
  assert(output[0] == 30000 && output[1] == 30000);
  assert(manager.stop(second_voice));
  assert(!manager.stop(second_voice));
  manager.stop_category(pc2ps3::AudioCategory::kUi);
  assert(manager.active_voice_count() == 0);

  std::shared_ptr<MemoryPcmStream> stream(new MemoryPcmStream(
      std::vector<std::int16_t>{1000, -1000, 2000, -2000}, 1));
  assert(manager.set_category_volume(pc2ps3::AudioCategory::kMusic, 0.5f));
  assert(manager.play_music(stream, true) == pc2ps3::AudioStatus::kOk);
  assert(manager.render(output, 3));
  assert(output[0] == 500 && output[1] == -500);
  assert(output[2] == 1000 && output[3] == -1000);
  assert(output[4] == 500 && output[5] == -500);
  assert(stream->rewind_count == 1);
  manager.stop_music();
  assert(manager.render(output, 1) && output[0] == 0 && output[1] == 0);

  std::shared_ptr<MemoryPcmStream> empty_stream(
      new MemoryPcmStream(std::vector<std::int16_t>()));
  assert(manager.play_music(empty_stream, true) == pc2ps3::AudioStatus::kOk);
  assert(manager.render(output, 2));
  assert(empty_stream->rewind_count == 1);
  assert(output[0] == 0 && output[1] == 0 && output[2] == 0 &&
         output[3] == 0);
  assert(manager.render(0, 0));
  assert(!manager.render(0, 1));
  return 0;
}
