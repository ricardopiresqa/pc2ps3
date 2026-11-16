#include "pc2ps3/projectile_pattern.hpp"

namespace pc2ps3 {

ProjectilePattern::ProjectilePattern(const ProjectileStep* steps,
                                     std::size_t step_count)
    : steps_(steps), step_count_(step_count), cursor_(0), current_tick_(0),
      valid_(steps != 0 && step_count > 0 && step_count <= kMaxSteps) {
 if (!valid_) {
  return;
 }
 for (std::size_t i = 1; i < step_count_; ++i) {
  if (steps_[i].tick_offset < steps_[i - 1].tick_offset) {
   valid_ = false;
   return;
  }
 }
}

bool ProjectilePattern::valid() const { return valid_; }

bool ProjectilePattern::finished() const {
 return valid_ && cursor_ >= step_count_;
}

std::uint32_t ProjectilePattern::current_tick() const { return current_tick_; }

bool ProjectilePattern::advance(ProjectileSpawn* output,
                                std::size_t output_capacity,
                                std::size_t& output_count) {
 output_count = 0;
 if (!valid_) {
  return false;
 }
 if (finished()) {
  return true;
 }

 std::size_t due_count = 0;
 while (cursor_ + due_count < step_count_ &&
        steps_[cursor_ + due_count].tick_offset == current_tick_) {
  ++due_count;
 }
 if (output_capacity < due_count || (due_count > 0 && !output)) {
  return false;
 }

 for (std::size_t i = 0; i < due_count; ++i) {
  output[i] = steps_[cursor_ + i].projectile;
 }
 cursor_ += due_count;
 output_count = due_count;
 if (cursor_ < step_count_) {
  ++current_tick_;
 }
 return true;
}

void ProjectilePattern::reset() {
 cursor_ = 0;
 current_tick_ = 0;
}

} // namespace pc2ps3
