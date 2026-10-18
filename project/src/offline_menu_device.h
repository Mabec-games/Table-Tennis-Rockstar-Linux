#pragma once

#include <rex/filesystem/devices/host_path_device.h>
#include <rex/logging.h>

// A separate mount is essential: VFS picks the first matching device, so a
// HostPathDevice nested below the existing disc mount never overrides the disc.
// Only the menu archive comes from the host; all other files stay on the disc.
class OfflineMenuDevice final : public rex::filesystem::HostPathDevice {
 public:
  OfflineMenuDevice(const std::filesystem::path& directory,
                    rex::filesystem::Device* disc)
      : HostPathDevice("\\Device\\TabletennisOfflineMenu", directory, true),
        disc_(disc) {}

  rex::filesystem::Entry* ResolvePath(std::string_view path) override {
    std::string normalized(path);
    for (char& c : normalized) {
      if (c == '/') c = '\\';
      if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    if (!normalized.empty() && normalized.front() == '\\') {
      normalized.erase(0, 1);
    }
    // The game opens the directory first, then opens flash.rpf relative to that
    // handle. Return the host directory too, so the relative open stays here.
    if (normalized == "assets\\resource\\flash") {
      return HostPathDevice::ResolvePath("");
    }
    if (normalized == "assets\\resource\\flash\\flash.rpf" ||
        normalized == "flash.rpf") {
      auto* entry = HostPathDevice::ResolvePath("flash.rpf");
      return entry;
    }
    return disc_->ResolvePath(path);
  }

 private:
  rex::filesystem::Device* disc_;
};
