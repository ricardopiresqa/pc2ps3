#pragma once

#include <memory>

namespace pc2ps3 {

class Scene {
 public:
  virtual ~Scene() noexcept {}
  virtual void on_enter() noexcept {}
  virtual void on_exit() noexcept {}
  virtual void update(double dt) noexcept = 0;
};

class SceneManager {
 public:
  SceneManager();
  ~SceneManager() noexcept;

  SceneManager(const SceneManager&) = delete;
  SceneManager& operator=(const SceneManager&) = delete;
  SceneManager(SceneManager&&) = delete;
  SceneManager& operator=(SceneManager&&) = delete;

  // Ownership is consumed on success and failure. Transitions apply in update().
  bool request_transition(std::unique_ptr<Scene> destination) noexcept;
  bool update(double dt) noexcept;

  // Non-owning observer, valid until the next transition.
  Scene* active_scene() const noexcept;

 private:
  enum Phase { kIdle, kUpdating, kEntering, kExiting, kDestroying };

  void apply_pending() noexcept;
  void destroy_rejected(std::unique_ptr<Scene>& scene) noexcept;

  std::unique_ptr<Scene> active_;
  std::unique_ptr<Scene> pending_;
  Phase phase_;
};

}  // namespace pc2ps3
