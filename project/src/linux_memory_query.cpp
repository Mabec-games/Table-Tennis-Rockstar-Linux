// Linux overrides for ReXGlue 0.10.0's exported rex::memory POSIX routines.
//
// Why this exists:
// 1. ReXGlue's MMIO write-watch handler calls rex::memory::QueryProtect inside
//    a SIGSEGV handler under global_critical_region_ on every GPU buffer/texture
//    write fault.
// 2. ReXGlue's memory_posix.cpp also parses /proc/self/maps via std::ifstream +
//    std::sscanf inside rex::memory::AllocFixed (IsRangeFullyMapped on every
//    guest heap commit) and rex::memory::Protect (when out_old_access != nullptr).
// 3. PROCMAP_QUERY ioctl only exists on Linux 6.11+, whereas SteamOS 3.5/3.6
//    (Steam Deck) and Ubuntu 24.04 GA run Linux 6.1-6.8.
//
// Solution:
// - Track all guest mappings created through rex::memory (MapFileView,
//   UnmapFileView, AllocFixed, DeallocFixed, Protect) in a lock-free,
//   zero-allocation 3-level radix page table in .bss so QueryProtect, Protect,
//   and AllocFixed resolve in O(1) user-space memory on all Linux kernels.
// - For untracked mappings (e.g., raw mmap in tests), use PROCMAP_QUERY when
//   supported by the kernel (Linux 6.11+) and fall back to a zero-allocation,
//   async-signal-safe stack-buffer /proc/self/maps parser on older kernels.
// - Setting TABLETENNIS_LEGACY_MEMORY_QUERY=1 restores the stock SDK behavior.

#include <rex/memory/utils.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

using rex::memory::AllocationType;
using rex::memory::DeallocationType;
using rex::memory::FileMappingHandle;
using rex::memory::PageAccess;

using QueryProtectFn = bool (*)(void*, size_t&, PageAccess&);
using ProtectFn = bool (*)(void*, size_t, PageAccess, PageAccess*);
using AllocFixedFn = void* (*)(void*, size_t, AllocationType, PageAccess);
using DeallocFixedFn = bool (*)(void*, size_t, DeallocationType);
using MapFileViewFn = void* (*)(FileMappingHandle, void*, size_t, PageAccess, size_t);
using UnmapFileViewFn = bool (*)(FileMappingHandle, void*, size_t);

constexpr char kQueryProtectSymbol[] =
    "_ZN3rex6memory12QueryProtectEPvRmRNS0_10PageAccessE";
constexpr char kProtectSymbol[] =
    "_ZN3rex6memory7ProtectEPvmNS0_10PageAccessEPS2_";
constexpr char kAllocFixedSymbol[] =
    "_ZN3rex6memory10AllocFixedEPvmNS0_14AllocationTypeENS0_10PageAccessE";
constexpr char kDeallocFixedSymbol[] =
    "_ZN3rex6memory12DeallocFixedEPvmNS0_16DeallocationTypeE";
constexpr char kMapFileViewSymbol[] =
    "_ZN3rex6memory11MapFileViewElPvmNS0_10PageAccessEm";
constexpr char kUnmapFileViewSymbol[] =
    "_ZN3rex6memory13UnmapFileViewElPvm";

QueryProtectFn original_query_protect = nullptr;
ProtectFn original_protect = nullptr;
AllocFixedFn original_alloc_fixed = nullptr;
DeallocFixedFn original_dealloc_fixed = nullptr;
MapFileViewFn original_map_file_view = nullptr;
UnmapFileViewFn original_unmap_file_view = nullptr;

int maps_fd = -1;
bool legacy_mode = false;
bool radix_enabled = false;
volatile bool in_fork_child = false;

// 4 KiB host page geometry on x86_64 Linux (47-bit user virtual address space).
constexpr size_t kPageShift = 12;
constexpr size_t kPageSize = 1ULL << kPageShift;
constexpr uintptr_t kMaxUserAddress = 1ULL << 47;
constexpr size_t kMaxVpn = kMaxUserAddress >> kPageShift;

