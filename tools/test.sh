#!/usr/bin/env bash
# Runs the USB scripts' tests (tests/run.sh) in the test container
# (tests/Dockerfile).
#
#   tools/test.sh [--factory DIR]
#
# --factory DIR is passed on to tests/run.sh (see there); the folder is
# mounted read-only. HUDLINK_TEST_IMAGE names another image.
set -euo pipefail

tools=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$tools")
image=${HUDLINK_TEST_IMAGE:-hudlink-test}

mounts=(-v "${repo}:/src:ro")
args=()
while [ $# -gt 0 ]; do
  case "$1" in
    --factory)
      dir=$(cd "${2:?--factory needs a folder}" && pwd)
      mounts+=(-v "${dir}:/factory:ro")
      args+=(--factory /factory)
      shift 2 ;;
    *) args+=("$1"); shift ;;
  esac
done

if ! docker image inspect "$image" > /dev/null 2>&1; then
  echo "Building the ${image} image..."
  docker build --pull -t "$image" "${repo}/tests"
fi

docker run --rm "${mounts[@]}" -w /src \
  "$image" bash tests/run.sh ${args[@]+"${args[@]}"}
