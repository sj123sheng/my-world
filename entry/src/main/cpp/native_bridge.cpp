#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <string>
#include <hilog/log.h>
#include <cmath>
#include <atomic>
#include "engine/core/loop.h"
#include "engine/input/pointer_input.h"

#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "Ethelan", __VA_ARGS__)

static Loop g_loop;
static std::atomic_bool g_foregroundRequested{false};

static InputAction MapTouchAction(OH_NativeXComponent_TouchEventType type) {
  switch (type) {
    case OH_NATIVEXCOMPONENT_DOWN: return InputAction::PointerDown;
    case OH_NATIVEXCOMPONENT_MOVE: return InputAction::PointerMove;
    case OH_NATIVEXCOMPONENT_UP: return InputAction::PointerUp;
    default: return InputAction::PointerCancel;
  }
}

static void InvalidateSurfaceSnapshot() {
  surface_destroy(g_loop.surface);
  g_loop.publishRendererStopped();
}

static napi_value ThrowInputTypeError(napi_env env, const char* message) {
  napi_throw_type_error(env, nullptr, message);
  return nullptr;
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
  OH_NativeXComponent_TouchEvent touchEvent;
  int32_t ret = OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent);
  if (ret != 0) return;
  g_loop.enqueueInput(MapTouchAction(touchEvent.type),
                      static_cast<int32_t>(touchEvent.id),
                      touchEvent.x,
                      touchEvent.y);
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
  double pointerIdNumber = 0.0;
  double x = 0.0;
  double y = 0.0;
  if (!GetNumberProperty(env, args[0], "type", true, typeNumber) ||
      !GetNumberProperty(env, args[0], "pointerId", false, pointerIdNumber) ||
      !GetNumberProperty(env, args[0], "x", true, x) ||
      !GetNumberProperty(env, args[0], "y", true, y)) {
    return ThrowInputTypeError(env, "pushInput requires numeric type/x/y and optional pointerId");
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

static napi_value NativePullSnapshot(napi_env env, napi_callback_info) {
  const GameSnapshot snapshot = g_loop.snapshot();
  napi_value result;
  napi_create_object(env, &result);
  napi_value tickVal, hpVal, poiseVal, xVal, yVal, fpsVal, movingVal;
  napi_value moveXVal, moveYVal, cameraYawVal, cameraPitchVal, distVal;
  napi_value targetIdVal, bossPhaseVal, rendererReadyVal;
  napi_create_int64(env, static_cast<int64_t>(snapshot.tick), &tickVal);
  napi_create_int32(env, snapshot.hp, &hpVal);
  napi_create_int32(env, snapshot.poise, &poiseVal);
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
  napi_get_boolean(env, snapshot.moving, &movingVal);
  napi_get_boolean(env, snapshot.rendererReady, &rendererReadyVal);
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
  napi_set_named_property(env, result, "rendererReady", rendererReadyVal);
  return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"nativeStart", nullptr, NativeStart, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeStop", nullptr, NativeStop, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"pushInput", nullptr, NativePushInput, nullptr, nullptr, nullptr, napi_default, nullptr},
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
