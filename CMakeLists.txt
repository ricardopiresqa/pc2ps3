cmake_minimum_required(VERSION 3.16)
project(PC2PS3 VERSION 0.1.0 LANGUAGES CXX)

option(PC2PS3_BUILD_TESTS "Build deterministic runtime tests" ON)
option(PC2PS3_BUILD_TOOLS "Expose asset analysis tools" ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(pc2ps3_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

add_library(pc2ps3_core
  src/core/audio.cpp
  src/core/animation.cpp
  src/core/camera.cpp
  src/core/checkpoint.cpp
  src/core/boss_encounter.cpp
  src/core/campaign_progression.cpp
  src/core/enemy.cpp
  src/core/hitbox.cpp
 src/core/combat_resolution.cpp
 src/core/parry_window.cpp
 src/core/projectile_pattern.cpp
 src/core/weapon_system.cpp
 src/core/boss_pattern.cpp
 src/core/coop_input_router.cpp
 src/core/local_coop_session.cpp
  src/core/input.cpp
src/core/fixed_step_scheduler.cpp
src/core/frame_time.cpp
src/core/fidelity_trace.cpp
src/core/fidelity_compare.cpp
src/core/performance_budget.cpp
src/core/input_tick_distributor.cpp
src/core/runtime_loop.cpp
  src/core/physics.cpp
  src/core/image_decoder.cpp
  src/core/jump_assist.cpp
  src/core/knockback.cpp
  src/core/particles.cpp
  src/core/projectiles.cpp
  src/core/render.cpp
  src/core/render_batch.cpp
  src/core/scene_manager.cpp
  src/core/menu_flow.cpp
  src/core/state_machine.cpp
  src/core/storage.cpp
  src/core/tilemap.cpp
  src/core/trigger.cpp
  src/core/atlas_runtime_index.cpp
  src/core/texture_manager.cpp
  src/core/world.cpp
  src/platform/platform_input.cpp
  src/platform/audio_callback.cpp
  src/platform/bundle_file_system.cpp
  src/platform/framebuffer_presenter.cpp
)
if(WIN32)
  target_sources(pc2ps3_core PRIVATE src/platform/host_file_system.cpp)
else()
  target_sources(pc2ps3_core PRIVATE src/platform/posix_file_system.cpp)
endif()
target_include_directories(pc2ps3_core PUBLIC include)
pc2ps3_enable_warnings(pc2ps3_core)

add_executable(pc2ps3_demo apps/headless/main.cpp)
target_link_libraries(pc2ps3_demo PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_demo)
add_executable(pc2ps3_prototype apps/headless/prototype.cpp)
target_link_libraries(pc2ps3_prototype PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_prototype)
add_executable(pc2ps3_runtime_demo apps/headless/runtime_demo.cpp)
target_link_libraries(pc2ps3_runtime_demo PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_runtime_demo)
add_executable(pc2ps3_phase6_combat_demo apps/headless/phase6_combat_demo.cpp)
target_link_libraries(pc2ps3_phase6_combat_demo PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_phase6_combat_demo)

if(WIN32)
    add_library(pc2ps3_pc_platform
        src/platform/win32_framebuffer.cpp
 src/platform/win32_audio_output.cpp
 src/platform/win32_gamepad.cpp
        src/platform/win32_runtime_platform.cpp
    )
    target_include_directories(pc2ps3_pc_platform
        PUBLIC include
        PRIVATE src/platform
    )
 target_link_libraries(pc2ps3_pc_platform PUBLIC pc2ps3_core user32 gdi32 winmm)
    pc2ps3_enable_warnings(pc2ps3_pc_platform)

add_executable(pc2ps3_pc_demo
    apps/pc/runtime_demo.cpp
    apps/pc/runtime_demo_atlas.cpp)
    target_link_libraries(pc2ps3_pc_demo PRIVATE pc2ps3_pc_platform)
    pc2ps3_enable_warnings(pc2ps3_pc_demo)
endif()

if(PC2PS3_BUILD_TESTS)
  enable_testing()

  add_executable(pc2ps3_core_test tests/core_test.cpp)
  target_link_libraries(pc2ps3_core_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_core_test)
  add_test(NAME core_determinism COMMAND pc2ps3_core_test)
add_test(NAME demo_smoke COMMAND pc2ps3_demo)
add_test(NAME prototype_vertical_slice COMMAND pc2ps3_prototype)

add_executable(pc2ps3_moving_platform_test tests/moving_platform_test.cpp)
target_link_libraries(pc2ps3_moving_platform_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_moving_platform_test)
add_test(NAME moving_platforms COMMAND pc2ps3_moving_platform_test)

add_executable(pc2ps3_hitbox_test tests/hitbox_test.cpp)
target_link_libraries(pc2ps3_hitbox_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_hitbox_test)
add_test(NAME hitboxes COMMAND pc2ps3_hitbox_test)

add_executable(pc2ps3_knockback_test tests/knockback_test.cpp)
target_link_libraries(pc2ps3_knockback_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_knockback_test)
add_test(NAME knockback COMMAND pc2ps3_knockback_test)

add_executable(pc2ps3_camera_test tests/camera_test.cpp)
target_link_libraries(pc2ps3_camera_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_camera_test)
add_test(NAME camera COMMAND pc2ps3_camera_test)

add_executable(pc2ps3_enemy_test tests/enemy_test.cpp)
target_link_libraries(pc2ps3_enemy_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_enemy_test)
add_test(NAME enemies COMMAND pc2ps3_enemy_test)

add_executable(pc2ps3_debug_hud_test tests/debug_hud_test.cpp)
target_link_libraries(pc2ps3_debug_hud_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_debug_hud_test)
add_test(NAME debug_hud COMMAND pc2ps3_debug_hud_test)

add_executable(pc2ps3_render_text_test tests/render_text_test.cpp)
target_link_libraries(pc2ps3_render_text_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_render_text_test)
add_test(NAME render_text COMMAND pc2ps3_render_text_test)

add_executable(pc2ps3_vfx_overlay_test tests/vfx_overlay_test.cpp)
target_link_libraries(pc2ps3_vfx_overlay_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_vfx_overlay_test)
add_test(NAME vfx_overlay COMMAND pc2ps3_vfx_overlay_test)

add_executable(pc2ps3_frame_time_test tests/frame_time_test.cpp)
target_link_libraries(pc2ps3_frame_time_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_frame_time_test)
add_test(NAME frame_time COMMAND pc2ps3_frame_time_test)

add_executable(pc2ps3_fidelity_trace_test tests/fidelity_trace_test.cpp)
target_link_libraries(pc2ps3_fidelity_trace_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_fidelity_trace_test)
add_test(NAME fidelity_trace COMMAND pc2ps3_fidelity_trace_test)

add_executable(pc2ps3_fidelity_compare_test tests/fidelity_compare_test.cpp)
target_link_libraries(pc2ps3_fidelity_compare_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_fidelity_compare_test)
add_test(NAME fidelity_compare COMMAND pc2ps3_fidelity_compare_test)

add_executable(pc2ps3_performance_budget_test tests/performance_budget_test.cpp)
target_link_libraries(pc2ps3_performance_budget_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_performance_budget_test)
add_test(NAME performance_budget COMMAND pc2ps3_performance_budget_test)

add_executable(pc2ps3_render_texture_test tests/render_texture_test.cpp)
target_link_libraries(pc2ps3_render_texture_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_render_texture_test)
add_test(NAME render_texture COMMAND pc2ps3_render_texture_test)
add_executable(pc2ps3_render_transform_test tests/render_transform_test.cpp)
target_link_libraries(pc2ps3_render_transform_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_render_transform_test)
add_test(NAME render_transform COMMAND pc2ps3_render_transform_test)
add_executable(pc2ps3_render_batch_test tests/render_batch_test.cpp)
target_link_libraries(pc2ps3_render_batch_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_render_batch_test)
add_test(NAME render_batch COMMAND pc2ps3_render_batch_test)
add_executable(pc2ps3_image_decoder_test tests/image_decoder_test.cpp)
target_link_libraries(pc2ps3_image_decoder_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_image_decoder_test)
add_test(NAME image_decoder COMMAND pc2ps3_image_decoder_test)
set_tests_properties(image_decoder PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(pc2ps3_tilemap_test tests/tilemap_test.cpp)
target_link_libraries(pc2ps3_tilemap_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_tilemap_test)
add_test(NAME tilemap COMMAND pc2ps3_tilemap_test)
add_executable(pc2ps3_particles_test tests/particles_test.cpp)
target_link_libraries(pc2ps3_particles_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_particles_test)
add_test(NAME particles COMMAND pc2ps3_particles_test)
add_executable(pc2ps3_texture_manager_test tests/texture_manager_test.cpp)
target_link_libraries(pc2ps3_texture_manager_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_texture_manager_test)
add_test(NAME texture_manager COMMAND pc2ps3_texture_manager_test)
add_executable(pc2ps3_atlas_runtime_index_test tests/atlas_runtime_index_test.cpp)
target_link_libraries(pc2ps3_atlas_runtime_index_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_atlas_runtime_index_test)
add_test(NAME atlas_runtime_index COMMAND pc2ps3_atlas_runtime_index_test)

add_executable(pc2ps3_jump_assist_test tests/jump_assist_test.cpp)
target_link_libraries(pc2ps3_jump_assist_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_jump_assist_test)
add_test(NAME jump_assist COMMAND pc2ps3_jump_assist_test)

add_executable(pc2ps3_trigger_test tests/trigger_test.cpp)
target_link_libraries(pc2ps3_trigger_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_trigger_test)
add_test(NAME triggers COMMAND pc2ps3_trigger_test)

add_executable(pc2ps3_state_machine_test tests/state_machine_test.cpp)
target_link_libraries(pc2ps3_state_machine_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_state_machine_test)
add_test(NAME state_machine COMMAND pc2ps3_state_machine_test)

add_executable(pc2ps3_checkpoint_test tests/checkpoint_test.cpp)
target_link_libraries(pc2ps3_checkpoint_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_checkpoint_test)
add_test(NAME checkpoints COMMAND pc2ps3_checkpoint_test)

add_executable(pc2ps3_projectiles_test tests/projectiles_test.cpp)
target_link_libraries(pc2ps3_projectiles_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_projectiles_test)
add_test(NAME projectiles COMMAND pc2ps3_projectiles_test)

add_executable(pc2ps3_animation_render_test tests/animation_render_test.cpp)
target_link_libraries(pc2ps3_animation_render_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_animation_render_test)
add_test(NAME animation_render COMMAND pc2ps3_animation_render_test)

  add_executable(pc2ps3_physics_test tests/physics_test.cpp)
  target_link_libraries(pc2ps3_physics_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_physics_test)
  add_test(NAME physics_aabb COMMAND pc2ps3_physics_test)

  add_executable(pc2ps3_input_test tests/input_test.cpp)
  target_link_libraries(pc2ps3_input_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_input_test)
  add_test(NAME input_mapping COMMAND pc2ps3_input_test)
add_executable(pc2ps3_input_actions_test tests/input_actions_test.cpp)
target_link_libraries(pc2ps3_input_actions_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_input_actions_test)
add_test(NAME input_actions COMMAND pc2ps3_input_actions_test)

  add_executable(pc2ps3_animation_test tests/animation_test.cpp)
  target_link_libraries(pc2ps3_animation_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_animation_test)
  add_test(NAME animation_playback COMMAND pc2ps3_animation_test)

  add_executable(pc2ps3_animation_event_test tests/animation_event_test.cpp)
  target_link_libraries(pc2ps3_animation_event_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_animation_event_test)
  add_test(NAME animation_events COMMAND pc2ps3_animation_event_test)

  add_executable(pc2ps3_scene_manager_test tests/scene_manager_test.cpp)
  target_link_libraries(pc2ps3_scene_manager_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_scene_manager_test)
  add_test(NAME scene_manager_lifecycle COMMAND pc2ps3_scene_manager_test)

  add_executable(pc2ps3_storage_test tests/storage_test.cpp)
  target_link_libraries(pc2ps3_storage_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_storage_test)
  add_test(NAME storage_and_saves COMMAND pc2ps3_storage_test)

  add_executable(pc2ps3_bundle_file_system_test
    tests/bundle_file_system_test.cpp)
  target_link_libraries(pc2ps3_bundle_file_system_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_bundle_file_system_test)

  add_executable(pc2ps3_bundle_texture_integration_test
    tests/bundle_texture_integration_test.cpp)
  target_link_libraries(pc2ps3_bundle_texture_integration_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_bundle_texture_integration_test)
if(NOT WIN32)
    add_executable(pc2ps3_posix_file_system_test tests/posix_file_system_test.cpp)
    target_link_libraries(pc2ps3_posix_file_system_test PRIVATE pc2ps3_core)
    pc2ps3_enable_warnings(pc2ps3_posix_file_system_test)
    add_test(NAME posix_file_system COMMAND pc2ps3_posix_file_system_test)
endif()
if(WIN32)
  add_executable(pc2ps3_host_file_system_test tests/host_file_system_test.cpp)
  target_link_libraries(pc2ps3_host_file_system_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_host_file_system_test)
  add_test(NAME host_file_system COMMAND pc2ps3_host_file_system_test)

  add_executable(pc2ps3_host_save_integration_test
    tests/host_save_integration_test.cpp)
  target_link_libraries(pc2ps3_host_save_integration_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_host_save_integration_test)
  add_test(NAME host_save_persistence COMMAND pc2ps3_host_save_integration_test)

  add_executable(pc2ps3_host_texture_integration_test
    tests/host_texture_integration_test.cpp)
  target_link_libraries(pc2ps3_host_texture_integration_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_host_texture_integration_test)
  add_test(NAME host_texture_loading
    COMMAND pc2ps3_host_texture_integration_test)
endif()

  add_executable(pc2ps3_audio_test tests/audio_test.cpp)
  target_link_libraries(pc2ps3_audio_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_audio_test)
  add_test(NAME audio_mixing COMMAND pc2ps3_audio_test)
  add_executable(pc2ps3_platform_input_test tests/platform_input_adapter_test.cpp)
  target_link_libraries(pc2ps3_platform_input_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_platform_input_test)
  add_test(NAME platform_input_adapter COMMAND pc2ps3_platform_input_test)
  add_executable(pc2ps3_audio_callback_test tests/audio_callback_test.cpp)
  target_link_libraries(pc2ps3_audio_callback_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_audio_callback_test)
  add_test(NAME audio_callback COMMAND pc2ps3_audio_callback_test)
  add_executable(pc2ps3_framebuffer_presenter_test tests/framebuffer_presenter_test.cpp)
  target_link_libraries(pc2ps3_framebuffer_presenter_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_framebuffer_presenter_test)
  add_test(NAME framebuffer_presenter COMMAND pc2ps3_framebuffer_presenter_test)
add_executable(pc2ps3_fixed_step_scheduler_test tests/fixed_step_scheduler_test.cpp)
target_link_libraries(pc2ps3_fixed_step_scheduler_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_fixed_step_scheduler_test)
add_test(NAME fixed_step_scheduler COMMAND pc2ps3_fixed_step_scheduler_test)
add_executable(pc2ps3_input_tick_distributor_test tests/input_tick_distributor_test.cpp)
target_link_libraries(pc2ps3_input_tick_distributor_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_input_tick_distributor_test)
add_test(NAME input_tick_distribution COMMAND pc2ps3_input_tick_distributor_test)
add_executable(pc2ps3_runtime_loop_test tests/runtime_loop_test.cpp)
target_link_libraries(pc2ps3_runtime_loop_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_runtime_loop_test)
add_test(NAME runtime_loop COMMAND pc2ps3_runtime_loop_test)
add_executable(pc2ps3_world_interpolation_test tests/world_interpolation_test.cpp)
target_link_libraries(pc2ps3_world_interpolation_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_world_interpolation_test)
add_test(NAME world_interpolation COMMAND pc2ps3_world_interpolation_test)
add_test(NAME runtime_demo_smoke COMMAND pc2ps3_runtime_demo)
endif()

if(PC2PS3_BUILD_TESTS)
  add_executable(pc2ps3_menu_flow_test tests/menu_flow_test.cpp)
  target_link_libraries(pc2ps3_menu_flow_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_menu_flow_test)
  add_test(NAME menu_flow COMMAND pc2ps3_menu_flow_test)

  add_executable(pc2ps3_campaign_progression_test
    tests/campaign_progression_test.cpp)
  target_link_libraries(pc2ps3_campaign_progression_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_campaign_progression_test)
  add_test(NAME campaign_progression COMMAND pc2ps3_campaign_progression_test)

 add_executable(pc2ps3_combat_resolution_test
  tests/combat_resolution_test.cpp)
 target_link_libraries(pc2ps3_combat_resolution_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_combat_resolution_test)
 add_test(NAME combat_resolution COMMAND pc2ps3_combat_resolution_test)

 add_executable(pc2ps3_parry_window_test tests/parry_window_test.cpp)
 target_link_libraries(pc2ps3_parry_window_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_parry_window_test)
 add_test(NAME parry_window COMMAND pc2ps3_parry_window_test)

 add_executable(pc2ps3_projectile_pattern_test
  tests/projectile_pattern_test.cpp)
 target_link_libraries(pc2ps3_projectile_pattern_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_projectile_pattern_test)
 add_test(NAME projectile_pattern COMMAND pc2ps3_projectile_pattern_test)

 add_executable(pc2ps3_weapon_system_test tests/weapon_system_test.cpp)
 target_link_libraries(pc2ps3_weapon_system_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_weapon_system_test)
 add_test(NAME weapon_system COMMAND pc2ps3_weapon_system_test)

 add_executable(pc2ps3_boss_pattern_test tests/boss_pattern_test.cpp)
 target_link_libraries(pc2ps3_boss_pattern_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_boss_pattern_test)
 add_test(NAME boss_pattern COMMAND pc2ps3_boss_pattern_test)

 add_executable(pc2ps3_phase6_combat_integration_test
  tests/phase6_combat_integration_test.cpp)
 target_link_libraries(pc2ps3_phase6_combat_integration_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_phase6_combat_integration_test)
 add_test(NAME phase6_combat_integration
  COMMAND pc2ps3_phase6_combat_integration_test)
add_test(NAME phase6_combat_demo COMMAND pc2ps3_phase6_combat_demo)

 add_executable(pc2ps3_coop_input_router_test
  tests/coop_input_router_test.cpp)
 target_link_libraries(pc2ps3_coop_input_router_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_coop_input_router_test)
 add_test(NAME coop_input_router COMMAND pc2ps3_coop_input_router_test)

 add_executable(pc2ps3_local_coop_session_test
  tests/local_coop_session_test.cpp)
 target_link_libraries(pc2ps3_local_coop_session_test PRIVATE pc2ps3_core)
 pc2ps3_enable_warnings(pc2ps3_local_coop_session_test)
 add_test(NAME local_coop_session COMMAND pc2ps3_local_coop_session_test)

 add_executable(pc2ps3_boss_encounter_test tests/boss_encounter_test.cpp)
  target_link_libraries(pc2ps3_boss_encounter_test PRIVATE pc2ps3_core)
  pc2ps3_enable_warnings(pc2ps3_boss_encounter_test)
  add_test(NAME boss_encounter COMMAND pc2ps3_boss_encounter_test)

if(WIN32)
    add_executable(pc2ps3_win32_framebuffer_test
        tests/win32_framebuffer_test.cpp
    )
    target_include_directories(pc2ps3_win32_framebuffer_test PRIVATE
        src/platform
        include
    )
    target_link_libraries(pc2ps3_win32_framebuffer_test PRIVATE
        pc2ps3_pc_platform
    )
    pc2ps3_enable_warnings(pc2ps3_win32_framebuffer_test)
    add_test(NAME win32_framebuffer COMMAND pc2ps3_win32_framebuffer_test)

 add_executable(pc2ps3_win32_gamepad_test tests/win32_gamepad_test.cpp)
 target_link_libraries(pc2ps3_win32_gamepad_test PRIVATE pc2ps3_pc_platform)
 pc2ps3_enable_warnings(pc2ps3_win32_gamepad_test)
 add_test(NAME win32_gamepad COMMAND pc2ps3_win32_gamepad_test)

 add_executable(pc2ps3_win32_frame_pacer_test tests/win32_frame_pacer_test.cpp)
 target_include_directories(pc2ps3_win32_frame_pacer_test PRIVATE src/platform)
 target_link_libraries(pc2ps3_win32_frame_pacer_test PRIVATE pc2ps3_pc_platform)
 pc2ps3_enable_warnings(pc2ps3_win32_frame_pacer_test)
 add_test(NAME win32_frame_pacer COMMAND pc2ps3_win32_frame_pacer_test)

add_executable(pc2ps3_runtime_demo_options_test
    tests/runtime_demo_options_test.cpp)
target_include_directories(pc2ps3_runtime_demo_options_test PRIVATE apps/pc)
pc2ps3_enable_warnings(pc2ps3_runtime_demo_options_test)
add_test(NAME runtime_demo_options COMMAND pc2ps3_runtime_demo_options_test)
add_executable(pc2ps3_runtime_demo_atlas_test
    tests/runtime_demo_atlas_test.cpp
    apps/pc/runtime_demo_atlas.cpp)
target_include_directories(pc2ps3_runtime_demo_atlas_test PRIVATE apps/pc)
target_link_libraries(pc2ps3_runtime_demo_atlas_test PRIVATE pc2ps3_core)
pc2ps3_enable_warnings(pc2ps3_runtime_demo_atlas_test)
add_test(NAME runtime_demo_atlas COMMAND pc2ps3_runtime_demo_atlas_test)
add_executable(pc2ps3_win32_runtime_platform_test
    tests/win32_runtime_platform_test.cpp)
target_link_libraries(pc2ps3_win32_runtime_platform_test PRIVATE
    pc2ps3_pc_platform)
pc2ps3_enable_warnings(pc2ps3_win32_runtime_platform_test)
add_test(NAME win32_runtime_platform
    COMMAND pc2ps3_win32_runtime_platform_test)
add_executable(pc2ps3_win32_audio_output_test
 tests/win32_audio_output_test.cpp
 )
 target_include_directories(pc2ps3_win32_audio_output_test PRIVATE src/platform)
 target_link_libraries(pc2ps3_win32_audio_output_test PRIVATE pc2ps3_pc_platform)
 pc2ps3_enable_warnings(pc2ps3_win32_audio_output_test)
 add_test(NAME win32_audio_output COMMAND pc2ps3_win32_audio_output_test)
endif()
endif()

if(PC2PS3_BUILD_TOOLS)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  if(PC2PS3_BUILD_TESTS)
add_test(NAME unityfs_header
COMMAND ${Python3_EXECUTABLE} -m unittest discover
-s tests -p test_unityfs_header.py
)
set_tests_properties(unityfs_header PROPERTIES
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME unityfs_lz4_metadata
COMMAND ${Python3_EXECUTABLE} -m unittest discover
-s tests -p test_lz4_metadata.py
)
set_tests_properties(unityfs_lz4_metadata PROPERTIES
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME unityfs_directory_info
COMMAND ${Python3_EXECUTABLE} -m unittest discover
-s tests -p test_unityfs_directory_info.py
)
set_tests_properties(unityfs_directory_info PROPERTIES
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME fidelity_trace_json
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
  -s tests -p test_fidelity_trace_json.py
)
set_tests_properties(fidelity_trace_json PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME fidelity_trace_comparison
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
  -s tests -p test_compare_fidelity_trace.py
)
set_tests_properties(fidelity_trace_comparison PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME performance_report
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
  -s tests -p test_performance_report.py
)
set_tests_properties(performance_report PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
    add_test(NAME asset_manifest_inventory
      COMMAND ${Python3_EXECUTABLE} -m unittest discover
      -s tests -p test_asset_manifest.py
    )
    set_tests_properties(asset_manifest_inventory PROPERTIES
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(NAME asset_package_pipeline
      COMMAND ${Python3_EXECUTABLE} -m unittest discover
      -s tests -p test_asset_pack.py
    )
    set_tests_properties(asset_package_pipeline PROPERTIES
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      ENVIRONMENT
        "PC2PS3_BUNDLE_FS_TEST=$<TARGET_FILE:pc2ps3_bundle_file_system_test>;PC2PS3_BUNDLE_TEXTURE_TEST=$<TARGET_FILE:pc2ps3_bundle_texture_integration_test>"
    )
  endif()
  add_custom_target(pc2ps3_asset_manifest_help
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/asset_manifest.py --help
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
    USES_TERMINAL
  )
endif()

install(TARGETS pc2ps3_core pc2ps3_demo
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
