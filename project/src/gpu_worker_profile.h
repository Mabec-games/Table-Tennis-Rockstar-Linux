#pragma once

// Optional bridge from the separately built Linux GPU plugin to the executable.
#if defined(__linux__)
#include <dlfcn.h>
#include <utility>

inline void TabletennisGpuProfileEvent(unsigned event) {
  using Callback = void (*)(unsigned);
  static Callback callback = reinterpret_cast<Callback>(dlsym(RTLD_DEFAULT, "Tabletennis_ProfileGraphics"));
  if (callback) callback(event);
}

class TabletennisGpuProfileScope {
 public:
  explicit TabletennisGpuProfileScope(unsigned event) : event_(event) {
    TabletennisGpuProfileEvent(event_);
  }
  ~TabletennisGpuProfileScope() { TabletennisGpuProfileEvent(event_ + 1); }
  TabletennisGpuProfileScope(const TabletennisGpuProfileScope&) = delete;
  TabletennisGpuProfileScope& operator=(const TabletennisGpuProfileScope&) = delete;

 private:
  unsigned event_;
};
#else
inline void TabletennisGpuProfileEvent(unsigned) {}
class TabletennisGpuProfileScope {
 public:
  explicit TabletennisGpuProfileScope(unsigned) {}
};
#endif
