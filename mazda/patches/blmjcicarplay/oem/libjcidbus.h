// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Copyright (C) 2026 KID MIXER-MODER (the original CarPlay -> HUD bridge).
// Copyright (C) 2026 The HUDLink authors (changes since; see AUTHORS).
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// libjcidbus: connecting to the head unit's D-Bus buses.
//
// The prototypes of the OEM functions the bridge calls, written as the
// header Mazda never shipped. Each is backed by a thunk in libjcidbus.cpp
// (see oem_thunk.h): no set-up is needed, and a function missing from the
// library returns a failure value (NULL or 0) instead of crashing.
//
// Why libjcidbus rather than a D-Bus binding of our own: libdbus by default
// exits the process when a connection drops, and the service bus connection
// does drop, for example when switching between Android Auto and CarPlay.
// Crash dumps showed exactly that killing jciCARPLAY. libjcidbus turns the
// behaviour off when it connects (dbus_connection_set_exit_on_disconnect
// FALSE) and has a clean shutdown in JCIDBUS_worker_stop().
//
// No header exists for this library: the signatures were worked out from a
// Ghidra decompilation of libjcidbus.so on firmware 74.00.324. Check them
// again after a firmware update.

#ifndef LIBPATCH_BLMJCICARPLAY_LIBJCIDBUS_H
#define LIBPATCH_BLMJCICARPLAY_LIBJCIDBUS_H

// JCIDBUS_conn_connect bus selector (decompiled libjcidbus:
// 0 = SERVICE bus / $JCI_SERVICE_BUS, 1 = HMI bus / $JCI_HMI_BUS).
constexpr int kJciServiceBus = 0;
constexpr int kJciHmiBus     = 1;

// --- connection lifecycle -------------------------------------
// conn_create returns the connection handle (NULL on failure).
// conn_connect returns NON-zero on success (acquires the well-known
// name and, internally, disables exit-on-disconnect). The remaining
// calls are fire-and-forget teardown.
void *JCIDBUS_conn_create(void *error_cb, int reserved);
int   JCIDBUS_conn_connect(void *conn, const char *name,
                           int jcibus, void *reserved);
int   JCIDBUS_worker_start(void *conn);
void  JCIDBUS_worker_stop(void *conn);
void  JCIDBUS_conn_disconnect(void *conn);
void  JCIDBUS_conn_free(void *conn);

#endif  // LIBPATCH_BLMJCICARPLAY_LIBJCIDBUS_H
