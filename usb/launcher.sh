#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 The HUDLink authors (see AUTHORS).
#
# HUDLink USB launcher.
#
# Started by the USB drive's run.sh. Offers each step below in turn with
# Open / Skip / Exit, then a reboot if a step changed the head unit. Every
# step checks itself and the head unit before changing anything, and running
# a step twice is harmless.
# POSIX sh for the head unit's BusyBox. ASCII only, LF line endings.

# Empty on the head unit. The tests run this script against a fake head unit
# kept in a folder, by setting this one line.
ROOT=""

TITLE="HUDLink"
DIALOG="${ROOT}/jci/tools/jci-dialog"

ME=$(readlink -f "$0")
ME_NAME=${ME##*/}
USB_DIR=${ME%/*}

file_md5() { md5sum "$1" 2>/dev/null | awk '{ print $1 }'; }
# The md5 an md5 file records ("<md5>  <name>").
recorded_md5() { awk 'NR == 1 { print $1 }' "$1" 2>/dev/null; }

# On-screen dialogs, as in hudlink.sh.
jci_dialog() {
  _dlg_kind=$1
  _dlg_text=$2
  shift 2
  "$DIALOG" "$_dlg_kind" --title="$TITLE" --text="$_dlg_text" "$@"
}
message() { killall -q jci-dialog; jci_dialog --info "$1" --no-cancel; }
choose() {
  killall -q jci-dialog
  if [ $# -ge 4 ]
  then
    jci_dialog --3-button-dialog "$1" --ok-label="$2" --cancel-label="$3" --button3-label="$4"
  else
    jci_dialog --question "$1" --ok-label="$2" --cancel-label="$3"
  fi
}

REBOOT=0

# step <script> <text>: offer one step. Returns 1 if Exit was chosen.
step() {
  [ -f "${USB_DIR}/$1" ] || return 0
  choose "$2" "Open" "Skip" "Exit"
  case $? in
    0) sh "${USB_DIR}/$1"
       [ $? -eq 2 ] && REBOOT=1
       return 0 ;;
    1) return 0 ;;
    *) return 1 ;;
  esac
}

finish() {
  sync
  if [ "$REBOOT" = "1" ]
  then
    if choose "Done. The head unit needs a reboot.\nPress Reboot now. While the screen is black,\npull out the USB drive." "Reboot now" "Later"
    then
      reboot
    fi
  else
    message "Done. Nothing needs a reboot.\nYou can pull out the USB drive."
  fi
  exit 0
}

_want=$(recorded_md5 "${ME}.md5")
if [ -z "$_want" ] || [ "$(file_md5 "$ME")" != "$_want" ]
then
  sync
  message "${ME_NAME} on the USB drive does not match its md5 file.\nNOTHING was changed.\nCopy the HUDLink files again."
  exit 1
fi

step "hudlink.sh" "STEP 1 of 2: HUDLink\nCarPlay directions on the HUD.\nOpen, then choose Install or Uninstall.\nIf its self-test fails, nothing is changed." || finish
step "hudlink-logs.sh" "STEP 2 of 2: Save logs\nCopies the HUDLink logs and a backup of the\nfiles Install changes to the USB drive.\nChanges nothing on the head unit." || finish
finish
