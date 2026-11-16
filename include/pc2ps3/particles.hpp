#pragma once

#include "pc2ps3/render.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

struct ParticleSpawn {
  int x;
  int y;
  std::int32_t velocity_x;
  std::int32_t velocity_y;
  std::uint32_t lifetime_ms;
  int size;
  Color color;
};

class ParticleSystem {
 public:
  enum {
    kMaxCapacity = 1024,
    kMaxLifetimeMs = 600000,
    kMaxVelocityPixelsPerSecond = 1000000,
    kMaxSize = 64
  };

  explicit ParticleSystem(std::size_t capacity);

  bool valid() const;
  bool spawn(const ParticleSpawn& particle);
  bool update(std::uint32_t dt_ms);
  void draw(Framebuffer& framebuffer, int camera_x = 0,
            int camera_y = 0) const;
  void clear();

  std::size_t capacity() const;
  std::size_t active_count() const;

 private:
  struct Slot {
    Slot();
    std::int64_t x_q16;
    std::int64_t y_q16;
    std::int32_t velocity_x;
    std::int32_t velocity_y;
    std::uint32_t remaining_ms;
    int size;
    Color color;
    bool active;
  };

  bool valid_;
  std::vector<Slot> slots_;
};

}  // namespace pc2ps3
