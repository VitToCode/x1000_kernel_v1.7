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
#include "jz4780_route_conf.h"
/***************************************************************************************\
 *                                                                                     *
 *typical config for each route                                                        *
 *                                                                                     *
\***************************************************************************************/


/*######################################################################################################*/

route_conf_base const record_mic1_an1_to_adclr  = {
	.route_ready_mode = ROUTE_READY_FOR_ADC,	//fix
	/*--------route-----------*/
	//record
	.route_mic1_mode = MIC1_DIFF_WITH_MICBIAS,					//fix
	.route_mic2_mode = MIC2_DISABLE,							//fix
	.route_inputl_mux_mode = INPUTL_MUX_MIC1_TO_AN1,			//fix
	.route_inputl_mode = INPUTL_TO_ADC_ENABLE,					//fix
	.route_inputr_mode = INPUTR_TO_ADC_DISABLE,					//fix
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_DISABLE,	//fix
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_DISABLE,	//fix
	.route_record_mux_mode = RECORD_MUX_INPUTL_TO_LR,			//fix
	.route_adc_mode = ADC_STEREO_WITH_LEFT_ONLY,				//fix
	/*If you just have one mic ,you should select this for stereo output*/
	.route_record_mixer_mode = RECORD_MIXER_MIX_MONO_INPUT_ONLY,	//fix
	.attibute_adc_l_gain	=	23,
	.attibute_adc_r_gain	=	23,
};

route_conf_base const record_mic1_single_an2_to_adclr = {
	.route_ready_mode = ROUTE_READY_FOR_ADC,					//fix
	/*--------route-----------*/
	//record
	.route_mic1_mode = MIC1_SING_WITH_MICBIAS,					//..
	.route_mic2_mode = MIC2_DISABLE,							//fix
	.route_inputl_mux_mode = INPUTL_MUX_MIC1_TO_AN2,			//fix
	.route_inputl_mode = INPUTL_TO_ADC_ENABLE,					//fix
	.route_inputr_mode = INPUTR_TO_ADC_DISABLE,					//fix
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_DISABLE,	//fix
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_DISABLE,	//fix
	.route_record_mux_mode = RECORD_MUX_INPUTL_TO_LR,			//fix
	.route_adc_mode = ADC_STEREO_WITH_LEFT_ONLY,				//fix
	/*If you just have one mic ,you should select this for stereo output*/
	.route_record_mixer_mode = RECORD_MIXER_MIX_MONO_INPUT_ONLY,//fix
	.attibute_adc_l_gain	=	23,
	.attibute_adc_r_gain	=	23,
};
route_conf_base const record_mic2_single_an3_to_adclr  = {
	.route_ready_mode = ROUTE_READY_FOR_ADC,	//fix
	/*--------route-----------*/
	//record
	.route_mic1_mode = MIC1_DISABLE,							//..
	.route_mic2_mode = MIC2_SING_WITH_MICBIAS,					//fix
	.route_inputr_mux_mode = INPUTR_MUX_MIC2_TO_AN3,			//fix
	.route_inputl_mode = INPUTL_TO_ADC_ENABLE,					//fix
	.route_inputr_mode = INPUTR_TO_ADC_DISABLE,					//fix
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_DISABLE,	//fix
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_DISABLE,	//fix
	.route_record_mux_mode = RECORD_MUX_INPUTL_TO_LR,			//..
	.route_adc_mode = ADC_STEREO_WITH_LEFT_ONLY,				//fix
	/*If you just have one mic ,you should select this for stereo output*/
	.route_record_mixer_mode = RECORD_MIXER_MIX_MONO_INPUT_ONLY,//fix
	.attibute_adc_l_gain	=	23,
	.attibute_adc_r_gain	=	23,
};
route_conf_base const record_linein1_diff_to_adclr = {
	.route_ready_mode = ROUTE_READY_FOR_ADC,					//fix
	/*--------route-----------*/
	//record
	.route_mic1_mode = MIC1_DISABLE,							//fix
	.route_mic2_mode = MIC2_DISABLE,							//fix
	.route_line1_mode = LINE1_DIFF,								//fix
	.route_inputl_mode = INPUTL_TO_ADC_ENABLE,					//fix
	.route_inputr_mode = INPUTR_TO_ADC_DISABLE,					//fix
	.route_inputl_mux_mode = INPUTL_MUX_MIC1_TO_AN1,			//..
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_DISABLE,	//fix
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_DISABLE,    //fix
	.route_record_mux_mode = RECORD_MUX_INPUTL_TO_LR,			//fix
	.route_adc_mode = ADC_STEREO_WITH_LEFT_ONLY,				//fix
	/*If you just have one mic ,you should select this for stereo output*/
	.route_record_mixer_mode = RECORD_MIXER_MIX_MONO_INPUT_ONLY,	//fix
};

