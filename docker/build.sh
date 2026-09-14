#!/usr/bin/env bash
set -euo pipefail
frontend_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
if [[ $# -gt 2 ]]; then
    echo 'Usage: bash docker/build.sh [debug|release] [both|3dsx|cia|elf]' >&2
    exit 2
fi
mode=${1:-debug}
format=${2:-both}
case "$mode" in debug|release) ;; *) echo 'Invalid build mode' >&2; exit 2 ;; esac
case "$format" in both|3dsx|cia|elf) ;; *) echo 'Invalid output format' >&2; exit 2 ;; esac
jobs=${BUILD_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'BUILD_JOBS must be a positive integer' >&2; exit 2; }
bash "$frontend_dir/docker/validate-base-url.sh" SITE_URL
command -v docker >/dev/null || { echo 'Docker Desktop is required.' >&2; exit 1; }
docker info >/dev/null
docker build --tag 3ls-builder:local "$frontend_dir/docker"
mkdir -p "$frontend_dir/.build-docker"
exec docker run --rm --network none \
    --mount "type=bind,src=$frontend_dir,dst=/source,readonly" \
    --mount "type=bind,src=$frontend_dir/.build-docker,dst=/output" \
    --env "BUILD_JOBS=$jobs" \
    --env "SITE_URL=$SITE_URL" \
    3ls-builder:local "$mode" "$format"
