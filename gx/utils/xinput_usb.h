/*
 * xinput_usb.h - Xbox 360 (XInput) USB controllers on the Nintendo Wii
 *
 * Copyright (c) 2026 VrepliroidV
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Drives wired Xbox 360 controllers and the many 2.4 GHz receivers that
 * present themselves as one (VID:PID 045e:028e, interface FF/5D/01).
 *
 *  - Several controllers at once. The first one plugged in is port 0
 *    (player 1), the next one port 1, and so on.
 *  - Hot-plug: the device list is checked twice a second, so controllers can
 *    be plugged in or out while the emulator is running.
 *  - The start-up requests the Windows driver sends; some third-party
 *    receivers send nothing, or reset themselves, without them.
 *  - No blocking USB call after a device is opened. A receiver that never
 *    answers cannot freeze the emulator.
 *  - Guide is reported as XI_GUIDE and logged with a time stamp. Some
 *    receivers send it on their own (for Select + Start, or in bursts), so
 *    the emulators do not map it yet.
 *  - With XINPUT_USB_LOG defined, events are appended to that file
 *    (e.g. "sd:/xinput_usb.log").
 *
 * Call XInput_Update() once per frame, before reading the buttons.
 */

#ifndef XINPUT_USB_H
#define XINPUT_USB_H

#ifdef HW_RVL

#include <gctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Button bits. The low 16 bits are the Xbox 360 report's button word. */
#define XI_DPAD_UP       0x00000001
#define XI_DPAD_DOWN     0x00000002
#define XI_DPAD_LEFT     0x00000004
#define XI_DPAD_RIGHT    0x00000008
#define XI_START         0x00000010
#define XI_BACK          0x00000020
#define XI_LTHUMB        0x00000040
#define XI_RTHUMB        0x00000080
#define XI_LB            0x00000100
#define XI_RB            0x00000200
#define XI_GUIDE         0x00000400
#define XI_A             0x00001000
#define XI_B             0x00002000
#define XI_X             0x00004000
#define XI_Y             0x00008000

/* Analog inputs turned into digital bits (more than about half way). */
#define XI_LT            0x00010000
#define XI_RT            0x00020000
#define XI_LSTICK_UP     0x00100000
#define XI_LSTICK_DOWN   0x00200000
#define XI_LSTICK_LEFT   0x00400000
#define XI_LSTICK_RIGHT  0x00800000
#define XI_RSTICK_UP     0x01000000
#define XI_RSTICK_DOWN   0x02000000
#define XI_RSTICK_LEFT   0x04000000
#define XI_RSTICK_RIGHT  0x08000000

#define XI_ANY_UP    (XI_DPAD_UP    | XI_LSTICK_UP)
#define XI_ANY_DOWN  (XI_DPAD_DOWN  | XI_LSTICK_DOWN)
#define XI_ANY_LEFT  (XI_DPAD_LEFT  | XI_LSTICK_LEFT)
#define XI_ANY_RIGHT (XI_DPAD_RIGHT | XI_LSTICK_RIGHT)

#define XI_MAX_PORTS 4

/* Scan for controllers, start reads, handle removals. Once per frame. */
void XInput_Update(void);

/* Buttons held on a port (0-3), XI_* bits. 0 when nothing is on that port. */
u32 XInput_ButtonsHeld(int port);

/* Bit n is set when a controller is working on port n. */
u32 XInput_ConnectedMask(void);

/* Short text for a status line, under 40 characters.
   Examples: "P1 ok, P2 ok", "P1 wait", "none", "replug". */
const char *XInput_Status(void);

#ifdef __cplusplus
}
#endif

#endif /* HW_RVL */
#endif /* XINPUT_USB_H */
