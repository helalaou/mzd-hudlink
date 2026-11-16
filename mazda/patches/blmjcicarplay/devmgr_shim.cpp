// SPDX-License-Identifier: AGPL-3.0-or-later
//
// HUDLink: CarPlay directions on the Mazda Connect (CMU150) HUD.
// Copyright (C) 2026 KID MIXER-MODER (the original CarPlay -> HUD bridge).
// Copyright (C) 2026 The HUDLink authors (changes since; see AUTHORS).
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Hooks into the device manager client API (libdevmgr_interface.so) that
// jciCARPLAY uses to talk to the iPhone, and into libc msgrcv().
//
// Arming. Once a CarPlay session is up, jciCARPLAY calls
// eDevMgriPodStartCallStateUpdates, eDevMgriPodStartCommunicationUpdates and
// eDevMgriPodSendLocationInformation. The wrappers below pass each call on
// unchanged and then arm navigation (arm_navi): start the HUD sender,
// subscribe to the OEM CarPlay status signals, and ask the iPhone for route
// guidance with eDevMgriPodStartNaviRouteGuidenceUpdates, which jciCARPLAY
// never calls on its own. A background thread repeats that request every
// 5 seconds for the life of the process.
//
// Receiving. The device manager's receive thread (vDevMgrProcessRcvThread)
// runs inside jciCARPLAY and takes every iAP2 event off a System V message
// queue with msgrcv() (found by disassembling libdevmgr_interface.so). The
// msgrcv() wrapper calls the real function and, once navigation is armed,
// hands the large messages to nav.cpp. It never changes a message, so
// jciCARPLAY receives exactly what it would without the bridge.
//
// Every wrapped devmgr function takes four word-sized arguments in r0..r3
// (ARM EABI), so each is declared with four void* and forwards all four.

#include "patch.h"
#include "oem/libjcidbus.h"

#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

