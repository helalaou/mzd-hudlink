#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 The HUDLink authors (see AUTHORS).
#
# Tests of the USB scripts (usb/) on a fake head unit.
#
#   tests/run.sh [--factory DIR]
#
# Each case builds a fake head unit in a temporary folder (the files HUDLink
# reads and changes, a fake /proc, a fake dynamic loader and dialog) and a
# fake USB drive made by tools/make-usb.sh, then runs the scripts with only
# BusyBox and the fakes on the PATH: the shell and tools the head unit has.
# Needs Linux with busybox; tools/test.sh runs it in the build container.
#
#   --factory DIR   also install and uninstall on copies of real factory
#                   files: DIR/sm.conf and DIR/sm_WCP.conf from your own head
#                   unit's backup, and DIR/expected.md5 with the md5 each must
#                   have once installed ("<md5>  sm.conf" lines). Mazda's
#                   files are not part of this repository; never commit them.
#
# ok and bad always succeed, so "check && ok || bad" is an if-then-else.
# shellcheck disable=SC2015

set -u

repo=$(cd "$(dirname "$0")/.." && pwd)
fixtures="${repo}/tests/fixtures"
factory=""
while [ $# -gt 0 ]; do
  case "$1" in
    --factory) factory=${2:-}; shift 2 ;;
    *) echo "usage: tests/run.sh [--factory DIR]" >&2; exit 2 ;;
  esac
done

# BusyBox for the scripts, and a static one for the fake loader: the scripts
# set LD_PRELOAD and LD_TRACE_LOADED_OBJECTS for the loader they test with,
# which a dynamically linked interpreter would obey itself. See
# tests/Dockerfile.
busybox=$(command -v busybox) || { echo "tests need busybox (run tools/test.sh)" >&2; exit 2; }
static=${STATIC_BUSYBOX:-/opt/busybox-static/busybox}
[ -x "$static" ] && [ "$(LD_TRACE_LOADED_OBJECTS=1 "$static" echo static 2>&1)" = static ] ||
  { echo "tests need a static busybox at ${static} (run tools/test.sh)" >&2; exit 2; }

T=$(mktemp -d "${TMPDIR:-/tmp}/hudlink-test.XXXXXX")
trap 'rm -rf -- "$T"' EXIT
R="${T}/root"          # the fake head unit
U="${T}/usb"           # the fake USB drive
B="${T}/busybox"       # BusyBox applets
F="${T}/fake"          # fake programs of the head unit
D="${R}/data_persist/cp-hud-mod"
LIBPATH="/jci/lib:/jci/opera/3rdpartylibs/freetype:/usr/lib/imx-mm/audio-codec:/usr/lib/imx-mm/parser:/data_persist/dev/lib:"
SO_NAME=libpatch-blmjcicarplay.so

pass=0
fail=0
ok()  { pass=$((pass + 1)); echo "    ok    $*"; }
bad() { fail=$((fail + 1)); echo "    FAIL  $*"; }
case_() { echo; echo "--- $*"; }

# ---------------------------------------------------------------------------
# BusyBox and the fake programs
# ---------------------------------------------------------------------------

mkdir -p "$B" "$F"
for a in $("$busybox" --list); do ln -s "$busybox" "${B}/${a}"; done

# fake <name>: write a fake program from stdin. @T@ is the test folder, @B@
# the BusyBox applets, and @SH@ the static shell the fakes run with.
fake() {
  sed -e "s#@T@#${T}#g" -e "s#@B@#${B}#g" -e "s#@SH@#${static} sh#g" > "$1"
  chmod 755 "$1"
}

