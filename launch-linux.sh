#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# XWayland is the verified display path on the current Linux setup.
# Honour an explicitly selected SDL backend.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
exec python3 "$root/project/launcher/launcher_linux.py" "$@"
