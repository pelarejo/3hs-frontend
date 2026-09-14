# Upstream changes

This file records the small integration surface between the 3LS fork and the
original 3HS frontend. The original-source baseline is 3HS version 1.5.11 at
commit `983fbc947a23ef85dd381f4a1c7d7a999865ea03`. This runtime feature was
applied atop the current 3LS commit
`b2a632c1e3d2b7111c5a22c95738d57878c1709e`.

## 2026-09-14 — Runtime server and HSAPI authentication

Purpose: move the local NBAPI/content/update server address and HSAPI
username/token from build-time selection to a versioned SD-card configuration at
`/3ds/3ls/server-config`. Missing, incomplete, malformed, and unsupported
configuration is represented by empty/default runtime values and follows the
existing network error path. The public website link is intentionally outside
that runtime configuration: it remains the compile-time `HS_SITE_LOC` value.

Original/upstream files modified:

- `include/settings.hh`: adds one settings-menu identifier.
- `source/settings.cc`: adds the 3LS server entry and delegates its editor to
  the fork-specific configuration module.
- `source/main.cc`: loads runtime configuration before network use; the displayed
  website link remains `HS_SITE_LOC "/releases"`.
- `source/httpclient.cc`: both the HTTPC and curl backends obtain authenticated
  request header values directly from runtime configuration.
- `source/hsapi.cc`: catalog, content, and update URLs are assembled from the
  runtime server address and port. Its compile-time `HS_SITE_LOC` validation is
  retained for the website link used by `source/main.cc`.

New 3LS-specific files:

- `include/3ls_config.hh`
- `source/3ls_config.cc`
- `source/3ls_config_ui.cc`
- `tests/3ls_config_test.cc`
- `docs/upstream-changes.md`

Behavioral differences:

- Settings contains a **3LS server** editor for address, port, HSAPI username,
  and HSAPI token. Token keyboard input uses the existing password mode and the
  saved token is represented as “configured” rather than displayed. Its Clear
  button clears all four values and persists them when the editor exits.
- The configured base is `http://<server>:<port>`. NBAPI uses `/nbapi`, content
  uses the base directly, and updates use `/update`.
- The website is not derived from the local server. The startup notice displays
  compile-time `HS_SITE_LOC/releases`. Docker builds default to
  `https://github.com/pelarejo/3ds-local-shop/releases`.
- Authenticated requests send the configured username as `X-Auth-User` and the
  configured token as `X-Auth-Password`. Empty values are still sent through
  the existing authenticated-request path. Unauthenticated requests are not
  changed.
- Saving opens the configuration file for direct binary overwrite. If writing is
  interrupted, startup rejects the missing, truncated, or malformed file and
  uses empty/default runtime values; the user must re-enter the server settings.

Compile-time HSAPI credential generation has been removed. The Docker container
passes fixed reserved `.invalid` URL placeholders for the otherwise-unused
NBAPI/content/update definitions required by the untouched upstream configure
step. They are not user-configurable. Request URL construction is runtime-driven;
`HS_SITE_LOC` remains the sole configurable compile-time website location.

### Rebase guidance

When rebasing onto a newer 3HS snapshot, first port the two new 3LS configuration
files unchanged. Then reapply only the small hooks listed above: startup load,
settings row/editor call, runtime header getters in each HTTP backend, runtime
URL getter calls in HSAPI, and the compile-time `HS_SITE_LOC` use in main. Review
upstream changes to proxy input, request
authentication flags, endpoint paths, redirects/resume handling, and settings
menu types before resolving conflicts. Do not restore compile-time HSAPI
credential symbols or generation.

### Manual device/emulator verification

1. Start without `/3ds/3ls/server-config`; confirm normal startup and the
   existing network error path, with no new onboarding or dialog.
2. Enter all four values under Settings → 3LS server, exit the editor, restart,
   and confirm the values reload (the token should only show “configured”).
   Change only the token, exit, restart again, and confirm the replacement token
   is used rather than the original token.
   Re-enter the editor, press Clear, confirm all four labels clear immediately,
   then exit and restart to confirm the cleared configuration persists.
3. Corrupt the configuration magic/version or truncate the file; confirm startup
   does not crash and requests follow the existing error path. Re-enter the
   server settings to restore configuration after an interrupted write.
4. Against a test server, inspect requests: authenticated catalog/content calls
   contain the configured `X-Auth-User` and `X-Auth-Password`; unauthenticated
   traffic has no new headers. Repeat with empty username/token.
5. Download, cancel/resume, and follow a redirected download to confirm those
   behaviors remain unchanged. Check application and build logs do not reveal
   the token.
