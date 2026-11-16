#include "pc2ps3/camera.hpp"
#include "pc2ps3/checkpoint.hpp"
#include "pc2ps3/core.hpp"
#include "pc2ps3/enemy.hpp"
#include "pc2ps3/hitbox.hpp"
#include "pc2ps3/animation.hpp"
#include "pc2ps3/render.hpp"
#include "pc2ps3/state_machine.hpp"
#include "pc2ps3/trigger.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

namespace {
const std::uint32_t kPlayerId = 1;
const std::uint32_t kEnemyId = 2;
const std::uint32_t kCheckpointId = 10;
const std::uint32_t kAttackId = 100;
const std::uint32_t kIdle = 0;
const std::uint32_t kRun = 1;
const std::uint32_t kAttack = 2;
const std::uint32_t kVictory = 3;

pc2ps3::Aabb player_bounds(const pc2ps3::Actor& player) {
  return pc2ps3::Aabb{player.position.x - 12.0f, player.position.y - 32.0f,
                      player.position.x + 12.0f, player.position.y};
}

int screen_x(float world_x, int camera_x) {
  return static_cast<int>(std::floor(world_x - camera_x));
}

int screen_y(float world_y, int camera_y) {
  return static_cast<int>(std::floor(world_y - camera_y));
}

pc2ps3::Texture make_character_atlas() {
  const int frame_width = 24;
  const int frame_height = 32;
  const int frame_count = 4;
  const pc2ps3::Color transparent = {0, 0, 0, 0};
  const pc2ps3::Color skin = {236, 190, 144, 255};
  const pc2ps3::Color body = {72, 198, 146, 255};
  const pc2ps3::Color trim = {246, 214, 92, 255};
  std::vector<pc2ps3::Color> pixels(
      static_cast<std::size_t>(frame_width * frame_count * frame_height),
      transparent);
  for (int frame = 0; frame < frame_count; ++frame) {
    for (int y = 2; y <= 30; ++y) {
      for (int x = 5; x <= 18; ++x) {
        pc2ps3::Color color = transparent;
        bool colored = false;
        if (y <= 8 && x >= 9 && x <= 14) {
          color = skin;
          colored = true;
        } else if (y >= 9 && y <= 22 && x >= 7 && x <= 16) {
          color = body;
          colored = true;
        } else if (y >= 23 &&
                   ((frame % 2 == 0 && x >= 7 && x <= 11) ||
                    (frame % 2 == 0 && x >= 13 && x <= 17) ||
                    (frame % 2 == 1 && x >= 5 && x <= 10) ||
                    (frame % 2 == 1 && x >= 14 && x <= 18))) {
          color = frame >= 2 ? skin : trim;
          colored = true;
        } else if (frame >= 2 && y >= 13 && y <= 17 && x >= 17 && x <= 21) {
          color = trim;
          colored = true;
        }
        if (colored) {
          const std::size_t index =
              static_cast<std::size_t>(y) * frame_width * frame_count +
              frame * frame_width + x;
          pixels[index] = color;
        }
      }
    }
  }
  return pc2ps3::Texture(frame_width * frame_count, frame_height,
                         std::move(pixels));
}
}  // namespace

