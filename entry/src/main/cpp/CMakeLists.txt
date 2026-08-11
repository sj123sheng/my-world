cmake_minimum_required(VERSION 3.5)
project(native_game)

# ---- include paths (relative to entry/src/main/cpp) ----
# native/ is at project root: ../../../../native
set(NATIVE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../native)

include_directories(
    ${NATIVE_ROOT}
    ${NATIVE_ROOT}/engine/core
    ${NATIVE_ROOT}/engine/input
    ${NATIVE_ROOT}/engine/math
    ${NATIVE_ROOT}/engine/render
    ${NATIVE_ROOT}/engine/presentation
    ${NATIVE_ROOT}/engine/resource
    ${NATIVE_ROOT}/engine/world
    ${NATIVE_ROOT}/gameplay/combat
    ${NATIVE_ROOT}/gameplay/ai
    ${NATIVE_ROOT}/gameplay/entities
    ${NATIVE_ROOT}/gameplay/growth
    ${NATIVE_ROOT}/gameplay/inventory
    ${NATIVE_ROOT}/gameplay/player
    ${NATIVE_ROOT}/gameplay/quest
    ${NATIVE_ROOT}/gameplay/targeting
    ${NATIVE_ROOT}/gameplay/world
    ${NATIVE_ROOT}/platform/harmony
    # 构建期代码生成产物（world_layout.gen.h，由 automation/assets/generate_world_layout.mjs 生成）
    ${NATIVE_ROOT}/generated
)

