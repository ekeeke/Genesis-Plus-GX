#ifdef HW_RVL
#include <gccore.h>
#ifndef EXTRAINPUT_NO_MOUSE
#include <ogc/usbmouse.h>
#endif
#include "usbpointer.h"
#include "input_tablet.h"

#define MOUSE_SPEED     1	// screen pixels per mouse count
#define ACTIVE_FRAMES   180	// ~3 s at 60 fps

static bool running = false;
static int screenW = 640, screenH = 480;
static float posX = 320, posY = 240;
static u8 mouseHeld = 0, tabletHeld = 0;
static u8 held = 0, down = 0, up = 0;
static int idleFrames = ACTIVE_FRAMES;
static bool connected = false;

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static void setPosition(float x, float y)
{
	posX = clampf(x, 0, screenW - 1);
	posY = clampf(y, 0, screenH - 1);
}

void UsbPointer_Init(int width, int height)
{
	if (running)
	{
		return;
	}
	screenW = width > 0 ? width : 640;
	screenH = height > 0 ? height : 480;
	setPosition(screenW / 2, screenH / 2);
	mouseHeld = tabletHeld = held = down = up = 0;
	idleFrames = ACTIVE_FRAMES;

	// Neither is fatal when nothing is plugged in yet: both pick a device up when it appears
#ifndef EXTRAINPUT_NO_MOUSE
	MOUSE_Init();
#endif
	TB_Init();
	running = true;
}

void UsbPointer_SetScreen(int width, int height)
{
	if (width > 0 && height > 0)
	{
		screenW = width;
		screenH = height;
		setPosition(posX, posY);
	}
}

void UsbPointer_Shutdown(void)
{
	if (!running)
	{
		return;
	}
	running = false;
	TB_Shutdown();
#ifndef EXTRAINPUT_NO_MOUSE
	MOUSE_Deinit();
#endif
	held = down = up = mouseHeld = tabletHeld = 0;
	connected = false;
}

void UsbPointer_Update(void)
{
	u8 prev = held;
	bool moved = false;

	down = up = 0;
	if (!running)
	{
		return;
	}

#ifndef EXTRAINPUT_NO_MOUSE
	bool mouseOn = MOUSE_IsConnected();
#else
	bool mouseOn = false; // the program reads the USB mouse itself
#endif
	bool tabletOn = TB_Connected();
	connected = mouseOn || tabletOn;

	if (mouseOn)
	{
#ifndef EXTRAINPUT_NO_MOUSE
		mouse_event ev;
		int guard = 0, dx = 0, dy = 0;
		while (guard++ < 64 && MOUSE_GetEvent(&ev) > 0)
		{
			dx += ev.rx;
			dy += ev.ry;
			mouseHeld = ev.button & 7;
		}
		if (dx || dy)
		{
			setPosition(posX + dx * MOUSE_SPEED, posY + dy * MOUSE_SPEED);
			moved = true;
		}
#endif
	}
	else
	{
		mouseHeld = 0;
	}

	if (tabletOn)
	{
		tb_sample s;
		while (TB_Pop(&s))
		{
			tabletHeld = 0;
			if (s.buttons & TB_BUTTON_TIP) tabletHeld |= USBPOINTER_LEFT;
			if (s.buttons & TB_BUTTON_BARREL) tabletHeld |= USBPOINTER_RIGHT;
			if (s.buttons & TB_BUTTON_BARREL2) tabletHeld |= USBPOINTER_MIDDLE;
			if (!s.in_range)
			{
				tabletHeld = 0;
				continue;
			}
			int x, y;
			TB_ToScreen(&s, screenW, screenH, &x, &y);
			setPosition(x, y);
			moved = true;
		}
	}
	else
	{
		tabletHeld = 0;
	}

	held = mouseHeld | tabletHeld;
	down = held & ~prev;
	up = prev & ~held;

	if (moved || down)
	{
		idleFrames = 0;
	}
	else if (idleFrames < ACTIVE_FRAMES)
	{
		idleFrames++;
	}
}

bool UsbPointer_Connected(void)
{
	return running && connected;
}

bool UsbPointer_Active(void)
{
	return running && connected && (idleFrames < ACTIVE_FRAMES || held != 0);
}

void UsbPointer_Position(float *x, float *y)
{
	if (x) *x = posX;
	if (y) *y = posY;
}

u8 UsbPointer_ButtonsHeld(void) { return held; }
u8 UsbPointer_ButtonsDown(void) { return down; }
u8 UsbPointer_ButtonsUp(void)   { return up; }

#endif
