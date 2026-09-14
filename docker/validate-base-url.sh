#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo 'Usage: validate-base-url VARIABLE_NAME' >&2
    exit 2
fi

name=$1
case "$name" in SITE_URL) ;; *)
    echo 'Invalid URL variable name.' >&2
    exit 2
esac

value=${!name-}
URL_NAME="$name" URL_VALUE="$value" perl -e '
    use strict; use warnings;
    my ($name, $url) = @ENV{qw(URL_NAME URL_VALUE)};
    sub fail { print STDERR "$name must be an absolute http:// or https:// URL with a nonempty authority, no credentials, query, fragment, comma, whitespace, control characters, or trailing slash.\n"; exit 2; }
    fail() unless length($url);
    fail() if $url =~ /[\x00-\x20\x7f,\\<>"{}|^`]/;
    fail() if $url =~ /%(?![0-9A-Fa-f]{2})/;
    my ($authority) = $url =~ m{\Ahttps?://([^/?#]+)(?:/[^?#]*)?\z} or fail();
    fail() if $authority =~ /@/;
    my ($host, $port);
    if ($authority =~ /\A(\[[0-9A-Fa-f:.]+\])(?::([0-9]+))?\z/) {
        ($host, $port) = ($1, $2);
    } elsif ($authority =~ /\A([A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?)(?::([0-9]+))?\z/) {
        ($host, $port) = ($1, $2);
    } else {
        fail();
    }
    fail() if defined($port) && ($port < 1 || $port > 65535);
    fail() if $url =~ m{/\z};
' 
