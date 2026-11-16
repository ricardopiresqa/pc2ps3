#include "pc2ps3/runtime_loop.hpp"

#include "pc2ps3/input_tick_distributor.hpp"

namespace pc2ps3 {
namespace {

RuntimeLoopStatus run_frames(RuntimePlatform& platform,
                             RuntimeApplication& application,
                             Framebuffer& framebuffer,
                             const FixedStepConfig& config) {
  InputMapper mapper;
  InputTickDistributor distributor;
  FixedStepScheduler scheduler(config);
  const double fixed_dt_seconds =
      static_cast<double>(config.fixed_step_ns) / 1000000000.0;

  for (;;) {
    InputSnapshot snapshot;
    std::uint64_t elapsed_ns = 0;
    RuntimePollStatus poll_status = RuntimePollStatus::kError;
    try {
      poll_status = platform.poll(snapshot, elapsed_ns);
    } catch (...) {
      return RuntimeLoopStatus::kPollFailed;
    }
    if (poll_status == RuntimePollStatus::kQuit) {
      return RuntimeLoopStatus::kQuit;
    }
    if (poll_status != RuntimePollStatus::kFrame) {
      return RuntimeLoopStatus::kPollFailed;
    }

    distributor.submit(mapper.sample(snapshot));
    const FixedStepAdvance advance = scheduler.advance(elapsed_ns);
    if (!advance.valid) {
      return RuntimeLoopStatus::kInvalidConfiguration;
    }

    for (std::uint32_t step = 0; step < advance.steps; ++step) {
      InputFrame tick_input = {};
      if (!distributor.consume(tick_input)) {
        return RuntimeLoopStatus::kCallbackFailed;
      }
      bool updated = false;
      try {
        updated = application.update(tick_input, fixed_dt_seconds);
      } catch (...) {
        return RuntimeLoopStatus::kUpdateFailed;
      }
      if (!updated) {
        return RuntimeLoopStatus::kUpdateFailed;
      }
    }

    bool rendered = false;
    try {
      rendered = application.render(framebuffer, advance.alpha);
    } catch (...) {
      return RuntimeLoopStatus::kRenderFailed;
    }
    if (!rendered) {
      return RuntimeLoopStatus::kRenderFailed;
    }
    bool presented = false;
    try {
      presented = platform.present(framebuffer);
    } catch (...) {
      return RuntimeLoopStatus::kPresentFailed;
    }
    if (!presented) {
      return RuntimeLoopStatus::kPresentFailed;
    }
  }
}

} // namespace

RuntimeLoopStatus RuntimeLoop::run(
    RuntimePlatform& platform, RuntimeApplication& application,
    Framebuffer& framebuffer,
    const FixedStepConfig& fixed_step_config) const {
  const FixedStepScheduler scheduler(fixed_step_config);
  if (!scheduler.valid() || framebuffer.width() <= 0 ||
      framebuffer.height() <= 0 || framebuffer.pixel_data() == 0 ||
      framebuffer.pixel_storage_bytes() == 0) {
    return RuntimeLoopStatus::kInvalidConfiguration;
  }

  bool platform_initialized = false;
  try {
    if (!platform.initialize(framebuffer.width(), framebuffer.height())) {
      return RuntimeLoopStatus::kPlatformInitializationFailed;
    }
    platform_initialized = true;
  } catch (...) {
    return RuntimeLoopStatus::kPlatformInitializationFailed;
  }

  bool application_started = false;
  RuntimeLoopStatus status = RuntimeLoopStatus::kApplicationStartFailed;
  try {
    if (application.start()) {
      application_started = true;
      status = run_frames(platform, application, framebuffer,
                          fixed_step_config);
    }
  } catch (...) {
    status = application_started ? RuntimeLoopStatus::kCallbackFailed
                                 : RuntimeLoopStatus::kApplicationStartFailed;
  }

  if (application_started) {
    try {
      application.stop();
    } catch (...) {
      status = RuntimeLoopStatus::kCleanupFailed;
    }
  }
  if (platform_initialized) {
    try {
      platform.shutdown();
    } catch (...) {
      status = RuntimeLoopStatus::kCleanupFailed;
    }
  }
  return status;
}

} // namespace pc2ps3
