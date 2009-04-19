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

#include "shared.h"
#include "font.h"
#include "menu.h"

/* Backdrop Frame Width (to avoid writing outside of the background frame) */
u16 back_framewidth = 640;
int font_size[256], fheight;

#ifndef HW_RVL
/* disable Qoob Modchip before IPL access (emukiddid) */
static void ipl_set_config(unsigned char c)
{
  volatile unsigned long* exi = (volatile unsigned long*)0xCC006800;
  unsigned long val,addr;
  addr=0xc0000000;
  val = c << 24;
  exi[0] = ((((exi[0]) & 0x405) | 256) | 48);     //select IPL
  //write addr of IPL
  exi[0 * 5 + 4] = addr;
  exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
  while (exi[0 * 5 + 3] & 1);
  //write the ipl we want to send
  exi[0 * 5 + 4] = val;
  exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
  while (exi[0 * 5 + 3] & 1);
  exi[0] &= 0x405;        //deselect IPL
}
#endif

static sys_fontheader *fontHeader;
static u8 *fontTexture;

int FONT_Init(void)
{
#ifndef HW_RVL
  /* disable Qoob before accessing IPL */
  ipl_set_config(6);
#endif

  /* initialize IPL font */
  fontHeader = memalign(32,sizeof(sys_fontheader));
  if (!fontHeader) return 0;
  SYS_InitFont(&fontHeader);

  /* character width table */
  int i,c;
  for (i=0; i<256; ++i)
  {
    if ((i < fontHeader->first_char) || (i > fontHeader->last_char)) c = fontHeader->inval_char;
    else c = i - fontHeader->first_char;
    font_size[i] = ((unsigned char*)fontHeader)[fontHeader->width_table + c];
  }

  /* default font height */
  fheight = fontHeader->cell_height;
  
  /* initialize texture data */
  fontTexture = memalign(32, fontHeader->cell_width * fontHeader->cell_height / 2);
  if (!fontTexture)
  {
    free(fontHeader);
    return 0;
  }

  return 1;
}

void FONT_Shutdown(void)
{
  if (fontHeader) free(fontHeader);
  if (fontTexture) free(fontTexture);
}

