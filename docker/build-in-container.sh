#!/usr/bin/env bash
set -euo pipefail

# /source must be a read-only bind of 3hs-frontend; /output must be a writable
# bind of 3hs-frontend/.build-docker. Docker Desktop manages toolchain/image storage.
if [[ $# -gt 2 ]]; then
    echo 'Usage: build-3hs [debug|release] [3dsx|cia|both|elf]' >&2
    exit 2
fi
mode=${1:-debug}
format=${2:-both}
case "$mode" in debug|release) ;; *) echo 'Invalid build mode' >&2; exit 2 ;; esac
case "$format" in
    both) targets='3dsx cia' ;;
    3dsx|cia|elf) targets=$format ;;
    *) echo 'Invalid output format' >&2; exit 2 ;;
esac
jobs=${BUILD_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'BUILD_JOBS must be a positive integer' >&2; exit 2; }
[[ -f /source/build.pl && -d /output ]] || {
    echo 'Bind the frontend read-only at /source and its .build-docker directory at /output.' >&2
    exit 2
}
for tool in perl make gcc rsync arm-none-eabi-g++ tex3ds bin2s bannertool; do
    command -v "$tool" >/dev/null || { echo "Missing build tool: $tool" >&2; exit 1; }
done
case "$format" in both|cia) command -v makerom >/dev/null ;; esac
case "$format" in both|3dsx) command -v 3dsxtool >/dev/null ;; esac

# A fresh directory prevents stale binaries from being mistaken for success.
mkdir -p /output/runs
run_dir=$(mktemp -d /output/runs/build.XXXXXXXX)
mkdir -p "$run_dir/source" "$run_dir/artifacts" "$run_dir/tmp"
export TMPDIR="$run_dir/tmp"
exec > >(tee "$run_dir/build.log") 2>&1
echo "Build directory: $run_dir"
echo 'Compile-only build using dummy credentials and nonfunctional .invalid endpoints.'

# Never copy Git internals or prior container state into the build tree.
rsync -a --safe-links \
    --exclude='.git' --exclude='/.build-docker/' --exclude='/docker/' \
    --exclude='/.build-stage/' --exclude='/3hs.elf' --exclude='/3hs.3dsx' \
    --exclude='/3hs.cia' --exclude='/3hstool/3hstool' --exclude='/3hstool/*.o' \
    --exclude='/source/hsapi_auth.c' \
    /source/ "$run_dir/source/"
cd "$run_dir/source"
cat > source/hsapi_auth.c <<'AUTH'
#include <string.h>
const char *hsapi_user = "local-build-placeholder";
const int hsapi_password_length = sizeof("not-a-real-password") - 1;
void hsapi_password(char *ret) {
    memcpy(ret, "not-a-real-password", hsapi_password_length);
}
AUTH

export VERSION=0
config="$mode,http_backend=httpc,targets=$targets,update_base=http://updates.invalid/3hs,nb_base=http://catalog.invalid/nbapi,cdn_base=http://content.invalid,site_url=http://site.invalid"

# build.pl swallows Make's exit status. Initialize only, then call Make directly.
perl ./build.pl --init --target "$mode" --configure "$config"
make -f ".build-stage/$mode.target.mk" -j"$jobs"

artifacts=(3hs.elf)
case "$format" in both|3dsx) artifacts+=(3hs.3dsx) ;; esac
case "$format" in both|cia) artifacts+=(3hs.cia) ;; esac
for artifact in "${artifacts[@]}"; do
    [[ -s "$artifact" ]] || { echo "Missing or empty artifact: $artifact" >&2; exit 1; }
    cp "$artifact" "$run_dir/artifacts/"
done
{
    echo "Mode: $mode; targets: $targets; CIA version: 0"
    echo 'Endpoints/authentication: nonfunctional build-only placeholders'
    arm-none-eabi-g++ --version
    dpkg-query -W libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
    dkp-pacman -Q
} > "$run_dir/artifacts/build-environment.txt"
echo "Build succeeded. Artifacts: $run_dir/artifacts"
