#include "pc2ps3/audio.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace pc2ps3 {
namespace {

const std::uint32_t kMinimumSampleRate = 8000;
const std::uint32_t kMaximumSampleRate = 192000;
const std::size_t kMaximumVoices = 64;
const std::size_t kMaximumClipBytes = 16u * 1024u * 1024u;
const std::size_t kMaximumWavBytes = 32u * 1024u * 1024u;

bool fourcc(const std::vector<std::uint8_t>& bytes, std::size_t offset,
            const char (&expected)[5]) {
  return bytes[offset] == static_cast<std::uint8_t>(expected[0]) &&
         bytes[offset + 1] == static_cast<std::uint8_t>(expected[1]) &&
         bytes[offset + 2] == static_cast<std::uint8_t>(expected[2]) &&
         bytes[offset + 3] == static_cast<std::uint8_t>(expected[3]);
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes,
                          std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(
             static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes,
                          std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool finite_volume(float volume) {
  return std::isfinite(volume) && volume >= 0.0f && volume <= 1.0f;
}

std::int16_t clamp_sample(std::int32_t sample) {
  if (sample > 32767) sample = 32767;
  if (sample < -32768) sample = -32768;
  return static_cast<std::int16_t>(sample);
}

}  // namespace

PcmClip::PcmClip() : sample_rate_(0), channels_(0) {}

PcmClip::PcmClip(std::uint32_t sample_rate, std::uint16_t channels,
                 std::vector<std::int16_t> samples)
    : sample_rate_(sample_rate), channels_(channels),
      samples_(std::move(samples)) {}

bool PcmClip::valid() const {
  return sample_rate_ >= kMinimumSampleRate &&
         sample_rate_ <= kMaximumSampleRate && (channels_ == 1 || channels_ == 2) &&
         !samples_.empty() && samples_.size() % channels_ == 0 &&
         samples_.size() <= kMaximumClipBytes / sizeof(std::int16_t);
}

std::uint32_t PcmClip::sample_rate() const { return sample_rate_; }

std::uint16_t PcmClip::channels() const { return channels_; }

std::size_t PcmClip::frame_count() const {
  return channels_ == 0 ? 0 : samples_.size() / channels_;
}

const std::vector<std::int16_t>& PcmClip::samples() const { return samples_; }

WavDecodeStatus decode_pcm_wav(const std::vector<std::uint8_t>& wav,
                               std::size_t max_frames, PcmClip& output) {
  if (wav.size() > kMaximumWavBytes) return WavDecodeStatus::kTooLarge;
  if (wav.size() < 12) return WavDecodeStatus::kMalformed;
  if (!fourcc(wav, 0, "RIFF") || !fourcc(wav, 8, "WAVE")) {
    return WavDecodeStatus::kMalformed;
  }

  const std::uint32_t riff_size = read_u32_le(wav, 4);
  if (riff_size < 4 ||
      static_cast<std::uint64_t>(riff_size) >
          static_cast<std::uint64_t>(wav.size() - 8)) {
    return WavDecodeStatus::kMalformed;
  }
  if (static_cast<std::size_t>(riff_size) != wav.size() - 8) {
    return WavDecodeStatus::kMalformed;
  }
  const std::size_t riff_end = 8 + static_cast<std::size_t>(riff_size);

  bool found_format = false;
  bool found_data = false;
  std::uint16_t format_code = 0;
  std::uint16_t channels = 0;
  std::uint16_t bits_per_sample = 0;
  std::uint16_t block_align = 0;
  std::uint32_t sample_rate = 0;
  std::uint32_t byte_rate = 0;
  std::size_t data_offset = 0;
  std::size_t data_size = 0;

  std::size_t offset = 12;
  while (offset < riff_end) {
    if (riff_end - offset < 8) return WavDecodeStatus::kMalformed;
    const std::uint32_t chunk_size_u32 = read_u32_le(wav, offset + 4);
    const std::size_t chunk_size = static_cast<std::size_t>(chunk_size_u32);
    const std::size_t chunk_data = offset + 8;
    if (chunk_size > riff_end - chunk_data) {
      return WavDecodeStatus::kMalformed;
    }

    if (fourcc(wav, offset, "fmt ")) {
      if (found_format || chunk_size < 16) return WavDecodeStatus::kMalformed;
      format_code = read_u16_le(wav, chunk_data);
      channels = read_u16_le(wav, chunk_data + 2);
      sample_rate = read_u32_le(wav, chunk_data + 4);
      byte_rate = read_u32_le(wav, chunk_data + 8);
      block_align = read_u16_le(wav, chunk_data + 12);
      bits_per_sample = read_u16_le(wav, chunk_data + 14);
      found_format = true;
    } else if (fourcc(wav, offset, "data")) {
      if (found_data) return WavDecodeStatus::kMalformed;
      data_offset = chunk_data;
      data_size = chunk_size;
      found_data = true;
    }

    offset = chunk_data + chunk_size;
    if ((chunk_size & 1u) != 0u) {
      if (offset >= riff_end) return WavDecodeStatus::kMalformed;
      ++offset;
    }
  }
  if (offset != riff_end || !found_format || !found_data) {
    return WavDecodeStatus::kMalformed;
  }

  if (format_code != 1 || (bits_per_sample != 8 && bits_per_sample != 16) ||
      (channels != 1 && channels != 2)) {
    return WavDecodeStatus::kUnsupportedFormat;
  }
  if (sample_rate < kMinimumSampleRate || sample_rate > kMaximumSampleRate) {
    return WavDecodeStatus::kUnsupportedFormat;
  }

  const std::uint16_t bytes_per_sample = bits_per_sample / 8;
  const std::uint16_t expected_align = channels * bytes_per_sample;
  if (block_align != expected_align ||
      byte_rate != sample_rate * static_cast<std::uint32_t>(block_align) ||
      block_align == 0 || data_size % block_align != 0) {
    return WavDecodeStatus::kMalformed;
  }

  const std::size_t frames = data_size / block_align;
  if (frames == 0) return WavDecodeStatus::kMalformed;
  if (frames > max_frames) return WavDecodeStatus::kTooLarge;
  if (frames > std::numeric_limits<std::size_t>::max() / channels) {
    return WavDecodeStatus::kTooLarge;
  }
  if (frames * channels > kMaximumClipBytes / sizeof(std::int16_t)) {
    return WavDecodeStatus::kTooLarge;
  }

  std::vector<std::int16_t> samples;
  samples.reserve(frames * channels);
  for (std::size_t i = 0; i < frames * channels; ++i) {
    if (bits_per_sample == 8) {
      const std::int32_t unsigned_sample = wav[data_offset + i];
      samples.push_back(static_cast<std::int16_t>(
          (unsigned_sample - 128) * 256));
    } else {
      const std::uint16_t raw = read_u16_le(wav, data_offset + i * 2);
      const std::int32_t signed_sample =
          raw >= 0x8000u ? static_cast<std::int32_t>(raw) - 0x10000
                         : static_cast<std::int32_t>(raw);
      samples.push_back(static_cast<std::int16_t>(signed_sample));
    }
  }

  PcmClip decoded(sample_rate, channels, std::move(samples));
  if (!decoded.valid()) return WavDecodeStatus::kMalformed;
  output = std::move(decoded);
  return WavDecodeStatus::kOk;
}

AudioManager::Voice::Voice()
    : active(false), category(AudioCategory::kEffects), source_position(0.0),
      volume(1.0f), looping(false) {}

AudioManager::AudioManager(std::uint32_t output_rate,
                           std::size_t max_effect_voices)
    : output_rate_(output_rate),
      music_looping_(false),
      master_volume_(1.0f),
      next_handle_(1),
      valid_(output_rate_ >= kMinimumSampleRate &&
             output_rate_ <= kMaximumSampleRate && max_effect_voices > 0 &&
             max_effect_voices <= kMaximumVoices) {
  category_volumes_.fill(1.0f);
  if (valid_) {
    voices_.resize(max_effect_voices);
    retired_clips_.resize(max_effect_voices);
  }
}

bool AudioManager::valid() const { return valid_; }

AudioStatus AudioManager::play_effect(
    const std::shared_ptr<const PcmClip>& clip, AudioCategory category,
    float volume, bool looping, AudioVoiceHandle& handle) {
  handle = AudioVoiceHandle();
  if (!valid_ || !clip || !clip->valid() || !valid_category(category) ||
      category == AudioCategory::kMusic || !finite_volume(volume)) {
    return AudioStatus::kInvalidArgument;
  }

  for (std::size_t i = 0; i < voices_.size(); ++i) {
    if (voices_[i].active) continue;
    const std::uint32_t id = allocate_handle();
    if (id == 0) return AudioStatus::kNoVoice;
    retired_clips_[i].reset();
    voices_[i].active = true;
    voices_[i].handle = AudioVoiceHandle(id);
    voices_[i].clip = clip;
    voices_[i].category = category;
    voices_[i].source_position = 0.0;
    voices_[i].volume = volume;
    voices_[i].looping = looping;
    handle = voices_[i].handle;
    return AudioStatus::kOk;
  }
  return AudioStatus::kNoVoice;
}

bool AudioManager::stop(AudioVoiceHandle handle) {
  if (!handle.valid()) return false;
  for (std::size_t i = 0; i < voices_.size(); ++i) {
    if (voices_[i].active && voices_[i].handle.value == handle.value) {
      stop_voice(voices_[i]);
      return true;
    }
  }
  return false;
}

void AudioManager::stop_category(AudioCategory category) {
  if (!valid_category(category)) return;
  if (category == AudioCategory::kMusic) {
    stop_music();
    return;
  }
  for (std::size_t i = 0; i < voices_.size(); ++i) {
    if (voices_[i].active && voices_[i].category == category) {
      stop_voice(voices_[i]);
    }
  }
}

AudioStatus AudioManager::play_music(
    const std::shared_ptr<PcmStream>& stream, bool looping) {
  if (!valid_ || !stream) return AudioStatus::kInvalidArgument;
  retired_music_.reset();
  music_ = stream;
  music_looping_ = looping;
  return AudioStatus::kOk;
}

void AudioManager::stop_music() {
  music_.reset();
  retired_music_.reset();
  music_looping_ = false;
}

bool AudioManager::set_master_volume(float volume) {
  if (!valid_ || !finite_volume(volume)) return false;
  master_volume_ = volume;
  return true;
}

bool AudioManager::set_category_volume(AudioCategory category, float volume) {
  if (!valid_ || !valid_category(category) || !finite_volume(volume)) return false;
  category_volumes_[static_cast<std::size_t>(category)] = volume;
  return true;
}

bool AudioManager::render(std::int16_t* stereo_output,
                          std::size_t frame_count) {
  if (!valid_ || (frame_count != 0 && stereo_output == 0) ||
      frame_count > std::numeric_limits<std::size_t>::max() / 2) {
    return false;
  }
  if (frame_count == 0) return true;

  const std::size_t sample_count = frame_count * 2;
  std::fill(stereo_output, stereo_output + sample_count, 0);
  std::size_t music_frames = 0;
  std::size_t empty_rewinds = 0;
  while (music_ && music_frames < frame_count) {
    const std::size_t remaining = frame_count - music_frames;
    const std::size_t produced =
        music_->read_frames(stereo_output + music_frames * 2, remaining);
    if (produced > remaining) {
      retired_music_ = std::move(music_);
      music_looping_ = false;
      break;
    }
    music_frames += produced;

    if (produced == 0) {
      if (!music_->finished()) break;
      if (music_looping_ && empty_rewinds == 0 && music_->rewind()) {
        ++empty_rewinds;
        continue;
      }
      retired_music_ = std::move(music_);
      music_looping_ = false;
      break;
    }

    if (music_->finished()) {
      if (music_looping_ && music_->rewind()) {
        empty_rewinds = 0;
        continue;
      }
      retired_music_ = std::move(music_);
      music_looping_ = false;
    }
  }

  const float music_gain =
      master_volume_ * category_volumes_[static_cast<std::size_t>(
                           AudioCategory::kMusic)];
  for (std::size_t output_frame = 0; output_frame < frame_count;
       ++output_frame) {
    std::int32_t mixed_left = 0;
    std::int32_t mixed_right = 0;
    if (output_frame < music_frames) {
      mixed_left = static_cast<std::int32_t>(
          stereo_output[output_frame * 2] * music_gain);
      mixed_right = static_cast<std::int32_t>(
          stereo_output[output_frame * 2 + 1] * music_gain);
    }

    for (std::size_t i = 0; i < voices_.size(); ++i) {
      Voice& voice = voices_[i];
      if (!voice.active) continue;

      const PcmClip& clip = *voice.clip;
      const std::size_t source_frames = clip.frame_count();
      if (voice.source_position >= static_cast<double>(source_frames)) {
        if (!voice.looping) {
          retire_voice(i);
          continue;
        }
        voice.source_position =
            std::fmod(voice.source_position, static_cast<double>(source_frames));
      }

      const std::size_t first =
          static_cast<std::size_t>(voice.source_position);
      std::size_t second = first + 1;
      if (second >= source_frames) second = voice.looping ? 0 : first;
      const double fraction = voice.source_position - first;
      const float gain = master_volume_ * voice.volume *
                         category_volumes_[static_cast<std::size_t>(voice.category)];
      const std::vector<std::int16_t>& samples = clip.samples();
      const std::size_t channels = clip.channels();
      const std::int16_t first_left = samples[first * channels];
      const std::int16_t second_left = samples[second * channels];
      const std::int16_t first_right =
          channels == 1 ? first_left : samples[first * channels + 1];
      const std::int16_t second_right =
          channels == 1 ? second_left : samples[second * channels + 1];
      const double left = first_left + (second_left - first_left) * fraction;
      const double right = first_right + (second_right - first_right) * fraction;
      mixed_left += static_cast<std::int32_t>(left * gain);
      mixed_right += static_cast<std::int32_t>(right * gain);

      voice.source_position +=
          static_cast<double>(clip.sample_rate()) / output_rate_;
      if (!voice.looping &&
          voice.source_position >= static_cast<double>(source_frames)) {
        retire_voice(i);
      }
    }

    stereo_output[output_frame * 2] = clamp_sample(mixed_left);
    stereo_output[output_frame * 2 + 1] = clamp_sample(mixed_right);
  }
  return true;
}

std::size_t AudioManager::active_voice_count() const {
  std::size_t count = 0;
  for (std::size_t i = 0; i < voices_.size(); ++i) {
    if (voices_[i].active) ++count;
  }
  return count;
}

std::uint32_t AudioManager::output_rate() const { return output_rate_; }

bool AudioManager::valid_category(AudioCategory category) const {
  const int value = static_cast<int>(category);
  return value >= 0 && value < static_cast<int>(AudioCategory::kCount);
}

std::uint32_t AudioManager::allocate_handle() {
  for (std::size_t attempt = 0; attempt <= voices_.size(); ++attempt) {
    if (next_handle_ == 0) next_handle_ = 1;
    const std::uint32_t candidate = next_handle_++;
    bool used = false;
    for (std::size_t i = 0; i < voices_.size(); ++i) {
      if (voices_[i].active && voices_[i].handle.value == candidate) {
        used = true;
        break;
      }
    }
    if (!used) return candidate;
  }
  return 0;
}

void AudioManager::stop_voice(Voice& voice) {
  voice.active = false;
  voice.handle = AudioVoiceHandle();
  voice.clip.reset();
  voice.source_position = 0.0;
  voice.looping = false;
}

void AudioManager::retire_voice(std::size_t index) {
  Voice& voice = voices_[index];
  voice.active = false;
  voice.handle = AudioVoiceHandle();
  retired_clips_[index] = std::move(voice.clip);
  voice.source_position = 0.0;
  voice.looping = false;
}

}  // namespace pc2ps3
