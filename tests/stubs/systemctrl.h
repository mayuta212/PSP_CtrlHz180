#ifndef TEST_SYSTEMCTRL_H
#define TEST_SYSTEMCTRL_H
#include "pspkernel.h"
u32 sctrlHENFindFunction(const char *module, const char *lib, u32 nid);
void sctrlHENPatchSyscall(void *addr, void *newaddr);
#endif
