#include "pc2ps3/runtime_loop.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

struct QueuedFrame {
  pc2ps3::RuntimePollStatus status;
  pc2ps3::InputSnapshot snapshot;
  std::uint64_t elapsed_ns;

  QueuedFrame()
      : status(pc2ps3::RuntimePollStatus::kQuit), snapshot(), elapsed_ns(0) {}
};

class FakePlatform : public pc2ps3::RuntimePlatform {
public:
  FakePlatform(std::vector<std::string>& events)
      : events_(events), frames(), next_frame(0), initialize_result(true),
        present_result(true), initialized_width(0), initialized_height(0),
        present_count(0), shutdown_count(0) {}

  bool initialize(int width, int height) {
    events_.push_back("initialize");
    initialized_width = width;
    initialized_height = height;
    return initialize_result;
  }

  pc2ps3::RuntimePollStatus poll(pc2ps3::InputSnapshot& snapshot,
                                 std::uint64_t& elapsed_ns) {
    events_.push_back("poll");
    if (next_frame >= frames.size()) {
      return pc2ps3::RuntimePollStatus::kQuit;
    }
    const QueuedFrame& frame = frames[next_frame++];
    if (frame.status == pc2ps3::RuntimePollStatus::kFrame) {
      snapshot = frame.snapshot;
      elapsed_ns = frame.elapsed_ns;
    }
    return frame.status;
  }

  bool present(const pc2ps3::Framebuffer& framebuffer) {
    events_.push_back("present");
    ++present_count;
    return present_result && framebuffer.pixel_data() != 0;
  }

  void shutdown() {
    events_.push_back("shutdown");
    ++shutdown_count;
  }

  std::vector<QueuedFrame> frames;
  std::size_t next_frame;
  bool initialize_result;
  bool present_result;
  int initialized_width;
  int initialized_height;
  int present_count;
  int shutdown_count;

private:
  std::vector<std::string>& events_;
};

class FakeApplication : public pc2ps3::RuntimeApplication {
public:
  FakeApplication(std::vector<std::string>& events)
      : events_(events), start_result(true), update_result(true),
render_result(true), stop_count(0), update_inputs(), update_deltas(),
render_alphas() {}

  bool start() {
    events_.push_back("start");
    return start_result;
  }

  bool update(const pc2ps3::InputFrame& input, double fixed_dt_seconds) {
    events_.push_back("update");
    update_inputs.push_back(input);
    update_deltas.push_back(fixed_dt_seconds);
    return update_result;
  }

bool render(pc2ps3::Framebuffer&, double interpolation_alpha) {
    events_.push_back("render");
render_alphas.push_back(interpolation_alpha);
    return render_result;
  }

  void stop() {
    events_.push_back("stop");
    ++stop_count;
  }

  bool start_result;
  bool update_result;
  bool render_result;
  int stop_count;
  std::vector<pc2ps3::InputFrame> update_inputs;
  std::vector<double> update_deltas;
std::vector<double> render_alphas;

private:
  std::vector<std::string>& events_;
};

QueuedFrame frame(std::uint64_t elapsed_ns, bool cross) {
  QueuedFrame queued;
  queued.status = pc2ps3::RuntimePollStatus::kFrame;
  queued.elapsed_ns = elapsed_ns;
  queued.snapshot.dualshock3_cross = cross;
  return queued;
}

QueuedFrame quit_frame() {
  QueuedFrame queued;
  queued.status = pc2ps3::RuntimePollStatus::kQuit;
  return queued;
}

bool equal_events(const std::vector<std::string>& actual,
                  const std::vector<std::string>& expected) {
  return actual == expected;
}

void test_normal_lifecycle_and_catch_up() {
  std::vector<std::string> events;
  FakePlatform platform(events);
  FakeApplication application(events);
  pc2ps3::Framebuffer framebuffer(4, 3);
  platform.frames.push_back(frame(20, true));
platform.frames.push_back(frame(25, false));
platform.frames.push_back(frame(0, false));
  platform.frames.push_back(quit_frame());

  const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
      platform, application, framebuffer, pc2ps3::FixedStepConfig(10, 30, 2));
  assert(status == pc2ps3::RuntimeLoopStatus::kQuit);
  assert(platform.initialized_width == 4 && platform.initialized_height == 3);
assert(application.update_inputs.size() == 4);
  assert(application.update_inputs[0].confirm_pressed);
  assert(application.update_inputs[0].jump_pressed);
  assert(!application.update_inputs[1].confirm_pressed);
  assert(!application.update_inputs[1].jump_pressed);
  assert(application.update_inputs[0].confirm_held &&
         application.update_inputs[1].confirm_held);
