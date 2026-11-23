#ifndef TEST_PSPKERNEL_H
#define TEST_PSPKERNEL_H
#include <stdint.h>
/* Host test uses uintptr_t to keep function addresses untruncated on x86_64. */
typedef uintptr_t u32;
typedef unsigned int SceSize;
#define PSP_MODULE_INFO(name, flags, major, minor)
#define PSP_MAIN_THREAD_ATTR(attr)
void sceKernelDcacheWritebackInvalidateAll(void);
void sceKernelIcacheClearAll(void);
#endif
