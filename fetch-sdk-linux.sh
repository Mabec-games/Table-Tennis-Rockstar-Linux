#!/usr/bin/env bash
# Public SDK only; does not access the ISO or upload any files.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mkdir -p "$root/.tools"
archive="$root/.tools/rexglue-sdk-0.10.0-linux-amd64.zip"
expected=0388edb3f8fb1444f1b0ad5d440cb02d9690ac20a76b04fd6b7dcc19f59328a6
if [[ ! -f "$archive" ]] || [[ $(sha256sum "$archive" | cut -d ' ' -f 1) != "$expected" ]]; then
    curl --ipv4 --fail --location --retry 2 --connect-timeout 15 \
      --speed-limit 1024 --speed-time 30 \
      https://github.com/rexglue/rexglue-sdk/releases/download/v0.10.0/rexglue-sdk-0.10.0-linux-amd64.zip \
      --output "$archive.download"
    printf '%s  %s\n' "$expected" "$archive.download" | sha256sum --check
    mv -- "$archive.download" "$archive"
fi
printf 'Verified Linux SDK archive: %s\n' "$archive"
printf 'Extract with: unzip "%s" -d "%s"\n' "$archive" "$root/.tools/sdk-linux"
printf 'Then run: chmod +x "%s"\n' "$root/.tools/sdk-linux/linux-amd64/bin/rexglue"
printf 'See LINUX.md for the pinned renderer-source preparation and build steps.\n'
