#include "pc2ps3/trigger.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace pc2ps3 {
namespace {

const std::size_t kMaxTriggers = 1024u;

} // namespace

TriggerSystem::TriggerSystem(std::size_t capacity)
    : capacity_(capacity), valid_(false) {
  if (capacity == 0 || capacity > kMaxTriggers) return;
  try {
    entries_.reserve(capacity);
    events_.reserve(capacity);
  } catch (const std::bad_alloc&) {
    std::vector<Entry>().swap(entries_);
    std::vector<TriggerEvent>().swap(events_);
    capacity_ = 0;
    return;
  }
  valid_ = true;
}

bool TriggerSystem::valid() const { return valid_; }

bool TriggerSystem::add(std::uint32_t id, const Aabb& bounds) {
  if (!valid_ || !valid_bounds(bounds) || entries_.size() >= capacity_) {
    return false;
  }
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].id == id) return false;
  }
  entries_.push_back(Entry(id, bounds));
  return true;
}

bool TriggerSystem::update(const Aabb& actor_bounds) {
  if (!valid_ || !valid_bounds(actor_bounds)) return false;

  events_.clear();
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    Entry& entry = entries_[i];
    const bool overlapping =
        query_aabb(actor_bounds, entry.bounds).overlapping;
    if (overlapping == entry.overlapping) continue;
    entry.overlapping = overlapping;
    events_.push_back(TriggerEvent{
        entry.id, overlapping ? TriggerEventType::kEnter
                              : TriggerEventType::kExit});
  }
  return true;
}

void TriggerSystem::reset() {
  events_.clear();
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    entries_[i].overlapping = false;
  }
}

const std::vector<TriggerEvent>& TriggerSystem::events() const {
  return events_;
}

std::size_t TriggerSystem::trigger_count() const { return entries_.size(); }

bool TriggerSystem::valid_bounds(const Aabb& bounds) {
  return query_aabb(bounds, bounds).overlapping;
}

} // namespace pc2ps3
