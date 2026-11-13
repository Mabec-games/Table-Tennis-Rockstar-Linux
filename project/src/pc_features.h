#pragma once
#include <functional>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
struct PPCContext;
namespace tabletennis::pc {
// Compatibility switches, environment variables set to 1 (on) or 0 (off):
//   TT_LEGACY_PACING       pace on the presenting thread, as Linux v1.2 did
//   TT_NO_ASPECT           keep the picture and letterboxing at the v1.2 16:9 behaviour
//   TT_NO_ACHIEVEMENT_UI   no achievement popup, list or sound
//   TT_NO_UPDATE_CHECK     do not start the background update check
//   TT_NO_PROMPTS          do not follow the controller type for button icons
// On a Steam Deck they all default to on: v1.3 took down the Deck's session
// (issue #4) and the responsible change is not yet known. Elsewhere they
// default to off.
inline bool IsSteamDeck() {
  static const bool deck = [] {
    const char* steam = std::getenv("SteamDeck");
    if (steam && steam[0] == '1') return true;
    std::string product;
    std::ifstream file("/sys/devices/virtual/dmi/id/product_name");
    std::getline(file, product);
    return product == "Jupiter" || product == "Galileo";
  }();
  return deck;
}
inline bool Diagnostic(const char* name) {
  const char* value = std::getenv(name);
  if (value && (value[0] == '0' || value[0] == '1')) return value[0] == '1';
  return IsSteamDeck();
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
