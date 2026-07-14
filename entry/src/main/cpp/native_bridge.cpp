#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <string>
#include <hilog/log.h>
#include "engine/core/loop.h"

#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "Ethelan", __VA_ARGS__)

static Loop g_loop;

static InputAction MapTouchAction(OH_NativeXComponent_TouchEventType type) {
  switch (type) {
    case OH_NATIVEXCOMPONENT_DOWN: return InputAction::PointerDown;
    case OH_NATIVEXCOMPONENT_MOVE: return InputAction::PointerMove;
    case OH_NATIVEXCOMPONENT_UP: return InputAction::PointerUp;
    default: return InputAction::PointerCancel;
  }
}

static InputAction MapInputAction(int32_t type) {
  switch (type) {
    case 0: return InputAction::PointerDown;
    case 1: return InputAction::PointerUp;
    case 2: return InputAction::PointerMove;
    default: return InputAction::PointerCancel;
  }
}

static void OnSurfaceCreated(OH_NativeXComponent* component, void* window) {
  LOGI("OnSurfaceCreated");
  OHNativeWindow* nativeWindow = static_cast<OHNativeWindow*>(window);
  if (g_loop.surface.ready) {
    g_loop.stop();
    surface_destroy(g_loop.surface);
  }
  if (!surface_init(g_loop.surface, nativeWindow)) {
    LOGE("surface_init failed");
    return;
  }
  g_loop.start();
}

static void OnSurfaceChanged(OH_NativeXComponent* component, void* window) {
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
      return;
    }
  } else if (!surface_resize(g_loop.surface, nativeWindow)) {
    LOGE("surface resize failed");
    return;
  }
  g_loop.start();
}

static void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window) {
  LOGI("OnSurfaceDestroyed");
  g_loop.stop();
  surface_destroy(g_loop.surface);
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
  g_loop.start();
  return nullptr;
}

static napi_value NativeStop(napi_env env, napi_callback_info) {
  g_loop.stop();
  return nullptr;
}

static napi_value NativePushInput(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  napi_value typeVal, pointerIdVal, xVal, yVal;
  napi_get_named_property(env, args[0], "type", &typeVal);
  napi_get_named_property(env, args[0], "pointerId", &pointerIdVal);
  napi_get_named_property(env, args[0], "x", &xVal);
  napi_get_named_property(env, args[0], "y", &yVal);
  int32_t type, pointerId = -1; double x, y;
  napi_get_value_int32(env, typeVal, &type);
  napi_get_value_int32(env, pointerIdVal, &pointerId);
  napi_get_value_double(env, xVal, &x);
  napi_get_value_double(env, yVal, &y);
  g_loop.enqueueInput(MapInputAction(type), pointerId, static_cast<float>(x), static_cast<float>(y));
  return nullptr;
}

static napi_value NativePullSnapshot(napi_env env, napi_callback_info) {
  const GameSnapshot snapshot = g_loop.snapshot();
  napi_value result;
  napi_create_object(env, &result);
  napi_value tickVal, hpVal, poiseVal, xVal, yVal, fpsVal, movingVal, distVal;
  napi_value targetIdVal, bossPhaseVal, rendererReadyVal;
  napi_create_int64(env, static_cast<int64_t>(snapshot.tick), &tickVal);
  napi_create_int32(env, snapshot.hp, &hpVal);
  napi_create_int32(env, snapshot.poise, &poiseVal);
  napi_create_double(env, snapshot.playerX, &xVal);
  napi_create_double(env, snapshot.playerY, &yVal);
  napi_create_double(env, snapshot.fps, &fpsVal);
  napi_create_double(env, 0.0, &distVal);
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
