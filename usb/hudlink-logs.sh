#!/bin/sh
# HUDLink: save logs.
#
# Copies to the USB drive (logs/check-<date>-<time>/) what is needed to see
# why CarPlay directions do not reach the HUD: the bridge's logs, the CarPlay
# service entry and process, and the device manager settings the head unit
# really uses. It changes NOTHING on the head unit: it only reads there and
# writes on the USB drive.
#
# Run by launcher.sh. Exit status: 0 (nothing on the head unit changes).
# POSIX sh for the head unit's BusyBox. ASCII only, LF line endings.

# Empty on the head unit. The tests run this script against a fake head unit
# kept in a folder, by setting this one line.
ROOT=""

TITLE="HUDLink: save logs"
DIR="${ROOT}/data_persist/cp-hud-mod"
SM_DIR="${ROOT}/jci/sm"
XML="${ROOT}/etc/devmgr_config_master.xml"
VERSION_INI="${ROOT}/jci/version.ini"
BOARD_TYPE="${ROOT}/jci/scripts/get_board_type.sh"
DIALOG="${ROOT}/jci/tools/jci-dialog"
PROC="${ROOT}/proc"
TMP="${ROOT}/tmp"

hwclock --hctosys

ME=$(readlink -f "$0")
ME_NAME=${ME##*/}
USB_DIR=${ME%/*}

file_md5() { md5sum "$1" 2>/dev/null | awk '{ print $1 }'; }
# The md5 an md5 file records ("<md5>  <name>").
recorded_md5() { awk 'NR == 1 { print $1 }' "$1" 2>/dev/null; }

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

# On-screen dialogs, as in hudlink.sh.
jci_dialog() {
  _dlg_kind=$1
  _dlg_text=$2
  shift 2
  "$DIALOG" "$_dlg_kind" --title="$TITLE" --text="$_dlg_text" "$@"
}
note() { killall -q jci-dialog; jci_dialog --info "$1" --no-cancel & }
message() { killall -q jci-dialog; jci_dialog --info "$1" --no-cancel; }
choose() {
  killall -q jci-dialog
  jci_dialog --question "$1" --ok-label="$2" --cancel-label="$3"
}

# The USB drive is mounted read-only: remount it read-write for the logs.
mount -o rw,remount "$(mount_point_of "$USB_DIR")"

OUT="${USB_DIR}/logs/check-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"
exec > "${OUT}/hudlink-logs.log" 2>&1
echo "${TITLE} ${ME_NAME}, $(date)"

_want=$(recorded_md5 "${ME}.md5")
if [ -z "$_want" ] || [ "$(file_md5 "$ME")" != "$_want" ]
then
  echo "${ME_NAME} does not match its md5 file"
  sync
  message "${ME_NAME} on the USB drive does not match its md5 file.\nNOTHING was changed.\nCopy the HUDLink files again."
  exit 0
fi

if ! choose "Copy the HUDLink logs to the USB drive?\nThis changes nothing on the head unit." "Save" "Cancel"
then
  echo "cancelled"
  sync
  exit 0
fi
note "Saving logs...\nDo not remove the USB drive."

# Strip NUL-separated /proc files into lines.
lines_of() { tr '\000' '\n' < "$1" 2>/dev/null; }

# --- 1. The head unit and the bridge
echo "--- head unit"
grep -E '^JCI_SW_(VER|FLAVOR|VER_PATCH)=' "$VERSION_INI"
"$BOARD_TYPE"; echo "board type: $? (2 = WCP, the unit uses sm_WCP.conf)"
cat "${PROC}/uptime" 2>/dev/null

echo "--- bridge"
ls -la "$DIR"
for _f in diag.log diag.1.log diag.2.log off-after-crash.txt navi-was-on.txt
do
  [ -f "${DIR}/${_f}" ] && cp "${DIR}/${_f}" "${OUT}/bridge-${_f}"
done
[ -f "${TMP}/carplay_bridge.log" ] && cp "${TMP}/carplay_bridge.log" "${OUT}/carplay_bridge.log"
md5sum "${SM_DIR}/sm.conf" "${SM_DIR}/sm_WCP.conf" "${DIR}"/*.so 2>/dev/null

# --- 2. The CarPlay service entry
echo "--- CarPlay service entry"
for _conf in sm.conf sm_WCP.conf
do
  echo "${_conf}, the jciCARPLAY entry:"
  grep -n -A 4 'name="jciCARPLAY"' "${SM_DIR}/${_conf}"
done

# --- 3. The CarPlay process
echo "--- CarPlay process"
for _p in "${PROC}"/[0-9]*
do
  if tr '\000' ' ' < "${_p}/cmdline" 2>/dev/null | grep -q 'sm_svclaunche[r].*jciCARPLAY'
  then
    echo "pid $(basename "$_p"): $(tr '\000' ' ' < "${_p}/cmdline")"
    lines_of "${_p}/environ" | grep '^LD_'
    echo "environment variable names: $(lines_of "${_p}/environ" | cut -d= -f1 | tr '\n' ' ')"
    echo "maps lines: $(grep -c . "${_p}/maps" 2>/dev/null)"
    grep 'libpatch' "${_p}/maps" 2>/dev/null
  fi
done

# --- 4. Device manager settings: the master file and the copies built from
#        it at start-up
echo "--- device manager"
grep -n 'Navi' "$XML"
for _f in "${TMP}/devmgr_config_edit.xml" "${TMP}/devmgr_config.xml"
do
  if [ -f "$_f" ]
  then
    echo "${_f}:"
    grep -n 'Navi' "$_f"
  fi
done

# --- 5. Processes and kernel messages
for _p in "${PROC}"/[0-9]*
do
  _c=$(tr '\000' ' ' < "${_p}/cmdline" 2>/dev/null)
  [ -n "$_c" ] && echo "$(basename "$_p") ${_c}"
done > "${OUT}/processes.txt"
dmesg 2>/dev/null | tail -n 300 > "${OUT}/dmesg.txt"

echo "${TITLE} finished"
sync
message "Logs saved to the USB drive.\nNothing was changed on the head unit."
exit 0
