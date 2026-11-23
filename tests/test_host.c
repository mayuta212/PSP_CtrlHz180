/* Host-side behavioral test for v1.5. It is NOT a hardware/PRX execution test. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pspkernel.h"
#include "pspsdk.h"
#include "pspctrl.h"
#include "systemctrl.h"

static int physical_cycle, patch_count, set_calls, k1 = 1;
static int missing_peek, fail_set, fake_stale_read, fail_two_peek;
static void *cur_set, *cur_get, *cur_pp, *cur_pn, *cur_rp, *cur_rn;
static SceCtrlData history[2];

static int fake_set(int value) {
    int old = physical_cycle;
    set_calls++;
    if (fail_set) return -9;
    if (value != 0 && (value < 5555 || value > 20000)) return -10;
    physical_cycle = value;
    return old;
}
static int fake_get(int *out) {if (!out) return -11; *out = physical_cycle; return 0;}
static int fake_pp(SceCtrlData *out, int count) {
    if (!out || count < 1 || count > 2) return -15;
    if (count == 2 && fail_two_peek) return -16;
    if (count == 1) out[0] = history[1];
    else {out[0] = history[0]; out[1] = history[1];}
    return count;
}
static int fake_pn(SceCtrlData *out, int count) {
    int i, r=fake_pp(out,count);
    for(i=0;i<r;i++)out[i].Buttons=~out[i].Buttons;
    return r;
}
static int fake_rp(SceCtrlData *out, int count) {
    int ret = fake_pp(out,count);
    if (ret == 1 && fake_stale_read) out[0] = history[0];
    return ret;
}
static int fake_rn(SceCtrlData *out, int count) {
    int ret = fake_rp(out,count);
    int i; for (i = 0; i < ret; ++i) out[i].Buttons = ~out[i].Buttons;
    return ret;
}

u32 sctrlHENFindFunction(const char *mod, const char *lib, u32 nid) {
    assert(strcmp(mod,"sceController_Service")==0 && strcmp(lib,"sceCtrl")==0);
    switch (nid) {
        case 0x6A2774F3: return (u32)(uintptr_t)fake_set;
        case 0x02BAAD91: return (u32)(uintptr_t)fake_get;
        case 0x3A622550: return missing_peek ? 0 : (u32)(uintptr_t)fake_pp;
        case 0xC152080A: return (u32)(uintptr_t)fake_pn;
        case 0x1F803938: return (u32)(uintptr_t)fake_rp;
        case 0x60B81F86: return (u32)(uintptr_t)fake_rn;
    }
    return 0;
}
void sctrlHENPatchSyscall(void *orig, void *replacement) {
    patch_count++;
    if(orig==(void *)fake_set)cur_set=replacement;
    else if(orig==(void *)fake_get)cur_get=replacement;
    else if(orig==(void *)fake_pp)cur_pp=replacement;
    else if(orig==(void *)fake_pn)cur_pn=replacement;
    else if(orig==(void *)fake_rp)cur_rp=replacement;
    else if(orig==(void *)fake_rn)cur_rn=replacement;
    else abort();
}
void sceKernelDcacheWritebackInvalidateAll(void) {}
void sceKernelIcacheClearAll(void) {}
int pspSdkSetK1(int val) {int old=k1; k1=val; return old;}

#include "../source/main.c"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAILED: %s line %d\n",#x,__LINE__);exit(1);}}while(0)
static void reset(void) {
    physical_cycle=0; patch_count=0; set_calls=0; k1=1; missing_peek=0;fail_set=0;fake_stale_read=0;fail_two_peek=0;
    cur_set=(void *)fake_set;cur_get=(void *)fake_get;
    cur_pp=(void *)fake_pp;cur_pn=(void *)fake_pn;
    cur_rp=(void *)fake_rp;cur_rn=(void *)fake_rn;
    realSetSamplingCycle=NULL;realGetSamplingCycle=NULL;
#if CTRL_DEBOUNCE
    realPeekBufferPositive=NULL;realPeekBufferNegative=NULL;
    realReadBufferPositive=NULL;realReadBufferNegative=NULL;
#endif
    g_virtual_cycle=0;g_forced_cycle=0;g_installed=0;
#if CTRL_DEBOUNCE
    g_180hz_cycle=0;
#endif
}
static void push_sample(uint32_t t0,uint32_t b0,uint32_t t1,uint32_t b1) {
    memset(history,0,sizeof history);
    history[0].TimeStamp=t0;history[0].Buttons=b0;
    history[1].TimeStamp=t1;history[1].Buttons=b1;
    history[0].Lx=0;history[0].Ly=255;
    history[1].Lx=255;history[1].Ly=0;
    for (int i=0; i<6; ++i) {
        history[0].Reserved[i]=(uint8_t)(0x10+i);
        history[1].Reserved[i]=(uint8_t)(0x80+i);
    }
}
/* Byte-exact preservation of analog axes, timestamp and reserved fields.
 * Only Buttons may change. This is checked on every successful API call. */
