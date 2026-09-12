# Third-party components (THIRD-PARTY)

This repository holds build inputs only: a kernel configuration fragment and a
device tree overlay. No kernel binaries are stored here. The components below
are downloaded and built by `.github/workflows/build-kernel.yml` (or by hand,
following `README.md`).

## Components

| Component | Version / ref | License | Source |
|---|---|---|---|
| Linux kernel (`Xilinx/linux-xlnx`) | tag `xlnx_rebase_v6.6_LTS_2024.1_merge_6.6.80` (6.6.80), commit `e29e392a451244a11aa3559738b6617536fde460` | GPL-2.0-only, with the Linux syscall note | https://github.com/Xilinx/linux-xlnx |
| Upstream ZYBO device tree (`arch/arm/boot/dts/xilinx/zynq-zybo.dts`) | same tag / commit | GPL-2.0-or-later (per-file SPDX) | same |
| `kernel/config.fragment`, `kernel/zybo-audio.dts` | this repository | GPL-2.0 (see `LICENSE`) | this repository |

The kernel is pinned to the commit SHA above, not to the tag: a tag can be
re-pointed or deleted upstream, a commit SHA cannot. The mapping was resolved
with

```
git ls-remote https://github.com/Xilinx/linux-xlnx.git \
    'refs/tags/xlnx_rebase_v6.6_LTS_2024.1_merge_6.6.80^{}'
```

The tag is annotated, so `^{}` yields the commit; the tag object itself is
`60ffc99400e598d1dadb44e0a7c36432f0f1f57b`. `KERNEL_TAG`/`KERNEL_REF` in the
workflow keep both spellings of the same revision.

## Corresponding source (GPL-2.0)

`zImage`, `uImage` and `zybo-audio.dtb` built from this repository combine the
linux-xlnx kernel (**GPL-2.0-only**) with the configuration fragment and the
device tree in this repository. The complete corresponding source therefore is:

1. the upstream kernel at the pinned commit
   `e29e392a451244a11aa3559738b6617536fde460` (tag
   `xlnx_rebase_v6.6_LTS_2024.1_merge_6.6.80`,
   <https://github.com/Xilinx/linux-xlnx>), plus
2. every file in this repository (`kernel/config.fragment`,
   `kernel/zybo-audio.dts`).

The build is fully reproducible from those two inputs with the commands in
`README.md`; the pinned tag, the pinned commit and the commit actually checked
out are also recorded in the CI artifact `kernel-version.txt`. Written offers for
the corresponding source, as allowed by GPL-2.0 section 3(b), can be requested
through the repository issue tracker.

The kernel is GPL-2.0-**only** (it carries the Linux syscall note), so GPLv3
cannot be used to satisfy its source obligation.

## Build-time tools (not distributed)

`gcc-arm-linux-gnueabihf`, GNU make, `bc`, `bison`, `flex`, `libssl-dev`,
`device-tree-compiler`, `u-boot-tools` (`mkimage`) and git. These run only on the
build machine and are not redistributed by this repository.