namespace {

typedef int (*pfn4)(void *, void *, void *, void *);

pfn4  g_real_register  = nullptr;
pfn4  g_real_callstate = nullptr;
pfn4  g_real_commupd   = nullptr;
pfn4  g_real_sendloc   = nullptr;
pfn4  g_real_startnavi = nullptr;

void *g_devmgr_handle = nullptr;
pthread_mutex_t g_mu        = PTHREAD_MUTEX_INITIALIZER;
bool            g_navi_armed = false;
volatile int    g_armed      = 0;   // set by arm_navi(); until then the msgrcv hook only passes through
void *g_procH = nullptr;
void *g_devH  = nullptr;

extern "C" int eDevMgrRegisterProcess(void *, void *, void *, void *);
extern "C" int eDevMgriPodStartCallStateUpdates(void *, void *, void *, void *);
extern "C" int eDevMgriPodStartCommunicationUpdates(void *, void *, void *, void *);
extern "C" int eDevMgriPodSendLocationInformation(void *, void *, void *, void *);

// True if `a` is one of the wrappers in this file rather than the OEM function.
bool addr_is_own(void *a)
{
    return a == (void *)&eDevMgrRegisterProcess ||
           a == (void *)&eDevMgriPodStartCallStateUpdates ||
           a == (void *)&eDevMgriPodStartCommunicationUpdates ||
           a == (void *)&eDevMgriPodSendLocationInformation;
}

// The real libdevmgr_interface function behind one of the wrappers.
// dlsym(RTLD_NEXT) normally finds it; if that yields a wrapper instead, look
// the name up in the already-loaded library itself.
void *resolve_oem(const char *name)
{
    void *p = dlsym(RTLD_NEXT, name);
    if (p && !addr_is_own(p)) return p;
    if (!g_devmgr_handle) {
        g_devmgr_handle = dlopen("libdevmgr_interface.so", RTLD_NOW | RTLD_NOLOAD);
        if (!g_devmgr_handle)
            g_devmgr_handle = dlopen("/usr/lib/libdevmgr_interface.so", RTLD_NOW | RTLD_NOLOAD);
        if (!g_devmgr_handle) { LOGE("resolve_oem: dlopen NOLOAD failed: %s", dlerror()); return nullptr; }
    }
    p = dlsym(g_devmgr_handle, name);
    if (!p) { LOGE("resolve_oem(%s): dlsym failed: %s", name, dlerror()); return nullptr; }
    if (addr_is_own(p)) { LOGC("resolve_oem(%s): interposition trap", name); return nullptr; }
    return p;
}

void resolve_all_once(void)
{
    if (!g_real_register)  g_real_register  = (pfn4)resolve_oem("eDevMgrRegisterProcess");
    if (!g_real_callstate) g_real_callstate = (pfn4)resolve_oem("eDevMgriPodStartCallStateUpdates");
    if (!g_real_commupd)   g_real_commupd   = (pfn4)resolve_oem("eDevMgriPodStartCommunicationUpdates");
    if (!g_real_sendloc)   g_real_sendloc   = (pfn4)resolve_oem("eDevMgriPodSendLocationInformation");
    if (!g_real_startnavi) g_real_startnavi = (pfn4)resolve_oem("eDevMgriPodStartNaviRouteGuidenceUpdates");
}

// Repeats the route guidance request every 5 seconds, alternating the third
// argument between 0 and 0x5555.
void *navi_retry_thread(void *)
{
    int n = 0;
    for (;;) {
        sleep(5);
        if (!g_real_startnavi || !g_procH) continue;
        void *p3 = (n & 1) ? (void *)0x5555 : (void *)0;
        int rc = g_real_startnavi(g_procH, g_devH, p3, (void *)0);
        // Log the first few, then only a changed result and every 60th
        // (about every 5 minutes), so a long drive does not fill the log.
        static int last_rc = 0x7fffffff;
        if (n < 6 || rc != last_rc || (n % 60) == 0)
            LOGD("retry#%d StartNaviRouteGuidence(procH=%p devH=%p p3=%p) rc=%d",
                 n, g_procH, g_devH, p3, rc);
        last_rc = rc;
        n++;
    }
    return nullptr;
}

// ---- CarPlay status signals ----------------------------------------------
// jciCARPLAY publishes CarPlay's state on the HMI bus (interface
// com.jci.carplay). The bridge subscribes to two of its signals through the
// OEM's own client library, libjcicarplay_client.so, which does the D-Bus
// matching and decoding:
//   TurnByTurnEntitySignal(entityType)  who shows turn-by-turn directions:
//                                       0 nobody, 1 CarPlay, 2 the built-in
//                                       navigation
//   SessionDeactiveSignal               the CarPlay session ended
// The HUD is cleared when CarPlay stops showing directions and when the
// session ends.
//
// Callback ABI, from the client library's signal dispatcher and from
// svcjcinavi, which listens to the same signals: cb(conn, field0 *, field1 *,
// userdata). Fields arrive by pointer and are freed as soon as the callback
// returns, so copy anything that is needed later.
typedef int (*pfn_cp_enable)(void *conn, int enable, void *cb, void *userdata);
pfn_cp_enable g_cp_tbt_enable      = nullptr;
pfn_cp_enable g_cp_deactive_enable = nullptr;
void *g_cp_hmi_conn   = nullptr;
void *g_cp_client_lib = nullptr;
bool  g_cp_subscribed = false;

void cp_hmi_err(void *conn, void *closure)
{ (void)conn; (void)closure; LOGW("carplay_status: HMI bus error cb (non-fatal)"); }

// TurnByTurnEntitySignal. Runs on the OEM D-Bus worker thread, so no exception
// may leave it.
void cp_tbt_entity_cb(void *conn, void *p_entity, void *p_aux, void *userdata)
{
    (void)conn; (void)p_aux; (void)userdata;
    try {
        unsigned e = p_entity ? *reinterpret_cast<const uint32_t *>(p_entity) : 0xffffffffu;
        LOGD("carplay TBT entity=%u (%s)",
             e, e == 1 ? "CARPLAY" : e == 2 ? "NATIVE" : e == 0 ? "NONE" : "?");
        // 1 means CarPlay is giving directions. 0 (navigation ended in the app)
        // or 2 (the built-in navigation took over) means it no longer is.
        if (e != 1) hud_request_clear();
    } catch (...) { LOGE("cp_tbt_entity_cb exception swallowed (OEM dbus thread)"); }
}

// SessionDeactiveSignal. Its fields (client id, type, reason, ...) are not
// needed: the signal itself means the session is gone.
void cp_deactive_cb(void *a0, void *a1, void *a2, void *a3)
{
    (void)a0; (void)a1; (void)a2; (void)a3;
    LOGD("carplay SessionDeactive (session ended)");
    hud_request_clear();
}

void *cp_resolve(const char *name)
{
    void *p = g_cp_client_lib ? dlsym(g_cp_client_lib, name) : nullptr;
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    return p;
}

// Subscribes once to the two signals on the HMI bus. Best effort: if any step
// fails, guidance still reaches the HUD, but the HUD keeps the last maneuver
// after navigation ends.
void carplay_status_start(void)
{
    if (g_cp_subscribed) return;
    // The CARPLAY_*Signal_enable functions are in the client library, which
    // jciCARPLAY (the publisher) does not load itself. Load it RTLD_GLOBAL so
    // its libjcidbus imports bind to the copy already in the process.
    if (!g_cp_client_lib) {
        g_cp_client_lib = dlopen("libjcicarplay_client.so", RTLD_NOW | RTLD_GLOBAL);
        if (!g_cp_client_lib)
            g_cp_client_lib = dlopen("/jci/lib/libjcicarplay_client.so", RTLD_NOW | RTLD_GLOBAL);
    }
    if (!g_cp_client_lib) { LOGW("carplay_status: dlopen libjcicarplay_client.so failed: %s", dlerror()); return; }
    g_cp_tbt_enable      = (pfn_cp_enable)cp_resolve("CARPLAY_TurnByTurnEntitySignal_enable");
    g_cp_deactive_enable = (pfn_cp_enable)cp_resolve("CARPLAY_SessionDeactiveSignal_enable");
    if (!g_cp_tbt_enable) { LOGW("carplay_status: TurnByTurnEntitySignal_enable unresolved, the HUD will not clear at navigation end"); return; }

    g_cp_hmi_conn = JCIDBUS_conn_create(reinterpret_cast<void *>(&cp_hmi_err), 0);
    if (!g_cp_hmi_conn) { LOGW("carplay_status: HMI conn_create failed"); return; }
    if (JCIDBUS_conn_connect(g_cp_hmi_conn, "com.jci.carplay.hudstatus", kJciHmiBus, nullptr) == 0) {
        LOGW("carplay_status: HMI conn_connect failed, the HUD will not clear at navigation end");
        JCIDBUS_conn_free(g_cp_hmi_conn); g_cp_hmi_conn = nullptr; return;
    }
    JCIDBUS_worker_start(g_cp_hmi_conn);
    int re = g_cp_tbt_enable(g_cp_hmi_conn, 1, reinterpret_cast<void *>(&cp_tbt_entity_cb), nullptr);
    int rd = g_cp_deactive_enable
               ? g_cp_deactive_enable(g_cp_hmi_conn, 1, reinterpret_cast<void *>(&cp_deactive_cb), nullptr)
               : -1;
    g_cp_subscribed = true;
    LOGD("carplay_status: subscribed HMI com.jci.carplay TurnByTurnEntity(rc=%d) + SessionDeactive(rc=%d)", re, rd);
}

void arm_navi(const char *from, void *procH, void *devH)
{
    // g_enabled is set by main.cpp only inside the jciCARPLAY process.
    if (!g_enabled) { LOGW("carplay: not enabled (%s)", from); return; }
    g_armed = 1;
    pthread_mutex_lock(&g_mu);
    g_procH = procH;
    g_devH  = devH;
    if (!g_navi_armed) {
        g_navi_armed = true;
        LOGD("arm_navi from %s: StartNaviRouteGuidence(procH=%p devH=%p 0)", from, procH, devH);
        hud_send_start();         // the HUD side must be up before maneuvers arrive
        carplay_status_start();   // to clear the HUD when navigation ends
        if (g_real_startnavi) {
            int rc = g_real_startnavi(procH, devH, (void *)0, (void *)0);
            LOGD("StartNaviRouteGuidence rc=%d; spawning retry+watch", rc);
            pthread_t th;
            if (pthread_create(&th, nullptr, navi_retry_thread, nullptr) == 0)
                pthread_detach(th);
        } else { LOGE("arm_navi: StartNavi unresolved"); g_navi_armed = false; }
    }
    pthread_mutex_unlock(&g_mu);
}

// ---- msgrcv hook -----------------------------------------------------------
typedef long (*pfn_msgrcv)(int, void *, unsigned, long, int);
pfn_msgrcv      g_real_msgrcv = nullptr;

void msg_tap(int msqid, const void *msgp, long n)
{
    (void)msqid;
    // The maneuver and guidance messages are the only ones of 0x300 bytes or
    // more. Nothing from them is written to disk: maneuver text is location data.
    if (n < 0x300 || !msgp) return;
    nav_on_devmgr_msg(msgp, n);
}

} // namespace

