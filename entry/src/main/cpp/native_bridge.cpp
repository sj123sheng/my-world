#include "napi/native_api.h"
#include <string>
#include "../../../native/engine/core/loop.h"
#include "../../../native/engine/input/input_queue.h"

static Loop g_loop;
static InputQueue g_inputQueue;

static napi_value NativeStart(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  char buf[128] = {0};
  size_t len = 0;
  napi_get_value_string_utf8(env, args[0], buf, sizeof(buf), &len);
  g_loop.start(buf);
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
  g_inputQueue.push({type, (float)x, (float)y});
  return nullptr;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"nativeStart", nullptr, NativeStart, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeStop", nullptr, NativeStop, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"pushInput", nullptr, NativePushInput, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  return exports;
}
EXTERN_C_END

static napi_module demoModule = {
  .nm_version = 1,
  .nm_flags = 0,
  .nm_filename = nullptr,
  .nm_register_func = Init,
  .nm_modname = "native_game",
  .nm_priv = nullptr
};

extern "C" void RegisterNativeGame() {
  napi_module_register(&demoModule);
}
