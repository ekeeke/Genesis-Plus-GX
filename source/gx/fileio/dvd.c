/*
 * dvd.c
 * 
 *  Low-level DVD access
 *
 *  Softdev (2006)
 *  Eke-Eke (2007,2008,2009)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ********************************************************************************/

#include "shared.h"
#include "file_dvd.h"
#include "gui.h"

#ifndef HW_RVL
static u64 DvdMaxOffset = 0x57057C00;                 /* 1.4 GB max. by default */
static vu32* const dvd = (u32*)0xCC006000;            /* DVD I/O Address base */
static u8 *inquiry=(unsigned char *)0x80000004;       /* pointer to drive ID */
#else
static u64 DvdMaxOffset = 0x118244F00LL;              /* 4.7 GB max. */
#endif

static u8 DVDreadbuffer[2048] ATTRIBUTE_ALIGN (32);   /* data buffer for all DVD operations */


/***************************************************************************
 * dvd_read
 *
 * Read DVD disc sectors
 ***************************************************************************/
u32 dvd_read (void *dst, u32 len, u64 offset)
{
  /*** We only allow 2k reads **/
  if (len > DVDCHUNK) return 0;

  /*** Let's not read past end of DVD ***/
  if(offset < DvdMaxOffset)
  {
    unsigned char *buffer = (unsigned char *) (unsigned int) DVDreadbuffer;
    DCInvalidateRange((void *)buffer, len);

#ifndef HW_RVL
    dvd[0] = 0x2E;
    dvd[1] = 0;
    dvd[2] = 0xA8000000;
    dvd[3] = (u32)(offset >> 2);
    dvd[4] = len;
    dvd[5] = (u32) buffer;
    dvd[6] = len;
    dvd[7] = 3; 

    /*** Enable reading with DMA ***/
    while (dvd[7] & 1) usleep(10);
    memcpy (dst, buffer, len);

    /*** Ensure it has completed ***/
    if (dvd[0] & 0x4) return 0;

#else
    if (DI_ReadDVD(buffer, len >> 11, (u32)(offset >> 11))) return 0;
    memcpy (dst, buffer, len);
#endif
    return 1;
  }

  return 0; 
}

/****************************************************************************
 * dvd_motor_off
 *
 * Stop the DVD Motor
 *
 * This can be used to prevent the Disc from spinning during playtime
 ****************************************************************************/
void dvd_motor_off( )
{
  GUI_MsgBoxOpen("Information", "Stopping DVD drive ...", 1);

#ifndef HW_RVL
  dvd[0] = 0x2e;
  dvd[1] = 0;
  dvd[2] = 0xe3000000;
  dvd[3] = 0;
  dvd[4] = 0;
  dvd[5] = 0;
  dvd[6] = 0;
  dvd[7] = 1; // Do immediate
  while (dvd[7] & 1) usleep(10);

  /*** PSO Stops blackscreen at reload ***/
  dvd[0] = 0x14;
  dvd[1] = 0;

#else
  DI_StopMotor();
#endif

  GUI_MsgBoxClose();
}

#ifndef HW_RVL
/****************************************************************************
 * uselessinquiry
 *
 * As the name suggests, this function is quite useless.
 * It's only purpose is to stop any pending DVD interrupts while we use the
 * memcard interface.
 *
 * libOGC tends to foul up if you don't, and sometimes does if you do!
 ****************************************************************************/
void uselessinquiry ()
{
  dvd[0] = 0;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0;
  dvd[4] = 0x20;
  dvd[5] = 0x80000000;
  dvd[6] = 0x20;
  dvd[7] = 1;

  while (dvd[7] & 1) usleep(10);
}

/****************************************************************************
 * dvd_drive_detect()
 *
 * Detect the DVD Drive Type
 *
 ****************************************************************************/
void dvd_drive_detect()
{
  dvd[0] = 0x2e;
  dvd[1] = 0;
  dvd[2] = 0x12000000;
  dvd[3] = 0;
  dvd[4] = 0x20;
  dvd[5] = 0x80000000;
  dvd[6] = 0x20;
  dvd[7] = 3;
  while( dvd[7] & 1 ) usleep(10);
  DCFlushRange((void *)0x80000000, 32);

  int driveid = (int)inquiry[2];
  
  if ((driveid == 4) || (driveid == 6) || (driveid == 8))
  {
    /* Gamecube DVD Drive (1.4 GB)*/
    DvdMaxOffset = 0x57057C00;
  }
  else 
  {
    /* Wii DVD Drive (4.7GB) */
    DvdMaxOffset = 0x118244F00LL;
  }
}
#endif
