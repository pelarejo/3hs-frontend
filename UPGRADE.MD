# Upgrading the 3LS frontend

This file records the locally maintained differences from the original 3HS
source so that a future upstream import does not silently restore the official
application identity or its SD-card paths.

## Baseline and repository state

The original source baseline is commit
`983fbc947a23ef85dd381f4a1c7d7a999865ea03` (`3hs version 1.5.11`). At the time
this document was added, that commit was reachable as local branch `main` and
remote-tracking ref `origin/main`. The checked-out `dev` branch also contained
local integration changes on top of it.

The 3LS identity changes described below were initially an uncommitted working
tree delta from `cd1a330`. Use the following command to review the complete local
delta, including the committed Docker integration:

```sh
git diff 983fbc947a23ef85dd381f4a1c7d7a999865ea03
```

## Identity and visible branding

These values must stay distinct from official 3HS:

| Purpose | 3LS value | Maintained in |
| --- | --- | --- |
| Application UniqueId | `0x3DF20` | `cia_stuff/3hs.rsf` |
| Derived application title ID | `0004000003DF2000` | Derived by `makerom` from category `Application` and the UniqueId |
| Product code | `CTR-N-3LSA` | `cia_stuff/3hs.rsf` |
| Internal title | `3ls_app` | `cia_stuff/3hs.rsf` |
| Output basename | `3ls` | `build.pl` |
| SMDH short title | `3LS` | `build.pl` |
| SMDH long description | `3DS Local Shop client` | `build.pl` |
| SMDH publisher | `3LS` | `build.pl` |

The UniqueId is the collision-critical value. Reverting it to upstream
`0x3DF10` would derive title ID `0004000003DF1000` and make this package target
the official 3HS title/ticket/install record. A name, icon, product code, or CIA
version change alone does not create a separate installed title.

Keep the category as `Application`, `UseOnSD: true`, and the existing exheader
permissions unless an upstream change makes a reviewed functional change
necessary. The current app has `UseExtSaveData: false` and `SaveDataSize: 0KB`,
so there is no title-owned save or extdata migration.

## App-owned SD-card data

All app-owned persistent paths use `/3ds/3ls`, not `/3ds/3hs`:

- Settings: `/3ds/3ls/settings` in `source/settings.cc`.
- User themes: `/3ds/3ls/themes/` in `source/settings.cc`.
- Legacy proxy file: `/3ds/3ls/proxy` in `source/proxy.cc`.
- Audio configuration: `/3ds/3ls/audio.cfg` in
  `source/audio/configuration.c`.
- Music directory: `/3ds/3ls/music/` in `source/audio/configuration.c` and
  `source/audio_cfg.cc`.
- Logs: `/3ds/3ls/3ls.log` and its numbered rotations in `source/log.cc`.

The translated music-directory guidance in `lang/` must agree with the code.
After an import, search every language for `/3ds/3hs` and update any newly added
or changed message to `/3ds/3ls`.

The in-memory install queue has no persistent file to isolate. RomFS resources
are packaged per title and are naturally isolated.

`/3ds/dspfirm.cdc` in `include/ctr/mng.hh` is intentionally unchanged. It is a
shared, conventional DSP firmware dump rather than 3LS-owned state. Do not move
it under `/3ds/3ls` merely as part of rebranding.

## Updates and network identity

`source/hsapi.cc` builds self-update package names as:

- `${HS_UPDATE_BASE}/3ls-<version>.cia`
- `${HS_UPDATE_BASE}/3ls-<version>-<DEVICE_ID>.cia` for device-specific builds

The update version check remains `GET ${HS_UPDATE_BASE}/version`. A downloaded
update CIA must carry 3LS title ID `0004000003DF2000`; otherwise the running app
will not update itself and could target another installed title. The backend or
static update host must publish filenames matching the patterns above and keep
the version response compatible with the client's three-component parser.

`include/update.hh` uses the rebranded User-Agent
`3LS (3DS/CTR/KTR; ARMv6) 3ls/<version>`. Preserve it when resolving upstream
changes to update or HTTP headers. The existing NB API log-upload route
`/3hs-log` is a server API compatibility path and is intentionally not renamed;
changing it requires a coordinated backend contract change.

The 3DSX build cannot self-install a CIA update and remains a manual-update
target. The Docker workflow currently exports CIA version `0`; before production
update releases, define and document a monotonic CIA-version policy in addition
to the semantic `/version` response.

## Docker integration and build artifacts

The entire `docker/` directory is a locally maintained integration layer that
did not exist in baseline commit `983fbc9`. It provides:

- A pinned devkitPro base image plus container-built `bannertool` and `makerom`.
- Local HSAPI credential generation and validation.
- Build-time `NB_BASE`, `CDN_BASE`, `UPDATE_BASE`, and `SITE_URL` configuration,
  including `SERVER_BASE` convenience defaults.
- A read-only source mount and retained build runs under `.build-docker/runs/`.

The Docker image is `3ls-builder:local`, its entrypoint is `build-3ls`, and a
successful `both` build must export `3ls.elf`, `3ls.3dsx`, and `3ls.cia`.
`docker/build-in-container.sh`, `docker/build.sh`, `docker/Makefile`,
`docker/Dockerfile`, and `docker/README.md` must use those names consistently.
`supplement_font/update.sh` must invalidate `../3ls.3dsx`.

