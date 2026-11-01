#ifndef _DS3_H_
#define _DS3_H_

/*
 * Sony DualShock 3 / Sixaxis over USB, through libsicksaxis. Without HAVE_SICKSAXIS
 * (the library is not installed) every call does nothing and Ds3_Connected() is false.
 */

#ifdef HW_RVL

#include <gccore.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS3_BUTTON_UP       (1 << 0)
#define DS3_BUTTON_DOWN     (1 << 1)
#define DS3_BUTTON_LEFT     (1 << 2)
#define DS3_BUTTON_RIGHT    (1 << 3)
#define DS3_BUTTON_CROSS    (1 << 4)
#define DS3_BUTTON_CIRCLE   (1 << 5)
#define DS3_BUTTON_SQUARE   (1 << 6)
#define DS3_BUTTON_TRIANGLE (1 << 7)
#define DS3_BUTTON_L1       (1 << 8)
#define DS3_BUTTON_R1       (1 << 9)
#define DS3_BUTTON_L2       (1 << 10)
#define DS3_BUTTON_R2       (1 << 11)
#define DS3_BUTTON_L3       (1 << 12)
#define DS3_BUTTON_R3       (1 << 13)
#define DS3_BUTTON_START    (1 << 14)
#define DS3_BUTTON_SELECT   (1 << 15)
#define DS3_BUTTON_PS       (1 << 16)

void Ds3_Init(void);
void Ds3_Shutdown(void);

// Once per input update. Looks for a controller about every two seconds while none is open.
void Ds3_Update(void);

bool Ds3_Connected(void);

// DS3_BUTTON_* bits: held now, and pressed / released since the last update.
u32 Ds3_ButtonsHeld(void);
u32 Ds3_ButtonsDown(void);
u32 Ds3_ButtonsUp(void);

// Sticks, -1..1, y up, no dead zone.
void Ds3_Sticks(float *lx, float *ly, float *rx, float *ry);

#ifdef __cplusplus
}
#endif

#endif // HW_RVL
#endif