static void check_nonbutton_fields(const SceCtrlData *p, int n, int stale) {
    for (int i=0; i<n; ++i) {
        SceCtrlData expect = history[n == 1 ? (stale ? 0 : 1) : i];
        expect.Buttons = p[i].Buttons;
        CHECK(memcmp(&expect, &p[i], sizeof(expect)) == 0);
    }
}
static int call_pp(SceCtrlData *p,int n){
    int ret=((int(*)(SceCtrlData*,int))cur_pp)(p,n);
    if (ret>0) check_nonbutton_fields(p,ret,0);
    return ret;
}
static int call_pn(SceCtrlData *p,int n){
    int ret=((int(*)(SceCtrlData*,int))cur_pn)(p,n);
    if (ret>0) check_nonbutton_fields(p,ret,0);
    return ret;
}
static int call_rp(SceCtrlData *p,int n){
    int ret=((int(*)(SceCtrlData*,int))cur_rp)(p,n);
    if (ret>0) check_nonbutton_fields(p,ret,fake_stale_read);
    return ret;
}
static int call_rn(SceCtrlData *p,int n){
    int ret=((int(*)(SceCtrlData*,int))cur_rn)(p,n);
    if (ret>0) check_nonbutton_fields(p,ret,fake_stale_read);
    return ret;
}
int main(void) {
    SceCtrlData p[2];int v,n;
    reset(); CHECK(module_start(0,NULL)==0); CHECK(physical_cycle==5555);
    CHECK(patch_count==(CTRL_DEBOUNCE ? 6 : 2));
    CHECK(((int(*)(int*))cur_get)(&v)==0&&v==0);
    CHECK(((int(*)(int))cur_set)(10000)==0);
    CHECK(physical_cycle==10000);
    CHECK(((int(*)(int))cur_set)(0)==10000);
    CHECK(physical_cycle==5555);

    /* A quick press is immediately accepted; a 1-sample release is hidden. */
    push_sample(100,0,5655,PSP_CTRL_CROSS);
    CHECK(call_pp(p,1)==1 && p[0].Buttons==PSP_CTRL_CROSS);
    CHECK(p[0].TimeStamp==5655 && p[0].Lx==255 && k1==1);
    push_sample(5655,PSP_CTRL_CROSS,11210,0);
    CHECK(call_pp(p,1)==1);
    CHECK(p[0].Buttons==(CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0));
    CHECK(call_rp(p,1)==1);
    CHECK(p[0].Buttons==(CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0));
    CHECK(p[0].TimeStamp==11210);
    push_sample(11210,0,16765,0);
    CHECK(call_pp(p,1)==1 && p[0].Buttons==0);

    /* Independent buttons: release CROSS; new CIRCLE press registers instantly. */
    push_sample(16765,PSP_CTRL_CROSS,22320,PSP_CTRL_CIRCLE);
    CHECK(call_pp(p,1)==1);
    CHECK(p[0].Buttons==(CTRL_DEBOUNCE ? (PSP_CTRL_CROSS|PSP_CTRL_CIRCLE) : PSP_CTRL_CIRCLE));
    CHECK(call_rp(p,1)==1);
    CHECK(p[0].Buttons==(CTRL_DEBOUNCE ? (PSP_CTRL_CROSS|PSP_CTRL_CIRCLE) : PSP_CTRL_CIRCLE));

    /* Active-low APIs must also keep their requested polarity. */
    push_sample(22320,PSP_CTRL_CIRCLE,27875,0);
    CHECK(call_pn(p,1)==1);
    CHECK(p[0].Buttons==(CTRL_DEBOUNCE ? ~((uint32_t)PSP_CTRL_CIRCLE) : UINT32_MAX));
    CHECK(call_rn(p,1)==1);
    CHECK(p[0].Buttons==(CTRL_DEBOUNCE ? ~((uint32_t)PSP_CTRL_CIRCLE) : UINT32_MAX));

    /* If a game requests a nonzero 10ms sampling cycle, experimental
     * debounce must NOT hold its releases for an extra 10ms. */
    CHECK(((int(*)(int))cur_set)(10000) == 0);
    push_sample(5000, PSP_CTRL_CROSS, 15000, 0);
    CHECK(call_pp(p,1) == 1 && p[0].Buttons == 0);
    CHECK(((int(*)(int))cur_set)(0) == 10000);

    /* A game explicitly selecting 5555us still receives 2-sample filtering. */
    CHECK(((int(*)(int))cur_set)(5555) == 0);
    CHECK(physical_cycle == 5555);
    push_sample(24000, PSP_CTRL_CROSS, 29555, 0);
    CHECK(call_pp(p,1)==1);
    CHECK(p[0].Buttons == (CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0));
    CHECK(((int(*)(int))cur_set)(0) == 5555);

    /* New presses must never be delayed by the release filter. */
    push_sample(29555, 0, 35110, PSP_CTRL_CROSS);
    CHECK(call_pp(p,1)==1 && p[0].Buttons == PSP_CTRL_CROSS);
    CHECK(call_rp(p,1)==1 && p[0].Buttons == PSP_CTRL_CROSS);

    /* Multi-read and multi-peek are never rewritten. */
    push_sample(27875,PSP_CTRL_CROSS,33430,0);
    CHECK(call_pp(p,2)==2 && p[0].Buttons==PSP_CTRL_CROSS && p[1].Buttons==0);
    CHECK(call_rp(p,2)==2 && p[0].Buttons==PSP_CTRL_CROSS && p[1].Buttons==0);
    CHECK(call_pn(p,2)==2 && p[0].Buttons==~((uint32_t)PSP_CTRL_CROSS) && p[1].Buttons==UINT32_MAX);

    /* Regression: sampled ON -> OFF -> ON within 3 ticks. A single OFF
     * vanishes in 2-sample mode, making the second tap unobservable.
     * This is a real limitation, NOT a test failure. */
    {
        static const uint32_t raw[] = {PSP_CTRL_CROSS, 0, PSP_CTRL_CROSS};
        uint32_t observed[3];
        unsigned i;
        for (i = 0; i < 3; ++i) {
            const uint32_t prev = (i == 0) ? 0 : raw[i - 1];
            const uint32_t t = (i + 1) * 5555u;
            push_sample(t - 5555u, prev, t, raw[i]);
            CHECK(call_pp(p,1) == 1);
            observed[i] = p[0].Buttons & PSP_CTRL_CROSS;
        }
        CHECK(observed[0] == PSP_CTRL_CROSS);
        CHECK(observed[1] == (CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0));
        CHECK(observed[2] == PSP_CTRL_CROSS);
        printf("MODE=%d ON-OFF-ON: %d-%d-%d\n", CTRL_DEBOUNCE,
               observed[0] != 0, observed[1] != 0, observed[2] != 0);
    }
    /* Regression: OFF is physically present for two 180Hz samples, but
     * 60Hz game polling at ticks 2 and 5 can miss it *only* after filtering.
     * ON,ON,OFF,OFF,ON,ON -> filtered ON,ON,ON,OFF,ON,ON. */
    {
        static const uint32_t raw[] = {PSP_CTRL_CROSS, PSP_CTRL_CROSS, 0, 0, PSP_CTRL_CROSS, PSP_CTRL_CROSS};
        uint32_t observed[6];
        unsigned i;
        for (i = 0; i < 6; ++i) {
            const uint32_t prev = (i == 0) ? 0 : raw[i - 1];
            const uint32_t t = (i + 1) * 5555u;
            push_sample(t - 5555u, prev, t, raw[i]);
            CHECK(call_pp(p,1) == 1);
            observed[i] = p[0].Buttons & PSP_CTRL_CROSS;
        }
        CHECK(observed[2] == (CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0));
        CHECK(observed[3] == 0);
        CHECK(observed[4] == PSP_CTRL_CROSS);
    }
    /* A ReadBuffer caller can obtain an older sample. v1.2's additional
     * newest-Peek timestamp check then fails open (no filtering). */
    {
        push_sample(100, PSP_CTRL_CROSS, 5655, 0);
        fake_stale_read = 1;
        CHECK(call_rp(p,1) == 1);
        CHECK(p[0].TimeStamp == 100 && p[0].Buttons == PSP_CTRL_CROSS);
        fake_stale_read = 0;
        CHECK(call_rp(p,1) == 1);
        CHECK(p[0].TimeStamp == 5655);
        CHECK(p[0].Buttons == (CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0));
    }

    /* TimeStamp wraps; unsigned subtraction remains valid. */
    push_sample(UINT32_MAX - 2000u, PSP_CTRL_CROSS, 3554u, 0);
    CHECK(call_pp(p,1)==1);
    CHECK(p[0].Buttons == (CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0));
    CHECK(p[0].TimeStamp == 3554u);

    /* The filter must not change unrelated system-button bits. */
    push_sample(100, PSP_CTRL_CROSS | 0x10000000u,
                5655, 0x20000000u);
    CHECK(call_pp(p,1)==1);
    CHECK(p[0].Buttons == (0x20000000u | (CTRL_DEBOUNCE ? PSP_CTRL_CROSS : 0)));

    /* If reading two extra samples fails, return the unmodified game sample. */
    push_sample(100, PSP_CTRL_CROSS, 5655, 0);
    fail_two_peek = 1;
    CHECK(call_pp(p,1) == 1 && p[0].Buttons == 0 && p[0].TimeStamp == 5655);
    CHECK(call_rp(p,1) == 1 && p[0].Buttons == 0 && p[0].TimeStamp == 5655);
    fail_two_peek = 0;

    /* Same timestamp => no evidence of 2 distinct samples; unchanged. */
    push_sample(10000,PSP_CTRL_CROSS,10000,0);
    CHECK(call_pp(p,1)==1 && p[0].Buttons==0);
    CHECK(call_pp(p,0)<0);
    CHECK(call_pp(NULL,1)<0);

    /* Game-facing sampling APIs remain stable. */
    n=set_calls; CHECK(module_stop(0,NULL)==0 && physical_cycle==0);
    CHECK(set_calls==n+1 && patch_count==(CTRL_DEBOUNCE ? 12 : 4));
    CHECK(module_stop(0,NULL)==0);
    reset();missing_peek=1;
#if CTRL_DEBOUNCE
    CHECK(module_start(0,NULL)<0 && patch_count==0 && physical_cycle==0);
#else
    CHECK(module_start(0,NULL)==0 && physical_cycle==5555); CHECK(module_stop(0,NULL)==0);
#endif
    reset(); CHECK(module_start(0,NULL)==0);
    /* Deterministic fuzz: filter is OR of two adjacent samples, masked. */
    {
        uint32_t seed=0x180DBu;
        unsigned i;
        for(i=0;i<25000;i++){
            uint32_t a,b,expect;
            seed=seed*1664525u+1013904223u;a=seed;
            seed=seed*1664525u+1013904223u;b=seed;
            push_sample(100,a,5655,b);
            CHECK(call_pp(p,1)==1);
            #if CTRL_DEBOUNCE
            expect=((b&~DB_MASK)|((a|b)&DB_MASK));
#else
            expect=b;
#endif
            CHECK(p[0].Buttons==expect);
            CHECK(call_pn(p,1)==1 && p[0].Buttons==~expect);
            CHECK(p[0].TimeStamp==5655 && p[0].Lx==255 && k1==1);
        }
        push_sample(100,PSP_CTRL_CROSS,40000,0);
        CHECK(call_pp(p,1)==1 && p[0].Buttons==0); /* stale sample */
    }
    reset();fail_set=1;
    CHECK(module_start(0,NULL)<0 && patch_count==0 && physical_cycle==0);
    puts("PASS: lifecycle, sampling, polarities, two-sample releases, analog byte passthrough, fallback");
    return 0;
}
