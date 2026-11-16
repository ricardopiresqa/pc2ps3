// PC2PS3 PS3 executable: original vertical slice (Phase 3).
// Run (stick/D-pad), jump (X), attack (Square), patrolling enemy, checkpoint,
// goal and result screen. Art, audio and tuning are original placeholders, not
// measurements of any commercial game. If the user drops a locally converted
// atlas (tools/build_atlas.py) into USRDIR/player/, the player sprite comes
// from it; nothing commercial ships in the PKG.
#include "pc2ps3/animation.hpp"
#include "pc2ps3/audio.hpp"
#include "pc2ps3/camera.hpp"
#include "pc2ps3/checkpoint.hpp"
#include "pc2ps3/core.hpp"
#include "pc2ps3/enemy.hpp"
#include "pc2ps3/hitbox.hpp"
#include "pc2ps3/posix_file_system.hpp"
#include "pc2ps3/render.hpp"
#include "pc2ps3/runtime_loop.hpp"
#include "pc2ps3/sdl_runtime_platform.hpp"
#include "pc2ps3/storage.hpp"
#include "pc2ps3/trigger.hpp"
#include "runtime_demo_atlas.hpp"
#ifdef PC2PS3_LOCAL_GAME
// Optional local-only module under games/ (gitignored); compiled only when present.
#include "root_pack_app.hpp"
#endif

#include <SDL.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

const int kWidth = 640;
const int kHeight = 360;
const std::uint32_t kAudioRate = 48000;
const char kDataRoot[] = "/dev_hdd0/game/PCPS00001/USRDIR";
const char kPlayerAtlas[] = "player/atlas.p2sa";

const float kFloorY = 320.0f;
const float kLevelWidth = 1600.0f;
const float kGoalX = 1480.0f;
const std::uint32_t kPlayerId = 1;
const std::uint32_t kEnemyId = 2;
const std::uint32_t kCheckpointId = 10;
const std::uint32_t kAttackId = 100;
const std::uint32_t kEnemyHealth = 30;
const std::uint32_t kAttackDamage = 10;
const int kAttackTicks = 12;
const int kHurtTicks = 45;

std::shared_ptr<const pc2ps3::PcmClip> make_tone(float hz_start, float hz_end,
                                                 float seconds, float gain) {
    const std::size_t frames = static_cast<std::size_t>(seconds * kAudioRate);
    std::vector<std::int16_t> samples(frames * 2);
    double phase = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(frames);
        phase += (hz_start + (hz_end - hz_start) * t) / kAudioRate;
        const float square = (phase - std::floor(phase)) < 0.5 ? 1.0f : -1.0f;
        const std::int16_t v =
            static_cast<std::int16_t>(square * gain * (1.0f - t) * 32767.0f);
        samples[i * 2] = v;
        samples[i * 2 + 1] = v;
    }
    return std::make_shared<pc2ps3::PcmClip>(kAudioRate, 2, samples);
}

// Four-note arpeggio loop; original placeholder music.
std::shared_ptr<const pc2ps3::PcmClip> make_music() {
    static const float notes[] = {220.0f, 277.18f, 329.63f, 440.0f};
    const std::size_t note_frames = kAudioRate / 4;
    std::vector<std::int16_t> samples(note_frames * 4 * 2);
    double phase = 0.0;
    for (std::size_t n = 0; n < 4; ++n) {
        for (std::size_t i = 0; i < note_frames; ++i) {
            phase += notes[n] / kAudioRate;
            const float env = 1.0f - static_cast<float>(i) / note_frames;
            const float tri = static_cast<float>(
                4.0 * std::fabs(phase - std::floor(phase) - 0.5) - 1.0);
            const std::int16_t v =
                static_cast<std::int16_t>(tri * 0.10f * env * 32767.0f);
            samples[(n * note_frames + i) * 2] = v;
            samples[(n * note_frames + i) * 2 + 1] = v;
        }
    }
    return std::make_shared<pc2ps3::PcmClip>(kAudioRate, 2, samples);
}

