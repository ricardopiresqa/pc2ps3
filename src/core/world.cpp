#include "pc2ps3/core.hpp"
#include "pc2ps3/knockback.hpp"
#include "pc2ps3/physics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace pc2ps3 {

namespace {

const float kPlayerHalfWidth = 12.0f;
const float kPlayerHeight = 32.0f;
const unsigned int kMaxCollisionPasses = 4;

}  // namespace

World::World(const WorldConfig& config)
    : config_(config),
      tick_(0),
      grounded_on_platform_(false),
      grounded_platform_index_(0) {
  reset();
}

void World::reset() {
  player_.position = Vec2{64.0f, config_.floor_y};
  previous_player_position_ = player_.position;
  player_.velocity = Vec2{0.0f, 0.0f};
  player_.grounded = true;
  tick_ = 0;
  jump_assist_.reset();
  for (std::size_t i = 0; i < moving_platforms_.size(); ++i) {
    moving_platforms_[i].bounds = moving_platforms_[i].initial_bounds;
    moving_platforms_[i].delta = Vec2{0.0f, 0.0f};
  }
  grounded_on_platform_ = false;
  select_grounded_platform();
}

bool World::respawn(Vec2 position) {
  if (!std::isfinite(position.x) || !std::isfinite(position.y)) return false;
  player_.position = position;
  previous_player_position_ = position;  // no interpolation streak across the teleport
  player_.velocity = Vec2{0.0f, 0.0f};
  player_.grounded = position.y >= config_.floor_y;
  jump_assist_.reset();
  grounded_on_platform_ = false;
  select_grounded_platform();
  return true;
}

void World::add_solid(const Aabb& solid) { solids_.push_back(solid); }

bool World::add_moving_platform(const Aabb& bounds, Vec2 velocity) {
  if (moving_platforms_.size() >= kMaxMovingPlatforms ||
      !query_aabb(bounds, bounds).overlapping || !std::isfinite(velocity.x) ||
      !std::isfinite(velocity.y)) {
    return false;
  }
  try {
    moving_platforms_.push_back(MovingPlatform(bounds, velocity));
  } catch (const std::bad_alloc&) {
    return false;
  }
  select_grounded_platform();
  return true;
}

bool World::apply_knockback(Vec2 impulse) {
  return apply_knockback_impulse(player_, impulse);
}

Aabb World::player_bounds() const {
  return Aabb{player_.position.x - kPlayerHalfWidth,
              player_.position.y - kPlayerHeight,
              player_.position.x + kPlayerHalfWidth,
              player_.position.y};
}

void World::resolve_solids() {
  for (unsigned int pass = 0; pass < kMaxCollisionPasses; ++pass) {
    bool resolved = false;
    const auto resolve_contact = [this, &resolved](const AabbContact& contact) {
      if (!contact.overlapping) return;
      player_.position.x -= contact.normal.x * contact.penetration;
      player_.position.y -= contact.normal.y * contact.penetration;
      if (contact.normal.x != 0.0f) player_.velocity.x = 0.0f;
      if (contact.normal.y != 0.0f) {
        player_.velocity.y = 0.0f;
        if (contact.normal.y > 0.0f) player_.grounded = true;
      }
      resolved = true;
    };
    for (std::size_t i = 0; i < solids_.size(); ++i) {
      resolve_contact(query_aabb(player_bounds(), solids_[i]));
    }
    for (std::size_t i = 0; i < moving_platforms_.size(); ++i) {
      resolve_contact(query_aabb(player_bounds(), moving_platforms_[i].bounds));
    }
    if (!resolved) {
      break;
    }
  }
}

void World::update_moving_platforms() {
  for (std::size_t i = 0; i < moving_platforms_.size(); ++i) {
    MovingPlatform& platform = moving_platforms_[i];
    platform.delta = Vec2{0.0f, 0.0f};
    const float dx = platform.velocity.x * config_.fixed_dt;
    const float dy = platform.velocity.y * config_.fixed_dt;
    if (!std::isfinite(dx) || !std::isfinite(dy)) continue;
    const Aabb candidate = {platform.bounds.left + dx,
                            platform.bounds.top + dy,
                            platform.bounds.right + dx,
                            platform.bounds.bottom + dy};
    if (!query_aabb(candidate, candidate).overlapping) continue;
    platform.bounds = candidate;
    platform.delta = Vec2{dx, dy};
  }
}

void World::select_grounded_platform() {
  grounded_on_platform_ = false;
  if (!player_.grounded) return;
  const Aabb bounds = player_bounds();
  const float kContactTolerance = 0.001f;
  for (std::size_t i = 0; i < moving_platforms_.size(); ++i) {
    const Aabb& platform = moving_platforms_[i].bounds;
    if (std::fabs(bounds.bottom - platform.top) <= kContactTolerance &&
        bounds.right > platform.left && bounds.left < platform.right) {
      grounded_on_platform_ = true;
      grounded_platform_index_ = i;
      return;
    }
  }
}

void World::step(const InputFrame& input) {
  previous_player_position_ = player_.position;
  const bool was_grounded = player_.grounded;
  const bool ride_platform = was_grounded && grounded_on_platform_ &&
                             grounded_platform_index_ < moving_platforms_.size();
  const bool start_jump = jump_assist_.update(input.jump_pressed, was_grounded);
  update_moving_platforms();
  if (ride_platform) {
    const Vec2 delta = moving_platforms_[grounded_platform_index_].delta;
    player_.position.x += delta.x;
    player_.position.y += delta.y;
  }
  player_.grounded = false;
  grounded_on_platform_ = false;
  float direction = std::isfinite(input.move_x)
    ? std::max(-1.0f, std::min(1.0f, input.move_x)) : 0.0f;
  if (direction == 0.0f) {
    direction = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f);
  }
  player_.velocity.x += direction * config_.move_accel * config_.fixed_dt;
  if (direction == 0.0f) {
    player_.velocity.x *= 0.82f;
  }
  player_.velocity.x = std::max(-config_.max_speed,
                                std::min(config_.max_speed, player_.velocity.x));

  if (start_jump) {
    player_.velocity.y = -config_.jump_speed;
    player_.grounded = false;
  }

  player_.velocity.y += config_.gravity * config_.fixed_dt;
  player_.position.x += player_.velocity.x * config_.fixed_dt;
  player_.position.y += player_.velocity.y * config_.fixed_dt;

  resolve_solids();

  if (player_.position.y >= config_.floor_y) {
    player_.position.y = config_.floor_y;
    player_.velocity.y = 0.0f;
    player_.grounded = true;
  }
  if (ride_platform && !start_jump) player_.grounded = true;
  select_grounded_platform();
  ++tick_;
}

const Actor& World::player() const { return player_; }
bool World::interpolated_player_position(double alpha, Vec2& output) const {
  if (!std::isfinite(alpha) || alpha < 0.0 || alpha >= 1.0) {
    return false;
  }
  const double previous_x = previous_player_position_.x;
  const double previous_y = previous_player_position_.y;
  const double x = previous_x +
                   (static_cast<double>(player_.position.x) - previous_x) *
                       alpha;
  const double y = previous_y +
                   (static_cast<double>(player_.position.y) - previous_y) *
                       alpha;
  const double max_float = (std::numeric_limits<float>::max)();
  if (!std::isfinite(x) || !std::isfinite(y) || std::fabs(x) > max_float ||
      std::fabs(y) > max_float) {
    return false;
  }
  output = Vec2{static_cast<float>(x), static_cast<float>(y)};
  return true;
}
std::uint64_t World::tick() const { return tick_; }

}  // namespace pc2ps3
