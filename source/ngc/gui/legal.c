/****************************************************************************
 *  legal.c
 *
 *  generic legal informations screen
 *
 *  code by Softdev (2006), Eke-Eke (2007,2008)
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
 ***************************************************************************/


#include "shared.h"
#include "font.h"
#include "dkpro.h"

/*
 * Unpack the devkit pro logo
 */
u32 *dkproraw;

int dkunpack ()
{
  unsigned long res, inbytes, outbytes;

  inbytes = dkpro_COMPRESSED;
  outbytes = dkpro_RAW;
  dkproraw = malloc (dkpro_RAW + 16);
  res = uncompress ((Bytef *) dkproraw, &outbytes, (Bytef *) &dkpro[0], inbytes);
  if (res == Z_OK) return 1;
  free (dkproraw);
  return 0;
}

/* 
 * This is the legal stuff - which must be shown at program startup 
 * Any derivative work MUST include the same textual output.
 *
 * In other words, play nice and give credit where it's due.
 */
void legal ()
{
  int ypos = 64;

  whichfb ^= 1;
  VIDEO_ClearFrameBuffer(&TVNtsc480IntDf, xfb[whichfb], COLOR_BLACK);
  back_framewidth = 640;

  WriteCentre (ypos, "Genesis Plus Sega Mega Drive Emulator (v1.2a)");
  ypos += fheight;
  WriteCentre (ypos, "(C) 1999 - 2003 Charles MacDonald");
  ypos += fheight;
  WriteCentre (ypos, "This is free software, and you are welcome to");
  ypos += fheight;
  WriteCentre (ypos, "redistribute it under the conditions of the");
  ypos += fheight;
  WriteCentre (ypos, "GNU GENERAL PUBLIC LICENSE Version 2");
  ypos += fheight;
  WriteCentre (ypos, "Additionally, the developers of this port");
  ypos += fheight;
  WriteCentre (ypos, "disclaims all copyright interests in the ");
  ypos += fheight;
  WriteCentre (ypos, "Nintendo Gamecube Porting code.");
  ypos += fheight;
  WriteCentre (ypos, "You are free to use it as you wish.");
  ypos += 6 * fheight;

  if (dkunpack ())
  {
      int w, h, p, dispoffset;
      p = 0;
      dispoffset = (316 * 320) + ((640 - dkpro_WIDTH) >> 2);

      for (h = 0; h < dkpro_HEIGHT; h++)
    {
      for (w = 0; w < dkpro_WIDTH >> 1; w++)
      xfb[whichfb][dispoffset + w] = dkproraw[p++];

      dispoffset += 320;
    }

      free (dkproraw);
  }
  else WriteCentre (ypos, "Developed with DevkitPPC and libOGC");
#ifdef HW_RVL
  SetScreen ();
  sleep(1);
#endif
  WriteCentre (ypos, "Press A to continue");
  SetScreen ();
  WaitButtonA ();
}
