// tabletennis_app.h - ReXGlue application configuration for Rockstar Games presents Table Tennis

#pragma once

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include "offline_menu_device.h"
#include "intro_skip.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/rex_app.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/window.h>
#include <imgui.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

REXCVAR_DECLARE(bool, show_fps);
REXCVAR_DECLARE(int32_t, framerate);

class FpsOverlayDialog : public rex::ui::ImGuiDialog {
 public:
  explicit FpsOverlayDialog(rex::ui::ImGuiDrawer* drawer, ImFont* loading_font)
      : rex::ui::ImGuiDialog(drawer), loading_font_(loading_font) {}

  void SetVisible(bool visible) { visible_ = visible; }
  bool IsVisible() const { return visible_; }
  void Toggle() { visible_ = !visible_; }

 protected:
  void OnDraw(ImGuiIO& io) override {
    tabletennis::intro::PollGamepads();
    if (tabletennis::intro::ShowTransition()) {
      const auto* viewport = ImGui::GetMainViewport();
      auto* draw = ImGui::GetForegroundDrawList();
      draw->AddRectFilled(viewport->Pos,
          ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
          IM_COL32(0, 0, 0, 255));
      const char* message = "LOADING MAIN MENU...";
      auto* font = loading_font_ ? loading_font_ : ImGui::GetFont();
      // Use the baked loading face size, independent of the smaller overlay font.
      const float font_size = loading_font_ ? loading_font_->LegacySize
                                            : ImGui::GetFontSize() * 3.0f;
      const auto size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, message);
      draw->AddText(font, font_size,
                    ImVec2(viewport->Pos.x + (viewport->Size.x - size.x) * 0.5f,
                          viewport->Pos.y + (viewport->Size.y - size.y) * 0.5f),
                    IM_COL32(255, 255, 255, 255), message);
    }
    // Pace presentation thread to target framerate limit
    int32_t target_fps = REXCVAR_GET(framerate);
    if (target_fps > 0) {
      using clock = std::chrono::steady_clock;
      static auto last_draw = clock::now();
      const auto target_dur = std::chrono::nanoseconds(1'000'000'000LL / target_fps);
      auto now = clock::now();
      auto elapsed = now - last_draw;
      if (elapsed < target_dur) {
        auto sleep_dur = target_dur - elapsed;
        if (sleep_dur > std::chrono::milliseconds(2)) {
          std::this_thread::sleep_for(sleep_dur - std::chrono::milliseconds(1));
        }
        while (clock::now() - last_draw < target_dur) {
#if defined(_M_X64) || defined(__x86_64__)
          _mm_pause();
#endif
        }
      }
      last_draw = clock::now();
    }

    static int perf_log_counter = 0;
    if (++perf_log_counter % 120 == 0) {
      REXLOG_INFO("PERF TICK: Current FPS: {:.1f} ({:.2f} ms)", io.Framerate, (io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f));
    }

    if (!visible_) {
      return;
    }

    // Position overlay at top-left with slight offset
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.12f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.42f, 0.70f, 0.60f));

    if (ImGui::Begin("##FpsOverlay", nullptr, flags)) {
      float fps = io.Framerate;
      float ms = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;

      ImVec4 color = (fps >= 115.0f) ? ImVec4(0.0f, 0.9f, 1.0f, 1.0f) :   // Cyan for 120Hz/144Hz/240Hz
                     (fps >= 55.0f)  ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) :   // Green for 60Hz
                     (fps >= 30.0f)  ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f) :  // Yellow
                                       ImVec4(1.0f, 0.3f, 0.3f, 1.0f);    // Red

      ImGui::TextColored(color, "FPS: %.1f", fps);
      ImGui::SameLine();
      ImGui::TextDisabled("(%.2f ms)", ms);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
  }

 private:
  ImFont* loading_font_ = nullptr;
  bool visible_ = false;
};

class TabletennisApp : public rex::ReXApp {
 public:
  TabletennisApp(rex::ui::WindowedAppContext& ctx, const std::string_view name,
                 const rex::PPCImageInfo& ppc_info)
      : rex::ReXApp(ctx, name, ppc_info) {
    rex::cvar::SetFlagByName("protect_zero", "false");
    rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");
  }

