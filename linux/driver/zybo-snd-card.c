// SPDX-License-Identifier: GPL-2.0
/*
 * ZYBO Audio ASoC Sound Card Driver
 * Based on: zed_pl_snd_card.c by Yuhei Horibe (GPL v2)
 * Adapted for: Digilent ZYBO + d_axi_i2s_audio IP + SSM2603 codec
 *
 * The d_axi_i2s_audio IP handles I2S TX/RX via AXI-Stream.
 * AXI DMA (xlnx,axi-dma) provides the platform DMA.
 * SSM2603 codec provides analog I/O via PL I2C.
 *
 * DT binding:
 *   cpu_dai node must have:
 *     - compatible = "digilent,d_axi_i2s_audio-2.0"
 *     - xlnx,snd-pcm = <&axi_dma_node>
 *     - audio-codec  = <&ssm2603_node>
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

/* d_axi_i2s_audio register map (offset from base 0x43C00000) */
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

/* PL card private data (following xlnx_snd_common.h pattern) */
struct zybo_card_data {
	struct clk *mclk;
	u32 mclk_val;
	u32 mclk_ratio;
	void __iomem *base;
};

static void zybo_i2s_write(struct zybo_card_data *prv, u32 reg, u32 val)
{
	writel(val, prv->base + reg);
}

/* Reset and initialize the I2S core */
static void zybo_i2s_init(struct zybo_card_data *prv)
{
	zybo_i2s_write(prv, I2S_RESET_REG, 0x1);
	udelay(10);
	zybo_i2s_write(prv, I2S_CLOCK_CTRL, RATE_48K);
	zybo_i2s_write(prv, I2S_FIFO_CTRL, 0x3);
	udelay(10);
	zybo_i2s_write(prv, I2S_FIFO_CTRL, 0x0);
	zybo_i2s_write(prv, I2S_RESET_REG, 0x0);
	udelay(10);
}

static int zybo_snd_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct zybo_card_data *prv = snd_soc_card_get_drvdata(rtd->card);
	u32 ch, rate_code, period;
	int ret;
	unsigned int fmt;

	ch = params_channels(params);

	/* Codec: SSM2603 as I2S slave, FPGA as master */
	fmt = SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_I2S;
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret)
		return ret;

	/* Set codec system clock from our MCLK */
	prv->mclk_val = clk_get_rate(prv->mclk);
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, prv->mclk_val,
				     SND_SOC_CLOCK_IN);
	if (ret && ret != -ENOTSUPP)
		return ret;

	/* CPU DAI: configure sampling rate */
	switch (params_rate(params)) {
	case 48000:  rate_code = RATE_48K;  break;
	case 44100:  rate_code = RATE_44K1; break;
	default:
		return -EINVAL;
	}
	zybo_i2s_write(prv, I2S_CLOCK_CTRL, rate_code);

	/* Set period count for stream mode */
	period = params_period_size(params);
	zybo_i2s_write(prv, I2S_PERIOD_COUNT, period & 0x1FFFFF);

	prv->mclk_ratio = DIV_ROUND_UP(prv->mclk_val, params_rate(params));

	return 0;
}

static int zybo_snd_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct zybo_card_data *prv = snd_soc_dai_get_drvdata(dai);

	zybo_i2s_init(prv);
	return 0;
}

static int zybo_snd_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *dai)
{
	struct zybo_card_data *prv = snd_soc_dai_get_drvdata(dai);
	u32 val;

	val = readl(prv->base + I2S_STREAM_CTRL);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			val |= 0x1;
		else
			val |= 0x2;
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

	writel(val, prv->base + I2S_STREAM_CTRL);
	return 0;
}

static const struct snd_soc_dai_ops zybo_snd_dai_ops = {
	.startup   = zybo_snd_startup,
	.hw_params = zybo_snd_hw_params,
	.trigger   = zybo_snd_trigger,
};

#define ZYBO_I2S_RATES  (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#define ZYBO_I2S_FORMATS SNDRV_PCM_FMTBIT_S24_LE

static struct snd_soc_dai_driver zybo_cpu_dai = {
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
	.ops = &zybo_snd_dai_ops,
};

