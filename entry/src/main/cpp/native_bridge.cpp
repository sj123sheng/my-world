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
    return ThrowInputTypeError(env, "pushAction type must be a finite integer from 0 to 5");
  }
  int32_t type = 0;
  if (!TryConvertInt32(typeNumber, type) || type < 0 || type > 5) {
    return ThrowInputTypeError(env, "pushAction type must be an integer from 0 to 5");
  }
  static constexpr InputAction kActions[] = {
      InputAction::Attack, InputAction::Dodge, InputAction::Radiance,
      InputAction::Current, InputAction::Corruption, InputAction::Ultimate};
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

static napi_value NativePullSnapshot(napi_env env, napi_callback_info) {
  const GameSnapshot snapshot = g_loop.snapshot();
  napi_value result;
  napi_create_object(env, &result);
  napi_value tickVal, hpVal, poiseVal, xVal, yVal, fpsVal, movingVal;
  napi_value moveXVal, moveYVal, cameraYawVal, cameraPitchVal, distVal;
  napi_value targetIdVal, bossPhaseVal, encounterModeVal, encounterStateVal, rendererReadyVal;
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
  napi_create_int32(env, snapshot.targetId, &targetIdVal);
  napi_create_int32(env, snapshot.bossPhase, &bossPhaseVal);
  napi_create_int32(env, snapshot.encounterMode, &encounterModeVal);
  napi_create_int32(env, snapshot.encounterState, &encounterStateVal);
  napi_get_boolean(env, snapshot.moving, &movingVal);
  napi_get_boolean(env, snapshot.rendererReady, &rendererReadyVal);
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
  napi_set_named_property(env, result, "targetId", targetIdVal);
  napi_set_named_property(env, result, "bossPhase", bossPhaseVal);
  napi_set_named_property(env, result, "encounterMode", encounterModeVal);
  napi_set_named_property(env, result, "encounterState", encounterStateVal);
  napi_set_named_property(env, result, "rendererReady", rendererReadyVal);
  napi_set_named_property(env, result, "stamina", staminaVal);
  napi_set_named_property(env, result, "comboSegment", comboSegmentVal);
  napi_set_named_property(env, result, "invulnerable", invulnerableVal);
  napi_set_named_property(env, result, "insightMs", insightMsVal);
  napi_set_named_property(env, result, "resonance", resonanceVal);
  napi_set_named_property(env, result, "targetHp", targetHpVal);
  napi_set_named_property(env, result, "targetPoise", targetPoiseVal);
  napi_set_named_property(env, result, "pulseHitRemainingMs", pulseHitRemainingMsVal);
  napi_set_named_property(env, result, "lastRejectReason", lastRejectReasonVal);
  napi_value extra[13];
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
  return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"nativeStart", nullptr, NativeStart, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeStop", nullptr, NativeStop, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeStartIfForeground", nullptr, NativeStartIfForeground, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeSetModelAssets", nullptr, NativeSetModelAssets, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"pushInput", nullptr, NativePushInput, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"pushAction", nullptr, NativePushAction, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"startEncounter", nullptr, NativeStartEncounter, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"advanceLevel", nullptr, NativeAdvanceLevel, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"useSupply", nullptr, NativeUseSupply, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"retryBoss", nullptr, NativeRetryBoss, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"toggleDebugHud", nullptr, NativeToggleDebugHud, nullptr, nullptr, nullptr, napi_default, nullptr},
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
