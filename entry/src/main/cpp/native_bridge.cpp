#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <string>
#include <hilog/log.h>
#include <cmath>
#include <atomic>
#include <vector>
#include "engine/core/loop.h"
#include "engine/input/changed_pointer_forwarder.h"
#include "engine/input/pointer_input.h"
#include "native/platform/harmony/model_asset_commit.h"
#include "native/platform/harmony/environment_asset_commit.h"

#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "Ethelan", __VA_ARGS__)

static Loop g_loop;
static std::atomic_bool g_foregroundRequested{false};

static void InvalidateSurfaceSnapshot() {
  surface_destroy(g_loop.surface);
  g_loop.publishRendererStopped();
}

static napi_value ThrowInputTypeError(napi_env env, const char* message) {
  napi_throw_type_error(env, nullptr, message);
  return nullptr;
}

static bool CopyArrayBuffer(napi_env env, napi_value value,
                            std::vector<uint8_t>& out) {
  bool isArrayBuffer = false;
  void* bytes = nullptr;
  size_t length = 0;
  return napi_is_arraybuffer(env, value, &isArrayBuffer) == napi_ok &&
         isArrayBuffer &&
         napi_get_arraybuffer_info(env, value, &bytes, &length) == napi_ok &&
         bytes != nullptr && length > 0 &&
         (out.assign(static_cast<uint8_t*>(bytes),
                     static_cast<uint8_t*>(bytes) + length),
          true);
}

static bool GetNumberProperty(napi_env env, napi_value object, const char* name,
                              bool required, double& value) {
  bool hasProperty = false;
  if (napi_has_named_property(env, object, name, &hasProperty) != napi_ok) return false;
  if (!hasProperty) return !required;
  napi_value property = nullptr;
  napi_valuetype propertyType = napi_undefined;
  if (napi_get_named_property(env, object, name, &property) != napi_ok ||
      napi_typeof(env, property, &propertyType) != napi_ok || propertyType != napi_number ||
      napi_get_value_double(env, property, &value) != napi_ok || !std::isfinite(value)) {
    return false;
  }
  return true;
}

static void OnSurfaceCreated(OH_NativeXComponent* component, void* window) {
  g_loop.withLifecycle([window]() {
    LOGI("OnSurfaceCreated");
    OHNativeWindow* nativeWindow = static_cast<OHNativeWindow*>(window);
    if (g_loop.surface.ready) {
      g_loop.stop();
      InvalidateSurfaceSnapshot();
    }
    if (!surface_init(g_loop.surface, nativeWindow)) {
      LOGE("surface_init failed");
      InvalidateSurfaceSnapshot();
      return;
    }
    if (g_foregroundRequested.load()) {
      g_loop.start();
    }
  });
}

static void OnSurfaceChanged(OH_NativeXComponent* component, void* window) {
  g_loop.withLifecycle([window]() {
    LOGI("OnSurfaceChanged");
    OHNativeWindow* nativeWindow = static_cast<OHNativeWindow*>(window);
    if (nativeWindow == nullptr) {
      LOGE("OnSurfaceChanged: window is null");
      return;
    }
    g_loop.stop();
    if (!g_loop.surface.ready) {
      LOGI("OnSurfaceChanged: surface not ready yet, init now");
      if (!surface_init(g_loop.surface, nativeWindow)) {
        LOGE("surface_init failed in OnSurfaceChanged");
        InvalidateSurfaceSnapshot();
        return;
      }
    } else if (!surface_resize(g_loop.surface, nativeWindow)) {
      LOGE("surface resize failed");
      InvalidateSurfaceSnapshot();
      return;
    }
    if (g_foregroundRequested.load()) {
      g_loop.start();
    }
  });
}

static void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window) {
  g_loop.withLifecycle([]() {
    LOGI("OnSurfaceDestroyed");
    g_loop.stop();
    InvalidateSurfaceSnapshot();
  });
}

static void OnDispatchTouchEvent(OH_NativeXComponent* component, void* window) {
  OH_NativeXComponent_TouchEvent touchEvent{};
  if (OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent) !=
      OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    LOGE("OH_NativeXComponent_GetTouchEvent failed");
    return;
  }
  LOGI("touch type=%{public}d id=%{public}d x=%{public}f y=%{public}f",
       touchEvent.type, touchEvent.id, touchEvent.x, touchEvent.y);
  ForwardChangedPointer(
      static_cast<int32_t>(touchEvent.type), touchEvent.id, touchEvent.x,
      touchEvent.y,
      [](InputAction action, int32_t pointerId, float x, float y) {
        return g_loop.enqueueInput(action, pointerId, x, y);
      });
}

static napi_value NativeStart(napi_env env, napi_callback_info) {
  g_foregroundRequested.store(true);
  g_loop.start();
  return nullptr;
}

static napi_value NativeStop(napi_env env, napi_callback_info) {
  g_foregroundRequested.store(false);
  g_loop.stop();
  return nullptr;
}

// GamePage may finish loading after EntryAbility has already sent nativeStop.
// Unlike NativeStart, this cannot turn a background request back into foreground.
static napi_value NativeStartIfForeground(napi_env env, napi_callback_info) {
  if (g_foregroundRequested.load()) {
    g_loop.start();
  }
  return nullptr;
}

static napi_value NativeSetModelAssets(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3] = {nullptr, nullptr, nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 3) {
    napi_get_boolean(env, false, &result);
    return result;
  }

  const bool committed = CopyAndCommitModelAssets(
      [&env, &args](ModelAssetSlot slot, std::vector<uint8_t>& out) {
        return CopyArrayBuffer(env, args[static_cast<size_t>(slot)], out);
      },
      [](auto operation) { g_loop.withLifecycle(operation); },
      [](ModelAssetSlot slot, std::vector<uint8_t> bytes) {
        switch (slot) {
          case ModelAssetSlot::Player:
            g_loop.surface.setModelAsset(ModelKind::Player, std::move(bytes));
            break;
          case ModelAssetSlot::Enemy:
            g_loop.surface.setModelAsset(ModelKind::Enemy, std::move(bytes));
            break;
          case ModelAssetSlot::Boss:
            g_loop.surface.setModelAsset(ModelKind::Boss, std::move(bytes));
            break;
        }
      });
  if (!committed) {
    napi_get_boolean(env, false, &result);
    return result;
  }
  napi_get_boolean(env, true, &result);
  return result;
}

static napi_value NativeSetEnvironmentAssets(napi_env env,
                                             napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4] = {nullptr, nullptr, nullptr, nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 4) {
    napi_get_boolean(env, false, &result);
    return result;
  }

  const bool committed = CopyAndCommitEnvironmentAssets(
      [&env, &args](EnvironmentAssetSlot slot, std::vector<uint8_t>& out) {
        return CopyArrayBuffer(env, args[static_cast<size_t>(slot)], out);
      },
      [](auto operation) { g_loop.withLifecycle(operation); },
      [](EnvironmentAssetSlot slot, std::vector<uint8_t> bytes) {
        g_loop.surface.setEnvironmentAsset(
            static_cast<EnvironmentBatchKind>(static_cast<size_t>(slot)),
            std::move(bytes));
      });
  napi_get_boolean(env, committed, &result);
  return result;
}

// Phase 2：按区块懒注入环境批次资产（blockId, ArrayBuffer）。
// 注意：N-API 数值参数必须先解包为 double 再 TryConvertInt32。
static napi_value NativeSetBlockEnvironmentAsset(napi_env env,
                                                 napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr, nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 2) {
    napi_get_boolean(env, false, &result);
    return result;
  }
  napi_valuetype idType = napi_undefined;
  double idNumber = 0.0;
  if (args[0] == nullptr || napi_typeof(env, args[0], &idType) != napi_ok ||
      idType != napi_number ||
      napi_get_value_double(env, args[0], &idNumber) != napi_ok ||
      !std::isfinite(idNumber)) {
    napi_get_boolean(env, false, &result);
    return result;
  }
  int32_t blockId = -1;
  if (!TryConvertInt32(idNumber, blockId) || blockId < 0 ||
      blockId >= kEnvironmentBlockCount) {
    napi_get_boolean(env, false, &result);
    return result;
  }
  const bool committed = CopyAndCommitBlockEnvironmentAsset(
      blockId,
      [&env, &args](std::vector<uint8_t>& out) {
        return CopyArrayBuffer(env, args[1], out);
      },
      [](auto operation) { g_loop.withLifecycle(operation); },
      [](int32_t id, std::vector<uint8_t> bytes) {
        g_loop.surface.setBlockEnvironmentAsset(id, std::move(bytes));
      });
  napi_get_boolean(env, committed, &result);
  return result;
}