/* DAI link definitions (Zedboard pattern) */
SND_SOC_DAILINK_DEFS(zybo_i2s_playback,
	DAILINK_COMP_ARRAY(COMP_CPU("zybo-i2s-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "ssm2602-hifi")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(NULL)));

SND_SOC_DAILINK_DEFS(zybo_i2s_capture,
	DAILINK_COMP_ARRAY(COMP_CPU("zybo-i2s-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "ssm2602-hifi")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(NULL)));

static struct snd_soc_dai_link zybo_snd_dai[] = {
	{
		.name = "ZYBO Audio Playback",
		.stream_name = "ZYBO Audio",
		SND_SOC_DAILINK_REG(zybo_i2s_playback),
		.ops = NULL,
	},
	{
		.name = "ZYBO Audio Capture",
		.stream_name = "ZYBO Audio",
		SND_SOC_DAILINK_REG(zybo_i2s_capture),
		.ops = NULL,
	},
};

/* DAPM widgets and routes (Zedboard pattern) */
static const struct snd_soc_dapm_widget zybo_snd_widgets[] = {
	SND_SOC_DAPM_SPK("Line Out", NULL),
	SND_SOC_DAPM_HP("Headphone Out", NULL),
	SND_SOC_DAPM_MIC("Mic In", NULL),
};

static const struct snd_soc_dapm_route zybo_snd_routes[] = {
	{ "Line Out", NULL, "LOUT" },
	{ "Line Out", NULL, "ROUT" },
	{ "Headphone Out", NULL, "LHP" },
	{ "Headphone Out", NULL, "RHP" },
	{ "Mic In", NULL, "MICBIAS" },
};

static int zybo_snd_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct snd_soc_dai_link *dai;
	struct zybo_card_data *prv;
	struct device_node *cpu_node, *codec_node, *platform_node;
	struct resource *res;
	int i, ret;

	cpu_node = pdev->dev.of_node;
	if (!cpu_node)
		return -ENODEV;

	/* Find platform (DMA) and codec via DT phandles (Zedboard pattern) */
	platform_node = of_parse_phandle(cpu_node, "xlnx,snd-pcm", 0);
	if (!platform_node) {
		dev_err(&pdev->dev, "platform (DMA) node not found\n");
		return -ENODEV;
	}

	codec_node = of_parse_phandle(cpu_node, "audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev, "codec node not found\n");
		of_node_put(platform_node);
		return -ENODEV;
	}

	/* Allocate card and private data */
	card = devm_kzalloc(&pdev->dev, sizeof(*card), GFP_KERNEL);
	if (!card) {
		ret = -ENOMEM;
		goto out;
	}

	card->dev = &pdev->dev;
	card->name = "ZYBO Audio DSP";
	card->owner = THIS_MODULE;

	prv = devm_kzalloc(&pdev->dev, sizeof(*prv), GFP_KERNEL);
	if (!prv) {
		ret = -ENOMEM;
		goto out;
	}

	/* Map I2S IP registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		goto out;
	}
	prv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(prv->base)) {
		ret = PTR_ERR(prv->base);
		goto out;
	}

	/* Get MCLK */
	prv->mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(prv->mclk)) {
		dev_warn(&pdev->dev, "no mclk, using FCLK0\n");
		prv->mclk = devm_clk_get(&pdev->dev, NULL);
	}
	if (!IS_ERR(prv->mclk))
		clk_prepare_enable(prv->mclk);

	snd_soc_card_set_drvdata(card, prv);

	/* Set up DAI links (Zedboard pattern: iterate over paths) */
	card->dai_link = devm_kzalloc(&pdev->dev,
				      sizeof(*dai) * ARRAY_SIZE(zybo_snd_dai),
				      GFP_KERNEL);
	if (!card->dai_link) {
		ret = -ENOMEM;
		goto clk_out;
	}

	card->num_links = 0;
	for (i = 0; i < ARRAY_SIZE(zybo_snd_dai); i++) {
		dai = &card->dai_link[card->num_links];
		*dai = zybo_snd_dai[i];
		dai->platforms->of_node = platform_node;
		dai->codecs->of_node = codec_node;
		dai->cpus->of_node = cpu_node;
		card->num_links++;
	}

	/* DAPM widgets and routes */
	card->dapm_widgets = zybo_snd_widgets;
	card->num_dapm_widgets = ARRAY_SIZE(zybo_snd_widgets);
	card->dapm_routes = zybo_snd_routes;
	card->num_dapm_routes = ARRAY_SIZE(zybo_snd_routes);
	card->fully_routed = true;

	/* Register with ASoC */
	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sound card: %d\n", ret);
		goto clk_out;
	}

	/* Register CPU DAI */
	ret = devm_snd_soc_register_component(&pdev->dev, NULL,
					      &zybo_cpu_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to register CPU DAI: %d\n", ret);
		goto clk_out;
	}

	/* Register PCM via DMA engine */
	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "failed to register PCM: %d\n", ret);
		goto clk_out;
	}

	dev_info(&pdev->dev, "ZYBO Audio DSP registered\n");
	of_node_put(platform_node);
	of_node_put(codec_node);
	return 0;

clk_out:
	if (!IS_ERR(prv->mclk))
		clk_disable_unprepare(prv->mclk);
out:
	of_node_put(platform_node);
	of_node_put(codec_node);
	return ret;
}

static int zybo_snd_remove(struct platform_device *pdev)
{
	struct zybo_card_data *prv = dev_get_drvdata(&pdev->dev);

	if (!IS_ERR(prv->mclk))
		clk_disable_unprepare(prv->mclk);
	return 0;
}

static const struct of_device_id zybo_snd_of_match[] = {
	{ .compatible = "digilent,d_axi_i2s_audio-2.0" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, zybo_snd_of_match);

static struct platform_driver zybo_snd_driver = {
	.driver = {
		.name = "zybo-snd-card",
		.of_match_table = zybo_snd_of_match,
	},
	.probe  = zybo_snd_probe,
	.remove = zybo_snd_remove,
};
module_platform_driver(zybo_snd_driver);

MODULE_DESCRIPTION("ZYBO Audio ASoC Sound Card Driver (Zedboard pattern)");
MODULE_AUTHOR("ZYBO Audio Project");
MODULE_LICENSE("GPL v2");
