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

#include "patch.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <unistd.h>

bool g_enabled = false;
std::FILE *g_logf = nullptr;

namespace {

pid_t gettid_compat(void)
{
#ifdef SYS_gettid
    return (pid_t)syscall(SYS_gettid);
#else
    return -1;
#endif
}

// On SIGSEGV, SIGBUS, SIGABRT, SIGFPE or SIGILL, writes the signal, the
// registers, a backtrace and /proc/self/maps to stderr, then re-raises the
// signal with the default action. The ARM unwinder cannot step past the
// kernel's signal trampoline, so the registers and the memory map are what
// locate the fault. Only async-signal-safe calls are used.
void fatal_signal_handler(int sig, siginfo_t *info, void *ucontext)
{
    static const char hdr[] =
        "\n[libpatch-blmjcicarplay][C] *** fatal signal caught ***\n";
    (void)!write(STDERR_FILENO, hdr, sizeof(hdr) - 1);

    char buf[256];
    int  m = snprintf(buf, sizeof(buf),
                 "[libpatch-blmjcicarplay][C] signal=%d pid=%d tid=%d "
                 "si_code=%d si_addr=%p\n",
                 sig, (int)getpid(), (int)gettid_compat(),
                 info ? info->si_code : -1, info ? info->si_addr : nullptr);
    if (m > 0) (void)!write(STDERR_FILENO, buf, (size_t)m);

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
        if (m > 0) (void)!write(STDERR_FILENO, buf, (size_t)m);
    }

    void *frames[64];
    int   n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);

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
        }
        close(mfd);
    }
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
    g_enabled = true;
    // The log file is in /tmp (RAM), appended to across restarts of the
    // process and line-buffered so `tail -f` shows lines as they come.
    g_logf = std::fopen("/tmp/carplay_bridge.log", "a");
    if (g_logf) std::setvbuf(g_logf, nullptr, _IOLBF, 0);
    install_fatal_handler();
    LOGD("=== loaded into jciCARPLAY launcher pid=%d ppid=%d (CarPlay->HUD bridge, FW 74.00.324) ===",
         (int)getpid(), (int)getppid());
}