// Phase 4：注入 NPC 模型字节（ArrayBuffer）。缺失时渲染保持静态 Mesh
// 回退，不影响其余槽位；字节复制成功后才进入生命周期锁提交。
static napi_value NativeSetNpcAsset(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    napi_get_boolean(env, false, &result);
    return result;
  }
  std::vector<uint8_t> bytes;
  if (!CopyArrayBuffer(env, args[0], bytes)) {
    napi_get_boolean(env, false, &result);
    return result;
  }
  g_loop.withLifecycle([&bytes]() {
    g_loop.surface.setModelAsset(ModelKind::Npc, std::vector<uint8_t>(bytes));
  });
  napi_get_boolean(env, true, &result);
  return result;
}

static napi_value NativePushInput(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc != 1) {
    return ThrowInputTypeError(env, "pushInput expects exactly one event object");
  }
  napi_valuetype argumentType = napi_undefined;
  if (args[0] == nullptr || napi_typeof(env, args[0], &argumentType) != napi_ok ||
      argumentType != napi_object) {
    return ThrowInputTypeError(env, "pushInput event must be an object");
  }

  double typeNumber = 0.0;
  double pointerIdNumber;
  double x = 0.0;
  double y = 0.0;
  if (!GetNumberProperty(env, args[0], "type", true, typeNumber) ||
      !GetNumberProperty(env, args[0], "pointerId", true, pointerIdNumber) ||
      !GetNumberProperty(env, args[0], "x", true, x) ||
      !GetNumberProperty(env, args[0], "y", true, y)) {
    return ThrowInputTypeError(env, "pushInput requires numeric type/pointerId/x/y");
  }
  int32_t type = 0;
  InputAction action = InputAction::PointerCancel;
  if (!TryConvertInt32(typeNumber, type) || !TryMapPointerAction(type, action)) {
    return ThrowInputTypeError(env, "pushInput type must be a pointer action from 0 to 3");
  }
  int32_t pointerId = 0;
  if (!TryConvertInt32(pointerIdNumber, pointerId)) {
    return ThrowInputTypeError(env, "pushInput pointerId must be an integer");
  }
  float inputX = 0.0f;
  float inputY = 0.0f;
  if (!TryConvertFloat(x, inputX) || !TryConvertFloat(y, inputY)) {
    return ThrowInputTypeError(env, "pushInput x/y must fit finite native coordinates");
  }
  g_loop.enqueueInput(action, pointerId, inputX, inputY);
  return nullptr;
}

static napi_value NativePushAction(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc != 1) {
    return ThrowInputTypeError(env, "pushAction expects exactly one action type");
  }
  napi_valuetype argumentType = napi_undefined;
  double typeNumber = 0.0;
  if (args[0] == nullptr || napi_typeof(env, args[0], &argumentType) != napi_ok ||
      argumentType != napi_number || napi_get_value_double(env, args[0], &typeNumber) != napi_ok ||
      !std::isfinite(typeNumber)) {
    return ThrowInputTypeError(env, "pushAction type must be a finite integer from 0 to 10");
  }
  int32_t type = 0;
  if (!TryConvertInt32(typeNumber, type) || type < 0 || type > 10) {
    return ThrowInputTypeError(env, "pushAction type must be an integer from 0 to 10");
  }
  static constexpr InputAction kActions[] = {
      InputAction::Attack, InputAction::Dodge, InputAction::Radiance,
      InputAction::Current, InputAction::Corruption, InputAction::Ultimate,
      InputAction::Jump, InputAction::Interact, InputAction::GlidePress,
      InputAction::GlideRelease, InputAction::SwitchCharacter};
  const InputAction action = kActions[type];
  g_loop.enqueueInput(action, -1, 0.0f, 0.0f);
  return nullptr;
}

static napi_value NativeStartEncounter(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc != 1) {
    return ThrowInputTypeError(env, "startEncounter expects exactly one mode");
  }
  napi_valuetype argumentType = napi_undefined;
  double modeNumber = 0.0;
  if (args[0] == nullptr || napi_typeof(env, args[0], &argumentType) != napi_ok ||
      argumentType != napi_number || napi_get_value_double(env, args[0], &modeNumber) != napi_ok ||
     !std::isfinite(modeNumber)) {
    return ThrowInputTypeError(env, "startEncounter mode must be a finite integer from 0 to 5");
  }
  int32_t mode = 0;
  if (!TryConvertInt32(modeNumber, mode) || mode < 0 || mode > 5) {
    return ThrowInputTypeError(env, "startEncounter mode must be an integer from 0 to 5");
  }
  const bool started = g_loop.startEncounter(static_cast<EncounterMode>(mode));
  napi_value result = nullptr;
  napi_get_boolean(env, started, &result);
  return result;
}

static napi_value NativeAdvanceLevel(napi_env env, napi_callback_info) {
  const bool advanced = g_loop.advanceLevel();
  napi_value result = nullptr;
  napi_get_boolean(env, advanced, &result);
  return result;
}

static napi_value NativeUseSupply(napi_env env, napi_callback_info) {
  const bool supplied = g_loop.useSupply();
  napi_value result = nullptr;
  napi_get_boolean(env, supplied, &result);
  return result;
}

static napi_value NativeRetryBoss(napi_env env, napi_callback_info) {
  const bool retried = g_loop.retryBoss();
  napi_value result = nullptr;
  napi_get_boolean(env, retried, &result);
  return result;
}

static napi_value NativeToggleDebugHud(napi_env env, napi_callback_info) {
  g_loop.toggleDebugHud();
  return nullptr;
}

static napi_value NativeAdvanceDialog(napi_env env, napi_callback_info) {
  g_loop.advanceDialog();
  return nullptr;
}

static napi_value NativeTeleportToAnchor(napi_env env,
                                         napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "teleportToAnchor requires exactly one argument");
  }
  napi_valuetype argumentType;
  if (napi_typeof(env, args[0], &argumentType) != napi_ok ||
      argumentType != napi_number) {
    return ThrowInputTypeError(env, "teleportToAnchor anchorId must be a number");
  }
  double anchorIdNumber = 0.0;
  if (napi_get_value_double(env, args[0], &anchorIdNumber) != napi_ok ||
      !std::isfinite(anchorIdNumber)) {
    return ThrowInputTypeError(env, "teleportToAnchor anchorId must be finite");
  }
  int32_t anchorId = 0;
  if (!TryConvertInt32(anchorIdNumber, anchorId)) {
    return ThrowInputTypeError(env, "teleportToAnchor anchorId must be an integer");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.teleportToAnchor(anchorId), &result);
  return result;
}

static bool CopyInt32Argument(napi_env env, napi_value value, int32_t& out) {
  napi_valuetype valueType = napi_undefined;
  double number = 0.0;
  return value != nullptr &&
         napi_typeof(env, value, &valueType) == napi_ok &&
         valueType == napi_number &&
         napi_get_value_double(env, value, &number) == napi_ok &&
         std::isfinite(number) && TryConvertInt32(number, out);
}

static napi_value NativeUpgradeWeapon(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "upgradeWeapon expects exactly one weaponId");
  }
  int32_t weaponId = 0;
  if (!CopyInt32Argument(env, args[0], weaponId)) {
    return ThrowInputTypeError(env, "upgradeWeapon weaponId must be an integer");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.upgradeWeapon(weaponId), &result);
  return result;
}

static napi_value NativeEquipWeapon(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 2) {
    return ThrowInputTypeError(env,
                               "equipWeapon expects weaponId and characterId");
  }
  int32_t weaponId = 0;
  int32_t characterId = 0;
  if (!CopyInt32Argument(env, args[0], weaponId) ||
      !CopyInt32Argument(env, args[1], characterId)) {
    return ThrowInputTypeError(env, "equipWeapon arguments must be integers");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.equipWeapon(weaponId, characterId), &result);
  return result;
}

// 原神式养成新导出：矿石强化/武器突破/精炼/经验书/圣遗物/等阶奖励。
static napi_value NativeUpgradeWeaponWithOre(napi_env env,
                                             napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 3) {
    return ThrowInputTypeError(
        env, "upgradeWeaponWithOre expects weaponId, oreItemId and count");
  }
  int32_t weaponId = 0;
  int32_t oreItemId = 0;
  int32_t count = 0;
  if (!CopyInt32Argument(env, args[0], weaponId) ||
      !CopyInt32Argument(env, args[1], oreItemId) ||
      !CopyInt32Argument(env, args[2], count)) {
    return ThrowInputTypeError(env, "upgradeWeaponWithOre arguments must be integers");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.upgradeWeaponWithOre(weaponId, oreItemId, count),
                   &result);
  return result;
}

static napi_value NativeAscendWeapon(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "ascendWeapon expects exactly one weaponId");
  }
  int32_t weaponId = 0;
  if (!CopyInt32Argument(env, args[0], weaponId)) {
    return ThrowInputTypeError(env, "ascendWeapon weaponId must be an integer");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.ascendWeapon(weaponId), &result);
  return result;
}

