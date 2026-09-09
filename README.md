# zybo-linux

Linux kernel build for a ZYBO (Zynq-7000) audio player.

Cross-compiles a Xilinx kernel with the audio device tree overlay and produces
`zImage`, `uImage` and `zybo-audio.dtb` on GitHub Actions.

## What it builds

| Item | Value |
|---|---|
| Kernel source | [Xilinx/linux-xlnx](https://github.com/Xilinx/linux-xlnx) |
| Ref (pinned tag) | `xlnx_rebase_v6.6_LTS_2024.1_merge_6.6.80` (6.6 LTS, paired with Vivado/Vitis 2024.1) |
| Base config | `xilinx_zynq_defconfig` + `kernel/config.fragment` |
| Device tree | `kernel/zybo-audio.dts` (applied on top of `zynq-zybo.dts`) |
| Toolchain | `arm-linux-gnueabihf-` |

`kernel/config.fragment` enables the audio stack:

- `SND_SOC_ADI` / `SND_SOC_ADI_AXI_I2S` — ADI AXI-I2S CPU DAI (PL330 mode)
- `SND_SOC_SSM2602(_I2C)` — SSM2603 codec (register compatible)
- `SND_SIMPLE_CARD` / `SND_SOC_GENERIC_DMAENGINE_PCM` — DT-only sound card
- `PL330_DMA` — Zynq PS DMA used by the I2S core
- Xilinx I2C / GPIO / SPI drivers

`kernel/zybo-audio.dts` adds the PL devices to the board device tree:

| Node | Address | Notes |
|---|---|---|
| `axi_i2s_0` | `0x43C00000` | `adi,axi-i2s-1.00.a`, `dmas = <&dmac_s 0>, <&dmac_s 1>` |
| `axi_iic_0` | `0x41600000` | PL I2C → SSM2603 at `0x1A` |
| `axi_gpio_btn` | `0x41200000` | 4-bit button input |
| `sound` | — | `simple-audio-card` ("Zybo-Sound-Card") |

## Hardware notes

- Board: Digilent ZYBO Rev B (XC7Z010-CLG400), SSM2603 codec wired to PL pins
- MCLK is fixed at **12.288 MHz** (256 × 48 kHz) → playback is native for the
  48 kHz family; 44.1 kHz content needs resampling
- `AC_MUTEN` must be driven **high** (the codec is muted otherwise)
- Data path: PS PL330 DMA (tx = channel 0, rx = channel 1) ↔ ADI AXI-I2S ↔ SSM2603

## CI

`.github/workflows/build-kernel.yml` runs on `workflow_dispatch` and on pushes
touching `kernel/**`. Artifacts (`kernel-images`):

```
zImage  uImage  zybo-audio.dtb  .config  System.map  kernel-version.txt
```

`kernel-version.txt` records the pinned ref, resolved commit and kernel version.

## Layout

```
zybo-linux/
├── .github/workflows/build-kernel.yml
├── kernel/
│   ├── config.fragment     # kernel config fragment (audio stack)
│   └── zybo-audio.dts      # audio device tree overlay
└── README.md
```

## Related

- Root filesystem: [zybo-buildroot](https://github.com/xufooo/zybo-buildroot)