# ---- source files ----
add_library(native_game SHARED
    # bridge
    native_bridge.cpp

    # engine core
    ${NATIVE_ROOT}/engine/core/loop.cpp
    ${NATIVE_ROOT}/engine/core/tick_clock.h
    ${NATIVE_ROOT}/engine/core/fixed_step.h
    ${NATIVE_ROOT}/engine/core/game_snapshot.h
    ${NATIVE_ROOT}/engine/core/lifecycle_state.h
    ${NATIVE_ROOT}/engine/core/snapshot_store.h
    ${NATIVE_ROOT}/engine/core/event_queue.h

    # engine input
    ${NATIVE_ROOT}/engine/input/input_queue.h
    ${NATIVE_ROOT}/engine/input/input_event.h
    ${NATIVE_ROOT}/engine/input/pointer_input.h

   # engine render
   ${NATIVE_ROOT}/engine/render/camera.cpp
   ${NATIVE_ROOT}/engine/render/surface.cpp
  ${NATIVE_ROOT}/engine/render/camera3d.cpp
   ${NATIVE_ROOT}/engine/render/mesh.cpp
   ${NATIVE_ROOT}/engine/render/terrain_mesh.cpp
   ${NATIVE_ROOT}/engine/render/chunked_terrain.cpp
   ${NATIVE_ROOT}/engine/render/texture.cpp
   ${NATIVE_ROOT}/engine/render/texture_asset.cpp
   ${NATIVE_ROOT}/engine/render/environment_quality.cpp
   ${NATIVE_ROOT}/engine/render/foliage_renderer.cpp
   ${NATIVE_ROOT}/engine/render/terrain_material.cpp
    ${NATIVE_ROOT}/engine/render/shader_3d.cpp
    ${NATIVE_ROOT}/engine/render/skinned_model.cpp
    ${NATIVE_ROOT}/engine/render/static_model.cpp
   ${NATIVE_ROOT}/engine/render/environment.cpp
    ${NATIVE_ROOT}/engine/render/asset_profile.cpp
    ${NATIVE_ROOT}/engine/render/digit_atlas.cpp
    ${NATIVE_ROOT}/engine/render/digit_atlas.h

    # engine presentation
    ${NATIVE_ROOT}/engine/presentation/vfx_system.cpp
    ${NATIVE_ROOT}/engine/presentation/vfx_system.h
    ${NATIVE_ROOT}/engine/presentation/performance_guard.cpp
    ${NATIVE_ROOT}/engine/presentation/performance_guard.h
    ${NATIVE_ROOT}/engine/presentation/damage_numbers.cpp
    ${NATIVE_ROOT}/engine/presentation/damage_numbers.h
    ${NATIVE_ROOT}/engine/presentation/procedural_sound.cpp
    ${NATIVE_ROOT}/engine/presentation/procedural_sound.h

    # engine resource
    ${NATIVE_ROOT}/engine/resource/loader.cpp
    ${NATIVE_ROOT}/engine/resource/save.cpp

    # engine world (开放世界探索基础)
    ${NATIVE_ROOT}/engine/world/world_grid.cpp
    ${NATIVE_ROOT}/engine/world/terrain_heightfield.cpp
    ${NATIVE_ROOT}/engine/world/stream_scheduler.cpp
    ${NATIVE_ROOT}/engine/world/environment_collision.cpp
    ${NATIVE_ROOT}/engine/world/weather_system.cpp
    ${NATIVE_ROOT}/engine/world/foliage_system.cpp
    ${NATIVE_ROOT}/engine/world/water_body.cpp

    # gameplay combat
    ${NATIVE_ROOT}/gameplay/combat/source_aura.cpp
    ${NATIVE_ROOT}/gameplay/combat/resonance.cpp
    ${NATIVE_ROOT}/gameplay/combat/action_state_machine.cpp
    ${NATIVE_ROOT}/gameplay/combat/combat_resources.cpp
    ${NATIVE_ROOT}/gameplay/combat/training_pulse.cpp
    ${NATIVE_ROOT}/gameplay/combat/training_target.cpp
    ${NATIVE_ROOT}/gameplay/combat/damage_resolver.cpp
    ${NATIVE_ROOT}/gameplay/combat/source_reaction_system.cpp
    ${NATIVE_ROOT}/gameplay/combat/combat_controller.cpp

    # gameplay AI contracts
    ${NATIVE_ROOT}/gameplay/ai/enemy_ai_types.h
    ${NATIVE_ROOT}/gameplay/ai/enemy_ai_config.h
    ${NATIVE_ROOT}/gameplay/ai/perception_system.cpp
    ${NATIVE_ROOT}/gameplay/ai/perception_system.h
    ${NATIVE_ROOT}/gameplay/ai/decision_policy.cpp
    ${NATIVE_ROOT}/gameplay/ai/decision_policy.h
    ${NATIVE_ROOT}/gameplay/ai/tactical_planner.cpp
    ${NATIVE_ROOT}/gameplay/ai/tactical_planner.h
    ${NATIVE_ROOT}/gameplay/ai/action_executor.cpp
    ${NATIVE_ROOT}/gameplay/ai/action_executor.h
    ${NATIVE_ROOT}/gameplay/ai/combat_region.cpp
    ${NATIVE_ROOT}/gameplay/ai/combat_region.h
    ${NATIVE_ROOT}/gameplay/ai/enemy_agent.cpp
    ${NATIVE_ROOT}/gameplay/ai/enemy_agent.h
    ${NATIVE_ROOT}/gameplay/ai/enemy_archetypes.cpp
    ${NATIVE_ROOT}/gameplay/ai/enemy_archetypes.h
    ${NATIVE_ROOT}/gameplay/ai/encounter_controller.cpp
    ${NATIVE_ROOT}/gameplay/flow/demo_director.cpp
    ${NATIVE_ROOT}/gameplay/flow/story_director.cpp
    ${NATIVE_ROOT}/gameplay/ai/encounter_controller.h
    ${NATIVE_ROOT}/gameplay/ai/wild_spawn_system.cpp
    ${NATIVE_ROOT}/gameplay/ai/wild_spawn_system.h

    # gameplay entities
    ${NATIVE_ROOT}/gameplay/entities/boss.cpp
    ${NATIVE_ROOT}/gameplay/entities/boss.h

    # gameplay player
    ${NATIVE_ROOT}/gameplay/player/character.cpp
    ${NATIVE_ROOT}/gameplay/player/player_controller.cpp
    ${NATIVE_ROOT}/gameplay/player/exploration_motion.cpp

    # gameplay world
    ${NATIVE_ROOT}/gameplay/world/teleport_anchor.cpp
    ${NATIVE_ROOT}/gameplay/world/interactable.cpp
    ${NATIVE_ROOT}/gameplay/world/exploration_content.cpp
    ${NATIVE_ROOT}/gameplay/world/world_terrain.cpp
    ${NATIVE_ROOT}/gameplay/world/terrain_wall_collision.cpp
    ${NATIVE_ROOT}/gameplay/world/exploration_gate_collision.cpp
    ${NATIVE_ROOT}/gameplay/world/exploration_feedback.cpp
    ${NATIVE_ROOT}/gameplay/world/npc_agent.cpp
    ${NATIVE_ROOT}/gameplay/world/npc_agent.h

    # gameplay quest (内容与任务系统)
    ${NATIVE_ROOT}/gameplay/quest/quest_system.cpp
    ${NATIVE_ROOT}/gameplay/quest/side_quests.cpp
    ${NATIVE_ROOT}/gameplay/quest/daily_quest.cpp
    ${NATIVE_ROOT}/gameplay/quest/dialog.cpp
    ${NATIVE_ROOT}/gameplay/growth/weapon_system.cpp

    # gameplay targeting
    ${NATIVE_ROOT}/gameplay/targeting/soft_targeting.cpp

    # gameplay growth
    ${NATIVE_ROOT}/gameplay/growth/relic.cpp
    ${NATIVE_ROOT}/gameplay/growth/character_growth.cpp
    ${NATIVE_ROOT}/gameplay/growth/gacha_system.cpp
    ${NATIVE_ROOT}/gameplay/growth/adventure_rank.cpp
    ${NATIVE_ROOT}/gameplay/growth/artifact_system.cpp

    # gameplay inventory
    ${NATIVE_ROOT}/gameplay/inventory/inventory.cpp

    # platform harmony
    ${NATIVE_ROOT}/platform/harmony/lifecycle.cpp
    ${NATIVE_ROOT}/platform/harmony/audio_bridge.cpp
    ${NATIVE_ROOT}/platform/harmony/audio_bridge.h
    ${NATIVE_ROOT}/platform/harmony/audio.cpp
    ${NATIVE_ROOT}/platform/harmony/fence_wait.cpp
)

# 工程根目录（${NATIVE_ROOT}/..）作为 include 根，使 "native/generated/world_layout.gen.h"
# 与既有 "native/..." 风格 include 均可解析。
target_include_directories(native_game PRIVATE ${NATIVE_ROOT}/..)

target_compile_features(native_game PUBLIC cxx_std_17)

target_compile_definitions(native_game PUBLIC OHOS_PLATFORM)

target_link_libraries(native_game PUBLIC
    libace_napi.z.so
    libace_ndk.z.so
    libnative_window.so
    libnative_buffer.so
    libEGL.so
    libGLESv3.so
    libhilog_ndk.z.so
    libohaudio.so
)
