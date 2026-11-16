#pragma once

#include "pc2ps3/core.hpp"

namespace pc2ps3 {

class InputTickDistributor {
public:
  InputTickDistributor();

  void submit(const InputFrame& input);
  bool consume(InputFrame& output);
  void reset();

private:
  InputFrame latest_;
  InputFrame pending_pressed_;
  bool has_input_;
};

} // namespace pc2ps3
