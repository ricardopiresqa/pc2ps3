#include "pc2ps3/core.hpp"
#include "pc2ps3/audio.hpp"
#include "pc2ps3/input.hpp"
#include "pc2ps3/render.hpp"
#include "pc2ps3/runtime_loop.hpp"
#include "pc2ps3/win32_runtime_platform.hpp"
#include "pc2ps3/win32_audio_output.hpp"
#include "pc2ps3/host_file_system.hpp"
#include "runtime_demo_atlas.hpp"
#include "runtime_demo_options.hpp"

#include <cmath>
#include <iostream>
#include <memory>

namespace {

pc2ps3::WorldConfig make_demo_config() {
    return pc2ps3::WorldConfig{1.0f / 60.0f, 600.0f, 1600.0f, 240.0f,
                               320.0f, 300.0f};
}

class DemoApplication : public pc2ps3::RuntimeApplication {
public:
    explicit DemoApplication(const pc2ps3::RuntimeDemoOptions& options)
        : options_(options), asset_file_system_(), atlas_(),
          world_(make_demo_config()), started_(false),
          audio_manager_(44100, 4), audio_output_(), audio_enabled_(false) {}

    bool start() override {
        world_.reset();
        started_ = true;
        if (!options_.asset_root.empty()) {
            asset_file_system_.reset(
                new pc2ps3::HostFileSystem(options_.asset_root));
            if (asset_file_system_->valid()) {
                atlas_.reset(
                    new pc2ps3::RuntimeDemoAtlas(*asset_file_system_));
                if (!atlas_->load(options_.atlas_index,
                                  options_.sprite_index)) {
                    atlas_.reset();
                    asset_file_system_.reset();
                    std::cerr << "Atlas unavailable; using vector fallback\n";
                }
            } else {
                asset_file_system_.reset();
                std::cerr << "Atlas unavailable; using vector fallback\n";
            }
        }
        audio_enabled_ = audio_output_.initialize(&audio_manager_);
        if (!audio_enabled_) {
            std::cerr << "WinMM audio unavailable; continuing silently (error "
                      << audio_output_.last_error() << ")\n";
        }
        return true;
    }

    bool update(const pc2ps3::InputFrame& input,
                double fixed_dt_seconds) override {
        if (!started_ || fixed_dt_seconds <= 0.0) {
            return false;
        }
        world_.step(input);
        return true;
    }

    bool render(pc2ps3::Framebuffer& framebuffer,
                double interpolation_alpha) override {
        pc2ps3::Vec2 position;
        if (!started_ || !world_.interpolated_player_position(
                             interpolation_alpha, position)) {
            return false;
        }
        if (audio_enabled_ && !audio_output_.pump()) {
            audio_enabled_ = false;
            std::cerr << "WinMM audio stopped (error "
                      << audio_output_.last_error() << ")\n";
            if (!audio_output_.shutdown()) {
                std::cerr << "WinMM cleanup will be retried during shutdown\n";
            }
        }
        framebuffer.clear(pc2ps3::Color{18, 28, 48, 255});
        framebuffer.fill_rect(0, 300, 640, 60,
                              pc2ps3::Color{45, 94, 65, 255});
        pc2ps3::render_runtime_demo_player(
            framebuffer, atlas_.get(),
            static_cast<int>(position.x) - 12,
            static_cast<int>(position.y) - 32,
            pc2ps3::Color{222, 82, 74, 255},
            pc2ps3::Color{246, 214, 92, 255});
        return true;
    }

    void stop() override {
        audio_enabled_ = false;
        if (!audio_output_.shutdown()) {
            std::cerr << "WinMM cleanup remains pending; buffers stay owned\n";
        }
        atlas_.reset();
        asset_file_system_.reset();
        started_ = false;
    }

private:
    pc2ps3::RuntimeDemoOptions options_;
    std::unique_ptr<pc2ps3::HostFileSystem> asset_file_system_;
    std::unique_ptr<pc2ps3::RuntimeDemoAtlas> atlas_;
    pc2ps3::World world_;
    bool started_;
    pc2ps3::AudioManager audio_manager_;
    pc2ps3::Win32AudioOutput audio_output_;
    bool audio_enabled_;
};

} // namespace

int main(int argc, char* argv[]) {
    pc2ps3::RuntimeDemoOptions options;
    if (!pc2ps3::parse_runtime_demo_options(argc, argv, options)) {
        std::cerr << "Usage: pc2ps3_pc_demo [--max-fps 0..240] "
                     "[--asset-root PATH --atlas-index LOGICAL_PATH "
                     "[--sprite-index 0..1023]]\n";
        return 2;
    }

    pc2ps3::Win32RuntimePlatform platform(true, options.max_fps);
    DemoApplication application(options);
    pc2ps3::Framebuffer framebuffer(640, 360);
    const pc2ps3::FixedStepConfig timing(16666667u, 100000000u, 5u);
    const pc2ps3::RuntimeLoopStatus status =
        pc2ps3::RuntimeLoop().run(platform, application, framebuffer, timing);
    if (status != pc2ps3::RuntimeLoopStatus::kQuit) {
        std::cerr << "PC2PS3 runtime demo failed (status "
                  << static_cast<int>(status) << ")\n";
        return 1;
    }
    return 0;
}
