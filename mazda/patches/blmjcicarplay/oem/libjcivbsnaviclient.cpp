// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CarPlay -> HUD bridge for Mazda Connect (CMU150).
// Copyright (C) 2026 KID MIXER-MODER.
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Thunks for the libjcivbsnaviclient functions in libjcivbsnaviclient.h
// (see oem_thunk.h).

#include "../log.h"

#include "libjcivbsnaviclient.h"
#include "oem_thunk.h"

#include <dlfcn.h>

namespace {

// Looks up one libjcivbsnaviclient function. The library is normally loaded
// already, pulled in by other OEM client libraries; opening it here as well
// means the lookup does not depend on that. If the dlopen fails, search the
// whole process instead.
void *oem_sym(const char *name)
{
	static void *handle = dlopen("libjcivbsnaviclient.so",
	                             RTLD_NOW | RTLD_GLOBAL);
	void *sym = dlsym(handle ? handle : RTLD_DEFAULT, name);
	if (sym == nullptr) {
		LOGC("oem: dlsym(%s) failed: %s", name, dlerror());
	}
	return sym;
}

} // namespace

OEM_THUNK(int, VBS_NAVI_SetHUDDisplayMsgReq,
          (void *conn, VbsNaviHudDisplay *disp, void *unused, void *cb, void *user),
          (conn, disp, unused, cb, user), -1)
OEM_THUNK(int, VBS_NAVI_TMC_SetHUD_Display_Msg2,
          (void *conn, VbsNaviHudMsg2 *msg2, void *unused, void *cb, void *user),
          (conn, msg2, unused, cb, user), -1)
OEM_THUNK(int, VBS_NAVI_GetHUDStatus,
          (void *conn, void *cb, void *user), (conn, cb, user), -1)
