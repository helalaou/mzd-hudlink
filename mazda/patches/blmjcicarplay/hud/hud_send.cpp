// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Adapted from headunit (https://github.com/Trevelopment/headunit),
// licensed under GNU AGPL v3, by KID MIXER-MODER (2026), and changed since
// by the HUDLink authors (see AUTHORS).
// See NOTICE.md at the repo root for the full attribution.
//
// The HUD sender: turns the maneuvers decoded by nav.cpp into Mazda Active
// Driving Display frames and sends them to the OEM navigation service,
// com.jci.vbs.navi, on the service bus.
//
// Transport. The frames go through the OEM libraries libjcidbus and
// libjcivbsnaviclient (see oem/), the same calls the built-in navigation
// uses, rather than through a D-Bus binding of our own:
//   * libjcidbus turns off libdbus's exit-on-disconnect, so the service bus
//     going away (for example when switching between Android Auto and
//     CarPlay) cannot kill jciCARPLAY.
//   * libjcidbus runs its own I/O thread, so nothing of ours is left polling
//     a closed connection.
//   * All D-Bus setup happens on the sender thread, so arming navigation
//     never waits for the bus.
//
// Threads. The hud_on_*() functions are called on the device manager's
// receive thread and must not block. They write a snapshot protected by a
// sequence lock (g_seq is odd while a write is in progress) and wake the
// sender thread. The sender reads the snapshot, compares it with the last
// one sent, and makes the OEM calls: SetHUDDisplayMsgReq for the turn icon
// and distance, SetHUD_Display_Msg2 for the road name. Updates that arrive
// faster than they are sent fold into one; the latest wins. A ticker wakes
// the sender twice a second to send the current frame again, as the
// built-in navigation does, so the HUD does not time it out.

#include "../patch.h"
#include "hud_send.h"
#include "../oem/libjcivbsnaviclient.h"   // OEM transport (libjcidbus + VBS_NAVI_*)

#include <cstring>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

// Defined in nav.cpp. Called after the HUD is cleared, so that a route started
// again is shown even if it begins with the same maneuver.
extern "C" void nav_request_reset(void);

