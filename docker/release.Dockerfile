# Builds a release binary against an older Ubuntu baseline so the result's
# glibc / shared library requirements stay compatible with systems older
# than whatever the developer's own machine happens to run. See
# package_release.sh, which builds this image and copies /out/ back out.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        liblua5.4-dev \
        libglm-dev \
        libenet-dev \
        zlib1g-dev \
        libbullet-dev \
        libassimp-dev \
        libsdl2-dev \
        libglew-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

RUN mkdir -p /out/lib \
    && cp build/LandOfDran /out/ \
    && ./scripts/bundle-libs.sh /out/LandOfDran /out/lib
