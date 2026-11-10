// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CarPlay -> HUD bridge for Mazda Connect (CMU150).
// Copyright (C) 2026 KID MIXER-MODER.
//
// Part of an AGPL-3.0 work - see NOTICE.md for full attribution.
//
// Entry point. The constructor below runs when the dynamic loader maps the
// library (LD_PRELOAD), before sm_svclauncher loads
// /jci/carplay/blmjcicarplay.so.
//
// The library is only active inside the jciCARPLAY process, which the
// system manager starts as
//   sm_svclauncher -l jciCARPLAY /jci/carplay/blmjcicarplay.so 0 -a
// blmjcicarplay.so exports nothing that could be probed with dlsym(), so the
// check reads /proc/self/cmdline instead. Any other process that inherits
// LD_PRELOAD keeps g_enabled false, and the hooks in devmgr_shim.cpp then
// only pass calls through to the real functions.
//
// The off switch. If jciCARPLAY ever dies of a fatal signal while the bridge
// is active, the signal handler writes the crash details to kOffMarker on the
// persistent partition. From then on the bridge stays inactive at every
// start, exactly as if it were not installed, so a fault cannot come back on
// the next drive. Installing the bridge again deletes the file.
//
// The diagnostic build (make diag) logs at debug level to kDiagLog on the
// persistent partition instead of /tmp, one file per start of jciCARPLAY with
// the two before it kept as kDiagLog1 and kDiagLog2, and starts each file with
// the process's command line and library search settings.

#include "patch.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <stdlib.h>
#include <unistd.h>

bool g_enabled = false;
std::FILE *g_logf = nullptr;

namespace {

const char kOffMarker[] = "/data_persist/cp-hud-mod/off-after-crash.txt";
#ifdef HUDLINK_DIAG
const char kDiagLog[]   = "/data_persist/cp-hud-mod/diag.log";
const char kDiagLog1[]  = "/data_persist/cp-hud-mod/diag.1.log";
const char kDiagLog2[]  = "/data_persist/cp-hud-mod/diag.2.log";
#endif

// kOffMarker, open while the signal handler writes the crash details.
int g_off_fd = -1;

pid_t gettid_compat(void)
{
#ifdef SYS_gettid
    return (pid_t)syscall(SYS_gettid);
#else
    return -1;
#endif
}

// Writes to stderr and, when it is open, to kOffMarker.
void write_crash(const char *p, size_t n)
{
    (void)!write(STDERR_FILENO, p, n);
    if (g_off_fd >= 0) (void)!write(g_off_fd, p, n);
}

// On SIGSEGV, SIGBUS, SIGABRT, SIGFPE or SIGILL, writes the signal, the
// registers, a backtrace and /proc/self/maps to stderr and to kOffMarker
// (which switches the bridge off, see above), then re-raises the signal with
// the default action. The ARM unwinder cannot step past the kernel's signal
// trampoline, so the registers and the memory map are what locate the fault.
// Only async-signal-safe calls are used.
void fatal_signal_handler(int sig, siginfo_t *info, void *ucontext)
{
    if (g_off_fd < 0)
        g_off_fd = open(kOffMarker, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    static const char hdr[] =
        "\n[libpatch-blmjcicarplay][C] *** fatal signal caught ***\n";
    write_crash(hdr, sizeof(hdr) - 1);

    char buf[256];
    int  m = snprintf(buf, sizeof(buf),
                 "[libpatch-blmjcicarplay][C] signal=%d pid=%d tid=%d "
                 "si_code=%d si_addr=%p\n",
                 sig, (int)getpid(), (int)gettid_compat(),
                 info ? info->si_code : -1, info ? info->si_addr : nullptr);
    if (m > 0) write_crash(buf, (size_t)m);

    if (ucontext) {
        ucontext_t *uc = static_cast<ucontext_t *>(ucontext);
        const mcontext_t &mc = uc->uc_mcontext;
        m = snprintf(buf, sizeof(buf),
                     "[libpatch-blmjcicarplay][C] regs: pc=%08lx lr=%08lx "
                     "sp=%08lx fp=%08lx ip=%08lx cpsr=%08lx fault=%08lx\n",
                     (unsigned long)mc.arm_pc, (unsigned long)mc.arm_lr,
                     (unsigned long)mc.arm_sp, (unsigned long)mc.arm_fp,
                     (unsigned long)mc.arm_ip, (unsigned long)mc.arm_cpsr,
                     (unsigned long)mc.fault_address);
        if (m > 0) write_crash(buf, (size_t)m);
    }

    void *frames[64];
    int   n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    if (g_off_fd >= 0) backtrace_symbols_fd(frames, n, g_off_fd);

    int mfd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (mfd >= 0) {
        char mbuf[4096];
        ssize_t rn;
        while ((rn = read(mfd, mbuf, sizeof(mbuf))) > 0) {
            ssize_t off = 0;
            while (off < rn) {
                ssize_t wn = write(STDERR_FILENO, mbuf + off, (size_t)(rn - off));
                if (wn <= 0) break;
                off += wn;
            }
            if (g_off_fd >= 0) (void)!write(g_off_fd, mbuf, (size_t)rn);
        }
        close(mfd);
    }
    if (g_off_fd >= 0) { fsync(g_off_fd); close(g_off_fd); g_off_fd = -1; }
    signal(sig, SIG_DFL);
    raise(sig);
}

void install_fatal_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = fatal_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
}

// /proc/self/cmdline with the NUL separators turned into spaces.
static int read_cmdline(char *out, size_t outsz)
{
    if (!out || outsz == 0) return 0;
    out[0] = '\0';
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    ssize_t n = read(fd, out, outsz - 1);
    close(fd);
    if (n <= 0) { out[0] = '\0'; return 0; }
    for (ssize_t i = 0; i < n; ++i) if (out[i] == '\0') out[i] = ' ';
    out[n] = '\0';
    return (int)n;
}

} // namespace