// 3-level radix tree over 35-bit VPNs (47-bit address / 4 KiB page):
// - L1: 10 bits (1024 entries, 128 GiB each)
// - L2:  9 bits ( 512 entries, 256 MiB each)
// - L3: 16 bits (65536 entries,   4 KiB each)
constexpr size_t kL3Bits = 16;
constexpr size_t kL3Entries = 1ULL << kL3Bits;
constexpr size_t kL3Mask = kL3Entries - 1;

constexpr size_t kL2Bits = 9;
constexpr size_t kL2Entries = 1ULL << kL2Bits;
constexpr size_t kL2Mask = kL2Entries - 1;

constexpr size_t kL1Bits = 10;
constexpr size_t kL1Entries = 1ULL << kL1Bits;

// Per-page uint16_t entry encoding:
// - bits 0..2 (0x0007): PageAccess enum value (0, 1, 3, 5, 7)
// - bit  3    (0x0008): kTrackedBit (1 = page is tracked in radix table)
// - bits 4..15(0xFFF0): view_id (0 = anonymous, 1..4095 = file view identity)
constexpr uint16_t kAccessMask = 0x0007;
constexpr uint16_t kTrackedBit = 0x0008;
constexpr unsigned kViewShift = 4;
constexpr size_t kMaxViewIds = 1024;

struct L3Node {
  std::atomic<uint16_t> entries[kL3Entries];
};

struct L2Node {
  std::atomic<L3Node*> entries[kL2Entries];
};

constexpr size_t kMaxL2Nodes = 64;
constexpr size_t kMaxL3Nodes = 512;

// Static .bss pools: demand-paged by Linux so untouched nodes use 0 physical RAM,
// and never call malloc/free on any memory or signal-handler path.
std::atomic<L2Node*> l1_table[kL1Entries];
L2Node l2_pool[kMaxL2Nodes];
L3Node l3_pool[kMaxL3Nodes];
std::atomic<size_t> l2_pool_next{0};
std::atomic<size_t> l3_pool_next{0};

struct ViewIdentity {
  std::atomic<uint32_t> state{0};  // 0 = free, 1 = initializing, 2 = ready
  FileMappingHandle handle{rex::memory::kFileMappingHandleInvalid};
  uintptr_t base_minus_offset{0};
};

ViewIdentity view_identities[kMaxViewIds];

