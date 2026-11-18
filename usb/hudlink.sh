#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 The HUDLink authors (see AUTHORS).
#
# HUDLink installer for Mazda Connect CMU150, firmware 74.00.324.
#
# Installs or uninstalls the HUDLink bridge, which shows CarPlay turn-by-turn
# directions (Apple Maps, Google Maps) on the factory head-up display.
#
# What Install changes, and Uninstall puts back:
#   /data_persist/cp-hud-mod/libpatch-blmjcicarplay.so
#       the bridge (a new file)
#   /jci/sm/sm.conf, /jci/sm/sm_WCP.conf
#       the jciCARPLAY service entry only: it preloads the bridge, keeps the
#       service's factory library path, and a CarPlay fault restarts CarPlay
#       instead of rebooting the head unit (reset_board="no"). The rest of
#       each file is kept byte for byte.
#   /etc/devmgr_config_master.xml
#       NaviSupported FALSE -> TRUE, so the phone sends its directions
#
# Safety:
#   - nothing is changed before Install or Uninstall is chosen;
#   - this script and every file it brings are checked against their md5;
#   - the jciCARPLAY entry must be exactly the factory one or exactly the one
#     this script writes; anything else is left alone;
#   - every edited file is built in a temporary copy, checked (undoing the
#     edit must give the current file byte for byte), then renamed over the
#     original in one step;
#   - the new bridge is tested on this head unit before anything points at
#     it: its libraries and symbol versions, the CarPlay program's library
#     list with and without it, and a load test in a throwaway program. If a
#     test fails, NOTHING is changed;
#   - if a write fails, the factory state is put back.
#
# Run by launcher.sh. Exit status: 0 nothing changed, 1 failed and nothing
# changed, 2 something changed (a reboot is needed).
# Log: logs/hudlink.log on the USB drive.
# POSIX sh for the head unit's BusyBox. ASCII only, LF line endings.

# Empty on the head unit. The tests run this script against a fake head unit
# kept in a folder, by setting this one line.
ROOT=""

TITLE="HUDLink"
PKG_NAME="hudlink"
FILE_COUNT=2
SO_NAME="libpatch-blmjcicarplay.so"

# The bridge as the CarPlay process sees it. Written into the service entry.
BRIDGE_PATH="/data_persist/cp-hud-mod/${SO_NAME}"
# The library path the CarPlay service has from the factory. The service entry
# sets exactly this, so CarPlay keeps the same libraries whether the service
# manager adds the entry's variables to its environment or replaces it.
LIBPATH="/jci/lib:/jci/opera/3rdpartylibs/freetype:/usr/lib/imx-mm/audio-codec:/usr/lib/imx-mm/parser:/data_persist/dev/lib:"
# The two lines Install adds after the jciCARPLAY service line.
ENV_PRELOAD="            <environ_var env_name=\"LD_PRELOAD\" env_value=\"${BRIDGE_PATH}\"/>"
ENV_LIBPATH="            <environ_var env_name=\"LD_LIBRARY_PATH\" env_value=\"${LIBPATH}\"/>"
CONFS="sm.conf sm_WCP.conf"
NAV_OFF='<name>NaviSupported</name><value>FALSE</value>'
NAV_ON='<name>NaviSupported</name><value>TRUE</value>'
CMU_VERSION="74.00.324"

DIR="${ROOT}/data_persist/cp-hud-mod"
SO="${DIR}/${SO_NAME}"
NEXT="${SO}.next"
OFF="${DIR}/off-after-crash.txt"
NAV_KEEP="${DIR}/navi-was-on.txt"
SM_DIR="${ROOT}/jci/sm"
XML="${ROOT}/etc/devmgr_config_master.xml"
CP_LIB="${ROOT}/jci/carplay/blmjcicarplay.so"
VERSION_INI="${ROOT}/jci/version.ini"
BOARD_TYPE="${ROOT}/jci/scripts/get_board_type.sh"
DIALOG="${ROOT}/jci/tools/jci-dialog"
WATCHDOG="${ROOT}/sys/class/gpio/Watchdog Disable/value"
LDSO="${ROOT}/lib/ld-linux.so.3"
TRUE_BIN="${ROOT}/bin/true"
PROC="${ROOT}/proc"
BRIDGE_LOG="${ROOT}/tmp/carplay_bridge.log"
WORK="${ROOT}/tmp/hudlink.$$"

