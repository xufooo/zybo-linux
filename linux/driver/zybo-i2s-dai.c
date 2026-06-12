// SPDX-License-Identifier: GPL-2.0
/*
 * ZYBO I2S Audio DAI Driver — for Digilent d_axi_i2s_audio IP
 *
 * Provides the CPU DAI for the ZYBO audio system.
 * Connects AXI DMA (via dmaengine API) to the I2S codec (SSM2603)
 * through the d_axi_i2s_audio IP in the FPGA PL.
 *
 * Register map (d_axi_i2s_audio, offset from 0x43C00000):
 *   0x00 I2S_RESET_REG         [0] core reset
 *   0x04 I2S_TRANSFER_CTRL     TX/RX reset sync
 *   0x08 I2S_FIFO_CTRL         TX/RX FIFO reset
 *   0x14 I2S_STATUS_REG        [0]=TX empty, [1]=TX full, [16]=RX empty, [17]=RX full
 *   0x18 I2S_CLOCK_CONTROL     [3:0] sampling rate (0=48k,1=44.1k,2=32k,3=96k)
 *   0x1C I2S_PERIOD_COUNT      [20:0] samples per period
 *   0x20 I2S_STREAM_CTRL       [0]=TX_EN, [1]=RX_EN
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>

#define DRV_NAME "zybo-i2s-dai"

/* Registers */
#define I2S_RESET_REG     0x00
#define I2S_TRANSFER_CTRL 0x04
#define I2S_FIFO_CTRL     0x08
#define I2S_STATUS_REG    0x14
#define I2S_CLOCK_CTRL    0x18
#define I2S_PERIOD_COUNT  0x1C
#define I2S_STREAM_CTRL   0x20

/* Sampling rate codes (from i2s_ctl.vhd) */
#define RATE_48K   0x0
#define RATE_44K1  0x1
#define RATE_32K   0x2
#define RATE_96K   0x3

struct zybo_i2s_dai {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct snd_dmaengine_dai_dma_data dma_data[2]; /* 0=playback, 1=capture */
};

static void zybo_i2s_write(struct zybo_i2s_dai *dai, u32 reg, u32 val)
{
	writel(val, dai->base + reg);
}

static u32 zybo_i2s_read(struct zybo_i2s_dai *dai, u32 reg)
{
	return readl(dai->base + reg);
}

/* Reset and initialize the I2S core */
static void zybo_i2s_reset(struct zybo_i2s_dai *dai)
{
	/* Reset core */
	zybo_i2s_write(dai, I2S_RESET_REG, 0x1);
	udelay(10);
	/* Set 48kHz */
	zybo_i2s_write(dai, I2S_CLOCK_CTRL, RATE_48K);
	/* Reset FIFOs */
	zybo_i2s_write(dai, I2S_FIFO_CTRL, 0x3);
	udelay(10);
	zybo_i2s_write(dai, I2S_FIFO_CTRL, 0x0);
	/* Release reset */
	zybo_i2s_write(dai, I2S_RESET_REG, 0x0);
	udelay(10);
}

static int zybo_i2s_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct zybo_i2s_dai *priv = snd_soc_dai_get_drvdata(dai);

	zybo_i2s_reset(priv);
	return 0;
}

static int zybo_i2s_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct zybo_i2s_dai *priv = snd_soc_dai_get_drvdata(dai);
	u32 rate_code;
	u32 period;

	switch (params_rate(params)) {
	case 48000:  rate_code = RATE_48K;  break;
	case 44100:  rate_code = RATE_44K1; break;
	case 32000:  rate_code = RATE_32K;  break;
	case 96000:  rate_code = RATE_96K;  break;
	default:
		dev_err(priv->dev, "unsupported rate: %d\n", params_rate(params));
		return -EINVAL;
	}

	zybo_i2s_write(priv, I2S_CLOCK_CTRL, rate_code);

	/* Set period count (in samples per channel) */
	period = params_period_size(params);
	zybo_i2s_write(priv, I2S_PERIOD_COUNT, period & 0x1FFFFF);

	dev_dbg(priv->dev, "hw_params: rate=%d, period=%d\n",
		params_rate(params), period);

	return 0;
}

static int zybo_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *dai)
{
	struct zybo_i2s_dai *priv = snd_soc_dai_get_drvdata(dai);
	u32 val;

	val = zybo_i2s_read(priv, I2S_STREAM_CTRL);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			val |= 0x1; /* TX_STREAM_EN */
		else
			val |= 0x2; /* RX_STREAM_EN */
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			val &= ~0x1;
		else
			val &= ~0x2;
		break;

	default:
		return -EINVAL;
	}

	zybo_i2s_write(priv, I2S_STREAM_CTRL, val);
	return 0;
}

static const struct snd_soc_dai_ops zybo_i2s_dai_ops = {
	.startup   = zybo_i2s_startup,
	.hw_params = zybo_i2s_hw_params,
	.trigger   = zybo_i2s_trigger,
};

#define ZYBO_I2S_RATES  (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
			 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000)
#define ZYBO_I2S_FORMATS SNDRV_PCM_FMTBIT_S24_LE

static struct snd_soc_dai_driver zybo_i2s_dai = {
	.name = "zybo-i2s-dai",
	.playback = {
		.stream_name  = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates        = ZYBO_I2S_RATES,
		.formats      = ZYBO_I2S_FORMATS,
	},
	.capture = {
		.stream_name  = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates        = ZYBO_I2S_RATES,
		.formats      = ZYBO_I2S_FORMATS,
	},
	.ops = &zybo_i2s_dai_ops,
};

static const struct snd_soc_component_driver zybo_i2s_component = {
	.name = DRV_NAME,
};

static int zybo_i2s_probe(struct platform_device *pdev)
{
	struct zybo_i2s_dai *priv;
	struct resource *res;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->dev = &pdev->dev;

	/* Map registers */
	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	/* Get register base address for DMA config */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	/* Get clock */
	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(priv->clk),
				     "failed to get clock\n");

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	/* Set up DMA engine data */
	priv->dma_data[0].addr      = res->start + I2S_STREAM_CTRL;
	priv->dma_data[0].addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	priv->dma_data[0].maxburst  = 16;

	priv->dma_data[1] = priv->dma_data[0];

	dev_set_drvdata(&pdev->dev, priv);

	/* Register with ASoC */
	ret = devm_snd_soc_register_component(&pdev->dev,
			&zybo_i2s_component, &zybo_i2s_dai, 1);
	if (ret) {
		clk_disable_unprepare(priv->clk);
		return ret;
	}

	/* Register PCM with DMA engine */
	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	if (ret) {
		clk_disable_unprepare(priv->clk);
		return ret;
	}

	dev_info(&pdev->dev, "ZYBO I2S DAI registered\n");
	return 0;
}

static int zybo_i2s_remove(struct platform_device *pdev)
{
	struct zybo_i2s_dai *priv = dev_get_drvdata(&pdev->dev);

	clk_disable_unprepare(priv->clk);
	return 0;
}

static const struct of_device_id zybo_i2s_of_match[] = {
	{ .compatible = "digilent,d_axi_i2s_audio-2.0" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zybo_i2s_of_match);

static struct platform_driver zybo_i2s_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = zybo_i2s_of_match,
	},
	.probe  = zybo_i2s_probe,
	.remove = zybo_i2s_remove,
};
module_platform_driver(zybo_i2s_driver);

MODULE_AUTHOR("ZYBO Audio Project");
MODULE_DESCRIPTION("ZYBO d_axi_i2s_audio CPU DAI Driver");
MODULE_LICENSE("GPL");