for p in hwclock killall; do printf '#!@SH@\nexit 0\n' | fake "${F}/${p}"; done
fake "${F}/mount" <<'EOF'
#!@SH@
echo "mount $*" >> @T@/mounts
EOF
fake "${F}/reboot" <<'EOF'
#!@SH@
echo "reboot" >> @T@/events
EOF
fake "${F}/dmesg" <<'EOF'
#!@SH@
echo "[    1.000000] fake kernel message"
EOF
fake "${F}/df" <<'EOF'
#!@SH@
echo "Filesystem 1024-blocks Used Available Capacity Mounted on"
echo "/dev/root 300000 200000 $(cat @T@/dffree 2>/dev/null || echo 50000) 80% /"
EOF
# mv that fails when its target's name is in @T@/mvfail (a failed write).
fake "${F}/mv" <<'EOF'
#!@SH@
for last in "$@"; do :; done
if [ -f @T@/mvfail ] && [ "$(basename "$last")" = "$(cat @T@/mvfail)" ]
then
  echo "mv: simulated failure" >&2
  exit 1
fi
exec @B@/mv "$@"
EOF

# ---------------------------------------------------------------------------
# The fake head unit
# ---------------------------------------------------------------------------

# The on-screen dialog: logs what it shows; questions take the next answer
# (the button number) from @T@/answers.
dialog() {
  fake "$1" <<'EOF'
#!@SH@
text=""
for a in "$@"; do case "$a" in --text=*) text=${a#--text=} ;; esac; done
case " $* " in
  *" --info "*) echo "info: $text" >> @T@/events; exit 0 ;;
esac
answer=$(head -n 1 @T@/answers)
sed -i 1d @T@/answers
echo "ask: $text => ${answer:-none}" >> @T@/events
exit "${answer:-9}"
EOF
}

# The dynamic loader, tracing only. Mode in @T@/ldmode:
#   good       everything resolves
#   version    the bridge needs a symbol version the unit lacks
#   pathdiff   with the bridge, one CarPlay library resolves elsewhere
#   crash      the load test program crashes with the bridge
loader() {
  fake "$1" <<'EOF'
#!@SH@
mode=$(cat @T@/ldmode 2>/dev/null || echo good)
[ -n "${LD_TRACE_LOADED_OBJECTS:-}" ] || { echo "fake loader: tracing only" >&2; exit 1; }
echo "$1 LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-} LD_PRELOAD=${LD_PRELOAD:-}" >> @T@/ldcalls
case "$1" in
  *libpatch-blmjcicarplay.so|*libpatch-blmjcicarplay.so.next)
    for l in libpthread.so.0 libdl.so.2 libstdc++.so.6 libm.so.6 libgcc_s.so.1 libc.so.6
    do
      printf '\t%s => /lib/%s (0x4%05x00)\n' "$l" "$l" "$$"
    done
    printf '\t/lib/ld-linux.so.3 (0x40000000)\n'
    [ "$mode" = version ] &&
      echo "$1: /lib/libgcc_s.so.1: version \`GCC_3.3.1' not found (required by $1)"
    exit 0 ;;
  *blmjcicarplay.so)
    [ -n "${LD_PRELOAD:-}" ] && printf '\t%s (0x4%05x00)\n' "$LD_PRELOAD" "$$"
    i=1
    while [ $i -le 44 ]
    do
      d=/jci/lib
      [ $i -gt 40 ] && d=/lib
      [ "$mode" = pathdiff ] && [ $i = 7 ] && [ -n "${LD_PRELOAD:-}" ] && d=/usr/lib
      printf '\tlib%02d.so => %s/lib%02d.so (0x4%05x00)\n' "$i" "$d" "$i" "$$"
      i=$((i + 1))
    done
    exit 0 ;;
esac
exit 1
EOF
}

# /bin/true for the load test.
true_bin() {
  fake "$1" <<'EOF'
#!@SH@
echo "true LD_PRELOAD=${LD_PRELOAD:-}" >> @T@/ldcalls
if [ "$(cat @T@/ldmode 2>/dev/null)" = crash ] && [ -n "${LD_PRELOAD:-}" ]
then
  echo "Segmentation fault" >&2
  exit 139
fi
exit 0
EOF
}

