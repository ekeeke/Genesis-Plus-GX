/*****************************************************************************
 * font.c
 *
 *   IPL Font Engine, powered by GX hardware
 *
 *   code Eke-Eke(2009)
 * 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ********************************************************************************/

#ifndef _FONT_H
#define _FONT_H

extern int  FONT_Init(void);
extern void FONT_Shutdown(void);
extern void FONT_alignLeft(char *string, int size, int x, int y, GXColor color);
extern void FONT_alignRight(char *string, int size, int x, int y, GXColor color);
extern void FONT_writeCenter(char *string, int size, int x1, int x2, int y, GXColor color);


extern void WaitButtonA ();
extern void WaitPrompt (char *msg);
extern void ShowAction (char *msg);


extern void WriteCentre_HL( int y, char *string);
extern void WriteCentre (int y, char *string);
extern void write_font (int x, int y, char *string);
extern void WriteText(char *text, int size, int x, int y);
extern void fntDrawBoxFilled (int x1, int y1, int x2, int y2, int color);
extern int fheight;
extern int font_size[256];
extern u16 back_framewidth;

#endif
