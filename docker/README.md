# Docker build

Verified on Docker Desktop, Linux arm64, on 2026-09-08: the image built and a
`debug both` build produced `3hs.elf`, `3hs.3dsx`, and `3hs.cia`. The original
client source was unchanged. Release mode and on-device behavior are untested.

This builds the existing frontend in a fresh copy using locally supplied HSAPI
authentication and service URLs. It does not change the original client source.
CIA metadata, including title ID, stays
upstream; the result is a compilation artifact, not a separately identified clone.

## Storage and execution

The user has authorized normal Docker Desktop storage as an exception to the
repository boundary. Docker manages its VM disk, images, layers, and build cache.
Project source copies, logs, and output artifacts remain in this frontend folder.
No devkitPro toolchain or FFmpeg libraries are installed directly on macOS.

Start Docker Desktop, then use its normal Docker connection. The wrapper honors
standard Docker environment/context configuration.

## Build

First create the required local credential file with the interactive helper:

```sh
make -C docker auth
```

The helper writes `.local-secrets/hsapi-auth.env` with mode `0600`; that directory
is ignored by Git. It asks before replacing an existing file and never displays
the token. To use another location for both setup and builds, set
`AUTH_FILE=/absolute/or/relative/path`. The file format is exactly:

```text
HSAPI_USER=your username
HSAPI_TOKEN=your token
```

Values may contain spaces, quotes, backslashes, equals signs, and other ordinary
characters, but must be nonempty single-line byte strings without NUL bytes. Blank, extra, unknown,
or duplicate entries are rejected. The file is parsed as data and is never sourced
as shell code. A missing or invalid file stops the build before Docker is invoked.
The Docker build translates `HSAPI_TOKEN` into the upstream client's
`hsapi_password` and `hsapi_password_length` C symbols. Those compatibility names,
and the upstream `X-Auth-Password` HTTP header they populate, remain unchanged.
Legacy files containing `HSAPI_PASSWORD` are rejected; rerun `make -C docker auth`.

From `3hs-frontend/`, the locally maintained Make interface can derive all four
service URLs from one server base (replace the example address with the backend
host):

```sh
make -C docker build SERVER_BASE=http://192.168.1.50:8000
```

On macOS, print the LAN IPv4 address for the default network interface with
`ipconfig getifaddr "$(route -n get default 2>/dev/null | awk '/interface:/{print $2; exit}')"`,
then substitute its output for `192.168.1.50` above.

This sets `NB_BASE` to `SERVER_BASE/nbapi`, `CDN_BASE` to `SERVER_BASE`,
`UPDATE_BASE` to `SERVER_BASE/update`, and `SITE_URL` to `SERVER_BASE/site`.
An explicit value overrides only its corresponding default, for example:

```sh
make -C docker build SERVER_BASE=http://192.168.1.50:8000 \
  SITE_URL=https://example.test
```

Fully explicit configuration remains supported:

```sh
make -C docker build \
  NB_BASE=http://192.168.1.50:8000/nbapi \
  CDN_BASE=http://192.168.1.50:8000 \
  UPDATE_BASE=http://192.168.1.50:8000/update \
  SITE_URL=http://192.168.1.50:8000/site
```

`NB_BASE` and `CDN_BASE` map to the current backend: the catalog API is under
`http://<LAN-IP>:8000/nbapi`, while content downloads are rooted at
`http://<LAN-IP>:8000`. The backend does not currently provide update or website
surfaces, but `UPDATE_BASE` and `SITE_URL` are still mandatory; calls to those
example paths will fail until the corresponding backend features are implemented.

`SERVER_BASE`, when used, and all four resolved values must be absolute `http://`
or `https://` URLs with a nonempty authority and no trailing slash, query,
fragment, embedded credentials, comma,
whitespace, or control characters. Invalid or missing values stop the build before
Docker is invoked and are revalidated inside the build container.

`SERVER_BASE` fallback is a Make-interface convenience. Calling
`bash docker/build.sh` directly still requires `NB_BASE`, `CDN_BASE`,
`UPDATE_BASE`, and `SITE_URL` to be set explicitly.

The build defaults to debug mode and both package formats. Override the build
settings alongside the required URLs when needed:

```sh
make -C docker build MODE=release FORMAT=cia BUILD_JOBS=4 \
  SERVER_BASE=http://192.168.1.50:8000
```

Other targets are:

```sh
make -C docker help
make -C docker clean
make -C docker image-clean
make -C docker distclean
```

`clean` removes only `3hs-frontend/.build-docker/`. `image-clean` removes only the
local `3hs-builder:local` image and succeeds when that image is already absent.
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
The credential file is mounted read-only into the transient build container; it is
not copied into the Docker build context or an image layer. Generated C safely
escapes every credential byte and is scrubbed from the retained run source on
success, failure, or interruption.

The credentials remain extractable from resulting CIA/ELF/3DSX files and retained
build outputs. In addition, the client sends its authentication headers in
plaintext over HTTP on the LAN because these builds still use HTTP endpoints.
Use a dedicated, unprivileged HSAPI token, protect the credential file, do not
share artifacts with embedded credentials, and only use a trusted local network.

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