# carplay_proc <LD_LIBRARY_PATH>: the running CarPlay service.
carplay_proc() {
  mkdir -p "${R}/proc/1234"
  printf '/jci/sm/sm_svclauncher\0-l\0jciCARPLAY\0/jci/carplay/blmjcicarplay.so\0' > "${R}/proc/1234/cmdline"
  printf 'PATH=/bin:/usr/bin\0LD_LIBRARY_PATH=%s\0HOME=/\0' "$1" > "${R}/proc/1234/environ"
  echo '40000000-40010000 r-xp 00000000 1f:03 123 /jci/carplay/blmjcicarplay.so' > "${R}/proc/1234/maps"
}

# new_unit [DIR]: a fresh fake head unit with the factory files (from DIR,
# default the fixtures) and a fresh USB drive.
new_unit() {
  local confs=${1:-$fixtures}
  rm -rf -- "$R" "$U"
  rm -f "${T}/ldmode" "${T}/mvfail" "${T}/dffree" "${T}/mounts"
  mkdir -p "${R}/jci/sm" "${R}/jci/tools" "${R}/jci/scripts" "${R}/jci/carplay" \
           "${R}/etc" "${R}/data_persist" "${R}/tmp" "${R}/lib" "${R}/bin" \
           "${R}/sys/class/gpio/Watchdog Disable" "${R}/proc/1"
  echo 0 > "${R}/sys/class/gpio/Watchdog Disable/value"
  cp "${confs}/sm.conf" "${confs}/sm_WCP.conf" "${R}/jci/sm/"
  cp "${fixtures}/devmgr_config_master.xml" "${R}/etc/"
  chmod 640 "${R}/etc/devmgr_config_master.xml"
  cp "${fixtures}/version.ini" "${R}/jci/"
  printf '#!%s sh\nexit 1\n' "$static" > "${R}/jci/scripts/get_board_type.sh"
  chmod 755 "${R}/jci/scripts/get_board_type.sh"
  echo "the CarPlay program" > "${R}/jci/carplay/blmjcicarplay.so"
  dialog "${R}/jci/tools/jci-dialog"
  loader "${R}/lib/ld-linux.so.3"
  true_bin "${R}/bin/true"
  printf '/sbin/init\0' > "${R}/proc/1/cmdline"
  printf 'PATH=/bin\0' > "${R}/proc/1/environ"
  carplay_proc "$LIBPATH"
  echo "/dev/sda1 ${U} vfat ro 0 0" > "${R}/proc/mounts"
  make_usb "bridge build 1"
}

# make_usb <content>: the fake USB drive, with a fake bridge of <content>.
make_usb() {
  echo "$1" > "${T}/bridge.so"
  "${repo}/tools/make-usb.sh" --units metric --so "${T}/bridge.so" --out "$U" > "${T}/make-usb.log" 2>&1 ||
    { cat "${T}/make-usb.log"; echo "make-usb.sh failed" >&2; exit 1; }
  # Point the scripts at the fake head unit (their one ROOT line).
  local s
  for s in launcher.sh hudlink.sh hudlink-logs.sh; do
    sed -i "s#^ROOT=\"\"\$#ROOT=\"${R}\"#" "${U}/${s}"
    grep -q "^ROOT=\"${R}\"\$" "${U}/${s}" || { echo "could not set ROOT in ${s}" >&2; exit 1; }
    echo "$(md5sum "${U}/${s}" | cut -d' ' -f1)  ${s}" > "${U}/${s}.md5"
  done
}

# Everything on the head unit except the fake programs and /proc: names,
# permissions, sizes and md5 sums.
snap() {
  (cd "$R" && find . -path ./proc -prune -o -path ./sys -prune -o -print | sort | while read -r p; do
     if [ -f "$p" ]; then echo "$(stat -c '%a %s' "$p") $(md5sum < "$p" | cut -c1-32) $p"
     else echo "$(stat -c '%a' "$p") dir $p"; fi
   done)
}