Preserve `docker/`, `.build-docker/`, `.local-secrets/`, and the nested `.git/`
when importing an upstream ZIP or replacing the upstream-owned source tree.
Never copy a credential file or generated `source/hsapi_auth.c` into a commit or
container image.

## License and artwork

3LS remains licensed under GPLv3. Preserve `LICENSE`, all upstream copyright and
license headers, source availability, attribution, and the README notice that
this is a modified and rebranded version changed for local-network use in 2026.
Do not describe the modified build as official 3HS.

The existing `cia_stuff/icon.png` and prebuilt `cia_stuff/banner.bnr` are
intentionally reused. `cia_stuff/banner.png` and `cia_stuff/audio.cwav` are source
assets but are not regenerated by the normal `build.pl` flow; the build copies
`banner.bnr` directly. Do not assume editing `banner.png` changes the CIA. Before
replacing or redistributing artwork, confirm its applicable license and preserve
the upstream art credit in `README`.

## Upgrade procedure and verification checklist

1. Ensure the working tree is understood and back up any uncommitted work. Never
   delete the nested `.git/`, `docker/`, `.build-docker/`, or `.local-secrets/`.
2. Record the old and proposed upstream commits. Confirm the proposed source and
   ref actually come from the intended upstream; this repository currently has
   no separately configured upstream remote.
3. Import or merge only upstream-owned files. Keep the upstream import and local
   Docker-overlay changes separable in history.
4. Review upstream changes to `build.pl`, the RSF/exheader, settings, logging,
   proxy, audio, update, HTTP, translations, and supplement-font scripts before
   reapplying this document's invariants.
5. Confirm the RSF still has `Application`, UniqueId `0x3DF20`, ProductCode
   `CTR-N-3LSA`, and internal title `3ls_app`. Confirm the resulting title ID is
   `0004000003DF2000`.
6. Confirm `build.pl` emits basename `3ls` and the SMDH strings remain `3LS`,
   `3DS Local Shop client`, and `3LS`.
7. Search collision-sensitive values. The following should return no active
   source matches (exclude `.git` and retained `.build-docker` snapshots):

   ```sh
   rg -n --hidden --glob '!.git/**' --glob '!.build-docker/**' \
     '/3ds/3hs|0x3DF10|CTR-N-3HSA|3hs_app|3hs-[^ ]*\.cia|3hs\.(elf|3dsx|cia)|3hs-builder|build-3hs' .
   ```

8. Confirm `/3ds/dspfirm.cdc` is still present and was not rebranded. Confirm
   every app-owned path and translated music hint uses `/3ds/3ls`.
9. Confirm the updater requests `3ls-<version>[-<DEVICE_ID>].cia` and coordinate
   that exact filename contract with the backend/update host. Inspect the
   downloaded CIA's title ID before publishing it.
10. Run static checks:

    ```sh
    bash -n docker/build.sh docker/build-in-container.sh docker/setup-auth.sh docker/validate-base-url.sh
    perl -c build.pl
    git diff --check
    ```

11. Run a clean Docker `both` build with explicit trusted local endpoints and a
    dedicated low-privilege token:

    ```sh
    make -C docker build MODE=debug FORMAT=both \
      SERVER_BASE=http://<LAN-IP>:8000
    ```

12. Verify nonempty `3ls.elf`, `3ls.3dsx`, and `3ls.cia` artifacts. Inspect the
    CIA metadata with an independent 3DS package tool and verify title ID
    `0004000003DF2000`, ProductCode `CTR-N-3LSA`, SMDH title `3LS`, and the
    intended CIA version.
13. On a test console, verify official 3HS and 3LS appear as separate HOME Menu
    records, launch independently, and retain separate settings, themes, proxy,
    audio, music, and logs. Verify uninstalling 3LS leaves official 3HS installed
    and does not delete `/3ds/3hs`.
14. Exercise update success and rejection cases: a correctly identified newer
    3LS CIA should update 3LS; a CIA with the official 3HS title ID must not be
    published at the 3LS update URL.

## Recommended Git organization

Use the existing repository as a real fork-style patch stack rather than
replacing the tree opaquely:

- Keep `origin` for the user's writable fork.
- After verifying the authoritative original repository URL, add it as a
  read-only `upstream` remote and fetch explicit refs. Do not invent that URL
  from the current `origin` configuration.
- Maintain a clean branch or tag at each imported upstream release, and keep the
  3LS product changes on a long-lived branch rebased or merged from that clean
  baseline according to the project's chosen history policy.
- Keep upstream imports, 3LS identity/runtime patches, and the local `docker/`
  overlay in separate commits. This makes conflicts reviewable and allows the
  Docker layer to be reapplied without mixing it into an upstream snapshot.
- If upstream is only available as archives, create one clearly labeled vendor
  import commit per archive, recording its checksum/source, while explicitly
  preserving the local-only paths listed above. Then replay or merge the small
  3LS and Docker commit series and run the checklist.

This approach is analogous to syncing a hosted fork: the clean upstream history
or vendor-import branch provides the comparison base, while the 3LS branch
contains a small, auditable local patch stack.
