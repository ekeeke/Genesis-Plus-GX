#ifndef _EXTRAINPUT_COMPAT_H_
#define _EXTRAINPUT_COMPAT_H_

/*
 * Passed to every file with -include (see the Makefile): makes the app's own WPAD_* and PAD_* calls
 * see the controllers in extrainput.h as well. Wii Remote channel 0's WPAD_ButtonsDown/Held/Up and
 * WPAD_Data() carry the extra devices' buttons, WPAD_IR() / WPAD_Data()->ir give a USB mouse, tablet or
 * uDraw as the pointer while it is in use, and PAD_* carry the GameCube adapter's ports. Nothing in
 * the app's own input code changes.
 */

#ifdef HW_RVL

// libwiiinput's uDraw driver writes wiiuse's wiimote_t, which is read-only unless this is defined before wpad.h is first included
#ifndef WIIUSE_INTERNAL_H_INCLUDED
#define WIIUSE_INTERNAL_H_INCLUDED
#endif


#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "extrainput.h"

static inline s32 ExtraInput_WPADInit(void)
{
	s32 r = WPAD_Init();
	ExtraInput_Init(640, 480);
	return r;
}

static inline s32 ExtraInput_WPADShutdown(void)
{
	ExtraInput_Shutdown();
	return WPAD_Shutdown();
}

static inline s32 ExtraInput_WPADSetVRes(int chan, u32 xres, u32 yres)
{
	ExtraInput_SetScreen((int)xres, (int)yres);
	return WPAD_SetVRes(chan, xres, yres);
}

static inline s32 ExtraInput_WPADScanPads(void)
{
	s32 r = WPAD_ScanPads();
	ExtraInput_Update();
	return r;
}

static inline void ExtraInput_WPADIR(int chan, struct ir_t *ir)
{
	float x, y;
	WPAD_IR(chan, ir);
	if (chan == 0 && ir && ExtraInput_Pointer(&x, &y))
	{
		ir->valid = 1;
		ir->x = x;
		ir->y = y;
		ir->angle = 0;
	}
}

static inline WPADData *ExtraInput_WPADData(int chan, WPADData *real)
{
	static WPADData patched[4];

	if (!real || chan < 0 || chan > 3)
	{
		return real;
	}
	patched[chan] = *real;
	patched[chan].btns_h |= ExtraInput_WpadHeld(chan);
	patched[chan].btns_d |= ExtraInput_WpadDown(chan);
	patched[chan].btns_u |= ExtraInput_WpadUp(chan);
	if (chan == 0)
	{
		float x, y;
		if (ExtraInput_Pointer(&x, &y))
		{
			patched[chan].ir.valid = 1;
			patched[chan].ir.x = x;
			patched[chan].ir.y = y;
			patched[chan].ir.angle = 0;
		}
	}
	return &patched[chan];
}

#define WPAD_Init()                  ExtraInput_WPADInit()
#define WPAD_Shutdown()              ExtraInput_WPADShutdown()
#define WPAD_SetVRes(c, w, h)        ExtraInput_WPADSetVRes((c), (w), (h))
#define WPAD_ScanPads()              ExtraInput_WPADScanPads()
#define WPAD_IR(c, ir)               ExtraInput_WPADIR((c), (ir))
#define WPAD_Data(c)                 ExtraInput_WPADData((c), WPAD_Data(c))
#define WPAD_ButtonsHeld(c)          (WPAD_ButtonsHeld(c) | ExtraInput_WpadHeld(c))
#define WPAD_ButtonsDown(c)          (WPAD_ButtonsDown(c) | ExtraInput_WpadDown(c))
#define WPAD_ButtonsUp(c)            (WPAD_ButtonsUp(c) | ExtraInput_WpadUp(c))

// The GameCube controller adapter for Wii U: a real pad on a port wins, otherwise the adapter's pad is that port's pad
static inline u32 ExtraInput_PADScanPads(void)
{
	u32 mask = PAD_ScanPads();
	for (int i = 0; i < 4; i++)
	{
		if (ExtraInput_GcPad(i, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
		{
			mask |= 1u << i;
		}
	}
	return mask;
}

static inline u16 ExtraInput_PADButtons(int port, u16 real, int which)
{
	u16 held, down, up;
	if (!ExtraInput_GcPad(port, &held, &down, &up, NULL, NULL, NULL, NULL, NULL, NULL))
	{
		return real;
	}
	return real | (which == 0 ? held : which == 1 ? down : up);
}

static inline s8 ExtraInput_PADAxis(int port, s8 real, int which)
{
	s8 v[4];
	if (!ExtraInput_GcPad(port, NULL, NULL, NULL, &v[0], &v[1], &v[2], &v[3], NULL, NULL))
	{
		return real;
	}
	return abs(v[which]) > abs(real) ? v[which] : real;
}

static inline u8 ExtraInput_PADTrigger(int port, u8 real, int right)
{
	u8 l, r;
	if (!ExtraInput_GcPad(port, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &l, &r))
	{
		return real;
	}
	u8 v = right ? r : l;
	return v > real ? v : real;
}

#define PAD_ScanPads()               ExtraInput_PADScanPads()
#define PAD_ButtonsHeld(c)           ExtraInput_PADButtons((c), PAD_ButtonsHeld(c), 0)
#define PAD_ButtonsDown(c)           ExtraInput_PADButtons((c), PAD_ButtonsDown(c), 1)
#define PAD_ButtonsUp(c)             ExtraInput_PADButtons((c), PAD_ButtonsUp(c), 2)
#define PAD_StickX(c)                ExtraInput_PADAxis((c), PAD_StickX(c), 0)
#define PAD_StickY(c)                ExtraInput_PADAxis((c), PAD_StickY(c), 1)
#define PAD_SubStickX(c)             ExtraInput_PADAxis((c), PAD_SubStickX(c), 2)
#define PAD_SubStickY(c)             ExtraInput_PADAxis((c), PAD_SubStickY(c), 3)
#define PAD_TriggerL(c)              ExtraInput_PADTrigger((c), PAD_TriggerL(c), 0)
#define PAD_TriggerR(c)              ExtraInput_PADTrigger((c), PAD_TriggerR(c), 1)

#endif
#endif
