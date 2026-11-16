#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

struct AnimationFrame {
  AnimationFrame()
      : x(0), y(0), width(0), height(0), duration_seconds(0.0), event_id(0) {}
  AnimationFrame(int frame_x, int frame_y, int frame_width, int frame_height,
                 double duration, std::uint32_t marker = 0)
      : x(frame_x), y(frame_y), width(frame_width), height(frame_height),
        duration_seconds(duration), event_id(marker) {}

  int x;
  int y;
  int width;
  int height;
  double duration_seconds;
  // Zero means that entering this frame emits no gameplay/effect event.
  std::uint32_t event_id;
};

struct AnimationEvent {
  std::uint32_t event_id;
  std::size_t frame_index;
};

class AnimationClip {
 public:
  AnimationClip(const std::vector<AnimationFrame>& frames, bool looping);

  bool valid() const;
  bool looping() const;
  std::size_t frame_count() const;
  const AnimationFrame& frame(std::size_t index) const;
  double duration_seconds() const;

 private:
  std::vector<AnimationFrame> frames_;
  bool looping_;
  bool valid_;
  double duration_seconds_;
};

class AnimationPlayer {
 public:
enum { kMaxEventTransitionsPerUpdate = 4096 };

  AnimationPlayer();

  // The clip is referenced, so it must outlive this player while selected.
  bool play(const AnimationClip& clip);
  void reset();
// Legacy playback keeps its timeline behavior but cannot report markers.
// Use update_with_events consistently for clips that contain event IDs.
  void update(double dt);
  // Emits crossed frame markers into caller storage. Fails atomically when
  // input, transition count, or output capacity is invalid.
  bool update_with_events(double dt, AnimationEvent* output,
                          std::size_t output_capacity,
                          std::size_t& output_count);
  const AnimationFrame* current_frame() const;
  std::size_t current_index() const;
  double elapsed_in_frame() const;
  bool finished() const;

 private:
  const AnimationClip* clip_;
  std::size_t index_;
  double elapsed_;
  bool finished_;
  bool initial_event_pending_;
};

}  // namespace pc2ps3
