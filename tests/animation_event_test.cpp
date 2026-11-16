#include "pc2ps3/animation.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

int main() {
  using pc2ps3::AnimationClip;
  using pc2ps3::AnimationEvent;
  using pc2ps3::AnimationFrame;
  using pc2ps3::AnimationPlayer;

  const AnimationFrame legacy_frame{0, 0, 16, 16, 0.25};
  assert(legacy_frame.event_id == 0);

  const std::vector<AnimationFrame> frames = {
      AnimationFrame{0, 0, 16, 16, 0.1, 11},
      AnimationFrame{16, 0, 16, 16, 0.2, 22},
      AnimationFrame{32, 0, 16, 16, 0.1}};
  const AnimationClip looping(frames, true);
  assert(looping.valid());

  AnimationPlayer eventful;
  AnimationPlayer legacy;
  assert(eventful.play(looping));
  assert(legacy.play(looping));

  AnimationEvent events[8] = {};
  std::size_t event_count = 99;
  assert(eventful.update_with_events(0.95, events, 8, event_count));
  assert(event_count == 6);
  assert(events[0].event_id == 11 && events[0].frame_index == 0);
  assert(events[1].event_id == 22 && events[1].frame_index == 1);
  assert(events[2].event_id == 11 && events[2].frame_index == 0);
  assert(events[3].event_id == 22 && events[3].frame_index == 1);
  assert(events[4].event_id == 11 && events[4].frame_index == 0);
  assert(events[5].event_id == 22 && events[5].frame_index == 1);

  legacy.update(0.95);
  assert(legacy.current_index() == eventful.current_index());
  assert(std::fabs(legacy.elapsed_in_frame() -
                   eventful.elapsed_in_frame()) < 1e-9);
  assert(legacy.finished() == eventful.finished());

  eventful.reset();
  event_count = 99;
  assert(eventful.update_with_events(0.0, events, 8, event_count));
  assert(event_count == 1 && events[0].event_id == 11 &&
         events[0].frame_index == 0);
  assert(eventful.current_index() == 0 &&
         eventful.elapsed_in_frame() == 0.0);

  AnimationPlayer capacity_player;
  assert(capacity_player.play(looping));
  AnimationEvent too_small[1] = {{77, 88}};
  event_count = 99;
  assert(!capacity_player.update_with_events(0.25, too_small, 1,
                                              event_count));
  assert(event_count == 0 && too_small[0].event_id == 77 &&
         too_small[0].frame_index == 88);
  assert(capacity_player.current_index() == 0 &&
         capacity_player.elapsed_in_frame() == 0.0);

  AnimationEvent enough[2] = {};
  assert(capacity_player.update_with_events(0.25, enough, 2, event_count));
  assert(event_count == 2 && enough[0].event_id == 11 &&
         enough[1].event_id == 22);

  const std::size_t saved_index = capacity_player.current_index();
  const double saved_elapsed = capacity_player.elapsed_in_frame();
  AnimationEvent unchanged[2] = {{91, 92}, {93, 94}};
  event_count = 99;
  assert(!capacity_player.update_with_events(
      std::numeric_limits<double>::quiet_NaN(), unchanged, 2, event_count));
  assert(event_count == 0 && unchanged[0].event_id == 91 &&
         unchanged[1].frame_index == 94);
  assert(capacity_player.current_index() == saved_index &&
         capacity_player.elapsed_in_frame() == saved_elapsed);

  const std::vector<AnimationFrame> markerless_frame = {
      AnimationFrame{0, 0, 16, 16, 1.0}};
  const AnimationClip long_loop(markerless_frame, true);
  AnimationPlayer bounded;
  assert(bounded.play(long_loop));
  AnimationEvent sentinel[1] = {{55, 66}};
  event_count = 99;
  assert(!bounded.update_with_events(
      static_cast<double>(AnimationPlayer::kMaxEventTransitionsPerUpdate) +
          1.0,
      sentinel, 1, event_count));
  assert(event_count == 0 && sentinel[0].event_id == 55 &&
         sentinel[0].frame_index == 66);
  assert(bounded.current_index() == 0 &&
         bounded.elapsed_in_frame() == 0.0);

  event_count = 99;
  assert(!bounded.update_with_events(0.0, 0, 1, event_count));
  assert(event_count == 0 && bounded.current_index() == 0);
  return 0;
}
