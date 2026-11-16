#include "pc2ps3/render.hpp"
#include "pc2ps3/animation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace pc2ps3 {
namespace {

struct Glyph3x5 {
  char character;
  std::uint8_t rows[5];
};

const Glyph3x5 kBitmapFont[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}},
    {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 7, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}},
    {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}},
    {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
    {'C', {3, 4, 4, 4, 3}}, {'D', {6, 5, 5, 5, 6}},
    {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'G', {3, 4, 5, 5, 3}}, {'H', {5, 5, 7, 5, 5}},
    {'I', {7, 2, 2, 2, 7}}, {'J', {1, 1, 1, 5, 2}},
    {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}},
    {'M', {5, 7, 7, 5, 5}}, {'N', {5, 7, 7, 7, 5}},
    {'O', {2, 5, 5, 5, 2}}, {'P', {6, 5, 6, 4, 4}},
    {'Q', {2, 5, 5, 7, 3}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {7, 4, 7, 1, 7}}, {'T', {7, 2, 2, 2, 2}},
    {'U', {5, 5, 5, 5, 7}}, {'V', {5, 5, 5, 5, 2}},
    {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}},
    {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}},
    {'!', {2, 2, 2, 0, 2}}, {'%', {5, 1, 2, 4, 5}},
    {':', {0, 2, 0, 2, 0}}, {'.', {0, 0, 0, 0, 2}},
    {'?', {7, 1, 2, 0, 2}}, {'-', {0, 0, 7, 0, 0}},
    {'/', {1, 1, 2, 4, 4}}, {'_', {0, 0, 0, 0, 7}},
};

const Glyph3x5* find_glyph(char character) {
  for (std::size_t i = 0; i < sizeof(kBitmapFont) / sizeof(kBitmapFont[0]); ++i) {
    if (kBitmapFont[i].character == character) return &kBitmapFont[i];
  }
  return 0;
}

int saturated_int(std::int64_t value) {
  if (value < std::numeric_limits<int>::min()) {
    return std::numeric_limits<int>::min();
  }
  if (value > std::numeric_limits<int>::max()) {
    return std::numeric_limits<int>::max();
  }
  return static_cast<int>(value);
}

void draw_hud_text(Framebuffer& framebuffer, const char* text, int x, int y,
                   Color color, int scale) {
  if (text == 0) return;
  framebuffer.draw_text(text, std::strlen(text), x, y, color, scale);
}

void blend_over(Color& destination, const Color& source) {
  const unsigned int source_alpha = source.a;
  if (source_alpha == 0) return;
  if (source_alpha == 255) {
    destination = source;
    return;
  }

  const unsigned int destination_alpha = destination.a;
  const unsigned int inverse_source_alpha = 255 - source_alpha;
  if (destination_alpha == 255) {  // opaque framebuffer (the common case): same rounding, no division by a variable
    destination.r = static_cast<std::uint8_t>(
        (2 * (source.r * source_alpha + destination.r * inverse_source_alpha) + 255) / 510);
    destination.g = static_cast<std::uint8_t>(
        (2 * (source.g * source_alpha + destination.g * inverse_source_alpha) + 255) / 510);
    destination.b = static_cast<std::uint8_t>(
        (2 * (source.b * source_alpha + destination.b * inverse_source_alpha) + 255) / 510);
    return;
  }
  const unsigned int output_alpha_numerator =
      source_alpha * 255 + destination_alpha * inverse_source_alpha;
  if (output_alpha_numerator == 0) {
    destination = Color{0, 0, 0, 0};
    return;
  }

  const unsigned int red_numerator =
      static_cast<unsigned int>(source.r) * source_alpha * 255 +
      static_cast<unsigned int>(destination.r) * destination_alpha *
          inverse_source_alpha;
  const unsigned int green_numerator =
      static_cast<unsigned int>(source.g) * source_alpha * 255 +
      static_cast<unsigned int>(destination.g) * destination_alpha *
          inverse_source_alpha;
  const unsigned int blue_numerator =
      static_cast<unsigned int>(source.b) * source_alpha * 255 +
      static_cast<unsigned int>(destination.b) * destination_alpha *
          inverse_source_alpha;
  const unsigned int half_denominator = output_alpha_numerator / 2;

  destination.r = static_cast<std::uint8_t>(
      (red_numerator + half_denominator) / output_alpha_numerator);
  destination.g = static_cast<std::uint8_t>(
      (green_numerator + half_denominator) / output_alpha_numerator);
  destination.b = static_cast<std::uint8_t>(
      (blue_numerator + half_denominator) / output_alpha_numerator);
  destination.a = static_cast<std::uint8_t>(
      (output_alpha_numerator + 127) / 255);
}

}  // namespace

