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
#include "Overlay_bar.h"

#include <png.h>

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
static u8 *texture;

int FONT_Init(void)
{
#ifndef HW_RVL
  /* disable Qoob before accessing IPL */
  ipl_set_config(6);
#endif

  /* initialize IPL font */
  fontHeader = memalign(32,sizeof(sys_fontheader));
  if (!fontHeader) return -1;
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
  texture = memalign(32, fontHeader->cell_width * fontHeader->cell_height / 2);
  if (!texture) return -1;

  return 0;
}

static void DrawChar(unsigned char c, int xpos, int ypos, int size)
{
  s32 width;

  /* reintialize texture object */
  GXTexObj texobj;
  GX_InitTexObj(&texobj, texture, fontHeader->cell_width, fontHeader->cell_height, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_LoadTexObj(&texobj, GX_TEXMAP0);

  /* reinitialize font texture data */
  memset(texture,0,fontHeader->cell_width * fontHeader->cell_height / 2);
  SYS_GetFontTexel(c,texture,0,fontHeader->cell_width/2,&width);
  DCFlushRange(texture, fontHeader->cell_width * fontHeader->cell_height / 2);
  GX_InvalidateTexAll();

  /* adjust texture width */
  width = (fontHeader->cell_width * size) / fontHeader->cell_height;

  /* GX rendering */
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
  GX_Position2s16(xpos, ypos - size);
  GX_TexCoord2f32(0.0, 0.0);
  GX_Position2s16(xpos + width, ypos - size);
  GX_TexCoord2f32(1.0, 0.0);
  GX_Position2s16(xpos + width, ypos);
  GX_TexCoord2f32(1.0, 1.0);
  GX_Position2s16(xpos, ypos);
  GX_TexCoord2f32(0.0, 1.0);
  GX_End ();
  GX_DrawDone();
}


void write_font(int x, int y, char *string)
{
  int ox = x;
  while (*string && (x < (ox + back_framewidth)))
  {
    DrawChar(*string, x -(vmode->fbWidth/2), y-(vmode->efbHeight/2),fontHeader->cell_height);
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
  WriteCentre(y, string);
  png_texture *texture = OpenTexturePNG(Overlay_bar);
  if (texture)
  {
    DrawTexture(texture, 0, y-fheight,  640, fheight);
    if (texture->data) free(texture->data);
    free(texture);
  }
}

void FONT_alignLeft(char *string, int size, int x, int y)
{
  x -= (vmode->fbWidth / 2);
  y -= (vmode->efbHeight / 2);

  while (*string)
  {
    DrawChar(*string, x, y, size);
    x += (font_size[(u8)*string++] * size) / fheight;
  }
}

void FONT_alignRight(char *string, int size, int x, int y)
{
  int i;
  u16 width = 0;

  for (i=0; i<strlen(string); i++)
    width += (font_size[(u8)string[i]] * size) / fheight;

  x -= (vmode->fbWidth / 2) + width;
  y -= (vmode->efbHeight / 2);

  while (*string)
  {
    DrawChar(*string, x, y, size);
    x += (font_size[(u8)*string++] * size) / fheight;
  }
}

void FONT_writeCenter(char *string, int size, int x1, int x2, int y)
{
  int i;
  u16 width = 0;

  for (i=0; i<strlen(string); i++)
    width += (font_size[(u8)string[i]] * size) / fheight;

  x1 += (x2 - x1 - width - vmode->fbWidth) / 2;
  y -= (vmode->efbHeight / 2);

  while (*string)
  {
    DrawChar(*string, x1, y, size);
    x1 += (font_size[(u8)*string++] * size) / fheight;
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
 *  Draw functions (GX)
 *
 ****************************************************************************/
typedef struct
{
  u8 *buffer;
  u32 offset;
} png_file;

/* libpng read callback function */
static void png_read_from_mem (png_structp png_ptr, png_bytep data, png_size_t length)
{
  png_file *file = (png_file *)png_get_io_ptr (png_ptr);

  /* copy data from image buffer */
  memcpy (data, file->buffer + file->offset, length);

  /* advance in the file */
  file->offset += length;
}

/* convert a png file into RGBA8 texture */
png_texture *OpenTexturePNG(const u8 *buffer)
{
  int i;
  png_file file;

  /* init PNG file structure */
  file.buffer = (u8 *) buffer;
  file.offset = 0;

  /* check for valid magic number */
  /*if (!png_check_sig (file.buffer, 8)) return;*/

  /* create a png read struct */
  png_structp png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) return NULL;

  /* create a png info struct */
  png_infop info_ptr = png_create_info_struct (png_ptr);
  if (!info_ptr)
  {
    png_destroy_read_struct (&png_ptr, NULL, NULL);
    return NULL;
  }

  /* set callback for the read function */
  png_set_read_fn (png_ptr, (png_voidp *)(&file), png_read_from_mem);

  /* read png info */
  png_read_info (png_ptr, info_ptr);

  /* retrieve image information */
  u32 width  = png_get_image_width(png_ptr, info_ptr);
  u32 height = png_get_image_height(png_ptr, info_ptr);
  /*u32 bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  u32 color_type = png_get_color_type(png_ptr, info_ptr);*/

  /* support for RGBA8 textures ONLY !*/
  /*if ((color_type != PNG_COLOR_TYPE_RGB_ALPHA) || (bit_depth != 8))
  {
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return;
  }*/

  /* 4x4 tiles are required */
  /*if ((width%4) || (height%4))
  {
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return;
  }*/

  /* allocate memory to store raw image data */
  u32 stride = width << 2;
  u8 *img_data = memalign (32, stride * height);
  if (!img_data)
  {
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return NULL;
  }

  /* allocate row pointer data */
  png_bytep *row_pointers = (png_bytep *)memalign (32, sizeof (png_bytep) * height);
  if (!row_pointers)
  {
    free (img_data);
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return NULL;
  }

  /* store raw image data */
  for (i = 0; i < height; i++)
  {
    row_pointers[i] = img_data + (i * stride);
  }

  /* decode image */
  png_read_image (png_ptr, row_pointers);

  /* finish decompression and release memory */
  png_read_end (png_ptr, NULL);
  png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
  free(row_pointers);

  /* initialize texture */
  png_texture *texture = (png_texture *)memalign(32, sizeof(png_texture));
  if (!texture)
  {
    free (img_data);
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return NULL;
  }

  /* initialize texture data */
  texture->data = memalign(32, stride * height);
  if (!texture->data)
  {
    free (img_data);
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    free(texture);
    return NULL;
  }

  memset(texture->data, 0, stride * height);
  texture->width  = width;
  texture->height = height;
  texture->format = GX_TF_RGBA8;

  /* encode to GX_TF_RGBA8 format (4x4 pixels paired titles) */
  u16 *dst_ar = (u16 *)(texture->data);
  u16 *dst_gb = (u16 *)(texture->data + 32);
  u32 *src1 = (u32 *)(img_data);
  u32 *src2 = (u32 *)(img_data + stride);
  u32 *src3 = (u32 *)(img_data + 2*stride);
  u32 *src4 = (u32 *)(img_data + 3*stride);
  u32 pixel,h,w;

  for (h=0; h<height; h+=4)
  {
    for (w=0; w<width; w+=4)
    {
      /* line N (4 pixels) */
      for (i=0; i<4; i++)
      {
        pixel = *src1++;
        *dst_ar++= ((pixel << 8) & 0xff00) | ((pixel >> 24) & 0x00ff);
        *dst_gb++= (pixel >> 8) & 0xffff;
      }

      /* line N + 1 (4 pixels) */
      for (i=0; i<4; i++)
      {
        pixel = *src2++;
        *dst_ar++= ((pixel << 8) & 0xff00) | ((pixel >> 24) & 0x00ff);
        *dst_gb++= (pixel >> 8) & 0xffff;
      }

      /* line N + 2 (4 pixels) */
      for (i=0; i<4; i++)
      {
        pixel = *src3++;
        *dst_ar++= ((pixel << 8) & 0xff00) | ((pixel >> 24) & 0x00ff);
        *dst_gb++= (pixel >> 8) & 0xffff;
      }

      /* line N + 3 (4 pixels) */
      for (i=0; i<4; i++)
      {
        pixel = *src4++;
        *dst_ar++= ((pixel << 8) & 0xff00) | ((pixel >> 24) & 0x00ff);
        *dst_gb++= (pixel >> 8) & 0xffff;
      }

      /* next paired tiles */
      dst_ar += 16;
      dst_gb += 16;
    }

    /* next 4 lines */
    src1 = src4;
    src2 = src1 + width;
    src3 = src2 + width;
    src4 = src3 + width;
  }

  /* release memory */
  free(img_data);

  /* flush texture data from cache */
  DCFlushRange(texture->data, height * stride);

  return texture;
}

void DrawTexture(png_texture *texture, int x, int y, int w, int h)
{
  if (!texture) 
  {
    FONT_alignLeft("error",16,x,y);
    return;
  }

  if (texture->data)
  {
    /* load texture object */
    GXTexObj texObj;
    GX_InitTexObj(&texObj, texture->data, texture->width, texture->height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texObj,GX_LINEAR,GX_LIN_MIP_LIN,0.0,10.0,0.0,GX_FALSE,GX_TRUE,GX_ANISO_4);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    /* adjust coordinate system */
    x -= (vmode->fbWidth/2);
    y -= (vmode->efbHeight/2);

    /* Draw textured quad */
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(x,y+h);
    GX_TexCoord2f32(0.0, 1.0);
    GX_Position2s16(x+w,y+h);
    GX_TexCoord2f32(1.0, 1.0);
    GX_Position2s16(x+w,y);
    GX_TexCoord2f32(1.0, 0.0);
    GX_Position2s16(x,y);
    GX_TexCoord2f32(0.0, 0.0);
    GX_End ();
    GX_DrawDone();
  }
}

/****************************************************************************
 *  Display functions
 *
 ****************************************************************************/
u8 SILENT = 0;

void SetScreen ()
{
  GX_CopyDisp(xfb[whichfb], GX_FALSE);
  GX_Flush();
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();
}

void ClearScreen (GXColor color)
{
  whichfb ^= 1;
  GX_SetCopyClear(color,0x00ffffff);
  GX_CopyDisp(xfb[whichfb], GX_TRUE);
  GX_Flush();
}

extern s16 ogc_input__getMenuButtons(u32 cnt);

void WaitButtonA ()
{
  s16 p = ogc_input__getMenuButtons(0);
  while (p & PAD_BUTTON_A)    p = ogc_input__getMenuButtons(0);
  while (!(p & PAD_BUTTON_A)) p = ogc_input__getMenuButtons(0);
}

void WaitPrompt (char *msg)
{
  if (SILENT) return;
  ClearScreen((GXColor)BLACK);
  WriteCentre(254, msg);
  WriteCentre(254 + fheight, "Press A to Continue");
  SetScreen();
  WaitButtonA ();
}

void ShowAction (char *msg)
{
  if (SILENT) return;

  ClearScreen((GXColor)BLACK);
  WriteCentre(254, msg);
  SetScreen();
}