# run <script> [answers...]: run a USB script as the head unit would.
run() {
  local script=$1; shift
  : > "${T}/events"
  : > "${T}/ldcalls"
  printf '%s\n' "$@" > "${T}/answers"
  env -i PATH="${F}:${B}" HOME=/ "${B}/sh" "${U}/${script}" > "${T}/stdout" 2>&1
  STATUS=$?
  sleep 0.2   # let the messages shown in the background land
}

# --- checks
expect_status() { [ "$STATUS" = "$1" ] && ok "exit status $1" || bad "exit status ${STATUS}, expected $1"; }
expect_shown() { grep -qF -- "$1" "${T}/events" && ok "shows \"$1\"" || { bad "does not show \"$1\""; sed 's/^/          /' "${T}/events"; }; }
expect_not_shown() { grep -qF -- "$1" "${T}/events" && bad "shows \"$1\"" || ok "does not show \"$1\""; }
expect_same() {   # expect_same <snapshot file> <what>
  snap > "${T}/now"
  if cmp -s "$1" "${T}/now"; then ok "$2"
  else bad "$2"; diff "$1" "${T}/now" | head -n 8 | sed 's/^/          /'; fi
}
expect_file() {   # expect_file <head unit file> <expected file>
  cmp -s "${R}$1" "$2" && ok "$1 is as expected" || { bad "$1 differs"; diff "$2" "${R}$1" | head -n 6 | sed 's/^/          /'; }
}
expect_absent() { [ ! -e "${R}$1" ] && ok "$1 is absent" || bad "$1 exists"; }
expect_present() { [ -e "${R}$1" ] && ok "$1 exists" || bad "$1 is missing"; }
expect_no_temp_files() {
  local n
  n=$(find "$R" \( -name '*.swaptmp' -o -name '*.next' -o -name 'hudlink.*' \) | wc -l)
  [ "$n" = 0 ] && ok "no temporary files left" || { bad "${n} temporary files left"; find "$R" \( -name '*.swaptmp' -o -name '*.next' -o -name 'hudlink.*' \); }
}
navi_is() { grep -qF "<name>NaviSupported</name><value>$1</value>" "${R}/etc/devmgr_config_master.xml"; }

refused() {   # refused <message> : the installer refused and changed nothing
  expect_status 1
  expect_shown "$1"
  expect_shown "NOTHING was changed"
  expect_same "${T}/before" "head unit unchanged"
  expect_no_temp_files
}

install_ok() { run hudlink.sh 0; [ "$STATUS" = 2 ] || { echo "install failed:"; cat "${T}/events"; }; }

# ---------------------------------------------------------------------------
# Cases
# ---------------------------------------------------------------------------

case_ "Exit changes nothing"
new_unit; snap > "${T}/before"
run hudlink.sh 2
expect_status 0
expect_same "${T}/before" "head unit unchanged"
grep -q "rw,remount ${U}" "${T}/mounts" && ok "remounts the USB drive writable for the log" || bad "does not remount the USB drive"

case_ "A damaged installer is refused"
new_unit; echo "# changed" >> "${U}/hudlink.sh"; snap > "${T}/before"
run hudlink.sh 0
refused "hudlink.sh on the USB drive does not match its md5 file"

case_ "A damaged bridge on the USB drive is refused"
new_unit; echo x >> "${U}/pkg/hudlink/${SO_NAME}"; snap > "${T}/before"
run hudlink.sh 0
refused "does not match the file list (md5)"

case_ "A damaged file list is refused"
new_unit; echo "00000000000000000000000000000000 extra" >> "${U}/pkg/hudlink/manifest.txt"; snap > "${T}/before"
run hudlink.sh 0
refused "does not match its md5 file"

