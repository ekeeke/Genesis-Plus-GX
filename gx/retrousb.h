#ifndef _RETROUSB_H_
#define _RETROUSB_H_

#include <gctypes.h>

bool RetroUSB_ScanPads();
u32 RetroUSB_ButtonsHeld();
bool RetroUSB_OK();
char* RetroUSB_TestChars();

extern u8 RetroUSB_Counter;

#endif