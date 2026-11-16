#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pc2ps3 {

class TextureManager;

struct Color {
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
  std::uint8_t a;
};

class Texture {
 public:
  Texture();
  Texture(int width, int height, const std::vector<Color>& pixels);
  Texture(int width, int height, std::vector<Color>&& pixels);
  bool valid() const;
  int width() const;
  int height() const;
  std::size_t pixel_storage_bytes() const;
  const Color* pixel_at(int x, int y) const;

 private:
friend class TextureManager;

  int width_;
  int height_;
  std::vector<Color> pixels_;
};

class AnimationPlayer;

class Framebuffer {
 public:
 enum { kMaxTextBytes = 1024 };

  Framebuffer(int width, int height);
  void clear(Color color);
  void fill_rect(int x, int y, int width, int height, Color color);
  void blend_rect(int x, int y, int width, int height, Color color);
 void blend_overlay(Color color);
 bool draw_text(const char* text, std::size_t length, int x, int y,
                Color color, int scale = 1);
  void draw_player(int x, int y, Color body, Color accent);
  bool draw_texture_region(const Texture& texture, int source_x, int source_y,
                           int source_width, int source_height,
                           int destination_x, int destination_y);
 bool draw_texture_region_scaled(const Texture& texture, int source_x,
                                 int source_y, int source_width,
                                 int source_height, int destination_x,
                                 int destination_y, int destination_width,
                                 int destination_height, bool flip_x = false);
 bool draw_texture_region_rotated(const Texture& texture, int source_x,
                                  int source_y, int source_width,
                                  int source_height, int destination_x,
                                  int destination_y, int angle_degrees);
  bool draw_animation_frame(const Texture& texture,
                            const AnimationPlayer& animation,
                            int destination_x, int destination_y);
  void draw_debug_hud(int x, int y, double frames_per_second,
                      std::uint64_t memory_bytes, int scale = 2);
  bool read_pixel(int x, int y, Color& color) const;
  bool write_ppm(const std::string& path) const;
  int width() const;
  int height() const;
  std::size_t pixel_storage_bytes() const;
  const Color* pixel_data() const;

 private:
  void put_pixel(int x, int y, Color color);
  int width_;
  int height_;
  std::vector<Color> pixels_;
};

}  // namespace pc2ps3
