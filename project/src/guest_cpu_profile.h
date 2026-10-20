#pragma once

// Optional Linux guest-thread sampling. Enabled only by TT_CPU_PROFILE=1.
// The signal handler records instruction addresses; file I/O happens at VdSwap.
#if defined(__linux__) && defined(__x86_64__)
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <pthread.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

namespace tabletennis::cpu_profile {
inline constexpr size_t capacity = 8192;
inline constexpr size_t caller_depth = 6;
static_assert(std::atomic<uintptr_t>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
struct State {
  std::array<std::atomic<uintptr_t>, capacity> samples{};
  std::array<std::array<std::atomic<uintptr_t>, caller_depth>, capacity> callers{};
  std::atomic<unsigned> count{0};
  timer_t timer{};
  FILE* output = nullptr;
  bool attempted = false;
  bool created = false;
  ~State() {
    if (created) {
      sigset_t block{};
      sigemptyset(&block);
      sigaddset(&block, SIGPROF);
      pthread_sigmask(SIG_BLOCK, &block, nullptr);
      timer_delete(timer);
    }
    if (output) std::fclose(output);
  }
};
inline thread_local State state;

inline void Sample(int, siginfo_t* info, void* context) {
  if (info->si_code != SI_TIMER || !info->si_value.sival_ptr) return;
  auto& recording = *static_cast<State*>(info->si_value.sival_ptr);
  const unsigned index = recording.count.load(std::memory_order_relaxed);
  if (index < capacity) {
    auto* state = static_cast<ucontext_t*>(context);
    const auto& registers = state->uc_mcontext.gregs;
    recording.samples[index].store(registers[REG_RIP], std::memory_order_relaxed);
    // The interrupted stack's top address identifies the immediate caller,
    // which is especially useful when a sample lands in a libc syscall stub.
    const auto* stack = reinterpret_cast<const uintptr_t*>(registers[REG_RSP]);
    for (size_t depth = 0; depth < caller_depth; ++depth) {
      recording.callers[index][depth].store(stack[depth], std::memory_order_relaxed);
    }
    recording.count.store(index + 1, std::memory_order_relaxed);
  }
}

inline void Start(const char* role = "guest") {
  if (state.attempted) return;
  state.attempted = true;
  const char* enabled = std::getenv("TT_CPU_PROFILE");
  if (!enabled || std::strcmp(enabled, "1") != 0) return;
  // Signal disposition is process-wide; each timer and buffer is thread-local.
  static const bool installed = [] {
    struct sigaction previous{}, action{};
    if (sigaction(SIGPROF, nullptr, &previous) != 0 || previous.sa_handler != SIG_DFL)
      return false;
    action.sa_sigaction = Sample;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    return sigaction(SIGPROF, &action, nullptr) == 0;
  }();
  if (!installed) {
    REXLOG_WARN("CPU PROFILE: SIGPROF unavailable; sampling skipped");
    return;
  }
  auto& output = state.output;
  const auto owner = static_cast<pid_t>(syscall(SYS_gettid));
  char filename[128];
  std::snprintf(filename, sizeof(filename), "logs/%s-cpu-%d-%d.txt", role, getpid(), owner);
  output = std::fopen(filename, "w");
  if (!output) {
    REXLOG_WARN("CPU PROFILE: cannot create {}", filename);
    return;
  }
  sigevent event{};
  event.sigev_notify = SIGEV_THREAD_ID;
  event.sigev_signo = SIGPROF;
  event.sigev_value.sival_ptr = &state;
  event._sigev_un._tid = owner;
  state.created = timer_create(CLOCK_THREAD_CPUTIME_ID, &event, &state.timer) == 0;
  // 200 samples per second of guest-thread CPU time; no wakeups while it sleeps.
  itimerspec interval{};
  interval.it_value.tv_nsec = interval.it_interval.tv_nsec = 5'000'000;
  if (!state.created || timer_settime(state.timer, 0, &interval, nullptr) != 0) {
    if (state.created) timer_delete(state.timer);
    state.created = false;
    std::fclose(output);
    output = nullptr;
    REXLOG_WARN("CPU PROFILE: could not start guest-thread sampling");
    return;
  }
  REXLOG_INFO("CPU PROFILE: recording guest-thread samples to {}", filename);
}

inline void Flush(double swaps_per_second, double cpu_ms) {
  auto& output = state.output;
  if (!output) return;
  sigset_t block{}, previous{};
  sigemptyset(&block);
  sigaddset(&block, SIGPROF);
  if (pthread_sigmask(SIG_BLOCK, &block, &previous) != 0) return;
  const unsigned size = state.count.load(std::memory_order_relaxed);
  std::array<uintptr_t, capacity> copy;
  std::array<std::array<uintptr_t, caller_depth>, capacity> callers;
  for (unsigned i = 0; i < size; ++i) {
    copy[i] = state.samples[i].load(std::memory_order_relaxed);
    for (size_t depth = 0; depth < caller_depth; ++depth) {
      callers[i][depth] = state.callers[i][depth].load(std::memory_order_relaxed);
    }
  }
  state.count.store(0, std::memory_order_relaxed);
  pthread_sigmask(SIG_SETMASK, &previous, nullptr);
  std::fprintf(output, "WINDOW %.3f %.3f %u\n", swaps_per_second, cpu_ms, size);
  for (unsigned i = 0; i < size; ++i) {
    std::fprintf(output, "PC %zx\n", size_t(copy[i]));
    for (size_t depth = 0; depth < caller_depth; ++depth) {
      std::fprintf(output, "RET%zu %zx\n", depth, size_t(callers[i][depth]));
    }
  }
  // Include mappings each time so modules loaded after startup can be resolved.
  if (FILE* maps = std::fopen("/proc/self/maps", "r")) {
    char line[4096];
    while (std::fgets(line, sizeof(line), maps)) std::fprintf(output, "MAP %s", line);
    std::fclose(maps);
  }
  std::fflush(output);
}
inline void Metric(unsigned event, unsigned count, double milliseconds) {
  if (state.output) {
    std::fprintf(state.output, "MEASURE %u %u %.3f\n", event, count, milliseconds);
    std::fflush(state.output);
  }
}
}  // namespace tabletennis::cpu_profile
#else
namespace tabletennis::cpu_profile {
inline void Start() {}
inline void Start(const char*) {}
inline void Flush(double, double) {}
inline void Metric(unsigned, unsigned, double) {}
}
#endif
