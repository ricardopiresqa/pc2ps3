#pragma once

#include "pc2ps3/render.hpp"

#include <cstddef>
#include <vector>

namespace pc2ps3 {

// Bounded command queue. Textures are borrowed until flush() or clear().
class RenderBatch {
 public:
  enum { kMaxCommands = 4096 };

  explicit RenderBatch(std::size_t capacity);

  bool valid() const;
  std::size_t capacity() const;
  std::size_t size() const;
  bool empty() const;

  bool submit_texture_region(const Texture& texture, int source_x,
                             int source_y, int source_width,
                             int source_height, int destination_x,
                             int destination_y);
  bool submit_texture_region_scaled(const Texture& texture, int source_x,
                                    int source_y, int source_width,
                                    int source_height, int destination_x,
                                    int destination_y, int destination_width,
                                    int destination_height);
  bool submit_texture_region_rotated(const Texture& texture, int source_x,
                                     int source_y, int source_width,
                                     int source_height, int destination_x,
                                     int destination_y, int angle_degrees);

  // Prevalidates the whole queue; on failure the queue remains unchanged.
  bool flush(Framebuffer& framebuffer);
  void clear();

 private:
  enum CommandType {
    kTextureRegion,
    kScaledTextureRegion,
    kRotatedTextureRegion
  };

  struct Command {
    CommandType type;
    const Texture* texture;
    int source_x;
    int source_y;
    int source_width;
    int source_height;
    int destination_x;
    int destination_y;
    int destination_width;
    int destination_height;
    int angle_degrees;
  };

  bool enqueue(const Command& command);
  bool valid_command(const Command& command) const;

  bool valid_;
  std::size_t max_commands_;
  std::vector<Command> commands_;
};

}  // namespace pc2ps3
