// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Copyright (C) 2026 KID MIXER-MODER (the original CarPlay -> HUD bridge).
// Copyright (C) 2026 The HUDLink authors (changes since; see AUTHORS).
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Decodes Apple's route guidance messages and drives the HUD.
//
// The device manager turns the iPhone's iAP2 route guidance updates into two
// fixed-layout messages, which arrive through the msgrcv() hook:
//
//   0x8059  maneuver  one per maneuver of the current route, 840 bytes
//   0x8058  guidance  route status, 1628 bytes
//
// The iPhone sends the maneuver list again from the start (index 0, "start
// route") every time it recalculates, so a maneuver's index is its position
// in the latest list, not a stable id. The guidance message carries
// maneuverList[0], the index of the next maneuver, and that is what picks
// the maneuver to show. Maneuvers of the current list are buffered (index 0
// and context entries left out, see is_context_type) and the HUD shows the
// buffered one whose index matches the guidance head, or head + 1 when the
// head is a context entry. If the head is not buffered yet, because a new
// list is still arriving, the HUD keeps what it shows.
//
// Maneuver message layout (offsets from the start of the message, found by
// disassembling the ipoddev producer on firmware 74.00.324):
//   +0x1c   u32       validInfo: which optional fields are present
//                     (0x10 road name, 0x800 roundabout junction)
//   +0x20   u32       index << 16
//   +0x24   char[256] maneuverDesc, the instruction text
//   +0x124  u32       length of maneuverDesc
//   +0x128  u32       maneuverType (see below)
//   +0x12c  char[256] afterManeuverRoadName, the road the maneuver leads onto
//   +0x33c  u32       driveSide: 0 right-hand traffic, 1 left-hand traffic
//   +0x340  u32       junctionType: 0 intersection, 1 roundabout
//   +0x344  s16       junctionElementAngle: zero on every roundabout
//                     captured, not used
//   +0x346  s16       junctionElementExitAngle: bearing of the exit taken, in
//                     degrees; 0 straight on, negative left, positive right
//
// Guidance message layout:
//   +0x34c  u32       distance to the next maneuver, in metres
//   +0x458  u16       maneuverList[0], the index of the next maneuver
//   +0x656  u16       number of entries in maneuverList
//
// maneuverType is Apple's iAP2 maneuver type (the same values as
// CPManeuverType):
//   0 NoTurn  1 Left  2 Right  3 Straight  4 UTurn  5 FollowRoad
//   6 EnterRoundabout  7 ExitRoundabout  8 OffRamp  9 OnRamp  10 ArriveEnd
//   11 StartRoute  12 Arrive  13 KeepLeft  14 KeepRight  15-17 Ferry
//   18 StartRouteWithUTurn  19 UTurnAtRoundabout  20/21 Left/RightTurnAtEnd
//   22/23 HighwayOffRampLeft/Right  24/25 ArriveLeft/Right
//   26 UTurnWhenPossible  27 ArriveEndOfDirections
//   28-46 RoundaboutExit1..19  47/48 SharpLeft/Right  49/50 SlightLeft/Right
//   51 ChangeHighway  52/53 ChangeHighwayLeft/Right

#include "patch.h"
#include "hud/hud_send.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <atomic>

