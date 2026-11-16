#include "pc2ps3/animation.hpp"

#include <cmath>
#include <limits>

namespace pc2ps3 {
namespace {

struct PlayerState {
  std::size_t index;
  double elapsed;
  bool finished;
  bool initial_event_pending;
};

void append_event(const AnimationFrame& frame, std::size_t frame_index,
                  AnimationEvent* output, std::size_t& count) {
  if (frame.event_id == 0) return;
  if (output) {
    output[count].event_id = frame.event_id;
    output[count].frame_index = frame_index;
  }
  ++count;
}

bool simulate_event_update(const AnimationClip& clip, double dt,
                           const PlayerState& current, AnimationEvent* output,
                           PlayerState& next, std::size_t& event_count) {
  event_count = 0;
  next = current;
  if (!std::isfinite(dt) || dt < 0.0 ||
      current.index >= clip.frame_count() || !std::isfinite(current.elapsed) ||
      current.elapsed < 0.0) {
    return false;
  }

  const double current_duration = clip.frame(current.index).duration_seconds;
  if ((!current.finished && current.elapsed >= current_duration) ||
      (current.finished && current.elapsed > current_duration)) {
    return false;
  }

  if (current.initial_event_pending) {
    append_event(clip.frame(current.index), current.index, output, event_count);
    next.initial_event_pending = false;
  }
  if (current.finished) return true;

  double remaining = dt;
  std::size_t transitions = 0;
  while (remaining >=
         clip.frame(next.index).duration_seconds - next.elapsed) {
    remaining -= clip.frame(next.index).duration_seconds - next.elapsed;
    ++transitions;
    if (transitions > AnimationPlayer::kMaxEventTransitionsPerUpdate) {
      return false;
    }

    if (next.index + 1 == clip.frame_count()) {
      if (!clip.looping()) {
        next.elapsed = clip.frame(next.index).duration_seconds;
        next.finished = true;
        remaining = 0.0;
        break;
      }
      next.index = 0;
    } else {
      ++next.index;
    }
    next.elapsed = 0.0;
    append_event(clip.frame(next.index), next.index, output, event_count);
  }

  if (!next.finished) next.elapsed += remaining;
  return true;
}

}  // namespace

AnimationClip::AnimationClip(const std::vector<AnimationFrame>& frames,
                             bool looping)
    : frames_(frames),
      looping_(looping),
      valid_(!frames.empty()),
      duration_seconds_(0.0) {
  for (std::size_t i = 0; i < frames_.size(); ++i) {
    const AnimationFrame& frame = frames_[i];
    if (frame.x < 0 || frame.y < 0 || frame.width <= 0 || frame.height <= 0 ||
        !std::isfinite(frame.duration_seconds) ||
        frame.duration_seconds <= 0.0 ||
        duration_seconds_ >
            std::numeric_limits<double>::max() - frame.duration_seconds) {
      valid_ = false;
      break;
    }
    duration_seconds_ += frame.duration_seconds;
  }
}

bool AnimationClip::valid() const { return valid_; }
bool AnimationClip::looping() const { return looping_; }
std::size_t AnimationClip::frame_count() const { return frames_.size(); }
const AnimationFrame& AnimationClip::frame(std::size_t index) const {
  return frames_[index];
}
double AnimationClip::duration_seconds() const { return duration_seconds_; }

AnimationPlayer::AnimationPlayer()
    : clip_(0),
      index_(0),
      elapsed_(0.0),
      finished_(false),
      initial_event_pending_(false) {}

bool AnimationPlayer::play(const AnimationClip& clip) {
  if (!clip.valid()) return false;
  clip_ = &clip;
  reset();
  return true;
}

void AnimationPlayer::reset() {
  index_ = 0;
  elapsed_ = 0.0;
  finished_ = false;
  initial_event_pending_ = clip_ != 0;
}

void AnimationPlayer::update(double dt) {
  if (!clip_ || finished_ || !std::isfinite(dt) || dt <= 0.0) return;

  if (clip_->looping()) {
    double position = elapsed_;
    for (std::size_t i = 0; i < index_; ++i) {
      position += clip_->frame(i).duration_seconds;
    }
    const double advance = std::fmod(dt, clip_->duration_seconds());
    const double until_wrap = clip_->duration_seconds() - position;
    if (advance >= until_wrap) {
      position = advance - until_wrap;
    } else {
      position += advance;
    }
    if (position >= clip_->duration_seconds()) {
      position = std::fmod(position, clip_->duration_seconds());
    }
    index_ = 0;
    while (index_ + 1 < clip_->frame_count() &&
           position >= clip_->frame(index_).duration_seconds) {
      position -= clip_->frame(index_).duration_seconds;
      ++index_;
    }
    elapsed_ = position;
    return;
  }

  double remaining = dt;
  while (remaining >=
         clip_->frame(index_).duration_seconds - elapsed_) {
    remaining -= clip_->frame(index_).duration_seconds - elapsed_;
    if (index_ + 1 == clip_->frame_count()) {
      elapsed_ = clip_->frame(index_).duration_seconds;
      finished_ = true;
      return;
    }
    ++index_;
    elapsed_ = 0.0;
  }
  elapsed_ += remaining;
}

bool AnimationPlayer::update_with_events(double dt, AnimationEvent* output,
                                         std::size_t output_capacity,
                                         std::size_t& output_count) {
  output_count = 0;
  if (!clip_ || (output_capacity > 0 && !output)) return false;

  const PlayerState current = {index_, elapsed_, finished_,
                               initial_event_pending_};
  PlayerState measured = current;
  std::size_t required = 0;
  if (!simulate_event_update(*clip_, dt, current, 0, measured, required) ||
      required > output_capacity) {
    return false;
  }

  PlayerState next = current;
  std::size_t emitted = 0;
  if (!simulate_event_update(*clip_, dt, current, output, next, emitted)) {
    return false;
  }
  index_ = next.index;
  elapsed_ = next.elapsed;
  finished_ = next.finished;
  initial_event_pending_ = next.initial_event_pending;
  output_count = emitted;
  return true;
}

const AnimationFrame* AnimationPlayer::current_frame() const {
  return clip_ ? &clip_->frame(index_) : 0;
}
std::size_t AnimationPlayer::current_index() const { return index_; }
double AnimationPlayer::elapsed_in_frame() const { return elapsed_; }
bool AnimationPlayer::finished() const { return finished_; }

}  // namespace pc2ps3
