# Dated upstream image, pinned to the digest resolved during the first image build.
ARG DEVKITPRO_IMAGE=devkitpro/devkitarm:20260610@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5
FROM ${DEVKITPRO_IMAGE}

ARG BANNERTOOL_REF=1.2.3
ARG MAKEROM_REF=makerom-v0.19.0

# Bookworm FFmpeg retains the channel_layout APIs used by the bundled 3hstool.
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential ca-certificates git perl rsync python3-venv \
      libavformat-dev libavcodec-dev libavutil-dev libswresample-dev \
    && rm -rf /var/lib/apt/lists/*

# bannertool 1.2.3 requires CMake >= 3.28; Bookworm's system CMake is older.
RUN python3 -m venv /opt/build-cmake \
    && /opt/build-cmake/bin/pip install --no-cache-dir cmake==3.28.4

RUN git clone --depth 1 --branch "${BANNERTOOL_REF}" \
      https://github.com/carstene1ns/3ds-bannertool.git /opt/build-src/bannertool \
    && /opt/build-cmake/bin/cmake -S /opt/build-src/bannertool \
      -B /opt/build-src/bannertool/build -DCMAKE_BUILD_TYPE=Release \
    && /opt/build-cmake/bin/cmake --build /opt/build-src/bannertool/build --parallel 2 \
    && /opt/build-cmake/bin/cmake --install /opt/build-src/bannertool/build

RUN git clone --depth 1 --branch "${MAKEROM_REF}" --recurse-submodules \
      https://github.com/3DSGuy/Project_CTR.git /opt/build-src/Project_CTR \
    && make -C /opt/build-src/Project_CTR/makerom deps \
    && make -C /opt/build-src/Project_CTR/makerom -j2 \
    && install -m 755 /opt/build-src/Project_CTR/makerom/bin/makerom /usr/local/bin/makerom

ENV PATH="/opt/devkitpro/devkitARM/bin:/opt/devkitpro/tools/bin:/opt/devkitpro/portlibs/3ds/bin:${PATH}"
COPY build-in-container.sh /usr/local/bin/build-3hs
RUN chmod 755 /usr/local/bin/build-3hs
WORKDIR /output
ENTRYPOINT ["/usr/local/bin/build-3hs"]
