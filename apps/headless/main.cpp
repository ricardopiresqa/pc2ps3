#include "pc2ps3/animation.hpp"
#include "pc2ps3/core.hpp"
#include "pc2ps3/input.hpp"
#include "pc2ps3/render.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

pc2ps3::Texture make_demo_atlas() {
  const int logical_frame_width = 24;
  const int logical_frame_height = 32;
  const int pixel_scale = 2;
  const int frame_width = logical_frame_width * pixel_scale;
  const int frame_height = logical_frame_height * pixel_scale;
  const int atlas_width = frame_width * 2;
  const pc2ps3::Color transparent = {0, 0, 0, 0};
  const pc2ps3::Color skin = {238, 190, 145, 255};
  const pc2ps3::Color body = {222, 82, 74, 255};
  const pc2ps3::Color trim = {246, 214, 92, 255};
  std::vector<pc2ps3::Color> pixels(
      static_cast<std::size_t>(atlas_width * frame_height), transparent);

  for (int frame = 0; frame < 2; ++frame) {
    const int frame_x = frame * frame_width;
    for (int y = 0; y < logical_frame_height; ++y) {
      for (int x = 0; x < logical_frame_width; ++x) {
        bool colored = false;
        pc2ps3::Color color = transparent;
        if (x >= 8 && x <= 15 && y >= 2 && y <= 9) {
          color = skin;
          colored = true;
        } else if (x >= 6 && x <= 17 && y >= 10 && y <= 23) {
          color = body;
          colored = true;
        } else if (y >= 12 && y <= 20 &&
                   ((frame == 0 && x >= 3 && x <= 5) ||
                    (frame == 1 && x >= 4 && x <= 6) ||
                    (x >= 18 && x <= 20))) {
          color = body;
          colored = true;
        } else if (y >= 24 && y <= 30 &&
                   ((frame == 0 && x >= 6 && x <= 10) ||
                    (frame == 1 && x >= 4 && x <= 9) ||
                    (frame == 0 && x >= 14 && x <= 18) ||
                    (frame == 1 && x >= 15 && x <= 20))) {
          color = trim;
          colored = true;
        }
        if (colored) {
          for (int sub_y = 0; sub_y < pixel_scale; ++sub_y) {
            for (int sub_x = 0; sub_x < pixel_scale; ++sub_x) {
              const int atlas_x = frame_x + x * pixel_scale + sub_x;
              const int atlas_y = y * pixel_scale + sub_y;
              const std::size_t index =
                  static_cast<std::size_t>(atlas_y) * atlas_width + atlas_x;
              pixels[index] = color;
            }
          }
        }
      }
    }
  }

  return pc2ps3::Texture(atlas_width, frame_height, pixels);
}

}  // namespace

int main() {
  const pc2ps3::WorldConfig config = {1.0f / 60.0f, 1500.0f, 2200.0f,
                                      280.0f, 560.0f, 360.0f};
  pc2ps3::World world(config);
  pc2ps3::InputMapper input_mapper;
  const pc2ps3::Texture atlas = make_demo_atlas();
  if (!atlas.valid()) {
    std::cerr << "failed to create the generated demo atlas\n";
    return 1;
  }

  const pc2ps3::AnimationClip run_clip(
      std::vector<pc2ps3::AnimationFrame>{
          {0, 0, 48, 64, 0.10}, {48, 0, 48, 64, 0.25}},
      true);
  pc2ps3::AnimationPlayer animation;
  if (!animation.play(run_clip)) {
    std::cerr << "failed to start the demo animation\n";
    return 1;
  }

  for (int tick = 0; tick < 180; ++tick) {
    pc2ps3::InputSnapshot snapshot;
    snapshot.keyboard_right = tick < 30;
    snapshot.keyboard_left = tick >= 30 && tick < 60;
    snapshot.keyboard_jump = tick >= 8 && tick < 20;
    const pc2ps3::InputFrame input = input_mapper.sample(snapshot);
    world.step(input);
    animation.update(config.fixed_dt);
  }

  const pc2ps3::Actor& player = world.player();
  pc2ps3::Framebuffer framebuffer(640, 360);
  framebuffer.clear(pc2ps3::Color{24, 32, 56, 255});
  framebuffer.fill_rect(0, 328, 640, 32, pc2ps3::Color{42, 92, 61, 255});
  if (!framebuffer.draw_animation_frame(
          atlas, animation, static_cast<int>(player.position.x) - 24,
          static_cast<int>(player.position.y) - 64)) {
    std::cerr << "failed to draw the animated demo player\n";
    return 1;
  }

  const double target_fps = 1.0 / static_cast<double>(config.fixed_dt);
  const std::uint64_t pixel_buffer_bytes =
      static_cast<std::uint64_t>(atlas.pixel_storage_bytes()) +
      static_cast<std::uint64_t>(framebuffer.pixel_storage_bytes());
  framebuffer.draw_debug_hud(8, 8, target_fps, pixel_buffer_bytes, 4);

  if (!framebuffer.write_ppm("prototype.ppm")) {
    std::cerr << "failed to write prototype.ppm\n";
    return 1;
  }

  std::cout << std::fixed << std::setprecision(3)
            << "{\"tick\":" << world.tick() << ",\"player\":{\"x\":"
            << player.position.x << ",\"y\":" << player.position.y
            << ",\"vx\":" << player.velocity.x << ",\"vy\":"
            << player.velocity.y << ",\"grounded\":"
            << (player.grounded ? "true" : "false")
            << "},\"animation_frame\":" << animation.current_index()
            << ",\"hud\":{\"target_fps\":" << target_fps
            << ",\"pixel_buffer_bytes\":" << pixel_buffer_bytes << "}}\n";
  return 0;
}
