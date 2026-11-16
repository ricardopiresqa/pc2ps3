#pragma once

#include "pc2ps3/fixed_step_scheduler.hpp"
#include "pc2ps3/input.hpp"
#include "pc2ps3/render.hpp"

#include <cstdint>

namespace pc2ps3 {

enum class RuntimePollStatus {
  kFrame,
  kQuit,
  kError
};

enum class RuntimeLoopStatus {
  kQuit,
  kInvalidConfiguration,
  kPlatformInitializationFailed,
  kApplicationStartFailed,
  kPollFailed,
  kUpdateFailed,
  kRenderFailed,
  kPresentFailed,
  kCallbackFailed,
  kCleanupFailed
};

class RuntimePlatform {
public:
  virtual ~RuntimePlatform() noexcept {}

  virtual bool initialize(int width, int height) = 0;
  virtual RuntimePollStatus poll(InputSnapshot& snapshot,
                                 std::uint64_t& elapsed_ns) = 0;
  virtual bool present(const Framebuffer& framebuffer) = 0;
  virtual void shutdown() = 0;
};

class RuntimeApplication {
public:
  virtual ~RuntimeApplication() noexcept {}

  virtual bool start() = 0;
  virtual bool update(const InputFrame& input, double fixed_dt_seconds) = 0;
  virtual bool render(Framebuffer& framebuffer,
                      double interpolation_alpha) = 0;
  virtual void stop() = 0;
};

class RuntimeLoop {
public:
  RuntimeLoopStatus run(RuntimePlatform& platform,
                        RuntimeApplication& application,
                        Framebuffer& framebuffer,
                        const FixedStepConfig& fixed_step_config) const;
};

} // namespace pc2ps3