// Same original 4-frame character atlas as apps/headless/prototype.cpp
// (frames 0-1 run, 2-3 attack), 24x32 each.
pc2ps3::Texture make_character_atlas() {
    const int fw = 24, fh = 32, count = 4;
    const pc2ps3::Color clear = {0, 0, 0, 0};
    const pc2ps3::Color skin = {236, 190, 144, 255};
    const pc2ps3::Color body = {72, 198, 146, 255};
    const pc2ps3::Color trim = {246, 214, 92, 255};
    std::vector<pc2ps3::Color> pixels(static_cast<std::size_t>(fw * count * fh), clear);
    for (int frame = 0; frame < count; ++frame) {
        for (int y = 2; y <= 30; ++y) {
            for (int x = 5; x <= 21; ++x) {
                pc2ps3::Color c = clear;
                if (y <= 8 && x >= 9 && x <= 14) {
                    c = skin;
                } else if (y >= 9 && y <= 22 && x >= 7 && x <= 16) {
                    c = body;
                } else if (y >= 23 && ((frame % 2 == 0 && ((x >= 7 && x <= 11) || (x >= 13 && x <= 17))) ||
                                       (frame % 2 == 1 && ((x >= 5 && x <= 10) || (x >= 14 && x <= 18))))) {
                    c = frame >= 2 ? skin : trim;
                } else if (frame >= 2 && y >= 13 && y <= 17 && x >= 17) {
                    c = trim;
                }
                if (c.a != 0) pixels[static_cast<std::size_t>(y * fw * count + frame * fw + x)] = c;
            }
        }
    }
    return pc2ps3::Texture(fw * count, fh, pixels);
}

pc2ps3::Aabb player_box(const pc2ps3::Actor& p) {
    return pc2ps3::Aabb{p.position.x - 12.0f, p.position.y - 32.0f,
                        p.position.x + 12.0f, p.position.y};
}

