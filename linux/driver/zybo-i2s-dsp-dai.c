/*
 * zybo-i2s-dsp-dai.c — ASoC CPU DAI Driver for ZYBO FPGA I2S+DSP
 * ----------------------------------------------------------------
 * This driver interfaces with the custom FPGA audio processing IP
 * (i2s_top + dsp_eq_top + axi_dsp_regmap) through AXI DMA.
 *
 * The IP appears as a standard ASoC CPU DAI that:
 *   - Uses AXI DMA for memory ↔ stream data movement
 *   - Exposes DSP controls through ALSA kcontrols
 *   - Supports configurable sample rates and formats
 *
 * Hardware parameters:
 *   - Format: S24_LE (24-bit in 32-bit slot)
 *   - Rate: 8000, 16000, 22050, 44100, 48000, 96000 Hz
 *   - Channels: 2 (stereo)
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

/* ── Hardware limits ─────────────────────────────────────────────────── */
#define ZYBO_I2S_DSP_RATES \
    (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
     SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
     SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000)

#define ZYBO_I2S_DSP_FORMATS   (SNDRV_PCM_FMTBIT_S24_LE)
#define ZYBO_I2S_DSP_CHANNELS  2
#define ZYBO_I2S_DSP_PERIOD_BYTES_MIN  1024
#define ZYBO_I2S_DSP_PERIOD_BYTES_MAX  (64 * 1024)
#define ZYBO_I2S_DSP_PERIODS_MIN  2
#define ZYBO_I2S_DSP_PERIODS_MAX  32
#define ZYBO_I2S_DSP_BUFFER_BYTES_MAX  (256 * 1024)

/* ── Private data ────────────────────────────────────────────────────── */

struct zybo_i2s_dsp {
    struct device *dev;
    struct clk *audio_clk;
    struct clk *axi_clk;

    /* DMA channels */
    struct dma_chan *tx_chan;  /* playback: MM2S */
    struct dma_chan *rx_chan;  /* capture:  S2MM */

    /* Register base (for DSP control) */
    void __iomem *regs;
    resource_size_t regs_phys;
    resource_size_t regs_size;

    /* Current params */
    unsigned int rate;
    unsigned int channels;
    unsigned int format;
};

/* ── DMA Callback ────────────────────────────────────────────────────── */

static void zybo_i2s_dsp_dma_complete(void *data)
{
    struct snd_pcm_substream *substream = data;

    snd_pcm_period_elapsed(substream);
}

/* ── PCM Operations: Hardware Parameters ─────────────────────────────── */

static int zybo_i2s_dsp_hw_params(struct snd_pcm_substream *substream,
                                   struct snd_pcm_hw_params *params,
                                   struct snd_soc_dai *dai)
{
    struct zybo_i2s_dsp *dsp = snd_soc_dai_get_drvdata(dai);
    struct dma_chan *chan;
    struct dma_slave_config slave_config;
    int ret;

    /* Select DMA channel based on stream direction */
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
        chan = dsp->tx_chan;
    else
        chan = dsp->rx_chan;

    if (!chan) {
        dev_err(dsp->dev, "DMA channel not available\n");
        return -ENODEV;
    }

    /* Configure DMA slave */
    memset(&slave_config, 0, sizeof(slave_config));

    /* Each sample: 24-bit left + 24-bit right = 48 bits = 6 bytes */
    slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    slave_config.dst_maxburst   = 16;
    slave_config.src_maxburst   = 16;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
        slave_config.dst_addr = dsp->regs_phys;  /* MM2S → I2S DSP */
    } else {
        slave_config.src_addr = dsp->regs_phys;  /* I2S DSP → S2MM */
    }

    ret = dmaengine_slave_config(chan, &slave_config);
    if (ret) {
        dev_err(dsp->dev, "DMA slave config failed: %d\n", ret);
        return ret;
    }

    /* Store current params */
    dsp->rate     = params_rate(params);
    dsp->channels = params_channels(params);
    dsp->format   = params_format(params);

    dev_dbg(dsp->dev, "HW params: rate=%u, channels=%u, format=%u\n",
            dsp->rate, dsp->channels, dsp->format);

    return 0;
}

static int zybo_i2s_dsp_hw_free(struct snd_pcm_substream *substream,
                                 struct snd_soc_dai *dai)
{
    /* Clean up any HW-specific resources */
    return 0;
}

/* ── PCM Operations: Trigger ─────────────────────────────────────────── */

static int zybo_i2s_dsp_trigger(struct snd_pcm_substream *substream,
                                 int cmd, struct snd_soc_dai *dai)
{
    struct zybo_i2s_dsp *dsp = snd_soc_dai_get_drvdata(dai);
    struct dma_chan *chan;
    struct dma_async_tx_descriptor *desc;
    enum dma_transfer_direction direction;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
        chan = dsp->tx_chan;
        direction = DMA_MEM_TO_DEV;
    } else {
        chan = dsp->rx_chan;
        direction = DMA_DEV_TO_MEM;
    }

    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
    case SNDRV_PCM_TRIGGER_RESUME:
    case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
        if (!chan)
            return -ENODEV;
        /* DMA will be started by the PCM framework via dmaengine_pcm_* */
        break;

    case SNDRV_PCM_TRIGGER_STOP:
    case SNDRV_PCM_TRIGGER_SUSPEND:
    case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
        if (chan)
            dmaengine_terminate_all(chan);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

/* ── DAI Operations ──────────────────────────────────────────────────── */

