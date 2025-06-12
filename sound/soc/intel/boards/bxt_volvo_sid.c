/*
 * Intel Broxton-P I2S Machine Driver for IVI reference platform
 *
 * Copyright (C) 2014-2015, Intel Corporation. All rights reserved.
 *
 * Modified from:
 *   Intel Skylake I2S Machine driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>


static const struct snd_soc_pcm_stream media1_out_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 3,
	.channels_max = 3,
};

static const struct snd_soc_pcm_stream codec1_in_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 8,
	.channels_max = 8,
};

static const struct snd_soc_dapm_widget broxton_widgets[] = {
	SND_SOC_DAPM_MIC("a2bCp", NULL),
	SND_SOC_DAPM_HP("DiranaPb1", NULL),
	SND_SOC_DAPM_MIC("DiranaCp1", NULL),
	SND_SOC_DAPM_HP("DiranaPb0", NULL),
	SND_SOC_DAPM_MIC("DiranaCp0", NULL),
	SND_SOC_DAPM_MIC("BtCp0", NULL),
	SND_SOC_DAPM_HP("BtPb0", NULL),
   SND_SOC_DAPM_MIC("BtLoopCp0", NULL),
   SND_SOC_DAPM_HP("BtLoopPb0", NULL),
	SND_SOC_DAPM_MIC("AdcCp1", NULL),
	SND_SOC_DAPM_HP("DummySpeaker3", NULL),
	SND_SOC_DAPM_MIC("AnalogTunerCp", NULL),
};

static const struct snd_soc_dapm_route broxton_volvo_sid_map[] = {

	/* Speaker BE connections */
	{ "a2b0_in", NULL, "ssp0 Rx"},
	{ "ssp0 Rx", NULL, "Dummy Capture" },
	{ "Dummy Capture", NULL,"a2bCp"},



	{ "DiranaPb1", NULL, "Dummy Playback"},
	{ "Dummy Playback", NULL,"ssp1 Tx"},
	{ "ssp1 Tx", NULL, "dirana1_out"},

	{ "dirana1_in", NULL, "ssp1 Rx"},
	{ "ssp1 Rx", NULL, "Dummy Capture" },
	{ "Dummy Capture", NULL, "DiranaCp1"},


	{ "DiranaPb0", NULL, "Dummy Playback"},
	{ "Dummy Playback", NULL,"ssp2 Tx"},
	{ "ssp2 Tx", NULL, "dirana0_out"},

	{ "dirana0_in", NULL, "ssp2 Rx"},
	{ "ssp2 Rx", NULL, "Dummy Capture"},
	{ "Dummy Capture", NULL, "DiranaCp0"},



	{ "bt0_in", NULL, "ssp3 Rx"},
	{ "ssp3 Rx", NULL, "Dummy Capture"},
	{ "Dummy Capture", NULL, "BtCp0"},

	{ "BtPb0", NULL, "Dummy Playback"},
	{ "Dummy Playback", NULL, "ssp3 Tx"},
	{ "ssp3 Tx", NULL, "bt0_out"},


   { "BtLoopIn", NULL, "ssp3 Rx-b"},
   { "ssp3 Rx-b", NULL, "Dummy Capture"},
   { "Dummy Capture", NULL, "BtLoopCp0"},

   { "BtLoopPb0", NULL, "Dummy Playback"},
   { "Dummy Playback", NULL, "ssp3 Tx-b"},
   { "ssp3 Tx-b", NULL, "BtLoopOut"},


	{ "adc1_in", NULL, "ssp5 Rx"},
	{ "ssp5 Rx", NULL, "Dummy Capture" },
	{ "Dummy Capture", NULL, "AdcCp1"},


	/* (ANC) Codec1_in - Loop pipe */
	{ "codec1_in", NULL, "ssp0-b Rx" },
	{ "ssp0-b Rx", NULL, "Dummy Capture" },

	/* Media1_out Loop Path */
	{"DummySpeaker3", NULL, "Dummy Playback"},
	{ "Dummy Playback", NULL, "ssp1-b Tx"},
	{ "ssp1-b Tx", NULL, "media1_out"},

			/* Analog Tuner Path */
	{ "AnalogTunerIn", NULL, "ssp5 Rx-b"},  /*make the fdk reflect this DAPM port: AnalogTunerIn*/
	{ "ssp5 Rx-b", NULL, "Dummy Capture"},
	{ "Dummy Capture", NULL, "AnalogTunerCp"},
};

