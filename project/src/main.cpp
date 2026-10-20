// tabletennis - ReXGlue Recompiled Project

#include "generated/default/tabletennis_init.h"
#include <rex/cvar.h>
#include <chrono>
#include <thread>
#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#include "tabletennis_app.h"
#include "guest_cpu_profile.h"

REXCVAR_DEFINE_BOOL(show_fps, false, "UI", "Show in-game FPS counter");
REXCVAR_DEFINE_INT32(framerate, 60, "Video", "Target framerate limit (60, 120, 144, 240, 0=unlimited)");

static double TabletennisCurrentThreadCpuSeconds() {
#if defined(_WIN32)
  FILETIME creation{}, exit{}, kernel{}, user{};
  if (!GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user)) return 0.0;
  ULARGE_INTEGER kernel_ticks{}, user_ticks{};
  kernel_ticks.LowPart = kernel.dwLowDateTime;
  kernel_ticks.HighPart = kernel.dwHighDateTime;
  user_ticks.LowPart = user.dwLowDateTime;
  user_ticks.HighPart = user.dwHighDateTime;
  return static_cast<double>(kernel_ticks.QuadPart + user_ticks.QuadPart) * 1.0e-7;
#else
  timespec cpu_time{};
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_time) != 0) return 0.0;
  return static_cast<double>(cpu_time.tv_sec) + static_cast<double>(cpu_time.tv_nsec) * 1.0e-9;
#endif
}

#undef __imp__VdSwap
extern "C" REX_FUNC(__imp__VdSwap);

extern "C" void Tabletennis_VdSwap_Hook(PPCContext& ctx, uint8_t* base) {
  tabletennis::cpu_profile::Start();
  // Measure guest swaps separately from the ImGui presentation FPS counter.
  using timing_clock = std::chrono::steady_clock;
  const auto swap_begin = timing_clock::now();
  const double thread_cpu_now = TabletennisCurrentThreadCpuSeconds();
  // Call original VdSwap to commit guest frame to GPU
  __imp__VdSwap(ctx, base);
  const auto swap_return = timing_clock::now();

  // High-precision frame limiter for high refresh rate modes
  int32_t target_fps = REXCVAR_GET(framerate);
  if (target_fps > 0) {
    using clock = std::chrono::steady_clock;
    static thread_local auto last_swap = clock::now();
    const auto target_duration = std::chrono::nanoseconds(1'000'000'000LL / target_fps);

    auto now = clock::now();
    auto elapsed = now - last_swap;
    if (elapsed < target_duration) {
      auto sleep_dur = target_duration - elapsed;
      if (sleep_dur > std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(sleep_dur - std::chrono::milliseconds(1));
      }
      while (clock::now() - last_swap < target_duration) {
#if defined(_M_X64) || defined(__x86_64__)
        _mm_pause();
#endif
      }
    }
    last_swap = clock::now();
  }
  const auto swap_end = timing_clock::now();
  static thread_local auto timing_window = swap_begin;
  static thread_local double last_thread_cpu = thread_cpu_now;
  static thread_local uint32_t guest_swaps = 0;
  static thread_local double vd_swap_ms = 0.0, limiter_ms = 0.0, thread_cpu_ms = 0.0;
  ++guest_swaps;
  vd_swap_ms += std::chrono::duration<double, std::milli>(swap_return - swap_begin).count();
  limiter_ms += std::chrono::duration<double, std::milli>(swap_end - swap_return).count();
  if (thread_cpu_now >= last_thread_cpu) {
    thread_cpu_ms += (thread_cpu_now - last_thread_cpu) * 1000.0;
  }
  last_thread_cpu = thread_cpu_now;
  const double seconds = std::chrono::duration<double>(swap_end - timing_window).count();
  if (seconds >= 2.0) {
    REXLOG_INFO("GUEST TIMING: {:.1f} swaps/s, VdSwap {:.2f} ms/call, limiter {:.2f} ms/call, guest-thread CPU {:.2f} ms/frame",
                guest_swaps / seconds, vd_swap_ms / guest_swaps, limiter_ms / guest_swaps,
                thread_cpu_ms / guest_swaps);
    tabletennis::cpu_profile::Flush(guest_swaps / seconds, thread_cpu_ms / guest_swaps);
    guest_swaps = 0;
    vd_swap_ms = limiter_ms = thread_cpu_ms = 0.0;
    timing_window = swap_end;
  }
}

extern "C" void __imp__Tabletennis_VdSwap_Hook(PPCContext& ctx, uint8_t* base) {
  Tabletennis_VdSwap_Hook(ctx, base);
}

#if defined(__linux__)
extern "C" __attribute__((visibility("default"))) void Tabletennis_ProfileGraphics(unsigned event) {
  static const bool enabled = [] {
    const char* value = std::getenv("TT_CPU_PROFILE");
    return value && std::strcmp(value, "1") == 0;
  }();
  if (!enabled) return;
  using clock = std::chrono::steady_clock;
  struct ProfileWindow {
    clock::time_point window = clock::now();
    clock::time_point starts[3]{};
    uint64_t elapsed_ns[3]{};
    uint32_t counts[3]{};
  };
  static thread_local ProfileWindow profile;
  tabletennis::cpu_profile::Start("graphics");
  if (event == 0) {
    const auto now = clock::now();
    if (now - profile.window >= std::chrono::seconds(2)) {
      tabletennis::cpu_profile::Flush(-1.0, -1.0);
      for (unsigned i = 0; i < 3; ++i) {
        tabletennis::cpu_profile::Metric(i + 1, profile.counts[i],
            static_cast<double>(profile.elapsed_ns[i]) / 1.0e6);
        profile.counts[i] = 0;
        profile.elapsed_ns[i] = 0;
      }
      profile.window = now;
    }
  } else if (event <= 6) {
    const unsigned index = (event - 1) / 2;
    if ((event & 1) != 0) {
      profile.starts[index] = clock::now();
    } else {
      profile.elapsed_ns[index] += static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - profile.starts[index]).count());
      ++profile.counts[index];
    }
  }
}
#endif

