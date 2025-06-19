#include <napi.h>
#include <obs.h>
#include "permission_manager.h"

static bool initialized = false;

Napi::String ObsVersion(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  const char* version = obs_get_version_string();
  return Napi::String::New(env, version ? version : "unknown");
}

Napi::Boolean CheckScreenPermission(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Napi::Boolean::New(env, check_screen_permission());
}

Napi::Boolean RequestScreenPermission(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Napi::Boolean::New(env, request_screen_permission());
}

Napi::Boolean InitOBS(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (initialized)
    return Napi::Boolean::New(env, true);
  if (!obs_startup("en-US", nullptr, nullptr)) {
    Napi::Error::New(env, "Failed to start OBS").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  initialized = true;
  return Napi::Boolean::New(env, true);
}

Napi::Value ShutdownOBS(const Napi::CallbackInfo& info) {
  if (initialized) {
    obs_shutdown();
    initialized = false;
  }
  return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "obsVersion"), Napi::Function::New(env, ObsVersion));
  exports.Set(Napi::String::New(env, "checkScreenPermission"), Napi::Function::New(env, CheckScreenPermission));
  exports.Set(Napi::String::New(env, "requestScreenPermission"), Napi::Function::New(env, RequestScreenPermission));
  exports.Set(Napi::String::New(env, "init"), Napi::Function::New(env, InitOBS));
  exports.Set(Napi::String::New(env, "shutdown"), Napi::Function::New(env, ShutdownOBS));
  return exports;
}

NODE_API_MODULE(obs_screen_capture, Init)
