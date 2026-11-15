#!/usr/bin/env bash
# Builds the HUDLink bridge in the build container (tools/Dockerfile).
#
#   tools/build.sh [make arguments]
#
#   tools/build.sh                     build/release, metric
#   tools/build.sh UNITS=imperial      build/release, feet and miles
#   tools/build.sh diag UNITS=imperial build/diag, with a log on the head unit
#
# The library is written to mazda/build/<release|diag|debug>/. See
# mazda/Makefile for every option. HUDLINK_IMAGE names another image.
set -euo pipefail

tools=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$tools")
image=${HUDLINK_IMAGE:-hudlink-build}

if ! docker image inspect "$image" > /dev/null 2>&1; then
  echo "Building the ${image} image (downloads the ARM toolchain once)..."
  docker build --pull --platform linux/amd64 -t "$image" "$tools"
fi

docker run --rm --platform linux/amd64 \
  --user "$(id -u):$(id -g)" \
  -v "${repo}:/src" -w /src/mazda \
  "$image" make M3TOOLCHAIN=/opt/m3 "$@"