Texture::Texture() : width_(0), height_(0) {}

Texture::Texture(int width, int height, const std::vector<Color>& pixels)
    : width_(0), height_(0) {
  if (width <= 0 || height <= 0) return;

  const std::size_t width_size = static_cast<std::size_t>(width);
  const std::size_t height_size = static_cast<std::size_t>(height);
  if (width_size > pixels_.max_size() / height_size) return;
  const std::size_t pixel_count = width_size * height_size;
  if (pixels.size() != pixel_count) return;

  pixels_ = pixels;
  width_ = width;
  height_ = height;
}

Texture::Texture(int width, int height, std::vector<Color>&& pixels)
    : width_(0), height_(0) {
  if (width <= 0 || height <= 0) return;

  const std::size_t width_size = static_cast<std::size_t>(width);
  const std::size_t height_size = static_cast<std::size_t>(height);
  if (width_size > pixels_.max_size() / height_size) return;
  const std::size_t pixel_count = width_size * height_size;
  if (pixels.size() != pixel_count) return;

  pixels_ = std::move(pixels);
  width_ = width;
  height_ = height;
}

bool Texture::valid() const {
  return width_ > 0 && height_ > 0 && !pixels_.empty();
}

int Texture::width() const { return width_; }

int Texture::height() const { return height_; }

std::size_t Texture::pixel_storage_bytes() const {
  return pixels_.capacity() * sizeof(Color);
}

const Color* Texture::pixel_at(int x, int y) const {
  if (!valid() || x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
  const std::size_t index =
      static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
      static_cast<std::size_t>(x);
  return &pixels_[index];
}

Framebuffer::Framebuffer(int width, int height)
    : width_(width), height_(height) {
  if (width <= 0 || height <= 0) {
    width_ = 0;
    height_ = 0;
    return;
  }

  const std::size_t width_size = static_cast<std::size_t>(width_);
  const std::size_t height_size = static_cast<std::size_t>(height_);
  if (width_size > pixels_.max_size() / height_size) {
    width_ = 0;
    height_ = 0;
    return;
  }
  pixels_.resize(width_size * height_size);
}

void Framebuffer::clear(Color color) {
  std::fill(pixels_.begin(), pixels_.end(), color);
}

void Framebuffer::put_pixel(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
          static_cast<std::size_t>(x)] = color;
}

void Framebuffer::fill_rect(int x, int y, int width, int height, Color color) {
  if (width <= 0 || height <= 0 || width_ == 0 || height_ == 0) return;

  const std::int64_t left = std::max<std::int64_t>(0, x);
  const std::int64_t top = std::max<std::int64_t>(0, y);
  const std::int64_t right = std::min<std::int64_t>(
      width_, static_cast<std::int64_t>(x) + width);
  const std::int64_t bottom = std::min<std::int64_t>(
      height_, static_cast<std::int64_t>(y) + height);
  if (right <= left || bottom <= top) return;

  for (std::int64_t py = top; py < bottom; ++py) {
    for (std::int64_t px = left; px < right; ++px) {
      put_pixel(static_cast<int>(px), static_cast<int>(py), color);
    }
  }
}

void Framebuffer::blend_rect(int x, int y, int width, int height, Color color) {
  if (width <= 0 || height <= 0 || width_ == 0 || height_ == 0) return;

  const std::int64_t left = std::max<std::int64_t>(0, x);
  const std::int64_t top = std::max<std::int64_t>(0, y);
  const std::int64_t right = std::min<std::int64_t>(
      width_, static_cast<std::int64_t>(x) + width);
  const std::int64_t bottom = std::min<std::int64_t>(
      height_, static_cast<std::int64_t>(y) + height);
  if (right <= left || bottom <= top) return;

  for (std::int64_t py = top; py < bottom; ++py) {
    for (std::int64_t px = left; px < right; ++px) {
      Color& destination =
          pixels_[static_cast<std::size_t>(py) * static_cast<std::size_t>(width_) +
                  static_cast<std::size_t>(px)];
      blend_over(destination, color);
    }
  }
}

void Framebuffer::draw_player(int x, int y, Color body, Color accent) {
  fill_rect(x - 12, y - 32, 24, 32, body);
  fill_rect(x - 8, y - 38, 16, 8, accent);
  fill_rect(x - 9, y - 10, 6, 4, accent);
  fill_rect(x + 3, y - 10, 6, 4, accent);
}

void Framebuffer::blend_overlay(Color color) {
  blend_rect(0, 0, width_, height_, color);
}