case_ "Other firmware is refused"
new_unit; sed -i 's/74\.00\.324/74.00.330/' "${R}/jci/version.ini"; snap > "${T}/before"
run hudlink.sh 0
refused "HUDLink needs CMU firmware 74.00.324"

case_ "A bridge that needs a missing symbol version is refused"
new_unit; echo version > "${T}/ldmode"; snap > "${T}/before"
run hudlink.sh 0
refused "The bridge does not fit this head unit"

case_ "A bridge that changes CarPlay's libraries is refused"
new_unit; echo pathdiff > "${T}/ldmode"; snap > "${T}/before"
run hudlink.sh 0
refused "CarPlay would not load"

case_ "A bridge that fails the load test is refused"
new_unit; echo crash > "${T}/ldmode"; snap > "${T}/before"
run hudlink.sh 0
refused "The bridge failed a load test"

case_ "An unknown CarPlay entry is refused (install and uninstall)"
new_unit
sed -i '/name="jciCARPLAY"/a\            <environ_var env_name="FOO" env_value="1"/>' "${R}/jci/sm/sm.conf"
snap > "${T}/before"
run hudlink.sh 0
refused "is not the factory one"
run hudlink.sh 1
refused "is not the factory one"

case_ "A config with CR line endings is refused"
new_unit; sed -i 's/$/\r/' "${R}/jci/sm/sm_WCP.conf"; snap > "${T}/before"
run hudlink.sh 0
refused "is not the factory one"

case_ "Two CarPlay entries are refused"
new_unit; entry=$(grep 'name="jciCARPLAY"' "${R}/jci/sm/sm.conf"); echo "$entry" >> "${R}/jci/sm/sm.conf"
snap > "${T}/before"
run hudlink.sh 0
refused "is not the factory one"

case_ "NaviSupported twice is refused"
new_unit
echo '    <param><name>NaviSupported</name><value>FALSE</value></param>' >> "${R}/etc/devmgr_config_master.xml"
snap > "${T}/before"
run hudlink.sh 0
refused "not in the expected form"

case_ "CarPlay running with another library path is refused"
new_unit; carplay_proc "/jci/lib:/usr/lib"; snap > "${T}/before"
run hudlink.sh 0
refused "library path HUDLink expects"

case_ "CarPlay not running is refused"
new_unit; rm -rf -- "${R}/proc/1234"; snap > "${T}/before"
run hudlink.sh 0
refused "library path HUDLink expects"

case_ "Too little free space is refused"
new_unit; echo 100 > "${T}/dffree"; snap > "${T}/before"
run hudlink.sh 0
refused "Not enough free space"

case_ "Install"
new_unit; snap > "${T}/factory"
run hudlink.sh 0
expect_status 2
expect_shown "HUDLink is installed"
expect_file /jci/sm/sm.conf "${fixtures}/installed/sm.conf"
expect_file /jci/sm/sm_WCP.conf "${fixtures}/installed/sm_WCP.conf"
navi_is TRUE && ok "NaviSupported is TRUE" || bad "NaviSupported is not TRUE"
sed 's#<name>NaviSupported</name><value>TRUE</value>#<name>NaviSupported</name><value>FALSE</value>#' \
    "${R}/etc/devmgr_config_master.xml" | cmp -s - "${fixtures}/devmgr_config_master.xml" &&
  ok "only NaviSupported changed in the device manager settings" || bad "the device manager settings changed more"
[ "$(stat -c %a "${R}/etc/devmgr_config_master.xml")" = 640 ] && ok "the settings file keeps its permissions" || bad "the settings file lost its permissions"
cmp -s "${D}/${SO_NAME}" "${U}/pkg/hudlink/${SO_NAME}" && ok "the bridge is in place" || bad "the bridge is not in place"
[ "$(stat -c %a "${D}/${SO_NAME}")" = 755 ] && ok "the bridge is mode 755" || bad "the bridge is not mode 755"
[ "$(cat "${R}/sys/class/gpio/Watchdog Disable/value")" = 1 ] && ok "the watchdog was disabled for the writes" || bad "the watchdog was not disabled"
[ "$(grep -c "LD_LIBRARY_PATH=${LIBPATH} " "${T}/ldcalls")" = 3 ] && ok "every loader test used the factory library path" || bad "a loader test used another library path"
grep -q "^build: units: metric" "${U}/logs/hudlink.log" && ok "the log names the build" || bad "the log does not name the build"
expect_no_temp_files

