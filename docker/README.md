# Docker build

Verified on Docker Desktop, Linux arm64, on 2026-09-08: the image built and a
`debug both` build produced `3ls.elf`, `3ls.3dsx`, and `3ls.cia`. Release mode
and on-device behavior are untested.

This builds the modified frontend in a fresh copy. The client reads its local
NBAPI/content/update server and HSAPI credentials from
`/3ds/3ls/server-config` at runtime. The upstream copyright and GPLv3 notices
remain in the source.
CIA metadata identifies the modified client as 3LS with application title ID
`0004000003DF2000`, separate from upstream 3HS.

## Storage and execution

The user has authorized normal Docker Desktop storage as an exception to the
repository boundary. Docker manages its VM disk, images, layers, and build cache.
Project source copies, logs, and output artifacts remain in this frontend folder.
No devkitPro toolchain or FFmpeg libraries are installed directly on macOS.

Start Docker Desktop, then use its normal Docker connection. The wrapper honors
standard Docker environment/context configuration.

## Build

From `3hs-frontend/`, build with the locally maintained Make interface:

```sh
make -C docker build
```

`SITE_URL` is the only build-time URL. It defaults to
`https://github.com/pelarejo/3ds-local-shop`; override it explicitly when needed:

```sh
make -C docker build SITE_URL=https://example.com/3ls
```

The saved runtime configuration produces catalog URLs under
`http://<server>:<port>/nbapi`, content URLs rooted at
`http://<server>:<port>`, and update URLs under
`http://<server>:<port>/update`. The untouched upstream configure step still
requires corresponding macros, so the container supplies reserved `.invalid`
placeholders for them. They are fixed implementation details, cannot be
overridden through Make or the environment, and are not used for requests.

`SITE_URL` defines `HS_SITE_LOC`, the compile-time project website shown by the
startup notice. The client appends `/releases`, so the default opens
`https://github.com/pelarejo/3ds-local-shop/releases`. It must be an absolute
`http://` or `https://` URL with a nonempty authority and no trailing slash,
query, fragment, embedded credentials, comma, whitespace, or control characters.
An invalid or missing value stops the build before Docker is invoked and is
revalidated inside the build container.

The build defaults to debug mode and both package formats. Override the build
settings alongside the required URLs when needed:

```sh
make -C docker build MODE=release FORMAT=cia BUILD_JOBS=4
```

Other targets are:

```sh
make -C docker help
make -C docker clean
make -C docker image-clean
make -C docker distclean
```

`clean` removes only `3hs-frontend/.build-docker/`. `image-clean` removes only the
local `3ls-builder:local` image and succeeds when that image is already absent.
`distclean` performs both operations. None of these commands performs a global
Docker prune.

The image uses the Docker daemon's native architecture. On Apple Silicon this
is Linux arm64, avoiding amd64 emulation. The source mount is read-only. The
wrapper builds the image and runs the compiler; Docker caches image layers on
subsequent runs.

Modes: `debug` (default) or `release`. Formats: `both` (default), `3dsx`, `cia`,
or `elf`. Set `BUILD_JOBS=4` to change parallelism.
Compilation runs with networking disabled; dependency acquisition happens during
the image build.

Each attempt retains a new `.build-docker/runs/build.XXXXXXXX/` containing
`source/`, `build.log`, and `artifacts/`. Failed runs retain their logs and source
copy. Successful runs export ELF and requested packages plus dependency versions.
Runtime credentials are stored on the SD card and sent in authentication headers
over plain HTTP on the LAN. Use a dedicated, unprivileged HSAPI token, protect
the SD-card configuration, and only use a trusted local network.

## Verified environment and remaining limits

- The dated [official devkitARM image](https://hub.docker.com/r/devkitpro/devkitarm/tags)
  `20260610` is the starting point. Its [upstream recipe](https://github.com/devkitPro/docker/blob/master/devkitarm/Dockerfile)
  installs `3ds-dev` and `3ds-portlibs`. The base image digest is pinned after the
  first permitted pull; system package versions still need locking for a fully
  reproducible build.
- devkitARM r68 / GCC 16.1.0, libctru 2.7.0, citro2d 1.7.0, citro3d 1.7.1,
  and 3DS mbedTLS 2.28.8 compiled and linked successfully.
- Native GCC, Make, Perl, and Debian Bookworm FFmpeg 5.1.9 development libraries
  successfully built `3hstool`, including its legacy `channel_layout` API.
- [bannertool 1.2.3](https://github.com/carstene1ns/3ds-bannertool/releases/tag/1.2.3)
  is built from the maintained source backup using CMake 3.28.4 in a container
  virtual environment. [makerom 0.19.0](https://github.com/3DSGuy/Project_CTR/releases/tag/makerom-v0.19.0)
  is built with its bundled libraries. Source tags and system packages should be
  locked to commits/versions after a successful build is established.
- `build.pl` still contains hardcoded C++ 12.2.0 include paths; the verified
  GCC 16.1.0 build succeeded without modifying them.
- The log contains existing warnings about incomplete translations, generated
  assembly comments, the debug-build flag, and a missing GNU-stack note. These
  did not prevent compilation or packaging.
- Supplemental font and banner are already bundled; rebuilding them is outside
  this initial build. The separate `file_forwarder/` project is not built.

The successful run is retained in
[build.2SPHCRWI](../.build-docker/runs/build.2SPHCRWI/), including its
[build log](../.build-docker/runs/build.2SPHCRWI/build.log) and
[dependency manifest](../.build-docker/runs/build.2SPHCRWI/artifacts/build-environment.txt).
These generated files are local and ignored by Git.

The wrapper calls Make directly after `build.pl --init`, so compiler failures
produce a nonzero container exit status instead of being hidden by the Perl
script. Runtime and on-device behavior require later validation.

## Local integration layer

The entire `docker/` directory is maintained locally and is separate from the
imported upstream frontend source. When replacing the upstream source with a new
ZIP or snapshot, preserve `docker/`, the nested `.git/` directory,
`.build-docker/`, and any other explicitly local files. Replace only
upstream-owned files and review the resulting diff before continuing.