static napi_value NativeRefineWeapon(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "refineWeapon expects exactly one weaponId");
  }
  int32_t weaponId = 0;
  if (!CopyInt32Argument(env, args[0], weaponId)) {
    return ThrowInputTypeError(env, "refineWeapon weaponId must be an integer");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.refineWeapon(weaponId), &result);
  return result;
}

static napi_value NativeUseExpItem(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 3) {
    return ThrowInputTypeError(
        env, "useExpItem expects characterId, itemId and count");
  }
  int32_t characterId = 0;
  int32_t itemId = 0;
  int32_t count = 0;
  if (!CopyInt32Argument(env, args[0], characterId) ||
      !CopyInt32Argument(env, args[1], itemId) ||
      !CopyInt32Argument(env, args[2], count)) {
    return ThrowInputTypeError(env, "useExpItem arguments must be integers");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.useExpItem(characterId, itemId, count), &result);
  return result;
}

static napi_value NativeUpgradeArtifact(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 2) {
    return ThrowInputTypeError(
        env, "upgradeArtifact expects targetInstanceId and feedInstanceIds");
  }
  int32_t targetInstanceId = 0;
  if (!CopyInt32Argument(env, args[0], targetInstanceId)) {
    return ThrowInputTypeError(env,
                               "upgradeArtifact targetInstanceId must be an integer");
  }
  bool isArray = false;
  if (napi_is_array(env, args[1], &isArray) != napi_ok || !isArray) {
    return ThrowInputTypeError(env, "upgradeArtifact feedInstanceIds must be an array");
  }
  uint32_t length = 0;
  if (napi_get_array_length(env, args[1], &length) != napi_ok ||
      length > 64) {
    return ThrowInputTypeError(env, "upgradeArtifact feedInstanceIds too large");
  }
  std::vector<int32_t> feedIds;
  feedIds.reserve(length);
  for (uint32_t index = 0; index < length; ++index) {
    napi_value element = nullptr;
    int32_t feedId = 0;
    if (napi_get_element(env, args[1], index, &element) != napi_ok ||
        !CopyInt32Argument(env, element, feedId)) {
      return ThrowInputTypeError(env,
                                 "upgradeArtifact feedInstanceIds must be integers");
    }
    feedIds.push_back(feedId);
  }
  napi_value result;
  napi_get_boolean(env,
                   g_loop.upgradeArtifact(targetInstanceId, feedIds),
                   &result);
  return result;
}

static napi_value NativeEquipArtifact(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 2) {
    return ThrowInputTypeError(
        env, "equipArtifact expects instanceId and characterId");
  }
  int32_t instanceId = 0;
  int32_t characterId = 0;
  if (!CopyInt32Argument(env, args[0], instanceId) ||
      !CopyInt32Argument(env, args[1], characterId)) {
    return ThrowInputTypeError(env, "equipArtifact arguments must be integers");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.equipArtifact(instanceId, characterId), &result);
  return result;
}

static napi_value NativeClaimRankReward(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "claimRankReward expects exactly one rank");
  }
  int32_t rank = 0;
  if (!CopyInt32Argument(env, args[0], rank)) {
    return ThrowInputTypeError(env, "claimRankReward rank must be an integer");
  }
  napi_value result;
  napi_get_boolean(env, g_loop.claimRankReward(rank), &result);
  return result;
}

static napi_value NativePerformGacha(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "performGacha expects exactly one count");
  }
  int32_t count = 0;
  if (!CopyInt32Argument(env, args[0], count)) {
    return ThrowInputTypeError(env, "performGacha count must be an integer");
  }
  const bool pulled = g_loop.performGacha(count);
  napi_get_boolean(env, pulled, &result);
  return result;
}

static napi_value NativePerformWeaponGacha(napi_env env,
                                           napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env,
                               "performWeaponGacha expects exactly one count");
  }
  int32_t count = 0;
  if (!CopyInt32Argument(env, args[0], count)) {
    return ThrowInputTypeError(env,
                               "performWeaponGacha count must be an integer");
  }
  napi_value result;
  const bool pulled = g_loop.performWeaponGacha(count);
  napi_get_boolean(env, pulled, &result);
  return result;
}

static napi_value NativeUseExpMaterial(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr, nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 2) {
    return ThrowInputTypeError(env,
                               "useExpMaterial expects characterId and count");
  }
  int32_t characterId = 0;
  int32_t materialCount = 0;
  if (!CopyInt32Argument(env, args[0], characterId) ||
      !CopyInt32Argument(env, args[1], materialCount)) {
    return ThrowInputTypeError(env,
                               "useExpMaterial arguments must be integers");
  }
  const bool used = g_loop.useExpMaterial(characterId, materialCount);
  napi_get_boolean(env, used, &result);
  return result;
}

static napi_value NativeAscendCharacter(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env,
                               "ascendCharacter expects exactly one characterId");
  }
  int32_t characterId = 0;
  if (!CopyInt32Argument(env, args[0], characterId)) {
    return ThrowInputTypeError(env, "ascendCharacter id must be an integer");
  }
  const bool ascended = g_loop.ascendCharacter(characterId);
  napi_get_boolean(env, ascended, &result);
  return result;
}

static bool CopyStringArgument(napi_env env, napi_value value,
                                std::string& out) {
  napi_valuetype valueType = napi_undefined;
  if (value == nullptr || napi_typeof(env, value, &valueType) != napi_ok ||
      valueType != napi_string) {
    return false;
  }
  size_t length = 0;
  if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok) {
    return false;
  }
  out.resize(length);
  size_t copied = 0;
  return napi_get_value_string_utf8(env, value, out.data(), length + 1,
                                    &copied) == napi_ok;
}

static napi_value NativeSaveProgress(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "saveProgress expects exactly one path");
  }
  std::string path;
  if (!CopyStringArgument(env, args[0], path) || path.empty()) {
    return ThrowInputTypeError(env, "saveProgress path must be a non-empty string");
  }
  const bool saved = g_loop.saveProgress(path);
  napi_get_boolean(env, saved, &result);
  return result;
}

static napi_value NativeLoadProgress(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value result = nullptr;
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "loadProgress expects exactly one path");
  }
  std::string path;
  if (!CopyStringArgument(env, args[0], path) || path.empty()) {
    return ThrowInputTypeError(env, "loadProgress path must be a non-empty string");
  }
  const bool loaded = g_loop.loadProgress(path);
  napi_get_boolean(env, loaded, &result);
  return result;
}

static napi_value NativeSetQualityPreset(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "setQualityPreset expects exactly one preset");
  }
  int32_t preset = 0;
  if (!CopyInt32Argument(env, args[0], preset)) {
    return ThrowInputTypeError(env, "setQualityPreset preset must be an integer");
  }
  g_loop.setQualityPreset(preset);
  return nullptr;
}

static napi_value NativeSetAudioEnabled(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "setAudioEnabled expects exactly one boolean");
  }
  napi_valuetype valueType = napi_undefined;
  if (args[0] == nullptr || napi_typeof(env, args[0], &valueType) != napi_ok ||
      valueType != napi_boolean) {
    return ThrowInputTypeError(env, "setAudioEnabled requires a boolean argument");
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  g_loop.setAudioEnabled(value);
  return nullptr;
}

static napi_value NativeSetPaused(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok ||
      argc != 1) {
    return ThrowInputTypeError(env, "setPaused expects exactly one boolean");
  }
  napi_valuetype valueType = napi_undefined;
  if (args[0] == nullptr || napi_typeof(env, args[0], &valueType) != napi_ok ||
      valueType != napi_boolean) {
    return ThrowInputTypeError(env, "setPaused requires a boolean argument");
  }
  bool value = false;
  napi_get_value_bool(env, args[0], &value);
  g_loop.setPaused(value);
  return nullptr;
}

static napi_value NativeSkipDemoPhase(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_get_cb_info(env, info, &argc, nullptr, nullptr, nullptr);
  if (argc != 1) {
    napi_throw_type_error(env, nullptr, "skipDemoPhase expects exactly one argument");
    return nullptr;
  }
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  napi_valuetype argumentType;
  napi_typeof(env, args[0], &argumentType);
  if (argumentType != napi_number) {
    napi_throw_type_error(env, nullptr, "skipDemoPhase phase must be a number");
    return nullptr;
  }
  double phaseNumber;
  napi_get_value_double(env, args[0], &phaseNumber);
  if (!std::isfinite(phaseNumber)) {
    napi_throw_type_error(env, nullptr, "skipDemoPhase phase must be finite");
    return nullptr;
  }
  int32_t phase;
  if (!TryConvertInt32(phaseNumber, phase)) {
    napi_throw_type_error(env, nullptr, "skipDemoPhase phase must be an integer");
    return nullptr;
  }
  if (phase < 0 || phase > 6) {
    napi_throw_type_error(env, nullptr, "skipDemoPhase phase must be 0..6");
    return nullptr;
  }
  g_loop.skipDemoPhase(static_cast<DemoPhase>(phase));
  return nullptr;
}