#ifdef HUDLINK_DIAG
// Diagnostic build: which message types reach jciCARPLAY, to tell whether the
// phone sends route guidance at all. Logs each new (queue, type) pair once,
// then every 30 seconds the counts that changed. Types and sizes only, never
// message contents.
namespace {
struct DiagEnt { int q; unsigned t; long maxn; unsigned long cnt; unsigned long logged; };
DiagEnt         g_de[48];
int             g_den   = 0;
time_t          g_dlast = 0;
pthread_mutex_t g_dmu   = PTHREAD_MUTEX_INITIALIZER;

void diag_count(int q, const void *msgp, long n)
{
    unsigned t = (n >= 4) ? (unsigned)((*(const uint32_t *)msgp) & 0xffff) : 0xffffu;
    pthread_mutex_lock(&g_dmu);
    int i = 0;
    while (i < g_den && !(g_de[i].q == q && g_de[i].t == t)) i++;
    if (i == g_den && g_den < 48) {
        g_de[i].q = q; g_de[i].t = t; g_de[i].maxn = 0; g_de[i].cnt = 0; g_de[i].logged = 0;
        g_den++;
        LOGD("diag msg: first q=%d type=0x%04x size=%ld armed=%d", q, t, n, (int)g_armed);
    }
    if (i < g_den) { g_de[i].cnt++; if (n > g_de[i].maxn) g_de[i].maxn = n; }
    time_t now = time(nullptr);
    if (now - g_dlast >= 30) {
        g_dlast = now;
        for (int k = 0; k < g_den; k++) {
            if (g_de[k].cnt == g_de[k].logged) continue;
            LOGD("diag msg: q=%d type=0x%04x count=%lu (+%lu) max=%ld armed=%d", g_de[k].q, g_de[k].t,
                 g_de[k].cnt, g_de[k].cnt - g_de[k].logged, g_de[k].maxn, (int)g_armed);
            g_de[k].logged = g_de[k].cnt;
        }
    }
    pthread_mutex_unlock(&g_dmu);
}
} // namespace
#endif

