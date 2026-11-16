#include "pc2ps3/campaign_progression.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
int failures = 0;

void expect(bool condition, const char* label) {
  if (!condition) {
    std::cerr << "FAIL: " << label << '\n';
    ++failures;
  }
}

class MemoryFileSystem : public pc2ps3::FileSystem {
 public:
  pc2ps3::FileSystemStatus read_all(
      const std::string& path, std::size_t max_bytes,
      std::vector<std::uint8_t>& output) {
    std::map<std::string, std::vector<std::uint8_t> >::const_iterator it =
        files.find(path);
    if (it == files.end()) return pc2ps3::FileSystemStatus::kNotFound;
    if (it->second.size() > max_bytes) {
      return pc2ps3::FileSystemStatus::kTooLarge;
    }
    output = it->second;
    return pc2ps3::FileSystemStatus::kOk;
  }

  pc2ps3::FileSystemStatus write_atomic(const std::string& path,
                                        const std::uint8_t* data,
                                        std::size_t size) {
    if (size != 0 && data == 0) return pc2ps3::FileSystemStatus::kError;
    std::vector<std::uint8_t> value;
    if (size != 0) value.assign(data, data + size);
    files[path] = value;
    return pc2ps3::FileSystemStatus::kOk;
  }

  pc2ps3::FileSystemStatus remove(const std::string& path) {
    return files.erase(path) == 0 ? pc2ps3::FileSystemStatus::kNotFound
                                  : pc2ps3::FileSystemStatus::kOk;
  }

  std::map<std::string, std::vector<std::uint8_t> > files;
};

void test_unlock_select_and_reset() {
  pc2ps3::CampaignProgression progress(3);
  expect(progress.valid(), "three levels are valid");
  expect(progress.is_unlocked(0) && !progress.is_unlocked(1),
         "only first level starts unlocked");
  expect(!progress.select_level(1), "locked level cannot be selected");
  expect(progress.complete_current_level(), "first level completes");
  expect(progress.is_completed(0) && progress.is_unlocked(1),
         "completion unlocks next level");
  expect(progress.select_level(1), "newly unlocked level selects");
  expect(progress.complete_current_level() && progress.is_unlocked(2),
         "second completion unlocks third level");
  progress.reset();
  expect(progress.is_unlocked(0) && !progress.is_completed(0) &&
             !progress.is_unlocked(1) && progress.current_level() == 0,
         "reset restores initial progression");
}

void test_save_round_trip_and_corrupt_payload_atomicity() {
  MemoryFileSystem files;
  pc2ps3::SaveManager saves(files, "save", 2, 64);
  pc2ps3::CampaignProgression source(3);
  source.complete_current_level();
  source.select_level(1);
  expect(source.save(saves, 0) == pc2ps3::SaveStatus::kOk,
         "progress saves through SaveManager");

  pc2ps3::CampaignProgression loaded(3);
  expect(loaded.load(saves, 0) == pc2ps3::SaveStatus::kOk,
         "progress loads through SaveManager");
  expect(loaded.current_level() == 1 && loaded.is_unlocked(1) &&
             loaded.is_completed(0),
         "round trip restores state");

  const std::vector<std::uint8_t> invalid_payload(1, 99);
  expect(saves.save(1, invalid_payload) == pc2ps3::SaveStatus::kOk,
         "save manager stores opaque payload");
  expect(loaded.load(saves, 1) == pc2ps3::SaveStatus::kCorruptData,
         "invalid progression payload is rejected");
  expect(loaded.current_level() == 1 && loaded.is_completed(0),
         "corrupt load leaves live state unchanged");

  std::vector<std::uint8_t> impossible_progress(19, 0);
  impossible_progress[0] = 1;
  impossible_progress[1] = 3;
  impossible_progress[2] = 7;
  expect(saves.save(1, impossible_progress) == pc2ps3::SaveStatus::kOk,
         "save manager stores progression payload for validation");
  expect(loaded.load(saves, 1) == pc2ps3::SaveStatus::kCorruptData,
         "impossible unlocked mask is rejected");
  expect(loaded.current_level() == 1 && loaded.is_completed(0),
         "impossible progress also leaves live state unchanged");
}

void test_invalid_campaign_limits() {
  pc2ps3::CampaignProgression empty(0);
  pc2ps3::CampaignProgression too_many(65);
  expect(!empty.valid() && !too_many.valid(), "level count is bounded");
  expect(!empty.complete_current_level() && !too_many.select_level(0),
         "invalid campaign rejects operations");
}
}  // namespace

int main() {
  test_unlock_select_and_reset();
  test_save_round_trip_and_corrupt_payload_atomicity();
  test_invalid_campaign_limits();
  if (failures != 0) return 1;
  std::cout << "campaign progression tests passed\n";
  return 0;
}