/*##########################################################################################################*/

route_conf_base const replay_hp_stereo = {
	.route_ready_mode = ROUTE_READY_FOR_DAC, //fix
	/*--------route-----------*/
	//replay
	.route_replay_mixer_mode = REPLAY_MIXER_NOUSE, //fix
	.route_dac_mode = DAC_STEREO, //fix
	.route_hp_mux_mode = HP_MUX_DACL_TO_L_DACR_TO_R, //fix
	.route_hp_mode = HP_ENABLE, //fix
	.route_lineout_mode = LINEOUT_DISABLE,	//fix
	.attibute_hp_r_gain = 0,
	.attibute_hp_l_gain = 0,
};

route_conf_base const replay_lineout_lr = {
	.route_ready_mode = ROUTE_READY_FOR_DAC, //fix
	/*--------route-----------*/
	//replay
	.route_replay_mixer_mode = REPLAY_MIXER_NOUSE,
	.route_dac_mode = DAC_STEREO, //fix
	.route_lineout_mux_mode = LO_MUX_DACLR_TO_LO,	//fix
	.route_hp_mode = HP_DISABLE, //fix
	.route_lineout_mode = LINEOUT_ENABLE, //FIX
};

route_conf_base const repaly_hp_stereo_and_lineout_lr = {
	.route_ready_mode = ROUTE_READY_FOR_DAC, //fix
	/*--------route-----------*/
	//replay
	.route_replay_mixer_mode = REPLAY_MIXER_NOUSE, //fix
	.route_dac_mode = DAC_STEREO, //fix
	.route_hp_mux_mode = HP_MUX_DACL_TO_L_DACR_TO_R, //fix
	.route_hp_mode = HP_ENABLE, //fix
	.route_lineout_mux_mode = LO_MUX_DACLR_TO_LO,	//fix
	.route_lineout_mode = LINEOUT_ENABLE, //FIX
	.attibute_hp_r_gain = 0,
	.attibute_hp_l_gain = 0,

};

/*########################################################################################################*/
route_conf_base const replay_linein2_bypass_to_hp_lr = {
	.route_ready_mode = ROUTE_READY_FOR_DAC,
	//record//		//fix
	.route_mic1_mode = MIC1_DISABLE,
	.route_mic2_mode = MIC2_DISABLE,
	.route_inputr_mux_mode = INPUTR_MUX_LINEIN2_TO_AN3,
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_DISABLE,
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_ENABLE,
	.route_adc_mode = ADC_DISABLE,
	//replay
	.route_replay_mixer_mode = REPLAY_MIXER_PLAYBACK_DAC_ONLY,
	.route_dac_mode = DAC_DISABLE,
	.route_hp_mux_mode = HP_MUX_INPUTR_TO_LR,
	.route_hp_mode = HP_ENABLE,
	.route_lineout_mux_mode = LO_MUX_DACLR_TO_LO,	//new
	.route_lineout_mode = LINEOUT_DISABLE,
};

