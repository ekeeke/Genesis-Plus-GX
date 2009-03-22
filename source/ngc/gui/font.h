/*****************************************************************************
 * font.c
 *
 *   IPL FONT Engine, based on Qoob MP3 Player Font
 *
 *   code by Softdev (2006), Eke-Eke(2007-2008)
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

#define BLACK {0,0,0,255}
#define WHITE {255,255,255,255}

typedef struct
{
  u8 *data;
  u16 width;
  u16 height;
  u8 format;
} png_texture;

extern void OpenPNGFromMemory(png_texture *texture, const u8 *buffer);
extern void DrawTexture(png_texture *texture, u32 xOrigin, u32 yOrigin, u32 w, u32 h);

extern int FONT_Init(void);
void FONT_WriteLeft(char *string, u16 size, u16 x, u16 y);
void FONT_WriteRight(char *string, u16 size, u16 x, u16 y);
void FONT_WriteCenter(char *string, u16 size, u16 x1, u16 x2, u16 y);


extern void WriteCentre_HL( int y, char *string);
extern void WriteCentre (int y, char *string);
extern void write_font (int x, int y, char *string);
extern void WriteText(char *text, u16 size, u16 x, u16 y);
extern void WaitPrompt (char *msg);
extern void ShowAction (char *msg);
extern void WaitButtonA ();
extern void ClearScreen (GXColor color);
extern void SetScreen ();
extern void fntDrawBoxFilled (int x1, int y1, int x2, int y2, int color);
extern int fheight;
extern int font_size[256];
extern u16 back_framewidth;
extern u8 SILENT;

#endif
