#include "pc2ps3/local_coop_session.hpp"

namespace pc2ps3 {

LocalCoopSession::LocalCoopSession(World& player_one, World& player_two,
                                   CoopInputRouter& input_router)
    : worlds_{&player_one, &player_two},
      input_router_(&input_router),
      valid_(&player_one != &player_two) {}

bool LocalCoopSession::valid() const { return valid_; }

bool LocalCoopSession::step() {
  if (!valid_) return false;

  const InputFrame* player_one_frame = input_router_->frame(0);
  const InputFrame* player_two_frame = input_router_->frame(1);
  if (!player_one_frame || !player_two_frame) return false;

  worlds_[0]->step(*player_one_frame);
  worlds_[1]->step(*player_two_frame);
  return true;
}

}  // namespace pc2ps3