uint16_t GetOrAssignViewId(FileMappingHandle handle, uintptr_t base_minus_offset) {
  for (size_t i = 1; i < kMaxViewIds; ++i) {
    uint32_t state = view_identities[i].state.load(std::memory_order_acquire);
    if (state == 2) {
      if (view_identities[i].handle == handle &&
          view_identities[i].base_minus_offset == base_minus_offset) {
        return static_cast<uint16_t>(i);
      }
      continue;
    }
    if (state == 0) {
      uint32_t expected = 0;
      if (view_identities[i].state.compare_exchange_strong(
              expected, 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
        view_identities[i].handle = handle;
        view_identities[i].base_minus_offset = base_minus_offset;
        view_identities[i].state.store(2, std::memory_order_release);
        return static_cast<uint16_t>(i);
      }
    }
    while (view_identities[i].state.load(std::memory_order_acquire) == 1) {
      // Wait briefly for concurrent writer to publish handle & offset.
    }
    if (view_identities[i].handle == handle &&
        view_identities[i].base_minus_offset == base_minus_offset) {
      return static_cast<uint16_t>(i);
    }
  }
  return 1;
}

L3Node* GetL3(size_t vpn) noexcept {
  const size_t l1_idx = vpn >> (kL2Bits + kL3Bits);
  if (l1_idx >= kL1Entries) return nullptr;
  L2Node* l2 = l1_table[l1_idx].load(std::memory_order_acquire);
  if (!l2) return nullptr;
  const size_t l2_idx = (vpn >> kL3Bits) & kL2Mask;
  return l2->entries[l2_idx].load(std::memory_order_acquire);
}

L3Node* GetOrCreateL3(size_t vpn) noexcept {
  const size_t l1_idx = vpn >> (kL2Bits + kL3Bits);
  if (l1_idx >= kL1Entries) return nullptr;
  L2Node* l2 = l1_table[l1_idx].load(std::memory_order_acquire);
  if (!l2) {
    const size_t idx = l2_pool_next.fetch_add(1, std::memory_order_relaxed);
    if (idx >= kMaxL2Nodes) return nullptr;
    L2Node* candidate = &l2_pool[idx];
    L2Node* expected = nullptr;
    if (l1_table[l1_idx].compare_exchange_strong(
            expected, candidate, std::memory_order_release, std::memory_order_acquire)) {
      l2 = candidate;
    } else {
      l2 = expected;
    }
  }
  const size_t l2_idx = (vpn >> kL3Bits) & kL2Mask;
  L3Node* l3 = l2->entries[l2_idx].load(std::memory_order_acquire);
  if (!l3) {
    const size_t idx = l3_pool_next.fetch_add(1, std::memory_order_relaxed);
    if (idx >= kMaxL3Nodes) return nullptr;
    L3Node* candidate = &l3_pool[idx];
    L3Node* expected = nullptr;
    if (l2->entries[l2_idx].compare_exchange_strong(
            expected, candidate, std::memory_order_release, std::memory_order_acquire)) {
      l3 = candidate;
    } else {
      l3 = expected;
    }
  }
  return l3;
}

uint16_t GetPageEntry(size_t vpn) noexcept {
  L3Node* l3 = GetL3(vpn);
  if (!l3) return 0;
  return l3->entries[vpn & kL3Mask].load(std::memory_order_acquire);
}

void TrackNewRange(uintptr_t base, size_t length, PageAccess access, uint16_t view_id) noexcept {
  if (!radix_enabled || in_fork_child || length == 0) return;
  const uintptr_t end = base + ((length + kPageSize - 1) & ~(kPageSize - 1));
  if (end <= base || end > kMaxUserAddress) return;

  const uint16_t entry = static_cast<uint16_t>(
      (view_id << kViewShift) | kTrackedBit | (static_cast<uint16_t>(access) & kAccessMask));

  size_t vpn = base >> kPageShift;
  const size_t end_vpn = end >> kPageShift;
  while (vpn < end_vpn) {
    L3Node* l3 = GetOrCreateL3(vpn);
    if (!l3) return;
    const size_t l3_start = vpn & kL3Mask;
    const size_t count = (end_vpn - vpn < kL3Entries - l3_start)
                             ? (end_vpn - vpn)
                             : (kL3Entries - l3_start);
    for (size_t i = 0; i < count; ++i) {
      l3->entries[l3_start + i].store(entry, std::memory_order_release);
    }
    vpn += count;
  }
}

void UpdateTrackedRangeAccess(uintptr_t base, size_t length, PageAccess access) noexcept {
  if (!radix_enabled || in_fork_child || length == 0) return;
  const uintptr_t end = base + ((length + kPageSize - 1) & ~(kPageSize - 1));
  if (end <= base || end > kMaxUserAddress) return;

  const uint16_t access_bits = static_cast<uint16_t>(access) & kAccessMask;
  size_t vpn = base >> kPageShift;
  const size_t end_vpn = end >> kPageShift;
  while (vpn < end_vpn) {
    L3Node* l3 = GetL3(vpn);
    const size_t l3_start = vpn & kL3Mask;
    const size_t count = (end_vpn - vpn < kL3Entries - l3_start)
                             ? (end_vpn - vpn)
                             : (kL3Entries - l3_start);
    if (l3) {
      for (size_t i = 0; i < count; ++i) {
        const uint16_t old_entry = l3->entries[l3_start + i].load(std::memory_order_relaxed);
        if (old_entry & kTrackedBit) {
          const uint16_t new_entry = static_cast<uint16_t>((old_entry & ~kAccessMask) | access_bits);
          l3->entries[l3_start + i].store(new_entry, std::memory_order_release);
        }
      }
    }
    vpn += count;
  }
}

void UntrackRange(uintptr_t base, size_t length) noexcept {
  if (!radix_enabled || in_fork_child || length == 0) return;
  const uintptr_t end = base + ((length + kPageSize - 1) & ~(kPageSize - 1));
  if (end <= base || end > kMaxUserAddress) return;

  size_t vpn = base >> kPageShift;
  const size_t end_vpn = end >> kPageShift;
  while (vpn < end_vpn) {
    L3Node* l3 = GetL3(vpn);
    const size_t l3_start = vpn & kL3Mask;
    const size_t count = (end_vpn - vpn < kL3Entries - l3_start)
                             ? (end_vpn - vpn)
                             : (kL3Entries - l3_start);
    if (l3) {
      for (size_t i = 0; i < count; ++i) {
        l3->entries[l3_start + i].store(0, std::memory_order_release);
      }
    }
    vpn += count;
  }
}

bool IsRangeTracked(uintptr_t base, size_t length) noexcept {
  if (!radix_enabled || in_fork_child || length == 0 || (base & (kPageSize - 1)) != 0) {
    return false;
  }
  const uintptr_t end = base + ((length + kPageSize - 1) & ~(kPageSize - 1));
  if (end <= base || end > kMaxUserAddress) return false;

  size_t vpn = base >> kPageShift;
  const size_t end_vpn = end >> kPageShift;
  while (vpn < end_vpn) {
    L3Node* l3 = GetL3(vpn);
    if (!l3) return false;
    const size_t l3_start = vpn & kL3Mask;
    const size_t count = (end_vpn - vpn < kL3Entries - l3_start)
                             ? (end_vpn - vpn)
                             : (kL3Entries - l3_start);
    for (size_t i = 0; i < count; ++i) {
      if ((l3->entries[l3_start + i].load(std::memory_order_acquire) & kTrackedBit) == 0) {
        return false;
      }
    }
    vpn += count;
  }
  return true;
}

bool QueryTrackedRange(void* base_address, size_t& length, PageAccess& access_out) noexcept {
  if (!radix_enabled || in_fork_child) return false;
  const uintptr_t addr = reinterpret_cast<uintptr_t>(base_address);
  if (addr >= kMaxUserAddress) return false;

  const size_t start_vpn = addr >> kPageShift;
  const uint16_t target_entry = GetPageEntry(start_vpn);
  if ((target_entry & kTrackedBit) == 0) return false;

  access_out = static_cast<PageAccess>(target_entry & kAccessMask);

  size_t vpn = start_vpn + 1;
  while (vpn < kMaxVpn) {
    L3Node* l3 = GetL3(vpn);
    if (!l3) break;
    size_t l3_idx = vpn & kL3Mask;
    bool stopped = false;
    for (; l3_idx < kL3Entries; ++l3_idx, ++vpn) {
      if (l3->entries[l3_idx].load(std::memory_order_relaxed) != target_entry) {
        stopped = true;
        break;
      }
    }
    if (stopped) break;
  }

  const uintptr_t vma_end = vpn << kPageShift;
  length = static_cast<size_t>(vma_end - addr);
  return true;
}

uint32_t ToPosixProtectFlagsLocal(PageAccess access) noexcept {
  switch (access) {
    case PageAccess::kNoAccess:
      return PROT_NONE;
    case PageAccess::kReadOnly:
      return PROT_READ;
    case PageAccess::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageAccess::kExecuteReadOnly:
      return PROT_READ | PROT_EXEC;
    case PageAccess::kExecuteReadWrite:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
      return PROT_NONE;
  }
}

PageAccess PermsToPageAccessLocal(const char perms[4]) noexcept {
  const bool r = perms[0] == 'r';
  const bool w = perms[1] == 'w';
  const bool x = perms[2] == 'x';
  if (!r && !w && !x) return PageAccess::kNoAccess;
  if (x) return w ? PageAccess::kExecuteReadWrite : PageAccess::kExecuteReadOnly;
  return w ? PageAccess::kReadWrite : PageAccess::kReadOnly;
}

int HexDigitValue(char c) noexcept {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool ParseProcMapsPrefix(const char* line, size_t len, uintptr_t& start_out, uintptr_t& end_out,
                         char perms_out[4]) noexcept {
  size_t i = 0;
  uintptr_t start = 0;
  int digits = 0;
  while (i < len) {
    const int d = HexDigitValue(line[i]);
    if (d < 0) break;
    start = (start << 4) | static_cast<uintptr_t>(d);
    ++i;
    ++digits;
  }
  if (digits == 0 || i >= len || line[i] != '-') return false;
  ++i;

  uintptr_t end = 0;
  digits = 0;
  while (i < len) {
    const int d = HexDigitValue(line[i]);
    if (d < 0) break;
    end = (end << 4) | static_cast<uintptr_t>(d);
    ++i;
    ++digits;
  }
  if (digits == 0 || i + 5 > len || line[i] != ' ') return false;
  ++i;

  perms_out[0] = line[i];
  perms_out[1] = line[i + 1];
  perms_out[2] = line[i + 2];
  perms_out[3] = line[i + 3];
  start_out = start;
  end_out = end;
  return start < end;
}

// Fast zero-allocation, async-signal-safe /proc/self/maps lookup for untracked
// addresses on Linux kernels older than 6.11 (where PROCMAP_QUERY is absent).
bool QueryProcMapsFast(void* base_address, size_t& length, PageAccess& access_out) noexcept {
  access_out = PageAccess::kNoAccess;
  length = 0;

  const uintptr_t addr = reinterpret_cast<uintptr_t>(base_address);
  const int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;

  char buf[8192];
  char prefix[64];
  size_t prefix_len = 0;
  bool in_tail = false;

  while (true) {
    ssize_t n;
    do {
      n = read(fd, buf, sizeof(buf));
    } while (n < 0 && errno == EINTR);
    if (n <= 0) break;

    for (ssize_t i = 0; i < n; ++i) {
      const char c = buf[i];
      if (c != '\n') {
        if (!in_tail) {
          if (prefix_len < sizeof(prefix) - 1) {
            prefix[prefix_len++] = c;
          } else {
            in_tail = true;
          }
        }
        continue;
      }

      uintptr_t start = 0;
      uintptr_t end = 0;
      char perms[4];
      if (ParseProcMapsPrefix(prefix, prefix_len, start, end, perms)) {
        if (addr >= start && addr < end) {
          close(fd);
          length = static_cast<size_t>(end - addr);
          access_out = PermsToPageAccessLocal(perms);
          return true;
        }
        if (start > addr) {
          // /proc/self/maps is sorted by ascending VMA start address.
          close(fd);
          return false;
        }
      }
      prefix_len = 0;
      in_tail = false;
    }
  }

  close(fd);
  return false;
}

bool IsRangeFullyMappedUntracked(void* base_address, size_t length) noexcept {
  if (!base_address || length == 0) return false;
  const uintptr_t begin = reinterpret_cast<uintptr_t>(base_address);
  const uintptr_t end = begin + length;
  if (end < begin) return false;

#ifdef PROCMAP_QUERY
  if (maps_fd >= 0) {
    uintptr_t cursor = begin;
    while (cursor < end) {
      procmap_query query{};
      query.size = sizeof(query);
      query.query_addr = cursor;
      int result;
      do {
        result = ioctl(maps_fd, PROCMAP_QUERY, &query);
      } while (result < 0 && errno == EINTR);
      if (result != 0 || query.vma_start > cursor || query.vma_end <= cursor) {
        return false;
      }
      cursor = static_cast<uintptr_t>(query.vma_end);
    }
    return true;
  }
#endif

  const int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;

  uintptr_t cursor = begin;
  char buf[8192];
  char prefix[64];
  size_t prefix_len = 0;
  bool in_tail = false;

  while (true) {
    ssize_t n;
    do {
      n = read(fd, buf, sizeof(buf));
    } while (n < 0 && errno == EINTR);
    if (n <= 0) break;

    for (ssize_t i = 0; i < n; ++i) {
      const char c = buf[i];
      if (c != '\n') {
        if (!in_tail) {
          if (prefix_len < sizeof(prefix) - 1) {
            prefix[prefix_len++] = c;
          } else {
            in_tail = true;
          }
        }
        continue;
      }

      uintptr_t vma_start = 0;
      uintptr_t vma_end = 0;
      char perms[4];
      if (ParseProcMapsPrefix(prefix, prefix_len, vma_start, vma_end, perms)) {
        if (vma_end > cursor) {
          if (vma_start > cursor) {
            close(fd);
            return false;
          }
          cursor = vma_end;
          if (cursor >= end) {
            close(fd);
            return true;
          }
        }
      }
      prefix_len = 0;
      in_tail = false;
    }
  }

  close(fd);
  return cursor >= end;
}

void AfterForkChild() {
  // A maps descriptor and parent user-space page table refer to the parent
  // process. A forked child must query its own /proc/self/maps directly.
  if (maps_fd >= 0) close(maps_fd);
  maps_fd = -1;
  in_fork_child = true;
}

template <typename Fn>
Fn ResolveSymbol(const char* name, Fn self_fn) {
  Fn resolved = reinterpret_cast<Fn>(dlsym(RTLD_NEXT, name));
  if (!resolved || resolved == self_fn) {
    constexpr char error[] = "Cannot resolve ReXGlue rex::memory fallback symbol\n";
    (void)write(STDERR_FILENO, error, sizeof(error) - 1);
    std::abort();
  }
  return resolved;
}

bool EnvFlagEnabled(const char* name) {
  const char* val = std::getenv(name);
  return val && val[0] == '1' && val[1] == '\0';
}

struct InitializeQuery {
  InitializeQuery() {
    // Resolve once at startup, before guest threads or signal handlers exist.
    original_query_protect = ResolveSymbol(kQueryProtectSymbol, &rex::memory::QueryProtect);
    original_protect = ResolveSymbol(kProtectSymbol, &rex::memory::Protect);
    original_alloc_fixed = ResolveSymbol(kAllocFixedSymbol, &rex::memory::AllocFixed);
    original_dealloc_fixed = ResolveSymbol(kDeallocFixedSymbol, &rex::memory::DeallocFixed);
    original_map_file_view = ResolveSymbol(kMapFileViewSymbol, &rex::memory::MapFileView);
    original_unmap_file_view = ResolveSymbol(kUnmapFileViewSymbol, &rex::memory::UnmapFileView);

    if (EnvFlagEnabled("TABLETENNIS_LEGACY_MEMORY_QUERY")) {
      legacy_mode = true;
      return;
    }

    (void)pthread_atfork(nullptr, nullptr, AfterForkChild);

    if (static_cast<size_t>(getpagesize()) == kPageSize) {
      radix_enabled = true;
    }

#ifdef PROCMAP_QUERY
    if (EnvFlagEnabled("TABLETENNIS_DISABLE_PROCMAP_QUERY")) return;
    const int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;
    procmap_query query{};
    query.size = sizeof(query);
    query.query_addr = reinterpret_cast<uintptr_t>(&maps_fd);
    if (ioctl(fd, PROCMAP_QUERY, &query) != 0) {
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

void* MapFileView(FileMappingHandle handle, void* base_address, size_t length, PageAccess access,
                  size_t file_offset) {
  void* result = original_map_file_view(handle, base_address, length, access, file_offset);
  if (result && !legacy_mode && !in_fork_child) {
    const uintptr_t base = reinterpret_cast<uintptr_t>(result);
    const uint16_t view_id = GetOrAssignViewId(handle, base - file_offset);
    TrackNewRange(base, length, access, view_id);
  }
  return result;
}

bool UnmapFileView(FileMappingHandle handle, void* base_address, size_t length) {
  const bool ok = original_unmap_file_view(handle, base_address, length);
  if (ok && !legacy_mode && !in_fork_child) {
    UntrackRange(reinterpret_cast<uintptr_t>(base_address), length);
  }
  return ok;
}

void* AllocFixed(void* base_address, size_t length, AllocationType allocation_type,
                 PageAccess access) {
  if (legacy_mode || in_fork_child) {
    return original_alloc_fixed(base_address, length, allocation_type, access);
  }

  const uintptr_t base = reinterpret_cast<uintptr_t>(base_address);
  const uint32_t prot_requested = ToPosixProtectFlagsLocal(access);

  // Fast path for committing pages inside an already-reserved guest arena
  // (e.g. MapFileView or prior AllocFixed(kReserve)): skip the failing
  // mmap(MAP_FIXED_NOREPLACE) syscall and the /proc/self/maps scan.
  if (base_address != nullptr &&
      (allocation_type == AllocationType::kCommit ||
       allocation_type == AllocationType::kReserveCommit) &&
      IsRangeTracked(base, length)) {
    if (mprotect(base_address, length, static_cast<int>(prot_requested)) == 0) {
      UpdateTrackedRangeAccess(base, length, access);
      return base_address;
    }
    return nullptr;
  }

  int prot_initial = static_cast<int>(prot_requested);
  PageAccess initial_access = access;
  if (allocation_type == AllocationType::kReserve) {
    prot_initial = PROT_NONE;
    initial_access = PageAccess::kNoAccess;
  }

  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(MAP_FIXED_NOREPLACE)
  if (base_address) {
    flags |= MAP_FIXED_NOREPLACE;
  }
#else
  if (base_address) {
    flags |= MAP_FIXED;
  }
#endif

  void* result = mmap(base_address, length, prot_initial, flags, -1, 0);
  if (result != MAP_FAILED) {
    TrackNewRange(reinterpret_cast<uintptr_t>(result), length, initial_access, 0);
    return result;
  }

#if defined(MAP_FIXED_NOREPLACE)
  if (errno == EEXIST && base_address &&
      (allocation_type == AllocationType::kCommit ||
       allocation_type == AllocationType::kReserveCommit)) {
    if (IsRangeFullyMappedUntracked(base_address, length)) {
      if (mprotect(base_address, length, static_cast<int>(prot_requested)) == 0) {
        UpdateTrackedRangeAccess(base, length, access);
        return base_address;
      }
    }
  }
#endif

  return nullptr;
}

bool DeallocFixed(void* base_address, size_t length, DeallocationType deallocation_type) {
  const bool ok = original_dealloc_fixed(base_address, length, deallocation_type);
  if (ok && !legacy_mode && !in_fork_child) {
    const uintptr_t base = reinterpret_cast<uintptr_t>(base_address);
    if (deallocation_type == DeallocationType::kDecommit) {
      UpdateTrackedRangeAccess(base, length, PageAccess::kNoAccess);
    } else if (deallocation_type == DeallocationType::kRelease) {
      UntrackRange(base, length);
    }
  }
  return ok;
}

bool Protect(void* base_address, size_t length, PageAccess access, PageAccess* out_old_access) {
  if (legacy_mode || in_fork_child) {
    return original_protect(base_address, length, access, out_old_access);
  }

  const uintptr_t base = reinterpret_cast<uintptr_t>(base_address);
  if (out_old_access) {
    *out_old_access = PageAccess::kNoAccess;
    const uint16_t entry = (radix_enabled && base < kMaxUserAddress)
                               ? GetPageEntry(base >> kPageShift)
                               : 0;
    if (entry & kTrackedBit) {
      *out_old_access = static_cast<PageAccess>(entry & kAccessMask);
    } else {
      size_t ignored_length = 0;
      (void)QueryProtect(base_address, ignored_length, *out_old_access);
    }
  }

  const uint32_t prot = ToPosixProtectFlagsLocal(access);
  if (mprotect(base_address, length, static_cast<int>(prot)) != 0) {
    return false;
  }

  UpdateTrackedRangeAccess(base, length, access);
  return true;
}

bool QueryProtect(void* base_address, size_t& length, PageAccess& access_out) {
  if (legacy_mode) {
    return original_query_protect(base_address, length, access_out);
  }

  // 1. O(1) user-space radix page table lookup for all rex::memory mappings.
  if (QueryTrackedRange(base_address, length, access_out)) {
    return true;
  }

  // 2. Kernel PROCMAP_QUERY ioctl for untracked mappings on Linux 6.11+.
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
    if (errno == ENOENT) {
      access_out = PageAccess::kNoAccess;
      length = 0;
      return false;
    }
  }
#endif

  // 3. Zero-allocation stack-buffer /proc/self/maps lookup for untracked
  //    mappings on Linux kernels < 6.11 (including SteamOS 3.5/3.6).
  return QueryProcMapsFast(base_address, length, access_out);
}

}  // namespace rex::memory
