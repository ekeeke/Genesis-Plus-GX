#ifndef _EXTRAINPUT_H_
#define _EXTRAINPUT_H_

/*
 * Controllers that libogc's WPAD / PAD calls do not give an app, folded onto the Wii Remote's
 * WPAD_BUTTON_* bits (and, for the GameCube adapter, onto PAD_*) so an app that already polls
 * those can use them without new input code:
 *   - USB mouse, pen tablet (Wacom, Huion, XP-Pen, ...) and THQ uDraw GameTablet, as a pointer
 *     (left click / pen tip = A, right click / lower switch or rocker = B, middle / upper = 2)
 *   - DualShock 3 / Sixaxis over USB (libsicksaxis, HAVE_SICKSAXIS): Cross A, Circle B,
 *     Triangle 2, Square 1, Start / R1 / R2 +, Select / L1 / L2 -, PS HOME, D-pad
 *   - Wii U Pro Controller (libwupc, HAVE_WUPC) and Wii U GamePad in vWii (libwiidrc, HAVE_WIIDRC)
 *     on the same layout (X = 2, Y = 1)
 *   - the GameCube controller adapter for Wii U: its four ports as GameCube pads
 * The left stick of the pad controllers also presses the D-pad bits.
 *
 * extrainput_compat.h wraps WPAD_* and PAD_* so that including it (the Makefiles pass it with
 * -include) is all an app needs.
 */

#include <gccore.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screen size the pointer lives in (the same size the app gives WPAD_SetVRes)
void ExtraInput_Init(int width, int height);
void ExtraInput_SetScreen(int width, int height);
void ExtraInput_Shutdown(void);

// Once per frame, after the app's own WPAD_ScanPads() / PAD_ScanPads()
void ExtraInput_Update(void);

// WPAD_BUTTON_* bits for Wii Remote channel 0-3: held now, pressed / released since the last update
u32 ExtraInput_WpadHeld(int chan);
u32 ExtraInput_WpadDown(int chan);
u32 ExtraInput_WpadUp(int chan);

// A mouse, tablet or uDraw used in the last few seconds: pointer in screen pixels
bool ExtraInput_Pointer(float *x, float *y);

// GameCube adapter port 0-3 as a GameCube pad (PAD_BUTTON_* bits, PAD_StickX-style values); false if nothing is plugged in
bool ExtraInput_GcPad(int port, u16 *held, u16 *down, u16 *up, s8 *stickX, s8 *stickY, s8 *cstickX, s8 *cstickY, u8 *triggerL, u8 *triggerR);

#ifdef __cplusplus
}
#endif

#endif
