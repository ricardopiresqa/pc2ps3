#include "pc2ps3/storage.hpp"

#include <limits>

namespace pc2ps3 {
namespace {

const std::size_t kHeaderSize = 14;
const std::size_t kMaximumPayloadBytes = 16u * 1024u * 1024u;
const std::uint16_t kSaveVersion = 1;
const std::uint32_t kCrcPolynomial = 0xedb88320u;
const std::uint8_t kMagic[4] = {'P', 'C', '2', 'S'};

void append_u16_le(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
}

void append_u32_le(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes,
                          std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(
             static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes,
                          std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (unsigned int bit = 0; bit < 8; ++bit) {
      crc = (crc & 1u) != 0u ? (crc >> 1) ^ kCrcPolynomial : crc >> 1;
    }
  }
  return crc ^ 0xffffffffu;
}

}  // namespace

SaveManager::SaveManager(FileSystem& file_system,
                         const std::string& trusted_root,
                         std::uint32_t slot_count,
                         std::size_t max_payload_bytes)
    : file_system_(file_system),
      trusted_root_(trusted_root),
      slot_count_(slot_count),
      max_payload_bytes_(max_payload_bytes),
      valid_(false) {
  const std::uint64_t largest_payload =
      static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) -
      static_cast<std::uint64_t>(kHeaderSize);
  const std::size_t largest_size =
      std::numeric_limits<std::size_t>::max() - kHeaderSize;
  valid_ = slot_count_ > 0 &&
           max_payload_bytes_ <= kMaximumPayloadBytes &&
           static_cast<std::uint64_t>(max_payload_bytes_) <= largest_payload &&
           max_payload_bytes_ <= largest_size;
}

bool SaveManager::valid() const { return valid_; }

SaveStatus SaveManager::save(std::uint32_t slot,
                             const std::vector<std::uint8_t>& payload) {
  if (!valid_ || slot >= slot_count_) return SaveStatus::kInvalidArgument;
  if (payload.size() > max_payload_bytes_ ||
      payload.size() > std::numeric_limits<std::uint32_t>::max()) {
    return SaveStatus::kTooLarge;
  }

  std::vector<std::uint8_t> record;
  record.reserve(kHeaderSize + payload.size());
  record.insert(record.end(), kMagic, kMagic + 4);
  append_u16_le(record, kSaveVersion);
  append_u32_le(record, static_cast<std::uint32_t>(payload.size()));
  const std::uint32_t checksum =
      crc32(payload.empty() ? 0 : &payload[0], payload.size());
  append_u32_le(record, checksum);
  record.insert(record.end(), payload.begin(), payload.end());

  const FileSystemStatus status = file_system_.write_atomic(
      slot_path(slot), &record[0], record.size());
  return map_io_status(status);
}

SaveStatus SaveManager::load(std::uint32_t slot,
                             std::vector<std::uint8_t>& payload) {
  if (!valid_ || slot >= slot_count_) return SaveStatus::kInvalidArgument;

  std::vector<std::uint8_t> record;
  const FileSystemStatus io_status = file_system_.read_all(
      slot_path(slot), kHeaderSize + max_payload_bytes_, record);
  if (io_status != FileSystemStatus::kOk) return map_io_status(io_status);
  if (record.size() > kHeaderSize + max_payload_bytes_) {
    return SaveStatus::kTooLarge;
  }
  if (record.size() < kHeaderSize) return SaveStatus::kCorruptData;
  for (std::size_t i = 0; i < 4; ++i) {
    if (record[i] != kMagic[i]) return SaveStatus::kCorruptData;
  }

  if (read_u16_le(record, 4) != kSaveVersion) {
    return SaveStatus::kUnsupportedVersion;
  }
  const std::uint32_t length = read_u32_le(record, 6);
  if (length > max_payload_bytes_) return SaveStatus::kTooLarge;
  if (record.size() != kHeaderSize + static_cast<std::size_t>(length)) {
    return SaveStatus::kCorruptData;
  }

  const std::uint8_t* bytes = length == 0 ? 0 : &record[kHeaderSize];
  if (crc32(bytes, length) != read_u32_le(record, 10)) {
    return SaveStatus::kCorruptData;
  }

  record.erase(record.begin(), record.begin() + kHeaderSize);
  payload.swap(record);
  return SaveStatus::kOk;
}

SaveStatus SaveManager::erase(std::uint32_t slot) {
  if (!valid_ || slot >= slot_count_) return SaveStatus::kInvalidArgument;
  return map_io_status(file_system_.remove(slot_path(slot)));
}

std::size_t SaveManager::max_payload_bytes() const {
  return max_payload_bytes_;
}

std::string SaveManager::slot_path(std::uint32_t slot) const {
  std::string filename = "slot_";
  std::uint32_t value = slot;
  do {
    filename.insert(filename.begin() + 5,
                    static_cast<char>('0' + value % 10u));
    value /= 10u;
  } while (value != 0u);
  filename += ".pc2save";

  if (trusted_root_.empty()) return filename;
  const char last = trusted_root_[trusted_root_.size() - 1];
  if (last == '/' || last == '\\') return trusted_root_ + filename;
  return trusted_root_ + "/" + filename;
}

SaveStatus SaveManager::map_io_status(FileSystemStatus status) const {
  switch (status) {
    case FileSystemStatus::kOk:
      return SaveStatus::kOk;
    case FileSystemStatus::kNotFound:
      return SaveStatus::kNotFound;
    case FileSystemStatus::kTooLarge:
      return SaveStatus::kTooLarge;
    case FileSystemStatus::kError:
      return SaveStatus::kIoError;
  }
  return SaveStatus::kIoError;
}

}  // namespace pc2ps3
