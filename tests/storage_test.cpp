#include "pc2ps3/storage.hpp"

#include <cassert>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

class MemoryFileSystem : public pc2ps3::FileSystem {
 public:
  MemoryFileSystem()
      : fail_reads(false), fail_writes(false), fail_removes(false),
        read_count(0), last_read_limit(0) {}

  pc2ps3::FileSystemStatus read_all(
      const std::string& path, std::size_t max_bytes,
      std::vector<std::uint8_t>& output) override {
    ++read_count;
    last_path = path;
    last_read_limit = max_bytes;
    if (fail_reads) return pc2ps3::FileSystemStatus::kError;
    const std::map<std::string, std::vector<std::uint8_t> >::const_iterator it =
        files.find(path);
    if (it == files.end()) return pc2ps3::FileSystemStatus::kNotFound;
    if (it->second.size() > max_bytes) {
      return pc2ps3::FileSystemStatus::kTooLarge;
    }
    output = it->second;
    return pc2ps3::FileSystemStatus::kOk;
  }

  pc2ps3::FileSystemStatus write_atomic(
      const std::string& path, const std::uint8_t* data,
      std::size_t size) override {
    last_path = path;
    if (fail_writes) return pc2ps3::FileSystemStatus::kError;
    std::vector<std::uint8_t> replacement(data, data + size);
    files[path].swap(replacement);
    return pc2ps3::FileSystemStatus::kOk;
  }

  pc2ps3::FileSystemStatus remove(const std::string& path) override {
    last_path = path;
    if (fail_removes) return pc2ps3::FileSystemStatus::kError;
    return files.erase(path) == 0 ? pc2ps3::FileSystemStatus::kNotFound
                                  : pc2ps3::FileSystemStatus::kOk;
  }

  std::map<std::string, std::vector<std::uint8_t> > files;
  std::string last_path;
  bool fail_reads;
  bool fail_writes;
  bool fail_removes;
  std::size_t read_count;
  std::size_t last_read_limit;
};

void expect_load_preserves_output(pc2ps3::SaveManager& saves,
                                  pc2ps3::SaveStatus expected) {
  std::vector<std::uint8_t> output(1, 0xaau);
  assert(saves.load(1, output) == expected);
  assert(output.size() == 1 && output[0] == 0xaau);
}

}  // namespace

