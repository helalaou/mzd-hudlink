# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 The HUDLink authors (see AUTHORS).
#
# Build environment for HUDLink.
#
# Debian with the lmagder/m3-toolchain ARM cross-compiler (crosstool-NG,
# GCC 4.9.1 for the CMU's Cortex-A9), pinned to the commit HUDLink is built
# and tested with. The toolchain's programs are x86-64 Linux binaries, so the
# image is always linux/amd64.
#
#   docker build --platform linux/amd64 -t hudlink-build tools
#
# tools/build.sh builds this image when it is missing.

FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates git make \
 && rm -rf /var/lib/apt/lists/*

ARG M3_TOOLCHAIN_URL=https://github.com/lmagder/m3-toolchain.git
ARG M3_TOOLCHAIN_COMMIT=61ec0343de84f6fc7c46840056df1d600d44be8a

RUN git clone --quiet "$M3_TOOLCHAIN_URL" /opt/m3 \
 && git -C /opt/m3 checkout --quiet "$M3_TOOLCHAIN_COMMIT" \
 && rm -rf /opt/m3/.git

ENV M3TOOLCHAIN=/opt/m3
