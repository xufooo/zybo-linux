# Makefile — Linux Kernel Drivers for ZYBO Audio DSP
# Based on Zedboard audio driver pattern (yuhei1horibe/Zedboard_audio_driver)
#
# zybo-snd-card.c — main sound card driver (Zedboard pattern)
# zybo-i2s-dai.c    — CPU DAI driver (simpler variant, kept as reference)
# zed_pl_snd_card.c — original Zedboard reference (not compiled)

obj-$(CONFIG_SND_SOC_ZYBO_SND_CARD) += zybo-snd-card.o

# Legacy / reference
# obj-m += zybo-audio-machine.o
# obj-m += zybo-i2s-dsp-dai.o

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
