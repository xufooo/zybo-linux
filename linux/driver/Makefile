# ──────────────────────────────────────────────────────────────────────────
# Makefile — Linux Kernel Drivers for ZYBO Audio DSP
# ──────────────────────────────────────────────────────────────────────────
# Build with: make -C /path/to/kernel M=$(pwd) modules
# (Not needed with Buildroot + simple-audio-card; kept for reference)
# ──────────────────────────────────────────────────────────────────────────

obj-m += zybo-audio-machine.o
obj-m += zybo-i2s-dsp-dai.o

# If building built-in instead of modules:
# obj-y += zybo-audio-machine.o
# obj-y += zybo-i2s-dsp-dai.o

# ── Kernel build system integration ──────────────────────────────────────
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install
	depmod -a

.PHONY: all clean install
