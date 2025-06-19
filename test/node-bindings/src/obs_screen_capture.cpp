#include <napi.h>
#include <obs.h>
#include "permission_manager.h"
#include <vector>
#if defined(__linux__)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#import <Cocoa/Cocoa.h>
#endif

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

  uint32_t width = 1280;
  uint32_t height = 720;
  int fps = 30;
  int display_id = 0;
  std::string display_uuid;
  int window_id = 0;
  bool use_window = false;

  if (info.Length() > 1 && info[1].IsObject()) {
    Napi::Object opts = info[1].As<Napi::Object>();
    if (opts.Has("width"))
      width = opts.Get("width").As<Napi::Number>().Uint32Value();
    if (opts.Has("height"))
      height = opts.Get("height").As<Napi::Number>().Uint32Value();
    if (opts.Has("fps"))
      fps = opts.Get("fps").As<Napi::Number>().Int32Value();
    if (opts.Has("displayId")) {
      if (opts.Get("displayId").IsNumber())
        display_id = opts.Get("displayId").As<Napi::Number>().Int32Value();
      else
        display_uuid = opts.Get("displayId").As<Napi::String>().Utf8Value();
    }
    if (opts.Has("windowId")) {
      window_id = opts.Get("windowId").As<Napi::Number>().Int32Value();
      use_window = true;
    }
  }

  struct obs_video_info ovi = {};
  ovi.adapter = 0;
  ovi.graphics_module = "libobs-opengl";
  ovi.base_width = width;
  ovi.base_height = height;
  ovi.output_width = width;
  ovi.output_height = height;
  ovi.fps_num = fps;
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
  const char *src_id = use_window ? "window_capture" : "monitor_capture";
  if (use_window)
    obs_data_set_int(source_settings, "window", window_id);
  else
    obs_data_set_int(source_settings, "monitor", display_id);
  capture_source = obs_source_create(src_id, "capture", source_settings, nullptr);
#elif defined(__APPLE__)
  const char *src_id = use_window ? "window_capture" : "display_capture";
  if (use_window)
    obs_data_set_int(source_settings, "window", window_id);
  else
    obs_data_set_string(source_settings, "display_uuid", display_uuid.c_str());
  capture_source = obs_source_create(src_id, "capture", source_settings, nullptr);
#else
  const char *src_id = "xshm_input";
  obs_data_set_int(source_settings, "screen", display_id);
  capture_source = obs_source_create(src_id, "capture", source_settings, nullptr);
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

Napi::Value ListDisplays(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  Napi::Array arr = Napi::Array::New(env);
#if defined(__linux__)
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy)
    return arr;
  Window root = DefaultRootWindow(dpy);
  XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
  if (res) {
    int index = 0;
    for (int i = 0; i < res->noutput; ++i) {
      XRROutputInfo *infoo = XRRGetOutputInfo(dpy, res, res->outputs[i]);
      if (infoo && infoo->connection == RR_Connected && infoo->crtc) {
        XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, infoo->crtc);
        if (crtc) {
          Napi::Object o = Napi::Object::New(env);
          o.Set("id", Napi::Number::New(env, res->outputs[i]));
          o.Set("name", Napi::String::New(env, infoo->name));
          o.Set("width", Napi::Number::New(env, crtc->width));
          o.Set("height", Napi::Number::New(env, crtc->height));
          arr.Set(index++, o);
          XRRFreeCrtcInfo(crtc);
        }
      }
      if (infoo)
        XRRFreeOutputInfo(infoo);
    }
    XRRFreeScreenResources(res);
  }
  XCloseDisplay(dpy);
#elif defined(__APPLE__)
  uint32_t count = 0;
  CGGetActiveDisplayList(0, nullptr, &count);
  std::vector<CGDirectDisplayID> displays(count);
  CGGetActiveDisplayList(count, displays.data(), &count);
  for (uint32_t i = 0; i < count; ++i) {
    CGDirectDisplayID did = displays[i];
    uint32_t width = CGDisplayPixelsWide(did);
    uint32_t height = CGDisplayPixelsHigh(did);
    CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(did);
    CFStringRef uuidStr = CFUUIDCreateString(kCFAllocatorDefault, uuid);
    Napi::Object o = Napi::Object::New(env);
    o.Set("id", Napi::String::New(env, CFStringGetCStringPtr(uuidStr, kCFStringEncodingUTF8)));
    o.Set("width", Napi::Number::New(env, width));
    o.Set("height", Napi::Number::New(env, height));
    arr.Set(i, o);
    CFRelease(uuidStr);
    CFRelease(uuid);
  }
