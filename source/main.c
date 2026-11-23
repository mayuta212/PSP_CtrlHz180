/*
 * CtrlHz180 v1.5
 * PSP controller sampling: default cycle -> 5555 us (~180 Hz).
 * 2-sample release debounce is enabled by default.
 * Single-item Read/Peek: buttons only. Analog axes and wait semantics are untouched.
 */
#include <pspkernel.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <systemctrl.h>

PSP_MODULE_INFO("CtrlHz180", 0x1007, 1, 5);
PSP_MAIN_THREAD_ATTR(0);

#define CTRL_MOD "sceController_Service"
#define CTRL_LIB "sceCtrl"
#define NID_SET_CYCLE 0x6A2774F3
#define NID_GET_CYCLE 0x02BAAD91
#define TARGET_CYCLE_US 5555
/* Debounce is the default; build with CTRL_DEBOUNCE=0 for comparison.
 * A single sampled OFF can be hidden, including during very fast tapping. */
#ifndef CTRL_DEBOUNCE
#define CTRL_DEBOUNCE 1
#endif

#if CTRL_DEBOUNCE
#define NID_PEEK_POS    0x3A622550
#define NID_PEEK_NEG    0xC152080A
#define NID_READ_POS    0x1F803938
#define NID_READ_NEG    0x60B81F86

/* Only PSP face buttons, D-pad, shoulders and START/SELECT. No system buttons. */
#define DB_MASK (PSP_CTRL_SELECT | PSP_CTRL_START | PSP_CTRL_UP | \
                 PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT | \
                 PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | \
                 PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE | \
                 PSP_CTRL_CROSS | PSP_CTRL_SQUARE)
#endif

static int (*realSetSamplingCycle)(int cycle) = NULL;
static int (*realGetSamplingCycle)(int *cycle) = NULL;
#if CTRL_DEBOUNCE
static int (*realPeekBufferPositive)(SceCtrlData *, int) = NULL;
static int (*realPeekBufferNegative)(SceCtrlData *, int) = NULL;
static int (*realReadBufferPositive)(SceCtrlData *, int) = NULL;
static int (*realReadBufferNegative)(SceCtrlData *, int) = NULL;
#endif

/* These are simple aligned integer states; no blocking/IO in the hooks. */
static volatile int g_virtual_cycle = 0;
static volatile int g_installed = 0;
static volatile int g_forced_cycle = 0;
#if CTRL_DEBOUNCE
/* Filter only while the physical controller cycle is 5555 us. */
static volatile int g_180hz_cycle = 0;
#endif

static u32 find_ctrl(u32 nid)
{
    return sctrlHENFindFunction(CTRL_MOD, CTRL_LIB, nid);
}

/* The caller sees the game's requested cycle and previous value. */
static int hookSetSamplingCycle(int cycle)
{
    int previous_virtual = g_virtual_cycle;
    int actual_cycle = (cycle == 0) ? TARGET_CYCLE_US : cycle;
    int ret = realSetSamplingCycle(actual_cycle);

    if (ret < 0)
        return ret;

    g_virtual_cycle = cycle;
    g_forced_cycle = (cycle == 0);
#if CTRL_DEBOUNCE
    g_180hz_cycle = (actual_cycle == TARGET_CYCLE_US);
#endif
    return previous_virtual;
}

static int hookGetSamplingCycle(int *cycle)
{
    int ret = realGetSamplingCycle(cycle);

    if (ret >= 0 && cycle != NULL) {
        int old_k1 = pspSdkSetK1(0);
        *cycle = g_virtual_cycle;
        pspSdkSetK1(old_k1);
    }
    return ret;
}

#if CTRL_DEBOUNCE
/*
 * Stateless, based on the last two hardware samples, not the poll count:
 * PRESS  = present in latest sample -> immediate.
 * RELEASE = absent from TWO consecutive controller-service samples.
 *
 * Read/Peek originals are called FIRST to preserve wait/return semantics.
 * A second non-blocking peek requests the two newest samples. It does not
 * advance ReadBuffer's queue. Only apply if the last TimeStamp matches
 * the sample returned to the game (otherwise fail open to original data).
 * This extends releases by one sampling period. A fast ON/OFF/ON can lose
 * its OFF edge. It does not reject isolated false ON samples and does not
 * modify analog axes, timestamps, multi-sample buffers or latch APIs.
 */
static void filter_single_sample(SceCtrlData *pad, int ret, int count, int negative)
{
    SceCtrlData samples[2];
    unsigned int current_positive;
    unsigned int filtered_positive;
    int n, old_k1;

    /* No 2-sample hold at slower game-requested sampling cycles. */
    if (!g_180hz_cycle || ret != 1 || count != 1 ||
        !pad || !realPeekBufferPositive)
        return;

    old_k1 = pspSdkSetK1(0);
    n = realPeekBufferPositive(samples, 2);
    if (n == 2 &&
        samples[1].TimeStamp == pad[0].TimeStamp &&
        (unsigned int)(samples[1].TimeStamp - samples[0].TimeStamp) <= 10000u &&
        samples[0].TimeStamp != samples[1].TimeStamp) {
        current_positive = negative ? ~pad[0].Buttons : pad[0].Buttons;
        filtered_positive = (current_positive & ~DB_MASK) |
                            ((current_positive | samples[0].Buttons) & DB_MASK);
        /* Change only the button word; Lx/Ly and the remaining data stay raw. */
        pad[0].Buttons = negative ? ~filtered_positive : filtered_positive;
    }
    pspSdkSetK1(old_k1);
}