int main() {
  const pc2ps3::WorldConfig config = {1.0f / 60.0f, 1200.0f, 1800.0f,
                                      240.0f, 420.0f, 320.0f};
  pc2ps3::World world(config);
  pc2ps3::StateMachine states(4, kIdle);
  states.allow_transition(kIdle, kRun);
  states.allow_transition(kIdle, kAttack);
  states.allow_transition(kIdle, kVictory);
  states.allow_transition(kRun, kIdle);
  states.allow_transition(kRun, kAttack);
  states.allow_transition(kRun, kVictory);
  states.allow_transition(kAttack, kIdle);
  states.allow_transition(kAttack, kRun);
  states.allow_transition(kAttack, kVictory);

  pc2ps3::EnemySystem enemies(4);
  pc2ps3::HitboxSystem combat(2, 2);
  pc2ps3::TriggerSystem triggers(1);
  pc2ps3::CheckpointSystem checkpoints(1, pc2ps3::Vec2{64.0f, 320.0f});
  pc2ps3::Camera2D camera(320, 180);
  const pc2ps3::Texture atlas = make_character_atlas();
  const std::vector<pc2ps3::AnimationFrame> run_frames = {
      pc2ps3::AnimationFrame{0, 0, 24, 32, 0.10},
      pc2ps3::AnimationFrame{24, 0, 24, 32, 0.10}};
  const std::vector<pc2ps3::AnimationFrame> attack_frames = {
      pc2ps3::AnimationFrame{48, 0, 24, 32, 0.05},
      pc2ps3::AnimationFrame{72, 0, 24, 32, 0.05}};
  const pc2ps3::AnimationClip run_clip(run_frames, true);
  const pc2ps3::AnimationClip attack_clip(attack_frames, false);
  pc2ps3::AnimationPlayer animation;
  if (!enemies.valid() || !combat.valid() || !triggers.valid() ||
      !checkpoints.valid() || !camera.valid() || !atlas.valid() ||
      !run_clip.valid() || !attack_clip.valid() || !animation.play(run_clip) ||
      !camera.set_world_bounds(pc2ps3::Aabb{0.0f, 0.0f, 640.0f, 360.0f}) ||
      !enemies.spawn(kEnemyId, pc2ps3::Aabb{198.0f, 288.0f, 222.0f, 320.0f},
                     10) ||
      !triggers.add(kCheckpointId,
                    pc2ps3::Aabb{90.0f, 286.0f, 108.0f, 320.0f}) ||
      !checkpoints.add(kCheckpointId, pc2ps3::Vec2{96.0f, 320.0f})) {
    std::cerr << "prototype initialization failed\n";
    return 1;
  }

  bool enemy_defeated = false;
  for (std::uint32_t tick = 0; tick < 60; ++tick) {
    const bool attacking = tick == 30;
    const pc2ps3::InputFrame input = {false, tick < 30, false, false};
    world.step(input);

    const pc2ps3::Aabb actor = player_bounds(world.player());
    if (!triggers.update(actor)) {
      std::cerr << "trigger update failed\n";
      return 1;
    }
    for (std::size_t i = 0; i < triggers.events().size(); ++i) {
      if (triggers.events()[i].type == pc2ps3::TriggerEventType::kEnter) {
        checkpoints.activate(triggers.events()[i].trigger_id);
      }
    }

    combat.begin_tick();
    pc2ps3::EnemyState enemy = {0, pc2ps3::Aabb{0, 0, 0, 0}, 0};
    if (enemies.get(kEnemyId, enemy)) {
      combat.add_hurtbox(kEnemyId, kEnemyId, enemy.bounds);
    }
    if (attacking) {
      states.transition_to(kAttack);
      animation.play(attack_clip);
      combat.add_hitbox(
          kAttackId, kPlayerId,
          pc2ps3::Aabb{world.player().position.x + 10.0f,
                       world.player().position.y - 28.0f,
                       world.player().position.x + 34.0f,
                       world.player().position.y - 4.0f});
    } else if (states.current_state() != kVictory) {
      states.transition_to(input.right ? kRun : kIdle);
    }
    if (!combat.update()) {
      std::cerr << "combat update failed\n";
      return 1;
    }
    animation.update(config.fixed_dt);
    for (std::size_t i = 0; i < combat.events().size(); ++i) {
      if (combat.events()[i].attack_id == kAttackId &&
          combat.events()[i].target_id == kEnemyId) {
        bool defeated = false;
        if (enemies.damage(kEnemyId, 10, defeated) && defeated) {
          enemy_defeated = true;
          states.transition_to(kVictory);
        }
      }
    }

    camera.follow(pc2ps3::Vec2{world.player().position.x,
                               world.player().position.y - 16.0f});
  }

  std::uint32_t checkpoint_id = 0;
  const bool checkpoint_active =
      checkpoints.active_checkpoint_id(checkpoint_id);
  pc2ps3::EnemyState enemy = {0, pc2ps3::Aabb{0, 0, 0, 0}, 0};
  const bool enemy_alive = enemies.get(kEnemyId, enemy);

  pc2ps3::Framebuffer framebuffer(320, 180);
  framebuffer.clear(pc2ps3::Color{18, 24, 38, 255});
  const pc2ps3::Color ground_color = {70, 84, 104, 255};
  const pc2ps3::Color player_color = {72, 198, 146, 255};
  const pc2ps3::Color enemy_color = {226, 86, 90, 255};
  const int ground_y = screen_y(320.0f, camera.y());
  framebuffer.fill_rect(0, ground_y, 320, 180 - ground_y, ground_color);
  if (enemy_alive) {
    framebuffer.fill_rect(screen_x(enemy.bounds.left, camera.x()),
                          screen_y(enemy.bounds.top, camera.y()),
                          static_cast<int>(enemy.bounds.right - enemy.bounds.left),
                          static_cast<int>(enemy.bounds.bottom - enemy.bounds.top),
                          enemy_color);
  }
  if (!framebuffer.draw_animation_frame(
          atlas, animation,
          screen_x(world.player().position.x - 12.0f, camera.x()),
          screen_y(world.player().position.y - 32.0f, camera.y()))) {
    std::cerr << "failed to draw prototype animation\n";
    return 1;
  }
  if (!framebuffer.write_ppm("prototype.ppm")) {
    std::cerr << "failed to write prototype.ppm\n";
    return 1;
  }

  std::cout << std::fixed << std::setprecision(3)
            << "{\"tick\":" << world.tick() << ",\"state\":"
            << states.current_state() << ",\"player_x\":"
            << world.player().position.x << ",\"enemy_alive\":"
            << (enemy_alive ? "true" : "false") << ",\"enemy_defeated\":"
            << (enemy_defeated ? "true" : "false")
            << ",\"checkpoint_active\":"
            << (checkpoint_active ? "true" : "false")
            << ",\"checkpoint_id\":" << checkpoint_id << ",\"camera_x\":"
            << camera.x() << ",\"camera_y\":" << camera.y()
            << ",\"animation_frame\":" << animation.current_index()
            << ",\"image\":\"prototype.ppm\"}\n";
  return enemy_defeated && checkpoint_active && states.current_state() == kVictory
             ? 0
             : 1;
}
