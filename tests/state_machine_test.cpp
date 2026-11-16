#include "pc2ps3/state_machine.hpp"

#include <iostream>

namespace {
int failures = 0;

#define CHECK(expression)                                                     \
  do {                                                                        \
    if (!(expression)) {                                                      \
      std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": "            \
                << #expression << '\n';                                      \
      ++failures;                                                             \
    }                                                                         \
  } while (false)

void test_declared_transitions_and_reset() {
  pc2ps3::StateMachine machine(3, 0);
  CHECK(machine.valid());
  CHECK(machine.state_count() == 3);
  CHECK(machine.current_state() == 0);
  CHECK(machine.allow_transition(0, 1));
  CHECK(machine.allow_transition(1, 2));
  CHECK(machine.allow_transition(2, 0));
  CHECK(machine.can_transition(0));
  CHECK(machine.transition_to(1));
  CHECK(machine.current_state() == 1);
  CHECK(machine.transition_to(1));
  CHECK(machine.current_state() == 1);
  CHECK(!machine.transition_to(0));
  CHECK(machine.current_state() == 1);
  CHECK(machine.transition_to(2));
  machine.reset();
  CHECK(machine.current_state() == 0);
  CHECK(machine.transition_to(1));
}

void test_invalid_configuration_and_ids() {
  pc2ps3::StateMachine zero_states(0, 0);
  pc2ps3::StateMachine bad_initial(2, 2);
  pc2ps3::StateMachine too_many(pc2ps3::StateMachine::kMaxStates + 1, 0);
  CHECK(!zero_states.valid());
  CHECK(!bad_initial.valid());
  CHECK(!too_many.valid());
  CHECK(!zero_states.allow_transition(0, 1));
  CHECK(!bad_initial.transition_to(0));

  pc2ps3::StateMachine machine(2, 0);
  CHECK(!machine.allow_transition(2, 1));
  CHECK(!machine.allow_transition(0, 2));
  CHECK(!machine.allow_transition(0, 0));
  CHECK(!machine.can_transition(2));
  CHECK(!machine.transition_to(2));
  CHECK(machine.current_state() == 0);
}
}  // namespace

int main() {
  test_declared_transitions_and_reset();
  test_invalid_configuration_and_ids();
  if (failures != 0) {
    std::cerr << failures << " state machine assertion(s) failed\n";
    return 1;
  }
  std::cout << "state machine tests passed\n";
  return 0;
}
