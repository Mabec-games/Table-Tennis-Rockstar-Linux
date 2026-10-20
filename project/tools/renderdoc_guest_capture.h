// Private diagnostic capture spanning guest frames, independent of presentation.
#pragma once
#include <dlfcn.h>
#include <cstdio>
#include <renderdoc_app.h>

namespace tt_diagnostics {
inline void GuestFrameBoundary() {
  static RENDERDOC_API_1_0_0* api = nullptr;
  static unsigned frames_remaining = 0;
  if (frames_remaining) {
    if (--frames_remaining == 0) {
      auto saved = api->EndFrameCapture(nullptr, nullptr);
      REXGPU_WARN("TT_CAPTURE capture ended, saved={}", saved);
    }
    return;
  }
  // Only inspect a request file; never load RenderDoc into a normal process.
  auto request = std::fopen("renderdoc-capture.request", "r");
  if (!request) return;
  std::fclose(request);
  std::remove("renderdoc-capture.request");
  if (!api) {
    void* library = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
    if (library) {
      auto get_api = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(library, "RENDERDOC_GetAPI"));
      if (get_api) get_api(eRENDERDOC_API_Version_1_0_0, reinterpret_cast<void**>(&api));
      // Retain the loaded library reference while API pointers are in use.
    }
  }
  if (!api) {
    REXGPU_WARN("TT_CAPTURE request ignored: RenderDoc is not loaded");
    return;
  }
  if (api->IsFrameCapturing()) {
    REXGPU_WARN("TT_CAPTURE request ignored: another capture is active");
    return;
  }
  api->SetLogFilePathTemplate("renderdoc-captures/guest-frames");
  api->StartFrameCapture(nullptr, nullptr);
  if (api->IsFrameCapturing()) {
    frames_remaining = 4;
    REXGPU_WARN("TT_CAPTURE started capture across four guest frames");
  } else {
    REXGPU_WARN("TT_CAPTURE failed to start capture");
  }
}
}