static int hookPeekBufferPositive(SceCtrlData *p, int count)
{
    int ret = realPeekBufferPositive(p, count);
    filter_single_sample(p, ret, count, 0);
    return ret;
}
static int hookPeekBufferNegative(SceCtrlData *p, int count)
{
    int ret = realPeekBufferNegative(p, count);
    filter_single_sample(p, ret, count, 1);
    return ret;
}
static int hookReadBufferPositive(SceCtrlData *p, int count)
{
    int ret = realReadBufferPositive(p, count);
    filter_single_sample(p, ret, count, 0);
    return ret;
}
static int hookReadBufferNegative(SceCtrlData *p, int count)
{
    int ret = realReadBufferNegative(p, count);
    filter_single_sample(p, ret, count, 1);
    return ret;
}
#endif

static void flush_caches(void)
{
    sceKernelDcacheWritebackInvalidateAll();
    sceKernelIcacheClearAll();
}

/*
 * Preflight is deliberately performed BEFORE hooking any syscall:
 *   - both exports must exist
 *   - reading the real initial cycle must work
 *   - if stock mode is active, switching to 5555 us must succeed
 *
 * sctrlHENPatchSyscall() returns void; it cannot report patch failure.
 * This plugin must not be stacked with another sceCtrl cycle-hooking plugin.
 */
static int install_hooks(void)
{
    u32 set_addr = find_ctrl(NID_SET_CYCLE);
    u32 get_addr = find_ctrl(NID_GET_CYCLE);
    int initial_cycle = 0;
    int get_ret;
    int old_k1;
#if CTRL_DEBOUNCE
    u32 peek_pos = find_ctrl(NID_PEEK_POS);
    u32 peek_neg = find_ctrl(NID_PEEK_NEG);
    u32 read_pos = find_ctrl(NID_READ_POS);
    u32 read_neg = find_ctrl(NID_READ_NEG);
    if (!peek_pos || !peek_neg || !read_pos || !read_neg)
        return -4;
#endif

    if (!set_addr || !get_addr)
        return -1;

    realSetSamplingCycle = (void *)set_addr;
    realGetSamplingCycle = (void *)get_addr;
#if CTRL_DEBOUNCE
    realPeekBufferPositive = (void *)peek_pos;
    realPeekBufferNegative = (void *)peek_neg;
    realReadBufferPositive = (void *)read_pos;
    realReadBufferNegative = (void *)read_neg;
#endif

    old_k1 = pspSdkSetK1(0);
    get_ret = realGetSamplingCycle(&initial_cycle);
    pspSdkSetK1(old_k1);
    if (get_ret < 0)
        return -2;

    if (initial_cycle == 0) {
        int ret = realSetSamplingCycle(TARGET_CYCLE_US);
        if (ret < 0)
            return -3;
        g_forced_cycle = 1;
    } else {
        g_forced_cycle = 0;
    }
    g_virtual_cycle = initial_cycle;
#if CTRL_DEBOUNCE
    g_180hz_cycle = (initial_cycle == 0 || initial_cycle == TARGET_CYCLE_US);
#endif

    sctrlHENPatchSyscall((void *)set_addr, (void *)hookSetSamplingCycle);
    sctrlHENPatchSyscall((void *)get_addr, (void *)hookGetSamplingCycle);
#if CTRL_DEBOUNCE
    sctrlHENPatchSyscall((void *)peek_pos, (void *)hookPeekBufferPositive);
    sctrlHENPatchSyscall((void *)peek_neg, (void *)hookPeekBufferNegative);
    sctrlHENPatchSyscall((void *)read_pos, (void *)hookReadBufferPositive);
    sctrlHENPatchSyscall((void *)read_neg, (void *)hookReadBufferNegative);
#endif
    flush_caches();
    g_installed = 1;
    return 0;
}

static void remove_hooks(void)
{
    if (!g_installed)
        return;

    /* Another PRX hooking the same syscalls concurrently is unsupported. */
    sctrlHENPatchSyscall((void *)realSetSamplingCycle,
                         (void *)realSetSamplingCycle);
    sctrlHENPatchSyscall((void *)realGetSamplingCycle,
                         (void *)realGetSamplingCycle);
#if CTRL_DEBOUNCE
    sctrlHENPatchSyscall((void *)realPeekBufferPositive,
                         (void *)realPeekBufferPositive);
    sctrlHENPatchSyscall((void *)realPeekBufferNegative,
                         (void *)realPeekBufferNegative);
    sctrlHENPatchSyscall((void *)realReadBufferPositive,
                         (void *)realReadBufferPositive);
    sctrlHENPatchSyscall((void *)realReadBufferNegative,
                         (void *)realReadBufferNegative);
#endif
    flush_caches();
    g_installed = 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    if (g_installed)
        return 0;

    /* Report a real error to the loader; module_stop is safe if invoked. */
    return install_hooks();
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    if (!g_installed)
        return 0;

    /*
     * Only a virtual cycle of 0 is physically substituted by this plugin.
     * For custom nonzero cycles no restore/set call is necessary.
     */
    if (g_forced_cycle && realSetSamplingCycle)
        realSetSamplingCycle(0);

    remove_hooks();
    g_forced_cycle = 0;
#if CTRL_DEBOUNCE
    g_180hz_cycle = 0;
#endif
    return 0;
}
