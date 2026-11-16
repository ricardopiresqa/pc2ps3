#pragma once

#include "pc2ps3/storage.hpp"

#include <cstddef>
#include <cstdint>

namespace pc2ps3 {

class CampaignProgression {
 public:
  enum { kMaxLevels = 64 };

  explicit CampaignProgression(std::size_t level_count);

  bool valid() const;
  std::size_t level_count() const;
  std::size_t current_level() const;
  bool is_unlocked(std::size_t level) const;
  bool is_completed(std::size_t level) const;
  bool select_level(std::size_t level);
  bool complete_current_level();
  void reset();

  SaveStatus save(SaveManager& manager, std::uint32_t slot) const;
  SaveStatus load(SaveManager& manager, std::uint32_t slot);

 private:
  std::size_t level_count_;
  std::uint64_t unlocked_mask_;
  std::uint64_t completed_mask_;
  std::size_t current_level_;
  bool valid_;
};

}  // namespace pc2ps3
