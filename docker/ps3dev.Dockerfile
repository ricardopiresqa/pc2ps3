# PC2PS3 PS3 toolchain image.
# Recipe reproduced from akinozgen/fallout1-ce-ps3 (proven on RPCS3 + real HEN console):
# the ps3aqua trio (ps3toolchain GCC 7.5 + newlib 4.4, PSL1GHT, SDL2_PSL1GHT) is
# patched against itself; mixing with upstream ps3dev does not build SDL2.
# Pins (full SHA-1, also recorded in docker/toolchain.lock.json):
#   ps3aqua/ps3toolchain   961fddac01337f18da08f4471d558a7a5b0d9af2
#   ps3aqua/PSL1GHT        af9d3d964c8faa69abce4961a269f9582e05a33f (pinned by 008-psl1ght.sh)
#   SDL2_PSL1GHT           vendored in akinozgen/fallout1-ce-ps3 e4f14cb406ccbab7bc94f85cfd8c596fe4b7c419
# Build (~1.5-2 h, one time):  docker build -t pc2ps3/ps3dev:1 -f docker/ps3dev.Dockerfile docker
FROM ubuntu:24.04

SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf automake bison bzip2 ca-certificates clang cmake curl flex g++ gcc git \
        libelf-dev libgmp-dev libmpc-dev libmpfr-dev libncurses-dev libssl-dev libtool \
        libtool-bin make patch pkg-config python-is-python3 python3 python3-dev \
        python3-setuptools texinfo wget xz-utils zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

ENV PS3DEV=/usr/local/ps3dev
ENV PSL1GHT=$PS3DEV
ENV PATH=$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin:$PATH

ARG PS3TOOLCHAIN_COMMIT=961fddac01337f18da08f4471d558a7a5b0d9af2
RUN git clone https://github.com/ps3aqua/ps3toolchain.git /opt/ps3toolchain \
    && git -C /opt/ps3toolchain checkout "$PS3TOOLCHAIN_COMMIT"
# ftp.gnu.org intermittently returns HTTP 500; kernel.org mirrors the same tarballs.
RUN sed -i 's|https://ftp.gnu.org/gnu/|https://mirrors.kernel.org/gnu/|g' /opt/ps3toolchain/scripts/*.sh
# GDB steps (003, 007) skipped. One layer per step so an interrupted build resumes.
RUN cd /opt/ps3toolchain && ./toolchain.sh 1
RUN cd /opt/ps3toolchain && ./toolchain.sh 2
RUN cd /opt/ps3toolchain && ./toolchain.sh 4
RUN cd /opt/ps3toolchain && ./toolchain.sh 5
RUN cd /opt/ps3toolchain && ./toolchain.sh 6
RUN cd /opt/ps3toolchain && ./toolchain.sh 8 && rm -rf /opt/ps3toolchain/build && ppu-gcc --version

ARG FALLOUT_PS3_COMMIT=e4f14cb406ccbab7bc94f85cfd8c596fe4b7c419
RUN git clone --filter=blob:none --no-checkout https://github.com/akinozgen/fallout1-ce-ps3.git /opt/f1 \
    && git -C /opt/f1 sparse-checkout set SDL2_PSL1GHT \
    && git -C /opt/f1 checkout "$FALLOUT_PS3_COMMIT" \
    && mv /opt/f1/SDL2_PSL1GHT /opt/SDL2_PSL1GHT && rm -rf /opt/f1
# autogen.sh is committed without the exec bit (exit 126), so run it through sh.
RUN cd /opt/SDL2_PSL1GHT && sh ./autogen.sh \
    && mkdir -p build-ps3 && cd build-ps3 \
    && ../configure --host=powerpc64-ps3-elf --prefix="$PS3DEV/portlibs/ppu" \
    && make -j"$(nproc)" && make install \
    && cd / && rm -rf /opt/SDL2_PSL1GHT

WORKDIR /src
