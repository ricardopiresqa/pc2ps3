#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace pc2ps3 {

class PcmClip {
 public:
  PcmClip();
  PcmClip(std::uint32_t sample_rate, std::uint16_t channels,
          std::vector<std::int16_t> samples);

  bool valid() const;
  std::uint32_t sample_rate() const;
  std::uint16_t channels() const;
  std::size_t frame_count() const;
  const std::vector<std::int16_t>& samples() const;

 private:
  std::uint32_t sample_rate_;
  std::uint16_t channels_;
  std::vector<std::int16_t> samples_;
};

enum class WavDecodeStatus {
  kOk,
  kInvalidArgument,
  kMalformed,
  kUnsupportedFormat,
  kTooLarge
};

// Accepts bounded RIFF/WAVE PCM integer data: 8/16-bit, mono/stereo.
WavDecodeStatus decode_pcm_wav(const std::vector<std::uint8_t>& wav,
                               std::size_t max_frames, PcmClip& output);

enum class AudioCategory {
  kMusic = 0,
  kEffects = 1,
  kUi = 2,
  kVoice = 3,
  kCount = 4
};

enum class AudioStatus {
  kOk,
  kInvalidArgument,
  kNoVoice,
  kNoStream
};

struct AudioVoiceHandle {
  std::uint32_t value;
  AudioVoiceHandle() : value(0) {}
  explicit AudioVoiceHandle(std::uint32_t id) : value(id) {}
  bool valid() const { return value != 0; }
};

// Supplies PCM16 interleaved stereo at the manager's output rate.
// Implementations must be non-blocking, allocate nothing in read_frames(),
// and write at most the requested number of frames.
class PcmStream {
 public:
  virtual ~PcmStream() {}
  virtual std::size_t read_frames(std::int16_t* stereo_output,
                                  std::size_t max_frames) = 0;
  virtual bool finished() const = 0;
  virtual bool rewind() = 0;
};

// Single-threaded. A platform callback and control calls must be serialized.
// render() writes interleaved stereo PCM16 and performs no dynamic allocation.
class AudioManager {
 public:
  AudioManager(std::uint32_t output_rate, std::size_t max_effect_voices);
  AudioManager(const AudioManager&) = delete;
  AudioManager& operator=(const AudioManager&) = delete;
  AudioManager(AudioManager&&) = delete;
  AudioManager& operator=(AudioManager&&) = delete;

  bool valid() const;
  AudioStatus play_effect(const std::shared_ptr<const PcmClip>& clip,
                          AudioCategory category, float volume, bool looping,
                          AudioVoiceHandle& handle);
  bool stop(AudioVoiceHandle handle);
  void stop_category(AudioCategory category);

  AudioStatus play_music(const std::shared_ptr<PcmStream>& stream,
                        bool looping);
  void stop_music();

  bool set_master_volume(float volume);
  bool set_category_volume(AudioCategory category, float volume);
  bool render(std::int16_t* stereo_output, std::size_t frame_count);

  std::size_t active_voice_count() const;
  std::uint32_t output_rate() const;

 private:
  struct Voice {
    Voice();
    bool active;
    AudioVoiceHandle handle;
    std::shared_ptr<const PcmClip> clip;
    AudioCategory category;
    double source_position;
    float volume;
    bool looping;
  };

  bool valid_category(AudioCategory category) const;
  std::uint32_t allocate_handle();
  void stop_voice(Voice& voice);
  void retire_voice(std::size_t index);

  std::uint32_t output_rate_;
  std::vector<Voice> voices_;
  std::vector<std::shared_ptr<const PcmClip> > retired_clips_;
  std::array<float, 4> category_volumes_;
  std::shared_ptr<PcmStream> music_;
  std::shared_ptr<PcmStream> retired_music_;
  bool music_looping_;
  float master_volume_;
  std::uint32_t next_handle_;
  bool valid_;
};

}  // namespace pc2ps3