int main() {
  MemoryFileSystem file_system;
  pc2ps3::SaveManager saves(file_system, "user/data", 3, 16);
  assert(saves.valid());
  assert(saves.max_payload_bytes() == 16);

  const std::string slot_path = "user/data/slot_1.pc2save";
  const std::vector<std::uint8_t> known_payload = {
      '1', '2', '3', '4', '5', '6', '7', '8', '9'};
  assert(saves.save(1, known_payload) == pc2ps3::SaveStatus::kOk);
  assert(file_system.last_path == slot_path);
  const std::vector<std::uint8_t>& encoded = file_system.files[slot_path];
  assert(encoded.size() == 14 + known_payload.size());
  assert(encoded[0] == 'P' && encoded[1] == 'C' && encoded[2] == '2' &&
         encoded[3] == 'S');
  assert(encoded[4] == 1 && encoded[5] == 0);
  assert(encoded[6] == 9 && encoded[7] == 0 && encoded[8] == 0 &&
         encoded[9] == 0);
  // CRC-32/IEEE of "123456789" is 0xcbf43926, stored little-endian.
  assert(encoded[10] == 0x26 && encoded[11] == 0x39 && encoded[12] == 0xf4 &&
         encoded[13] == 0xcb);

  std::vector<std::uint8_t> loaded(1, 0xeeu);
  assert(saves.load(1, loaded) == pc2ps3::SaveStatus::kOk);
  assert(loaded == known_payload);
  assert(file_system.last_read_limit == 14 + 16);

  const std::vector<std::uint8_t> empty_payload;
  assert(saves.save(2, empty_payload) == pc2ps3::SaveStatus::kOk);
  assert(saves.load(2, loaded) == pc2ps3::SaveStatus::kOk);
  assert(loaded.empty());
  assert(file_system.last_path == "user/data/slot_2.pc2save");

  const std::vector<std::uint8_t> boundary_payload(16, 0x5au);
  assert(saves.save(0, boundary_payload) == pc2ps3::SaveStatus::kOk);
  assert(saves.load(0, loaded) == pc2ps3::SaveStatus::kOk);
  assert(loaded == boundary_payload);

  const std::size_t reads_before_invalid_slot = file_system.read_count;
  assert(saves.load(3, loaded) == pc2ps3::SaveStatus::kInvalidArgument);
  assert(file_system.read_count == reads_before_invalid_slot);
  assert(saves.erase(3) == pc2ps3::SaveStatus::kInvalidArgument);

  std::vector<std::uint8_t> too_large_payload(17, 0x33u);
  const std::size_t files_before_large_save = file_system.files.size();
  const std::vector<std::uint8_t> slot_zero_before_large_save =
      file_system.files["user/data/slot_0.pc2save"];
  assert(saves.save(0, too_large_payload) == pc2ps3::SaveStatus::kTooLarge);
  assert(file_system.files.size() == files_before_large_save);
  assert(file_system.files["user/data/slot_0.pc2save"] ==
         slot_zero_before_large_save);

  std::vector<std::uint8_t> valid_record = file_system.files[slot_path];
  std::vector<std::uint8_t> corrupt = valid_record;
  corrupt[0] = 'X';
  file_system.files[slot_path] = corrupt;
  expect_load_preserves_output(saves, pc2ps3::SaveStatus::kCorruptData);

  corrupt = valid_record;
  corrupt[4] = 2;
  file_system.files[slot_path] = corrupt;
  expect_load_preserves_output(saves,
                               pc2ps3::SaveStatus::kUnsupportedVersion);

  corrupt = valid_record;
  corrupt[6] = 17;
  file_system.files[slot_path] = corrupt;
  expect_load_preserves_output(saves, pc2ps3::SaveStatus::kTooLarge);

  corrupt = valid_record;
  corrupt.pop_back();
  file_system.files[slot_path] = corrupt;
  expect_load_preserves_output(saves, pc2ps3::SaveStatus::kCorruptData);

  corrupt = valid_record;
  corrupt.push_back(0);
  file_system.files[slot_path] = corrupt;
  expect_load_preserves_output(saves, pc2ps3::SaveStatus::kCorruptData);

  corrupt = valid_record;
  corrupt[14] ^= 1u;
  file_system.files[slot_path] = corrupt;
  expect_load_preserves_output(saves, pc2ps3::SaveStatus::kCorruptData);

  corrupt = valid_record;
  corrupt.push_back(0);
  while (corrupt.size() <= 14 + 16) corrupt.push_back(0);
  file_system.files[slot_path] = corrupt;
  expect_load_preserves_output(saves, pc2ps3::SaveStatus::kTooLarge);

  file_system.files[slot_path] = valid_record;
  file_system.fail_reads = true;
  expect_load_preserves_output(saves, pc2ps3::SaveStatus::kIoError);
  file_system.fail_reads = false;

  const std::vector<std::uint8_t> previous_record = file_system.files[slot_path];
  file_system.fail_writes = true;
  assert(saves.save(1, empty_payload) == pc2ps3::SaveStatus::kIoError);
  assert(file_system.files[slot_path] == previous_record);
  file_system.fail_writes = false;

  file_system.fail_removes = true;
  assert(saves.erase(1) == pc2ps3::SaveStatus::kIoError);
  assert(file_system.files.find(slot_path) != file_system.files.end());
  file_system.fail_removes = false;
  assert(saves.erase(1) == pc2ps3::SaveStatus::kOk);
  assert(saves.load(1, loaded) == pc2ps3::SaveStatus::kNotFound);
  assert(saves.erase(1) == pc2ps3::SaveStatus::kNotFound);

  pc2ps3::SaveManager no_slots(file_system, "user/data", 0, 16);
  assert(!no_slots.valid());
  assert(no_slots.save(0, empty_payload) ==
         pc2ps3::SaveStatus::kInvalidArgument);

  pc2ps3::SaveManager impossible_limit(
      file_system, "user/data", 1,
      std::numeric_limits<std::size_t>::max());
  assert(!impossible_limit.valid());

  pc2ps3::SaveManager maximum_limit(
      file_system, "user/data", 1, 16u * 1024u * 1024u);
  assert(maximum_limit.valid());
  pc2ps3::SaveManager over_budget_limit(
      file_system, "user/data", 1, 16u * 1024u * 1024u + 1u);
  assert(!over_budget_limit.valid());

  pc2ps3::SaveManager multiple_digits(file_system, "root/", 13, 16);
  assert(multiple_digits.save(12, empty_payload) == pc2ps3::SaveStatus::kOk);
  assert(file_system.last_path == "root/slot_12.pc2save");

  return 0;
}
