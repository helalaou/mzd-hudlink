// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Copyright (C) 2026 KID MIXER-MODER (the original CarPlay -> HUD bridge).
// Copyright (C) 2026 The HUDLink authors (changes since; see AUTHORS).
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// libjcivbsnaviclient: the client side of the com.jci.vbs.navi service, which
// draws on the HUD.
//
// The structs and prototypes of the OEM functions the bridge calls, written
// as the header Mazda never shipped. Each function is backed by a thunk in
// libjcivbsnaviclient.cpp (see oem_thunk.h): no set-up is needed, and a
// function missing from the library returns -1 instead of crashing.
//
// The library sits on top of libjcidbus, whose header is included here, so
// this one header is all the HUD sender needs.
//
// No header exists for this library: the signatures were worked out from a
// Ghidra decompilation of libjcivbsnaviclient.so on firmware 74.00.324.
// Check them again after a firmware update.

#ifndef LIBPATCH_BLMJCICARPLAY_LIBJCIVBSNAVICLIENT_H
#define LIBPATCH_BLMJCICARPLAY_LIBJCIVBSNAVICLIENT_H

#include <stdint.h>

#include "libjcidbus.h"

// The HUD frame: turn icon, distance and speed limit. D-Bus signature
// (uqyqyy), 12 bytes; the offsets (u32 @0, u16 @4, u8 @6, u16 @8, u8 @10,
// u8 @11, with a pad byte at 7) match VBS_NAVI_HUD_Display_s_t_pack.
struct VbsNaviHudDisplay {
    uint32_t nextManeuverInfo;
    uint16_t distanceValue;
    uint8_t  distanceUnit;
    uint16_t displaySpeedLimit;
    uint8_t  displaySpeedUnit;
    uint8_t  text_ID3;
};

// The road name (com.jci.vbs.navi.tmc). D-Bus signature (sy), 8 bytes:
// char * @0, u8 @4. Takes a NUL-terminated UTF-8 string; the library
// converts it to UCS-2 and splits it into pages itself.
struct VbsNaviHudMsg2 {
    const char *guidancePointName;
    uint8_t     syncBit;
};

// The setters take (conn, &struct, unused, reply_cb, user) and return 0
// once the message is queued; reply_cb and user may be NULL.
// GetHUDStatus replies through cb(conn, status, user); status 0 means the
// car has no HUD, or it is off.
int VBS_NAVI_SetHUDDisplayMsgReq(void *conn, VbsNaviHudDisplay *disp,
                                 void *unused, void *cb, void *user);
int VBS_NAVI_TMC_SetHUD_Display_Msg2(void *conn, VbsNaviHudMsg2 *msg2,
                                     void *unused, void *cb, void *user);
int VBS_NAVI_GetHUDStatus(void *conn, void *cb, void *user);

#endif  // LIBPATCH_BLMJCICARPLAY_LIBJCIVBSNAVICLIENT_H
