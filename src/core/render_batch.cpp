#include "pc2ps3/render_batch.hpp"

#include <cstdint>
#include <new>

namespace pc2ps3 {

RenderBatch::RenderBatch(std::size_t capacity)
    : valid_(capacity > 0 && capacity <= kMaxCommands),
      max_commands_(valid_ ? capacity : 0),
      commands_() {
  if (!valid_) return;
  try {
    commands_.reserve(capacity);
  } catch (const std::bad_alloc&) {
    valid_ = false;
    max_commands_ = 0;
  }
}

bool RenderBatch::valid() const { return valid_; }
std::size_t RenderBatch::capacity() const {
  return max_commands_;
}
std::size_t RenderBatch::size() const { return commands_.size(); }
bool RenderBatch::empty() const { return commands_.empty(); }

bool RenderBatch::submit_texture_region(
    const Texture& texture, int source_x, int source_y, int source_width,
    int source_height, int destination_x, int destination_y) {
  const Command command = {kTextureRegion, &texture, source_x, source_y,
                           source_width, source_height, destination_x,
                           destination_y, source_width, source_height, 0};
  return enqueue(command);
}

bool RenderBatch::submit_texture_region_scaled(
    const Texture& texture, int source_x, int source_y, int source_width,
    int source_height, int destination_x, int destination_y,
    int destination_width, int destination_height) {
  const Command command = {kScaledTextureRegion, &texture, source_x, source_y,
                           source_width, source_height, destination_x,
                           destination_y, destination_width,
                           destination_height, 0};
  return enqueue(command);
}

bool RenderBatch::submit_texture_region_rotated(
    const Texture& texture, int source_x, int source_y, int source_width,
    int source_height, int destination_x, int destination_y,
    int angle_degrees) {
  const Command command = {kRotatedTextureRegion, &texture, source_x, source_y,
                           source_width, source_height, destination_x,
                           destination_y, source_width, source_height,
                           angle_degrees};
  return enqueue(command);
}

bool RenderBatch::enqueue(const Command& command) {
  if (!valid_ || commands_.size() >= max_commands_ ||
      !valid_command(command)) {
    return false;
  }
  try {
    commands_.push_back(command);
  } catch (const std::bad_alloc&) {
    return false;
  }
  return true;
}

bool RenderBatch::valid_command(const Command& command) const {
  if (command.texture == 0 || !command.texture->valid() ||
      command.source_width <= 0 || command.source_height <= 0 ||
      command.source_x < 0 || command.source_y < 0) {
    return false;
  }
  const std::int64_t source_right =
      static_cast<std::int64_t>(command.source_x) + command.source_width;
  const std::int64_t source_bottom =
      static_cast<std::int64_t>(command.source_y) + command.source_height;
  if (source_right > command.texture->width() ||
      source_bottom > command.texture->height()) {
    return false;
  }
  switch (command.type) {
    case kTextureRegion:
      return true;
    case kScaledTextureRegion:
      return command.destination_width > 0 && command.destination_height > 0;
    case kRotatedTextureRegion:
      return command.angle_degrees >= 0 && command.angle_degrees <= 359;
  }
  return false;
}

bool RenderBatch::flush(Framebuffer& framebuffer) {
  if (!valid_ || framebuffer.width() <= 0 || framebuffer.height() <= 0) {
    return false;
  }
  for (std::size_t i = 0; i < commands_.size(); ++i) {
    if (!valid_command(commands_[i])) return false;
  }
  for (std::size_t i = 0; i < commands_.size(); ++i) {
    const Command& command = commands_[i];
    bool drawn = false;
    switch (command.type) {
      case kTextureRegion:
        drawn = framebuffer.draw_texture_region(
            *command.texture, command.source_x, command.source_y,
            command.source_width, command.source_height,
            command.destination_x, command.destination_y);
        break;
      case kScaledTextureRegion:
        drawn = framebuffer.draw_texture_region_scaled(
            *command.texture, command.source_x, command.source_y,
            command.source_width, command.source_height,
            command.destination_x, command.destination_y,
            command.destination_width, command.destination_height);
        break;
      case kRotatedTextureRegion:
        drawn = framebuffer.draw_texture_region_rotated(
            *command.texture, command.source_x, command.source_y,
            command.source_width, command.source_height,
            command.destination_x, command.destination_y,
            command.angle_degrees);
        break;
    }
    if (!drawn) return false;
  }
  commands_.clear();
  return true;
}

void RenderBatch::clear() { commands_.clear(); }

}  // namespace pc2ps3