hwclock --hctosys

ME=$(readlink -f "$0")
ME_NAME=${ME##*/}
USB_DIR=${ME%/*}
PKG="${USB_DIR}/pkg/${PKG_NAME}"
MAN="${PKG}/manifest.txt"
LOGS="${USB_DIR}/logs"
LOG="${LOGS}/${ME_NAME%.sh}.log"

# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------

file_md5() { md5sum "$1" 2>/dev/null | awk '{ print $1 }'; }
# The md5 an md5 file records ("<md5>  <name>").
recorded_md5() { awk 'NR == 1 { print $1 }' "$1" 2>/dev/null; }
# matches_md5 <file> <md5>: the file is there and has that md5.
matches_md5() { [ -n "$2" ] && [ "$(file_md5 "$1")" = "$2" ]; }
pkg_md5() { awk -v n="$1" '$2 == n { print $1 }' "$MAN"; }
count_in() { grep -cF "$1" "$2" 2>/dev/null; }

# The mount point of the file system that holds folder $1: the longest mount
# point in /proc/mounts that contains it.
mount_point_of() {
  [ -r "${PROC}/mounts" ] || { echo /; return; }
  awk -v dir="$1" '
    $2 == "/" || $2 == dir || index(dir, $2 "/") == 1 {
      if (length($2) > length(best)) best = $2
    }
    END { print (best == "" ? "/" : best) }' "${PROC}/mounts"
}

# On-screen dialogs, drawn by the head unit's jci-dialog. Each one closes the
# dialog on screen first. jci_dialog keeps the argument order the head unit
# was tested with: the kind of dialog, the title, the text, then the rest.
jci_dialog() {
  _dlg_kind=$1
  _dlg_text=$2
  shift 2
  "$DIALOG" "$_dlg_kind" --title="$TITLE" --text="$_dlg_text" "$@"
}
# note <text>: a message that stays up while work goes on.
note() { killall -q jci-dialog; jci_dialog --info "$1" --no-cancel & }
# message <text>: a message that waits for OK.
message() { killall -q jci-dialog; jci_dialog --info "$1" --no-cancel; }
# choose <text> <button 0> <button 1> [<button 2>]: returns the button pressed.
choose() {
  killall -q jci-dialog
  if [ $# -ge 4 ]
  then
    jci_dialog --3-button-dialog "$1" --ok-label="$2" --cancel-label="$3" --button3-label="$4"
  else
    jci_dialog --question "$1" --ok-label="$2" --cancel-label="$3"
  fi
}

# Copies of the bridge's logs to the USB drive (only read on the head unit).
save_logs() {
  [ -f "$BRIDGE_LOG" ] && cp "$BRIDGE_LOG" "${LOGS}/carplay_bridge.log"
  for _l in diag.log diag.1.log diag.2.log
  do
    [ -f "${DIR}/${_l}" ] && cp "${DIR}/${_l}" "${LOGS}/hudlink-${_l}"
  done
  sync
}

# finish <exit status> <message>: log, show the message, and exit.
finish() {
  echo "$2" | sed 's/\\n/ /g'
  save_logs
  echo "${TITLE} finished, exit status $1"
  sync
  message "$2"
  exit "$1"
}

# ----------------------------------------------------------------------------
# The log, on the USB drive
# ----------------------------------------------------------------------------

# The USB drive is mounted read-only: remount it read-write for the log.
mount -o rw,remount "$(mount_point_of "$USB_DIR")"
mkdir -p "$LOGS"
exec > "$LOG" 2>&1
echo "${TITLE} ${ME_NAME}, $(date)"

mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT

# ----------------------------------------------------------------------------
# The jciCARPLAY service entry
# ----------------------------------------------------------------------------

# conf_add <in> <out>: the factory entry -> the HUDLink entry.
conf_add() {
  awk -v pre="$ENV_PRELOAD" -v lib="$ENV_LIBPATH" '
    /<service / && /name="jciCARPLAY"/ {
      n++
      if (gsub(/reset_board="yes"/, "reset_board=\"no\"") != 1) bad = 1
      print
      print pre
      print lib
      next
    }
    { print }
    END { if (n != 1 || bad) exit 1 }' "$1" > "$2"
}

# conf_remove <in> <out>: the HUDLink entry -> the factory entry.
conf_remove() {
  awk -v pre="$ENV_PRELOAD" -v lib="$ENV_LIBPATH" '
    want == 1 { if ($0 != pre) bad = 1; want = 2; next }
    want == 2 { if ($0 != lib) bad = 1; want = 0; next }
    /<service / && /name="jciCARPLAY"/ {
      n++
      if (gsub(/reset_board="no"/, "reset_board=\"yes\"") != 1) bad = 1
      want = 1
    }
    { print }
    END { if (n != 1 || bad || want) exit 1 }' "$1" > "$2"
}

# Number of environ_var lines in the jciCARPLAY entry of <file>.
entry_vars() {
  awk '
    /<service / && /name="jciCARPLAY"/ { inside = 1; next }
    inside && /<\/service>/ { inside = 0 }
    inside && /environ_var/ { n++ }
    END { print n + 0 }' "$1"
}

# conf_state <name>: orig (the factory entry) | new (exactly the HUDLink
# entry) | other. Both directions are checked: the edit must undo to the file
# it came from, byte for byte.
conf_state() {
  _f="${SM_DIR}/$1"
  _o="${WORK}/$1.orig"
  _n="${WORK}/$1.new"
  rm -f "$_o" "$_n"
  if [ ! -f "$_f" ] || [ "$(tr -d '\r' < "$_f" | md5sum | cut -d' ' -f1)" != "$(file_md5 "$_f")" ]
  then
    echo other
  elif grep -qF "$SO_NAME" "$_f"
  then
    if conf_remove "$_f" "$_o" && ! grep -qF "$SO_NAME" "$_o" &&
       [ "$(entry_vars "$_o")" = "0" ] &&
       conf_add "$_o" "$_n" && [ "$(file_md5 "$_n")" = "$(file_md5 "$_f")" ]
    then
      echo new
    else
      echo other
    fi
  else
    if [ "$(entry_vars "$_f")" = "0" ] &&
       conf_add "$_f" "$_n" && conf_remove "$_n" "$_o" &&
       [ "$(file_md5 "$_o")" = "$(file_md5 "$_f")" ]
    then
      echo orig
    else
      echo other
    fi
  fi
}

# ----------------------------------------------------------------------------
# What is on the head unit now
# ----------------------------------------------------------------------------

# devmgr master file: off | on | missing | other
xml_state() {
  if [ ! -f "$XML" ]; then echo missing; return; fi
  _f=$(count_in "$NAV_OFF" "$XML")
  _t=$(count_in "$NAV_ON" "$XML")
  if [ "$_f" = "1" ] && [ "$_t" = "0" ]; then echo off
  elif [ "$_f" = "0" ] && [ "$_t" = "1" ]; then echo on
  else echo other; fi
}

# bridge file: none | same (the one on the USB drive) | different
so_state() {
  if [ ! -e "$SO" ]; then echo none
  elif [ "$(file_md5 "$SO")" = "$SO_MD5" ]; then echo same
  else echo different; fi
}

# PID of the running CarPlay service (the [r] keeps grep from finding itself).
cp_pid() {
  for _p in "${PROC}"/[0-9]*
  do
    if tr '\000' ' ' < "${_p}/cmdline" 2>/dev/null | grep -q 'sm_svclaunche[r].*jciCARPLAY'
    then
      basename "$_p"
      return 0
    fi
  done
  return 1
}

# LD_LIBRARY_PATH of the running CarPlay service (empty if not running).
cp_libpath() {
  _pid=$(cp_pid) || return 0
  tr '\000' '\n' < "${PROC}/${_pid}/environ" 2>/dev/null |
    grep '^LD_LIBRARY_PATH=' | head -n 1 | cut -d= -f2-
}

read_state() {
  STATE_SM=$(conf_state sm.conf)
  STATE_WCP=$(conf_state sm_WCP.conf)
  STATE_XML=$(xml_state)
  STATE_SO=$(so_state)
  echo "state: sm.conf=${STATE_SM} sm_WCP.conf=${STATE_WCP} devmgr=${STATE_XML}" \
       "bridge=${STATE_SO} off-after-crash=$([ -f "$OFF" ] && echo yes || echo no)"
}

running_text() {
  _pid=$(cp_pid)
  if [ -n "$_pid" ] && grep -qF "$SO_NAME" "${PROC}/${_pid}/maps" 2>/dev/null
  then
    echo "The bridge is running inside CarPlay now."
  else
    echo "The bridge starts after the next reboot."
  fi
}

# ----------------------------------------------------------------------------
# Changing the head unit
# ----------------------------------------------------------------------------

# Make the root file system writable, once. The hardware watchdog is switched
# off first, as other head unit mods do before writing there.
ROOT_RW=no
root_writable() {
  [ "$ROOT_RW" = yes ] && return 0
  echo 1 > "$WATCHDOG"
  mount -o rw,remount /
  ROOT_RW=yes
}

free_kb() { df -Pk "$1" 2>/dev/null | tail -n 1 | awk '{ print $(NF-2) }'; }

need_space() {
  _a=$(free_kb "$1")
  echo "free space on $1: ${_a} KB, needed: $2 KB"
  case "$_a" in
    ''|*[!0-9]*) finish 1 "Could not read the free space.\nNOTHING was changed." ;;
  esac
  if [ "$_a" -lt "$2" ]
  then
    finish 1 "Not enough free space on the head unit\n($1: ${_a} KB free, $2 KB needed).\nNOTHING was changed."
  fi
}

# swap_one <target> <source> <md5>: replace one file. The new content goes
# into a temporary copy that keeps the original's owner and permissions, is
# checked against <md5>, then renamed over the original.
swap_one() {
  _tmp="${1}.swaptmp"
  rm -f "$_tmp"
  cp -a "$1" "$_tmp" || { rm -f "$_tmp"; sync; return 1; }
  cat "$2" > "$_tmp"
  sync
  if [ "$(file_md5 "$_tmp")" != "$3" ]
  then
    rm -f "$_tmp"; sync; return 1
  fi
  mv -f "$_tmp" "$1" || { rm -f "$_tmp"; sync; return 1; }
  sync
  [ "$(file_md5 "$1")" = "$3" ]
}

# conf_to <name> orig|new: bring one sm config to that state (one retry).
conf_to() {
  _f="${SM_DIR}/$1"
  _s=$(conf_state "$1")
  [ "$_s" = "$2" ] && return 0
  if [ "$_s" = "other" ]
  then
    echo "FAILED: ${_f} is not in a known form"; return 1
  fi
  _want="${WORK}/$1.want"
  if [ "$2" = "new" ]; then conf_add "$_f" "$_want"; else conf_remove "$_f" "$_want"; fi ||
    { echo "FAILED: could not edit ${_f}"; return 1; }
  _m=$(file_md5 "$_want")
  swap_one "$_f" "$_want" "$_m" || swap_one "$_f" "$_want" "$_m" ||
    { echo "FAILED: ${_f} -> $2"; return 1; }
  [ "$(conf_state "$1")" = "$2" ] || { echo "FAILED: check of ${_f}"; return 1; }
  echo "OK $2: ${_f}"
}

# xml_set on|off: change NaviSupported. Exactly that one value changes: the
# copy must have the wanted value once, the other one nowhere, the same number
# of lines, and changing it back must give the current file byte for byte.
xml_set() {
  if [ "$1" = "on" ]; then _from=$NAV_OFF; _to=$NAV_ON; else _from=$NAV_ON; _to=$NAV_OFF; fi
  [ "$(xml_state)" = "$1" ] && return 0
  _tmp="${XML}.swaptmp"
  rm -f "$_tmp"
  cp -a "$XML" "$_tmp" || { rm -f "$_tmp"; sync; return 1; }
  sed "s#${_from}#${_to}#" "$XML" > "$_tmp" || { rm -f "$_tmp"; sync; return 1; }
  sync
  if [ "$(count_in "$_to" "$_tmp")" != "1" ] || [ "$(count_in "$_from" "$_tmp")" != "0" ] ||
     [ "$(grep -c '' "$_tmp")" != "$(grep -c '' "$XML")" ] ||
     [ "$(sed "s#${_to}#${_from}#" "$_tmp" | md5sum | cut -d' ' -f1)" != "$(file_md5 "$XML")" ]
  then
    echo "FAILED: check of the new ${XML}"; rm -f "$_tmp"; sync; return 1
  fi
  mv -f "$_tmp" "$XML" || { rm -f "$_tmp"; sync; return 1; }
  sync
  [ "$(xml_state)" = "$1" ] || { echo "FAILED: ${XML} -> $1"; return 1; }
  echo "OK NaviSupported $1: ${XML}"
}

# Copy the new bridge next to the installed one under a temporary name
# (nothing loads it from there).
so_stage() {
  mkdir -p "$DIR" || return 1
  rm -f "$NEXT"
  if cat "${PKG}/${SO_NAME}" > "$NEXT" && chmod 755 "$NEXT" && sync &&
     [ "$(file_md5 "$NEXT")" = "$SO_MD5" ]
  then
    echo "OK bridge copied for the tests: ${NEXT}"
    return 0
  fi
  rm -f "$NEXT"; sync; return 1
}

# After the tests the tested copy takes the real name in one step. A running
# CarPlay keeps the file it loaded until the reboot.
so_commit() {
  if [ "$(file_md5 "$NEXT")" = "$SO_MD5" ] && mv -f "$NEXT" "$SO" && sync &&
     [ "$(file_md5 "$SO")" = "$SO_MD5" ]
  then
    echo "OK bridge in place: ${SO}"
    return 0
  fi
  return 1
}

# Remove everything this script put in its folder (only while no service
# entry loads the bridge).
so_remove() {
  rm -f "$SO" "$NEXT" "$OFF" "$NAV_KEEP" "${DIR}/diag.log" "${DIR}/diag.1.log" "${DIR}/diag.2.log"
  rmdir "$DIR" 2>/dev/null
  sync
  [ ! -e "$SO" ]
}

# ----------------------------------------------------------------------------
# Tests of the new bridge on this head unit
# ----------------------------------------------------------------------------

# trace_libs <program> <LD_LIBRARY_PATH> <LD_PRELOAD>: the libraries the
# dynamic loader would load for <program> (nothing is run), without load
# addresses, sorted. An empty variable is left unset.
trace_libs() {
  (
    unset LD_LIBRARY_PATH LD_PRELOAD
    if [ -n "$2" ]; then LD_LIBRARY_PATH=$2; export LD_LIBRARY_PATH; fi
    if [ -n "$3" ]; then LD_PRELOAD=$3; export LD_PRELOAD; fi
    LD_TRACE_LOADED_OBJECTS=1 "$LDSO" "$1" 2>&1
  ) | sed -e 's/(0x[0-9a-fA-F]*)//' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' | sort
}

# Test the staged copy $NEXT with the CarPlay service's factory library path.
# Changes nothing on the head unit. On a problem sets BAD and returns 1.
gates() {
  BAD=""
  _now="${LOGS}/hudlink-libs-now.txt"
  _with="${LOGS}/hudlink-libs-with-bridge.txt"
  _rest="${WORK}/libs-with-bridge-without-it.txt"
  echo "tests use the factory LD_LIBRARY_PATH=${LIBPATH}"

  # 1. Every library and symbol version the bridge needs is on this head unit
  #    (like "ldd -r").
  _out=$(LD_LIBRARY_PATH="$LIBPATH" LD_TRACE_LOADED_OBJECTS=1 LD_BIND_NOW=1 LD_WARN=1 "$LDSO" "$NEXT" 2>&1)
  _rc=$?
  echo "--- test 1 (rc=${_rc}):"
  echo "$_out"
  _bad=$(echo "$_out" | grep -iE 'not found|not defined|undefined|error|cannot|no such|not a dynamic|wrong' | head -n 1)
  if [ "$_rc" != "0" ] || [ -n "$_bad" ] ||
     ! echo "$_out" | grep -q 'libstdc++.so.6 => ' ||
     ! echo "$_out" | grep -q 'libgcc_s.so.1 => ' ||
     ! echo "$_out" | grep -q 'libc.so.6 => '
  then
    BAD="The bridge does not fit this head unit:\n${_bad:-library test failed (code ${_rc})}"
    return 1
  fi

  # 2. With the bridge preloaded, the CarPlay program loads exactly the
  #    libraries it loads today, plus the bridge itself.
  trace_libs "$CP_LIB" "$LIBPATH" "" > "$_now"
  trace_libs "$CP_LIB" "$LIBPATH" "$NEXT" > "$_with"
  grep -v -F "$SO_NAME" "$_with" > "$_rest"
  _n=$(grep -c ' => /' "$_now")
  echo "--- test 2: CarPlay libraries today: ${_n}, with the bridge: $(grep -c ' => /' "$_with") + the bridge"
  if [ "$_n" -lt 20 ] || grep -qiE 'not found|error|cannot' "$_with" ||
     [ "$(grep -c -F "$SO_NAME" "$_with")" != "1" ] ||
     [ "$(file_md5 "$_rest")" != "$(file_md5 "$_now")" ]
  then
    BAD="With the bridge, CarPlay would not load\nthe same libraries as today."
    return 1
  fi

  # 3. Loading it into a small throwaway program works: its start-up code
  #    runs and, outside CarPlay, stays inert and silent.
  _out=$(LD_LIBRARY_PATH="$LIBPATH" LD_PRELOAD="$NEXT" "$TRUE_BIN" 2>&1)
  _rc=$?
  echo "--- test 3 (rc=${_rc}): ${_out}"
  if [ "$_rc" != "0" ] || [ -n "$_out" ]
  then
    BAD="The bridge failed a load test (code ${_rc}):\n$(echo "$_out" | head -n 1)"
    return 1
  fi
  echo "--- all tests passed"
  return 0
}

# Back to the factory state: the service entries first, then NaviSupported
# (back off unless it was on before HUDLink), and the bridge's folder last,
# once nothing points at it. A failed step stops there, so the folder stays
# until everything else is back. Returns 1 if that failed too.
to_factory() {
  _b=0
  for _c in $CONFS
  do
    conf_to "$_c" orig || _b=1
  done
  if [ "$_b" = "0" ] && [ ! -f "$NAV_KEEP" ] && [ "$(xml_state)" = "on" ]
  then
    xml_set off || xml_set off || _b=1
  fi
  if [ "$_b" = "0" ]
  then
    so_remove || _b=1
  fi
  rm -f "$NEXT"
  sync
  return $_b
}

# ----------------------------------------------------------------------------
# Checks before anything is offered
# ----------------------------------------------------------------------------

if ! matches_md5 "$ME" "$(recorded_md5 "${ME}.md5")"
then
  finish 1 "${ME_NAME} on the USB drive does not match its md5 file.\nNOTHING was changed.\nCopy the HUDLink files again."
fi
echo "md5 checked: ${ME_NAME}"

CMU_VER=$(grep '^JCI_SW_VER=' "$VERSION_INI" 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)
echo "firmware: ${CMU_VER}"
if [ "$CMU_VER" != "$CMU_VERSION" ]
then
  finish 1 "HUDLink needs CMU firmware ${CMU_VERSION}.\nThis head unit has: ${CMU_VER:-unknown}\nNOTHING was changed."
fi

if ! matches_md5 "$MAN" "$(recorded_md5 "${MAN}.md5")"
then
  finish 1 "The HUDLink file list on the USB drive\ndoes not match its md5 file.\nNOTHING was changed.\nCopy the HUDLink files again."
fi
if [ "$(grep -c . "$MAN")" != "$FILE_COUNT" ]
then
  finish 1 "The HUDLink file list on the USB drive\nis incomplete.\nNOTHING was changed.\nCopy the HUDLink files again."
fi
while read -r _m _f
do
  [ -z "$_m" ] && continue
  if ! matches_md5 "${PKG}/${_f}" "$_m"
  then
    finish 1 "${_f} on the USB drive\ndoes not match the file list (md5).\nNOTHING was changed.\nCopy the HUDLink files again."
  fi
done < "$MAN"
SO_MD5=$(pkg_md5 "$SO_NAME")
if [ -z "$SO_MD5" ]
then
  finish 1 "The HUDLink file list on the USB drive\nhas no bridge.\nNOTHING was changed.\nCopy the HUDLink files again."
fi
echo "md5 checked: the file list and its ${FILE_COUNT} files"
sed 's/^/build: /' "${PKG}/build-info.txt"

read_state
"$BOARD_TYPE"; echo "board type: $? (2 = WCP, the unit uses sm_WCP.conf)"
grep -n 'NaviSupported' "$XML" 2>/dev/null | cut -c1-200
echo "CarPlay pid=$(cp_pid || echo none), LD_LIBRARY_PATH=$(cp_libpath)"

# ----------------------------------------------------------------------------
# Install / Uninstall
# ----------------------------------------------------------------------------

choose "CarPlay directions on the HUD.\nWhat would you like to do?" "Install" "Uninstall" "Exit"
case $? in
  0) MODE=install ;;
  1) MODE=uninstall ;;
  *) echo "exit chosen"
     save_logs
     echo "${TITLE} finished, exit status 0"
     exit 0 ;;
esac
echo "--- ${MODE}"
note "Checking...\nDo not remove the USB drive."
read_state
if [ "$STATE_SM" = "other" ] || [ "$STATE_WCP" = "other" ]
then
  for _c in $CONFS; do cp "${SM_DIR}/${_c}" "${LOGS}/${_c}.seen" 2>/dev/null; done
  finish 1 "The CarPlay entry in /jci/sm/sm.conf or\nsm_WCP.conf is not the factory one\nor the one HUDLink writes.\nNOTHING was changed."
fi

# ---------------------------------------------------------------- uninstall
if [ "$MODE" = "uninstall" ]
then
  if [ "$STATE_SM" = "orig" ] && [ "$STATE_WCP" = "orig" ] && [ ! -d "$DIR" ]
  then
    finish 0 "HUDLink is not installed.\nNothing was changed."
  fi
  save_logs
  note "Uninstalling...\nDo not remove the USB drive."
  root_writable
  if ! to_factory
  then
    read_state
    finish 2 "A write failed.\nEvery file is still complete (old or new),\nso the head unit is safe to use.\nRun Uninstall again."
  fi
  read_state
  finish 2 "HUDLink is uninstalled.\nThe factory files are back."
fi

# ---------------------------------------------------------------- install
if [ "$STATE_XML" != "off" ] && [ "$STATE_XML" != "on" ]
then
  [ -f "$XML" ] && cp "$XML" "${LOGS}/devmgr_config_master.xml.seen"
  finish 1 "The device manager settings on the head unit\nare not in the expected form (${STATE_XML}).\nNOTHING was changed."
fi

_lp=$(cp_libpath)
if [ "$_lp" != "$LIBPATH" ]
then
  finish 1 "The CarPlay service is not running with the\nlibrary path HUDLink expects.\nStart the car normally and try again.\nNOTHING was changed."
fi

if [ "$STATE_SM" = "new" ] && [ "$STATE_WCP" = "new" ] && [ "$STATE_XML" = "on" ] && [ "$STATE_SO" = "same" ]
then
  if [ -f "$OFF" ]
  then
    cp "$OFF" "${LOGS}/hudlink-crash.txt"
    rm -f "$OFF"
    sync
    echo "off switch cleared (crash details in logs/hudlink-crash.txt)"
    finish 2 "The bridge had switched itself off after a crash.\nIt is switched back on from the next reboot.\nThe crash details are on the USB drive."
  fi
  finish 0 "HUDLink is already installed.\n$(running_text)\nNothing was changed."
fi

SO_WAS=$STATE_SO
need_space /data_persist 2048
need_space /jci 1024
need_space /etc 1024
[ -f "${LOGS}/devmgr_config_master.xml.before" ] || cp "$XML" "${LOGS}/devmgr_config_master.xml.before"
save_logs

note "Testing the bridge on this head unit...\nDo not remove the USB drive."
root_writable
if ! so_stage
then
  [ "$SO_WAS" = "none" ] && so_remove
  finish 1 "Could not copy the bridge to the head unit.\nNOTHING was changed."
fi
if ! gates
then
  rm -f "$NEXT"
  sync
  [ "$SO_WAS" = "none" ] && so_remove
  finish 1 "${BAD}\nNOTHING was changed."
fi

note "Installing...\nDo not remove the USB drive."
_fail=0
so_commit || so_commit || _fail=1
rm -f "$OFF"
if [ "$_fail" = "0" ] && [ "$STATE_XML" = "on" ] && [ "$SO_WAS" = "none" ] &&
   [ "$STATE_SM" = "orig" ] && [ "$STATE_WCP" = "orig" ]
then
  # NaviSupported was on before HUDLink: Uninstall leaves it on.
  echo "NaviSupported was on before HUDLink" > "$NAV_KEEP" || _fail=1
fi
if [ "$_fail" = "0" ]
then
  xml_set on || xml_set on || _fail=1
fi
if [ "$_fail" = "0" ]
then
  for _c in $CONFS
  do
    conf_to "$_c" new || _fail=1
  done
fi
if [ "$_fail" != "0" ]
then
  note "A write failed.\nPutting the factory files back...\nDo not remove the USB drive."
  if to_factory
  then
    read_state
    finish 2 "A write failed.\nThe factory files were put back,\nHUDLink is not installed."
  fi
  read_state
  finish 2 "A write failed.\nEvery file is still complete (old or new),\nso the head unit is safe to use.\nRun Install again, or choose Uninstall."
fi
read_state
if [ "$SO_WAS" = "none" ]
then
  finish 2 "HUDLink is installed.\nAfter the reboot, CarPlay directions\n(Apple Maps, Google Maps) show on the HUD."
fi
finish 2 "HUDLink is updated.\nThe new bridge starts after the reboot."
