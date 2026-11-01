#ifdef HW_RVL
#include <math.h>
#include <wiiuse/wpad.h>
#include "extrainput.h"
#include "usbpointer.h"
#include "ds3.h"
#include "input_wii.h"
#include "libinput_internal.h"
#ifdef HAVE_WUPC
#include <wupc/wupc.h>
#endif
#ifdef HAVE_WIIDRC
#include <wiidrc/wiidrc.h>
#endif

#define CHANNELS 4
#define GC_PORTS 4
#define STICK_DPAD 0.5f // the left stick of a pad controller presses the D-pad bits past this

static bool running = false;
static u32 wHeld[CHANNELS], wDown[CHANNELS], wUp[CHANNELS];
static struct {
	bool present;
	u16 held, down, up;
	s8 sx, sy, csx, csy;
	u8 tl, tr;
} gc[GC_PORTS];

static u32 mapDs3(u32 b)
{
	u32 out = 0;
	if (b & (DS3_BUTTON_START | DS3_BUTTON_R1 | DS3_BUTTON_R2))  out |= WPAD_BUTTON_PLUS;
	if (b & (DS3_BUTTON_SELECT | DS3_BUTTON_L1 | DS3_BUTTON_L2)) out |= WPAD_BUTTON_MINUS;
	if (b & DS3_BUTTON_CROSS)    out |= WPAD_BUTTON_A;
	if (b & DS3_BUTTON_CIRCLE)   out |= WPAD_BUTTON_B;
	if (b & DS3_BUTTON_TRIANGLE) out |= WPAD_BUTTON_2;
	if (b & DS3_BUTTON_SQUARE)   out |= WPAD_BUTTON_1;
	if (b & DS3_BUTTON_PS)       out |= WPAD_BUTTON_HOME;
	if (b & DS3_BUTTON_UP)       out |= WPAD_BUTTON_UP;
	if (b & DS3_BUTTON_DOWN)     out |= WPAD_BUTTON_DOWN;
	if (b & DS3_BUTTON_LEFT)     out |= WPAD_BUTTON_LEFT;
	if (b & DS3_BUTTON_RIGHT)    out |= WPAD_BUTTON_RIGHT;
	return out;
}

static u32 mapPointer(u32 b)
{
	u32 out = 0;
	if (b & USBPOINTER_LEFT)   out |= WPAD_BUTTON_A;
	if (b & USBPOINTER_RIGHT)  out |= WPAD_BUTTON_B;
	if (b & USBPOINTER_MIDDLE) out |= WPAD_BUTTON_2;
	return out;
}

#if defined(HAVE_WUPC) || defined(HAVE_WIIDRC)
static u32 mapClassic(u32 b)
{
	u32 out = 0;
	if (b & (WPAD_CLASSIC_BUTTON_ZR | WPAD_CLASSIC_BUTTON_FULL_R | WPAD_CLASSIC_BUTTON_PLUS))
		out |= WPAD_BUTTON_PLUS;
	if (b & (WPAD_CLASSIC_BUTTON_ZL | WPAD_CLASSIC_BUTTON_FULL_L | WPAD_CLASSIC_BUTTON_MINUS))
		out |= WPAD_BUTTON_MINUS;
	if (b & WPAD_CLASSIC_BUTTON_A)     out |= WPAD_BUTTON_A;
	if (b & WPAD_CLASSIC_BUTTON_B)     out |= WPAD_BUTTON_B;
	if (b & WPAD_CLASSIC_BUTTON_X)     out |= WPAD_BUTTON_2;
	if (b & WPAD_CLASSIC_BUTTON_Y)     out |= WPAD_BUTTON_1;
	if (b & WPAD_CLASSIC_BUTTON_HOME)  out |= WPAD_BUTTON_HOME;
	if (b & WPAD_CLASSIC_BUTTON_UP)    out |= WPAD_BUTTON_UP;
	if (b & WPAD_CLASSIC_BUTTON_DOWN)  out |= WPAD_BUTTON_DOWN;
	if (b & WPAD_CLASSIC_BUTTON_LEFT)  out |= WPAD_BUTTON_LEFT;
	if (b & WPAD_CLASSIC_BUTTON_RIGHT) out |= WPAD_BUTTON_RIGHT;
	return out;
}
#endif

#ifdef HAVE_WIIDRC
static u32 mapDrc(u32 b)
{
	u32 out = 0;
	if (b & (WIIDRC_BUTTON_R | WIIDRC_BUTTON_ZR | WIIDRC_BUTTON_PLUS))  out |= WPAD_BUTTON_PLUS;
	if (b & (WIIDRC_BUTTON_L | WIIDRC_BUTTON_ZL | WIIDRC_BUTTON_MINUS)) out |= WPAD_BUTTON_MINUS;
	if (b & WIIDRC_BUTTON_A)     out |= WPAD_BUTTON_A;
	if (b & WIIDRC_BUTTON_B)     out |= WPAD_BUTTON_B;
	if (b & WIIDRC_BUTTON_X)     out |= WPAD_BUTTON_2;
	if (b & WIIDRC_BUTTON_Y)     out |= WPAD_BUTTON_1;
	if (b & WIIDRC_BUTTON_HOME)  out |= WPAD_BUTTON_HOME;
	if (b & WIIDRC_BUTTON_UP)    out |= WPAD_BUTTON_UP;
	if (b & WIIDRC_BUTTON_DOWN)  out |= WPAD_BUTTON_DOWN;
	if (b & WIIDRC_BUTTON_LEFT)  out |= WPAD_BUTTON_LEFT;
	if (b & WIIDRC_BUTTON_RIGHT) out |= WPAD_BUTTON_RIGHT;
	return out;
}
#endif

