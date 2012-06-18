/*
 * Linux/sound/oss/jz4760_route_conf.h
 *
 * DLV CODEC driver for Ingenic Jz4760 MIPS processor
 *
 * 2010-11-xx   jbbi <jbbi@ingenic.cn>
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#ifndef __JZ4780_ROUTE_CONF_H__
#define __JZ4780_ROUTE_CONF_H__

#include <mach/jzsnd.h>

typedef struct __route_conf_base {
	/*--------route-----------*/
	int route_ready_mode;
	//record//
	int route_mic1_mode;
	int route_mic2_mode;
	int route_inputl_mux_mode;
	int route_inputr_mux_mode;
	int route_inputl_to_bypass_mode;
	int route_inputr_to_bypass_mode;
	int route_record_mux_mode;
	int route_adc_mode;
	int route_record_mixer_mode;
	//replay
	int route_replay_mixer_mode;
	int route_dac_mode;
	int route_hp_mux_mode;
	int route_hp_mode;
	int route_lineout_mux_mode; //new
	int route_lineout_mode;
	/*--------attibute-------*/
	int attibute_agc_mode;

	/* gain note: use 32 instead of 0 */
	/*gain of mic or linein to adc ??*/
	int attibute_input_l_gain;			//val: 32(0), +4, +8, +16, +20 (dB);
	int attibute_input_r_gain;			//val: 32(0), +4, +8, +16, +20 (dB);
	int attibute_input_bypass_l_gain;	//val: +6 ~ +1, 32(0), -1 ~ -25 (dB);
	int attibute_input_bypass_r_gain;	//val: +6 ~ +1, 32(0), -1 ~ -25 (dB);
	int attibute_adc_l_gain;			//val: 32(0), +1 ~ +43 (dB);
	int attibute_adc_r_gain;			//val: 32(0), +1 ~ +43 (dB);
	int attibute_record_mixer_gain;		//val: 32(0), -1 ~ -31 (dB);
	int attibute_replay_mixer_gain;		//val: 32(0), -1 ~ -31 (dB);
	int attibute_dac_l_gain;			//val: 32(0), -1 ~ -31 (dB);
	int attibute_dac_r_gain;			//val: 32(0), -1 ~ -31 (dB);
	int attibute_hp_l_gain;				//val: +6 ~ +1, 32(0), -1 ~ -25 (dB);
	int attibute_hp_r_gain;				//val: +6 ~ +1, 32(0), -1 ~ -25 (dB);
} route_conf_base;

struct __codec_route_info {
	enum snd_codec_route_t route_name;
	route_conf_base const *route_conf;
};

/*================ route conf ===========================*/

#define DISABLE								99

/*-------------- route part selection -------------------*/

/*route global init??*/
#define ROUTE_READY_FOR_ADC					1
#define ROUTE_READY_FOR_DAC					2
#define ROUTE_READY_FOR_ADC_DAC				3

/*left input mux */
#define INPUTL_MUX_MIC1_TO_AN1				1
#define INPUTL_MUX_MIC1_TO_AN2				2
#define INPUTL_MUX_LINEIN1_TO_AN1			4
#define INPUTL_MUX_LINEIN1_TO_AN2			8

/*right input mux*/
#define INPUTR_MUX_MIC2_TO_AN3				1
#define INPUTR_MUX_MIC2_TO_AN4				2
#define INPUTR_MUX_LINEIN2_TO_AN3			4
#define INPUTR_MUX_LINEIN2_TO_AN4			8

/*mic1 mode select*/
#define MIC1_DIFF_WITH_MICBIAS				1
#define MIC1_DIFF_WITHOUT_MICBIAS			2
#define MIC1_SING_WITH_MICBIAS				3
#define MIC1_SING_WITHOUT_MICBIAS			4
#define MIC1_DISABLE						DISABLE

/*mic2 mode select*/
#define MIC2_DIFF_WITH_MICBIAS				1
#define MIC2_DIFF_WITHOUT_MICBIAS			2
#define MIC2_SING_WITH_MICBIAS				3
#define MIC2_SING_WITHOUT_MICBIAS			4
#define MIC2_DISABLE						DISABLE

/*left input bypass to output*/
#define INPUTL_TO_BYPASS_ENABLE				1
#define INPUTL_TO_BYPASS_DISABLE			DISABLE

/*right input bypass to output*/
#define INPUTR_TO_BYPASS_ENABLE				1
#define INPUTR_TO_BYPASS_DISABLE			DISABLE


/*record mux*/
#define RECORD_MUX_INPUTL_TO_LR				1		/*mic or linein mono*/
#define RECORD_MUX_INPUTR_TO_LR				2		/*mic or linein mono*/
#define RECORD_MUX_INPUTL_TO_L_INPUTR_TO_R	3		/*mic or linein stereo*/
#define RECORD_MUX_INPUTR_TO_L_INPUTL_TO_R	4		/*mic or linein stereo*/
#define RECORD_MUX_DIGITAL_MIC				5		/*digital mic stereo*/

/*adc mode*/
#define ADC_STEREO							1
#define ADC_STEREO_WITH_LEFT_ONLY			2
#define ADC_DISABLE							DISABLE

/*record mixer*/
#define RECORD_MIXER_MIX1_INPUT_ONLY		1
#define RECORD_MIXER_MIX1_INPUT_AND_DAC		2

/*lineout mode*/
#define LINEOUT_ENABLE						1
#define LINEOUT_DISABLE						DISABLE


/*line out mux*/
#define LO_MUX_INPUTL_TO_LO					1
#define LO_MUX_INPUTR_TO_LO					2
#define LO_MUX_INPUTLR_TO_LO				3
#define LO_MUX_DACL_TO_LO					4
#define LO_MUX_DACR_TO_LO					5
#define LO_MUX_DACLR_TO_LO					6

/*headphone mode*/
#define HP_ENABLE							1
#define HP_DISABLE							DISABLE

/*headphone mux*/
#define HP_MUX_DACL_TO_L_DACR_TO_R			1
#define HP_MUX_DACL_TO_LR					2
#define HP_MUX_INPUTL_TO_L_INPUTR_TO_R		3
#define HP_MUX_INPUTL_TO_LR					4
#define HP_MUX_INPUTR_TO_LR					5

/*dac mode*/
#define DAC_STEREO							1
#define DAC_STEREO_WITH_LEFT_ONLY			2
#define DAC_DISABLE							DISABLE


/*replay mixer*/
#define REPLAY_MIXER_PLAYBACK_DAC_ONLY		1
#define REPLAY_MIXER_PLAYBACK_DAC_AND_ADC	2


/*other control*/
#define RECORD_WND_FILTER					0X01
#define RECORD_HIGH_PASS_FILTER				0X02

#define AGC_ENABLE							1
#define AGC_DISABLE							DISABLE

extern struct __codec_route_info codec_route_info[];

#endif /*__JZ4780_ROUTE_CONF_H__*/
