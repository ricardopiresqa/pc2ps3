#include "pc2ps3/scene_manager.hpp"

#include <cmath>

namespace pc2ps3 {

SceneManager::SceneManager() : phase_(kIdle) {}

SceneManager::~SceneManager() noexcept {
  if (active_) {
    phase_ = kExiting;
    active_->on_exit();
    phase_ = kDestroying;
    active_.reset();
  }
  phase_ = kDestroying;
  pending_.reset();
}

bool SceneManager::request_transition(
    std::unique_ptr<Scene> destination) noexcept {
  if (!destination || pending_ || (phase_ != kIdle && phase_ != kUpdating)) {
    destroy_rejected(destination);
    return false;
  }
  pending_ = std::move(destination);
  return true;
}

bool SceneManager::update(double dt) noexcept {
  if (phase_ != kIdle || !std::isfinite(dt) || dt < 0.0) return false;

  phase_ = kUpdating;
  apply_pending();
  if (active_) active_->update(dt);
  apply_pending();
  phase_ = kIdle;
  return true;
}

Scene* SceneManager::active_scene() const noexcept { return active_.get(); }

void SceneManager::apply_pending() noexcept {
  if (!pending_) return;
  const Phase previous = phase_;
  std::unique_ptr<Scene> destination = std::move(pending_);
  if (active_) {
    phase_ = kExiting;
    active_->on_exit();
    phase_ = kDestroying;
    active_.reset();
  }
  active_ = std::move(destination);
  phase_ = kEntering;
  active_->on_enter();
  phase_ = previous;
}

void SceneManager::destroy_rejected(std::unique_ptr<Scene>& scene) noexcept {
  const Phase previous = phase_;
  phase_ = kDestroying;
  scene.reset();
  phase_ = previous;
}

}  // namespace pc2ps3
