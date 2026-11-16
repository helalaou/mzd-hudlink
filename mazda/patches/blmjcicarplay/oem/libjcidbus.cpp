// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Copyright (C) 2026 KID MIXER-MODER (the original CarPlay -> HUD bridge).
// Copyright (C) 2026 The HUDLink authors (changes since; see AUTHORS).
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Thunks for the libjcidbus functions in libjcidbus.h (see oem_thunk.h).

#include "../log.h"

#include "libjcidbus.h"
#include "oem_thunk.h"

#include <dlfcn.h>

namespace {

// Looks up one libjcidbus function. The library is normally loaded already;
// opening it here as well means the lookup does not depend on how the
// process happens to be linked. If the dlopen fails, search the whole
// process instead.
void *oem_sym(const char *name)
{
	static void *handle = dlopen("libjcidbus.so", RTLD_NOW | RTLD_GLOBAL);
	void *sym = dlsym(handle ? handle : RTLD_DEFAULT, name);
	if (sym == nullptr) {
		LOGC("oem: dlsym(%s) failed: %s", name, dlerror());
	}
	return sym;
}

} // namespace

OEM_THUNK(void *, JCIDBUS_conn_create,
          (void *error_cb, int reserved), (error_cb, reserved), nullptr)
OEM_THUNK(int, JCIDBUS_conn_connect,
          (void *conn, const char *name, int jcibus, void *reserved),
          (conn, name, jcibus, reserved), 0)
OEM_THUNK(int, JCIDBUS_worker_start, (void *conn), (conn), -1)
OEM_THUNK_VOID(JCIDBUS_worker_stop, (void *conn), (conn))
OEM_THUNK_VOID(JCIDBUS_conn_disconnect, (void *conn), (conn))
OEM_THUNK_VOID(JCIDBUS_conn_free, (void *conn), (conn))
