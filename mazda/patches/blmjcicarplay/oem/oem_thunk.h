// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Copyright (C) 2026 KID MIXER-MODER (the original CarPlay -> HUD bridge).
// Copyright (C) 2026 The HUDLink authors (changes since; see AUTHORS).
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Macros that define the OEM bindings in oem/ as thunks.
//
// Each binding .cpp defines its own file-local lookup function,
//
//   void *oem_sym(const char *name);   // dlsym() in the right library
//
// and then one OEM_THUNK or OEM_THUNK_VOID per function. On its first call a
// thunk looks up the OEM function of the same name, keeps the pointer in a
// function-local static (initialised once, thread-safely, as C++11
// guarantees) and calls it. If the function is missing, the thunk returns
// `failval` instead.
//
// `params` and `args` are lists in parentheses, for example
//   OEM_THUNK(int, Foo, (void *a, int b), (a, b), -1)
//
// The thunks are C++ functions with hidden visibility. Although they share
// the OEM names, they do not replace the OEM functions for anyone else, and
// dlsym() inside oem_sym() still finds the OEM function.

#ifndef LIBPATCH_BLMJCICARPLAY_OEM_THUNK_H
#define LIBPATCH_BLMJCICARPLAY_OEM_THUNK_H

#define OEM_THUNK(ret, name, params, args, failval) \
	ret name params \
	{ \
		using Fn = ret (*) params; \
		static Fn fn = reinterpret_cast<Fn>(oem_sym(#name)); \
		return fn ? fn args : (failval); \
	}

#define OEM_THUNK_VOID(name, params, args) \
	void name params \
	{ \
		using Fn = void (*) params; \
		static Fn fn = reinterpret_cast<Fn>(oem_sym(#name)); \
		if (fn) fn args; \
	}

#endif  // LIBPATCH_BLMJCICARPLAY_OEM_THUNK_H
