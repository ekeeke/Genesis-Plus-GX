/*
 * usbpad_shim.h - XInput USB controllers seen as GameCube controllers
 *
 * Copyright (c) 2026 VrepliroidV
 * Released under the zlib license, see xinput_usb.h.
 *
 * Including this header after the libogc headers makes PAD_ScanPads(),
 * PAD_ButtonsHeld(), PAD_ButtonsDown() and PAD_ButtonsUp() also report
 * Xbox 360 / XInput USB controllers (xinput_usb.c). USB controller N is
 * merged into GameCube port N, so every menu, the input setup screen and
 * the per-port key mapping work with it unchanged.
 *
 * Default Genesis layout with the GameCube key mapping of Genesis Plus GX:
 *   Xbox A -> A, B -> B, LT/RT/LB -> C, X -> X, Y -> Y, RB -> Z,
 *   Back -> Mode, Start -> Start, D-pad or left stick -> D-pad.
 *   Back + Right opens the menu.
 */

#ifndef USBPAD_SHIM_H
#define USBPAD_SHIM_H

#ifdef HW_RVL

#include <gccore.h>

#ifdef __cplusplus
extern "C" {
#endif

u32 USBPAD_ScanPads(void);
u16 USBPAD_ButtonsHeld(int chan);
u16 USBPAD_ButtonsDown(int chan);
u16 USBPAD_ButtonsUp(int chan);

#ifdef __cplusplus
}
#endif

#ifndef USBPAD_SHIM_IMPL
#define PAD_ScanPads    USBPAD_ScanPads
#define PAD_ButtonsHeld USBPAD_ButtonsHeld
#define PAD_ButtonsDown USBPAD_ButtonsDown
#define PAD_ButtonsUp   USBPAD_ButtonsUp
#endif

#endif /* HW_RVL */
#endif /* USBPAD_SHIM_H */
