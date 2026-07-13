#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <string>
#include <hilog/log.h>
#include <cmath>
#include "engine/core/loop.h"
#include "engine/input/input_queue.h"

#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, "Ethelan", __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, "Ethelan", __VA_ARGS__)

static Loop g_loop;

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
  g_loop.input.push({ static_cast<int>(touchEvent.type), touchEvent.x, touchEvent.y });
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
  napi_value typeVal, xVal, yVal;
  napi_get_named_property(env, args[0], "type", &typeVal);
  napi_get_named_property(env, args[0], "x", &xVal);
  napi_get_named_property(env, args[0], "y", &yVal);
  int32_t type; double x, y;
  napi_get_value_int32(env, typeVal, &type);
  napi_get_value_double(env, xVal, &x);
  napi_get_value_double(env, yVal, &y);
  g_loop.input.push({type, (float)x, (float)y});
  return nullptr;
}

static napi_value NativePullSnapshot(napi_env env, napi_callback_info) {
  napi_value result;
  napi_create_object(env, &result);
  const Player& p = g_loop.surface.player;
  float dx = p.targetX - p.x;
  float dy = p.targetY - p.y;
  float dist = std::sqrt(dx * dx + dy * dy);
  napi_value hpVal, poiseVal, xVal, yVal, fpsVal, movingVal, distVal, rendererReadyVal;
  napi_create_int32(env, 100, &hpVal);
  napi_create_int32(env, 100, &poiseVal);
  napi_create_double(env, p.x, &xVal);
  napi_create_double(env, p.y, &yVal);
  napi_create_double(env, g_loop.fps, &fpsVal);
  napi_create_double(env, dist, &distVal);
  napi_get_boolean(env, p.moving, &movingVal);
  napi_get_boolean(env, g_loop.surface.ready, &rendererReadyVal);
  napi_set_named_property(env, result, "hp", hpVal);
  napi_set_named_property(env, result, "poise", poiseVal);
  napi_set_named_property(env, result, "x", xVal);
  napi_set_named_property(env, result, "y", yVal);
  napi_set_named_property(env, result, "fps", fpsVal);
  napi_set_named_property(env, result, "moving", movingVal);
  napi_set_named_property(env, result, "targetDist", distVal);
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