#elif defined(_WIN32)
  struct EnumCtx {
    Napi::Env env; Napi::Array arr; uint32_t idx; } ctx{env, arr, 0};
  EnumDisplayMonitors(NULL, NULL, [](HMONITOR mon, HDC, LPRECT rect, LPARAM data) -> BOOL {
    EnumCtx *c = reinterpret_cast<EnumCtx*>(data);
    MONITORINFOEXA mi; mi.cbSize = sizeof(mi);
    if (GetMonitorInfoA(mon, (MONITORINFO*)&mi)) {
      Napi::Object o = Napi::Object::New(c->env);
      o.Set("id", Napi::Number::New(c->env, reinterpret_cast<uintptr_t>(mon)));
      o.Set("name", Napi::String::New(c->env, mi.szDevice));
      o.Set("width", Napi::Number::New(c->env, mi.rcMonitor.right - mi.rcMonitor.left));
      o.Set("height", Napi::Number::New(c->env, mi.rcMonitor.bottom - mi.rcMonitor.top));
      c->arr.Set(c->idx++, o);
    }
    return TRUE;
  }, (LPARAM)&ctx);
#endif
  return arr;
}

Napi::Value ListWindows(const Napi::CallbackInfo& info)
{
  Napi::Env env = info.Env();
  Napi::Array arr = Napi::Array::New(env);
#if defined(__APPLE__)
  CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  CFIndex count = CFArrayGetCount(list);
  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
    CFNumberRef wid = (CFNumberRef)CFDictionaryGetValue(dict, kCGWindowNumber);
    CFStringRef owner = (CFStringRef)CFDictionaryGetValue(dict, kCGWindowOwnerName);
    CFStringRef name = (CFStringRef)CFDictionaryGetValue(dict, kCGWindowName);
    int id = 0; CFNumberGetValue(wid, kCFNumberIntType, &id);
    Napi::Object o = Napi::Object::New(env);
    o.Set("id", Napi::Number::New(env, id));
    if (owner)
      o.Set("owner", Napi::String::New(env, CFStringGetCStringPtr(owner, kCFStringEncodingUTF8)));
    if (name)
      o.Set("name", Napi::String::New(env, CFStringGetCStringPtr(name, kCFStringEncodingUTF8)));
    arr.Set(i, o);
  }
  CFRelease(list);
#elif defined(__linux__)
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy)
    return arr;
  Window root = DefaultRootWindow(dpy);
  Window parent; Window *children = nullptr; unsigned int nchildren = 0;
  if (XQueryTree(dpy, root, &root, &parent, &children, &nchildren)) {
    int index = 0;
    for (unsigned int i = 0; i < nchildren; ++i) {
      XWindowAttributes attrs; XGetWindowAttributes(dpy, children[i], &attrs);
      if (attrs.map_state != IsViewable)
        continue;
      XTextProperty tp; char **list_return = nullptr; int count = 0;
      if (XGetWMName(dpy, children[i], &tp) && tp.value) {
        if (XmbTextPropertyToTextList(dpy, &tp, &list_return, &count) >= Success && count > 0) {
          Napi::Object o = Napi::Object::New(env);
          o.Set("id", Napi::Number::New(env, children[i]));
          o.Set("name", Napi::String::New(env, list_return[0]));
          o.Set("width", Napi::Number::New(env, attrs.width));
          o.Set("height", Napi::Number::New(env, attrs.height));
          arr.Set(index++, o);
          XFreeStringList(list_return);
        }
      }
    }
  }
  if (children)
    XFree(children);
  XCloseDisplay(dpy);
#elif defined(_WIN32)
  struct WinCtx { Napi::Env env; Napi::Array arr; uint32_t idx; } ctx{env, arr, 0};
  EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
    WinCtx *c = reinterpret_cast<WinCtx*>(lParam);
    if (!IsWindowVisible(hwnd))
      return TRUE;
    char title[256];
    if (GetWindowTextA(hwnd, title, sizeof(title))) {
      RECT r; GetWindowRect(hwnd, &r);
      Napi::Object o = Napi::Object::New(c->env);
      o.Set("id", Napi::Number::New(c->env, (uintptr_t)hwnd));
      o.Set("name", Napi::String::New(c->env, title));
      o.Set("width", Napi::Number::New(c->env, r.right - r.left));
      o.Set("height", Napi::Number::New(c->env, r.bottom - r.top));
      c->arr.Set(c->idx++, o);
    }
    return TRUE;
  }, (LPARAM)&ctx);
#endif
  return arr;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "obsVersion"), Napi::Function::New(env, ObsVersion));
  exports.Set(Napi::String::New(env, "checkScreenPermission"), Napi::Function::New(env, CheckScreenPermission));
  exports.Set(Napi::String::New(env, "requestScreenPermission"), Napi::Function::New(env, RequestScreenPermission));
  exports.Set(Napi::String::New(env, "init"), Napi::Function::New(env, InitOBS));
  exports.Set(Napi::String::New(env, "startRecording"), Napi::Function::New(env, StartRecording));
  exports.Set(Napi::String::New(env, "stopRecording"), Napi::Function::New(env, StopRecording));
  exports.Set(Napi::String::New(env, "shutdown"), Napi::Function::New(env, ShutdownOBS));
  exports.Set(Napi::String::New(env, "listDisplays"), Napi::Function::New(env, ListDisplays));
  exports.Set(Napi::String::New(env, "listWindows"), Napi::Function::New(env, ListWindows));
  return exports;
}

NODE_API_MODULE(obs_screen_capture, Init)
