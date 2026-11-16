#pragma once

#include "pc2ps3/coop_input_router.hpp"
#include "pc2ps3/core.hpp"

namespace pc2ps3 {

// Non-owning two-player tick runner. The worlds and router must outlive it.
class LocalCoopSession {
 public:
  LocalCoopSession(World& player_one, World& player_two,
                   CoopInputRouter& input_router);

  bool valid() const;
  // Both input frames are checked before either world is advanced.
  bool step();

 private:
  World* worlds_[2];
  CoopInputRouter* input_router_;
  bool valid_;
};

}  // namespace pc2ps3