bool Framebuffer::draw_text(const char* text, std::size_t length, int x, int y,
                            Color color, int scale) {
  if ((text == 0 && length != 0) || length > kMaxTextBytes || scale < 1 ||
      scale > 8) {
    return false;
  }
  const std::int64_t line_start_x = x;
  std::int64_t cursor_x = x;
  std::int64_t cursor_y = y;
  for (std::size_t i = 0; i < length; ++i) {
    unsigned char character = static_cast<unsigned char>(text[i]);
    if (character == '\n') {
      cursor_x = line_start_x;
      cursor_y += 6 * scale;
      continue;
    }
    if (character == '\r') continue;
    if (character == '\t') {
      cursor_x += 16 * scale;
      continue;
    }
    if (character >= 'a' && character <= 'z') {
      character = static_cast<unsigned char>(character - 'a' + 'A');
    }
    if (character == ' ') {
      cursor_x += 4 * scale;
      continue;
    }
    const Glyph3x5* glyph = find_glyph(static_cast<char>(character));
    if (glyph == 0) glyph = find_glyph('?');
    for (int row = 0; row < 5; ++row) {
      for (int column = 0; column < 3; ++column) {
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << (2 - column));
        if ((glyph->rows[row] & mask) == 0) continue;
        const std::int64_t pixel_x = cursor_x + column * scale;
        const std::int64_t pixel_y = cursor_y + row * scale;
        fill_rect(saturated_int(pixel_x), saturated_int(pixel_y), scale, scale,
                  color);
      }
    }
    cursor_x += 4 * scale;
  }
  return true;
}

bool Framebuffer::draw_texture_region(const Texture& texture, int source_x,
                                     int source_y, int source_width,
                                     int source_height, int destination_x,
                                     int destination_y) {
  if (!texture.valid() || source_width <= 0 || source_height <= 0) return false;

  const std::int64_t source_right =
      static_cast<std::int64_t>(source_x) + source_width;
  const std::int64_t source_bottom =
      static_cast<std::int64_t>(source_y) + source_height;
  if (source_x < 0 || source_y < 0 || source_right > texture.width() ||
      source_bottom > texture.height()) {
    return false;
  }

  const std::int64_t left = std::max<std::int64_t>(0, destination_x);
  const std::int64_t top = std::max<std::int64_t>(0, destination_y);
  const std::int64_t right = std::min<std::int64_t>(
      width_, static_cast<std::int64_t>(destination_x) + source_width);
  const std::int64_t bottom = std::min<std::int64_t>(
      height_, static_cast<std::int64_t>(destination_y) + source_height);
  if (right <= left || bottom <= top) return true;

  // The source rectangle was bounds-checked above, so rows are walked with plain pointers.
  for (std::int64_t y = top; y < bottom; ++y) {
    const Color* source_pixel = texture.pixel_at(
        static_cast<int>(source_x + left - destination_x),
        static_cast<int>(source_y + y - destination_y));
    Color* destination = &pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                                  static_cast<std::size_t>(left)];
    for (std::int64_t x = left; x < right; ++x) blend_over(*destination++, *source_pixel++);
  }
  return true;
}

bool Framebuffer::draw_texture_region_scaled(
    const Texture& texture, int source_x, int source_y, int source_width,
    int source_height, int destination_x, int destination_y,
    int destination_width, int destination_height, bool flip_x) {
  if (!texture.valid() || source_width <= 0 || source_height <= 0 ||
      destination_width <= 0 || destination_height <= 0) {
    return false;
  }
  const std::int64_t source_right =
      static_cast<std::int64_t>(source_x) + source_width;
  const std::int64_t source_bottom =
      static_cast<std::int64_t>(source_y) + source_height;
  if (source_x < 0 || source_y < 0 || source_right > texture.width() ||
      source_bottom > texture.height()) {
    return false;
  }

  const std::int64_t left = std::max<std::int64_t>(0, destination_x);
  const std::int64_t top = std::max<std::int64_t>(0, destination_y);
  const std::int64_t right = std::min<std::int64_t>(
      width_, static_cast<std::int64_t>(destination_x) + destination_width);
  const std::int64_t bottom = std::min<std::int64_t>(
      height_, static_cast<std::int64_t>(destination_y) + destination_height);
  if (right <= left || bottom <= top) return true;

  for (std::int64_t y = top; y < bottom; ++y) {
    const std::int64_t local_y = y - destination_y;
    const int source_row = source_y + static_cast<int>(
        (local_y * source_height) / destination_height);
    for (std::int64_t x = left; x < right; ++x) {
      const std::int64_t local_x = flip_x ? destination_x + destination_width - 1 - x
                                          : x - destination_x;
      const int source_column = source_x + static_cast<int>(
          (local_x * source_width) / destination_width);
      const Color* source = texture.pixel_at(source_column, source_row);
      if (source == 0) return false;
      Color& destination =
          pixels_[static_cast<std::size_t>(y) *
                      static_cast<std::size_t>(width_) +
                  static_cast<std::size_t>(x)];
      blend_over(destination, *source);
    }
  }
  return true;
}

