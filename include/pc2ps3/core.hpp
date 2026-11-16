#pragma once

#include "pc2ps3/jump_assist.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc2ps3 {

struct Vec2 {
  float x;
  float y;
};

struct Aabb {
  float left;
  float top;
  float right;
  float bottom;
};

struct InputFrame {
  bool left;
  bool right;
  bool jump_pressed;
  bool jump_held;
 bool up;
 bool down;
 float move_x;
 float move_y;
 bool confirm_pressed;
 bool confirm_held;
 bool attack_pressed;
 bool attack_held;
 bool cancel_pressed;
 bool cancel_held;
 bool ability_pressed;
 bool ability_held;
 bool l1_pressed;
 bool l1_held;
 bool r1_pressed;
 bool r1_held;
 bool pause_pressed;
 bool pause_held;
 bool menu_pressed;
 bool menu_held;
};

struct Actor {
  Vec2 position;
  Vec2 velocity;
  bool grounded;
};

struct WorldConfig {
  float fixed_dt;
  float gravity;
  float move_accel;
  float max_speed;
  float jump_speed;
  float floor_y;
};

class World {
 public:
  enum { kMaxMovingPlatforms = 128 };

  explicit World(const WorldConfig& config);
  void reset();
  // Teleports the player (checkpoint respawn): zero velocity, no interpolation.
  bool respawn(Vec2 position);
  void add_solid(const Aabb& solid);
  bool add_moving_platform(const Aabb& bounds, Vec2 velocity);
  bool apply_knockback(Vec2 impulse);
  void step(const InputFrame& input);
  const Actor& player() const;
  bool interpolated_player_position(double alpha, Vec2& output) const;
  std::uint64_t tick() const;

 private:
  Aabb player_bounds() const;
  void resolve_solids();
  void update_moving_platforms();
  void select_grounded_platform();

  struct MovingPlatform {
    MovingPlatform(const Aabb& initial, Vec2 speed)
        : initial_bounds(initial),
          bounds(initial),
          velocity(speed),
          delta{0.0f, 0.0f} {}

    Aabb initial_bounds;
    Aabb bounds;
    Vec2 velocity;
    Vec2 delta;
  };

  WorldConfig config_;
  Actor player_;
  Vec2 previous_player_position_;
  std::vector<Aabb> solids_;
  std::vector<MovingPlatform> moving_platforms_;
  std::uint64_t tick_;
  JumpAssist jump_assist_;
  bool grounded_on_platform_;
  std::size_t grounded_platform_index_;
};

}  // namespace pc2ps3