#define PRELOAD_EXPORT __attribute__((visibility("default")))

extern "C" PRELOAD_EXPORT
long msgrcv(int msqid, void *msgp, unsigned msgsz, long msgtyp, int msgflg)
{
    if (!g_real_msgrcv) g_real_msgrcv = (pfn_msgrcv)dlsym(RTLD_NEXT, "msgrcv");
    long n = g_real_msgrcv ? g_real_msgrcv(msqid, msgp, msgsz, msgtyp, msgflg) : -1;
#ifdef HUDLINK_DIAG
    if (g_enabled && n > 0 && msgp) diag_count(msqid, msgp, n);
#endif
    if (g_enabled && g_armed && n > 0) msg_tap(msqid, msgp, n);
    return n;
}

extern "C" PRELOAD_EXPORT
int eDevMgrRegisterProcess(void *procStruct, void *role, void *cbTable, void *ctx)
{
    resolve_all_once();
    LOGD("eDevMgrRegisterProcess: procStruct=%p role=%ld cbTable=%p ctx=%p",
         procStruct, (long)role, cbTable, ctx);
    if (!g_real_register) { LOGC("RegisterProcess: real unresolved"); return 0x103; }
    int rc = g_real_register(procStruct, role, cbTable, ctx);
    LOGD("eDevMgrRegisterProcess: rc=%d", rc);
    return rc;
}

extern "C" PRELOAD_EXPORT
int eDevMgriPodStartCallStateUpdates(void *a0, void *a1, void *a2, void *a3)
{
    resolve_all_once();
    LOGD("StartCallStateUpdates: a0=%p a1=%p a2=%p a3=%p", a0, a1, a2, a3);
    int rc = g_real_callstate ? g_real_callstate(a0, a1, a2, a3) : 0x103;
    arm_navi("CallState", a0, a1);
    return rc;
}

extern "C" PRELOAD_EXPORT
int eDevMgriPodStartCommunicationUpdates(void *a0, void *a1, void *a2, void *a3)
{
    resolve_all_once();
    int rc = g_real_commupd ? g_real_commupd(a0, a1, a2, a3) : 0x103;
    arm_navi("CommUpdates", a0, a1);
    return rc;
}

extern "C" PRELOAD_EXPORT
int eDevMgriPodSendLocationInformation(void *a0, void *a1, void *a2, void *a3)
{
    resolve_all_once();
    int rc = g_real_sendloc ? g_real_sendloc(a0, a1, a2, a3) : 0x103;
    arm_navi("SendLocation", a0, a1);
    return rc;
}
