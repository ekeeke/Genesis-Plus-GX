/**
 * Nintendo GameCube ARAM Wrapper for libOGC aram.c
 *
 * This is an often overlooked area of ~16Mb extra RAM
 * It's use in Genesis Plus is to shadow the ROM.
 * Actually, only SSF2TNC needs shadowing, but it's always
 * Good to know :)
 */
#include "shared.h"

#define ARAMSTART 0x8000
#define ARAM_READ        1
#define ARAM_WRITE       0

/**
 * StartARAM
 * This simply sets up the call to internal libOGC.
 * Passing NULL for array list, and 0 items to allocate.
 * Required so libOGC knows to handle any interrupts etc.
 */
void StartARAM ()
{
  AR_Init (NULL, 0);
}

/**
 * ARAMPut
 *
 * Move data from MAIN memory to ARAM
 */
void ARAMPut (char *src, char *dst, int len)
{
  DCFlushRange (src, len);
  AR_StartDMA( ARAM_WRITE, (u32)src, (u32)dst, len);
  while (AR_GetDMAStatus());
}

/**
 * ARAMFetch
 *
 * This function will move data from ARAM to MAIN memory
 */
void
ARAMFetch (char *dst, char *src, int len)
{
  DCInvalidateRange(dst, len);
  AR_StartDMA( ARAM_READ, (u32) dst, (u32) src, len);
  while (AR_GetDMAStatus());
}