static napi_value NativePullSnapshot(napi_env env, napi_callback_info) {
  const GameSnapshot snapshot = g_loop.snapshot();
  napi_value result;
  napi_create_object(env, &result);
  napi_value tickVal, hpVal, poiseVal, xVal, yVal, fpsVal, movingVal;
  napi_value moveXVal, moveYVal, cameraYawVal, cameraPitchVal, distVal;
  napi_value targetIdVal, bossPhaseVal, encounterModeVal, encounterStateVal, rendererReadyVal;
  napi_value targetArchetypeVal, targetHpRatioVal;
  napi_value environmentReadyVal, environmentDrawCallsVal, environmentTrianglesVal;
  napi_value staminaVal, comboSegmentVal, invulnerableVal, insightMsVal;
  napi_value resonanceVal, targetHpVal, targetPoiseVal, pulseHitRemainingMsVal;
  napi_value lastRejectReasonVal;
  napi_create_int64(env, static_cast<int64_t>(snapshot.tick), &tickVal);
  napi_create_double(env, static_cast<double>(snapshot.hp) / FP_ONE, &hpVal);
  napi_create_double(env, static_cast<double>(snapshot.poise) / FP_ONE, &poiseVal);
  napi_create_double(env, snapshot.playerX, &xVal);
  napi_create_double(env, snapshot.playerY, &yVal);
  napi_create_double(env, snapshot.fps, &fpsVal);
  napi_create_double(env, snapshot.moveX, &moveXVal);
  napi_create_double(env, snapshot.moveY, &moveYVal);
  napi_create_double(env, snapshot.cameraYaw, &cameraYawVal);
  napi_create_double(env, snapshot.cameraPitch, &cameraPitchVal);
  napi_create_double(env, snapshot.targetDist, &distVal);
  napi_create_int32(env, snapshot.targetArchetype, &targetArchetypeVal);
  napi_create_double(env, snapshot.targetHpRatio, &targetHpRatioVal);
  napi_create_int32(env, snapshot.targetId, &targetIdVal);
  napi_create_int32(env, snapshot.bossPhase, &bossPhaseVal);
  napi_create_int32(env, snapshot.encounterMode, &encounterModeVal);
  napi_create_int32(env, snapshot.encounterState, &encounterStateVal);
  napi_get_boolean(env, snapshot.moving, &movingVal);
  napi_get_boolean(env, snapshot.rendererReady, &rendererReadyVal);
  napi_get_boolean(env, snapshot.environmentReady, &environmentReadyVal);
  napi_create_uint32(env, snapshot.environmentDrawCalls, &environmentDrawCallsVal);
  napi_create_uint32(env, snapshot.environmentTriangles, &environmentTrianglesVal);
  napi_create_double(env, static_cast<double>(snapshot.stamina) / FP_ONE, &staminaVal);
  napi_create_uint32(env, snapshot.comboSegment, &comboSegmentVal);
  napi_get_boolean(env, snapshot.invulnerable, &invulnerableVal);
  napi_create_int64(env, snapshot.insightMs, &insightMsVal);
  napi_create_double(env, static_cast<double>(snapshot.resonance) / FP_ONE, &resonanceVal);
  napi_create_double(env, static_cast<double>(snapshot.targetHp) / FP_ONE, &targetHpVal);
  napi_create_double(env, static_cast<double>(snapshot.targetPoise) / FP_ONE, &targetPoiseVal);
  napi_create_int64(env, snapshot.pulseHitRemainingMs, &pulseHitRemainingMsVal);
  napi_create_int32(env, snapshot.lastRejectReason, &lastRejectReasonVal);
  napi_set_named_property(env, result, "tick", tickVal);
  napi_set_named_property(env, result, "hp", hpVal);
  napi_set_named_property(env, result, "poise", poiseVal);
  napi_set_named_property(env, result, "x", xVal);
  napi_set_named_property(env, result, "y", yVal);
  napi_set_named_property(env, result, "fps", fpsVal);
  napi_set_named_property(env, result, "moving", movingVal);
  napi_set_named_property(env, result, "moveX", moveXVal);
  napi_set_named_property(env, result, "moveY", moveYVal);
  napi_set_named_property(env, result, "cameraYaw", cameraYawVal);
  napi_set_named_property(env, result, "cameraPitch", cameraPitchVal);
  napi_set_named_property(env, result, "targetDist", distVal);
  napi_set_named_property(env, result, "targetArchetype", targetArchetypeVal);
  napi_set_named_property(env, result, "targetHpRatio", targetHpRatioVal);
  napi_set_named_property(env, result, "targetId", targetIdVal);
  napi_set_named_property(env, result, "bossPhase", bossPhaseVal);
  napi_set_named_property(env, result, "encounterMode", encounterModeVal);
  napi_set_named_property(env, result, "encounterState", encounterStateVal);
  napi_set_named_property(env, result, "rendererReady", rendererReadyVal);
  napi_set_named_property(env, result, "environmentReady", environmentReadyVal);
  napi_set_named_property(env, result, "environmentDrawCalls", environmentDrawCallsVal);
  napi_set_named_property(env, result, "environmentTriangles", environmentTrianglesVal);
  napi_set_named_property(env, result, "stamina", staminaVal);
  napi_set_named_property(env, result, "comboSegment", comboSegmentVal);
  napi_set_named_property(env, result, "invulnerable", invulnerableVal);
  napi_set_named_property(env, result, "insightMs", insightMsVal);
  napi_set_named_property(env, result, "resonance", resonanceVal);
  napi_set_named_property(env, result, "targetHp", targetHpVal);
  napi_set_named_property(env, result, "targetPoise", targetPoiseVal);
  napi_set_named_property(env, result, "pulseHitRemainingMs", pulseHitRemainingMsVal);
  napi_set_named_property(env, result, "lastRejectReason", lastRejectReasonVal);
  napi_value extra[16];
  napi_create_int32(env, snapshot.currentAction, &extra[0]);
  napi_create_int64(env, snapshot.comboWindowMs, &extra[1]);
  napi_create_int64(env, snapshot.radianceCooldownMs, &extra[2]);
  napi_create_int64(env, snapshot.currentCooldownMs, &extra[3]);
  napi_create_int64(env, snapshot.corruptionCooldownMs, &extra[4]);
  napi_create_int64(env, snapshot.ultimateWindowMs, &extra[5]);
  napi_get_boolean(env, snapshot.targetPoiseBroken, &extra[6]);
  napi_get_boolean(env, snapshot.radianceAttached, &extra[7]);
  napi_get_boolean(env, snapshot.currentAttached, &extra[8]);
  napi_get_boolean(env, snapshot.corruptionAttached, &extra[9]);
  napi_get_boolean(env, snapshot.corroded, &extra[10]);
  napi_create_int32(env, snapshot.currentReaction, &extra[11]);
  napi_create_int32(env, snapshot.pulsePhase, &extra[12]);
  napi_create_int64(env, snapshot.radianceCooldownTotalMs, &extra[13]);
  napi_create_int64(env, snapshot.currentCooldownTotalMs, &extra[14]);
  napi_create_int64(env, snapshot.corruptionCooldownTotalMs, &extra[15]);
  napi_value stage[7];
  napi_create_int32(env, snapshot.levelStage, &stage[0]);
  napi_create_int32(env, snapshot.gateState, &stage[1]);
  napi_create_int32(env, snapshot.supplyState, &stage[2]);
  napi_create_double(env, static_cast<double>(snapshot.bossHp) / FP_ONE, &stage[3]);
  napi_create_double(env, static_cast<double>(snapshot.bossPoise) / FP_ONE, &stage[4]);
  napi_create_int32(env, snapshot.bossMechanic, &stage[5]);
  napi_create_int64(env, snapshot.bossCastMs, &stage[6]);
  napi_value s6[7];
  napi_create_int32(env, snapshot.perfLevel, &s6[0]);
  napi_create_int32(env, snapshot.vfxFlags, &s6[1]);
  napi_create_double(env, snapshot.cameraShakeX, &s6[2]);
  napi_create_double(env, snapshot.cameraShakeY, &s6[3]);
  napi_create_double(env, snapshot.bossHpRatio, &s6[4]);
  napi_create_double(env, snapshot.bossCastRatio, &s6[5]);
  napi_get_boolean(env, snapshot.debugHud, &s6[6]);
  napi_set_named_property(env, result, "currentAction", extra[0]);
  napi_set_named_property(env, result, "comboWindowMs", extra[1]);
  napi_set_named_property(env, result, "radianceCooldownMs", extra[2]);
  napi_set_named_property(env, result, "currentCooldownMs", extra[3]);
  napi_set_named_property(env, result, "corruptionCooldownMs", extra[4]);
  napi_set_named_property(env, result, "radianceCooldownTotalMs", extra[13]);
  napi_set_named_property(env, result, "currentCooldownTotalMs", extra[14]);
  napi_set_named_property(env, result, "corruptionCooldownTotalMs", extra[15]);
  napi_set_named_property(env, result, "ultimateWindowMs", extra[5]);
  napi_set_named_property(env, result, "targetPoiseBroken", extra[6]);
  napi_set_named_property(env, result, "radianceAttached", extra[7]);
  napi_set_named_property(env, result, "currentAttached", extra[8]);
  napi_set_named_property(env, result, "corruptionAttached", extra[9]);
  napi_set_named_property(env, result, "corroded", extra[10]);
  napi_set_named_property(env, result, "currentReaction", extra[11]);
  napi_set_named_property(env, result, "pulsePhase", extra[12]);
  napi_set_named_property(env, result, "levelStage", stage[0]);
  napi_set_named_property(env, result, "gateState", stage[1]);
  napi_set_named_property(env, result, "supplyState", stage[2]);
  napi_set_named_property(env, result, "bossHp", stage[3]);
  napi_set_named_property(env, result, "bossPoise", stage[4]);
  napi_set_named_property(env, result, "bossMechanic", stage[5]);
  napi_set_named_property(env, result, "bossCastMs", stage[6]);
  napi_set_named_property(env, result, "perfLevel", s6[0]);
  napi_set_named_property(env, result, "vfxFlags", s6[1]);
  napi_set_named_property(env, result, "cameraShakeX", s6[2]);
  napi_set_named_property(env, result, "cameraShakeY", s6[3]);
  napi_set_named_property(env, result, "bossHpRatio", s6[4]);
  napi_set_named_property(env, result, "bossCastRatio", s6[5]);
  napi_set_named_property(env, result, "debugHud", s6[6]);
  napi_value objectiveLabelVal, resonanceSlotsVal, showDebugHudVal;
  napi_create_string_utf8(env, snapshot.objectiveLabel.c_str(), NAPI_AUTO_LENGTH,
                          &objectiveLabelVal);
  napi_create_array_with_length(env, snapshot.resonanceSlots.size(),
                                &resonanceSlotsVal);
  for (uint32_t index = 0; index < snapshot.resonanceSlots.size(); ++index) {
    napi_value slotVal;
    napi_create_uint32(env, snapshot.resonanceSlots[index], &slotVal);
    napi_set_element(env, resonanceSlotsVal, index, slotVal);
  }
  napi_get_boolean(env, snapshot.showDebugHud, &showDebugHudVal);
  napi_set_named_property(env, result, "objectiveLabel", objectiveLabelVal);
  napi_set_named_property(env, result, "resonanceSlots", resonanceSlotsVal);
  napi_set_named_property(env, result, "showDebugHud", showDebugHudVal);
  napi_value bossCinematicProgressVal, bossShardCountVal, bossSourceColorVal;
  napi_value bossRingBrokenVal;
  napi_create_double(env, snapshot.bossCinematicProgress,
                     &bossCinematicProgressVal);
  napi_create_uint32(env, snapshot.bossShardCount, &bossShardCountVal);
  napi_create_uint32(env, snapshot.bossSourceColor, &bossSourceColorVal);
  napi_get_boolean(env, snapshot.bossRingBroken, &bossRingBrokenVal);
  napi_set_named_property(env, result, "bossCinematicProgress",
                          bossCinematicProgressVal);
  napi_set_named_property(env, result, "bossShardCount", bossShardCountVal);
  napi_set_named_property(env, result, "bossSourceColor", bossSourceColorVal);
  napi_set_named_property(env, result, "bossRingBroken", bossRingBrokenVal);
  napi_value inputEventCountVal;
  napi_create_int32(env, snapshot.inputEventCount, &inputEventCountVal);
  napi_set_named_property(env, result, "inputEventCount", inputEventCountVal);
  // ---- 开放世界探索字段（阶段一）----
  napi_value explorationVal;
  napi_create_double(env, snapshot.explorationStamina, &explorationVal);
  napi_set_named_property(env, result, "explorationStamina", explorationVal);
  napi_value motionStateVal;
  napi_create_int32(env, snapshot.motionState, &motionStateVal);
  napi_set_named_property(env, result, "motionState", motionStateVal);
  napi_value playerHeightVal;
  napi_create_double(env, snapshot.playerHeight, &playerHeightVal);
  napi_set_named_property(env, result, "playerHeight", playerHeightVal);
  napi_value activeChunkCountVal;
  napi_create_int32(env, snapshot.activeChunkCount, &activeChunkCountVal);
  napi_set_named_property(env, result, "activeChunkCount", activeChunkCountVal);
  napi_value chunkLoadCountVal;
  napi_create_int32(env, snapshot.chunkLoadCount, &chunkLoadCountVal);
  napi_set_named_property(env, result, "chunkLoadCount", chunkLoadCountVal);
  napi_value interactionAnchorIdVal;
  napi_create_int32(env, snapshot.interactionAnchorId, &interactionAnchorIdVal);
  napi_set_named_property(env, result, "interactionAnchorId",
                          interactionAnchorIdVal);
  napi_value interactionUnlockedVal;
  napi_get_boolean(env, snapshot.interactionUnlocked, &interactionUnlockedVal);
  napi_set_named_property(env, result, "interactionUnlocked",
                          interactionUnlockedVal);
  napi_value interactionLabelVal;
  napi_create_string_utf8(env, snapshot.interactionLabel.c_str(),
                          NAPI_AUTO_LENGTH, &interactionLabelVal);
  napi_set_named_property(env, result, "interactionLabel", interactionLabelVal);
  napi_value unlockedAnchorCountVal;
  napi_create_int32(env, snapshot.unlockedAnchorCount, &unlockedAnchorCountVal);
  napi_set_named_property(env, result, "unlockedAnchorCount",
                          unlockedAnchorCountVal);
  napi_value cameraExplorationVal;
  napi_get_boolean(env, snapshot.cameraExploration, &cameraExplorationVal);
  napi_set_named_property(env, result, "cameraExploration",
                          cameraExplorationVal);
  napi_value teleportFlashMsVal;
  napi_create_int64(env, snapshot.teleportFlashMs, &teleportFlashMsVal);
  napi_set_named_property(env, result, "teleportFlashMs", teleportFlashMsVal);
  napi_value minimapXVal, minimapYVal, minimapUnlockedVal;
  napi_create_array_with_length(env, snapshot.minimapAnchorX.size(),
                                &minimapXVal);
  napi_create_array_with_length(env, snapshot.minimapAnchorY.size(),
                                &minimapYVal);
  napi_create_array_with_length(env, snapshot.minimapAnchorUnlocked.size(),
                                &minimapUnlockedVal);
  for (uint32_t index = 0; index < snapshot.minimapAnchorX.size(); ++index) {
    napi_value xVal, yVal, unlockedVal;
    napi_create_double(env, snapshot.minimapAnchorX[index], &xVal);
    napi_create_double(env, snapshot.minimapAnchorY[index], &yVal);
    napi_create_uint32(env, snapshot.minimapAnchorUnlocked[index],
                       &unlockedVal);
    napi_set_element(env, minimapXVal, index, xVal);
    napi_set_element(env, minimapYVal, index, yVal);
    napi_set_element(env, minimapUnlockedVal, index, unlockedVal);
  }
  napi_set_named_property(env, result, "minimapAnchorX", minimapXVal);
  napi_set_named_property(env, result, "minimapAnchorY", minimapYVal);
  napi_set_named_property(env, result, "minimapAnchorUnlocked",
                          minimapUnlockedVal);
  // ---- 内容与任务字段（阶段二）----
  napi_value questIdVal;
  napi_create_int32(env, snapshot.questId, &questIdVal);
  napi_set_named_property(env, result, "questId", questIdVal);
  napi_value questStatusVal;
  napi_create_int32(env, snapshot.questStatus, &questStatusVal);
  napi_set_named_property(env, result, "questStatus", questStatusVal);
  napi_value questTitleVal;
  napi_create_string_utf8(env, snapshot.questTitle.c_str(), NAPI_AUTO_LENGTH,
                          &questTitleVal);
  napi_set_named_property(env, result, "questTitle", questTitleVal);
  napi_value questObjectiveLabelVal;
  napi_create_string_utf8(env, snapshot.questObjectiveLabel.c_str(),
                          NAPI_AUTO_LENGTH, &questObjectiveLabelVal);
  napi_set_named_property(env, result, "questObjectiveLabel",
                          questObjectiveLabelVal);
  napi_value questObjectiveProgressVal;
  napi_create_int32(env, snapshot.questObjectiveProgress,
                    &questObjectiveProgressVal);
  napi_set_named_property(env, result, "questObjectiveProgress",
                          questObjectiveProgressVal);
  napi_value questObjectiveRequiredVal;
  napi_create_int32(env, snapshot.questObjectiveRequired,
                    &questObjectiveRequiredVal);
  napi_set_named_property(env, result, "questObjectiveRequired",
                          questObjectiveRequiredVal);
  napi_value completedQuestCountVal;
  napi_create_int32(env, snapshot.completedQuestCount, &completedQuestCountVal);
  napi_set_named_property(env, result, "completedQuestCount",
                          completedQuestCountVal);
  napi_value dialogActiveVal;
  napi_get_boolean(env, snapshot.dialogActive, &dialogActiveVal);
  napi_set_named_property(env, result, "dialogActive", dialogActiveVal);
  napi_value dialogSpeakerVal;
  napi_create_string_utf8(env, snapshot.dialogSpeaker.c_str(),
                          NAPI_AUTO_LENGTH, &dialogSpeakerVal);
  napi_set_named_property(env, result, "dialogSpeaker", dialogSpeakerVal);
  napi_value dialogTextVal;
  napi_create_string_utf8(env, snapshot.dialogText.c_str(), NAPI_AUTO_LENGTH,
                          &dialogTextVal);
  napi_set_named_property(env, result, "dialogText", dialogTextVal);
  napi_value dialogLineIndexVal;
  napi_create_int32(env, snapshot.dialogLineIndex, &dialogLineIndexVal);
  napi_set_named_property(env, result, "dialogLineIndex", dialogLineIndexVal);
  napi_value dialogLineCountVal;
  napi_create_int32(env, snapshot.dialogLineCount, &dialogLineCountVal);
  napi_set_named_property(env, result, "dialogLineCount", dialogLineCountVal);
  napi_value interactionKindVal;
  napi_create_int32(env, snapshot.interactionKind, &interactionKindVal);
  napi_set_named_property(env, result, "interactionKind", interactionKindVal);
  // ---- 养成与抽卡字段（阶段三）----
  napi_value fateCountVal;
  napi_create_int32(env, snapshot.fateCount, &fateCountVal);
  napi_set_named_property(env, result, "fateCount", fateCountVal);
  napi_value goldCountVal;
  napi_create_int32(env, snapshot.goldCount, &goldCountVal);
  napi_set_named_property(env, result, "goldCount", goldCountVal);
  napi_value expMaterialCountVal;
  napi_create_int32(env, snapshot.expMaterialCount, &expMaterialCountVal);
  napi_set_named_property(env, result, "expMaterialCount", expMaterialCountVal);
  napi_value ascensionMaterialCountVal;
  napi_create_int32(env, snapshot.ascensionMaterialCount,
                    &ascensionMaterialCountVal);
  napi_set_named_property(env, result, "ascensionMaterialCount",
                          ascensionMaterialCountVal);
  napi_value gachaPity5Val;
  napi_create_int32(env, snapshot.gachaPity5, &gachaPity5Val);
  napi_set_named_property(env, result, "gachaPity5", gachaPity5Val);
  napi_value gachaIdsVal, gachaRaritiesVal, gachaIsNewVal;
  napi_create_array_with_length(env, snapshot.gachaResultIds.size(),
                                &gachaIdsVal);
  napi_create_array_with_length(env, snapshot.gachaResultRarities.size(),
                                &gachaRaritiesVal);
  napi_create_array_with_length(env, snapshot.gachaResultIsNew.size(),
                                &gachaIsNewVal);
  for (uint32_t index = 0; index < snapshot.gachaResultIds.size(); ++index) {
    napi_value idVal, rarityVal, newVal;
    napi_create_int32(env, snapshot.gachaResultIds[index], &idVal);
    napi_create_int32(env, snapshot.gachaResultRarities[index], &rarityVal);
    napi_create_uint32(env, snapshot.gachaResultIsNew[index], &newVal);
    napi_set_element(env, gachaIdsVal, index, idVal);
    napi_set_element(env, gachaRaritiesVal, index, rarityVal);
    napi_set_element(env, gachaIsNewVal, index, newVal);
  }
  napi_set_named_property(env, result, "gachaResultIds", gachaIdsVal);
  napi_set_named_property(env, result, "gachaResultRarities", gachaRaritiesVal);
  napi_set_named_property(env, result, "gachaResultIsNew", gachaIsNewVal);
  napi_value rosterIdsVal, rosterLevelsVal, rosterAscensionsVal;
  napi_create_array_with_length(env, snapshot.rosterIds.size(), &rosterIdsVal);
  napi_create_array_with_length(env, snapshot.rosterLevels.size(),
                                &rosterLevelsVal);
  napi_create_array_with_length(env, snapshot.rosterAscensions.size(),
                                &rosterAscensionsVal);
  for (uint32_t index = 0; index < snapshot.rosterIds.size(); ++index) {
    napi_value idVal, levelVal, ascensionVal;
    napi_create_int32(env, snapshot.rosterIds[index], &idVal);
    napi_create_int32(env, snapshot.rosterLevels[index], &levelVal);
    napi_create_int32(env, snapshot.rosterAscensions[index], &ascensionVal);
    napi_set_element(env, rosterIdsVal, index, idVal);
    napi_set_element(env, rosterLevelsVal, index, levelVal);
    napi_set_element(env, rosterAscensionsVal, index, ascensionVal);
  }
  napi_set_named_property(env, result, "rosterIds", rosterIdsVal);
  napi_set_named_property(env, result, "rosterLevels", rosterLevelsVal);
  napi_set_named_property(env, result, "rosterAscensions", rosterAscensionsVal);
  // ---- 阶段四打磨字段 ----
  napi_value activeCharacterIdVal;
  napi_create_int32(env, snapshot.activeCharacterId, &activeCharacterIdVal);
  napi_set_named_property(env, result, "activeCharacterId",
                          activeCharacterIdVal);
  napi_value dayNightHourVal;
  napi_create_double(env, snapshot.dayNightHour, &dayNightHourVal);
  napi_set_named_property(env, result, "dayNightHour", dayNightHourVal);
  napi_value qualityPresetVal;
  napi_create_int32(env, snapshot.qualityPreset, &qualityPresetVal);
  napi_set_named_property(env, result, "qualityPreset", qualityPresetVal);
  napi_value weatherIdVal;
  napi_create_int32(env, snapshot.weatherId, &weatherIdVal);
  napi_set_named_property(env, result, "weatherId", weatherIdVal);
  napi_value musicRegionIdVal;
  napi_create_int32(env, snapshot.musicRegionId, &musicRegionIdVal);
  napi_set_named_property(env, result, "musicRegionId", musicRegionIdVal);
  napi_value completedSideQuestCountVal;
  napi_create_int32(env, snapshot.completedSideQuestCount, &completedSideQuestCountVal);
  napi_set_named_property(env, result, "completedSideQuestCount", completedSideQuestCountVal);
  napi_value dungeonStateVal;
  napi_create_int32(env, snapshot.dungeonState, &dungeonStateVal);
  napi_set_named_property(env, result, "dungeonState", dungeonStateVal);
  napi_value dungeonProgressVal;
  napi_create_int32(env, snapshot.dungeonProgress, &dungeonProgressVal);
  napi_set_named_property(env, result, "dungeonProgress", dungeonProgressVal);
  napi_value dungeonRequiredVal;
  napi_create_int32(env, snapshot.dungeonRequired, &dungeonRequiredVal);
  napi_set_named_property(env, result, "dungeonRequired", dungeonRequiredVal);
  // ---- 优化批次：小地图可交互物、支线进度、角色属性数组 ----
  napi_value minimapItemXVal, minimapItemYVal, minimapItemKindVal;
  napi_create_array_with_length(env, snapshot.minimapItemX.size(),
                                &minimapItemXVal);
  napi_create_array_with_length(env, snapshot.minimapItemY.size(),
                                &minimapItemYVal);
  napi_create_array_with_length(env, snapshot.minimapItemKind.size(),
                                &minimapItemKindVal);
  for (uint32_t index = 0; index < snapshot.minimapItemX.size(); ++index) {
    napi_value xVal, yVal, kindVal;
    napi_create_double(env, snapshot.minimapItemX[index], &xVal);
    napi_create_double(env, snapshot.minimapItemY[index], &yVal);
    napi_create_int32(env, snapshot.minimapItemKind[index], &kindVal);
    napi_set_element(env, minimapItemXVal, index, xVal);
    napi_set_element(env, minimapItemYVal, index, yVal);
    napi_set_element(env, minimapItemKindVal, index, kindVal);
  }
  napi_set_named_property(env, result, "minimapItemX", minimapItemXVal);
  napi_set_named_property(env, result, "minimapItemY", minimapItemYVal);
  napi_set_named_property(env, result, "minimapItemKind", minimapItemKindVal);
  napi_value sideQuestProgressVal, sideQuestRequiredVal;
  napi_create_array_with_length(env, snapshot.sideQuestProgress.size(),
                                &sideQuestProgressVal);
  napi_create_array_with_length(env, snapshot.sideQuestRequired.size(),
                                &sideQuestRequiredVal);
  for (uint32_t index = 0; index < snapshot.sideQuestProgress.size();
       ++index) {
    napi_value progressVal, requiredVal;
    napi_create_int32(env, snapshot.sideQuestProgress[index], &progressVal);
    napi_create_int32(env, snapshot.sideQuestRequired[index], &requiredVal);
    napi_set_element(env, sideQuestProgressVal, index, progressVal);
    napi_set_element(env, sideQuestRequiredVal, index, requiredVal);
  }
  napi_set_named_property(env, result, "sideQuestProgress", sideQuestProgressVal);
  napi_set_named_property(env, result, "sideQuestRequired", sideQuestRequiredVal);
  napi_value rosterHpVal, rosterAtkVal;
  napi_create_array_with_length(env, snapshot.rosterHp.size(), &rosterHpVal);
  napi_create_array_with_length(env, snapshot.rosterAtk.size(), &rosterAtkVal);
  for (uint32_t index = 0; index < snapshot.rosterHp.size(); ++index) {
    napi_value hpVal, atkVal;
    napi_create_int32(env, snapshot.rosterHp[index], &hpVal);
    napi_create_int32(env, snapshot.rosterAtk[index], &atkVal);
    napi_set_element(env, rosterHpVal, index, hpVal);
    napi_set_element(env, rosterAtkVal, index, atkVal);
  }
  napi_set_named_property(env, result, "rosterHp", rosterHpVal);
  napi_set_named_property(env, result, "rosterAtk", rosterAtkVal);
  // ---- 养成深化：命之座与武器清单 ----
  napi_value rosterConstellationsVal;
  napi_create_array_with_length(env, snapshot.rosterConstellations.size(),
                                &rosterConstellationsVal);
  for (uint32_t index = 0; index < snapshot.rosterConstellations.size();
       ++index) {
    napi_value constellationVal;
    napi_create_int32(env, snapshot.rosterConstellations[index],
                      &constellationVal);
    napi_set_element(env, rosterConstellationsVal, index, constellationVal);
  }
  napi_set_named_property(env, result, "rosterConstellations",
                          rosterConstellationsVal);
  napi_value weaponIdsVal, weaponLevelsVal, weaponEquippedByVal;
  napi_create_array_with_length(env, snapshot.weaponIds.size(), &weaponIdsVal);
  napi_create_array_with_length(env, snapshot.weaponLevels.size(),
                                &weaponLevelsVal);
  napi_create_array_with_length(env, snapshot.weaponEquippedBy.size(),
                                &weaponEquippedByVal);
  for (uint32_t index = 0; index < snapshot.weaponIds.size(); ++index) {
    napi_value idVal, levelVal, equippedVal;
    napi_create_int32(env, snapshot.weaponIds[index], &idVal);
    napi_create_int32(env, snapshot.weaponLevels[index], &levelVal);
    napi_create_int32(env, snapshot.weaponEquippedBy[index], &equippedVal);
    napi_set_element(env, weaponIdsVal, index, idVal);
    napi_set_element(env, weaponLevelsVal, index, levelVal);
    napi_set_element(env, weaponEquippedByVal, index, equippedVal);
  }
  napi_set_named_property(env, result, "weaponIds", weaponIdsVal);
  napi_set_named_property(env, result, "weaponLevels", weaponLevelsVal);
  napi_set_named_property(env, result, "weaponEquippedBy", weaponEquippedByVal);
  // 原神式养成：武器深化字段 + 圣遗物清单 + 冒险等级。
  napi_value weaponAscensionsVal, weaponRefinesVal, weaponRefineStocksVal,
      weaponExpsVal;
  napi_create_array_with_length(env, snapshot.weaponAscensions.size(),
                                &weaponAscensionsVal);
  napi_create_array_with_length(env, snapshot.weaponRefines.size(),
                                &weaponRefinesVal);
  napi_create_array_with_length(env, snapshot.weaponRefineStocks.size(),
                                &weaponRefineStocksVal);
  napi_create_array_with_length(env, snapshot.weaponExps.size(),
                                &weaponExpsVal);
  for (uint32_t index = 0; index < snapshot.weaponIds.size(); ++index) {
    napi_value ascVal, refineVal, stockVal, expVal;
    napi_create_int32(env, snapshot.weaponAscensions[index], &ascVal);
    napi_create_int32(env, snapshot.weaponRefines[index], &refineVal);
    napi_create_int32(env, snapshot.weaponRefineStocks[index], &stockVal);
    napi_create_int32(env, snapshot.weaponExps[index], &expVal);
    napi_set_element(env, weaponAscensionsVal, index, ascVal);
    napi_set_element(env, weaponRefinesVal, index, refineVal);
    napi_set_element(env, weaponRefineStocksVal, index, stockVal);
    napi_set_element(env, weaponExpsVal, index, expVal);
  }
  napi_set_named_property(env, result, "weaponAscensions", weaponAscensionsVal);
  napi_set_named_property(env, result, "weaponRefines", weaponRefinesVal);
  napi_set_named_property(env, result, "weaponRefineStocks",
                          weaponRefineStocksVal);
  napi_set_named_property(env, result, "weaponExps", weaponExpsVal);
  napi_value artifactInstanceIdsVal, artifactDefIdsVal, artifactRaritiesVal,
      artifactLevelsVal, artifactEquippedByVal, artifactSeedsVal;
  napi_create_array_with_length(env, snapshot.artifactInstanceIds.size(),
                                &artifactInstanceIdsVal);
  napi_create_array_with_length(env, snapshot.artifactDefIds.size(),
                                &artifactDefIdsVal);
  napi_create_array_with_length(env, snapshot.artifactRarities.size(),
                                &artifactRaritiesVal);
  napi_create_array_with_length(env, snapshot.artifactLevels.size(),
                                &artifactLevelsVal);
  napi_create_array_with_length(env, snapshot.artifactEquippedBy.size(),
                                &artifactEquippedByVal);
  napi_create_array_with_length(env, snapshot.artifactSeeds.size(),
                                &artifactSeedsVal);
  for (uint32_t index = 0; index < snapshot.artifactInstanceIds.size();
       ++index) {
    napi_value instanceVal, defVal, rarityVal, levelVal, equippedVal, seedVal;
    napi_create_int32(env, snapshot.artifactInstanceIds[index], &instanceVal);
    napi_create_int32(env, snapshot.artifactDefIds[index], &defVal);
    napi_create_int32(env, snapshot.artifactRarities[index], &rarityVal);
    napi_create_int32(env, snapshot.artifactLevels[index], &levelVal);
    napi_create_int32(env, snapshot.artifactEquippedBy[index], &equippedVal);
    napi_create_int32(env, snapshot.artifactSeeds[index], &seedVal);
    napi_set_element(env, artifactInstanceIdsVal, index, instanceVal);
    napi_set_element(env, artifactDefIdsVal, index, defVal);
    napi_set_element(env, artifactRaritiesVal, index, rarityVal);
    napi_set_element(env, artifactLevelsVal, index, levelVal);
    napi_set_element(env, artifactEquippedByVal, index, equippedVal);
    napi_set_element(env, artifactSeedsVal, index, seedVal);
  }
  napi_set_named_property(env, result, "artifactInstanceIds",
                          artifactInstanceIdsVal);
  napi_set_named_property(env, result, "artifactDefIds", artifactDefIdsVal);
  napi_set_named_property(env, result, "artifactRarities",
                          artifactRaritiesVal);
  napi_set_named_property(env, result, "artifactLevels", artifactLevelsVal);
  napi_set_named_property(env, result, "artifactEquippedBy",
                          artifactEquippedByVal);
  napi_set_named_property(env, result, "artifactSeeds", artifactSeedsVal);
  napi_value adventureRankVal, adventureExpVal, adventureExpRequiredVal,
      worldLevelVal;
  napi_create_int32(env, snapshot.adventureRank, &adventureRankVal);
  napi_create_int32(env, snapshot.adventureExp, &adventureExpVal);
  napi_create_int32(env, snapshot.adventureExpRequired,
                    &adventureExpRequiredVal);
  napi_create_int32(env, snapshot.worldLevel, &worldLevelVal);
  napi_set_named_property(env, result, "adventureRank", adventureRankVal);
  napi_set_named_property(env, result, "adventureExp", adventureExpVal);
  napi_set_named_property(env, result, "adventureExpRequired",
                          adventureExpRequiredVal);
  napi_set_named_property(env, result, "worldLevel", worldLevelVal);
  napi_value oreLowVal, oreMidVal, oreHighVal, expSmallVal, expMediumVal,
      expLargeVal;
  napi_create_int32(env, snapshot.oreLowCount, &oreLowVal);
  napi_create_int32(env, snapshot.oreMidCount, &oreMidVal);
  napi_create_int32(env, snapshot.oreHighCount, &oreHighVal);
  napi_create_int32(env, snapshot.expSmallCount, &expSmallVal);
  napi_create_int32(env, snapshot.expMediumCount, &expMediumVal);
  napi_create_int32(env, snapshot.expLargeCount, &expLargeVal);
  napi_set_named_property(env, result, "oreLowCount", oreLowVal);
  napi_set_named_property(env, result, "oreMidCount", oreMidVal);
  napi_set_named_property(env, result, "oreHighCount", oreHighVal);
  napi_set_named_property(env, result, "expSmallCount", expSmallVal);
  napi_set_named_property(env, result, "expMediumCount", expMediumVal);
  napi_set_named_property(env, result, "expLargeCount", expLargeVal);
  // ---- 第四轮优化：疾跑、每日委托、卡池种类 ----
  napi_value sprintActiveVal;
  napi_create_int32(env, snapshot.sprintActive, &sprintActiveVal);
  napi_set_named_property(env, result, "sprintActive", sprintActiveVal);
  napi_value dailyCompletedCountVal;
  napi_create_int32(env, snapshot.dailyCompletedCount, &dailyCompletedCountVal);
  napi_set_named_property(env, result, "dailyCompletedCount",
                          dailyCompletedCountVal);
  napi_value dailyQuestClaimedVal;
  napi_create_int32(env, snapshot.dailyQuestClaimed, &dailyQuestClaimedVal);
  napi_set_named_property(env, result, "dailyQuestClaimed", dailyQuestClaimedVal);
  napi_value gachaPoolKindVal;
  napi_create_int32(env, snapshot.gachaPoolKind, &gachaPoolKindVal);
  napi_set_named_property(env, result, "gachaPoolKind", gachaPoolKindVal);
  // ---- Phase 4：NPC 任务发布（尾部纯追加 2 字段） ----
  napi_value npcOfferQuestIdVal;
  napi_create_int32(env, snapshot.npcOfferQuestId, &npcOfferQuestIdVal);
  napi_set_named_property(env, result, "npcOfferQuestId", npcOfferQuestIdVal);
  napi_value npcOfferQuestTitleVal;
  napi_create_string_utf8(env, snapshot.npcOfferQuestTitle.c_str(),
                          snapshot.npcOfferQuestTitle.size(),
                          &npcOfferQuestTitleVal);
  napi_set_named_property(env, result, "npcOfferQuestTitle",
                          npcOfferQuestTitleVal);
  napi_value explorationPoiCountVal, explorationPuzzleCountVal,
      explorationRewardCountVal, explorationGateCountVal,
      explorationTraversalMaskVal, explorationCurrentPoiIdVal;
  napi_create_int32(env, snapshot.explorationPoiCount, &explorationPoiCountVal);
  napi_create_int32(env, snapshot.explorationPuzzleCount,
                    &explorationPuzzleCountVal);
  napi_create_int32(env, snapshot.explorationRewardCount,
                    &explorationRewardCountVal);
  napi_create_int32(env, snapshot.explorationGateCount,
                    &explorationGateCountVal);
  napi_create_int32(env, snapshot.explorationTraversalMask,
                    &explorationTraversalMaskVal);
  napi_create_int32(env, snapshot.explorationCurrentPoiId,
                    &explorationCurrentPoiIdVal);
  napi_set_named_property(env, result, "explorationPoiCount",
                          explorationPoiCountVal);
  napi_set_named_property(env, result, "explorationPuzzleCount",
                          explorationPuzzleCountVal);
  napi_set_named_property(env, result, "explorationRewardCount",
                          explorationRewardCountVal);
  napi_set_named_property(env, result, "explorationGateCount",
                          explorationGateCountVal);
  napi_set_named_property(env, result, "explorationTraversalMask",
                          explorationTraversalMaskVal);
  napi_set_named_property(env, result, "explorationCurrentPoiId",
                          explorationCurrentPoiIdVal);
  napi_value explorationCurrentTargetLabelVal,
      explorationCurrentTargetDistrictVal;
  napi_create_string_utf8(env, snapshot.explorationCurrentTargetLabel.c_str(),
                          snapshot.explorationCurrentTargetLabel.size(),
                          &explorationCurrentTargetLabelVal);
  napi_create_string_utf8(env,
                          snapshot.explorationCurrentTargetDistrict.c_str(),
                          snapshot.explorationCurrentTargetDistrict.size(),
                          &explorationCurrentTargetDistrictVal);
  napi_set_named_property(env, result, "explorationCurrentTargetLabel",
                          explorationCurrentTargetLabelVal);
  napi_set_named_property(env, result, "explorationCurrentTargetDistrict",
                          explorationCurrentTargetDistrictVal);
  napi_value explorationBlockedGateIdVal, explorationBlockedGateLabelVal,
      explorationBlockedByPuzzleLabelVal;
  napi_create_int32(env, snapshot.explorationBlockedGateId,
                    &explorationBlockedGateIdVal);
  napi_create_string_utf8(env, snapshot.explorationBlockedGateLabel.c_str(),
                          snapshot.explorationBlockedGateLabel.size(),
                          &explorationBlockedGateLabelVal);
  napi_create_string_utf8(
      env, snapshot.explorationBlockedByPuzzleLabel.c_str(),
      snapshot.explorationBlockedByPuzzleLabel.size(),
      &explorationBlockedByPuzzleLabelVal);
  napi_set_named_property(env, result, "explorationBlockedGateId",
                          explorationBlockedGateIdVal);
  napi_set_named_property(env, result, "explorationBlockedGateLabel",
                          explorationBlockedGateLabelVal);
  napi_set_named_property(env, result, "explorationBlockedByPuzzleLabel",
                          explorationBlockedByPuzzleLabelVal);
  return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"nativeStart", nullptr, NativeStart, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeStop", nullptr, NativeStop, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeStartIfForeground", nullptr, NativeStartIfForeground, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeSetModelAssets", nullptr, NativeSetModelAssets, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeSetEnvironmentAssets", nullptr, NativeSetEnvironmentAssets, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeSetBlockAsset", nullptr, NativeSetBlockEnvironmentAsset, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeSetNpcAsset", nullptr, NativeSetNpcAsset, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"pushInput", nullptr, NativePushInput, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"pushAction", nullptr, NativePushAction, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"startEncounter", nullptr, NativeStartEncounter, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"advanceLevel", nullptr, NativeAdvanceLevel, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"useSupply", nullptr, NativeUseSupply, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"retryBoss", nullptr, NativeRetryBoss, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"toggleDebugHud", nullptr, NativeToggleDebugHud, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"advanceDialog", nullptr, NativeAdvanceDialog, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"teleportToAnchor", nullptr, NativeTeleportToAnchor, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"saveProgress", nullptr, NativeSaveProgress, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"loadProgress", nullptr, NativeLoadProgress, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"performGacha", nullptr, NativePerformGacha, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"performWeaponGacha", nullptr, NativePerformWeaponGacha, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"useExpMaterial", nullptr, NativeUseExpMaterial, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"ascendCharacter", nullptr, NativeAscendCharacter, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"upgradeWeapon", nullptr, NativeUpgradeWeapon, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"equipWeapon", nullptr, NativeEquipWeapon, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"upgradeWeaponWithOre", nullptr, NativeUpgradeWeaponWithOre, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"ascendWeapon", nullptr, NativeAscendWeapon, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"refineWeapon", nullptr, NativeRefineWeapon, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"useExpItem", nullptr, NativeUseExpItem, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"upgradeArtifact", nullptr, NativeUpgradeArtifact, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"equipArtifact", nullptr, NativeEquipArtifact, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"claimRankReward", nullptr, NativeClaimRankReward, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"setQualityPreset", nullptr, NativeSetQualityPreset, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"setAudioEnabled", nullptr, NativeSetAudioEnabled, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"setPaused", nullptr, NativeSetPaused, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"skipDemoPhase", nullptr, NativeSkipDemoPhase, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"pullSnapshot", nullptr, NativePullSnapshot, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

  napi_value exportInstance = nullptr;
  napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
  if (exportInstance == nullptr) {
    LOGE("OH_NATIVE_XCOMPONENT_OBJ not found in exports");
    return exports;
  }

  OH_NativeXComponent* nativeXComponent = nullptr;
  napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent));
  if (nativeXComponent == nullptr) {
    LOGE("nativeXComponent is null");
    return exports;
  }

  static OH_NativeXComponent_Callback callback = {
    .OnSurfaceCreated = OnSurfaceCreated,
    .OnSurfaceChanged = OnSurfaceChanged,
    .OnSurfaceDestroyed = OnSurfaceDestroyed,
    .DispatchTouchEvent = OnDispatchTouchEvent,
  };
  OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);
  LOGI("XComponent callbacks registered");
  return exports;
}
EXTERN_C_END

static napi_module demoModule = {
  .nm_version = 1,
  .nm_flags = 0,
  .nm_filename = nullptr,
  .nm_register_func = Init,
  .nm_modname = "native_game",
  .nm_priv = nullptr,
  .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterNativeGame() {
  napi_module_register(&demoModule);
}
