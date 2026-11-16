#include "pc2ps3/scene_manager.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
struct State {
  State() : manager(0), enter_request(false), exit_request(false),
            destroy_request(false), update_transition(false), reenter(false),
            last_request_result(true), reentrant_update_result(true) {}
  pc2ps3::SceneManager* manager;
  std::vector<std::string> events;
  bool enter_request;
  bool exit_request;
  bool destroy_request;
  bool update_transition;
  bool reenter;
  bool last_request_result;
  bool reentrant_update_result;
};

class Probe : public pc2ps3::Scene {
 public:
  Probe(const std::string& name, State& state, bool requests = false)
      : name_(name), state_(state), requests_(requests) {}
  ~Probe() noexcept {
    state_.events.push_back(name_ + ".destroy");
    if (requests_ && state_.destroy_request)
      state_.last_request_result = state_.manager->request_transition(
          std::unique_ptr<pc2ps3::Scene>(new Probe("rejected_destroy", state_)));
  }
  void on_enter() noexcept {
    state_.events.push_back(name_ + ".enter");
    if (requests_ && state_.enter_request)
      state_.last_request_result = state_.manager->request_transition(
          std::unique_ptr<pc2ps3::Scene>(new Probe("rejected_enter", state_)));
  }
  void on_exit() noexcept {
    state_.events.push_back(name_ + ".exit");
    if (requests_ && state_.exit_request)
      state_.last_request_result = state_.manager->request_transition(
          std::unique_ptr<pc2ps3::Scene>(new Probe("rejected_exit", state_)));
  }
  void update(double) noexcept {
    state_.events.push_back(name_ + ".update");
    if (requests_ && state_.reenter)
      state_.reentrant_update_result = state_.manager->update(0.0);
    if (requests_ && state_.update_transition) {
      state_.last_request_result = state_.manager->request_transition(
          std::unique_ptr<pc2ps3::Scene>(new Probe("next", state_)));
      state_.update_transition = false;
    }
  }

 private:
  std::string name_;
  State& state_;
  bool requests_;
};
}  // namespace

int main() {
  {
    State state;
    pc2ps3::SceneManager manager;
    state.manager = &manager;
    assert(manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
        new Probe("first", state))));
    assert(manager.update(0.0));
    assert(manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
        new Probe("second", state))));
    assert(manager.update(0.0));
    const std::vector<std::string> expected = {
        "first.enter", "first.update", "first.exit", "first.destroy",
        "second.enter", "second.update"};
    assert(state.events == expected);
  }
  {
    State state;
    pc2ps3::SceneManager manager;
    state.manager = &manager;
    state.update_transition = true;
    state.reenter = true;
    manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
        new Probe("origin", state, true)));
    state.events.clear();
    manager.update(0.0);
    const std::vector<std::string> expected = {
        "origin.enter", "origin.update", "origin.exit", "origin.destroy",
        "next.enter"};
    assert(state.events == expected);
    assert(!state.reentrant_update_result && state.last_request_result);
    assert(manager.active_scene() != 0);
  }
  {
    State state;
    pc2ps3::SceneManager manager;
    state.manager = &manager;
    assert(manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
        new Probe("pending", state))));
    assert(!manager.update(-1.0));
    assert(!manager.update(std::numeric_limits<double>::quiet_NaN()));
    assert(!manager.update(std::numeric_limits<double>::infinity()));
    assert(state.events.empty() && manager.active_scene() == 0);
    assert(manager.update(0.0));
    assert(state.events[0] == "pending.enter");
  }
  {
    State state;
    pc2ps3::SceneManager manager;
    state.manager = &manager;
    assert(!manager.request_transition(std::unique_ptr<pc2ps3::Scene>()));
    assert(manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
        new Probe("kept", state))));
    assert(!manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
        new Probe("duplicate", state))));
    assert(state.events.size() == 1 && state.events[0] == "duplicate.destroy");
    manager.update(0.0);
    assert(state.events[1] == "kept.enter");
  }
  {
    State state;
    {
      pc2ps3::SceneManager manager;
      state.manager = &manager;
      state.enter_request = true;
      state.exit_request = true;
      state.destroy_request = true;
      state.update_transition = true;
      state.reenter = true;
      manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
          new Probe("guarded", state, true)));
      manager.update(0.0);
      assert(!state.last_request_result);
      assert(!state.reentrant_update_result);
      const std::vector<std::string> expected = {
          "guarded.enter", "rejected_enter.destroy", "guarded.update",
          "guarded.exit", "rejected_exit.destroy", "guarded.destroy",
          "rejected_destroy.destroy", "next.enter"};
      assert(state.events == expected);
    }
    const std::vector<std::string> expected = {
        "guarded.enter", "rejected_enter.destroy", "guarded.update",
        "guarded.exit", "rejected_exit.destroy", "guarded.destroy",
        "rejected_destroy.destroy", "next.enter", "next.exit", "next.destroy"};
    assert(state.events == expected);
  }
  {
    State state;
    {
      pc2ps3::SceneManager manager;
      state.manager = &manager;
      manager.request_transition(std::unique_ptr<pc2ps3::Scene>(
          new Probe("never_entered", state)));
    }
    const std::vector<std::string> expected = {"never_entered.destroy"};
    assert(state.events == expected);
  }
  return 0;
}
