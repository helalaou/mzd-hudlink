// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CarPlay -> HUD bridge for Mazda Connect (CMU150).
// Copyright (C) 2026 KID MIXER-MODER.
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Declarations shared by the bridge's source files.
//
// The bridge is a shared library loaded with LD_PRELOAD into jciCARPLAY, the
// OEM process that runs CarPlay (blmjcicarplay.so):
//   main.cpp         checks that it is in jciCARPLAY and sets up logging
//   devmgr_shim.cpp  wraps device manager calls to ask the iPhone for route
//                    guidance, and wraps msgrcv() to see the guidance messages
//   nav.cpp          decodes the guidance messages
//   hud/hud_send.cpp sends what to show to the HUD
//   oem/             the OEM library functions used to reach the HUD

#ifndef LIBPATCH_BLMJCICARPLAY_PATCH_H
#define LIBPATCH_BLMJCICARPLAY_PATCH_H

#include <stdint.h>

#include "log.h"

// True once main.cpp has confirmed this is the jciCARPLAY process. While it is
// false every hook only passes calls through.
extern bool g_enabled;

// ---- nav.cpp ----------------------------------------------------------------
// Decodes one device manager message: route guidance maneuvers (0x8059) and
// guidance status (0x8058); anything else is ignored. Only reads the buffer.
extern "C" void nav_on_devmgr_msg(const void *msgp, long n);

// ---- hud/hud_send.cpp -------------------------------------------------------
void hud_send_start(void);
void hud_send_stop(void);
// Clear the HUD because CarPlay navigation ended (the phone stays connected).
// Safe to call from any thread.
void hud_request_clear(void);
// Clear the HUD because the CarPlay session ended. Safe to call from any
// thread.
void hud_request_fullclear(void);

// ---- main.cpp ---------------------------------------------------------------
// True when /proc/self/cmdline shows this is the jciCARPLAY launcher.
bool in_carplay_launcher(void);

#endif // LIBPATCH_BLMJCICARPLAY_PATCH_H
