TARGET = ctrlhz180
OBJS = main.o

BUILD_PRX = 1
USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1
PSP_FW_VERSION = 660

CTRL_DEBOUNCE ?= 1
CFLAGS = -O2 -G0 -Wall -Wextra -fno-pic -DCTRL_DEBOUNCE=$(CTRL_DEBOUNCE)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspsystemctrl_kernel

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