/* broxton digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link broxton_volvo_sid_dais[] = {
	/* Front End DAI links */
	{
		.name = "A2B Cp Port",
		.stream_name = "A2B Cp",
		.cpu_dai_name = "System Pin 6",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
	},
	{
		.name = "Dirana Port 1",
		.stream_name = "Dirana 1",
		.cpu_dai_name = "System Pin 3",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Dirana Port 0",
		.stream_name = "Dirana 0",
		.cpu_dai_name = "System Pin 5",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "BT Port 0",
		.stream_name = "BT 0",
		.cpu_dai_name = "System Pin 4",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "BT Loop Port 0",
		.stream_name = "BT LOOP",
		.cpu_dai_name = "BTLoop",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "ADC Cp Port 1",
		.stream_name = "ADC Cp 1",
		.cpu_dai_name = "System Pin 7",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.init = NULL,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.nonatomic = 1,
		.dynamic = 1,
	},
	{
		.name = "Bxtn SSP0 Port",
		.stream_name = "Bxtn SSP0",
		.cpu_dai_name = "SSP0-B Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.params = &codec1_in_params,
		.dsp_loopback = true,
	},
	{
		.name = "Bxtn SSP1 port",
		.stream_name = "Bxtn SSP2",
		.cpu_dai_name = "SSP1-B Pin",
		.platform_name = "0000:00:0e.0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.params = &media1_out_params,
		.dsp_loopback = true,
	},
	{
		.name = "Analog Tuner",
		.stream_name = "Analog Tuner Stream",
		.cpu_dai_name = "AnalogTuner",
		.platform_name = "0000:00:0e.0",
		.nonatomic = 1,
		.dynamic = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_capture = 1,
	},

	/* Back End DAI links */
	{
		/* SSP0 - A2B */
		.name = "SSP0-Codec",
		.id = 0,
		.cpu_dai_name = "SSP0 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP1 - Dirana 1 */
		.name = "SSP1-Codec",
		.id = 1,
		.cpu_dai_name = "SSP1 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP2 - Dirana 0 */
		.name = "SSP2-Codec",
		.id = 2,
		.cpu_dai_name = "SSP2 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP3 - BT */
		.name = "SSP3-Codec",
		.id = 3,
		.cpu_dai_name = "SSP3 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP3 - BT 2nd path */
		.name = "SSP3-Codec-2",
		.id = 4,
		.cpu_dai_name = "SSP3-B Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP5 - ADC 1 */
		.name = "SSP5-Codec",
		.id = 5,
		.cpu_dai_name = "SSP5 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		/* SSP5-B - Analogue Tuner */
		.name = "SSP5-Codec-2",
		.id = 6,
		.cpu_dai_name = "SSP5-B Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
    }
};

static int bxt_add_dai_link(struct snd_soc_card *card,
			struct snd_soc_dai_link *link)
{
	link->platform_name = "0000:00:0e.0";
	link->nonatomic = 1;
	return 0;
}

/* broxton audio machine driver for SPT + RT298S */
static struct snd_soc_card broxton_volvo_sid = {
	.name = "broxton-volvo_sid",
	.owner = THIS_MODULE,
	.dai_link = broxton_volvo_sid_dais,
	.num_links = ARRAY_SIZE(broxton_volvo_sid_dais),
	.dapm_widgets = broxton_widgets,
	.num_dapm_widgets = ARRAY_SIZE(broxton_widgets),
	.dapm_routes = broxton_volvo_sid_map,
	.num_dapm_routes = ARRAY_SIZE(broxton_volvo_sid_map),
	.fully_routed = true,
	.add_dai_link = bxt_add_dai_link,
};

static int broxton_audio_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s registering %s\n", __func__, pdev->name);
	broxton_volvo_sid.dev = &pdev->dev;
	return snd_soc_register_card(&broxton_volvo_sid);
}

static int broxton_audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&broxton_volvo_sid);
	return 0;
}

static struct platform_driver broxton_audio = {
	.probe = broxton_audio_probe,
	.remove = broxton_audio_remove,
	.driver = {
		.name = "volvo_sid_machine",
		.pm = &snd_soc_pm_ops,
	},
};

module_platform_driver(broxton_audio)

/* Module information */
MODULE_DESCRIPTION("Intel SST Audio for Broxton Volvo SID");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:volvo_sid_machine");