bool in_carplay_launcher(void)
{
    char cmdline[512];
    read_cmdline(cmdline, sizeof(cmdline));
    return (strstr(cmdline, "jciCARPLAY") != nullptr) ||
           (strstr(cmdline, "blmjcicarplay.so") != nullptr);
}

__attribute__((constructor))
static void on_load(void)
{
    if (!in_carplay_launcher()) {
        // Some other process that inherited LD_PRELOAD, such as a child the
        // launcher starts. Do nothing.
        return;
    }
#ifdef HUDLINK_DIAG
    (void)std::rename(kDiagLog1, kDiagLog2);
    (void)std::rename(kDiagLog, kDiagLog1);
    g_logf = std::fopen(kDiagLog, "w");
#else
    // The log file is in /tmp (RAM), appended to across restarts of the
    // process.
    g_logf = std::fopen("/tmp/carplay_bridge.log", "a");
#endif
    // Line-buffered, so `tail -f` shows lines as they come.
    if (g_logf) std::setvbuf(g_logf, nullptr, _IOLBF, 0);
#ifdef HUDLINK_DIAG
    {
        char cmd[512];
        read_cmdline(cmd, sizeof(cmd));
        const char *lp = getenv("LD_LIBRARY_PATH"), *pre = getenv("LD_PRELOAD");
        LOGD("=== HUDLink diagnostic build loaded, pid=%d, cmdline: %s", (int)getpid(), cmd);
        LOGD("LD_LIBRARY_PATH=%s", lp ? lp : "(not set)");
        LOGD("LD_PRELOAD=%s", pre ? pre : "(not set)");
    }
#endif
    if (access(kOffMarker, F_OK) == 0) {
        LOGE("switched off after a crash (%s), staying inert", kOffMarker);
        return;
    }
    g_enabled = true;
    install_fatal_handler();
    LOGD("=== loaded into jciCARPLAY launcher pid=%d ppid=%d (CarPlay->HUD bridge, FW 74.00.324) ===",
         (int)getpid(), (int)getppid());
}
