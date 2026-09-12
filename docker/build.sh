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
for url_name in NB_BASE CDN_BASE UPDATE_BASE SITE_URL; do
    bash "$frontend_dir/docker/validate-base-url.sh" "$url_name"
done
auth_file=${AUTH_FILE:-"$frontend_dir/.local-secrets/hsapi-auth.env"}
if [[ "$auth_file" != /* ]]; then
    auth_file="$(pwd -P)/$auth_file"
fi
auth_error() {
    echo "Invalid HSAPI credential file: $auth_file" >&2
    echo "Run 'make -C docker auth' or set AUTH_FILE to a valid credential file." >&2
    exit 2
}
[[ -f "$auth_file" && ! -L "$auth_file" && -r "$auth_file" ]] || auth_error
command -v perl >/dev/null || { echo 'Perl is required to validate credentials.' >&2; exit 1; }
AUTH_FILE="$auth_file" perl -e '
    use strict; use warnings;
    open my $in, "<:raw", $ENV{AUTH_FILE} or exit 1;
    my %seen;
    while (defined(my $line = <$in>)) {
        $line =~ s/\n\z//;
        exit 1 if $line =~ /[\r\0]/;
        my ($key, $value) = $line =~ /\A(HSAPI_USER|HSAPI_TOKEN)=(.*)\z/s or exit 1;
        exit 1 if $seen{$key}++ || !length($value);
    }
    exit((keys(%seen) == 2 && $seen{HSAPI_USER} == 1 && $seen{HSAPI_TOKEN} == 1) ? 0 : 1);
' || auth_error
command -v docker >/dev/null || { echo 'Docker Desktop is required.' >&2; exit 1; }
docker info >/dev/null
docker build --tag 3hs-builder:local "$frontend_dir/docker"
mkdir -p "$frontend_dir/.build-docker"
exec docker run --rm --network none \
    --mount "type=bind,src=$frontend_dir,dst=/source,readonly" \
    --mount "type=bind,src=$frontend_dir/.build-docker,dst=/output" \
    --mount "type=bind,src=$auth_file,dst=/run/secrets/hsapi-auth.env,readonly" \
    --env "BUILD_JOBS=$jobs" \
    --env "NB_BASE=$NB_BASE" \
    --env "CDN_BASE=$CDN_BASE" \
    --env "UPDATE_BASE=$UPDATE_BASE" \
    --env "SITE_URL=$SITE_URL" \
    3hs-builder:local "$mode" "$format"