namespace {
constexpr uint32_t kMtManeuver      = 0x8059;
constexpr uint32_t kMtGuidance      = 0x8058;
constexpr int      kBurstIdxOff     = 0x20;   // u32: index << 16
constexpr int      kValidInfoOff    = 0x1c;   // u32 validInfo
constexpr int      kManeuverTextOff = 0x24;
constexpr int      kManeuverTextMax = 256;
constexpr int      kManeuverTypeOff = 0x128;  // u32 maneuverType
constexpr int      kRoadNameOff     = 0x12c;  // afterManeuverRoadName
constexpr int      kDriveSideOff    = 0x33c;  // u32 driveSide: 0=RHT, 1=LHT
constexpr int      kJunctionTypeOff = 0x340;  // u32 junctionType: 1=roundabout
constexpr int      kJunctionAngleOff = 0x346; // s16 junctionElementExitAngle
constexpr int      kGuidanceDistOff = 0x34c;  // u32 distance to the next maneuver, metres
constexpr int      kManListOff      = 0x458;  // u16 maneuverList[0]
constexpr int      kManListCntOff   = 0x656;  // u16 maneuverList count

// Turn events and sides as the HUD sender takes them (the Android Auto
// NAVTurnMessage values, see hud_send.h). Event 13, roundabout, picks its
// icon from the exit angle.
constexpr uint32_t EV_DEPART = 1, EV_NAME_CHANGE = 2, EV_SLIGHT = 3, EV_TURN = 4,
                   EV_SHARP = 5, EV_UTURN = 6, EV_ON_RAMP = 7, EV_OFF_RAMP = 8,
                   EV_FORK = 9, EV_MERGE = 10, EV_ROUNDABOUT = 13, EV_STRAIGHT = 14,
                   EV_FERRY = 16, EV_DEST = 19;
constexpr uint32_t SIDE_LEFT = 1, SIDE_RIGHT = 2, SIDE_NONE = 3;

// What the HUD shows now. The HUD sender repeats it twice a second.
uint32_t g_disp_event  = EV_STRAIGHT;
uint32_t g_disp_side   = SIDE_NONE;
int32_t  g_disp_angle  = 0;
int32_t  g_disp_number = 0;
char     g_disp_road[256] = {0};
// What was last passed to the HUD sender, to send only changes.
uint32_t g_emit_event  = 0xffffffff;
uint32_t g_emit_side   = 0xffffffff;
int32_t  g_emit_angle  = 0x7fffffff;
int32_t  g_emit_number = 0x7fffffff;
char     g_emit_road[256] = {0};
// Set by the HUD sender after it clears the HUD (see nav_request_reset).
std::atomic<bool> g_nav_reset_req{false};

// Maneuver types that describe the road the car is already on rather than a
// maneuver to show: 0 NoTurn, 5 FollowRoad, 11 StartRoute,
// 18 StartRouteWithUTurn. Index 0 is always such an entry; these can also
// appear later in the list. 3 (Straight) is a real maneuver and is shown.
bool is_context_type(uint32_t m)
{
    return m == 0 || m == 5 || m == 11 || m == 18;
}

struct Maneuver {
    uint32_t event;
    uint32_t side;     // 1=LEFT, 2=RIGHT, 3=unspecified
    int32_t  angle;    // roundabout exit angle in [0, 360); else 0
    int32_t  number;   // roundabout exit number (1..19); else 0
};

// One maneuver of the current list that can be shown.
struct BufManeuver {
    uint32_t mtype;
    uint32_t idx;         // index in the list as sent; matched against the guidance
                          // head (g_buf skips entries, so this is not the array index)
    uint32_t validInfo;
    Maneuver mv;
    char     road[256];
    char     desc[256];   // instruction text, shown when the road name is empty
};
constexpr int kMaxBurst = 48;          // the longest list seen had 33 maneuvers
BufManeuver g_buf[kMaxBurst];
int         g_buf_count = 0;
uint32_t    g_prev_idx  = 0xffffffff;
// The last guidance head, so that a new maneuver list can show the same
// maneuver again before the next guidance message arrives.
uint32_t    g_last_head       = 0;
bool        g_last_head_valid = false;

// Maps a maneuver type and its junction fields to the HUD's turn event, side
// and angle.
Maneuver classify(uint32_t mtype, uint32_t junctionType,
                  int32_t junctionAngle, uint32_t driveSide)
{
    Maneuver m{EV_STRAIGHT, SIDE_NONE, 0, 0};

    // A roundabout, by maneuver type or by junction type. The HUD sender picks
    // one of 12 roundabout icons from the exit angle.
    bool ra = (junctionType == 1) || mtype == 6 || mtype == 7 || mtype == 19 ||
              (mtype >= 28 && mtype <= 46);
    if (ra) {
        m.event = EV_ROUNDABOUT;
        m.side  = (driveSide == 1) ? SIDE_LEFT : SIDE_RIGHT;   // left- or right-hand traffic icons
        int32_t a = junctionAngle % 360;
        if (a < 0) a += 360;
        if (mtype >= 28 && mtype <= 46) m.number = (int32_t)mtype - 27;  // exit N
        // The exit angle is the bearing of the exit taken (0 = straight on).
        // It is not derived from the exit number, which says nothing about
        // the roundabout's shape.
        m.angle = a;
        return m;
    }

    switch (mtype) {
    case 1:  m.event = EV_TURN;   m.side = SIDE_LEFT;  break;  // LeftTurn
    case 2:  m.event = EV_TURN;   m.side = SIDE_RIGHT; break;  // RightTurn
    case 20: m.event = EV_TURN;   m.side = SIDE_LEFT;  break;  // LeftTurnAtEnd
    case 21: m.event = EV_TURN;   m.side = SIDE_RIGHT; break;  // RightTurnAtEnd
    case 47: m.event = EV_SHARP;  m.side = SIDE_LEFT;  break;  // SharpLeft
    case 48: m.event = EV_SHARP;  m.side = SIDE_RIGHT; break;  // SharpRight
    case 49: m.event = EV_SLIGHT; m.side = SIDE_LEFT;  break;  // SlightLeft
    case 50: m.event = EV_SLIGHT; m.side = SIDE_RIGHT; break;  // SlightRight
    case 13: m.event = EV_FORK;   m.side = SIDE_LEFT;  break;  // KeepLeft  (bear/fork)
    case 14: m.event = EV_FORK;   m.side = SIDE_RIGHT; break;  // KeepRight (bear/fork)
    case 51: m.event = EV_FORK;   m.side = SIDE_NONE;  break;  // ChangeHighway
    case 52: m.event = EV_FORK;   m.side = SIDE_LEFT;  break;  // ChangeHighwayLeft
    case 53: m.event = EV_FORK;   m.side = SIDE_RIGHT; break;  // ChangeHighwayRight
    case 4:  m.event = EV_UTURN;  m.side = (driveSide == 1) ? SIDE_RIGHT : SIDE_LEFT; break; // UTurn
    case 26: m.event = EV_UTURN;  m.side = (driveSide == 1) ? SIDE_RIGHT : SIDE_LEFT; break; // UTurnWhenPossible
    case 8:  m.event = EV_OFF_RAMP; m.side = SIDE_NONE;  break; // OffRamp
    case 22: m.event = EV_OFF_RAMP; m.side = SIDE_LEFT;  break; // HighwayOffRampLeft
    case 23: m.event = EV_OFF_RAMP; m.side = SIDE_RIGHT; break; // HighwayOffRampRight
    case 9:  m.event = EV_ON_RAMP;  m.side = SIDE_NONE;  break; // OnRamp
    case 15: case 16: case 17: m.event = EV_FERRY; m.side = SIDE_NONE; break; // Ferry
    case 10: case 12: case 27:  m.event = EV_DEST; m.side = SIDE_NONE;  break; // Arrive*
    case 24: m.event = EV_DEST; m.side = SIDE_LEFT;  break;  // ArriveDestinationLeft
    case 25: m.event = EV_DEST; m.side = SIDE_RIGHT; break;  // ArriveDestinationRight
    // 3 Straight, the context types and anything unknown show straight on.
    default: m.event = EV_STRAIGHT; m.side = SIDE_NONE; break;
    }
    return m;
}

const char *read_str(const uint8_t *b, long n, int off, char *out, int outsz)
{
    int k = 0;
    if (off < 0) { out[0] = '\0'; return out; }
    for (; (long)(off + k) < n && k < outsz - 1 && b[off + k]; ++k) out[k] = (char)b[off + k];
    out[k] = '\0';
    return out;
}

uint32_t read_u32(const uint8_t *b, long n, int off)
{
    if (off < 0 || (long)(off + 4) > n) return 0;
    return *reinterpret_cast<const uint32_t *>(b + off);
}
int32_t read_s16(const uint8_t *b, long n, int off)
{
    if (off < 0 || (long)(off + 2) > n) return 0;
    return *reinterpret_cast<const int16_t *>(b + off);
}
uint32_t read_u16(const uint8_t *b, long n, int off)
{
    if (off < 0 || (long)(off + 2) > n) return 0;
    return *reinterpret_cast<const uint16_t *>(b + off);
}

// The buffered maneuver for a guidance head: the one with that index, or the
// next one when the head is a context entry (those are not buffered).
// Returns its position in g_buf, or -1 if neither is buffered yet.
int select_by_head(uint32_t head)
{
    for (int i = 0; i < g_buf_count; ++i) if (g_buf[i].idx == head)     return i;
    for (int i = 0; i < g_buf_count; ++i) if (g_buf[i].idx == head + 1) return i;
    return -1;
}

// Shows g_buf[sel] and passes it to the HUD sender if it differs from what
// was last passed.
void emit_selection(int sel)
{
    if (sel < 0 || sel >= g_buf_count) return;
    const BufManeuver &im = g_buf[sel];
    // The road name of this maneuver, never of a later one. Apple leaves it
    // empty for many turns; show the instruction text then.
    const char *road = im.road[0] ? im.road : im.desc;

    g_disp_event  = im.mv.event;
    g_disp_side   = im.mv.side;
    g_disp_angle  = im.mv.angle;
    g_disp_number = im.mv.number;
    strncpy(g_disp_road, road, sizeof(g_disp_road) - 1);
    g_disp_road[sizeof(g_disp_road) - 1] = '\0';

    LOGD("nav PICK sel=%d/%d idx=%u type=%u vi=%u ev=%u side=%u ang=%d num=%d road=\"%s\"",
         sel, g_buf_count, im.idx, im.mtype, im.validInfo,
         g_disp_event, g_disp_side, g_disp_angle, g_disp_number, g_disp_road);

    if (g_disp_event != g_emit_event || g_disp_side != g_emit_side ||
        g_disp_angle != g_emit_angle || g_disp_number != g_emit_number ||
        strcmp(g_disp_road, g_emit_road) != 0) {
        g_emit_event = g_disp_event; g_emit_side = g_disp_side;
        g_emit_angle = g_disp_angle; g_emit_number = g_disp_number;
        strncpy(g_emit_road, g_disp_road, sizeof(g_emit_road) - 1);
        g_emit_road[sizeof(g_emit_road) - 1] = '\0';
        LOGD("nav HUD: event=%u side=%u angle=%d number=%d road=\"%s\"",
             g_disp_event, g_disp_side, g_disp_angle, g_disp_number, g_disp_road);
        hud_on_status(1);
        hud_on_next_turn(g_disp_road[0] ? g_disp_road : " ",
                         g_disp_side, g_disp_event, g_disp_angle, g_disp_number);
    }
}
} // namespace

