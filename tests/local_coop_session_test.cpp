#include "pc2ps3/local_coop_session.hpp"

#include <cassert>

int main() {
  const pc2ps3::WorldConfig config = {1.0f, 0.0f, 8.0f, 8.0f, 0.0f, 0.0f};
  pc2ps3::World player_one(config);
  pc2ps3::World player_two(config);
  pc2ps3::CoopInputRouter router;
  pc2ps3::LocalCoopSession session(player_one, player_two, router);
  assert(session.valid());

  const float initial_one = player_one.player().position.x;
  const float initial_two = player_two.player().position.x;
  pc2ps3::InputFrame one = {};
  pc2ps3::InputFrame two = {};
  one.move_x = 1.0f;
  two.move_x = -1.0f;

  router.begin_tick();
  assert(router.submit(0, one));
  assert(!session.step());
  assert(player_one.player().position.x == initial_one);
  assert(player_two.player().position.x == initial_two);
  assert(router.submit(1, two));
  assert(session.step());
  const float after_one = player_one.player().position.x;
  const float after_two = player_two.player().position.x;
  assert(after_one > initial_one && after_two < initial_two);

  router.begin_tick();
  assert(router.submit(0, one));
  assert(router.submit(1, two));
  assert(session.step());
  assert(player_one.player().position.x > after_one);
  assert(player_two.player().position.x < after_two);

  pc2ps3::LocalCoopSession duplicate_worlds(player_one, player_one, router);
  assert(!duplicate_worlds.valid());
  assert(!duplicate_worlds.step());
  return 0;
}
