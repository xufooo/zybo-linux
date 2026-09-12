# zybo-linux

Linux kernel build for a ZYBO (Zynq-7000) audio player.

Cross-compiles the Xilinx kernel with the audio device tree for the ZYBO Rev B
and produces `zImage`, `uImage` and `zybo-audio.dtb` — on GitHub Actions or on a
local Linux host.

## What it builds

| Item | Value |
|---|---|
| Kernel source | [Xilinx/linux-xlnx](https://github.com/Xilinx/linux-xlnx) |
| Ref | `xlnx_rebase_v6.6_LTS_2024.1_merge_6.6.80` (6.6 LTS, Vivado/Vitis 2024.1 line) |
| Base config | `xilinx_zynq_defconfig` + `kernel/config.fragment` |
| Device tree | `kernel/zybo-audio.dts`, layered on top of `xilinx/zynq-zybo.dts` |
| Cross toolchain | `arm-linux-gnueabihf-` |
| Load address | `0x8000` (for `uImage`) |

## Layout

```
.github/workflows/build-kernel.yml   # CI: cross-compile + artifact upload
kernel/config.fragment               # kernel config fragment
kernel/zybo-audio.dts                # audio/PL device tree overlay
```

## Requirements

- Linux host with `make` and the usual kernel build dependencies: `bc`, `bison`,
  `build-essential`, `cpio`, `device-tree-compiler`, `file`, `flex`,
  `libncurses-dev`, `libssl-dev`, `u-boot-tools` (`mkimage`), `git`
- ARM cross toolchain `gcc-arm-linux-gnueabihf` (`arm-linux-gnueabihf-`) with
  its 32-bit host libraries
- Network access to clone `linux-xlnx`

## Build

```bash
git clone --depth 1 --branch xlnx_rebase_v6.6_LTS_2024.1_merge_6.6.80 \
    https://github.com/Xilinx/linux-xlnx.git linux

cp kernel/zybo-audio.dts linux/arch/arm/boot/dts/zybo-audio.dts
cp kernel/config.fragment .config.fragment

cd linux
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- xilinx_zynq_defconfig
./scripts/kconfig/merge_config.sh -m .config ../.config.fragment
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j"$(nproc)" zImage
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j"$(nproc)" zybo-audio.dtb
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- LOADADDR=0x8000 -j"$(nproc)" uImage
```

Results: `arch/arm/boot/zImage`, `arch/arm/boot/uImage` and
`arch/arm/boot/dts/zybo-audio.dtb`.

## Kernel configuration

`kernel/config.fragment` extends `xilinx_zynq_defconfig` with:

- **Audio**: `SND_SOC_ADI` and `SND_SOC_ADI_AXI_I2S` (ADI AXI-I2S CPU DAI in
  PL330 mode), `SND_SOC_SSM2602(_I2C)` (SSM2603 codec), `SND_SIMPLE_CARD`,
  `SND_SOC_GENERIC_DMAENGINE_PCM` and `PL330_DMA`
- **PL peripherals**: Xilinx I2C, GPIO and SPI drivers, plus `UIO`
- **USB**: host (EHCI/ULPI) and gadget Ethernet
- **USB WiFi**: the `rtl8xxxu`, `rtw88` (USB), `mt7601u`, `mt76` (USB) and
  `rt2800usb` families, with `CFG80211`, `MAC80211` and `RFKILL`
- **systemd support**: cgroups, namespaces, `FANOTIFY`, POSIX ACLs, `SECCOMP`
- **Misc**: `IKCONFIG` with `/proc/config.gz`, ext4 and VFAT

## Device tree

`kernel/zybo-audio.dts` adds the programmable-logic devices on top of the
upstream `zynq-zybo.dts`:

| Node | Address | Description |
|---|---|---|
| `axi_i2s_0` | `0x43C00000` | `adi,axi-i2s-1.00.a`, DMA channels `&dmac_s 0` (tx) and `1` (rx) |
| `axi_iic_0` | `0x41600000` | PL I2C controller; SSM2603 codec at `0x1A` |
| `axi_gpio_btn` | `0x41200000` | Button input GPIO |
| `sound` | — | `simple-audio-card`, name `Zybo-Sound-Card` |
| `audio_ref_clk` | — | Fixed 12.288 MHz reference clock |

The reference clock is fixed at 12.288 MHz (256 × 48 kHz), so 48 kHz playback is
native and 44.1 kHz sources have to be resampled in user space.

## CI

`.github/workflows/build-kernel.yml` runs on `workflow_dispatch` and on pushes
that touch `kernel/**`. The `kernel-images` artifact contains:

```
zImage  uImage  zybo-audio.dtb  System.map  kernel-config.txt  kernel-version.txt
```

`kernel-version.txt` records the pinned ref, the resolved commit and the kernel
version. `kernel-config.txt` is the resolved configuration — hidden files such as
`.config` are not uploaded as artifacts.

## Related repositories

- Buildroot rootfs: [zybo-buildroot](https://github.com/xufooo/zybo-buildroot)
- Debian rootfs: [zybo-debian](https://github.com/xufooo/zybo-debian)
