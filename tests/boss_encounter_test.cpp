#include "pc2ps3/boss_encounter.hpp"

#include <iostream>
#include <limits>

namespace {
int failures = 0;

void expect(bool condition, const char* label) {
  if (!condition) {
    std::cerr << "FAIL: " << label << '\n';
    ++failures;
  }
}

void test_thresholds_and_multiple_phase_crossing() {
  const std::uint32_t thresholds[] = {70, 30};
  pc2ps3::BossEncounter boss(100, thresholds, 2);
  expect(boss.valid() && boss.phase_count() == 3, "valid phase config");
  bool phase_changed = false;
  bool defeated = false;
  expect(boss.damage(31, phase_changed, defeated) && phase_changed &&
             !defeated && boss.health() == 69 && boss.phase() == 1,
         "crosses first threshold");
  expect(boss.damage(44, phase_changed, defeated) && phase_changed &&
             !defeated && boss.health() == 25 && boss.phase() == 2,
         "one hit crosses multiple thresholds");
}

void test_lethal_damage_and_reset() {
  const std::uint32_t threshold[] = {50};
  pc2ps3::BossEncounter boss(100, threshold, 1);
  bool phase_changed = false;
  bool defeated = false;
  expect(boss.damage(50, phase_changed, defeated) && phase_changed &&
             !defeated && boss.phase() == 1,
         "threshold is inclusive");
  expect(boss.damage(std::numeric_limits<std::uint32_t>::max(),
                     phase_changed, defeated) &&
             defeated && boss.defeated() && boss.health() == 0,
         "lethal damage saturates without underflow");
  expect(!boss.damage(1, phase_changed, defeated),
         "defeated encounter rejects further damage");
  boss.reset();
  expect(!boss.defeated() && boss.health() == 100 && boss.phase() == 0,
         "reset restores encounter");
}

void test_invalid_phase_thresholds() {
  const std::uint32_t ascending[] = {30, 70};
  const std::uint32_t invalid_range[] = {100};
  const std::uint32_t too_many[pc2ps3::BossEncounter::kMaxThresholds + 1] =
      {90, 80, 70, 60, 50, 40, 30, 20};
  pc2ps3::BossEncounter bad_order(100, ascending, 2);
  pc2ps3::BossEncounter bad_range(100, invalid_range, 1);
  pc2ps3::BossEncounter bad_pointer(100, 0, 1);
  pc2ps3::BossEncounter excessive(100, too_many,
                                  pc2ps3::BossEncounter::kMaxThresholds + 1);
  expect(!bad_order.valid() && !bad_range.valid() && !bad_pointer.valid() &&
             !excessive.valid(),
         "invalid configs fail closed");
}
}  // namespace

int main() {
  test_thresholds_and_multiple_phase_crossing();
  test_lethal_damage_and_reset();
  test_invalid_phase_thresholds();
  if (failures != 0) return 1;
  std::cout << "boss encounter tests passed\n";
  return 0;
}
