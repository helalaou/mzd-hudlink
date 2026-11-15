#!/usr/bin/env bash
# Assembles the files HUDLink adds to a USB drive.
#
#   tools/make-usb.sh --units metric|imperial [--diag] [--so FILE] [--out DIR]
#
#   --units   the distance units the HUD shows (required)
#   --diag    the diagnostic build: also keeps a log on the head unit, which
#             the Save logs step copies to the USB drive
#   --so      package this library instead of building one (it must have
#             been built with the same --units)
#   --out     where to write the files (default dist/usb)
#
# Writes:
#   launcher.sh, hudlink.sh, hudlink-logs.sh   each with its .md5
#   pkg/hudlink/libpatch-blmjcicarplay.so
#   pkg/hudlink/build-info.txt
#   pkg/hudlink/manifest.txt, manifest.txt.md5
#
# Copy them to the root of a USB drive prepared as README.md describes.
set -euo pipefail

tools=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$tools")
so_name=libpatch-blmjcicarplay.so
scripts="launcher.sh hudlink.sh hudlink-logs.sh"

usage() { sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }
die() { echo "make-usb: $*" >&2; exit 1; }

units="" variant=release so="" out="${repo}/dist/usb"
while [ $# -gt 0 ]; do
  case "$1" in
    --units) units=${2:-}; shift 2 ;;
    --diag)  variant=diag; shift ;;
    --so)    so=${2:-}; shift 2 ;;
    --out)   out=${2:-}; shift 2 ;;
    -h|--help) usage 0 ;;
    *) echo "make-usb: unknown option: $1" >&2; usage 1 ;;
  esac
done
case "$units" in
  metric|imperial) ;;
  *) die "choose the HUD's distance units: --units metric or --units imperial" ;;
esac
[ -n "$out" ] || die "--out needs a folder"

md5_of() {
  if command -v md5sum > /dev/null; then md5sum "$1" | cut -d' ' -f1
  else md5 -q "$1"; fi
}

# --- The library
if [ -z "$so" ]; then
  "${tools}/build.sh" "$variant" "UNITS=${units}"
  so="${repo}/mazda/build/${variant}/${so_name}"
fi
[ -f "$so" ] || die "no library at ${so}"

# --- Checks of the USB scripts: ASCII only, LF line endings, valid sh
for s in $scripts; do
  f="${repo}/usb/${s}"
  [ -f "$f" ] || die "missing usb/${s}"
  if LC_ALL=C grep -q $'[^\t -~]' "$f"; then die "usb/${s} is not plain ASCII"; fi
  if LC_ALL=C grep -q $'\r' "$f"; then die "usb/${s} has CR line endings"; fi
  sh -n "$f" || die "usb/${s} is not valid sh"
done
file_count=$(sed -n 's/^FILE_COUNT=//p' "${repo}/usb/hudlink.sh")

# --- Assemble. Only the files this script writes are replaced.
mkdir -p "$out"
for s in $scripts; do rm -f "${out}/${s}" "${out}/${s}.md5"; done
rm -rf "${out}/pkg/hudlink"
mkdir -p "${out}/pkg/hudlink"
pkg="${out}/pkg/hudlink"

for s in $scripts; do
  cp "${repo}/usb/${s}" "${out}/${s}"
  echo "$(md5_of "${out}/${s}")  ${s}" > "${out}/${s}.md5"
done

cp "$so" "${pkg}/${so_name}"
# HUDLink's release tags are hudlink-v<version>; the repository also has the
# original project's tags, which are not HUDLink versions.
version=$(git -C "$repo" describe --tags --match 'hudlink-v*' --always --dirty 2> /dev/null || echo unknown)
{
  echo "HUDLink ${version}"
  echo "units: ${units}"
  echo "build: ${variant}"
} > "${pkg}/build-info.txt"

: > "${pkg}/manifest.txt"
for f in "$so_name" build-info.txt; do
  echo "$(md5_of "${pkg}/${f}") ${f}" >> "${pkg}/manifest.txt"
done
echo "$(md5_of "${pkg}/manifest.txt")  manifest.txt" > "${pkg}/manifest.txt.md5"

[ "$(grep -c . "${pkg}/manifest.txt")" = "$file_count" ] ||
  die "the manifest lists $(grep -c . "${pkg}/manifest.txt") files, hudlink.sh expects ${file_count}"

echo "HUDLink USB files in ${out}:"
(cd "$out" && find launcher.sh* hudlink.sh* hudlink-logs.sh* pkg/hudlink -type f | sort | sed 's/^/  /')
echo "bridge md5: $(md5_of "${pkg}/${so_name}") (${variant}, ${units})"
