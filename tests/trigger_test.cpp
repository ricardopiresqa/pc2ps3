#include "pc2ps3/trigger.hpp"

#include <cassert>
#include <iostream>
#include <limits>

int main() {
  pc2ps3::TriggerSystem triggers(2);
  assert(triggers.valid());
  assert(triggers.add(10, pc2ps3::Aabb{0.0f, 0.0f, 10.0f, 10.0f}));
  assert(!triggers.add(10, pc2ps3::Aabb{20.0f, 20.0f, 30.0f, 30.0f}));
  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(!triggers.add(30, pc2ps3::Aabb{nan, 0.0f, 1.0f, 1.0f}));
  assert(triggers.trigger_count() == 1);
  assert(triggers.add(20, pc2ps3::Aabb{5.0f, 5.0f, 15.0f, 15.0f}));
  assert(!triggers.add(30, pc2ps3::Aabb{20.0f, 20.0f, 30.0f, 30.0f}));
  assert(!triggers.add(30, pc2ps3::Aabb{2.0f, 2.0f, 2.0f, 5.0f}));

  const std::size_t event_capacity = triggers.events().capacity();
  assert(triggers.update(pc2ps3::Aabb{7.0f, 7.0f, 8.0f, 8.0f}));
  assert(triggers.events().size() == 2);
  assert(triggers.events()[0].trigger_id == 10);
  assert(triggers.events()[0].type == pc2ps3::TriggerEventType::kEnter);
  assert(triggers.events()[1].trigger_id == 20);
  assert(triggers.events()[1].type == pc2ps3::TriggerEventType::kEnter);

  assert(!triggers.update(pc2ps3::Aabb{nan, 4.0f, 5.0f, 6.0f}));
  assert(triggers.events().size() == 2);
  assert(triggers.update(pc2ps3::Aabb{7.5f, 7.5f, 8.5f, 8.5f}));
  assert(triggers.events().empty());

  assert(triggers.update(pc2ps3::Aabb{16.0f, 16.0f, 17.0f, 17.0f}));
  assert(triggers.events().size() == 2);
  assert(triggers.events()[0].trigger_id == 10);
  assert(triggers.events()[0].type == pc2ps3::TriggerEventType::kExit);
  assert(triggers.events()[1].trigger_id == 20);
  assert(triggers.events()[1].type == pc2ps3::TriggerEventType::kExit);
  assert(triggers.events().capacity() == event_capacity);

  assert(triggers.update(pc2ps3::Aabb{7.0f, 7.0f, 8.0f, 8.0f}));
  triggers.reset();
  assert(triggers.events().empty());
  assert(triggers.update(pc2ps3::Aabb{7.0f, 7.0f, 8.0f, 8.0f}));
  assert(triggers.events().size() == 2);
  assert(triggers.events()[0].type == pc2ps3::TriggerEventType::kEnter);

  pc2ps3::TriggerSystem invalid_capacity(0);
  assert(!invalid_capacity.valid());
  assert(!invalid_capacity.update(pc2ps3::Aabb{0.0f, 0.0f, 1.0f, 1.0f}));
  pc2ps3::TriggerSystem too_many_triggers(1025);
  assert(!too_many_triggers.valid());

  std::cout << "trigger tests passed\n";
  return 0;
}
