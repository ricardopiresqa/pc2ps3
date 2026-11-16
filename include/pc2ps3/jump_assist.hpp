#pragma once

namespace pc2ps3 {

// Deterministic jump buffer and coyote window measured in World::step ticks.
class JumpAssist {
public:
  JumpAssist();

  void reset();
  bool update(bool jump_pressed, bool grounded);

private:
  unsigned int jump_buffer_remaining_;
  unsigned int coyote_remaining_;
};

} // namespace pc2ps3
