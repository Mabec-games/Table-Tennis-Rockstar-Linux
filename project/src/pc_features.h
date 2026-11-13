#pragma once
#include <functional>
#include <cstdint>
#include <cstdlib>
struct PPCContext;
namespace tabletennis::pc {
// Troubleshooting switches, set as environment variables with the value 1:
//   TT_LEGACY_PACING       pace on the presenting thread, as Linux v1.2 did
//   TT_NO_ASPECT           keep the picture and letterboxing at the v1.2 16:9 behaviour
//   TT_NO_ACHIEVEMENT_UI   no achievement popup, list or sound
//   TT_NO_UPDATE_CHECK     do not start the background update check
//   TT_NO_PROMPTS          do not follow the controller type for button icons
inline bool Diagnostic(const char* name) {
  const char* value = std::getenv(name);
  return value && value[0] == '1';
}
double OverlayAspect();
void ToggleAchievements();
void SetAchievementsToggle(std::function<void()> callback);
void SetWindow(void* window);
void SetSurfaceSize(uint32_t width, uint32_t height);
void SetGameplay(bool active);
double CameraAspect(double original);
double VideoAspect(double original);
void FixUi(PPCContext& ctx, uint8_t* base, uint32_t matrix, double width);
void EndUi(PPCContext& ctx, uint8_t* base);
void Scissor(PPCContext& ctx);
void CheckQuit(PPCContext& ctx, uint8_t* base, uint32_t movie);
void SetQuitCallback(std::function<void()> callback);
void RequestQuit();
}
