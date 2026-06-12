#!/bin/sh
# ============================================================================
# post-build.sh — ZYBO Z7 Root FS Post-Build Hook
# ============================================================================
# Called by Buildroot after rootfs is assembled but before image generation.
# Arguments: $1 = target/   $2 = TARGET_DIR
#
# Do:
#   1. Generate BOOT.BIN from U-Boot SPL + FSBL (Vivado .hdf)
#   2. Copy kernel image + device tree to boot partition
#   3. Create /etc/asound.conf for default audio device
#   4. Set up startup scripts
# ============================================================================

set -e
set -x

TARGET_DIR="${1}/target"

echo "=== ZYBO Audio DSP: post-build ==="

# ── Create /etc/asound.conf (ALSA default device) ────────────────────────
mkdir -p "${TARGET_DIR}/etc"
cat > "${TARGET_DIR}/etc/asound.conf" << 'EOF'
# ALSA default PCM device — ZYBO Audio DSP
pcm.!default {
    type plug
    slave {
        pcm "hw:0,0"
        rate 48000
        format S24_LE
        channels 2
    }
}

ctl.!default {
    type hw
    card 0
}
EOF

# ── Create startup banner ────────────────────────────────────────────────
cat > "${TARGET_DIR}/etc/motd" << 'EOF'
╔══════════════════════════════════════════════════╗
║         ZYBO Z7 Audio DSP — Buildroot Linux      ║
║         Kernel: linux-xlnx 5.15                   ║
║         Audio: SSM2603 + FPGA DSP                 ║
╚══════════════════════════════════════════════════╝

  aplay -l          list audio devices
  speaker-test      test audio output
  amixer scontrols  list mixer controls

EOF

# ── Enable getty on ttyPS0 at startup ────────────────────────────────────
ln -sf /etc/init.d/S50getty "${TARGET_DIR}/etc/init.d/S50getty_ttyPS0" 2>/dev/null || true

# ── Create /mnt/sd for SD card mount ────────────────────────────────────
mkdir -p "${TARGET_DIR}/mnt/sd"

echo "=== Post-build complete ==="