// Returns SignedInLocally for user 0, allowing offline profiles/tournaments, but NOT Xbox Live.
extern "C" void Tabletennis_XamUserGetSigninState_Hook(PPCContext& ctx, uint8_t* base) {
  uint32_t user_index = ctx.r3.u32;
  if (user_index == 0) {
    ctx.r3.u64 = 1; // eXUserSigninState_SignedInLocally
  } else {
    ctx.r3.u64 = 0; // eXUserSigninState_NotSignedIn
  }
}

extern "C" void __imp__Tabletennis_XamUserGetSigninState_Hook(PPCContext& ctx, uint8_t* base) {
  Tabletennis_XamUserGetSigninState_Hook(ctx, base);
}

// Denies Xbox Live multiplayer privileges (XPRIVILEGE_MULTIPLAYER_SESSIONS),
// cleanly graying out and disabling the "Xbox Live" option in the main menu.
extern "C" void Tabletennis_XamUserCheckPrivilege_Hook(PPCContext& ctx, uint8_t* base) {
  uint32_t result_ptr = ctx.r5.u32;
  if (result_ptr != 0) {
    REX_STORE_U32(result_ptr, 0); // 0 = Denied / Disabled
  }
  ctx.r3.u64 = 0;
}

extern "C" void __imp__Tabletennis_XamUserCheckPrivilege_Hook(PPCContext& ctx, uint8_t* base) {
  Tabletennis_XamUserCheckPrivilege_Hook(ctx, base);
}

// Blocks any attempts to open the sign-in / Xbox Live UI, preventing infinite hangs.
extern "C" void Tabletennis_XamShowSigninUI_Hook(PPCContext& ctx, uint8_t* base) {
  REXLOG_INFO("Tabletennis: Blocked XamShowSigninUI (Xbox Live is disabled in offline PC port).");
  ctx.r3.u64 = 1223; // ERROR_CANCELLED
}

extern "C" void __imp__Tabletennis_XamShowSigninUI_Hook(PPCContext& ctx, uint8_t* base) {
  Tabletennis_XamShowSigninUI_Hook(ctx, base);
}

namespace {
struct EarlyInit {
  EarlyInit() {
    rex::cvar::SetFlagByName("protect_zero", "false");
    rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");
  }
} g_early_init;
}  // namespace

REX_DEFINE_APP(tabletennis, TabletennisApp::Create)
