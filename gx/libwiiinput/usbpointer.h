#ifndef _USBPOINTER_H_
#define _USBPOINTER_H_

/*
 * USB mouse and pen tablet as a pointer, for the Wii.
 *
 * The mouse goes through libogc's USB mouse driver; pen tablets (Wacom, Huion,
 * XP-Pen, Gaomon, ...) through the driver from libwiiinput (libwiiinput/). Both
 * feed one pointer in screen coordinates plus three buttons. The tablet reports
 * absolute positions and the mouse relative ones; whichever moved last wins, and
 * the other carries on from where it left off.
 *
 * The THQ uDraw GameTablet on a Wii Remote comes through the same calls (lower rocker is right,
 * upper rocker is middle).
 *
 * Needs -DHAVE_TABLET and -Wl,--wrap=classic_ctrl_handshake,--wrap=parse_event,--wrap=USB_GetDeviceList
 * (the last hides tablets from libogc's mouse driver, as a tablet also presents itself as a mouse).
 */

#ifdef HW_RVL

#include <gccore.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USBPOINTER_LEFT   (1 << 0)	// mouse left, pen tip
#define USBPOINTER_RIGHT  (1 << 1)	// mouse right, lower pen switch
#define USBPOINTER_MIDDLE (1 << 2)	// mouse middle, upper pen switch

// Screen size the pointer lives in.
void UsbPointer_Init(int width, int height);
void UsbPointer_Shutdown(void);

// The screen size changed (the app's WPAD_SetVRes)
void UsbPointer_SetScreen(int width, int height);

// Once per input update.
void UsbPointer_Update(void);

// A mouse or tablet is plugged in and answering.
bool UsbPointer_Connected(void);

// It moved or was clicked in the last few seconds, so it should steer the cursor instead of an idle Wii Remote.
bool UsbPointer_Active(void);

void UsbPointer_Position(float *x, float *y);

// USBPOINTER_* bits: held now, and pressed / released since the last update.
u8 UsbPointer_ButtonsHeld(void);
u8 UsbPointer_ButtonsDown(void);
u8 UsbPointer_ButtonsUp(void);

#ifdef __cplusplus
}
#endif

#endif // HW_RVL
#endif
