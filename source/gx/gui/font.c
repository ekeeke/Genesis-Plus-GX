/*****************************************************************************
 * font.c
 *
 *   IPL font engine
 *
 *   original font support by Softdev (2006)
 *   GX rendering by Eke-Eke (2008,2009)
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

#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

typedef struct _yay0header {
	unsigned int id ATTRIBUTE_PACKED;
	unsigned int dec_size ATTRIBUTE_PACKED;
	unsigned int links_offset ATTRIBUTE_PACKED;
	unsigned int chunks_offset ATTRIBUTE_PACKED;
} yay0header;

u8 font_size[256];
int fheight;

static u8 *fontImage;
static u8 *fontTexture;
static void *ipl_fontarea;
static sys_fontheader *fontHeader;


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

static void decode_szp(void *src,void *dest)
{
	u32 i,k,link;
	u8 *dest8,*tmp;
	u32 loff,coff,roff;
	u32 size,cnt,cmask,bcnt;
	yay0header *header;

	dest8 = (u8*)dest;
	header = (yay0header*)src;
	size = header->dec_size;
	loff = header->links_offset;
	coff = header->chunks_offset;

	roff = sizeof(yay0header);
	cmask = 0;
	cnt = 0;
	bcnt = 0;

	do {
		if(!bcnt) {
			cmask = *(u32*)(src+roff);
			roff += 4;
			bcnt = 32;
		}

		if(cmask&0x80000000) {
			dest8[cnt++] = *(u8*)(src+coff);
			coff++;
		} else {
			link = *(u16*)(src+loff);
			loff += 2;

			tmp = dest8+(cnt-(link&0x0fff)-1);
			k = link>>12;
			if(k==0) {
				k = (*(u8*)(src+coff))+18;
				coff++;
			} else k += 2;

			for(i=0;i<k;i++) {
				dest8[cnt++] = tmp[i];
			}
		}
		cmask <<= 1;
		bcnt--;
	} while(cnt<size);
}

static void expand_font(u8 *src,u8 *dest)
{
	s32 cnt;
	u32 idx;
	u8 val1,val2;
  sys_fontheader *sys_fontdata = fontHeader;
	u8 *data = (u8*)sys_fontdata+44;

	if(sys_fontdata->sheet_format==0x0000) {
		cnt = (sys_fontdata->sheet_fullsize/2)-1;

		while(cnt>=0) {
			idx = _SHIFTR(src[cnt],6,2);
			val1 = data[idx];

			idx = _SHIFTR(src[cnt],4,2);
			val2 = data[idx];

			dest[(cnt<<1)+0] =((val1&0xf0)|(val2&0x0f));

			idx = _SHIFTR(src[cnt],2,2);
			val1 = data[idx];

			idx = _SHIFTR(src[cnt],0,2);
			val2 = data[idx];

			dest[(cnt<<1)+1] =((val1&0xf0)|(val2&0x0f));

			cnt--;
		}
	}
	DCStoreRange(dest,sys_fontdata->sheet_fullsize);
}

static void GetFontTexel(s32 c,void *image,s32 pos,s32 stride)
{
	u32 sheets,rem;
	u32 xoff,yoff;
	u32 xpos,ypos;
	u8 *img_start;
	u8 *ptr1,*ptr2;
  sys_fontheader *sys_fontdata = fontHeader;

	if(c<sys_fontdata->first_char || c>sys_fontdata->last_char) c = sys_fontdata->inval_char;
	else c -= sys_fontdata->first_char;

	sheets = sys_fontdata->sheet_column*sys_fontdata->sheet_row;
	rem = c%sheets;
  sheets = c/sheets;
	xoff = (rem%sys_fontdata->sheet_column)*sys_fontdata->cell_width;
	yoff = (rem/sys_fontdata->sheet_column)*sys_fontdata->cell_height;
	img_start = fontImage+(sys_fontdata->sheet_size*sheets);

	ypos = 0;
	while(ypos<sys_fontdata->cell_height) {
		xpos = 0;
		while(xpos<sys_fontdata->cell_width) {
			ptr1 = img_start+(((sys_fontdata->sheet_width/8)<<5)*((ypos+yoff)/8));
			ptr1 = ptr1+(((xpos+xoff)/8)<<5);
			ptr1 = ptr1+(((ypos+yoff)%8)<<2);
			ptr1 = ptr1+(((xpos+xoff)%8)/2);

			ptr2 = image+((ypos/8)*(((stride<<1)/8)<<5));
			ptr2 = ptr2+(((xpos+pos)/8)<<5);
			ptr2 = ptr2+(((xpos+pos)%8)/2);
			ptr2 = ptr2+((ypos%8)<<2);

			*ptr2 = *ptr1;

			xpos += 2;
		}
		ypos++;
	}
}

static void DrawChar(unsigned char c, int xpos, int ypos, int size, GXColor color)
{
  /* reintialize texture object */
  GXTexObj texobj;
  GX_InitTexObj(&texobj, fontTexture, fontHeader->cell_width, fontHeader->cell_height, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_LoadTexObj(&texobj, GX_TEXMAP0);

  /* reinitialize font texture data */
  memset(fontTexture,0,fontHeader->cell_width * fontHeader->cell_height / 2);
  GetFontTexel(c,fontTexture,0,fontHeader->cell_width/2);
  DCFlushRange(fontTexture, fontHeader->cell_width * fontHeader->cell_height / 2);
  GX_InvalidateTexAll();

  /* adjust texture width */
  s32 width = (fontHeader->cell_width * size) / fontHeader->cell_height;

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

/****************************************************************************
 *  IPL font support
 *
 ****************************************************************************/
extern void __SYS_ReadROM(void *buf,u32 len,u32 offset);

int FONT_Init(void)
{
#ifndef HW_RVL
  /* --- Game Cube --- disable Qoob before accessing IPL */
  ipl_set_config(6);
#endif

  /* read IPL font (ASCII) from Mask ROM */
  ipl_fontarea = memalign(32,131360);
  if (!ipl_fontarea)
    return 0;
  memset(ipl_fontarea,0,131360);
  __SYS_ReadROM(ipl_fontarea+119072,12288,0x1FCF00);

  /* YAY0 decompression */
  decode_szp(ipl_fontarea+119072,ipl_fontarea);

	/* retrieve IPL font data */
  fontHeader = (sys_fontheader*)ipl_fontarea;
  fontImage = (u8*)((((u32)ipl_fontarea+fontHeader->sheet_image)+31)&~31);
  
  /* expand to I4 format */
  expand_font((u8*)ipl_fontarea+fontHeader->sheet_image,fontImage);

  /* character width table */
  int i,c;
  for (i=0; i<256; ++i)
  {
    if ((i < fontHeader->first_char) || (i > fontHeader->last_char))
      c = fontHeader->inval_char;
    else
      c = i - fontHeader->first_char;

    font_size[i] = ((u8*)fontHeader)[fontHeader->width_table + c];
  }

  /* font height */
  fheight = fontHeader->cell_height;

  /* initialize texture data */
  fontTexture = memalign(32, fontHeader->cell_width * fontHeader->cell_height / 2);
  if (!fontTexture)
  {
    free(ipl_fontarea);
    return 0;
  }

  return 1;
}

void FONT_Shutdown(void)
{
  if (fontHeader)
    free(ipl_fontarea);
  if (fontTexture)
    free(fontTexture);
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
  int i=0;
  u16 width = 0;

  while (string[i] && (string[i] != '\n'))
    width += (font_size[(u8)string[i++]] * size) / fheight;

  int x = x1 + (x2 - x1 - width - vmode->fbWidth) / 2;
  y -= (vmode->efbHeight / 2);

  while (*string && (*string != '\n'))
  {
    DrawChar(*string, x, y, size,color);
    x += (font_size[(u8)*string++] * size) / fheight;
  }

  if (*string == '\n')
  {
    string++;
    i = 0;
    width = 0;
    while (string[i])
      width += (font_size[(u8)string[i++]] * size) / fheight;
    x = x1 + (x2 - x1 - width - vmode->fbWidth) / 2;
    y += size;
    while (*string)
    {
      DrawChar(*string, x, y, size,color);
      x += (font_size[(u8)*string++] * size) / fheight;
    }
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
 *  Write functions (OLD)
 *
 ****************************************************************************/
void write_font(int x, int y, char *string)
{
  int ox = x;
  while (*string && (x < (ox + 640)))
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
  if (x>640) x=640;
  x = (640 - x) >> 1;
  write_font(x, y, string);
}

void WriteCentre_HL( int y, char *string)
{
  gx_texture *texture = gxTextureOpenPNG(Overlay_bar_png,0);
  if (texture)
  {
    gxDrawTexture(texture, 0, y-fheight,  640, fheight,240);
    if (texture->data) free(texture->data);
    free(texture);
  }
  WriteCentre(y, string);
}

/****************************************************************************
 *  Draw functions (FrameBuffer)
 *
 ****************************************************************************/
static void fntDrawHLine (int x1, int x2, int y, int color)
{
  int i;
  y = 320 * y;
  x1 >>= 1;
  x2 >>= 1;
  for (i = x1; i <= x2; i++) xfb[whichfb][y + i] = color;
}

void fntDrawBoxFilled (int x1, int y1, int x2, int y2, int color)
{
  int h;
  for (h = y1; h <= y2; h++) fntDrawHLine (x1, x2, h, color);
}
