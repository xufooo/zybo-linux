# ZYBO Audio DSP — Linux Buildroot

[![Buildroot Build](https://github.com/YOURNAME/zybo-audio-linux/actions/workflows/build.yml/badge.svg)](https://github.com/YOURNAME/zybo-audio-linux/actions/workflows/build.yml)

ZYBO (XC7Z010) Linux system for the FPGA audio DSP project. Builds a complete SD card image with:

- **linux-xlnx 5.15** kernel with Zynq-7000 support
- **SSM2602 audio codec driver** (SSM2603 compatible)
- **MPD** (Music Player Daemon) with NFS/WebDAV support
- **shairport-sync** (AirPlay 1 & 2)
- **ALSA audio stack** with simple-audio-card
- **Buildroot 2024.02** toolchain and rootfs

## Quick Start (GitHub Actions)

1. Fork this repo
2. Go to Actions → enable workflows
3. Push or manually trigger — builds in ~60 min
4. Download `sdcard.img` from Artifacts
5. Flash: `sudo dd if=sdcard.img of=/dev/sdX bs=4M status=progress`

## Local Build

```bash
# Install deps
sudo apt install -y build-essential flex bison libssl-dev libncurses-dev rsync cpio unzip wget bc git

# Clone Buildroot + configure
git clone --depth 1 --branch 2024.02 https://github.com/buildroot/buildroot.git
make -C buildroot BR2_EXTERNAL=./external zybo_z7_audio_defconfig

# Build (first time: ~60 min)
make -C buildroot -j$(nproc)

# Flash
sudo dd if=buildroot/output/images/sdcard.img of=/dev/sdX bs=4M status=progress
```

## SD Card Contents

| Partition | Type | Contents |
|-----------|------|----------|
| FAT32 | boot | BOOT.BIN, uImage, devicetree.dtb |
| ext4 | root | BusyBox + ALSA + MPD + shairport-sync |

## Device Tree

The `external/overlays/zybo-audio.dtsi` adds:
- AXI DMA at 0x40400000
- `d_axi_i2s_audio` I2S IP at 0x43C00000
- SSM2603 codec on PL I2C at 0x1A
- simple-audio-card binding

## Related Repos

- FPGA Bitstream: see main `zybo-dev` project
- Go backend: `zybo-dev/projects/audio-player/backend/`
- WebUI: `zybo-dev/projects/audio-player/webui/`