bool overlaps(const pc2ps3::Aabb& a, const pc2ps3::Aabb& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

// newlib-PPU's mallinfo() packs 32-bit fields under a size_t header (reads as
// garbage), so report heap growth from the program break instead.
char* const g_heap_base = static_cast<char*>(sbrk(0));

std::uint64_t heap_in_use() {
    return static_cast<std::uint64_t>(static_cast<char*>(sbrk(0)) - g_heap_base);
}

void text(pc2ps3::Framebuffer& fb, const char* s, int x, int y, int scale,
          pc2ps3::Color color = pc2ps3::Color{255, 255, 255, 255}) {
    fb.draw_text(s, std::strlen(s), x, y, color, scale);
}

enum class Phase { kPlaying, kVictory };

class Slice : public pc2ps3::RuntimeApplication {
public:
    explicit Slice(pc2ps3::SdlRuntimePlatform& platform)
        : platform_(platform),
          world_(pc2ps3::WorldConfig{1.0f / 60.0f, 1200.0f, 1800.0f, 240.0f, 420.0f, kFloorY}),
          enemies_(4), combat_(2, 2), triggers_(1),
          checkpoints_(1, pc2ps3::Vec2{64.0f, kFloorY}), camera_(kWidth, kHeight),
          character_(make_character_atlas()),
          run_clip_(std::vector<pc2ps3::AnimationFrame>{
                        pc2ps3::AnimationFrame{0, 0, 24, 32, 0.10},
                        pc2ps3::AnimationFrame{24, 0, 24, 32, 0.10}}, true),
          attack_clip_(std::vector<pc2ps3::AnimationFrame>{
                           pc2ps3::AnimationFrame{48, 0, 24, 32, 0.05},
                           pc2ps3::AnimationFrame{72, 0, 24, 32, 0.05}}, false),
          file_system_(kDataRoot), saves_(file_system_, "", 1, 16),
          audio_(kAudioRate, 8), paused_(false),
          ticks_(0), player_sprite_(0) {}

    bool start() override {
        world_.add_solid(pc2ps3::Aabb{-40.0f, 0.0f, 0.0f, kFloorY});
        world_.add_solid(pc2ps3::Aabb{kLevelWidth, 0.0f, kLevelWidth + 40.0f, kFloorY});
        world_.add_solid(pc2ps3::Aabb{700.0f, 280.0f, 780.0f, kFloorY});  // step to jump over
        if (!enemies_.valid() || !combat_.valid() || !triggers_.valid() ||
            !checkpoints_.valid() || !camera_.valid() || !character_.valid() ||
            !run_clip_.valid() || !attack_clip_.valid() ||
            !camera_.set_world_bounds(pc2ps3::Aabb{0.0f, 0.0f, kLevelWidth, kHeight}) ||
            !triggers_.add(kCheckpointId, pc2ps3::Aabb{560.0f, 280.0f, 580.0f, kFloorY}) ||
            !checkpoints_.add(kCheckpointId, pc2ps3::Vec2{570.0f, kFloorY})) {
            std::printf("[pc2ps3] slice init failed\n");
            return false;
        }
        load_player_atlas(0);
        load_progress();
        std::vector<std::uint8_t> flag;
        autoplay_ = file_system_.read_all("qa_autoplay", 64, flag) == pc2ps3::FileSystemStatus::kOk;
        if (autoplay_) std::printf("[pc2ps3] qa autoplay on\n");
        jump_sfx_ = make_tone(330.0f, 880.0f, 0.12f, 0.22f);
        hit_sfx_ = make_tone(180.0f, 90.0f, 0.10f, 0.30f);
        checkpoint_sfx_ = make_tone(523.0f, 1046.0f, 0.25f, 0.18f);
        hurt_sfx_ = make_tone(220.0f, 110.0f, 0.25f, 0.25f);
        victory_sfx_ = make_tone(392.0f, 1568.0f, 0.6f, 0.20f);
        if (platform_.start_audio(&audio_)) {
            pc2ps3::AudioVoiceHandle handle;
            platform_.lock_audio();
            audio_.play_effect(make_music(), pc2ps3::AudioCategory::kMusic, 1.0f, true, handle);
            platform_.unlock_audio();
        }
        restart();
        std::printf("[pc2ps3] slice started heap=%lu\n", static_cast<unsigned long>(heap_in_use()));
        return true;
    }

    bool update(const pc2ps3::InputFrame& pad_input, double dt) override {
        pc2ps3::InputFrame input = pad_input;
        if (autoplay_) {  // QA: scripted run to the goal, then quit like the XMB does.
            const unsigned t = ticks_;
            input = pc2ps3::InputFrame{};
            input.right = true;
            input.jump_pressed = t % 24 == 0;
            input.jump_held = t % 24 < 12;
            input.attack_pressed = t % 8 == 0;
            if (phase_ == Phase::kVictory && result_ticks_ == 120) {
                std::printf("[pc2ps3] qa autoplay done\n");
                SDL_Event quit;
                SDL_zero(quit);
                quit.type = SDL_QUIT;
                SDL_PushEvent(&quit);
            }
        }
        if (phase_ == Phase::kVictory) {
            // Start only, after 1 s, so buttons held through the goal don't skip the result.
            if (++result_ticks_ > 60 && input.pause_pressed) restart();
            return true;
        }
        if (input.pause_pressed) {
            paused_ = !paused_;
            std::printf("[pc2ps3] %s\n", paused_ ? "paused" : "resumed");
        }
        if (paused_) return true;
        if (player_atlas_.get() != 0 && !animated() && (input.l1_pressed || input.r1_pressed)) {
            load_player_atlas(input.r1_pressed ? player_sprite_ + 1 : (player_sprite_ == 0 ? 0 : player_sprite_ - 1));
        }

        if (input.left) facing_ = -1;
        if (input.right) facing_ = 1;
        if (input.jump_pressed && world_.player().grounded) play(jump_sfx_);
        world_.step(input);
        ++ticks_;
        if (hurt_timer_ > 0) --hurt_timer_;

        const pc2ps3::Aabb me = player_box(world_.player());
        triggers_.update(me);
        for (std::size_t i = 0; i < triggers_.events().size(); ++i) {
            if (triggers_.events()[i].type == pc2ps3::TriggerEventType::kEnter &&
                checkpoints_.activate(triggers_.events()[i].trigger_id) && !checkpoint_reached_) {
                checkpoint_reached_ = true;
                play(checkpoint_sfx_);
                std::printf("[pc2ps3] checkpoint %u\n", kCheckpointId);
            }
        }

        // Enemy patrol (caller-owned AI per EnemySystem contract).
        pc2ps3::EnemyState enemy = {0, pc2ps3::Aabb{0, 0, 0, 0}, 0};
        const bool enemy_alive = enemies_.get(kEnemyId, enemy);
        if (enemy_alive) {
            float x = enemy.bounds.left + enemy_dir_ * 1.2f;
            if (x < 960.0f || x > 1180.0f) enemy_dir_ = -enemy_dir_;
            x = std::max(960.0f, std::min(1180.0f, x));
            enemy.bounds = pc2ps3::Aabb{x, kFloorY - 32.0f, x + 28.0f, kFloorY};
            enemies_.set_bounds(kEnemyId, enemy.bounds);
        }

        combat_.begin_tick();
        if (enemy_alive) combat_.add_hurtbox(kEnemyId, kEnemyId, enemy.bounds);
        if (input.attack_pressed && attack_timer_ == 0) {
            attack_timer_ = kAttackTicks;
            animation_.play(attack_clip_);
        }
        if (attack_timer_ > 0) {
            const pc2ps3::Vec2 p = world_.player().position;
            const float reach_left = facing_ > 0 ? p.x + 8.0f : p.x - 36.0f;
            if (attack_timer_ > kAttackTicks - 4) {  // active frames only
                combat_.add_hitbox(kAttackId, kPlayerId,
                                   pc2ps3::Aabb{reach_left, p.y - 28.0f, reach_left + 28.0f, p.y - 4.0f});
            }
            if (--attack_timer_ == 0) animation_.play(run_clip_);
        }
        combat_.update();
        for (std::size_t i = 0; i < combat_.events().size(); ++i) {
            const pc2ps3::HitEvent& hit = combat_.events()[i];
            bool defeated = false;
            if (hit.attack_id == kAttackId && hit.target_id == kEnemyId &&
                enemies_.damage(kEnemyId, kAttackDamage, defeated)) {
                play(hit_sfx_);
                std::printf("[pc2ps3] enemy hit%s\n", defeated ? " - defeated" : "");
                if (defeated) enemy_defeated_ = true;
            }
        }

        // Touching a live enemy: respawn at the active checkpoint.
        if (enemy_alive && hurt_timer_ == 0 && enemies_.get(kEnemyId, enemy) &&
            overlaps(me, enemy.bounds)) {
            pc2ps3::Vec2 spawn;
            if (!checkpoints_.respawn_position(spawn)) spawn = pc2ps3::Vec2{64.0f, kFloorY};
            world_.respawn(spawn);
            hurt_timer_ = kHurtTicks;
            ++deaths_;
            play(hurt_sfx_);
            std::printf("[pc2ps3] hurt - respawn x=%d\n", static_cast<int>(spawn.x));
        }

        if (world_.player().position.x >= kGoalX) {
            phase_ = Phase::kVictory;
            play(victory_sfx_);
            std::printf("[pc2ps3] victory ticks=%u enemy_defeated=%d deaths=%u\n",
                        ticks_, enemy_defeated_ ? 1 : 0, deaths_);
            new_best_ = best_ticks_ == 0 || ticks_ < best_ticks_;
            if (new_best_) best_ticks_ = ticks_;
            ++clears_;
            save_progress();
        }
        if (attack_timer_ == 0) {
            const bool moving = std::fabs(world_.player().velocity.x) > 1.0f;
            if (moving) animation_.update(dt);
        } else {
            animation_.update(dt);
        }
        camera_.follow(pc2ps3::Vec2{world_.player().position.x, world_.player().position.y - 16.0f});
        return true;
    }

    bool render(pc2ps3::Framebuffer& fb, double alpha) override {
        pc2ps3::Vec2 p;
        if (!world_.interpolated_player_position(alpha, p)) return false;
        const int cx = camera_.x(), cy = camera_.y();
        fb.clear(pc2ps3::Color{18, 28, 48, 255});
        for (int i = 0; i < 40; ++i) {  // parallax stars at half camera speed
            const int x = ((i * 157) - cx / 2) % kWidth;
            fb.fill_rect(x < 0 ? x + kWidth : x, 16 + (i * 53) % 240, 2, 2,
                         pc2ps3::Color{200, 210, 255, 255});
        }
        fb.fill_rect(0, static_cast<int>(kFloorY) - cy, kWidth, kHeight, pc2ps3::Color{45, 94, 65, 255});
        fb.fill_rect(700 - cx, 280 - cy, 80, 40, pc2ps3::Color{96, 120, 92, 255});
        // Checkpoint flag.
        const pc2ps3::Color flag = checkpoint_reached_ ? pc2ps3::Color{246, 214, 92, 255}
                                                       : pc2ps3::Color{140, 140, 160, 255};
        fb.fill_rect(568 - cx, 272 - cy, 4, 48, pc2ps3::Color{220, 220, 220, 255});
        fb.fill_rect(572 - cx, 272 - cy, 18, 12, flag);
        // Goal gate.
        fb.fill_rect(static_cast<int>(kGoalX) - cx, 240 - cy, 8, 80, pc2ps3::Color{255, 255, 255, 255});
        fb.fill_rect(static_cast<int>(kGoalX) + 28 - cx, 240 - cy, 8, 80, pc2ps3::Color{255, 255, 255, 255});
        fb.fill_rect(static_cast<int>(kGoalX) - cx, 236 - cy, 36, 6, pc2ps3::Color{246, 214, 92, 255});

        pc2ps3::EnemyState enemy = {0, pc2ps3::Aabb{0, 0, 0, 0}, 0};
        if (enemies_.get(kEnemyId, enemy)) {
            const int ex = static_cast<int>(enemy.bounds.left) - cx;
            const int ey = static_cast<int>(enemy.bounds.top) - cy;
            fb.fill_rect(ex, ey, 28, 32, pc2ps3::Color{226, 86, 90, 255});
            fb.fill_rect(ex + (enemy_dir_ > 0 ? 18 : 4), ey + 8, 6, 6, pc2ps3::Color{255, 255, 255, 255});
            fb.fill_rect(ex, ey - 6, static_cast<int>(28 * enemy.health / kEnemyHealth), 3,
                         pc2ps3::Color{255, 120, 120, 255});
        }

        const bool blink = hurt_timer_ > 0 && (hurt_timer_ / 4) % 2 == 0;
        if (!blink) draw_player(fb, static_cast<int>(std::floor(p.x)) - cx, static_cast<int>(std::floor(p.y)) - cy);

        text(fb, "PC2PS3 VERTICAL SLICE", 16, 40, 2);
        char status[96];
        std::snprintf(status, sizeof(status), "CHECKPOINT %s  ENEMY %s  RETRIES %u  BEST %u.%02uS",
                      checkpoint_reached_ ? "ON" : "OFF", enemy_defeated_ ? "DOWN" : "UP", deaths_,
                      best_ticks_ / 60, (best_ticks_ % 60) * 100 / 60);
        text(fb, status, 16, 60, 1, pc2ps3::Color{230, 230, 230, 255});
        if (player_atlas_.get() != 0) {
            char label[48];
            if (animated()) std::snprintf(label, sizeof(label), "PLAYER ANIMATED %u FRAMES",
                                          static_cast<unsigned>(player_atlas_->index().sprites.size()));
            else std::snprintf(label, sizeof(label), "PLAYER ATLAS SPRITE %u  L1 R1", static_cast<unsigned>(player_sprite_));
            text(fb, label, 400, 8, 1, pc2ps3::Color{150, 220, 255, 255});
        }
        text(fb, "X JUMP  SQUARE ATTACK  START PAUSE  START AND SELECT EXIT", 16, 344, 1,
             pc2ps3::Color{230, 230, 230, 255});
        if (!platform_.audio_running()) text(fb, "AUDIO OFF", 560, 20, 1, pc2ps3::Color{255, 120, 120, 255});
        if (paused_) {
            fb.blend_overlay(pc2ps3::Color{0, 0, 0, 140});
            text(fb, "PAUSED", 280, 170, 3);
        }
        if (phase_ == Phase::kVictory) {
            fb.blend_overlay(pc2ps3::Color{0, 0, 0, 160});
            text(fb, "STAGE CLEAR", 220, 130, 4, pc2ps3::Color{246, 214, 92, 255});
            char result[96];
            std::snprintf(result, sizeof(result), "TIME %u.%02uS  ENEMY %s  RETRIES %u",
                          ticks_ / 60, (ticks_ % 60) * 100 / 60,
                          enemy_defeated_ ? "DEFEATED" : "AVOIDED", deaths_);
            text(fb, result, 170, 190, 2);
            char best[64];
            std::snprintf(best, sizeof(best), "%sBEST %u.%02uS  CLEARS %u", new_best_ ? "NEW " : "",
                          best_ticks_ / 60, (best_ticks_ % 60) * 100 / 60, clears_);
            text(fb, best, 200, 220, 2, pc2ps3::Color{150, 220, 255, 255});
            text(fb, "PRESS START TO PLAY AGAIN", 200, 250, 2);
        }
        fb.draw_debug_hud(16, 8, platform_.measured_fps(), heap_in_use(), 2);
        // QA: dump the real framebuffer (no window focus or key injection needed).
        const unsigned shot = phase_ == Phase::kVictory ? 1000u + result_ticks_ : ticks_;
        if (autoplay_ && shot != last_shot_ && (shot == 60 || shot == 130 || shot == 1100)) {
            last_shot_ = shot;
            char path[96];
            std::snprintf(path, sizeof(path), "%s/qa_frame_%u.ppm", kDataRoot, shot);
            std::printf("[pc2ps3] qa frame %s %s\n", path, fb.write_ppm(path) ? "ok" : "failed");
        }
        return true;
    }

    void stop() override {
        platform_.lock_audio();
        audio_.stop_category(pc2ps3::AudioCategory::kMusic);
        audio_.stop_category(pc2ps3::AudioCategory::kEffects);
        platform_.unlock_audio();
        player_atlas_.reset();
        std::printf("[pc2ps3] slice stopped after %u ticks\n", ticks_);
    }

private:
    void restart() {
        world_.reset();
        enemies_.clear();
        enemies_.spawn(kEnemyId, pc2ps3::Aabb{1000.0f, kFloorY - 32.0f, 1028.0f, kFloorY}, kEnemyHealth);
        checkpoints_.reset();
        triggers_.reset();
        combat_.reset();
        animation_.play(run_clip_);
        phase_ = Phase::kPlaying;
        result_ticks_ = 0;
        paused_ = false;
        checkpoint_reached_ = false;
        enemy_defeated_ = false;
        deaths_ = 0;
        ticks_ = 0;
        attack_timer_ = 0;
        hurt_timer_ = 0;
        enemy_dir_ = 1;
        facing_ = 1;
        std::printf("[pc2ps3] stage start\n");
    }

    // Save slot 0 = USRDIR/slot_0.pc2save: best time (ticks) and clear count, big endian.
    void load_progress() {
        std::vector<std::uint8_t> data;
        const pc2ps3::SaveStatus status = saves_.load(0, data);
        if (status == pc2ps3::SaveStatus::kOk && data.size() == 8) {
            best_ticks_ = (static_cast<unsigned>(data[0]) << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            clears_ = (static_cast<unsigned>(data[4]) << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
        }
        std::printf("[pc2ps3] save load status=%d best=%u clears=%u\n", static_cast<int>(status),
                    best_ticks_, clears_);
    }

    void save_progress() {
        std::vector<std::uint8_t> data(8);
        for (int i = 0; i < 4; ++i) {
            data[i] = static_cast<std::uint8_t>(best_ticks_ >> (24 - 8 * i));
            data[4 + i] = static_cast<std::uint8_t>(clears_ >> (24 - 8 * i));
        }
        std::printf("[pc2ps3] save write status=%d\n", static_cast<int>(saves_.save(0, data)));
    }

    void load_player_atlas(std::size_t sprite) {
        if (!file_system_.valid()) return;
        std::unique_ptr<pc2ps3::RuntimeDemoAtlas> atlas(new pc2ps3::RuntimeDemoAtlas(file_system_));
        if (atlas->load(kPlayerAtlas, sprite)) {
            player_atlas_.swap(atlas);
            player_sprite_ = sprite;
            std::printf("[pc2ps3] player sprite: atlas %s index %u\n", kPlayerAtlas,
                        static_cast<unsigned>(sprite));
            // Animated atlases name sprites <clip>_NN; pick them up as clips.
            static const char* const kClipNames[kClipCount] = {"idle_", "run_", "jump_", "attack_"};
            const std::vector<pc2ps3::AtlasSpriteRegion>& all = player_atlas_->index().sprites;
            for (int c = 0; c < kClipCount; ++c) {
                clips_[c].clear();
                for (std::size_t i = 0; i < all.size(); ++i) {
                    if (all[i].id.compare(0, std::strlen(kClipNames[c]), kClipNames[c]) == 0) clips_[c].push_back(i);
                }
            }
            std::printf("[pc2ps3] player animation frames idle=%u run=%u jump=%u attack=%u\n",
                        static_cast<unsigned>(clips_[kIdle].size()), static_cast<unsigned>(clips_[kRun].size()),
                        static_cast<unsigned>(clips_[kJump].size()), static_cast<unsigned>(clips_[kAttack].size()));
        } else if (player_atlas_.get() == 0) {
            std::printf("[pc2ps3] player sprite: original art (no %s/%s)\n", kDataRoot, kPlayerAtlas);
        }
    }

    void draw_player(pc2ps3::Framebuffer& fb, int x, int baseline) {
        if (player_atlas_.get() != 0) {
            const pc2ps3::Texture* tex = player_atlas_->texture();
            const pc2ps3::AtlasSpriteRegion* r = player_atlas_->sprite_region();
            const bool anim = animated();
            if (anim) r = &player_atlas_->index().sprites[animation_frame()];
            if (tex != 0 && r != 0 && r->height > 0) {
                // Animated frames: half size, bottom-center pivot. Single sprite: fit to 48 px tall.
                const int h = anim ? std::max(1, static_cast<int>(r->height / 2)) : 48;
                const int w = std::max(1, static_cast<int>(r->width * h / r->height));
                if (fb.draw_texture_region_scaled(*tex, static_cast<int>(r->x), static_cast<int>(r->y),
                                                  static_cast<int>(r->width), static_cast<int>(r->height),
                                                  x - w / 2, baseline - h, w, h, facing_ < 0)) {
                    return;
                }
            }
        }
        fb.draw_animation_frame(character_, animation_, x - 12, baseline - 32);
    }

    bool animated() const { return player_atlas_.get() != 0 && !clips_[kIdle].empty() && !clips_[kRun].empty(); }

    // Presentation only: picks a frame from the game state; it never feeds back into physics.
    std::size_t animation_frame() const {
        const pc2ps3::Actor& a = world_.player();
        const std::vector<std::size_t>* clip = &clips_[kIdle];
        std::size_t i = ticks_ / 8;
        if (attack_timer_ > 0 && !clips_[kAttack].empty()) {
            clip = &clips_[kAttack];
            i = static_cast<std::size_t>(kAttackTicks - attack_timer_) * clip->size() / kAttackTicks;
        } else if (!a.grounded && !clips_[kJump].empty()) {
            clip = &clips_[kJump];  // rising frames first, falling frames last
            const float t = std::max(0.0f, std::min(0.999f, (a.velocity.y + 420.0f) / 840.0f));
            i = static_cast<std::size_t>(t * clip->size());
        } else if (std::fabs(a.velocity.x) > 1.0f) {
            clip = &clips_[kRun];
            i = ticks_ / 3;
        }
        return (*clip)[i % clip->size()];
    }

    void play(const std::shared_ptr<const pc2ps3::PcmClip>& clip) {
        pc2ps3::AudioVoiceHandle handle;
        platform_.lock_audio();
        audio_.play_effect(clip, pc2ps3::AudioCategory::kEffects, 1.0f, false, handle);
        platform_.unlock_audio();
    }

    pc2ps3::SdlRuntimePlatform& platform_;
    pc2ps3::World world_;
    pc2ps3::EnemySystem enemies_;
    pc2ps3::HitboxSystem combat_;
    pc2ps3::TriggerSystem triggers_;
    pc2ps3::CheckpointSystem checkpoints_;
    pc2ps3::Camera2D camera_;
    pc2ps3::Texture character_;
    pc2ps3::AnimationClip run_clip_;
    pc2ps3::AnimationClip attack_clip_;
    pc2ps3::AnimationPlayer animation_;
    pc2ps3::PosixFileSystem file_system_;
    pc2ps3::SaveManager saves_;
    unsigned best_ticks_ = 0;
    unsigned clears_ = 0;
    bool new_best_ = false;
    bool autoplay_ = false;
    std::unique_ptr<pc2ps3::RuntimeDemoAtlas> player_atlas_;
    pc2ps3::AudioManager audio_;
    std::shared_ptr<const pc2ps3::PcmClip> jump_sfx_, hit_sfx_, checkpoint_sfx_, hurt_sfx_, victory_sfx_;
    Phase phase_ = Phase::kPlaying;
    bool paused_;
    bool checkpoint_reached_ = false;
    bool enemy_defeated_ = false;
    unsigned ticks_;
    unsigned deaths_ = 0;
    unsigned result_ticks_ = 0;
    unsigned last_shot_ = ~0u;
    int attack_timer_ = 0;
    int hurt_timer_ = 0;
    int enemy_dir_ = 1;
    int facing_ = 1;
    std::size_t player_sprite_;
    enum { kIdle, kRun, kJump, kAttack, kClipCount };
    std::vector<std::size_t> clips_[kClipCount];
};

} // namespace

int main(int, char**) {
    std::printf("[pc2ps3] boot\n");
    pc2ps3::SdlRuntimePlatform platform(1280, 720);
    pc2ps3::Framebuffer framebuffer(kWidth, kHeight);
    const pc2ps3::FixedStepConfig timing(16666667u, 100000000u, 5u);
    std::unique_ptr<pc2ps3::RuntimeApplication> app;
#ifdef PC2PS3_LOCAL_GAME
    if (cup::root_pack_data_present(kDataRoot)) app.reset(cup::create_root_pack_app(platform, kDataRoot));
#endif
    if (app.get() == 0) app.reset(new Slice(platform));
    const pc2ps3::RuntimeLoopStatus status =
        pc2ps3::RuntimeLoop().run(platform, *app, framebuffer, timing);
    std::printf("[pc2ps3] exit status %d\n", static_cast<int>(status));
    return status == pc2ps3::RuntimeLoopStatus::kQuit ? 0 : 1;
}
