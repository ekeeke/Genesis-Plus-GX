/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef _WIIDRC_STRUCTS_H_
#define _WIIDRC_STRUCTS_H_

struct WiiDRCStat {
	s16 xAxisLmid;
	s16 xAxisRmid;
	s16 yAxisLmid;
	s16 yAxisRmid;
};

struct WiiDRCButtons {
	u32 up;
	u32 down;
	u32 state;
};

#endif
