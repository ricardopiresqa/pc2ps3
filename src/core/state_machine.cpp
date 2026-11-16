#include "pc2ps3/state_machine.hpp"

#include <new>

namespace pc2ps3 {

StateMachine::StateMachine(std::size_t state_count,
                           std::uint32_t initial_state)
    : state_count_(0),
      initial_state_(0),
      current_state_(0),
      valid_(false) {
  if (state_count == 0 || state_count > kMaxStates ||
      initial_state >= state_count) {
    return;
  }
  try {
    transitions_.assign(state_count * state_count, 0);
  } catch (const std::bad_alloc&) {
    return;
  }
  state_count_ = state_count;
  initial_state_ = initial_state;
  current_state_ = initial_state;
  valid_ = true;
}

bool StateMachine::valid() const { return valid_; }

bool StateMachine::allow_transition(std::uint32_t from, std::uint32_t to) {
  if (!valid_ || !contains(from) || !contains(to) || from == to) return false;
  transitions_[static_cast<std::size_t>(from) * state_count_ + to] = 1;
  return true;
}

bool StateMachine::can_transition(std::uint32_t to) const {
  if (!valid_ || !contains(to)) return false;
  if (to == current_state_) return true;
  return transitions_[static_cast<std::size_t>(current_state_) * state_count_ +
                      to] != 0;
}

bool StateMachine::transition_to(std::uint32_t to) {
  if (!can_transition(to)) return false;
  current_state_ = to;
  return true;
}

void StateMachine::reset() {
  if (valid_) current_state_ = initial_state_;
}

std::uint32_t StateMachine::current_state() const { return current_state_; }

std::size_t StateMachine::state_count() const { return state_count_; }

bool StateMachine::contains(std::uint32_t state) const {
  return state < state_count_;
}

}  // namespace pc2ps3
