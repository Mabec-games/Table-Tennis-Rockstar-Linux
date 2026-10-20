#!/usr/bin/env bash
# Temporary Radeon-only run; all settings and the normal launcher are retained.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
amd_manifest=/usr/share/vulkan/icd.d/radeon_icd.json
if [[ ! -r "$amd_manifest" ]]; then
    echo "AMD RADV Vulkan driver manifest not found: $amd_manifest" >&2
    exit 1
fi
# Restrict Vulkan driver discovery to RADV, avoiding changing device indices.
# https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md
export VK_DRIVER_FILES="$amd_manifest"
export VK_ICD_FILENAMES="$amd_manifest"
unset VK_ADD_DRIVER_FILES VK_LOADER_DRIVERS_SELECT VK_LOADER_DRIVERS_DISABLE
unset __NV_PRIME_RENDER_OFFLOAD __VK_LAYER_NV_optimus DRI_PRIME
echo "AMD Radeon-only gameplay recording (RADV). Using your saved game settings."
exec bash "$root/profile-gameplay.sh" "$@"
