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

using rex::memory::AllocationType;
using rex::memory::DeallocationType;
using rex::memory::FileMappingHandle;
using rex::memory::PageAccess;
using Query = bool (*)(void*, size_t&, PageAccess&);
static Query original;
static size_t page;
static volatile sig_atomic_t faults = 0;
static volatile bool fault_use_rex_protect = false;

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
  if (fault_use_rex_protect) {
    PageAccess old_access = PageAccess::kNoAccess;
    if (!rex::memory::Protect(base, page, PageAccess::kReadWrite, &old_access) ||
        old_access != PageAccess::kReadOnly) _exit(93);
  } else {
    if (mprotect(base, page, PROT_READ | PROT_WRITE)) _exit(92);
  }
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

  // Exercise tracked rex::memory mappings (MapFileView, AllocFixed, Protect,
  // DeallocFixed, UnmapFileView) so kernels without PROCMAP_QUERY (e.g.
  // SteamOS 3.5/3.6 on Linux 6.1/6.5) resolve guest memory in user space.
  std::string shm_name = "tt_mem_query_test_" + std::to_string(getpid());
  FileMappingHandle mapping = rex::memory::CreateFileMappingHandle(
      shm_name, page * 16, PageAccess::kReadWrite, false);
  Check(mapping != rex::memory::kFileMappingHandleInvalid);

  auto* arena_hole = static_cast<char*>(mmap(nullptr, page * 12, PROT_NONE,
                                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  Check(arena_hole != MAP_FAILED);
  Check(munmap(arena_hole, page * 12) == 0);
  char* arena = arena_hole + page; // Leave unmapped guard holes on both sides.

  // Contiguous file views (view0 at offset 0, view1 at offset 4*page) merge in
  // the kernel; view2 at non-contiguous offset 8*page stays a separate VMA.
  Check(rex::memory::MapFileView(mapping, arena, page * 4, PageAccess::kReadWrite, 0) == arena);
  Check(rex::memory::MapFileView(mapping, arena + page * 4, page * 4, PageAccess::kReadWrite,
                                 page * 4) == arena + page * 4);
  Check(rex::memory::MapFileView(mapping, arena + page * 8, page * 2, PageAccess::kReadWrite,
                                 page * 2) == arena + page * 8);
  for (size_t idx = 0; idx < 10; ++idx) {
    Compare(arena + idx * page);
    Compare(arena + idx * page + 31);
  }

  // Commit and protect sub-ranges through rex::memory APIs.
  Check(rex::memory::AllocFixed(arena + page * 2, page * 2, AllocationType::kCommit,
                                PageAccess::kReadOnly) == arena + page * 2);
  for (size_t idx = 0; idx < 10; ++idx) Compare(arena + idx * page);

  PageAccess old_access = PageAccess::kNoAccess;
  Check(rex::memory::Protect(arena + page * 2, page, PageAccess::kExecuteReadOnly, &old_access));
  Check(old_access == PageAccess::kReadOnly);
  for (size_t idx = 0; idx < 10; ++idx) Compare(arena + idx * page);

  Check(rex::memory::DeallocFixed(arena + page * 3, page, DeallocationType::kDecommit));
  for (size_t idx = 0; idx < 10; ++idx) Compare(arena + idx * page);

  // Restore uniform kReadWrite across view0+view1 and verify VMA re-merge.
  Check(rex::memory::Protect(arena + page * 2, page * 2, PageAccess::kReadWrite, &old_access));
  Check(old_access == PageAccess::kExecuteReadOnly);
  for (size_t idx = 0; idx < 10; ++idx) Compare(arena + idx * page);

  // Exercise 1000 SIGSEGV write-watch faults on tracked guest memory.
  faults = 0;
  fault_use_rex_protect = true;
  Check(sigaction(SIGSEGV, &action, &previous) == 0);
  for (int i = 0; i < 1000; ++i) {
    Check(rex::memory::Protect(arena + page * 5, page, PageAccess::kReadOnly, nullptr));
    *reinterpret_cast<volatile char*>(arena + page * 5) = static_cast<char>(i);
  }
  Check(faults == 1000);
  Check(sigaction(SIGSEGV, &previous, nullptr) == 0);
  fault_use_rex_protect = false;
  Compare(arena + page * 5);

  // Benchmark tracked guest memory lookup with fragmented write-watch pages.
  for (size_t idx = 0; idx < 8; idx += 2) {
    Check(rex::memory::Protect(arena + idx * page, page, PageAccess::kReadOnly, nullptr));
  }
  constexpr int tracked_iters = 10000;
  auto tracked_start = std::chrono::steady_clock::now();
  for (int i = 0; i < tracked_iters; ++i) {
    size_t len = 0;
    PageAccess acc;
    Check(rex::memory::QueryProtect(arena + page * 6, len, acc));
    Check(len == page && acc == PageAccess::kReadOnly);
  }
  double tracked_us = std::chrono::duration<double, std::micro>(
      std::chrono::steady_clock::now() - tracked_start).count() / tracked_iters;

  Check(rex::memory::UnmapFileView(mapping, arena, page * 10));
  Compare(arena);
  rex::memory::CloseFileMappingHandle(mapping, shm_name);

  // An inherited descriptor or radix table must not report stale parent
  // permissions after fork.
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
  std::printf("PASS: permissions, boundaries, remap, 1000 fault retries, tracked arena, fork, concurrent queries\n"
              "Lookup benchmark: SDK %.3f us; untracked override %.3f us (%.1fx); tracked guest %.3f us (%.1fx)\n",
              old_us, new_us, old_us / new_us, tracked_us, old_us / tracked_us);
  Check(munmap(fragmented, page * pages) == 0);
}