case_ "Install again: already installed"
snap > "${T}/before"
run hudlink.sh 0
expect_status 0
expect_shown "HUDLink is already installed"
expect_shown "starts after the next reboot"
expect_same "${T}/before" "head unit unchanged"

case_ "Install again after the reboot: running"
carplay_proc "$LIBPATH"
echo "40100000-40108000 r-xp 00000000 1f:05 77 /data_persist/cp-hud-mod/${SO_NAME}" >> "${R}/proc/1234/maps"
run hudlink.sh 0
expect_status 0
expect_shown "running inside CarPlay now"

case_ "Install after the bridge switched itself off: back on"
printf 'fatal signal 11\n' > "${D}/off-after-crash.txt"
run hudlink.sh 0
expect_status 2
expect_shown "switched itself off after a crash"
expect_absent /data_persist/cp-hud-mod/off-after-crash.txt
grep -q 'fatal signal 11' "${U}/logs/hudlink-crash.txt" && ok "the crash details are on the USB drive" || bad "the crash details are not on the USB drive"

case_ "Uninstall: back to the factory files"
for n in diag.log diag.1.log diag.2.log; do echo "log" > "${D}/${n}"; done
run hudlink.sh 1
expect_status 2
expect_shown "HUDLink is uninstalled"
expect_same "${T}/factory" "head unit byte for byte as before the install"
expect_no_temp_files
[ -f "${U}/logs/hudlink-diag.log" ] && ok "the diagnostic logs were saved first" || bad "the diagnostic logs were not saved"

case_ "Uninstall again: not installed"
snap > "${T}/before"
run hudlink.sh 1
expect_status 0
expect_shown "HUDLink is not installed"
expect_same "${T}/before" "head unit unchanged"

case_ "A write that fails half way puts the factory files back"
new_unit; snap > "${T}/factory"; echo sm_WCP.conf > "${T}/mvfail"
run hudlink.sh 0
expect_status 2
expect_shown "The factory files were put back"
expect_same "${T}/factory" "head unit back to the factory state"
expect_no_temp_files

case_ "A failed rename of the bridge puts the factory files back"
new_unit; snap > "${T}/factory"; echo "$SO_NAME" > "${T}/mvfail"
run hudlink.sh 0
expect_status 2
expect_shown "The factory files were put back"
expect_same "${T}/factory" "head unit back to the factory state"

case_ "Update to a new bridge"
new_unit; install_ok
make_usb "bridge build 2"
run hudlink.sh 0
expect_status 2
expect_shown "HUDLink is updated"
cmp -s "${D}/${SO_NAME}" "${U}/pkg/hudlink/${SO_NAME}" && ok "the new bridge is in place" || bad "the new bridge is not in place"
expect_file /jci/sm/sm.conf "${fixtures}/installed/sm.conf"
expect_no_temp_files

case_ "An update whose tests fail keeps the installed version"
new_unit; install_ok; snap > "${T}/before"
make_usb "bridge build 3"; echo crash > "${T}/ldmode"
run hudlink.sh 0
refused "The bridge failed a load test"

case_ "An uninstall whose write fails can be run again"
new_unit; snap > "${T}/factory"; install_ok; echo sm.conf > "${T}/mvfail"
run hudlink.sh 1
expect_status 2
expect_shown "Run Uninstall again"
expect_present /data_persist/cp-hud-mod/${SO_NAME}
rm -f "${T}/mvfail"
run hudlink.sh 1
expect_status 2
expect_shown "HUDLink is uninstalled"
expect_same "${T}/factory" "head unit back to the factory state"

