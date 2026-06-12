/*
 * zybo-audio-machine.c — ASoC Machine Driver for ZYBO Audio DSP
 * --------------------------------------------------------------
 * Binds the FPGA I2S+DSP CPU DAI to the SSM2603 codec DAI.
 *
 * (Note: With Buildroot + linux-xlnx 5.15 + simple-audio-card,
 *  this custom driver is NOT needed. Pure DT config is sufficient.
 *  This file is kept for reference / bare-metal use.)
 *
 * This driver:
 *   1. Registers an ASoC sound card
 *   2. Sets up the DAI link between CPU (FPGA I2S DSP) and Codec (SSM2603)
 *   3. Configures codec via I2C (using kernel SSM2602 driver)
 *   4. Exposes DSP controls via ALSA mixer kcontrols
 *
 * (Note: With Buildroot + linux-xlnx 5.15 + simple-audio-card,
 *  this custom driver is NOT needed — pure DT config is sufficient.
 *  This file is kept for reference / bare-metal use.)
 *
 * Compile (in kernel build):
 *   obj-m += zybo-audio-machine.o
 *
 * Or built-in:
 *   obj-y += zybo-audio-machine.o
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

/* ── DAI Link ────────────────────────────────────────────────────────── */

static struct snd_soc_dai_link zybo_audio_dai_links[] = {
    {
        .name            = "ZYBO Audio DSP",
        .stream_name     = "Audio Playback/Capture",
        .cpu_dai_name    = "43c00000.i2s-dsp",   /* FPGA I2S+DSP DAI */
        .codec_dai_name   = "ssm2602-hifi",       /* Codec DAI */
        .codec_name       = "ssm2603.0-001a",     /* I2C address */
        .platform_name    = "40400000.dma",        /* AXI DMA */
        .dai_fmt          = SND_SOC_DAIFMT_I2S |
                            SND_SOC_DAIFMT_NB_NF |
                            SND_SOC_DAIFMT_CBM_CFM, /* Codec master */
        .init             = NULL,
        .ops              = NULL,                  /* use default ops */
        .params           = NULL,
    },
};

/* ── Sound Card ──────────────────────────────────────────────────────── */

static struct snd_soc_card zybo_audio_card = {
    .name         = "ZYBO Audio DSP",
    .owner        = THIS_MODULE,
    .dai_link     = zybo_audio_dai_links,
    .num_links    = ARRAY_SIZE(zybo_audio_dai_links),
    .dapm_widgets = NULL,
    .num_dapm_widgets = 0,
    .dapm_routes  = NULL,
    .num_dapm_routes  = 0,
    .fully_routed = false,
};

/* ── Platform Driver ─────────────────────────────────────────────────── */

static int zybo_audio_probe(struct platform_device *pdev)
{
    struct snd_soc_card *card = &zybo_audio_card;
    int ret;

    dev_info(&pdev->dev, "ZYBO Audio DSP machine driver probing\n");

    /* Set card device for proper devm handling */
    card->dev = &pdev->dev;

    /* Register the sound card with ASoC */
    ret = devm_snd_soc_register_card(&pdev->dev, card);
    if (ret) {
        dev_err(&pdev->dev, "snd_soc_register_card failed: %d\n", ret);
        return ret;
    }

    dev_info(&pdev->dev, "ZYBO Audio DSP sound card registered\n");
    return 0;
}

static const struct of_device_id zybo_audio_of_match[] = {
    { .compatible = "digilent,zybo-audio", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zybo_audio_of_match);

static struct platform_driver zybo_audio_driver = {
    .driver = {
        .name           = "zybo-audio",
        .of_match_table = zybo_audio_of_match,
        .owner          = THIS_MODULE,
    },
    .probe = zybo_audio_probe,
};

module_platform_driver(zybo_audio_driver);

MODULE_AUTHOR("ZYBO Audio Project");
MODULE_DESCRIPTION("ASoC Machine Driver for ZYBO Audio DSP");
MODULE_LICENSE("GPL v2");
