#include "pc2ps3/boss_pattern.hpp"

#include <cassert>
#include <cstddef>

namespace {

void test_phase_step_and_offset_limits() {
 const pc2ps3::BossActionStep single_step[] = {{0, 1}};
 pc2ps3::BossPhaseSequence max_phases[pc2ps3::BossPatternTimeline::kMaxPhases];
 for (std::size_t i = 0; i < pc2ps3::BossPatternTimeline::kMaxPhases; ++i) {
  max_phases[i].phase_id = static_cast<std::uint32_t>(i);
  max_phases[i].steps = single_step;
  max_phases[i].step_count = 1;
 }
 pc2ps3::BossPatternTimeline at_phase_limit(
     max_phases, pc2ps3::BossPatternTimeline::kMaxPhases);
 assert(at_phase_limit.valid());

 pc2ps3::BossPhaseSequence too_many_phases[
     pc2ps3::BossPatternTimeline::kMaxPhases + 1];
 for (std::size_t i = 0;
      i < pc2ps3::BossPatternTimeline::kMaxPhases + 1; ++i) {
  too_many_phases[i].phase_id = static_cast<std::uint32_t>(i);
  too_many_phases[i].steps = single_step;
  too_many_phases[i].step_count = 1;
 }
 pc2ps3::BossPatternTimeline over_phase_limit(
     too_many_phases, pc2ps3::BossPatternTimeline::kMaxPhases + 1);
 assert(!over_phase_limit.valid());

 pc2ps3::BossActionStep max_steps[
     pc2ps3::BossPatternTimeline::kMaxStepsPerPhase];
 for (std::size_t i = 0;
      i < pc2ps3::BossPatternTimeline::kMaxStepsPerPhase; ++i) {
  max_steps[i].tick_offset = static_cast<std::uint32_t>(i);
  max_steps[i].action_id = static_cast<std::uint32_t>(100 + i);
 }
 const pc2ps3::BossPhaseSequence at_step_limit[] = {
     {1, max_steps, pc2ps3::BossPatternTimeline::kMaxStepsPerPhase}};
 pc2ps3::BossPatternTimeline steps_bounded(at_step_limit, 1);
 assert(steps_bounded.valid());

 pc2ps3::BossActionStep too_many_steps[
     pc2ps3::BossPatternTimeline::kMaxStepsPerPhase + 1];
 for (std::size_t i = 0;
      i < pc2ps3::BossPatternTimeline::kMaxStepsPerPhase + 1; ++i) {
  too_many_steps[i].tick_offset = static_cast<std::uint32_t>(i);
  too_many_steps[i].action_id = static_cast<std::uint32_t>(200 + i);
 }
 const pc2ps3::BossPhaseSequence over_step_limit[] = {
     {2, too_many_steps,
      pc2ps3::BossPatternTimeline::kMaxStepsPerPhase + 1}};
 pc2ps3::BossPatternTimeline steps_unbounded(over_step_limit, 1);
 assert(!steps_unbounded.valid());

 const pc2ps3::BossActionStep max_offset[] = {{65535, 1}};
 const pc2ps3::BossPhaseSequence max_offset_phase[] = {{3, max_offset, 1}};
 pc2ps3::BossPatternTimeline offset_bounded(max_offset_phase, 1);
 assert(offset_bounded.valid());
 const pc2ps3::BossActionStep excessive_offset[] = {{65536, 1}};
 const pc2ps3::BossPhaseSequence bad_offset_phase[] = {
     {4, excessive_offset, 1}};
 pc2ps3::BossPatternTimeline offset_unbounded(bad_offset_phase, 1);
 assert(!offset_unbounded.valid());
}

} // namespace

int main() {
 const pc2ps3::BossActionStep first_phase[] = {{0, 10}, {0, 11}, {2, 12}};
 const pc2ps3::BossActionStep second_phase[] = {{1, 20}};
 const pc2ps3::BossPhaseSequence phases[] = {
     {3, first_phase, 3}, {7, second_phase, 1}};
 pc2ps3::BossPatternTimeline pattern(phases, 2);
 assert(pattern.valid() && !pattern.active());

 std::uint32_t output[2] = {90, 91};
 std::size_t count = 8;
 assert(pattern.begin_phase(3));
 std::uint32_t current_phase = 0;
 assert(pattern.current_phase(current_phase) && current_phase == 3);
 assert(!pattern.advance(output, 1, count));
 assert(count == 0 && output[0] == 90 && pattern.current_tick() == 0);
 assert(pattern.advance(output, 2, count));
 assert(count == 2 && output[0] == 10 && output[1] == 11);
 assert(pattern.current_tick() == 1);
 assert(pattern.advance(0, 0, count) && count == 0);
 assert(pattern.current_tick() == 2);
 assert(!pattern.begin_phase(99));
 assert(pattern.current_phase(current_phase) && current_phase == 3);
 assert(pattern.advance(output, 2, count));
 assert(count == 1 && output[0] == 12 && pattern.finished());

 assert(pattern.begin_phase(7));
 assert(pattern.advance(0, 0, count) && count == 0);
 assert(pattern.advance(output, 2, count));
 assert(count == 1 && output[0] == 20 && pattern.finished());
 pattern.reset();
 assert(!pattern.active() && !pattern.current_phase(current_phase));

 const pc2ps3::BossActionStep descending[] = {{2, 1}, {1, 2}};
 const pc2ps3::BossPhaseSequence duplicate_phase[] = {
     {1, first_phase, 3}, {1, second_phase, 1}};
 const pc2ps3::BossPhaseSequence bad_steps[] = {{1, descending, 2}};
 pc2ps3::BossPatternTimeline invalid_order(bad_steps, 1);
 pc2ps3::BossPatternTimeline invalid_duplicate(duplicate_phase, 2);
 pc2ps3::BossPatternTimeline invalid_null(0, 0);
 assert(!invalid_order.valid() && !invalid_duplicate.valid() &&
        !invalid_null.valid());
 assert(!invalid_order.advance(output, 2, count) && count == 0);
 test_phase_step_and_offset_limits();
 return 0;
}
