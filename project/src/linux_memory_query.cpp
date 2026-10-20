// Linux override for ReXGlue 0.10.0's exported memory::QueryProtect.
// Its MMIO fault handler calls this for every GPU write-watch fault. Scanning
// /proc/self/maps there is expensive when the guest has many mappings.
// PROCMAP_QUERY asks the kernel for the same current VMA without parsing text.
// No permission cache: mprotect, unmap/remap, and racing watch removal remain
// visible to the existing runtime handler under its existing lock.

#include <rex/memory/utils.h>

#include <cerrno>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
using QueryFunction = bool (*)(void*, size_t&, rex::memory::PageAccess&);
constexpr char kQuerySymbol[] =
    "_ZN3rex6memory12QueryProtectEPvRmRNS0_10PageAccessE";

QueryFunction original_query = nullptr;
int maps_fd = -1;

void AfterForkChild() {
  // A maps descriptor refers to the process that opened it. A child must use
  // the original /proc/self/maps implementation until it execs a new image.
  if (maps_fd >= 0) close(maps_fd);
  maps_fd = -1;
}

struct InitializeQuery {
  InitializeQuery() {
    // Resolve once at startup, before guest threads or signal handlers exist.
    original_query = reinterpret_cast<QueryFunction>(dlsym(RTLD_NEXT, kQuerySymbol));
    if (!original_query || original_query == &rex::memory::QueryProtect) {
      constexpr char error[] = "Cannot resolve ReXGlue memory::QueryProtect fallback\n";
      (void)write(STDERR_FILENO, error, sizeof(error) - 1);
      std::abort();
    }
#ifdef PROCMAP_QUERY
    const char* legacy = std::getenv("TABLETENNIS_LEGACY_MEMORY_QUERY");
    if (legacy && legacy[0] == '1' && legacy[1] == '\0') return;
    const int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;
    procmap_query query{};
    query.size = sizeof(query);
    query.query_addr = reinterpret_cast<uintptr_t>(&maps_fd);
    // Unsupported kernels or blocked ioctls keep the stock implementation.
    if (ioctl(fd, PROCMAP_QUERY, &query) != 0 ||
        pthread_atfork(nullptr, nullptr, AfterForkChild) != 0) {
      close(fd);
      return;
    }
    maps_fd = fd;
    // Keep the descriptor until process exit, including shutdown callbacks.
#endif
  }
} initialize_query;
}  // namespace

namespace rex::memory {
bool QueryProtect(void* base_address, size_t& length, PageAccess& access_out) {
#ifdef PROCMAP_QUERY
  if (maps_fd >= 0) {
    procmap_query query{};
    query.size = sizeof(query);
    query.query_addr = reinterpret_cast<uintptr_t>(base_address);
    int result;
    do {
      result = ioctl(maps_fd, PROCMAP_QUERY, &query);
    } while (result < 0 && errno == EINTR);
    if (result == 0) {
      length = static_cast<size_t>(query.vma_end - query.query_addr);
      // Match the SDK's PermsToPageAccess, including write/execute-only VMAs.
      const auto permissions = query.vma_flags &
          (PROCMAP_QUERY_VMA_READABLE | PROCMAP_QUERY_VMA_WRITABLE |
           PROCMAP_QUERY_VMA_EXECUTABLE);
      if (!permissions) {
        access_out = PageAccess::kNoAccess;
      } else if (permissions & PROCMAP_QUERY_VMA_EXECUTABLE) {
        access_out = (permissions & PROCMAP_QUERY_VMA_WRITABLE)
                         ? PageAccess::kExecuteReadWrite : PageAccess::kExecuteReadOnly;
      } else {
        access_out = (permissions & PROCMAP_QUERY_VMA_WRITABLE)
                         ? PageAccess::kReadWrite : PageAccess::kReadOnly;
      }
      return true;
    }
    // Any per-call failure, including ENOENT, uses the original authoritative
    // lookup. This preserves the SDK's exact behavior if the kernel query and
    // the maps parser disagree about a transient or aliased guest-memory VMA.
  }
#endif
  return original_query(base_address, length, access_out);
}
}  // namespace rex::memory
