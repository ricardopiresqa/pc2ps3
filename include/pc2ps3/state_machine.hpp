#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

// A bounded, callback-free state graph. State IDs are local to each machine.
class StateMachine {
 public:
  enum { kMaxStates = 64 };

  StateMachine(std::size_t state_count, std::uint32_t initial_state);

  bool valid() const;
  bool allow_transition(std::uint32_t from, std::uint32_t to);
  bool can_transition(std::uint32_t to) const;
  bool transition_to(std::uint32_t to);
  void reset();
  std::uint32_t current_state() const;
  std::size_t state_count() const;

 private:
  bool contains(std::uint32_t state) const;

  std::size_t state_count_;
  std::uint32_t initial_state_;
  std::uint32_t current_state_;
  std::vector<unsigned char> transitions_;
  bool valid_;
};

}  // namespace pc2ps3
