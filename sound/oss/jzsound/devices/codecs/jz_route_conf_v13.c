/*
 * Linux/sound/oss/jz4760_route_conf.h
 *
 * DLV CODEC driver for Ingenic Jz4760 MIPS processor
 *
 * 2010-11-xx   jbbi <jbbi@ingenic.cn>
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#include <mach/jzsnd.h>
#include "jz_route_conf_v13.h"
/***************************************************************************************\
 *                                                                                     *
 *typical config for each route                                                        *
 *                                                                                     *
\***************************************************************************************/


/*######################################################################################################*/
route_conf_base const record_amic = {
	.route_ready_mode = ROUTE_READY_FOR_ADC,

	.route_record_mux_mode = RECORD_NORMAL_INPUT,
	.route_input_mode = INPUT_TO_ADC_ENABLE,
	.route_adc_mode = ADC_ENABLE_WITH_AMIC,
	.route_mic_mode = MIC_ENABLE,

	.route_output_mode = OUTPUT_FROM_DAC_DISABLE,
	.route_dac_mode = DAC_DISABLE,
};


/*##########################################################################################################*/
route_conf_base const replay_spk = {
	.route_ready_mode = ROUTE_READY_FOR_DAC,

	.route_mic_mode = MIC_DISABLE,
	.route_input_mode = INPUT_TO_ADC_DISABLE,
	.route_adc_mode = ADC_DISABLE,

	.route_replay_mux_mode = REPLAY_NORMAL_INPUT,
	.route_output_mode = OUTPUT_FROM_DAC_ENABLE,
	.route_dac_mode = DAC_ENABLE,
};

/*########################################################################################################*/

route_conf_base const record_amic_and_replay_spk = {
	.route_ready_mode = ROUTE_READY_FOR_ADC_DAC,

	.route_record_mux_mode = RECORD_NORMAL_INPUT,
	.route_input_mode = INPUT_TO_ADC_ENABLE,
	.route_adc_mode = ADC_ENABLE_WITH_AMIC,
	.route_mic_mode = MIC_ENABLE,

	.route_replay_mux_mode = REPLAY_NORMAL_INPUT,
	.route_output_mode = OUTPUT_FROM_DAC_ENABLE,
	.route_dac_mode = DAC_ENABLE,
};
/*##############################################################################################################*/

route_conf_base const route_all_clear_conf = {
	.route_ready_mode = ROUTE_READY_FOR_ADC_DAC,
	.route_record_mux_mode = RECORD_NORMAL_INPUT,
	.route_input_mode = INPUT_TO_ADC_DISABLE,
	.route_adc_mode = ADC_DISABLE,
	.route_mic_mode = MIC_DISABLE,

	.route_replay_mux_mode = REPLAY_NORMAL_INPUT,
	.route_output_mode = OUTPUT_FROM_DAC_DISABLE,
	.route_dac_mode = DAC_DISABLE,
};

route_conf_base const route_replay_clear_conf = {
	.route_ready_mode = ROUTE_READY_FOR_DAC,
	.route_output_mode = OUTPUT_FROM_DAC_DISABLE,
	.route_dac_mode = DAC_DISABLE,
};

route_conf_base const route_record_clear_conf = {
	.route_ready_mode = ROUTE_READY_FOR_DAC,
	.route_input_mode = INPUT_TO_ADC_DISABLE,
	.route_adc_mode = ADC_DISABLE,
	.route_mic_mode = MIC_DISABLE,
};

/*######################################################################################################*/

struct __codec_route_info codec_route_info[] = {

	/************************ route clear ************************/
	{
		.route_name = SND_ROUTE_ALL_CLEAR,
		.route_conf = &route_all_clear_conf,
	},
	{
		.route_name = SND_ROUTE_REPLAY_CLEAR,
		.route_conf = &route_replay_clear_conf,
	},
	{
		.route_name = SND_ROUTE_RECORD_CLEAR,
		.route_conf = &route_record_clear_conf,
	},

	/*********************** record route *************************/
	{
		.route_name = SND_ROUTE_RECORD_AMIC,
		.route_conf = &record_amic,
	},
	/*********************** replay route **************************/
	{
		.route_name = SND_ROUTE_REPLAY_SPK,
		.route_conf = &replay_spk,
	},
	/*********************** misc route *******************************/
	{
		.route_name = SND_ROUTE_RECORD_AMIC_AND_REPLAY_SPK,
		.route_conf = &record_amic_and_replay_spk,
	},

	/***************************end of array***************************/
	{
		.route_name = SND_ROUTE_NONE,
		.route_conf = NULL,
	},
};
