# zybo-linux — ZYBO Rev B Linux 内核（GitHub Actions 构建）

仅含 **Linux 内核编译**所需内容，不包含 FPGA / 后端 / 根文件系统等。

## 内核版本（已调研，2026-09）

- 官方仓库：**Xilinx/linux-xlnx** <https://github.com/Xilinx/linux-xlnx>
  （“The official Linux kernel from Xilinx”，仍由 Xilinx org 维护）
- 当前维护的 LTS 线（git ls-remote 实测）：5.15 / 6.1 / **6.6** / 6.12 / 6.18，
  并有 `_202x.y_update` 与 Vivado/Vitis 发行配套的细化分支
- 本项目选 **`xlnx_rebase_v6.6_LTS_2024.1_merge_6.6.80`**（tag 锁定，不可变）：属 Vivado/Vitis **2024.1**
  官方线（`xlnx_rebase_v6.6_LTS_2024.1` = 6.6.10），并已并入上游 stable 修复（6.6.80）；
  Zynq-7000 在该线内受官方维护。CI 会把 `ref/commit/kernelversion` 写入产物 `kernel-version.txt`。
- 早期项目用的 5.15 是 2022/2023.1 时代，弃用

## 内容

```
zybo-linux/
├── .github/workflows/build-kernel.yml
├── kernel/
│   ├── config.fragment      # linux-xlnx 6.6 音频相关配置（ASoC: adi-axi-i2s/ssm2602/simple-card、
│   │                        #   Xilinx DMA、PL I2C、GPIO…，与 Buildroot 全镜像共用）
│   └── zybo-audio.dts       # 音频 PL 外设 overlay（DMA/i2s/iic/gpio/sound，include zynq-zybo.dts）
└── README.md
```

## Actions 构建

- 内核：Xilinx `linux-xlnx` 分支 `xlnx_rebase_v6.6_LTS`（与 Vivado 2024.1 配套）
- 基配置：`xilinx_zynq_defconfig` + `kernel/config.fragment` 叠加
- DTS：`kernel/zybo-audio.dts` → 编出 `zybo-audio.dtb`
- 产物（artifact `kernel-images`）：`zImage` / `uImage` / `zybo-audio.dtb` / `.config` / `System.map`

触发：手动运行；或 push 改动 `kernel/**` / workflow 时自动。

## 烧录/对接提示

- `uImage` + `zybo-audio.dtb` 放入 SD 卡 FAT 分区（配合 BOOT.BIN：由 FPGA 侧 `.xsa` + bootgen 生成）；
- 该 fragment/dts 与 `ZYBO/projects/audio_player/linux/`（本地 Buildroot 全镜像）保持同步；
  本仓库为内核侧“单一事实来源”，改动后同步到 audio_player。

## 参考

- linux-xlnx：<https://github.com/Xilinx/linux-xlnx>
- 板级/方案背景：见 ZYBO 工程（`docs/PLAN.md`、`docs/TROUBLESHOOTING.md`）
