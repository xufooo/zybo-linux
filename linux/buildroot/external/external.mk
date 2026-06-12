# Buildroot external tree for ZYBO Z7 Audio DSP
include $(sort $(wildcard $(BR2_EXTERNAL_ZYBO_AUDIO_PATH)/package/*/*.mk))