assert(!application.update_inputs[2].confirm_held &&
!application.update_inputs[3].confirm_held);
assert(application.update_deltas.size() == 4);
  assert(std::fabs(application.update_deltas[0] - 0.00000001) < 1e-14);
assert(platform.present_count == 3);
assert(application.render_alphas.size() == 3);
assert(application.render_alphas[0] == 0.0);
assert(std::fabs(application.render_alphas[1] - 0.5) < 0.000001);
assert(std::fabs(application.render_alphas[2] - 0.5) < 0.000001);
  assert(application.stop_count == 1 && platform.shutdown_count == 1);
  const std::vector<std::string> expected = {
      "initialize", "start", "poll", "update", "update", "render",
"present", "poll", "update", "update", "render", "present", "poll",
"render", "present", "poll", "stop", "shutdown"};
  assert(equal_events(events, expected));
}

void test_zero_tick_still_renders_and_presents() {
  std::vector<std::string> events;
  FakePlatform platform(events);
  FakeApplication application(events);
  pc2ps3::Framebuffer framebuffer(2, 2);
  platform.frames.push_back(frame(5, false));
  platform.frames.push_back(quit_frame());

  const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
      platform, application, framebuffer, pc2ps3::FixedStepConfig(10, 30, 2));
  assert(status == pc2ps3::RuntimeLoopStatus::kQuit);
  assert(application.update_inputs.empty());
  assert(platform.present_count == 1);
assert(application.render_alphas.size() == 1);
assert(std::fabs(application.render_alphas[0] - 0.5) < 0.000001);
  const std::vector<std::string> expected = {
      "initialize", "start", "poll", "render", "present", "poll", "stop",
      "shutdown"};
  assert(equal_events(events, expected));
}

void test_invalid_configuration_does_not_initialize_host() {
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(2, 2);
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer, pc2ps3::FixedStepConfig(0, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kInvalidConfiguration);
    assert(events.empty());
  }
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(0, 2);
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer,
        pc2ps3::FixedStepConfig(10, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kInvalidConfiguration);
    assert(events.empty());
  }
}

void test_initialization_and_start_failures_cleanup_only_started_parts() {
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(2, 2);
    platform.initialize_result = false;
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer,
        pc2ps3::FixedStepConfig(10, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kPlatformInitializationFailed);
    assert(application.stop_count == 0 && platform.shutdown_count == 0);
    const std::vector<std::string> expected = {"initialize"};
    assert(equal_events(events, expected));
  }
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(2, 2);
    application.start_result = false;
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer,
        pc2ps3::FixedStepConfig(10, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kApplicationStartFailed);
    assert(application.stop_count == 0 && platform.shutdown_count == 1);
    const std::vector<std::string> expected = {
        "initialize", "start", "shutdown"};
    assert(equal_events(events, expected));
  }
}

void test_poll_update_render_and_present_failures_cleanup() {
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(2, 2);
    QueuedFrame error;
    error.status = pc2ps3::RuntimePollStatus::kError;
    platform.frames.push_back(error);
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer,
        pc2ps3::FixedStepConfig(10, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kPollFailed);
    assert(application.stop_count == 1 && platform.shutdown_count == 1);
  }
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(2, 2);
    platform.frames.push_back(frame(10, false));
    application.update_result = false;
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer,
        pc2ps3::FixedStepConfig(10, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kUpdateFailed);
    assert(application.stop_count == 1 && platform.shutdown_count == 1);
    const std::vector<std::string> expected = {
        "initialize", "start", "poll", "update", "stop", "shutdown"};
    assert(equal_events(events, expected));
  }
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(2, 2);
    platform.frames.push_back(frame(0, false));
    application.render_result = false;
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer,
        pc2ps3::FixedStepConfig(10, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kRenderFailed);
    assert(application.stop_count == 1 && platform.shutdown_count == 1);
  }
  {
    std::vector<std::string> events;
    FakePlatform platform(events);
    FakeApplication application(events);
    pc2ps3::Framebuffer framebuffer(2, 2);
    platform.frames.push_back(frame(0, false));
    platform.present_result = false;
    const pc2ps3::RuntimeLoopStatus status = pc2ps3::RuntimeLoop().run(
        platform, application, framebuffer,
        pc2ps3::FixedStepConfig(10, 30, 2));
    assert(status == pc2ps3::RuntimeLoopStatus::kPresentFailed);
    assert(application.stop_count == 1 && platform.shutdown_count == 1);
  }
}

} // namespace

int main() {
  test_normal_lifecycle_and_catch_up();
  test_zero_tick_still_renders_and_presents();
  test_invalid_configuration_does_not_initialize_host();
  test_initialization_and_start_failures_cleanup_only_started_parts();
  test_poll_update_render_and_present_failures_cleanup();
  return 0;
}