// The left stick, -1..1 with y up, as D-pad bits
static u32 stickToDpad(float x, float y)
{
	u32 out = 0;
	if (y > STICK_DPAD)  out |= WPAD_BUTTON_UP;
	if (y < -STICK_DPAD) out |= WPAD_BUTTON_DOWN;
	if (x < -STICK_DPAD) out |= WPAD_BUTTON_LEFT;
	if (x > STICK_DPAD)  out |= WPAD_BUTTON_RIGHT;
	return out;
}

void ExtraInput_Init(int width, int height)
{
	if (running)
	{
		return;
	}
	Ds3_Init();
	inpw_gcadapter_init(true);
#ifdef HAVE_WUPC
	WUPC_Init();
#endif
#ifdef HAVE_WIIDRC
	WiiDRC_Init();
#endif
	UsbPointer_Init(width, height);
	running = true;
}

void ExtraInput_SetScreen(int width, int height)
{
	UsbPointer_SetScreen(width, height);
}

void ExtraInput_Shutdown(void)
{
	if (!running)
	{
		return;
	}
	running = false;
	UsbPointer_Shutdown();
	inpw_gcadapter_shutdown();
	Ds3_Shutdown();
#ifdef HAVE_WUPC
	WUPC_Shutdown();
#endif
	for (int i = 0; i < CHANNELS; i++)
	{
		wHeld[i] = wDown[i] = wUp[i] = 0;
	}
	for (int i = 0; i < GC_PORTS; i++)
	{
		gc[i].present = false;
	}
}

void ExtraInput_Update(void)
{
	u32 now[CHANNELS] = { 0, 0, 0, 0 };

	if (!running)
	{
		return;
	}

	UsbPointer_Update();
	Ds3_Update();
	inpw_gcadapter_update();

	now[0] |= mapPointer(UsbPointer_ButtonsHeld());

	if (Ds3_Connected())
	{
		float lx, ly;
		Ds3_Sticks(&lx, &ly, NULL, NULL);
		now[0] |= mapDs3(Ds3_ButtonsHeld()) | stickToDpad(lx, ly);
	}

#ifdef HAVE_WUPC
	WUPC_UpdateButtonStats();
	for (int ch = 0; ch < CHANNELS; ch++)
	{
		if (!WUPC_Data(ch))
		{
			continue;
		}
		now[ch] |= mapClassic(WUPC_ButtonsHeld(ch)) |
			stickToDpad(WUPC_lStickX(ch) / 900.0f, WUPC_lStickY(ch) / 900.0f);
	}
#endif

#ifdef HAVE_WIIDRC
	if (WiiDRC_Inited() && WiiDRC_Connected() && WiiDRC_ScanPads())
	{
		now[0] |= mapDrc(WiiDRC_ButtonsHeld()) |
			stickToDpad(WiiDRC_lStickX() / 900.0f, WiiDRC_lStickY() / 900.0f);
	}
#endif

	for (int i = 0; i < CHANNELS; i++)
	{
		wDown[i] = now[i] & ~wHeld[i];
		wUp[i] = wHeld[i] & ~now[i];
		wHeld[i] = now[i];
	}

	for (int port = 0; port < GC_PORTS; port++)
	{
		input_gc_pad p;
		if (!INPUT_GCAdapterPad(port, &p))
		{
			gc[port].present = false;
			gc[port].held = gc[port].down = gc[port].up = 0;
			continue;
		}
		gc[port].present = true;
		gc[port].down = p.buttons & ~gc[port].held;
		gc[port].up = gc[port].held & ~p.buttons;
		gc[port].held = p.buttons;
		gc[port].sx = p.stick_x;
		gc[port].sy = p.stick_y;
		gc[port].csx = p.substick_x;
		gc[port].csy = p.substick_y;
		gc[port].tl = p.trigger_l;
		gc[port].tr = p.trigger_r;
	}
}

u32 ExtraInput_WpadHeld(int chan) { return (chan >= 0 && chan < CHANNELS) ? wHeld[chan] : 0; }
u32 ExtraInput_WpadDown(int chan) { return (chan >= 0 && chan < CHANNELS) ? wDown[chan] : 0; }
u32 ExtraInput_WpadUp(int chan)   { return (chan >= 0 && chan < CHANNELS) ? wUp[chan] : 0; }

bool ExtraInput_Pointer(float *x, float *y)
{
	if (!running || !UsbPointer_Active())
	{
		return false;
	}
	UsbPointer_Position(x, y);
	return true;
}

bool ExtraInput_GcPad(int port, u16 *held, u16 *down, u16 *up, s8 *stickX, s8 *stickY, s8 *cstickX, s8 *cstickY, u8 *triggerL, u8 *triggerR)
{
	if (port < 0 || port >= GC_PORTS || !gc[port].present)
	{
		return false;
	}
	if (held) *held = gc[port].held;
	if (down) *down = gc[port].down;
	if (up) *up = gc[port].up;
	if (stickX) *stickX = gc[port].sx;
	if (stickY) *stickY = gc[port].sy;
	if (cstickX) *cstickX = gc[port].csx;
	if (cstickY) *cstickY = gc[port].csy;
	if (triggerL) *triggerL = gc[port].tl;
	if (triggerR) *triggerR = gc[port].tr;
	return true;
}

#endif
