#ifdef HW_RVL
#include <gccore.h>
#include "ds3.h"

#ifdef HAVE_SICKSAXIS
#include <sicksaxis.h>

// Opening walks the USB device list through IOS: retry about every two seconds, not every frame
#define RETRY_FRAMES 120

static struct ss_device dev;
static bool ready = false;
static bool connected = false;
static int retry = 0;
static u32 held = 0, down = 0, up = 0;
static float stick[4];

void Ds3_Init(void)
{
	if (ready)
	{
		return;
	}
	if (ss_init() >= 0 && ss_initialize(&dev) >= 0)
	{
		ready = true;
		if (ss_open(&dev) > 0)
		{
			ss_start_reading(&dev);
		}
	}
}

void Ds3_Shutdown(void)
{
	if (!ready)
	{
		return;
	}
	if (ss_is_connected(&dev))
	{
		ss_stop_reading(&dev);
	}
	ss_close(&dev);
	ready = connected = false;
	held = down = up = 0;
}

void Ds3_Update(void)
{
	u32 prev = held;

	down = up = 0;
	if (!ready)
	{
		return;
	}

	if (!ss_is_connected(&dev))
	{
		connected = false;
		held = 0;
		up = prev;
		if (++retry < RETRY_FRAMES)
		{
			return;
		}
		retry = 0;
		if (ss_open(&dev) <= 0)
		{
			return;
		}
		ss_start_reading(&dev);
	}
	connected = true;

	const struct SS_BUTTONS *b = &dev.pad.buttons;
	u32 now = 0;
	if (b->up)       now |= DS3_BUTTON_UP;
	if (b->down)     now |= DS3_BUTTON_DOWN;
	if (b->left)     now |= DS3_BUTTON_LEFT;
	if (b->right)    now |= DS3_BUTTON_RIGHT;
	if (b->cross)    now |= DS3_BUTTON_CROSS;
	if (b->circle)   now |= DS3_BUTTON_CIRCLE;
	if (b->square)   now |= DS3_BUTTON_SQUARE;
	if (b->triangle) now |= DS3_BUTTON_TRIANGLE;
	if (b->L1)       now |= DS3_BUTTON_L1;
	if (b->R1)       now |= DS3_BUTTON_R1;
	if (b->L2)       now |= DS3_BUTTON_L2;
	if (b->R2)       now |= DS3_BUTTON_R2;
	if (b->L3)       now |= DS3_BUTTON_L3;
	if (b->R3)       now |= DS3_BUTTON_R3;
	if (b->start)    now |= DS3_BUTTON_START;
	if (b->select)   now |= DS3_BUTTON_SELECT;
	if (b->PS)       now |= DS3_BUTTON_PS;

	held = now;
	down = now & ~prev;
	up = prev & ~now;

	// 0..255 with 128 centred and y growing downward
	stick[0] = (dev.pad.left_analog.x - 128) / 128.0f;
	stick[1] = -(dev.pad.left_analog.y - 128) / 128.0f;
	stick[2] = (dev.pad.right_analog.x - 128) / 128.0f;
	stick[3] = -(dev.pad.right_analog.y - 128) / 128.0f;
}

bool Ds3_Connected(void)  { return ready && connected; }
u32 Ds3_ButtonsHeld(void) { return held; }
u32 Ds3_ButtonsDown(void) { return down; }
u32 Ds3_ButtonsUp(void)   { return up; }

void Ds3_Sticks(float *lx, float *ly, float *rx, float *ry)
{
	if (lx) *lx = stick[0];
	if (ly) *ly = stick[1];
	if (rx) *rx = stick[2];
	if (ry) *ry = stick[3];
}

#else // no libsicksaxis

void Ds3_Init(void) {}
void Ds3_Shutdown(void) {}
void Ds3_Update(void) {}
bool Ds3_Connected(void)  { return false; }
u32 Ds3_ButtonsHeld(void) { return 0; }
u32 Ds3_ButtonsDown(void) { return 0; }
u32 Ds3_ButtonsUp(void)   { return 0; }
void Ds3_Sticks(float *lx, float *ly, float *rx, float *ry)
{
	if (lx) *lx = 0;
	if (ly) *ly = 0;
	if (rx) *rx = 0;
	if (ry) *ry = 0;
}

#endif
#endif