bool Framebuffer::draw_texture_region_rotated(
    const Texture& texture, int source_x, int source_y, int source_width,
    int source_height, int destination_x, int destination_y,
    int angle_degrees) {
  if (!texture.valid() || source_width <= 0 || source_height <= 0 ||
      angle_degrees < 0 || angle_degrees > 359) {
    return false;
  }
  const std::int64_t source_right =
      static_cast<std::int64_t>(source_x) + source_width;
  const std::int64_t source_bottom =
      static_cast<std::int64_t>(source_y) + source_height;
  if (source_x < 0 || source_y < 0 || source_right > texture.width() ||
      source_bottom > texture.height()) {
    return false;
  }
  if (angle_degrees == 0) {
    return draw_texture_region(texture, source_x, source_y, source_width,
                               source_height, destination_x, destination_y);
  }

  const auto blend_source_pixel = [this, &texture](
      std::int64_t destination_pixel_x, std::int64_t destination_pixel_y,
      int source_pixel_x, int source_pixel_y) {
    if (destination_pixel_x < 0 || destination_pixel_y < 0 ||
        destination_pixel_x >= width_ || destination_pixel_y >= height_) {
      return;
    }
    const Color* source = texture.pixel_at(source_pixel_x, source_pixel_y);
    if (source == 0) return;
    Color& destination =
        pixels_[static_cast<std::size_t>(destination_pixel_y) *
                    static_cast<std::size_t>(width_) +
                static_cast<std::size_t>(destination_pixel_x)];
    blend_over(destination, *source);
  };

  if (angle_degrees == 90 || angle_degrees == 180 ||
      angle_degrees == 270) {
    const bool quarter_turn = angle_degrees != 180;
    const std::int64_t output_width = quarter_turn ? source_height : source_width;
    const std::int64_t output_height = quarter_turn ? source_width : source_height;
    const auto floor_half = [](std::int64_t value) -> std::int64_t {
      return value >= 0 ? value / 2 : (value - 1) / 2;
    };
    const std::int64_t left = quarter_turn
        ? floor_half(static_cast<std::int64_t>(destination_x) * 2 +
                     source_width - output_width)
        : destination_x;
    const std::int64_t top = quarter_turn
        ? floor_half(static_cast<std::int64_t>(destination_y) * 2 +
                     source_height - output_height)
        : destination_y;
    for (int y = 0; y < source_height; ++y) {
      for (int x = 0; x < source_width; ++x) {
        std::int64_t output_x = 0;
        std::int64_t output_y = 0;
        if (angle_degrees == 90) {
          output_x = source_height - 1 - y;
          output_y = x;
        } else if (angle_degrees == 180) {
          output_x = source_width - 1 - x;
          output_y = source_height - 1 - y;
        } else {
          output_x = y;
          output_y = source_width - 1 - x;
        }
        blend_source_pixel(left + output_x, top + output_y,
                           source_x + x, source_y + y);
      }
    }
    return true;
  }

  const double pi = 3.14159265358979323846;
  const double radians = static_cast<double>(angle_degrees) * pi / 180.0;
  const double cosine = std::cos(radians);
  const double sine = std::sin(radians);
  const double center_x = static_cast<double>(destination_x) +
                          static_cast<double>(source_width) / 2.0;
  const double center_y = static_cast<double>(destination_y) +
                          static_cast<double>(source_height) / 2.0;
  const double half_width =
      std::fabs(cosine) * source_width / 2.0 +
      std::fabs(sine) * source_height / 2.0;
  const double half_height =
      std::fabs(sine) * source_width / 2.0 +
      std::fabs(cosine) * source_height / 2.0;
  const std::int64_t bounds_left = static_cast<std::int64_t>(
      std::floor(center_x - half_width));
  const std::int64_t bounds_top = static_cast<std::int64_t>(
      std::floor(center_y - half_height));
  const std::int64_t bounds_right = static_cast<std::int64_t>(
      std::ceil(center_x + half_width));
  const std::int64_t bounds_bottom = static_cast<std::int64_t>(
      std::ceil(center_y + half_height));
  const std::int64_t left = std::max<std::int64_t>(0, bounds_left);
  const std::int64_t top = std::max<std::int64_t>(0, bounds_top);
  const std::int64_t right = std::min<std::int64_t>(width_, bounds_right);
  const std::int64_t bottom = std::min<std::int64_t>(height_, bounds_bottom);
  if (right <= left || bottom <= top) return true;

  for (std::int64_t y = top; y < bottom; ++y) {
    const double relative_y = static_cast<double>(y) + 0.5 - center_y;
    for (std::int64_t x = left; x < right; ++x) {
      const double relative_x = static_cast<double>(x) + 0.5 - center_x;
      const double source_edge_x =
          cosine * relative_x + sine * relative_y + source_width / 2.0;
      const double source_edge_y =
          -sine * relative_x + cosine * relative_y + source_height / 2.0;
      if (source_edge_x < 0.0 || source_edge_y < 0.0 ||
          source_edge_x >= source_width || source_edge_y >= source_height) {
        continue;
      }
      const int source_pixel_x =
          source_x + static_cast<int>(std::floor(source_edge_x));
      const int source_pixel_y =
          source_y + static_cast<int>(std::floor(source_edge_y));
      blend_source_pixel(x, y, source_pixel_x, source_pixel_y);
    }
  }
  return true;
}

