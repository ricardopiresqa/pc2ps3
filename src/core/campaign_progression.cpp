#include "pc2ps3/campaign_progression.hpp"

#include <vector>

namespace pc2ps3 {
namespace {
const std::uint8_t kSaveVersion = 1;
const std::size_t kPayloadSize = 19;

void append_u64_le(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
    value >>= 8;
  }
}

std::uint64_t read_u64_le(const std::vector<std::uint8_t>& bytes,
                          std::size_t offset) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8);
  }
  return value;
}

std::uint64_t level_mask(std::size_t count) {
  return count == 64 ? UINT64_MAX : (UINT64_C(1) << count) - 1;
}

bool is_prefix_mask(std::uint64_t mask) { return (mask & (mask + 1)) == 0; }
}  // namespace

CampaignProgression::CampaignProgression(std::size_t level_count)
    : level_count_(level_count),
      unlocked_mask_(0),
      completed_mask_(0),
      current_level_(0),
      valid_(level_count > 0 && level_count <= kMaxLevels) {
  if (valid_) unlocked_mask_ = 1;
}

bool CampaignProgression::valid() const { return valid_; }

std::size_t CampaignProgression::level_count() const { return level_count_; }

std::size_t CampaignProgression::current_level() const {
  return current_level_;
}

bool CampaignProgression::is_unlocked(std::size_t level) const {
  return valid_ && level < level_count_ &&
         (unlocked_mask_ & (UINT64_C(1) << level)) != 0;
}

bool CampaignProgression::is_completed(std::size_t level) const {
  return valid_ && level < level_count_ &&
         (completed_mask_ & (UINT64_C(1) << level)) != 0;
}

bool CampaignProgression::select_level(std::size_t level) {
  if (!is_unlocked(level)) return false;
  current_level_ = level;
  return true;
}

bool CampaignProgression::complete_current_level() {
  if (!valid_ || current_level_ >= level_count_) return false;
  completed_mask_ |= UINT64_C(1) << current_level_;
  if (current_level_ + 1 < level_count_) {
    unlocked_mask_ |= UINT64_C(1) << (current_level_ + 1);
  }
  return true;
}

void CampaignProgression::reset() {
  if (!valid_) return;
  unlocked_mask_ = 1;
  completed_mask_ = 0;
  current_level_ = 0;
}

SaveStatus CampaignProgression::save(SaveManager& manager,
                                     std::uint32_t slot) const {
  if (!valid_) return SaveStatus::kInvalidArgument;
  std::vector<std::uint8_t> payload;
  payload.reserve(kPayloadSize);
  payload.push_back(kSaveVersion);
  payload.push_back(static_cast<std::uint8_t>(level_count_));
  append_u64_le(payload, unlocked_mask_);
  append_u64_le(payload, completed_mask_);
  payload.push_back(static_cast<std::uint8_t>(current_level_));
  return manager.save(slot, payload);
}

SaveStatus CampaignProgression::load(SaveManager& manager,
                                     std::uint32_t slot) {
  if (!valid_) return SaveStatus::kInvalidArgument;
  std::vector<std::uint8_t> payload;
  const SaveStatus status = manager.load(slot, payload);
  if (status != SaveStatus::kOk) return status;
  if (payload.size() != kPayloadSize || payload[0] != kSaveVersion ||
      payload[1] != level_count_) {
    return SaveStatus::kCorruptData;
  }

  const std::uint64_t unlocked = read_u64_le(payload, 2);
  const std::uint64_t completed = read_u64_le(payload, 10);
  const std::size_t current = payload[18];
  const std::uint64_t allowed = level_mask(level_count_);
  std::size_t unlocked_count = 0;
  std::size_t completed_count = 0;
  for (std::size_t i = 0; i < level_count_; ++i) {
    if ((unlocked & (UINT64_C(1) << i)) != 0) ++unlocked_count;
    if ((completed & (UINT64_C(1) << i)) != 0) ++completed_count;
  }
  const std::size_t expected_unlocked_count =
      completed_count + (completed_count < level_count_ ? 1 : 0);
  if ((unlocked & 1) == 0 || (unlocked & ~allowed) != 0 ||
      (completed & ~unlocked) != 0 || !is_prefix_mask(unlocked) ||
      !is_prefix_mask(completed) ||
      unlocked_count != expected_unlocked_count || current >= level_count_ ||
      (unlocked & (UINT64_C(1) << current)) == 0) {
    return SaveStatus::kCorruptData;
  }

  unlocked_mask_ = unlocked;
  completed_mask_ = completed;
  current_level_ = current;
  return SaveStatus::kOk;
}

}  // namespace pc2ps3
