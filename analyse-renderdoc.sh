#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
output="$root/build/linux/renderdoc-analysis"
script="$root/project/tools/analyse_renderdoc.py"
if [[ "${1:-}" == "--debug-skin" ]]; then
    output="$output/skin-debug"
    script="$root/project/tools/debug_skin_capture.py"
elif [[ $# -ne 0 ]]; then
    printf '%s\n' 'Usage: bash analyse-renderdoc.sh [--debug-skin]' >&2
    exit 2
fi
mkdir -p "$output"
export TT_PROJECT_ROOT="$root"
export QT_QPA_PLATFORM=offscreen
printf '%s\n' 'Analysing the saved capture. No RenderDoc interaction is needed.'
if qrenderdoc --python "$script" > "$output/console.log" 2>&1; then
    tail -n 4 "$output/progress.log"
else
    printf '%s\n' "Analysis stopped; details: $output/error.txt"
    tail -n 8 "$output/progress.log"
    exit 1
fi