bool Framebuffer::draw_animation_frame(const Texture& texture,
                                      const AnimationPlayer& animation,
                                      int destination_x,
                                      int destination_y) {
  const AnimationFrame* frame = animation.current_frame();
  if (frame == 0) return false;
  return draw_texture_region(texture, frame->x, frame->y, frame->width,
                             frame->height, destination_x, destination_y);
}

void Framebuffer::draw_debug_hud(int x, int y, double frames_per_second,
                                 std::uint64_t memory_bytes, int scale) {
  const int font_scale = std::max(1, std::min(scale, 8));
  unsigned int fps = 0;
  if (std::isfinite(frames_per_second) && frames_per_second >= 0.0) {
    const double bounded_fps = std::min(frames_per_second, 9999.0);
    fps = static_cast<unsigned int>(std::floor(bounded_fps + 0.5));
  }

  char fps_text[16];
  char memory_text[32];
  std::snprintf(fps_text, sizeof(fps_text), "FPS:%u", fps);
  std::snprintf(memory_text, sizeof(memory_text), "MEM:%lluB",
                static_cast<unsigned long long>(memory_bytes));

  const std::size_t fps_length = std::strlen(fps_text);
  const std::size_t memory_length = std::strlen(memory_text);
  const std::size_t line_length = std::max(fps_length, memory_length);
  const int panel_width = saturated_int(
      static_cast<std::int64_t>(line_length) * 4 * font_scale +
      4 * font_scale);
  const int panel_height = 13 * font_scale;
  const Color panel_color = {12, 16, 24, 255};
  const Color text_color = {236, 240, 248, 255};
  fill_rect(x, y, panel_width, panel_height, panel_color);

  const int text_x =
      saturated_int(static_cast<std::int64_t>(x) + 2 * font_scale);
  const int first_line_y =
      saturated_int(static_cast<std::int64_t>(y) + 2 * font_scale);
  const int second_line_y =
      saturated_int(static_cast<std::int64_t>(first_line_y) + 6 * font_scale);
  draw_hud_text(*this, fps_text, text_x, first_line_y, text_color, font_scale);
  draw_hud_text(*this, memory_text, text_x, second_line_y, text_color,
                font_scale);
}

bool Framebuffer::read_pixel(int x, int y, Color& color) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return false;
  color = pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                  static_cast<std::size_t>(x)];
  return true;
}

bool Framebuffer::write_ppm(const std::string& path) const {
  std::ofstream output(path.c_str(), std::ios::binary);
  if (!output) return false;
  output << "P6\n" << width_ << " " << height_ << "\n255\n";
  for (std::size_t i = 0; i < pixels_.size(); ++i) {
    output.put(static_cast<char>(pixels_[i].r));
    output.put(static_cast<char>(pixels_[i].g));
    output.put(static_cast<char>(pixels_[i].b));
  }
  return output.good();
}

int Framebuffer::width() const { return width_; }

int Framebuffer::height() const { return height_; }

std::size_t Framebuffer::pixel_storage_bytes() const {
  return pixels_.capacity() * sizeof(Color);
}

const Color* Framebuffer::pixel_data() const {
  return pixels_.empty() ? 0 : &pixels_[0];
}

}  // namespace pc2ps3
