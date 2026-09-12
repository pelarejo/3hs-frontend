#!/usr/bin/env bash
set -euo pipefail

frontend_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
auth_file=${1:-"$frontend_dir/.local-secrets/hsapi-auth.env"}

if [[ -e "$auth_file" ]]; then
    [[ -f "$auth_file" && ! -L "$auth_file" ]] || {
        echo "Refusing to overwrite a non-regular file: $auth_file" >&2
        exit 1
    }
    [[ -t 0 ]] || {
        echo "Auth file already exists; rerun interactively to confirm replacement: $auth_file" >&2
        exit 1
    }
    read -r -p "Replace existing auth file $auth_file? [y/N] " replace
    case "$replace" in y|Y|yes|YES|Yes) ;; *) echo 'Auth file left unchanged.'; exit 0 ;; esac
fi

IFS= read -r -p 'HSAPI username: ' username
IFS= read -r -s -p 'HSAPI password: ' password
printf '\n'
IFS= read -r -s -p 'Confirm HSAPI password: ' password_confirm
printf '\n'

[[ -n "$username" ]] || { echo 'Username must not be empty.' >&2; exit 2; }
[[ -n "$password" ]] || { echo 'Password must not be empty.' >&2; exit 2; }
[[ "$password" == "$password_confirm" ]] || { echo 'Passwords do not match.' >&2; exit 2; }
case "$username$password" in *$'\n'*|*$'\r'*) echo 'Credentials must not contain newline characters.' >&2; exit 2 ;; esac

parent_dir=$(dirname -- "$auth_file")
umask 077
mkdir -p -- "$parent_dir"
tmp_file=$(mktemp "$parent_dir/.hsapi-auth.XXXXXXXX")
cleanup() { rm -f -- "$tmp_file"; }
trap cleanup EXIT HUP INT TERM
printf 'HSAPI_USER=%s\nHSAPI_PASSWORD=%s\n' "$username" "$password" > "$tmp_file"
chmod 600 "$tmp_file"
mv -f -- "$tmp_file" "$auth_file"
trap - EXIT HUP INT TERM
echo "Saved credentials to $auth_file (mode 0600)."
