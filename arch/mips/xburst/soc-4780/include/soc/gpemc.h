/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  GPEMC(NEMC) support functions
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __SOC_GPEMC_H__
#define __SOC_GPEMC_H__

typedef enum {
	bank_type_sram = 0,
	bank_type_nand,
	bank_type_toggle
} bank_type_t;

typedef enum {
	bus_width_8 = 0
} bus_width_t;

typedef enum  {
	burst_length_4 = 0,
	burst_length_8,
	burst_length_16,
	burst_length_32
} burst_length_t;

typedef enum {
	sram_type_normal = 0,
	sram_type_burst
} sram_type_t;

typedef struct {
	struct {

		/*
		 * CS/ADDR/DATA(write)
		 * ______<-------    Tah+Tas+Taw<+2>    ------> ________
		 *       |_____________________________________|
		 *       +                                     +
		 *       |         +             +             |
		 *       |<--Tas-->|             |             |
		 *       +         |             |<--- Tah --->|
		 * WE/RD           |<--- Taw --->|             +
		 * ________________+             +_______________________
		 *                 |_____________|
		 * DATA(read)
		 * _________________________         ____________________
		 *                          |_______|
		 *
		 */

		/* every timing parameter count in nanoseconds */
		u32 Tstrv;
		u32 Taw;
		u32 Tbp;
		u32 Tah;
		u32 Tas;

		/* access attributes */
		bus_width_t BW;
		burst_length_t BL;
		sram_type_t sram_type;
	} sram_timing;

	/*
	 * TODO
	 */
	struct {
		u32 Trv;
		u32 Trw;
		u32 Tww;
		u32 Tah;
		u32 Tas;
		u32 Tdpht;
		u32 Tdqsre;
		u32 Tfda;
		u32 Tclr;
		u32 Tdphtd;
		u32 Tcdqss;
		u32 Tcwaw;
	} toggle_timing;
} gpemc_bank_timing_t;

typedef struct {
	struct {
		u32 Tcls;
		u32 Tclh;

		u32 Tals;
		u32 Talh;

		u32 Tcs;
		u32 Tch;

		u32 Tds;
		u32 Tdh;

		u32 Twp;
		u32 Twh;
		u32 Twc;

		struct {
			/*
			 * address to data loading delay
			 */
			u32 Tadl;
		} busy_wait_timing;
	} dc_timing;

	struct {
		struct {
			/*
			 * Command Write cycle to Address Write
			 * cycle Time for Random data input
			 */
			u32 Tcwaw;

			/*
			 * #WE high to Busy
			 */
			u32 Twb;

			/*
			 * #WP High/Low to #WE Low
			 */
			u32 Tww;

			/*
			 * #RE High to #WE Low
			 */
			u32 Trhw;

			/*
			 * #WE High to #RE Low
			 */
			u32 Twhr;

			/*
			 * #WE High to #RE Low for Random data out
			 */
			u32 Twhr2;

			/*
			 * Device Resetting Time(Read/Program/Erase)
			 */
			u32 Trst;

			/*
			 * Cache Busy in Read Cache (following 31h and 3Fh)
			 */
			u32 Tdcbsyr;

			/*
			 * Dummy Busy Time for Intelligent Copy-Back Read
			 */
			u32 Tdcbsyr2;
		} busy_wait_timing;
	} ac_timing;

	bus_width_t BW;
} common_nand_timing_t;

typedef struct {
	/*
	 * TODO
	 */
} toggle_nand_timing_t;

typedef struct {
	/*
	 * TODO
	 */
} sram_timing_t;

typedef struct {
	int cs;
	bank_type_t bank_type;
	gpemc_bank_timing_t bank_timing;

	void __iomem *io_base;

	void __iomem *io_nand_dat;
	void __iomem *io_nand_addr;
	void __iomem *io_nand_cmd;
} gpemc_bank_t;

extern int gpemc_request_cs(gpemc_bank_t *bank, int cs);
extern void gpemc_release_cs(gpemc_bank_t *bank);

extern int gpemc_config_bank_timing(gpemc_bank_t *bank);
extern int gpemc_config_toggle_bank_timing(gpemc_bank_t *bank);

extern void gpemc_set_bank_as_common_nand(gpemc_bank_t *bank);
extern void gpemc_set_bank_as_toggle_nand(gpemc_bank_t *bank);
extern void gpemc_set_bank_as_sram(gpemc_bank_t *bank);
extern bank_type_t gpemc_get_bank_type(gpemc_bank_t *bank);

extern void gpemc_enable_nand_flash(gpemc_bank_t *bank, bool enable);

extern void gpemc_fill_timing_from_nand(gpemc_bank_t *bank, common_nand_timing_t *timing);
extern void gpemc_fill_timing_from_toggle(gpemc_bank_t *bank, toggle_nand_timing_t *timing);
extern void gpemc_fill_timing_from_sram(gpemc_bank_t *bank, sram_timing_t *timing);

#endif
