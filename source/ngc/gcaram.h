/**
 * Nintendo GameCube ARAM Wrapper for libOGC aram.c
 *
 * This is an often overlooked area of ~16Mb extra RAM
 * It's use in Genesis Plus is to shadow the ROM.
 * Actually, only SSF2TNC needs shadowing, but it's always
 * Good to know :)
 */

#ifndef _ARAM_H
#define _ARAM_H

extern void StartARAM ();
extern void ARAMFetch (char *src, char *dst, int len);
extern void ARAMPut (char *src, char *dst, int len);

#endif
