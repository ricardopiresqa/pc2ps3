#pragma once

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

struct BossActionStep {
 std::uint32_t tick_offset;
 std::uint32_t action_id;
};

struct BossPhaseSequence {
 std::uint32_t phase_id;
 const BossActionStep* steps;
 std::size_t step_count;
};

// Emits caller-authored action IDs for one selected boss phase.
class BossPatternTimeline {
 public:
 enum { kMaxPhases = 8, kMaxStepsPerPhase = 128, kMaxTickOffset = 65535 };

 BossPatternTimeline(const BossPhaseSequence* phases,
                     std::size_t phase_count);

 bool valid() const;
 bool begin_phase(std::uint32_t phase_id);
 bool active() const;
 bool finished() const;
 std::uint32_t current_tick() const;
 bool current_phase(std::uint32_t& phase_id) const;
 bool advance(std::uint32_t* output, std::size_t output_capacity,
              std::size_t& output_count);
 void reset();

 private:
 const BossPhaseSequence* phases_;
 std::size_t phase_count_;
 const BossActionStep* current_steps_;
 std::size_t current_step_count_;
 std::size_t cursor_;
 std::uint32_t current_phase_id_;
 std::uint32_t current_tick_;
 bool valid_;
 bool active_;
};

} // namespace pc2ps3
