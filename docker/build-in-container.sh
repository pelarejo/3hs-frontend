#!/usr/bin/env bash
set -euo pipefail

# /source must be a read-only bind of 3ds-local-shop; /output must be a writable
# bind of 3ds-local-shop/.build-docker. Docker Desktop manages toolchain/image storage.
if [[ $# -gt 2 ]]; then
    echo 'Usage: build-3ls [debug|release] [3dsx|cia|both|elf]' >&2
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
/usr/local/bin/validate-base-url SITE_URL
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
echo 'Building with a configured website URL; service endpoints and HSAPI credentials are loaded at runtime.'

# Never copy Git internals or prior container state into the build tree.
rsync -a --safe-links \
    --exclude='.git' --exclude='/.build-docker/' --exclude='/docker/' \
    --exclude='/.build-stage/' --exclude='/3ls.elf' --exclude='/3ls.3dsx' \
    --exclude='/3ls.cia' --exclude='/3hstool/3hstool' --exclude='/3hstool/*.o' \
    /source/ "$run_dir/source/"
cd "$run_dir/source"
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

export VERSION=0
readonly COMPILE_ONLY_CATALOG_URL='https://invalid.invalid/nbapi'
readonly COMPILE_ONLY_CONTENT_URL='https://invalid.invalid'
readonly COMPILE_ONLY_UPDATER_URL='https://invalid.invalid/update'
config="$mode,http_backend=httpc,targets=$targets,update_base=$COMPILE_ONLY_UPDATER_URL,nb_base=$COMPILE_ONLY_CATALOG_URL,cdn_base=$COMPILE_ONLY_CONTENT_URL,site_url=$SITE_URL"

# build.pl swallows Make's exit status. Initialize only, then call Make directly.
perl ./build.pl --init --target "$mode" --configure "$config"
make -f ".build-stage/$mode.target.mk" -j"$jobs"

artifacts=(3ls.elf)
case "$format" in both|3dsx) artifacts+=(3ls.3dsx) ;; esac
case "$format" in both|cia) artifacts+=(3ls.cia) ;; esac
for artifact in "${artifacts[@]}"; do
    [[ -s "$artifact" ]] || { echo "Missing or empty artifact: $artifact" >&2; exit 1; }
    cp "$artifact" "$run_dir/artifacts/"
done
{
    echo "Mode: $mode; targets: $targets; CIA version: 0"
    echo 'Unused compatibility endpoints: fixed invalid placeholders; website: supplied at build time; authentication: loaded at runtime'
    arm-none-eabi-g++ --version
    dpkg-query -W libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
    dkp-pacman -Q
} > "$run_dir/artifacts/build-environment.txt"
echo "Build succeeded. Artifacts: $run_dir/artifacts"
