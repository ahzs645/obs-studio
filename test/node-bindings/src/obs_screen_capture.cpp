#include <napi.h>
#include <obs.h>
#include "permission_manager.h"

static bool initialized = false;
static bool recording = false;
static obs_source_t *capture_source = nullptr;
static obs_output_t *file_output = nullptr;
static obs_encoder_t *video_encoder = nullptr;
static obs_encoder_t *audio_encoder = nullptr;

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

static void release_recording_objects()
{
  if (capture_source) {
    obs_source_release(capture_source);
    capture_source = nullptr;
  }
  if (file_output) {
    obs_output_release(file_output);
    file_output = nullptr;
  }
  if (video_encoder) {
    obs_encoder_release(video_encoder);
    video_encoder = nullptr;
  }
  if (audio_encoder) {
    obs_encoder_release(audio_encoder);
    audio_encoder = nullptr;
  }
}

Napi::Boolean StartRecording(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  if (!initialized) {
    Napi::Error::New(env, "OBS not initialized").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  if (recording)
    return Napi::Boolean::New(env, true);

  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Output path required").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  std::string path = info[0].As<Napi::String>();

  struct obs_video_info ovi = {};
  ovi.adapter = 0;
  ovi.graphics_module = "libobs-opengl";
  ovi.base_width = 1280;
  ovi.base_height = 720;
  ovi.output_width = 1280;
  ovi.output_height = 720;
  ovi.fps_num = 30;
  ovi.fps_den = 1;
  ovi.output_format = VIDEO_FORMAT_I420;
  ovi.colorspace = VIDEO_CS_601;
  ovi.range = VIDEO_RANGE_DEFAULT;
  if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) {
    Napi::Error::New(env, "Failed to reset video").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  struct obs_audio_info oai = {};
  oai.samples_per_sec = 48000;
  oai.speakers = SPEAKERS_STEREO;
  if (!obs_reset_audio(&oai)) {
    Napi::Error::New(env, "Failed to reset audio").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  obs_load_all_modules();
  obs_post_load_modules();

  obs_data_t *source_settings = obs_data_create();
#if defined(_WIN32)
  capture_source = obs_source_create("monitor_capture", "capture", source_settings, nullptr);
#elif defined(__APPLE__)
  capture_source = obs_source_create("display_capture", "capture", source_settings, nullptr);
#else
  capture_source = obs_source_create("xshm_input", "capture", source_settings, nullptr);
#endif
  obs_data_release(source_settings);
  if (!capture_source) {
    Napi::Error::New(env, "Failed to create capture source").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  obs_scene_t *scene = obs_scene_create("scene");
  obs_scene_add(scene, capture_source);
  obs_source_t *scene_source = obs_scene_get_source(scene);

  obs_data_t *vsettings = obs_data_create();
  obs_data_set_int(vsettings, "bitrate", 8000);
  video_encoder = obs_video_encoder_create("obs_x264", "simple_h264", vsettings, nullptr);
  obs_data_release(vsettings);
  if (!video_encoder) {
    Napi::Error::New(env, "Failed to create video encoder").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  obs_data_t *asettings = obs_data_create();
  obs_data_set_int(asettings, "bitrate", 160);
  audio_encoder = obs_audio_encoder_create("ffmpeg_aac", "simple_aac", asettings, 0, nullptr);
  obs_data_release(asettings);
  if (!audio_encoder) {
    Napi::Error::New(env, "Failed to create audio encoder").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  obs_encoder_set_video(video_encoder, obs_get_video());
  obs_encoder_set_audio(audio_encoder, obs_get_audio());

  obs_data_t *output_settings = obs_data_create();
  obs_data_set_string(output_settings, "path", path.c_str());
  file_output = obs_output_create("ffmpeg_muxer", "file_output", output_settings, nullptr);
  obs_data_release(output_settings);
  if (!file_output) {
    Napi::Error::New(env, "Failed to create output").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  obs_output_set_video_encoder(file_output, video_encoder);
  obs_output_set_audio_encoder(file_output, audio_encoder, 0);
  obs_output_set_mixers(file_output, 1);
  obs_output_set_service(file_output, nullptr);

  obs_source_inc_showing(scene_source);
  obs_source_inc_active(scene_source);
  obs_set_output_source(0, scene_source);

  if (!obs_output_start(file_output)) {
    Napi::Error::New(env, "Failed to start output").ThrowAsJavaScriptException();
    release_recording_objects();
    return Napi::Boolean::New(env, false);
  }

  recording = true;
  return Napi::Boolean::New(env, true);
}

Napi::Value StopRecording(const Napi::CallbackInfo& info)
{
  if (recording && file_output)
    obs_output_stop(file_output);
  release_recording_objects();
  recording = false;
  return info.Env().Undefined();
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
  exports.Set(Napi::String::New(env, "startRecording"), Napi::Function::New(env, StartRecording));
  exports.Set(Napi::String::New(env, "stopRecording"), Napi::Function::New(env, StopRecording));
  exports.Set(Napi::String::New(env, "shutdown"), Napi::Function::New(env, ShutdownOBS));
  return exports;
}

NODE_API_MODULE(obs_screen_capture, Init)
