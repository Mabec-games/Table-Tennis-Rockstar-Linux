// Compile with project/src/linux_memory_query.cpp and the pinned Linux SDK.
// Compare with the SDK implementation, including changes after opening maps.
#include <rex/memory/utils.h>

#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using rex::memory::PageAccess;
using Query = bool (*)(void*, size_t&, PageAccess&);
static Query original;
static size_t page;
static volatile sig_atomic_t faults = 0;

static void Check(bool okay) {
  if (!okay) {
    std::fprintf(stderr, "memory query test failed\n");
    std::abort();
  }
}

static void Compare(void* address) {
  size_t fast_length = 123, old_length = 123;
  PageAccess fast_access = PageAccess::kReadWrite, old_access = fast_access;
  bool fast_ok = rex::memory::QueryProtect(address, fast_length, fast_access);
  bool old_ok = original(address, old_length, old_access);
  Check(fast_ok == old_ok && fast_length == old_length && fast_access == old_access);
}

static void WriteFault(int, siginfo_t* info, void*) {
  size_t length = page;
  PageAccess access;
  if (!rex::memory::QueryProtect(info->si_addr, length, access) ||
      access != PageAccess::kReadOnly) _exit(91);
  auto* base = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(info->si_addr) & ~(page - 1));
  if (mprotect(base, page, PROT_READ | PROT_WRITE)) _exit(92);
  faults = faults + 1;
}

int main() {
  page = rex::memory::page_size(); // Also retain the SDK dependency at link time.
  void* runtime = dlopen("librexruntime.so", RTLD_NOW | RTLD_NOLOAD);
  Check(runtime != nullptr);
  original = reinterpret_cast<Query>(dlsym(runtime,
      "_ZN3rex6memory12QueryProtectEPvRmRNS0_10PageAccessE"));
  Check(original && original != &rex::memory::QueryProtect);

  auto* memory = static_cast<char*>(mmap(nullptr, page * 4, PROT_NONE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  Check(memory != MAP_FAILED);
  // All permission combinations, including non-readable executable/writable
  // mappings; querying never accesses the bytes themselves.
  for (int permissions = 0; permissions < 8; ++permissions) {
    Check(mprotect(memory + page, page, permissions) == 0);
    for (auto offset : {size_t(0), page - 1, page, page + 17, 2 * page - 1, 2 * page}) {
      Compare(memory + offset);
    }
  }
  Check(munmap(memory + page, page) == 0);
  Compare(memory + page); // Hole: false, length zero, kNoAccess.
  Check(mmap(memory + page, page, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
             -1, 0) == memory + page);
  Compare(memory + page); // Same address, newly mapped permissions.
  Compare(nullptr);

  // Exercise the query inside a real access-violation handler, followed by the
  // instruction retry, repeatedly changing permissions on an existing VMA.
  struct sigaction action{}, previous{};
  action.sa_sigaction = WriteFault;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  Check(sigaction(SIGSEGV, &action, &previous) == 0);
  for (int i = 0; i < 1000; ++i) {
    Check(mprotect(memory + page, page, PROT_READ) == 0);
    *reinterpret_cast<volatile char*>(memory + page) = static_cast<char>(i);
  }
  Check(faults == 1000);
  Check(sigaction(SIGSEGV, &previous, nullptr) == 0);

  // An inherited descriptor must not query the parent's mappings after fork.
  pid_t child = fork();
  Check(child >= 0);
  if (child == 0) {
    Check(mprotect(memory + page, page, PROT_NONE) == 0);
    Compare(memory + page);
    _exit(0);
  }
  int status;
  Check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  Compare(memory + page);
  Check(munmap(memory, page * 4) == 0);

  // Concurrent readers share the descriptor without seek offsets or a cache.
  std::array<std::thread, 4> threads;
  for (auto& thread : threads) {
    thread = std::thread([] {
      auto* region = mmap(nullptr, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      Check(region != MAP_FAILED);
      auto* middle = static_cast<char*>(region) + page;
      for (int i = 0; i < 500; ++i) {
        int permission = (i & 1) ? PROT_READ : PROT_READ | PROT_WRITE;
        Check(mprotect(middle, page, permission) == 0);
        size_t length = 0;
        PageAccess access;
        Check(rex::memory::QueryProtect(middle, length, access));
        Check(length == page && access == ((i & 1) ? PageAccess::kReadOnly : PageAccess::kReadWrite));
      }
      Check(munmap(region, page * 3) == 0);
    });
  }
  for (auto& thread : threads) thread.join();

  // Informational benchmark with fragmented permissions, as write watches
  // create. This is lookup timing, not a claim about game frame rate.
  constexpr int pages = 512, iterations = 2000;
  auto* fragmented = static_cast<char*>(mmap(nullptr, page * pages, PROT_NONE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  Check(fragmented != MAP_FAILED);
  for (int i = 1; i < pages; i += 2) Check(mprotect(fragmented + i * page, page, PROT_READ) == 0);
  auto bench = [&](Query query) {
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
      size_t length = 0;
      PageAccess access;
      Check(query(fragmented + (pages - 2) * page, length, access));
    }
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / iterations;
  };
  double old_us = bench(original), new_us = bench(&rex::memory::QueryProtect);
  std::printf("PASS: permissions, boundaries, remap, 1000 fault retries, fork, concurrent queries\n"
              "Lookup benchmark: SDK %.3f us; override %.3f us (%.1fx)\n", old_us, new_us, old_us / new_us);
  Check(munmap(fragmented, page * pages) == 0);
}
