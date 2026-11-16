#pragma once

#include "pc2ps3/physics.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

enum class TriggerEventType {
  kEnter,
  kExit
};

struct TriggerEvent {
  std::uint32_t trigger_id;
  TriggerEventType type;
};

// Single-threaded static AABB sensors. Events stay valid until update/reset.
class TriggerSystem {
public:
  explicit TriggerSystem(std::size_t capacity);
  TriggerSystem(const TriggerSystem&) = delete;
  TriggerSystem& operator=(const TriggerSystem&) = delete;

  bool valid() const;
  bool add(std::uint32_t id, const Aabb& bounds);
  bool update(const Aabb& actor_bounds);
  void reset();

  const std::vector<TriggerEvent>& events() const;
  std::size_t trigger_count() const;

private:
  struct Entry {
    Entry(std::uint32_t entry_id, const Aabb& entry_bounds)
        : id(entry_id), bounds(entry_bounds), overlapping(false) {}

    std::uint32_t id;
    Aabb bounds;
    bool overlapping;
  };

  static bool valid_bounds(const Aabb& bounds);

  std::size_t capacity_;
  std::vector<Entry> entries_;
  std::vector<TriggerEvent> events_;
  bool valid_;
};

} // namespace pc2ps3
