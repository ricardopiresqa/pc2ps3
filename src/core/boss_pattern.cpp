#include "pc2ps3/boss_pattern.hpp"

namespace pc2ps3 {

BossPatternTimeline::BossPatternTimeline(const BossPhaseSequence* phases,
                                         std::size_t phase_count)
    : phases_(phases), phase_count_(phase_count), current_steps_(0),
      current_step_count_(0), cursor_(0), current_phase_id_(0),
      current_tick_(0),
      valid_(phases != 0 && phase_count > 0 && phase_count <= kMaxPhases),
      active_(false) {
 if (!valid_) {
  return;
 }
 for (std::size_t i = 0; i < phase_count_; ++i) {
  if (!phases_[i].steps || phases_[i].step_count == 0 ||
      phases_[i].step_count > kMaxStepsPerPhase) {
   valid_ = false;
   return;
  }
  for (std::size_t j = 0; j < i; ++j) {
   if (phases_[j].phase_id == phases_[i].phase_id) {
    valid_ = false;
    return;
   }
  }
  for (std::size_t j = 0; j < phases_[i].step_count; ++j) {
   if (phases_[i].steps[j].tick_offset > kMaxTickOffset ||
       (j > 0 && phases_[i].steps[j].tick_offset <
                     phases_[i].steps[j - 1].tick_offset)) {
    valid_ = false;
    return;
   }
  }
 }
}

bool BossPatternTimeline::valid() const { return valid_; }

bool BossPatternTimeline::begin_phase(std::uint32_t phase_id) {
 if (!valid_) {
  return false;
 }
 for (std::size_t i = 0; i < phase_count_; ++i) {
  if (phases_[i].phase_id == phase_id) {
   current_phase_id_ = phase_id;
   current_steps_ = phases_[i].steps;
   current_step_count_ = phases_[i].step_count;
   cursor_ = 0;
   current_tick_ = 0;
   active_ = true;
   return true;
  }
 }
 return false;
}

bool BossPatternTimeline::active() const { return valid_ && active_; }

bool BossPatternTimeline::finished() const {
 return active() && cursor_ >= current_step_count_;
}

std::uint32_t BossPatternTimeline::current_tick() const {
 return current_tick_;
}

bool BossPatternTimeline::current_phase(std::uint32_t& phase_id) const {
 if (!active()) {
  return false;
 }
 phase_id = current_phase_id_;
 return true;
}

bool BossPatternTimeline::advance(std::uint32_t* output,
                                  std::size_t output_capacity,
                                  std::size_t& output_count) {
 output_count = 0;
 if (!active()) {
  return false;
 }
 if (finished()) {
  return true;
 }

 std::size_t due_count = 0;
 while (cursor_ + due_count < current_step_count_ &&
        current_steps_[cursor_ + due_count].tick_offset == current_tick_) {
  ++due_count;
 }
 if (output_capacity < due_count || (due_count > 0 && !output)) {
  return false;
 }

 for (std::size_t i = 0; i < due_count; ++i) {
  output[i] = current_steps_[cursor_ + i].action_id;
 }
 cursor_ += due_count;
 output_count = due_count;
 if (cursor_ < current_step_count_) {
  ++current_tick_;
 }
 return true;
}

void BossPatternTimeline::reset() {
 current_steps_ = 0;
 current_step_count_ = 0;
 cursor_ = 0;
 current_phase_id_ = 0;
 current_tick_ = 0;
 active_ = false;
}

} // namespace pc2ps3