static void DrawChar(unsigned char c, int xpos, int ypos, int size, GXColor color)
{
  s32 width;

  /* reintialize texture object */
  GXTexObj texobj;
  GX_InitTexObj(&texobj, fontTexture, fontHeader->cell_width, fontHeader->cell_height, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_LoadTexObj(&texobj, GX_TEXMAP0);

  /* reinitialize font texture data */
  memset(fontTexture,0,fontHeader->cell_width * fontHeader->cell_height / 2);
  SYS_GetFontTexel(c,fontTexture,0,fontHeader->cell_width/2,&width);
  DCFlushRange(fontTexture, fontHeader->cell_width * fontHeader->cell_height / 2);
  GX_InvalidateTexAll();

  /* adjust texture width */
  width = (fontHeader->cell_width * size) / fontHeader->cell_height;

  /* GX rendering */
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
  GX_Position2s16(xpos, ypos - size);
  GX_Color4u8(color.r, color.g, color.b, 0xff);
  GX_TexCoord2f32(0.0, 0.0);
  GX_Position2s16(xpos + width, ypos - size);
  GX_Color4u8(color.r, color.g, color.b, 0xff);
  GX_TexCoord2f32(1.0, 0.0);
  GX_Position2s16(xpos + width, ypos);
  GX_Color4u8(color.r, color.g, color.b, 0xff);
  GX_TexCoord2f32(1.0, 1.0);
  GX_Position2s16(xpos, ypos);
  GX_Color4u8(color.r, color.g, color.b, 0xff);
  GX_TexCoord2f32(0.0, 1.0);
  GX_End ();
  GX_DrawDone();
}


void write_font(int x, int y, char *string)
{
  int ox = x;
  while (*string && (x < (ox + back_framewidth)))
  {
    DrawChar(*string, x -(vmode->fbWidth/2), y-(vmode->efbHeight/2),fontHeader->cell_height,(GXColor)WHITE);
    x += font_size[(u8)*string];
    string++;
  }
}

void WriteCentre( int y, char *string)
{
  int x, t;
  for (x=t=0; t<strlen(string); t++) x += font_size[(u8)string[t]];
  if (x>back_framewidth) x=back_framewidth;
  x = (640 - x) >> 1;
  write_font(x, y, string);
}

void WriteCentre_HL( int y, char *string)
{
  gx_texture *texture = gxTextureOpenPNG(Overlay_bar_png);
  if (texture)
  {
    gxDrawTexture(texture, 0, y-fheight,  640, fheight,240);
    if (texture->data) free(texture->data);
    free(texture);
  }
  WriteCentre(y, string);
}

int FONT_write(char *string, int size, int x, int y, int max_width, GXColor color)
{
  x -= (vmode->fbWidth / 2);
  y -= (vmode->efbHeight / 2);
  int w, ox = x;

  while (*string)
  {
    w = (font_size[(u8)*string] * size) / fheight;
    if ((x + w) <= (ox + max_width))
    {
      DrawChar(*string, x, y, size,color);
      x += w;
      string++;
    }
    else return 1;
  }
  return 0;
}

void FONT_writeCenter(char *string, int size, int x1, int x2, int y, GXColor color)
{
  int i;
  u16 width = 0;

  for (i=0; i<strlen(string); i++)
    width += (font_size[(u8)string[i]] * size) / fheight;

  x1 += (x2 - x1 - width - vmode->fbWidth) / 2;
  y -= (vmode->efbHeight / 2);

  while (*string)
  {
    DrawChar(*string, x1, y, size,color);
    x1 += (font_size[(u8)*string++] * size) / fheight;
  }
}

void FONT_alignRight(char *string, int size, int x, int y, GXColor color)
{
  int i;
  u16 width = 0;

  for (i=0; i<strlen(string); i++)
    width += (font_size[(u8)string[i]] * size) / fheight;

  x -= (vmode->fbWidth / 2) + width;
  y -= (vmode->efbHeight / 2);

  while (*string)
  {
    DrawChar(*string, x, y, size,color);
    x += (font_size[(u8)*string++] * size) / fheight;
  }
}

/****************************************************************************
 *  Draw functions (FrameBuffer)
 *
 ****************************************************************************/
void fntDrawHLine (int x1, int x2, int y, int color)
{
  int i;
  y = 320 * y;
  x1 >>= 1;
  x2 >>= 1;
  for (i = x1; i <= x2; i++) xfb[whichfb][y + i] = color;
}

void fntDrawVLine (int x, int y1, int y2, int color)
{
  int i;
  x >>= 1;
  for (i = y1; i <= y2; i++) xfb[whichfb][x + (640 * i) / 2] = color;
}

void fntDrawBox (int x1, int y1, int x2, int y2, int color)
{
  fntDrawHLine (x1, x2, y1, color);
  fntDrawHLine (x1, x2, y2, color);
  fntDrawVLine (x1, y1, y2, color);
  fntDrawVLine (x2, y1, y2, color);
}

void fntDrawBoxFilled (int x1, int y1, int x2, int y2, int color)
{
  int h;
  for (h = y1; h <= y2; h++) fntDrawHLine (x1, x2, h, color);
}

/****************************************************************************
 *  Generic GUI functions (deprecated)
 *
 ****************************************************************************/
u8 SILENT = 0;

void WaitButtonA ()
{
  while (m_input.keys & PAD_BUTTON_A)    VIDEO_WaitVSync();
  while (!(m_input.keys & PAD_BUTTON_A)) VIDEO_WaitVSync();
}

void WaitPrompt (char *msg)
{
  if (SILENT) return;
  gxClearScreen((GXColor)BLACK);
  WriteCentre(254, msg);
  WriteCentre(254 + fheight, "Press A to Continue");
  gxSetScreen();
  WaitButtonA ();
}

void ShowAction (char *msg)
{
  if (SILENT) return;

  gxClearScreen((GXColor)BLACK);
  WriteCentre(254, msg);
  gxSetScreen();
}
