#include "pc2ps3/core.hpp"
#include "pc2ps3/input.hpp"
#include "pc2ps3/render.hpp"
#include "pc2ps3/runtime_loop.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

struct ScriptedFrame {
  pc2ps3::InputSnapshot snapshot;
  std::uint64_t elapsed_ns;

  ScriptedFrame() : snapshot(), elapsed_ns(0) {}
};

class ScriptedPlatform : public pc2ps3::RuntimePlatform {
public:
  ScriptedPlatform()
      : frames_(), next_frame_(0), present_count_(0), shutdown_count_(0) {
    ScriptedFrame first;
    first.snapshot.keyboard_right = true;
    first.elapsed_ns = 8000000;
    frames_.push_back(first);

    ScriptedFrame catch_up;
    catch_up.snapshot.keyboard_right = true;
    catch_up.snapshot.keyboard_jump = true;
    catch_up.elapsed_ns = 32000000;
    frames_.push_back(catch_up);

    ScriptedFrame no_tick;
    no_tick.elapsed_ns = 0;
    frames_.push_back(no_tick);
  }

  bool initialize(int width, int height) {
    return width == 320 && height == 180;
  }

  pc2ps3::RuntimePollStatus poll(pc2ps3::InputSnapshot& snapshot,
                                 std::uint64_t& elapsed_ns) {
    if (next_frame_ >= frames_.size()) {
      return pc2ps3::RuntimePollStatus::kQuit;
    }
    snapshot = frames_[next_frame_].snapshot;
    elapsed_ns = frames_[next_frame_].elapsed_ns;
    ++next_frame_;
    return pc2ps3::RuntimePollStatus::kFrame;
  }

  bool present(const pc2ps3::Framebuffer& framebuffer) {
    ++present_count_;
    return framebuffer.write_ppm("pc2ps3_runtime_demo.ppm");
  }

  void shutdown() { ++shutdown_count_; }

  std::size_t frame_count() const { return next_frame_; }
  int present_count() const { return present_count_; }
  int shutdown_count() const { return shutdown_count_; }

private:
  std::vector<ScriptedFrame> frames_;
  std::size_t next_frame_;
  int present_count_;
  int shutdown_count_;
};

pc2ps3::WorldConfig make_world_config() {
  return pc2ps3::WorldConfig{0.016f, 600.0f, 3000.0f, 80.0f, 150.0f, 150.0f};
}

class WorldApplication : public pc2ps3::RuntimeApplication {
public:
  WorldApplication()
      : world_(make_world_config()), updates_(0), jump_press_updates_(0),
        renders_(0), stop_count_(0), last_alpha_(0.0), last_render_position_{0, 0},
        started_(false) {}

  bool start() {
    world_.reset();
    updates_ = 0;
    jump_press_updates_ = 0;
    renders_ = 0;
    stop_count_ = 0;
    started_ = true;
    return true;
  }

  bool update(const pc2ps3::InputFrame& input, double fixed_dt_seconds) {
    if (!started_ || std::fabs(fixed_dt_seconds - 0.016) > 0.000001) {
      return false;
    }
    world_.step(input);
    ++updates_;
    if (input.jump_pressed) {
      ++jump_press_updates_;
    }
    return true;
  }

  bool render(pc2ps3::Framebuffer& framebuffer, double interpolation_alpha) {
    if (!started_ || !world_.interpolated_player_position(
                         interpolation_alpha, last_render_position_)) {
      return false;
    }
    framebuffer.clear(pc2ps3::Color{24, 32, 56, 255});
    framebuffer.fill_rect(0, 150, 320, 30, pc2ps3::Color{42, 92, 61, 255});
    framebuffer.draw_player(
        static_cast<int>(last_render_position_.x) - 12,
        static_cast<int>(last_render_position_.y) - 32,
        pc2ps3::Color{222, 82, 74, 255}, pc2ps3::Color{246, 214, 92, 255});
    ++renders_;
    last_alpha_ = interpolation_alpha;
    return true;
  }

  void stop() {
    ++stop_count_;
    started_ = false;
  }

  const pc2ps3::World& world() const { return world_; }
  int updates() const { return updates_; }
  int jump_press_updates() const { return jump_press_updates_; }
  int renders() const { return renders_; }
  int stop_count() const { return stop_count_; }
  double last_alpha() const { return last_alpha_; }
  const pc2ps3::Vec2& last_render_position() const {
    return last_render_position_;
  }

private:
  pc2ps3::World world_;
  int updates_;
  int jump_press_updates_;
  int renders_;
  int stop_count_;
  double last_alpha_;
  pc2ps3::Vec2 last_render_position_;
  bool started_;
};

} // namespace

int main() {
  ScriptedPlatform platform;
  WorldApplication application;
  pc2ps3::Framebuffer framebuffer(320, 180);
  const pc2ps3::FixedStepConfig timing(16000000, 64000000, 2);
  const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
      platform, application, framebuffer, timing);
  std::ifstream artifact("pc2ps3_runtime_demo.ppm", std::ios::binary);
  if (status != pc2ps3::RuntimeLoopStatus::kQuit || !artifact.good() ||
      application.updates() != 2 || application.renders() != 3 ||
      application.jump_press_updates() != 1 || application.world().tick() != 2 ||
      application.stop_count() != 1 || platform.frame_count() != 3 ||
      platform.present_count() != 3 || platform.shutdown_count() != 1 ||
      std::fabs(application.last_alpha() - 0.5) > 0.000001 ||
      std::fabs(application.last_render_position().x -
                application.world().player().position.x) < 0.01f) {
    std::cerr << "headless runtime demo failed\n";
    return 1;
  }

  std::cout << std::fixed << std::setprecision(3)
            << "{\"status\":\"quit\",\"frames\":"
            << platform.frame_count() << ",\"ticks\":"
            << application.world().tick() << ",\"jump_pressed_updates\":"
            << application.jump_press_updates() << ",\"presents\":"
            << platform.present_count() << ",\"last_alpha\":"
            << application.last_alpha() << ",\"player_x\":"
            << application.world().player().position.x << ",\"render_x\":"
            << application.last_render_position().x << "}\n";
  return 0;
}
