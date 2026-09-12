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
for url_name in NB_BASE CDN_BASE UPDATE_BASE SITE_URL; do
    /usr/local/bin/validate-base-url "$url_name"
done
[[ -f /source/build.pl && -d /output ]] || {
    echo 'Bind the frontend read-only at /source and its .build-docker directory at /output.' >&2
    exit 2
}
[[ -f /run/secrets/hsapi-auth.env ]] || {
    echo 'Missing read-only HSAPI credential mount at /run/secrets/hsapi-auth.env.' >&2
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
echo 'Building with supplied HSAPI credentials and configured service URLs.'

# Never copy Git internals or prior container state into the build tree.
rsync -a --safe-links \
    --exclude='.git' --exclude='/.build-docker/' --exclude='/docker/' \
    --exclude='/.build-stage/' --exclude='/3hs.elf' --exclude='/3hs.3dsx' \
    --exclude='/3hs.cia' --exclude='/3hstool/3hstool' --exclude='/3hstool/*.o' \
    --exclude='/source/hsapi_auth.c' \
    /source/ "$run_dir/source/"
cd "$run_dir/source"
auth_source="$run_dir/source/source/hsapi_auth.c"
cleanup_auth_source() { rm -f -- "$auth_source"; }
trap cleanup_auth_source EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
AUTH_FILE=/run/secrets/hsapi-auth.env perl -e '
    use strict; use warnings;
    my $path = $ENV{AUTH_FILE};
    open my $in, "<:raw", $path or die "Unable to read HSAPI credential file.\n";
    my %value;
    while (defined(my $line = <$in>)) {
        $line =~ s/\n\z//;
        die "Invalid HSAPI credential file.\n" if $line =~ /[\r\0]/;
        my ($key, $val) = $line =~ /\A(HSAPI_USER|HSAPI_TOKEN)=(.*)\z/s
            or die "Invalid HSAPI credential file.\n";
        die "Invalid HSAPI credential file.\n" if exists $value{$key} || !length($val);
        $value{$key} = $val;
    }
    die "Invalid HSAPI credential file.\n"
        unless keys(%value) == 2 && exists($value{HSAPI_USER}) && exists($value{HSAPI_TOKEN});
    die "HSAPI token is too long.\n" if length($value{HSAPI_TOKEN}) > 2147483647;
    sub c_string { return join "", map { sprintf "\\%03o", $_ } unpack "C*", $_[0]; }
    open my $out, ">:raw", "source/hsapi_auth.c" or die "Unable to create generated auth source.\n";
    print {$out} "#include <string.h>\n";
    print {$out} "const char *hsapi_user = \"", c_string($value{HSAPI_USER}), "\";\n";
    # These symbol names are the upstream client ABI; the Docker-facing credential is a token.
    print {$out} "const int hsapi_password_length = ", length($value{HSAPI_TOKEN}), ";\n";
    print {$out} "void hsapi_password(char *ret) {\n    memcpy(ret, \"", c_string($value{HSAPI_TOKEN}), "\", hsapi_password_length);\n}\n";
'
unset AUTH_FILE

export VERSION=0
config="$mode,http_backend=httpc,targets=$targets,update_base=$UPDATE_BASE,nb_base=$NB_BASE,cdn_base=$CDN_BASE,site_url=$SITE_URL"

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
    echo 'Endpoints: supplied at build time; authentication: supplied at build time'
    arm-none-eabi-g++ --version
    dpkg-query -W libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
    dkp-pacman -Q
} > "$run_dir/artifacts/build-environment.txt"
echo "Build succeeded. Artifacts: $run_dir/artifacts"