// Called by the HUD sender after it clears the HUD. Without a reset, a route
// started again would begin with the maneuver last passed on, which would be
// dropped as unchanged and leave the HUD blank. The state belongs to the
// msgrcv thread, so this only sets a flag and nav_on_devmgr_msg resets it.
extern "C" void nav_request_reset(void)
{
    g_nav_reset_req.store(true, std::memory_order_relaxed);
}

extern "C" void nav_on_devmgr_msg(const void *msgp, long n)
{
    if (!msgp || n < (kManeuverTextOff + 4)) return;
    // This runs on the device manager's receive thread. An exception that
    // reached the OEM code would end in std::terminate() and take CarPlay
    // down, so none may leave. The decoder does not throw today.
    try {
    const uint8_t *b = static_cast<const uint8_t *>(msgp);
    uint32_t mt = (*reinterpret_cast<const uint32_t *>(b)) & 0xffff;

    // The HUD was cleared: forget the selection and what was last passed on.
    if (g_nav_reset_req.exchange(false, std::memory_order_relaxed)) {
        g_buf_count = 0; g_last_head_valid = false; g_last_head = 0;
        g_prev_idx = 0xffffffff;
        g_emit_event = 0xffffffff; g_emit_side = 0xffffffff;
        g_emit_angle = 0x7fffffff; g_emit_number = 0x7fffffff;
        g_emit_road[0] = '\0';
    }

    if (mt == kMtManeuver) {
        uint32_t idx = (n >= kBurstIdxOff + 4)
                     ? (*reinterpret_cast<const uint32_t *>(b + kBurstIdxOff) >> 16) : 0;

        // A new list starts when the index goes back to 0 or drops. The last
        // guidance head is kept, so the new list shows the same maneuver.
        bool burst_start = (idx == 0) || (idx < g_prev_idx);
        g_prev_idx = idx;
        if (burst_start) {
            g_buf_count = 0;
        }

        uint32_t mtype     = read_u32(b, n, kManeuverTypeOff);
        // Index 0 and context entries are not shown, so not buffered.
        if (idx == 0 || is_context_type(mtype)) return;
        if (g_buf_count >= kMaxBurst) return;

        uint32_t validInfo = read_u32(b, n, kValidInfoOff);
        uint32_t driveSide = read_u32(b, n, kDriveSideOff);
        uint32_t junction  = read_u32(b, n, kJunctionTypeOff);
        int32_t  jAngle    = read_s16(b, n, kJunctionAngleOff);
        char     road[256];
        read_str(b, n, kRoadNameOff, road, sizeof(road));
        char     text[kManeuverTextMax + 1];
        read_str(b, n, kManeuverTextOff, text, sizeof(text));

        Maneuver mv = classify(mtype, junction, jAngle, driveSide);

        BufManeuver &e = g_buf[g_buf_count++];
        e.mtype = mtype; e.idx = idx; e.validInfo = validInfo; e.mv = mv;
        strncpy(e.road, road, sizeof(e.road) - 1);
        e.road[sizeof(e.road) - 1] = '\0';
        strncpy(e.desc, text, sizeof(e.desc) - 1);
        e.desc[sizeof(e.desc) - 1] = '\0';

        LOGD("nav MAN idx=%u type=%u vi=%u jt=%u jAng=%d ev=%u side=%u ang=%d num=%d "
             "desc=\"%s\" road=\"%s\"",
             idx, mtype, validInfo, junction, jAngle, mv.event, mv.side, mv.angle, mv.number,
             text, road);

        // Show the maneuver for the last guidance head as soon as it is in
        // the new list. Until then the HUD keeps what it shows.
        if (g_last_head_valid) {
            int sel = select_by_head(g_last_head);
            if (sel >= 0) emit_selection(sel);
        }

    } else if (mt == kMtGuidance) {
        // maneuverList[0] is the index of the next maneuver.
        uint32_t cnt = read_u16(b, n, kManListCntOff);
        if (cnt > 0) {
            uint32_t head = read_u16(b, n, kManListOff);
            int sel = select_by_head(head);
            if (sel >= 0) {
                g_last_head = head;
                g_last_head_valid = true;
                emit_selection(sel);
            }
            // Not buffered yet (a new list is still arriving): keep what the HUD shows.
        }
        // Distance to the next maneuver, shown whatever the selection.
        if (n >= kGuidanceDistOff + 4) {
            int32_t dist = (int32_t)(*reinterpret_cast<const uint32_t *>(b + kGuidanceDistOff));
            if (dist > 0 && dist <= 100000) {
                int32_t  disp; uint32_t unit;
                // disp is the value to show times 1000, in `unit` (the
                // Android Auto units, see hud_on_distance).
#if HUDLINK_IMPERIAL
                // Feet, to the nearest 10, below 0.1 mile; miles, to one
                // decimal place, from there on.
                if (dist < 161) {
                    // 1 m = 3.28084 ft
                    int32_t ft = (int32_t)(((int64_t)dist * 328084 + 500000) / 1000000) * 10;
                    if (ft < 10) ft = 10;
                    unit = 6; disp = ft * 1000;                         // feet
                } else {
                    // 0.1 mi = 160.9344 m
                    int32_t tenths = (int32_t)(((int64_t)dist * 10000 + 804672) / 1609344);
                    if (tenths < 1) tenths = 1;
                    unit = 5; disp = tenths * 100;                      // miles
                }
#else
                // Metres below 1 km, kilometres from there on.
                if (dist < 1000) { unit = 1; disp = dist * 1000; }  // metres
                else             { unit = 3; disp = dist;        }  // kilometres
#endif
                hud_on_distance(disp, unit);
            }
        }
    }
    } catch (...) {
        LOGE("nav: nav_on_devmgr_msg exception swallowed — event dropped (no OEM-thread escape)");
    }
}