case_ "NaviSupported that was already on stays on after Uninstall"
new_unit
sed -i 's#<name>NaviSupported</name><value>FALSE</value>#<name>NaviSupported</name><value>TRUE</value>#' "${R}/etc/devmgr_config_master.xml"
snap > "${T}/factory"
install_ok
expect_present /data_persist/cp-hud-mod/navi-was-on.txt
run hudlink.sh 1
expect_status 2
navi_is TRUE && ok "NaviSupported is still TRUE" || bad "NaviSupported was turned off"
expect_same "${T}/factory" "head unit back to the state before the install"

case_ "Save logs reads only"
new_unit; install_ok
echo "bridge log line" > "${R}/tmp/carplay_bridge.log"
echo "diag line" > "${D}/diag.log"
snap > "${T}/before"
run hudlink-logs.sh 0
expect_status 0
expect_shown "Logs saved to the USB drive"
expect_same "${T}/before" "head unit unchanged"
out=$(find "${U}/logs" -maxdepth 1 -name 'check-*' | sort | tail -n 1)
for f in hudlink-logs.log carplay_bridge.log bridge-diag.log processes.txt dmesg.txt; do
  [ -s "${out}/${f}" ] && ok "saved ${f}" || bad "did not save ${f}"
done
grep -q "^LD_LIBRARY_PATH=" "${out}/hudlink-logs.log" 2> /dev/null && ok "the log shows CarPlay's environment" || bad "the log lacks CarPlay's environment"

case_ "Save logs: Cancel"
snap > "${T}/before"
run hudlink-logs.sh 1
expect_status 0
expect_not_shown "Logs saved"
expect_same "${T}/before" "head unit unchanged"

case_ "Launcher: install, then reboot"
new_unit
run launcher.sh 0 0 1 0
expect_status 0
expect_shown "STEP 1 of 2: HUDLink"
expect_shown "HUDLink is installed"
expect_shown "The head unit needs a reboot"
grep -qx reboot "${T}/events" && ok "rebooted" || bad "did not reboot"
expect_file /jci/sm/sm.conf "${fixtures}/installed/sm.conf"

case_ "Launcher: nothing changed, no reboot"
new_unit; snap > "${T}/before"
run launcher.sh 1 1
expect_status 0
expect_shown "Nothing needs a reboot"
grep -qx reboot "${T}/events" && bad "rebooted" || ok "did not reboot"
expect_same "${T}/before" "head unit unchanged"

case_ "Launcher: Exit after an install still offers the reboot"
new_unit
run launcher.sh 0 0 2 1
expect_shown "The head unit needs a reboot"
grep -qx reboot "${T}/events" && bad "rebooted on Later" || ok "did not reboot on Later"

case_ "A damaged launcher is refused"
new_unit; echo "# changed" >> "${U}/launcher.sh"; snap > "${T}/before"
run launcher.sh 0 0
expect_status 1
expect_shown "launcher.sh on the USB drive does not match its md5 file"
expect_same "${T}/before" "head unit unchanged"

if [ -n "$factory" ]; then
  case_ "Factory files: install and uninstall (${factory})"
  new_unit "$factory"; snap > "${T}/factory"
  run hudlink.sh 0
  expect_status 2
  while read -r want name; do
    [ -z "$want" ] && continue
    got=$(md5sum < "${R}/jci/sm/${name}" | cut -c1-32)
    [ "$got" = "$want" ] && ok "installed ${name} has md5 ${want}" || bad "installed ${name} has md5 ${got}, expected ${want}"
  done < "${factory}/expected.md5"
  run hudlink.sh 1
  expect_status 2
  expect_same "${T}/factory" "head unit byte for byte as before the install"
fi

echo
echo "${pass} passed, ${fail} failed"
[ "$fail" = 0 ]