  ~TabletennisApp() override {
    tabletennis::intro::Uninstall();
    rex::ui::UnregisterBind("bind_toggle_fps");
    rex::ui::UnregisterBind("bind_quit_game");
    if (imgui_drawer_ && fps_overlay_) {
      imgui_drawer_->RemoveDialog(fps_overlay_.get());
    }
    fps_overlay_.reset();
  }

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<TabletennisApp>(new TabletennisApp(ctx, "tabletennis",
        PPCImageConfig));
  }

 protected:
  void OnConfigurePaths(rex::PathConfig& paths) override {
    rex::cvar::SetFlagByName("protect_zero", "false");
    rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");

    auto exe_dir = rex::filesystem::GetExecutableFolder();
    auto root_dir = exe_dir.parent_path();

    // Scan settings.ini in executable folder
    std::filesystem::path settings_path = exe_dir / "settings.ini";
    if (std::filesystem::exists(settings_path)) {
      std::ifstream ini_file(settings_path);
      std::string line;
      while (std::getline(ini_file, line)) {
        if (line.rfind("iso_path=", 0) == 0 && (paths.game_data_root.empty() || !std::filesystem::exists(paths.game_data_root))) {
          std::string val = line.substr(9);
          while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' ' || val.back() == '"')) val.pop_back();
          if (!val.empty() && val.front() == '"') val = val.substr(1);
          if (!val.empty() && std::filesystem::exists(val)) {
            paths.game_data_root = val;
            REXLOG_INFO("TabletennisApp: Found ISO from settings.ini: {}", paths.game_data_root.string());
          }
        } else if (line.rfind("framerate=", 0) == 0) {
          std::string val = line.substr(10);
          try {
            int target = std::stoi(val);
            if (target >= 0) {
              REXCVAR_SET(framerate, target);
              REXLOG_INFO("TabletennisApp: Target framerate from settings.ini: {} FPS", target);
            }
          } catch (...) {}
        }
      }
    }

    // Auto-detect ISO if still empty
    if (paths.game_data_root.empty() || !std::filesystem::exists(paths.game_data_root)) {
      for (const auto& dir : {exe_dir, root_dir}) {
        if (std::filesystem::exists(dir)) {
          for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".iso") {
              paths.game_data_root = entry.path();
              REXLOG_INFO("TabletennisApp: Auto-detected ISO: {}", paths.game_data_root.string());
              break;
            }
          }
        }
        if (!paths.game_data_root.empty()) break;
      }
    }

    // Set isolated user_data and cache directories
    if (paths.user_data_root.empty() || paths.user_data_root == rex::filesystem::GetUserFolder() / GetName()) {
      auto custom_user = root_dir / "user_data";
      if (std::filesystem::exists(custom_user) || std::filesystem::exists(root_dir / "reports")) {
        paths.user_data_root = custom_user;
      }
    }

    if (paths.cache_root.empty() || paths.cache_root == paths.user_data_root / "cache") {
      auto custom_cache = root_dir / "cache";
      if (std::filesystem::exists(custom_cache) || std::filesystem::exists(root_dir / "reports")) {
        paths.cache_root = custom_cache;
      }
    }

    // Validate ISO if specified
    if (!paths.game_data_root.empty() && std::filesystem::is_regular_file(paths.game_data_root)) {
      ValidateIsoSource(paths.game_data_root);
    }
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    rex::cvar::SetFlagByName("protect_zero", "false");
    rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }

    // Always keep guest video mode refresh rate at 60.0 Hz.
    // This ensures guest VBlank interrupts fire at 60Hz so video player,
    // audio clocks, and menu background matches run at normal 1.0x real-time speed.
    rex::cvar::SetFlagByName("video_mode_refresh_rate", "60.0");

    int32_t target_hz = REXCVAR_GET(framerate);
    if (target_hz != 60) {
      // High refresh rate / uncapped presentation via D3D12 tearing
      rex::cvar::SetFlagByName("vsync", "false");
#ifdef _WIN32
      rex::cvar::SetFlagByName("d3d12_allow_variable_refresh_rate_and_tearing", "true");
#endif
      REXLOG_INFO("TabletennisApp::OnPreSetup - High Framerate Mode: target {} FPS (vsync=false, guest_vblank=60Hz)", target_hz);
    } else {
      rex::cvar::SetFlagByName("vsync", "true");
      REXLOG_INFO("TabletennisApp::OnPreSetup - Standard 60 Hz VSync enabled");
    }

    REXLOG_INFO("TabletennisApp::OnPreSetup - GPU plugin: {}, protect_zero: false", config.gpu_plugin);
  }

  void OnPostLoadXexImage() override {
    auto vfs = runtime()->kernel_state()->file_system();
    if (vfs) {
      vfs->RegisterSymbolicLink("cache:", "\\Device\\Harddisk0\\Cache0");

#ifdef _WIN32
      wchar_t exe_path_buf[MAX_PATH];
      GetModuleFileNameW(NULL, exe_path_buf, MAX_PATH);
      std::filesystem::path exe_dir = std::filesystem::path(exe_path_buf).parent_path();
#else
      const auto exe_dir = std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
      std::filesystem::path flash_override = exe_dir / "assets" / "resource" / "flash";
      if (std::filesystem::exists(flash_override / "flash.rpf")) {
        if (auto* root = vfs->ResolvePath("game:")) {
          auto dev = std::make_unique<OfflineMenuDevice>(flash_override, root->device());
          if (dev->Initialize() && vfs->RegisterDevice(std::move(dev))) {
            vfs->UnregisterSymbolicLink("game:");
            vfs->UnregisterSymbolicLink("d:");
            vfs->RegisterSymbolicLink("game:", "\\Device\\TabletennisOfflineMenu");
            vfs->RegisterSymbolicLink("d:", "\\Device\\TabletennisOfflineMenu");
            REXLOG_INFO("Tabletennis: Offline main-menu override mounted from {}", flash_override.string());
          }
        }
      }
      if (auto entry = vfs->ResolvePath("game:\\assets\\resource\\flash\\flash.rpf")) {
        REXLOG_INFO("Tabletennis: flash.rpf resolves to device {} ({} bytes)",
                    entry->device()->mount_path(), entry->size());
      }
    }
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override {
    // Keep the regular overlay font first. The loading message gets its own
    // TrueType face baked at 3x size, with extra raster resolution for DPI.
    if (atlas->Fonts.empty()) atlas->AddFontDefault();
#ifdef _WIN32
    wchar_t windows_dir[MAX_PATH]{};
    if (!GetWindowsDirectoryW(windows_dir, MAX_PATH)) return;
#endif
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = false;
    config.RasterizerDensity = 2.0f;
    static const ImWchar ranges[] = {0x20, 0x7E, 0};
#ifdef _WIN32
    const std::filesystem::path font_paths[] = {
      std::filesystem::path(windows_dir) / L"Fonts/seguisb.ttf",
      std::filesystem::path(windows_dir) / L"Fonts/segoeui.ttf",
      std::filesystem::path(windows_dir) / L"Fonts/arial.ttf"};
#else
    const std::filesystem::path font_paths[] = {
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf"};
#endif
    for (const auto& path : font_paths) {
      if (!std::filesystem::exists(path)) continue;
      loading_font_ = atlas->AddFontFromFileTTF(path.string().c_str(), 39.0f, &config, ranges);
      if (loading_font_) {
        REXLOG_INFO("Tabletennis: Loading text TrueType font: {} (39px, 2x raster density)", path.string());
        break;
      }
    }
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    imgui_drawer_ = drawer;
    if (drawer) {
      fps_overlay_ = std::make_unique<FpsOverlayDialog>(drawer, loading_font_);
      bool initial_visible = REXCVAR_GET(show_fps);
      fps_overlay_->SetVisible(initial_visible);
      drawer->AddDialog(fps_overlay_.get());
      REXLOG_INFO("TabletennisApp: FPS overlay created (initial state: {})", initial_visible ? "ON" : "OFF");
    }

    // Hotkey F10 toggles FPS overlay during gameplay
    rex::ui::RegisterBind("bind_toggle_fps", "F10", "Toggle In-Game FPS Counter", [this] {
      if (fps_overlay_) {
        fps_overlay_->Toggle();
        REXLOG_INFO("FPS counter toggled via F10: {}", fps_overlay_->IsVisible() ? "ON" : "OFF");
      }
    });

    // Hotkey ESC quits the game cleanly.
    rex::ui::RegisterBind("bind_quit_game", "Escape", "Quit Game", [this] {
      REXLOG_INFO("ESC pressed. Exiting cleanly...");
      if (window()) {
        window()->RequestClose();
      } else {
        std::_Exit(0);
      }
    });

    REXLOG_INFO("TabletennisApp: Binds for ESC (Quit) and F10 (FPS Toggle) are active");
  }

  void OnPostSetup() override {
    tabletennis::intro::Install();
    REXLOG_INFO("TabletennisApp::OnPostSetup - Runtime and VFS fully initialized");
  }

 private:
  void ValidateIsoSource(const std::filesystem::path& iso_path) {
    std::ifstream f(iso_path, std::ios::binary);
    if (!f.is_open()) {
      REXLOG_ERROR("Could not open ISO file: {}", iso_path.string());
      return;
    }

    // Check GDFX magic at sector 32 of game partition (offset 0xFDA0000 in XGD2)
    f.seekg(0xFDA0000);
    char magic[20] = {0};
    f.read(magic, 20);
    if (std::memcmp(magic, "MICROSOFT*XBOX*MEDIA", 20) == 0) {
      REXLOG_INFO("ISO validation PASSED: GDFX filesystem identified at sector 32 (offset 0xFDA0000)");
    } else {
      REXLOG_WARN("ISO validation WARNING: GDFX magic not found at 0xFDA0000. Image may be corrupt or non-standard.");
    }
  }

  ImFont* loading_font_ = nullptr;
  std::unique_ptr<FpsOverlayDialog> fps_overlay_;
  rex::ui::ImGuiDrawer* imgui_drawer_ = nullptr;
};
