/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include "wiidrc_structs.h"
#include "wiidrc/wiidrc.h"

static struct WiiDRCStat __WiiDRC_Status;
static struct WiiDRCData __WiiDRC_PadData;
static struct WiiDRCButtons __WiiDRC_PadButtons;
static bool __WiiDRC_ShutdownRequested;

static u32 __WiiDRC_Inited = 0;
static u8 *__WiiDRC_I2CBuf = NULL;
static u8 *__WiiDRC_DRCStateBuf = NULL;

static bool __WiiDRC_SetI2CBuf()
{
	DCInvalidateRange((void*)0x938B2964, 4);
	if(*(vu32*)0x938B2964 == 0x138BB004) //r569
	{
		__WiiDRC_I2CBuf = (u8*)0x938BB004;
		return true;
	}
	DCInvalidateRange((void*)0x938B2564, 4);
	if(*(vu32*)0x938B2564 == 0x138BB004) //r570
	{
		__WiiDRC_I2CBuf = (u8*)0x938BB004;
		return true;
	}
	else if(*(vu32*)0x938B2564 == 0x138BA004) //r590
	{
		__WiiDRC_I2CBuf = (u8*)0x938BA004;
		return true;
	}
	return false;
}

static bool __WiiDRC_SetDRCStateBuf()
{
	//TODO r569
	DCInvalidateRange((void*)0x938B563C, 4);
	if(*(vu32*)0x938B563C == 0x138BE770) //r570
	{
		__WiiDRC_DRCStateBuf = (u8*)0x938BE770;
		return true;
	}
	DCInvalidateRange((void*)0x938B5724, 4);
	if(*(vu32*)0x938B5724 == 0x138BD770) //r590
	{
		__WiiDRC_DRCStateBuf = (u8*)0x938BD770;
		return true;
	}
	return false;
}

bool WiiDRC_Init()
{
	if(__WiiDRC_Inited == 1)
		return false;
	if(!__WiiDRC_SetI2CBuf())
		return false;
	//can fail on r569 for now
	__WiiDRC_SetDRCStateBuf();

	__WiiDRC_Inited = 1;

	WiiDRC_Recalibrate(); //sets up __WiiDRC_Status
	memset(&__WiiDRC_PadData,0,sizeof(struct WiiDRCData));
	memset(&__WiiDRC_PadButtons,0,sizeof(struct WiiDRCButtons));
	__WiiDRC_ShutdownRequested = false;

	return true;
}

bool WiiDRC_Inited()
{
	return !!__WiiDRC_Inited;
}

bool WiiDRC_Recalibrate()
{
	if(__WiiDRC_Inited == 0)
		return false;

	DCInvalidateRange(__WiiDRC_I2CBuf,9);
	__WiiDRC_Status.xAxisLmid = (s8)(__WiiDRC_I2CBuf[4]-0x80);
	__WiiDRC_Status.yAxisLmid = (s8)(__WiiDRC_I2CBuf[5]-0x80);
	__WiiDRC_Status.xAxisRmid = (s8)(__WiiDRC_I2CBuf[6]-0x80);
	__WiiDRC_Status.yAxisRmid = (s8)(__WiiDRC_I2CBuf[7]-0x80);

	return true;
}

bool WiiDRC_ScanPads()
{
	if(__WiiDRC_Inited == 0)
		return false;

	DCInvalidateRange(__WiiDRC_I2CBuf,9);
	__WiiDRC_ShutdownRequested = !!(__WiiDRC_I2CBuf[1]&0x80);
	__WiiDRC_PadData.button = (__WiiDRC_I2CBuf[2]<<8) | (__WiiDRC_I2CBuf[3]);
	__WiiDRC_PadData.xAxisL = ((s8)(__WiiDRC_I2CBuf[4]-0x80)) - __WiiDRC_Status.xAxisLmid;
	__WiiDRC_PadData.yAxisL = ((s8)(__WiiDRC_I2CBuf[5]-0x80)) - __WiiDRC_Status.yAxisLmid;
	__WiiDRC_PadData.xAxisR = ((s8)(__WiiDRC_I2CBuf[6]-0x80)) - __WiiDRC_Status.xAxisRmid;
	__WiiDRC_PadData.yAxisR = ((s8)(__WiiDRC_I2CBuf[7]-0x80)) - __WiiDRC_Status.yAxisRmid;
	__WiiDRC_PadData.extra = __WiiDRC_I2CBuf[8];
	__WiiDRC_PadData.battery = (__WiiDRC_PadData.extra>>1)&7;

	u16 newstate, oldstate;

	newstate = __WiiDRC_PadData.button;
	oldstate = __WiiDRC_PadButtons.state;
	__WiiDRC_PadButtons.state = newstate;
	__WiiDRC_PadButtons.up = oldstate & ~newstate;
	__WiiDRC_PadButtons.down = newstate & (newstate ^ oldstate);

	return true;
}

bool WiiDRC_Connected()
{
	if(__WiiDRC_DRCStateBuf)
	{
		DCInvalidateRange(__WiiDRC_DRCStateBuf, 4);
		return !!(*(vu32*)__WiiDRC_DRCStateBuf);
	}
	return true; //default connect
}

bool WiiDRC_ShutdownRequested()
{
	return __WiiDRC_ShutdownRequested;
}

const u8 *WiiDRC_GetRawI2CAddr()
{
	return __WiiDRC_I2CBuf;
}
const struct WiiDRCData *WiiDRC_Data()
{
	return &__WiiDRC_PadData;
}
u32 WiiDRC_ButtonsUp()
{
	return __WiiDRC_PadButtons.up;
}
u32 WiiDRC_ButtonsDown()
{
	return __WiiDRC_PadButtons.down;
}
u32 WiiDRC_ButtonsHeld()
{
	return __WiiDRC_PadButtons.state;
}
s16 WiiDRC_lStickX()
{
	return __WiiDRC_PadData.xAxisL;
}
s16 WiiDRC_lStickY()
{
	return __WiiDRC_PadData.yAxisL;
}
s16 WiiDRC_rStickX()
{
	return __WiiDRC_PadData.xAxisR;
}
s16 WiiDRC_rStickY()
{
	return __WiiDRC_PadData.yAxisR;
}
