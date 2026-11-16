// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Adapted from headunit (https://github.com/Trevelopment/headunit),
// licensed under GNU AGPL v3, by KID MIXER-MODER (2026), and changed since
// by the HUDLink authors (see AUTHORS).
// See NOTICE.md at the repo root for the full attribution.
//
// The calls nav.cpp makes to put a maneuver on the HUD.
//
// The interface follows the Android Auto navigation messages that headunit's
// HUD code (mazda/hud/hud.{h,cpp}) was written for, so turn events, sides and
// distance units use the Android Auto values. Each call updates the sender's
// snapshot and wakes the sender thread; none of them blocks. Starting and
// stopping the sender is declared in patch.h.

#ifndef LIBPATCH_BLMJCICARPLAY_HUD_SEND_H
#define LIBPATCH_BLMJCICARPLAY_HUD_SEND_H

#include <stdint.h>

// Navigation status: 1 started; anything else ended, which clears the
// snapshot.
void hud_on_status(uint32_t status);

// The next maneuver.
//   road_name    road to show; may be NULL or empty. Copied, not kept.
//   turn_side    1 left, 2 right, 3 none
//   turn_event   Android Auto turn event, 0..19 (13 = roundabout)
//   turn_angle   roundabout exit angle in degrees
//   turn_number  roundabout exit number
void hud_on_next_turn(const char *road_name,
                      uint32_t    turn_side,
                      uint32_t    turn_event,
                      int32_t     turn_angle,
                      int32_t     turn_number);

// Distance to the next maneuver.
//   display_distance       the distance to show, times 1000, in
//                          display_distance_unit
//   display_distance_unit  Android Auto unit: 1 metres, 2 kilometres
//                          (tenths), 3 kilometres, 4 miles (tenths),
//                          5 miles, 6 feet
void hud_on_distance(int32_t display_distance, uint32_t display_distance_unit);

#endif // LIBPATCH_BLMJCICARPLAY_HUD_SEND_H
