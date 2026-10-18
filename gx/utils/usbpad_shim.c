/*
 * usbpad_shim.c - XInput USB controllers seen as GameCube controllers
 *
 * Copyright (c) 2026 VrepliroidV
 * Released under the zlib license, see xinput_usb.h.
 */

#ifdef HW_RVL

#define USBPAD_SHIM_IMPL
#include "usbpad_shim.h"
#include "xinput_usb.h"

static u16 usb_held[XI_MAX_PORTS];
static u16 usb_down[XI_MAX_PORTS];
static u16 usb_up[XI_MAX_PORTS];

/* Picks GameCube buttons so the default Genesis Plus GX mapping
   (B->A, A->B, X->C, L->X, Y->Y, R->Z, Z->Mode) lands on the labels of a
   6-button pad in XInput mode. The triggers and LB all give C, so C works
   whichever of them a pad uses for it. */
static u16 to_gamecube(u32 x)
{
	u16 p = 0;
	if (x & XI_ANY_UP)    p |= PAD_BUTTON_UP;
	if (x & XI_ANY_DOWN)  p |= PAD_BUTTON_DOWN;
	if (x & XI_ANY_LEFT)  p |= PAD_BUTTON_LEFT;
	if (x & XI_ANY_RIGHT) p |= PAD_BUTTON_RIGHT;
	if (x & XI_A) p |= PAD_BUTTON_B;               /* A */
	if (x & XI_B) p |= PAD_BUTTON_A;               /* B */
	if (x & (XI_LT | XI_RT | XI_LB)) p |= PAD_BUTTON_X; /* C */
	if (x & XI_X) p |= PAD_TRIGGER_L;              /* X */
	if (x & XI_Y) p |= PAD_BUTTON_Y;               /* Y */
	if (x & XI_RB) p |= PAD_TRIGGER_R;             /* Z */
	if (x & XI_BACK) p |= PAD_TRIGGER_Z;           /* Mode */
	if (x & XI_START) p |= PAD_BUTTON_START;
	return p;
}

u32 USBPAD_ScanPads(void)
{
	u32 connected = PAD_ScanPads();

	XInput_Update();
	for (int i = 0; i < XI_MAX_PORTS; i++)
	{
		u16 now = to_gamecube(XInput_ButtonsHeld(i));
		usb_down[i] = now & ~usb_held[i];
		usb_up[i] = usb_held[i] & ~now;
		usb_held[i] = now;
	}

	return connected | XInput_ConnectedMask();
}

u16 USBPAD_ButtonsHeld(int chan)
{
	u16 b = PAD_ButtonsHeld(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= usb_held[chan];
	return b;
}

u16 USBPAD_ButtonsDown(int chan)
{
	u16 b = PAD_ButtonsDown(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= usb_down[chan];
	return b;
}

u16 USBPAD_ButtonsUp(int chan)
{
	u16 b = PAD_ButtonsUp(chan);
	if (chan >= 0 && chan < XI_MAX_PORTS)
		b |= usb_up[chan];
	return b;
}

#endif /* HW_RVL */