static const struct snd_soc_dai_ops zybo_i2s_dsp_dai_ops = {
    .hw_params = zybo_i2s_dsp_hw_params,
    .hw_free   = zybo_i2s_dsp_hw_free,
    .trigger   = zybo_i2s_dsp_trigger,
};

/* ── DAI Driver ──────────────────────────────────────────────────────── */

static struct snd_soc_dai_driver zybo_i2s_dsp_dai = {
    .name = "zybo-i2s-dsp",
    .playback = {
        .stream_name  = "Playback",
        .channels_min = 1,
        .channels_max = ZYBO_I2S_DSP_CHANNELS,
        .rates        = ZYBO_I2S_DSP_RATES,
        .formats      = ZYBO_I2S_DSP_FORMATS,
    },
    .capture = {
        .stream_name  = "Capture",
        .channels_min = 1,
        .channels_max = ZYBO_I2S_DSP_CHANNELS,
        .rates        = ZYBO_I2S_DSP_RATES,
        .formats      = ZYBO_I2S_DSP_FORMATS,
    },
    .ops = &zybo_i2s_dsp_dai_ops,
};

/* ── Platform Driver ─────────────────────────────────────────────────── */

static int zybo_i2s_dsp_probe(struct platform_device *pdev)
{
    struct zybo_i2s_dsp *dsp;
    struct resource *res;
    int ret;

    dev_info(&pdev->dev, "ZYBO I2S+DSP DAI driver probing\n");

    dsp = devm_kzalloc(&pdev->dev, sizeof(*dsp), GFP_KERNEL);
    if (!dsp)
        return -ENOMEM;

    dsp->dev = &pdev->dev;

    /* Get register base */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "No memory resource\n");
        return -ENXIO;
    }
    dsp->regs_phys  = res->start;
    dsp->regs_size  = resource_size(res);
    dsp->regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(dsp->regs))
        return PTR_ERR(dsp->regs);

    /* Get clocks */
    dsp->audio_clk = devm_clk_get(&pdev->dev, "audio");
    if (IS_ERR(dsp->audio_clk)) {
        dev_warn(&pdev->dev, "No audio clock, using default\n");
        dsp->audio_clk = NULL;
    }

    dsp->axi_clk = devm_clk_get(&pdev->dev, "axi");
    if (IS_ERR(dsp->axi_clk)) {
        dev_warn(&pdev->dev, "No AXI clock\n");
        dsp->axi_clk = NULL;
    }

    /* Get DMA channels */
    dsp->tx_chan = dma_request_slave_channel(&pdev->dev, "tx");
    if (!dsp->tx_chan)
        dev_warn(&pdev->dev, "No TX DMA channel, playback unavailable\n");

    dsp->rx_chan = dma_request_slave_channel(&pdev->dev, "rx");
    if (!dsp->rx_chan)
        dev_warn(&pdev->dev, "No RX DMA channel, capture unavailable\n");

    /* Enable clocks */
    if (dsp->audio_clk)
        clk_prepare_enable(dsp->audio_clk);
    if (dsp->axi_clk)
        clk_prepare_enable(dsp->axi_clk);

    platform_set_drvdata(pdev, dsp);

    /* Register with ASoC */
    ret = devm_snd_soc_register_component(&pdev->dev,
            &zybo_i2s_dsp_component, &zybo_i2s_dsp_dai, 1);
    if (ret) {
        dev_err(&pdev->dev, "ASoC component register failed: %d\n", ret);
        goto err_clk;
    }

    dev_info(&pdev->dev, "ZYBO I2S+DSP DAI registered (regs @ 0x%pa)\n",
             &dsp->regs_phys);

    return 0;

err_clk:
    if (dsp->audio_clk)
        clk_disable_unprepare(dsp->audio_clk);
    if (dsp->axi_clk)
        clk_disable_unprepare(dsp->axi_clk);
    return ret;
}

static int zybo_i2s_dsp_remove(struct platform_device *pdev)
{
    struct zybo_i2s_dsp *dsp = platform_get_drvdata(pdev);

    if (dsp->tx_chan)
        dma_release_channel(dsp->tx_chan);
    if (dsp->rx_chan)
        dma_release_channel(dsp->rx_chan);
    if (dsp->audio_clk)
        clk_disable_unprepare(dsp->audio_clk);
    if (dsp->axi_clk)
        clk_disable_unprepare(dsp->axi_clk);

    return 0;
}

static const struct of_device_id zybo_i2s_dsp_of_match[] = {
    { .compatible = "digilent,zybo-i2s-dsp-1.0" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zybo_i2s_dsp_of_match);

/* ── ASoC Component ──────────────────────────────────────────────────── */

static const struct snd_soc_component_driver zybo_i2s_dsp_component = {
    .name = "zybo-i2s-dsp",
};

static struct platform_driver zybo_i2s_dsp_plat_driver = {
    .driver = {
        .name           = "zybo-i2s-dsp",
        .of_match_table = zybo_i2s_dsp_of_match,
        .owner          = THIS_MODULE,
    },
    .probe  = zybo_i2s_dsp_probe,
    .remove = zybo_i2s_dsp_remove,
};

module_platform_driver(zybo_i2s_dsp_plat_driver);

MODULE_AUTHOR("ZYBO Audio Project");
MODULE_DESCRIPTION("ASoC CPU DAI Driver for ZYBO FPGA I2S+DSP");
MODULE_LICENSE("GPL v2");
