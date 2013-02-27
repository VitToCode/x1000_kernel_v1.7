/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  HW ECC-BCH support functions
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


#ifndef __SOC_BCH_H__
#define __SOC_BCH_H__

typedef enum {
	bch_req_decode = 0,
	bch_req_encode,
	bch_req_correct
} request_type_t;

struct bch_request;
typedef int (*bch_complete_t)(struct bch_request *);

struct bch_request {
	/* public members */

	request_type_t type;     /* (in) decode or encode */
	int ecc_level;           /* (in) must 4 * n (n = 1, 2, 3 ... 16) */

	void *raw_data;          /* (in) must word aligned */
	u32 blksz;               /* (in) must 4 * n (n = 1, 2, 3 ... 475) */

	void *ecc_data;          /* (in) must word aligned */

	u32 errrept_word_cnt;
	void *errrept_data;      /* (in) must word aligned */

	struct device *dev;      /* (in) the device that request belong to */

	bch_complete_t complete; /* (in) called when request done */

	/* private members */
	struct list_head node;
};

typedef struct bch_request bch_request_t;

extern int bch_request_submit(bch_request_t *req);

#define MAX_ERRREPT_DATA_SIZE	(64 * sizeof(u32))
#define MAX_ECC_DATA_SIZE	(64 * 14 / 8)

#endif