route_conf_base const replay_linein2_bypass_to_lo_lr = {
	.route_ready_mode = ROUTE_READY_FOR_ADC,
	//record//		//fix
	.route_mic1_mode = MIC1_DISABLE,
	.route_mic2_mode = MIC2_DISABLE,
	.route_inputr_mux_mode = INPUTR_MUX_LINEIN2_TO_AN3,
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_ENABLE,
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_ENABLE,
	.route_adc_mode = ADC_DISABLE,
	//replay
	.route_dac_mode = DAC_DISABLE,
	.route_hp_mux_mode = HP_MUX_INPUTR_TO_LR,
	.route_hp_mode = HP_DISABLE,
	.route_lineout_mux_mode = LO_MUX_INPUTR_TO_LO,//new
	.route_lineout_mode = LINEOUT_ENABLE,
};

/*##############################################################################################################*/

route_conf_base const route_all_clear_conf = {
	.route_ready_mode = ROUTE_READY_FOR_DAC, //fix
	.route_mic1_mode = MIC1_DISABLE,
	.route_mic2_mode = MIC2_DISABLE,
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_DISABLE,
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_DISABLE,
	.route_adc_mode = ADC_DISABLE,
	//replay
	.route_dac_mode = DAC_DISABLE,
	.route_hp_mode = HP_DISABLE,
	.route_lineout_mode = LINEOUT_DISABLE,
};

route_conf_base const route_replay_clear_conf = {
	.route_ready_mode = ROUTE_READY_FOR_DAC, //fix
	/*--------route-----------*/
	.route_dac_mode = DAC_DISABLE,
	.route_hp_mode = HP_DISABLE,
	.route_lineout_mode = LINEOUT_DISABLE,
};

route_conf_base const route_record_clear_conf = {
	.route_ready_mode = ROUTE_READY_FOR_DAC, //fix
	/*--------route-----------*/
	.route_mic1_mode = MIC1_DISABLE,
	.route_mic2_mode = MIC2_DISABLE,
	.route_inputl_to_bypass_mode = INPUTL_TO_BYPASS_DISABLE,
	.route_inputr_to_bypass_mode = INPUTR_TO_BYPASS_DISABLE,
	.route_adc_mode = ADC_DISABLE,
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
		.route_name = SND_ROUTE_RECORD_MIC1_AN1,
		.route_conf = &record_mic1_an1_to_adclr,
	},
	{
		.route_name = SND_ROUTE_RECORD_MIC1_SIN_AN2,
		.route_conf = &record_mic1_single_an2_to_adclr,
	},
	{
		.route_name = SND_ROUTE_RECORD_MIC2_SIN_AN3,
		.route_conf = &record_mic2_single_an3_to_adclr,
	},
	{
		.route_name = SND_ROUTE_RECORD_LINEIN1_DIFF_AN1,
		.route_conf = &record_linein1_diff_to_adclr,
	},

	/*********************** replay route **************************/
	{
		.route_name = SND_ROUTE_REPLAY_DACRL_TO_HPRL,
		.route_conf = &replay_hp_stereo,
	},
	{
		.route_name = SND_ROUTE_REPLAY_DACRL_TO_LO,
		.route_conf = &replay_lineout_lr,
	},
	{
		.route_name = SND_ROUTE_REPLAY_DACRL_TO_ALL,
		.route_conf = &repaly_hp_stereo_and_lineout_lr,
	},
	/*********************** bypass route *************************/
	{
		.route_name = SND_ROUTE_REPLAY_LINEIN2_BYPASS_TO_LINEOUT,
		.route_conf = &replay_linein2_bypass_to_lo_lr,
	},
	{
		.route_name = SND_ROUTE_REPLAY_LINEIN2_BYPASS_TO_HPRL,
		.route_conf = &replay_linein2_bypass_to_hp_lr,
	},
	/***************************end of array***************************/
	{
		.route_name = SND_ROUTE_NONE,
		.route_conf = NULL,
	},
};