namespace {

// ---- Vietnamese road names --------------------------------------------------
//
// The HUD's font (in the HUD's own firmware) draws ASCII and the 2-byte UTF-8
// range (Latin-1 Supplement, Latin Extended-A and -B), but silently drops
// 3-byte characters and combining marks. Vietnamese letters with two
// diacritics, such as ề, ớ and ấ, are 3-byte characters (Latin Extended
// Additional, U+1E00..U+1EFF), so "về hướng" would show as "v  hư ng".
//
// With CARPLAY_VN_NORMALIZE=1 those letters are replaced by the nearest
// letter the HUD can draw, keeping the circumflex, horn or breve and
// dropping the tone mark (ề -> ê, ớ -> ơ, ả -> a), so the example shows as
// "vê hương". Other 3-byte and 4-byte characters become '?'. ASCII and
// 2-byte characters are not touched. The Makefile's VIETNAMESE option sets
// CARPLAY_VN_NORMALIZE; without it, road names are sent unchanged.
#ifndef CARPLAY_VN_NORMALIZE
#define CARPLAY_VN_NORMALIZE 1
#endif

// The replacement for a code point in U+1E00..U+1EFF: a code point below
// 0x800 (written as 1 or 2 bytes of UTF-8), or 0 if there is none.
static uint32_t vn_normalize_3byte_cp(uint32_t cp)
{
    switch (cp) {
    // ===== Uppercase ===========================================================
    case 0x1EA0: case 0x1EA2:                                       return 'A';        // Ạ Ả
    case 0x1EA4: case 0x1EA6: case 0x1EA8: case 0x1EAA: case 0x1EAC: return 0x00C2;     // Ấ Ầ Ẩ Ẫ Ậ -> Â
    case 0x1EAE: case 0x1EB0: case 0x1EB2: case 0x1EB4: case 0x1EB6: return 0x0102;     // Ắ Ằ Ẳ Ẵ Ặ -> Ă
    case 0x1EB8: case 0x1EBA: case 0x1EBC:                           return 'E';        // Ẹ Ẻ Ẽ
    case 0x1EBE: case 0x1EC0: case 0x1EC2: case 0x1EC4: case 0x1EC6: return 0x00CA;     // Ế Ề Ể Ễ Ệ -> Ê
    case 0x1EC8: case 0x1ECA:                                       return 'I';        // Ỉ Ị
    case 0x1ECC: case 0x1ECE:                                       return 'O';        // Ọ Ỏ
    case 0x1ED0: case 0x1ED2: case 0x1ED4: case 0x1ED6: case 0x1ED8: return 0x00D4;     // Ố Ồ Ổ Ỗ Ộ -> Ô
    case 0x1EDA: case 0x1EDC: case 0x1EDE: case 0x1EE0: case 0x1EE2: return 0x01A0;     // Ớ Ờ Ở Ỡ Ợ -> Ơ
    case 0x1EE4: case 0x1EE6:                                       return 'U';        // Ụ Ủ
    case 0x1EE8: case 0x1EEA: case 0x1EEC: case 0x1EEE: case 0x1EF0: return 0x01AF;     // Ứ Ừ Ử Ữ Ự -> Ư
    case 0x1EF2: case 0x1EF4: case 0x1EF6: case 0x1EF8:              return 'Y';        // Ỳ Ỵ Ỷ Ỹ
    // ===== Lowercase ===========================================================
    case 0x1EA1: case 0x1EA3:                                       return 'a';        // ạ ả
    case 0x1EA5: case 0x1EA7: case 0x1EA9: case 0x1EAB: case 0x1EAD: return 0x00E2;     // ấ ầ ẩ ẫ ậ -> â
    case 0x1EAF: case 0x1EB1: case 0x1EB3: case 0x1EB5: case 0x1EB7: return 0x0103;     // ắ ằ ẳ ẵ ặ -> ă
    case 0x1EB9: case 0x1EBB: case 0x1EBD:                           return 'e';        // ẹ ẻ ẽ
    case 0x1EBF: case 0x1EC1: case 0x1EC3: case 0x1EC5: case 0x1EC7: return 0x00EA;     // ế ề ể ễ ệ -> ê
    case 0x1EC9: case 0x1ECB:                                       return 'i';        // ỉ ị
    case 0x1ECD: case 0x1ECF:                                       return 'o';        // ọ ỏ
    case 0x1ED1: case 0x1ED3: case 0x1ED5: case 0x1ED7: case 0x1ED9: return 0x00F4;     // ố ồ ổ ỗ ộ -> ô
    case 0x1EDB: case 0x1EDD: case 0x1EDF: case 0x1EE1: case 0x1EE3: return 0x01A1;     // ớ ờ ở ỡ ợ -> ơ
    case 0x1EE5: case 0x1EE7:                                       return 'u';        // ụ ủ
    case 0x1EE9: case 0x1EEB: case 0x1EED: case 0x1EEF: case 0x1EF1: return 0x01B0;     // ứ ừ ử ữ ự -> ư
    case 0x1EF3: case 0x1EF5: case 0x1EF7: case 0x1EF9:              return 'y';        // ỳ ỵ ỷ ỹ
    default:                                                         return 0;
    }
}

// Copies UTF-8 src to dst as described above, NUL-terminated. Returns the
// number of bytes written, not counting the NUL.
static size_t normalize_vn(const char *src, char *dst, size_t dst_sz)
{
    if (!dst || dst_sz == 0) return 0;
    if (!src) { dst[0] = '\0'; return 0; }

    size_t di = 0;
    const size_t max = dst_sz - 1;  // reserve room for NUL
    const unsigned char *s = reinterpret_cast<const unsigned char *>(src);

    while (*s && di < max) {
        unsigned char c = *s;
        uint32_t cp = 0;
        size_t nb = 1;

        if ((c & 0x80) == 0) {
            // ASCII
            dst[di++] = static_cast<char>(c);
            s++;
            continue;
        } else if ((c & 0xE0) == 0xC0 && s[1]) {
            // 2 bytes: the HUD draws these
            if (di + 2 > max) break;
            dst[di++] = static_cast<char>(c);
            dst[di++] = static_cast<char>(s[1]);
            s += 2;
            continue;
        } else if ((c & 0xF0) == 0xE0 && s[1] && s[2]) {
            cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
            nb = 3;
        } else if ((c & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
            cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                 ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
            nb = 4;
        } else {
            // Not a valid first byte: skip it
            s++;
            continue;
        }

        // 3 or 4 bytes
        uint32_t mapped = (nb == 3) ? vn_normalize_3byte_cp(cp) : 0;
        if (mapped < 0x80 && mapped != 0) {
            dst[di++] = static_cast<char>(mapped);
        } else if (mapped >= 0x80 && mapped < 0x800) {
            if (di + 2 > max) break;
            dst[di++] = static_cast<char>(0xC0 | (mapped >> 6));
            dst[di++] = static_cast<char>(0x80 | (mapped & 0x3F));
        } else {
            // No replacement: '?' at least shows that a character is there.
            dst[di++] = '?';
        }
        s += nb;
    }

    dst[di] = '\0';
    return di;
}

// The name the sender takes on the service bus. It must differ from
// com.jci.vbs.navi, the service it calls, and from com.jci.aapa.hud, the name
// the Android Auto HUD mod uses: if both mods ran under one name, the second
// to connect would fail and get no HUD.
constexpr const char *kBusName = "com.jci.carplay.hud";

// ---- HUD icons ---------------------------------------------------------------
//
// The turn icon codes the HUD understands (NaviTurns in headunit's hud.h).
// The icons themselves are in the HUD's firmware.
enum MazdaIcon : uint8_t {
    HUD_STRAIGHT          = 1,
    HUD_LEFT              = 2,
    HUD_RIGHT             = 3,
    HUD_SLIGHT_LEFT       = 4,
    HUD_SLIGHT_RIGHT      = 5,
    HUD_OFF_RAMP_RIGHT    = 7,
    HUD_DESTINATION       = 8,
    HUD_SHARP_RIGHT       = 9,
    HUD_U_TURN_RIGHT      = 10,
    HUD_SHARP_LEFT        = 11,
    HUD_FLAG              = 12,
    HUD_U_TURN_LEFT       = 13,
    HUD_FORK_RIGHT        = 14,
    HUD_FORK_LEFT         = 15,
    HUD_MERGE_LEFT        = 16,
    HUD_MERGE_RIGHT       = 17,
    HUD_OFF_RAMP_LEFT     = 30,
    HUD_DESTINATION_LEFT  = 33,
    HUD_DESTINATION_RIGHT = 34,
    HUD_FLAG_LEFT         = 35,
    HUD_FLAG_RIGHT        = 36,
};

// Icon for a turn event and side: kTurnIcons[turn_event][side], side 0 left,
// 1 right, 2 none. Turn events are the Android Auto NAVTurnMessage values
// (0..19; 15 and 18 are unused). 0 means no icon. Roundabouts (13) are
// handled by roundabout_icon(). The table is headunit's turns[][] unchanged;
// it has been checked on real cars, so change it only after a test drive.
constexpr uint8_t kTurnIcons[20][3] = {
    /*  0 TURN_UNKNOWN                  */ {0, 0, 0},
    /*  1 TURN_DEPART                   */ {HUD_FLAG_LEFT, HUD_FLAG_RIGHT, HUD_FLAG},
    /*  2 TURN_NAME_CHANGE              */ {HUD_STRAIGHT, HUD_STRAIGHT, HUD_STRAIGHT},
    /*  3 TURN_SLIGHT_TURN              */ {HUD_SLIGHT_LEFT, HUD_SLIGHT_RIGHT, HUD_STRAIGHT},
    /*  4 TURN_TURN                     */ {HUD_LEFT, HUD_RIGHT, HUD_STRAIGHT},
    /*  5 TURN_SHARP_TURN               */ {HUD_SHARP_LEFT, HUD_SHARP_RIGHT, HUD_STRAIGHT},
    /*  6 TURN_U_TURN                   */ {HUD_U_TURN_LEFT, HUD_U_TURN_RIGHT, HUD_STRAIGHT},
    /*  7 TURN_ON_RAMP                  */ {HUD_LEFT, HUD_RIGHT, HUD_STRAIGHT},
    /*  8 TURN_OFF_RAMP                 */ {HUD_OFF_RAMP_LEFT, HUD_OFF_RAMP_RIGHT, HUD_STRAIGHT},
    /*  9 TURN_FORK                     */ {HUD_FORK_LEFT, HUD_FORK_RIGHT, HUD_STRAIGHT},
    /* 10 TURN_MERGE                    */ {HUD_MERGE_LEFT, HUD_MERGE_RIGHT, HUD_STRAIGHT},
    /* 11 TURN_ROUNDABOUT_ENTER         */ {0, 0, 0},
    /* 12 TURN_ROUNDABOUT_EXIT          */ {0, 0, 0},
    /* 13 TURN_ROUNDABOUT_ENTER_AND_EXIT*/ {0, 0, 0},  // handled by roundabout_icon()
    /* 14 TURN_STRAIGHT                 */ {HUD_STRAIGHT, HUD_STRAIGHT, HUD_STRAIGHT},
    /* 15 unassigned in proto           */ {0, 0, 0},
    /* 16 TURN_FERRY_BOAT               */ {0, 0, 0},
    /* 17 TURN_FERRY_TRAIN              */ {0, 0, 0},
    /* 18 unassigned in proto           */ {0, 0, 0},
    /* 19 TURN_DESTINATION              */ {HUD_DESTINATION_LEFT, HUD_DESTINATION_RIGHT, HUD_DESTINATION},
};

// Roundabout icon for an exit angle. The HUD has 12 roundabout icons, one per
// 30 degrees, for right-hand traffic (37..48) and for left-hand traffic
// (49..60).
uint8_t roundabout_icon(int32_t degrees, int32_t side_index_lr)
{
    // The first icon of each set points down (back along the entry road) and
    // the icons go round anticlockwise, while the exit angle is 0 for straight
    // on and grows clockwise. So the icon index is (180 - angle), rounded to the
    // nearest 30 degrees. Checked on the HUD: straight on -> 43 (up),
    // right -> 40, left -> 46, back -> 37.
    int32_t d = (180 - degrees) % 360;
    if (d < 0) d += 360;
    uint8_t nearest = static_cast<uint8_t>(((d + 15) / 30) % 12);
    uint8_t offset  = (side_index_lr == 0) ? 49 : 37;
    return static_cast<uint8_t>(nearest + offset);
}

// HUD distance unit for an Android Auto DISPLAY_DISTANCE_UNIT.
//   Android Auto: 1 metres, 2 kilometres (tenths), 3 kilometres,
//                 4 miles (tenths), 5 miles, 6 feet
//   HUD (HudDistanceUnit in headunit's hud.h): 1 metres, 2 miles,
//                 3 kilometres, 4 yards, 5 feet
uint8_t map_distance_unit(uint32_t android_unit)
{
    switch (android_unit) {
    case 1: return 1;  // METERS       -> METERS
    case 2: return 3;  // KILOMETERS10 -> KILOMETERS
    case 3: return 3;  // KILOMETERS   -> KILOMETERS
    case 4: return 2;  // MILES10      -> MILES
    case 5: return 2;  // MILES        -> MILES
    case 6: return 5;  // FEET         -> FEET
    default: return 0; // unknown: no unit
    }
}

// ---- Shared state -------------------------------------------------------------

// What the HUD should show. Written by the hud_on_*() functions under the
// sequence lock, read by the sender.
struct NaviSnapshot {
    char     road_name[256];     // NUL-terminated UTF-8
    uint32_t turn_side;          // 1 left, 2 right, 3 none
    uint32_t turn_event;         // Android Auto turn event, 0..19
    int32_t  turn_angle;         // degrees, signed
    int32_t  turn_number;        // maneuver / exit number
    int32_t  distance_dec;       // distance * 10, in distance_unit
    uint8_t  distance_unit;      // HUD distance unit
};

NaviSnapshot            g_snapshot   = {};
std::atomic<uint32_t>   g_seq{0};
std::condition_variable g_cv;
std::mutex              g_cv_mu;
std::atomic<bool>       g_stop{false};          // the sender should stop
std::atomic<uint32_t>   g_tick{0};              // counts keep-alive ticks
std::atomic<bool>       g_have_guidance{false}; // a route is active: repeat the frame on each tick
std::atomic<bool>       g_clear_req{false};     // navigation ended: clear the HUD

// The service bus connection. Created, used and freed by the sender thread
// only.
void *g_conn = nullptr;

// Set when VBS_NAVI_GetHUDStatus reports that the car has no HUD; frames are
// then not sent. It starts false, so a lost or late reply never stops the HUD,
// at the cost of a few unused frames on a car without one.
std::atomic<bool> g_hud_absent{false};

pthread_t g_sender_thread       = 0;
bool      g_sender_thread_up    = false;
pthread_t g_ticker_thread       = 0;
bool      g_ticker_up           = false;

// libjcidbus calls this as cb(conn, closure) on a connection error. It must
// not be NULL: the library calls it without checking.
extern "C" void hud_dbus_error_cb(void *conn, void *closure)
{
    (void)conn; (void)closure;
    LOGW("hud sender: libjcidbus signalled a service-bus connection error "
         "(non-fatal; exit-on-disconnect is off)");
}

// Reply to VBS_NAVI_GetHUDStatus: cb(conn, status, user). Status 0 means the
// car has no HUD.
extern "C" void hud_status_cb(void *conn, unsigned char status, void *user)
{
    (void)conn; (void)user;
    const bool absent = (status == 0);
    g_hud_absent.store(absent, std::memory_order_release);
    LOGD("hud sender: GetHUDStatus reply = %u (%s)",
         static_cast<unsigned>(status), absent ? "no HUD" : "HUD present");
}

// Sends `cur` if it differs from `prev`, or always when `force` is set (the
// keep-alive). sync_bit belongs to the sender loop. Returns false if an OEM
// call failed; the next keep-alive tick tries again.
bool send_one(const NaviSnapshot &cur,
              const NaviSnapshot &prev,
              uint8_t            &sync_bit,
              bool                force)
{
    // No HUD in this car.
    if (g_hud_absent.load(std::memory_order_acquire)) return true;

    const bool maneuver_changed =
        std::strncmp(cur.road_name, prev.road_name, sizeof(cur.road_name)) != 0 ||
        cur.turn_event  != prev.turn_event ||
        cur.turn_side   != prev.turn_side  ||
        cur.turn_number != prev.turn_number;
    const bool changed = maneuver_changed ||
        cur.distance_dec  != prev.distance_dec  ||
        cur.distance_unit != prev.distance_unit ||
        cur.turn_angle    != prev.turn_angle;
    if (!changed && !force) return true;

    // A new sync_bit value (cycling 1..7) makes the HUD redraw. It changes
    // whenever the maneuver changes, even if the road name is the same;
    // distance updates and keep-alives leave it alone.
    if (maneuver_changed) {
        sync_bit = static_cast<uint8_t>((sync_bit % 7) + 1);
    }

    // Road name.
#if CARPLAY_VN_NORMALIZE
    char hud_name[sizeof(cur.road_name)];
    normalize_vn(cur.road_name, hud_name, sizeof(hud_name));
    std::string road = hud_name;
#else
    std::string road = cur.road_name;
#endif
    // An empty road name makes the HUD draw garbage; a space draws nothing.
    if (road.empty()) road = " ";
    VbsNaviHudMsg2 msg2 = {};
    msg2.guidancePointName = road.c_str();
    msg2.syncBit           = sync_bit;

    // Turn icon and distance.
    uint32_t icon = 0;
    if (cur.turn_event == 13 /*TURN_ROUNDABOUT_ENTER_AND_EXIT*/) {
        int32_t side_lr = (cur.turn_side == 1) ? 0 : 1;
        icon = roundabout_icon(cur.turn_angle, side_lr);
    } else if (cur.turn_event < 20) {
        int32_t side_idx = static_cast<int32_t>(cur.turn_side) - 1;
        if (side_idx < 0 || side_idx > 2) side_idx = 2;
        icon = kTurnIcons[cur.turn_event][side_idx];
    }
    // Never send unit 0 with a maneuver, and clamp the distance to 16 bits so
    // a far-away maneuver cannot wrap round to a small number.
    int32_t draw = cur.distance_dec;
    if (draw < 0)      draw = 0;
    if (draw > 0xFFFF) draw = 0xFFFF;
    VbsNaviHudDisplay disp = {};
    disp.nextManeuverInfo = icon;
    disp.distanceValue    = static_cast<uint16_t>(draw);
    disp.distanceUnit     = cur.distance_unit ? cur.distance_unit
                                              : static_cast<uint8_t>(1);
    disp.text_ID3         = sync_bit;

    // Like the built-in navigation (thUpdateGuidanceChangeToHUD), always send
    // both messages together, road name included, even when only the distance
    // changed. A road name sent on its own would fade out on the HUD between
    // turns.
    LOGV("hud_send: SetHUDDisplayMsgReq icon=%u dist=%u unit=%u sync=%u",
         static_cast<unsigned>(disp.nextManeuverInfo),
         static_cast<unsigned>(disp.distanceValue),
         static_cast<unsigned>(disp.distanceUnit),
         static_cast<unsigned>(disp.text_ID3));
    int rc = VBS_NAVI_SetHUDDisplayMsgReq(g_conn, &disp, nullptr, nullptr, nullptr);
    if (rc != 0) { LOGE("hud_send: SetHUDDisplayMsgReq failed rc=%d", rc); return false; }

    LOGV("hud_send: SetHUD_Display_Msg2 road=\"%s\" sync=%u",
         road.c_str(), static_cast<unsigned>(msg2.syncBit));
    rc = VBS_NAVI_TMC_SetHUD_Display_Msg2(g_conn, &msg2, nullptr, nullptr, nullptr);
    if (rc != 0) { LOGE("hud_send: SetHUD_Display_Msg2 failed rc=%d", rc); return false; }
    return true;
}

// Sends an empty frame and a blank road name, which clears the HUD. Used at
// start-up (the HUD keeps the last frame across a restart) and when
// navigation ends. Returns false if an OEM call failed.
bool send_clear(uint8_t &sync_bit)
{
    if (g_conn == nullptr) return false;
    if (g_hud_absent.load(std::memory_order_acquire)) return true;   // no HUD -> nothing to wipe

    VbsNaviHudDisplay disp = {};
    int rc = VBS_NAVI_SetHUDDisplayMsgReq(g_conn, &disp, nullptr, nullptr, nullptr);
    if (rc != 0) { LOGE("hud_send: clear Msg1 failed rc=%d", rc); return false; }

    sync_bit = static_cast<uint8_t>((sync_bit % 7) + 1);   // make the HUD redraw
    VbsNaviHudMsg2 msg2 = {};
    const char *blank = " ";                     // see send_one: never an empty string
    msg2.guidancePointName = blank;
    msg2.syncBit           = sync_bit;
    rc = VBS_NAVI_TMC_SetHUD_Display_Msg2(g_conn, &msg2, nullptr, nullptr, nullptr);
    if (rc != 0) { LOGE("hud_send: clear Msg2 failed rc=%d", rc); return false; }

    LOGD("hud_send: HUD cleared (blank maneuver + street)");
    return true;
}

// Connects to the service bus, starts libjcidbus's worker thread and asks
// whether the car has a HUD. Returns true once connected.
bool sender_setup()
{
    if (g_stop.load(std::memory_order_relaxed)) return false;

    g_conn = JCIDBUS_conn_create(reinterpret_cast<void *>(&hud_dbus_error_cb), 0);
    if (g_conn == nullptr) {
        LOGC("hud sender: JCIDBUS_conn_create failed (OEM symbols unavailable?) "
             "— no HUD this session");
        return false;
    }

    LOGD("hud sender: connecting to service bus as %s", kBusName);
    if (JCIDBUS_conn_connect(g_conn, kBusName, kJciServiceBus, nullptr) == 0) {
        LOGE("hud sender: JCIDBUS_conn_connect failed — no HUD this session");
        JCIDBUS_conn_free(g_conn);
        g_conn = nullptr;
        return false;
    }

    JCIDBUS_worker_start(g_conn);
    LOGD("hud sender: service bus connected, worker started");

    if (g_stop.load(std::memory_order_relaxed)) return false;

    VBS_NAVI_GetHUDStatus(g_conn, reinterpret_cast<void *>(&hud_status_cb), nullptr);
    LOGD("hud sender: HUD-status probe sent (sending enabled, fail-open)");
    return true;
}

// Clears the HUD and closes the connection. Does nothing if there is none.
void sender_teardown()
{
    if (g_conn == nullptr) return;

    if (!g_hud_absent.load(std::memory_order_acquire)) {
        VbsNaviHudDisplay clear = {};
        int rc = VBS_NAVI_SetHUDDisplayMsgReq(g_conn, &clear, nullptr, nullptr, nullptr);
        if (rc != 0) LOGE("hud sender: clear-frame send failed rc=%d", rc);
        else         LOGD("hud sender: sent HUD clear frame");
        usleep(50 * 1000);   // let the worker send it before it is stopped
    }

    JCIDBUS_worker_stop(g_conn);
    JCIDBUS_conn_disconnect(g_conn);
    JCIDBUS_conn_free(g_conn);
    g_conn = nullptr;
    LOGD("hud sender: worker stopped, connection freed");
}

// Wakes the sender twice a second for the keep-alive. It sleeps with usleep()
// rather than using condition_variable::wait_for(), which needs
// std::chrono::steady_clock from GLIBCXX_3.4.19; the head unit's libstdc++
// is older.
void *ticker_main(void *)
{
    while (!g_stop.load(std::memory_order_relaxed)) {
        usleep(500 * 1000);   // the built-in navigation's rate
        g_tick.fetch_add(1, std::memory_order_relaxed);
        g_cv.notify_one();
    }
    return nullptr;
}

void *sender_main(void *)
{
    if (!sender_setup()) {
        sender_teardown();
        LOGD("hud sender: setup did not complete; thread exiting");
        return nullptr;
    }
    LOGD("hud sender: HUD plumbing ready (OEM transport)");

    if (pthread_create(&g_ticker_thread, nullptr, ticker_main, nullptr) == 0) {
        g_ticker_up = true;
    } else {
        LOGC("hud sender: failed to start the keep-alive ticker (the HUD still updates on changes)");
    }

    NaviSnapshot prev = {};
    uint8_t      sync_bit = 0;
    uint32_t     last_processed = 0;
    uint32_t     last_tick = 0;

    // Clear whatever the HUD kept from before, unless a route is already
    // being shown.
    if (!g_have_guidance.load(std::memory_order_relaxed)) {
        if (send_clear(sync_bit))
            LOGD("hud sender: startup HUD clear (no active route)");
    }

    while (!g_stop.load(std::memory_order_relaxed)) {
      try {  // an exception leaving this thread would abort jciCARPLAY
        // Read the snapshot: the copy is whole if g_seq was even and unchanged
        // around it; otherwise read again.
        NaviSnapshot snap;
        uint32_t s1, s2;
        for (;;) {
            s1 = g_seq.load(std::memory_order_acquire);
            if (s1 & 1u) { sched_yield(); continue; }
            std::memcpy(&snap, &g_snapshot, sizeof(snap));
            std::atomic_thread_fence(std::memory_order_acquire);
            s2 = g_seq.load(std::memory_order_relaxed);
            if (s1 == s2) break;
        }

        if (s2 != last_processed) {
            send_one(snap, prev, sync_bit, /*force=*/false);
            prev = snap;
            last_processed = s2;
        } else if (g_have_guidance.load(std::memory_order_relaxed)) {
            // Woken by the ticker with nothing new: send the last frame again.
            send_one(prev, prev, sync_bit, /*force=*/true);
        }

        // Clear the HUD when navigation or the CarPlay session ended (see
        // carplay_status_start in devmgr_shim.cpp). The callbacks only set a
        // flag; the clear happens here, so the snapshot keeps a single writer.
        // The HUD is not cleared just because updates pause, as they do while
        // the car waits at a light.
        if (g_clear_req.exchange(false, std::memory_order_relaxed)) {
            LOGD("hud: navigation ended, clearing the HUD");
            if (send_clear(sync_bit)) {
                g_have_guidance.store(false, std::memory_order_relaxed);
                prev       = {};
                last_processed = g_seq.load(std::memory_order_acquire);
                nav_request_reset();
            }
        }

        // A failed send is not retried here and the connection is not
        // remade: the keep-alive sends again once the bus is back.

        // Sleep until stopped, the snapshot changes, or the ticker ticks.
        last_tick = g_tick.load(std::memory_order_relaxed);
        std::unique_lock<std::mutex> lk(g_cv_mu);
        g_cv.wait(lk, [&]() {
            return g_stop.load(std::memory_order_relaxed) ||
                   g_seq.load(std::memory_order_acquire) != last_processed ||
                   g_tick.load(std::memory_order_relaxed) != last_tick;
        });
      } catch (...) {
        // Log, wait a moment and carry on.
        LOGE("hud: sender loop exception swallowed — continuing (no thread escape)");
        usleep(200 * 1000);
      }
    }

    if (g_ticker_up) {
        pthread_join(g_ticker_thread, nullptr);
        g_ticker_up     = false;
        g_ticker_thread = 0;
    }
    sender_teardown();
    LOGD("hud sender: thread exiting cleanly");
    return nullptr;
}

// Sequence lock around a snapshot write.
inline void seqlock_begin() { g_seq.fetch_add(1, std::memory_order_acq_rel); }
inline void seqlock_end()   { g_seq.fetch_add(1, std::memory_order_acq_rel);
                              g_cv.notify_one(); }

} // namespace

// ---- Lifecycle ----------------------------------------------------------------

void hud_send_start(void)
{
    if (g_sender_thread_up) {
        LOGD("hud_send_start: already running");
        return;
    }
    // Only starts the thread; it connects to the bus itself.
    g_stop.store(false, std::memory_order_release);

    if (pthread_create(&g_sender_thread, nullptr, sender_main, nullptr) != 0) {
        LOGC("hud_send_start: failed to spawn sender thread");
        return;
    }
    g_sender_thread_up = true;
    LOGD("hud_send_start: sender thread spawned (OEM D-Bus setup deferred to thread)");
}

// Not called at present: the sender runs for the life of the process.
void hud_send_stop(void)
{
    if (!g_sender_thread_up) return;

    // The sender clears the HUD, closes the connection and stops the ticker
    // on its way out.
    g_stop.store(true, std::memory_order_release);
    g_have_guidance.store(false, std::memory_order_relaxed);
    g_cv.notify_all();

    pthread_join(g_sender_thread, nullptr);
    g_sender_thread_up = false;
    g_sender_thread    = 0;
    LOGD("hud_send_stop: sender thread stopped, OEM D-Bus released");
}

// Called on the OEM D-Bus worker thread (devmgr_shim.cpp). Only sets a flag;
// the sender clears the HUD on its next pass, within half a second.
void hud_request_clear(void)
{
    g_clear_req.store(true, std::memory_order_relaxed);
    g_cv.notify_one();
}

// ---- Updates from nav.cpp (device manager receive thread) -------------------

void hud_on_status(uint32_t status)
{
    // The snapshot is kept current even before the sender is up, so a
    // sender that starts mid-route shows the route at once.
    if (status != 1u) {
        g_have_guidance.store(false, std::memory_order_relaxed);
        seqlock_begin();
        std::memset(&g_snapshot, 0, sizeof(g_snapshot));
        seqlock_end();
    } else {
        g_have_guidance.store(true, std::memory_order_relaxed);
        g_cv.notify_one();
    }
}

void hud_on_next_turn(const char *road_name,
                      uint32_t    turn_side,
                      uint32_t    turn_event,
                      int32_t     turn_angle,
                      int32_t     turn_number)
{
    seqlock_begin();
    if (road_name) {
        // Truncate a long name at a character boundary, never inside a
        // multi-byte UTF-8 sequence.
        const size_t max = sizeof(g_snapshot.road_name) - 1;
        size_t n = strnlen(road_name, max + 1);
        if (n > max) {
            n = max;
            while (n > 0 &&
                   (static_cast<unsigned char>(road_name[n]) & 0xC0) == 0x80) {
                n--;
            }
        }
        std::memcpy(g_snapshot.road_name, road_name, n);
        g_snapshot.road_name[n] = '\0';
    } else {
        g_snapshot.road_name[0] = '\0';
    }
    g_snapshot.turn_side   = turn_side;
    g_snapshot.turn_event  = turn_event;
    g_snapshot.turn_angle  = turn_angle;
    g_snapshot.turn_number = turn_number;
    seqlock_end();
    g_have_guidance.store(true, std::memory_order_relaxed);
}

void hud_on_distance(int32_t display_distance, uint32_t display_distance_unit)
{
    seqlock_begin();
    g_snapshot.distance_dec  = display_distance / 100;   // value * 1000 -> value * 10
    g_snapshot.distance_unit = map_distance_unit(display_distance_unit);
    seqlock_end();
}
