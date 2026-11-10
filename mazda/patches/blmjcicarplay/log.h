// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CarPlay -> HUD bridge for Mazda Connect (CMU150).
// Copyright (C) 2026 KID MIXER-MODER.
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Logging for the bridge.
//
//   LOGV  verbose   per-message detail (decoded maneuvers, every HUD frame)
//   LOGD  debug     lifecycle and one-off results
//   LOGW  warning   something odd that the bridge works around
//   LOGE  error     a failed call; the bridge carries on
//   LOGC  critical  the bridge cannot do its job
//
// LOG_LEVEL is the lowest level compiled in. A release build (-DNDEBUG)
// keeps errors and critical messages only, so no maneuver text or road
// names are written; a debug build keeps everything. Levels below the
// threshold compile to nothing, but their format strings are still
// type-checked.
//
// The process's stderr goes to syslog, which is not kept, so lines are
// written to the bridge log file (g_logf, opened by main.cpp) and only fall
// back to stderr before that file is open. The log file is in /tmp, which is
// RAM, so it stops growing at LOG_MAX_BYTES; it starts empty again when the
// head unit restarts.
//
// The diagnostic build (make diag, HUDLINK_DIAG) instead keeps one log per
// start of jciCARPLAY on the persistent partition (see main.cpp), with the
// Unix time at the start of each line.

#ifndef LIBPATCH_BLMJCICARPLAY_LOG_H
#define LIBPATCH_BLMJCICARPLAY_LOG_H

#include <cstdio>
#include <ctime>

// The bridge log file. NULL until main.cpp opens it.
extern std::FILE *g_logf;

#ifdef HUDLINK_DIAG
#  define LOG_MAX_BYTES 393216L   // 384 KB
#else
#  define LOG_MAX_BYTES 262144L   // 256 KB
#endif

#define LOG_LEVEL_VERBOSE  0
#define LOG_LEVEL_DEBUG    1
#define LOG_LEVEL_WARN     2
#define LOG_LEVEL_ERROR    3
#define LOG_LEVEL_CRITICAL 4
#define LOG_LEVEL_SILENT   5

#ifndef LOG_LEVEL
#  ifdef NDEBUG
#    define LOG_LEVEL LOG_LEVEL_ERROR
#  else
#    define LOG_LEVEL LOG_LEVEL_VERBOSE
#  endif
#endif

#ifdef HUDLINK_DIAG
#  define LOG_EMIT(level, fmt, ...)                                              \
    do {                                                                         \
        std::FILE *_lf = g_logf ? g_logf : stderr;                               \
        if (_lf != g_logf || std::ftell(_lf) < LOG_MAX_BYTES) {                  \
            std::fprintf(_lf, "%ld [" level "] " fmt "\n",                       \
                         (long)std::time(nullptr), ##__VA_ARGS__);               \
            std::fflush(_lf);                                                    \
        }                                                                        \
    } while (0)
#else
#  define LOG_EMIT(level, fmt, ...)                                              \
    do {                                                                         \
        std::FILE *_lf = g_logf ? g_logf : stderr;                               \
        if (_lf != g_logf || std::ftell(_lf) < LOG_MAX_BYTES) {                  \
            std::fprintf(_lf, "[libpatch-blmjcicarplay][" level "] " fmt "\n",   \
                         ##__VA_ARGS__);                                         \
            std::fflush(_lf);                                                    \
        }                                                                        \
    } while (0)
#endif

// A level that is compiled out: `if (false)` keeps the format string and
// arguments type-checked, and the compiler removes the call at any -O level.
#define LOG_DROP(fmt, ...)                                                       \
    do {                                                                         \
        if (false) {                                                             \
            std::fprintf(stderr, fmt "\n", ##__VA_ARGS__);                       \
        }                                                                        \
    } while (0)

#if LOG_LEVEL <= LOG_LEVEL_VERBOSE
#  define LOGV(fmt, ...) LOG_EMIT("V", fmt, ##__VA_ARGS__)
#else
#  define LOGV(fmt, ...) LOG_DROP(fmt, ##__VA_ARGS__)
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#  define LOGD(fmt, ...) LOG_EMIT("D", fmt, ##__VA_ARGS__)
#else
#  define LOGD(fmt, ...) LOG_DROP(fmt, ##__VA_ARGS__)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#  define LOGW(fmt, ...) LOG_EMIT("W", fmt, ##__VA_ARGS__)
#else
#  define LOGW(fmt, ...) LOG_DROP(fmt, ##__VA_ARGS__)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#  define LOGE(fmt, ...) LOG_EMIT("E", fmt, ##__VA_ARGS__)
#else
#  define LOGE(fmt, ...) LOG_DROP(fmt, ##__VA_ARGS__)
#endif

#if LOG_LEVEL <= LOG_LEVEL_CRITICAL
#  define LOGC(fmt, ...) LOG_EMIT("C", fmt, ##__VA_ARGS__)
#else
#  define LOGC(fmt, ...) LOG_DROP(fmt, ##__VA_ARGS__)
#endif

#endif  // LIBPATCH_BLMJCICARPLAY_LOG_H
