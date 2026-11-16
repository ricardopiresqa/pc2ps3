#include "pc2ps3/bundle_file_system.hpp"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_bytes(const std::string& path) {
  std::ifstream input(path.c_str(), std::ios::binary);
  assert(input.good());
  input.seekg(0, std::ios::end);
  const std::streamoff size = input.tellg();
  assert(size >= 0);
  input.seekg(0, std::ios::beg);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(&bytes[0]), size);
    assert(input.good() || input.eof());
    assert(input.gcount() == size);
  }
  return bytes;
}

bool write_bytes(const std::string& path,
                 const std::vector<std::uint8_t>& bytes) {
  std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!output) return false;
  if (!bytes.empty()) {
    output.write(reinterpret_cast<const char*>(&bytes[0]),
                 static_cast<std::streamsize>(bytes.size()));
  }
  output.close();
  return output.good();
}

void store_u64(std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint64_t value) {
  for (unsigned int i = 0; i < 8; ++i) {
    bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
  }
}

std::uint64_t load_u64(const std::vector<std::uint8_t>& bytes,
                       std::size_t offset) {
  std::uint64_t value = 0;
  for (unsigned int i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8);
  }
  return value;
}

void remove_file(const std::string& path) { std::remove(path.c_str()); }

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: bundle_file_system_test <package>\n";
    return 2;
  }
  const std::string package_path(argv[1]);

  pc2ps3::BundleFileSystem bundle(package_path);
  assert(bundle.valid());

  std::vector<std::uint8_t> output(1, 0xA5u);
  assert(bundle.read_all("assets/note.md", 64, output) ==
         pc2ps3::FileSystemStatus::kOk);
  const std::string readme(output.begin(), output.end());
  assert(readme == "portable asset\n");

  output.assign(1, 0xA5u);
  assert(bundle.read_all("assets/note.md", 1, output) ==
         pc2ps3::FileSystemStatus::kTooLarge);
  assert(output.size() == 1 && output[0] == 0xA5u);
  assert(bundle.read_all("missing/file.bin", 64, output) ==
         pc2ps3::FileSystemStatus::kNotFound);
  assert(output.size() == 1 && output[0] == 0xA5u);
  assert(bundle.read_all("../outside.bin", 64, output) ==
         pc2ps3::FileSystemStatus::kError);
  assert(bundle.write_atomic("assets/note.md", NULL, 0) ==
         pc2ps3::FileSystemStatus::kError);
  assert(bundle.remove("assets/note.md") ==
         pc2ps3::FileSystemStatus::kError);

  const std::vector<std::uint8_t> original = read_bytes(package_path);
  assert(original.size() > 48);
  const std::uint16_t first_path_size =
      static_cast<std::uint16_t>(original[24]) |
      static_cast<std::uint16_t>(original[25] << 8);
  const std::size_t second_record_start =
      24u + 24u + static_cast<std::size_t>(first_path_size);

  std::vector<std::uint8_t> corrupt_crc = original;
  corrupt_crc.back() ^= 0x01u;
  const std::string crc_path = package_path + ".crc-corrupt";
  assert(write_bytes(crc_path, corrupt_crc));
  {
    pc2ps3::BundleFileSystem corrupt_bundle(crc_path);
    assert(corrupt_bundle.valid());
    output.assign(1, 0xA5u);
    assert(corrupt_bundle.read_all("assets/red.png", 1024, output) ==
           pc2ps3::FileSystemStatus::kError);
    assert(output.size() == 1 && output[0] == 0xA5u);
  }
  remove_file(crc_path);

  std::vector<std::uint8_t> malformed = original;
  malformed[48] = static_cast<std::uint8_t>('/');
  const std::string malformed_path = package_path + ".bad-path";
  assert(write_bytes(malformed_path, malformed));
  {
    pc2ps3::BundleFileSystem malformed_bundle(malformed_path);
    assert(!malformed_bundle.valid());
    output.assign(1, 0xA5u);
    assert(malformed_bundle.read_all("assets/red.png", 64, output) ==
           pc2ps3::FileSystemStatus::kError);
    assert(output.size() == 1 && output[0] == 0xA5u);
  }
  remove_file(malformed_path);

  std::vector<std::uint8_t> unsupported_version = original;
  unsupported_version[8] = 2u;
  const std::string version_path = package_path + ".bad-version";
  assert(write_bytes(version_path, unsupported_version));
  {
    pc2ps3::BundleFileSystem unsupported_bundle(version_path);
    assert(!unsupported_bundle.valid());
  }
  remove_file(version_path);

  std::vector<std::uint8_t> duplicate = original;
  const std::size_t first_name_offset = 48u;
  const std::size_t second_name_offset = second_record_start + 24u;
  assert(second_name_offset + first_path_size <= duplicate.size());
  for (std::size_t i = 0; i < first_path_size; ++i) {
    duplicate[second_name_offset + i] = duplicate[first_name_offset + i];
  }
  const std::string duplicate_path = package_path + ".duplicate";
  assert(write_bytes(duplicate_path, duplicate));
  {
    pc2ps3::BundleFileSystem duplicate_bundle(duplicate_path);
    assert(!duplicate_bundle.valid());
  }
  remove_file(duplicate_path);

  std::vector<std::uint8_t> out_of_range = original;
  store_u64(out_of_range, second_record_start + 4u,
            ~static_cast<std::uint64_t>(0));
  const std::string range_path = package_path + ".out-of-range";
  assert(write_bytes(range_path, out_of_range));
  {
    pc2ps3::BundleFileSystem out_of_range_bundle(range_path);
    assert(!out_of_range_bundle.valid());
  }
  remove_file(range_path);

  std::vector<std::uint8_t> overlapping = original;
  const std::size_t second_record_offset = second_record_start;
  const std::uint64_t data_offset = 24u + load_u64(overlapping, 16u);
  assert(second_record_offset + 24u <= overlapping.size());
  store_u64(overlapping, second_record_offset + 4u, data_offset);
  const std::string overlap_path = package_path + ".overlap";
  assert(write_bytes(overlap_path, overlapping));
  {
    pc2ps3::BundleFileSystem overlapping_bundle(overlap_path);
    assert(!overlapping_bundle.valid());
  }
  remove_file(overlap_path);

  std::cout << "bundle file system scenarios passed\n";
  return 0;
}
