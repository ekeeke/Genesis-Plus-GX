/***************************************************************************************
 *  Genesis Plus 1.2a
 *  Video Display Processor (Rendering)
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  modified by Eke-Eke (compatibility fixes & additional code), GC/Wii port
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
 ****************************************************************************************/

#include "shared.h"

#include "md_ntsc.h"
#include "sms_ntsc.h"

/*** NTSC Filters ***/
extern md_ntsc_t md_ntsc;
extern sms_ntsc_t sms_ntsc;

/* Look-up pixel table information */
#define LUT_MAX     (5)
#define LUT_SIZE    (0x10000)

/* Clip structure */
typedef struct
{
  uint8 left;
  uint8 right;
  uint8 enable;
}clip_t;
  
/* Function prototypes */
void render_obj(int line, uint8 *buf, uint8 *table);
void render_obj_im2(int line, uint8 *buf, uint8 *table, uint8 odd);
void render_ntw(int line, uint8 *buf);
void render_ntw_im2(int line, uint8 *buf, uint8 odd);
void render_ntx(int which, int line, uint8 *buf);
void render_ntx_im2(int which, int line, uint8 *buf, uint8 odd);
void render_ntx_vs(int which, int line, uint8 *buf);
void update_bg_pattern_cache(void);
void get_hscroll(int line, int shift, uint16 *scroll);
void window_clip(int line);
int make_lut_bg(int bx, int ax);
int make_lut_obj(int bx, int sx);
int make_lut_bg_ste(int bx, int ax);
int make_lut_obj_ste(int bx, int sx);
int make_lut_bgobj_ste(int bx, int sx);
void merge(uint8 *srca, uint8 *srcb, uint8 *dst, uint8 *table, int width);
void make_name_lut(void);
void (*color_update)(int index, uint16 data);
void remap_16(uint8 *src, uint16 *dst, uint16 *table, int length);
#ifndef NGC
void remap_8(uint8 *src, uint8 *dst, uint8 *table, int length);
void remap_32(uint8 *src, uint32 *dst, uint32 *table, int length);
#endif

#ifndef LSB_FIRST
static uint32 ATTR_MSB;
#endif

#ifdef ALIGN_LONG
/* Or change the names if you depend on these from elsewhere.. */
#undef READ_LONG
#undef WRITE_LONG

static __inline__ uint32 READ_LONG(void *address)
{
  if ((uint32)address & 3)
	{
#ifdef LSB_FIRST  /* little endian version */
    return ( *((uint8 *)address) +
        (*((uint8 *)address+1) << 8)  +
        (*((uint8 *)address+2) << 16) +
        (*((uint8 *)address+3) << 24) );
#else       /* big endian version */
    return ( *((uint8 *)address+3) +
        (*((uint8 *)address+2) << 8)  +
        (*((uint8 *)address+1) << 16) +
        (*((uint8 *)address)   << 24) );
#endif  /* LSB_FIRST */
	}
	else return *(uint32 *)address;
}

static __inline__ void WRITE_LONG(void *address, uint32 data)
{
  if ((uint32)address & 3)
	{
#ifdef LSB_FIRST
      *((uint8 *)address) =  data;
      *((uint8 *)address+1) = (data >> 8);
      *((uint8 *)address+2) = (data >> 16);
      *((uint8 *)address+3) = (data >> 24);
#else
      *((uint8 *)address+3) =  data;
      *((uint8 *)address+2) = (data >> 8);
      *((uint8 *)address+1) = (data >> 16);
      *((uint8 *)address)   = (data >> 24);
#endif /* LSB_FIRST */
		return;
  	}
  	else *(uint32 *)address = data;
}

#endif  /* ALIGN_LONG */


/*   
   Two Pattern Attributes are written in VRAM as two 16bits WORD:

   P = priority bit
   C = color palette (2 bits)
   V = Vertical Flip bit
   H = Horizontal Flip bit
   N = Pattern Number (11 bits)

   MSB PCCVHNNN NNNNNNNN LSB  PCCVHNNN NNNNNNNN LSB
	    PATTERN1        PATTERN2

   Pattern attributes are read from VRAM as 32bits WORD like this:

   LIT_ENDIAN: ATTR is  MSB PCCVHNNN NNNNNNNN PCCVHNNN NNNNNNNN LSB
								PATTERN2       PATTERN1

   BIG_ENDIAN: ATTR is  MSB PCCVHNNN NNNNNNNN PCCVHNNN NNNNNNNN LSB
								PATTERN1       PATTERN2


   Each Line Buffer written byte describe one pixel data like this:
   
  	msb SPppcccc lsb
  
  	with:
    	S = sprite data indicator (not written here)
    	P = priority bit  (from Pattern Attribute)
    	p = color palette (from Pattern Attribute)
    	c = color data (from Pattern Cache)
    

   A column is 2 patterns wide
   A pattern is 8 pixels wide = 8 bytes = two 32 bits write
*/

/* Draw a single 8-pixel column */
/*
   pattern cache is addressed like this: 00000VHN NNNNNNNN NNYYYXXX
	with :  Y = pattern row (1-8 lines)
    	X = pattern column (1-8 pixels)
			V = Vertical Flip bit
		  H = Horizontal Flip bit
		  N = Pattern Number (1-2048)
*/
#ifdef ALIGN_LONG
#ifdef LSB_FIRST
#define DRAW_COLUMN(ATTR, LINE) \
  atex = atex_table[(ATTR >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR & 0x1FFF) << 6 | (LINE)]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  ATTR >>= 16; \
  atex = atex_table[(ATTR >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR & 0x1FFF) << 6 | (LINE)]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++;
#else
#define DRAW_COLUMN(ATTR, LINE) \
  ATTR_MSB = ATTR >> 16; \
	atex = atex_table[(ATTR_MSB >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR_MSB & 0x1FFF) << 6 | (LINE)]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  atex = atex_table[(ATTR >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR & 0x1FFF) << 6 | (LINE)]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++;
#endif
#else /* NOT ALIGNED */
#ifdef LSB_FIRST
#define DRAW_COLUMN(ATTR, LINE) \
  atex = atex_table[(ATTR >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR & 0x1FFF) << 6 | (LINE)]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex); \
  ATTR >>= 16; \
  atex = atex_table[(ATTR >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR & 0x1FFF) << 6 | (LINE)]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex);
#else
#define DRAW_COLUMN(ATTR, LINE) \
  ATTR_MSB = ATTR >> 16; \
	atex = atex_table[(ATTR_MSB >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR_MSB & 0x1FFF) << 6 | (LINE)]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex); \
  atex = atex_table[(ATTR >> 13) & 7]; \
  src = (uint32 *)&bg_pattern_cache[(ATTR & 0x1FFF) << 6 | (LINE)]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex);
#endif
#endif /* ALIGN_LONG */


/* Draw a single 16-pixel column */
/*
   pattern cache is addressed like this: 00000VHN NNNNNNNN NYYYYXXX
	with :  Y = pattern row (1-16 lines)
    	X = pattern column (1-8 pixels)
   			V = Vertical Flip bit
		  H = Horizontal Flip bit
		  N = Pattern Number (1-1024)

   one pattern line is 8 pixels = 8 bytes = 2 * 32 bits 
*/
#ifdef ALIGN_LONG
#ifdef LSB_FIRST 
#define DRAW_COLUMN_IM2(ATTR, LINE) \
  atex = atex_table[(ATTR >> 13) & 7]; \
  offs = (ATTR & 0x03FF) << 7 | (ATTR & 0x1800) << 6 | (LINE); \
  if(ATTR & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  ATTR >>= 16; \
  atex = atex_table[(ATTR >> 13) & 7]; \
  offs = (ATTR & 0x03FF) << 7 | (ATTR & 0x1800) << 6 | (LINE); \
  if(ATTR & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; 
#else
#define DRAW_COLUMN_IM2(ATTR, LINE) \
  ATTR_MSB = ATTR >> 16; \
	atex = atex_table[(ATTR_MSB >> 13) & 7]; \
  offs = (ATTR_MSB & 0x03FF) << 7 | (ATTR_MSB & 0x1800) << 6 | (LINE); \
  if(ATTR_MSB & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  atex = atex_table[(ATTR >> 13) & 7]; \
  offs = (ATTR & 0x03FF) << 7 | (ATTR & 0x1800) << 6 | (LINE); \
  if(ATTR & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; \
  WRITE_LONG(dst, READ_LONG(src) | atex); \
  dst++; \
  src++; 
#endif
#else /* NOT ALIGNED */
#ifdef LSB_FIRST 
#define DRAW_COLUMN_IM2(ATTR, LINE) \
  atex = atex_table[(ATTR >> 13) & 7]; \
  offs = (ATTR & 0x03FF) << 7 | (ATTR & 0x1800) << 6 | (LINE); \
  if(ATTR & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex); \
  ATTR >>= 16; \
  atex = atex_table[(ATTR >> 13) & 7]; \
  offs = (ATTR & 0x03FF) << 7 | (ATTR & 0x1800) << 6 | (LINE); \
  if(ATTR & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex);
#else
#define DRAW_COLUMN_IM2(ATTR, LINE) \
  ATTR_MSB = ATTR >> 16; \
  atex = atex_table[(ATTR_MSB >> 13) & 7]; \
  offs = (ATTR_MSB & 0x03FF) << 7 | (ATTR_MSB & 0x1800) << 6 | (LINE); \
  if(ATTR_MSB & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex); \
  atex = atex_table[(ATTR >> 13) & 7]; \
  offs = (ATTR & 0x03FF) << 7 | (ATTR & 0x1800) << 6 | (LINE); \
  if(ATTR & 0x1000) offs ^= 0x40; \
  src = (uint32 *)&bg_pattern_cache[offs]; \
  *dst++ = (*src++ | atex); \
  *dst++ = (*src++ | atex);
#endif
#endif /* ALIGN_LONG */

/*
  gcc complains about this:
    *lb++ = table[(*lb << 8) |(*src++ | palette)]; 
  .. claiming the result on lb is undefined.
  So we manually advance lb and use constant offsets into the line buffer.
*/

/* added sprite collision detection:
   check if non-transparent sprite data has been previously drawn
*/
#define DRAW_SPRITE_TILE \
	for(i=0; i<8; i++) \
	{ \
		if ((lb[i] & 0x80) && ((lb[i] | src[i]) & 0x0F)) status |= 0x20; \
		lb[i] = table[(lb[i] << 8) |(src[i] | palette)]; \
	}


/* Pixel creation macros, input is four bits each */
#ifndef NGC

/* 8:8:8 RGB */
#define MAKE_PIXEL_32(r,g,b) ((r) << 20 | (g) << 12 | (b) << 4)
           
/* 5:5:5 RGB */
#define MAKE_PIXEL_15(r,g,b) ((r) << 11 | (g) << 6 | (b) << 1)

/* 3:3:2 RGB */
#define MAKE_PIXEL_8(r,g,b)  ((r) <<  5 | (g) << 2 | ((b) >> 1))

#endif

/* 5:6:5 RGB */
#define MAKE_PIXEL_16(r,g,b) ((r) << 11 | (g) << 5 | (b))


/* Clip data */
static clip_t clip[2];

/* Attribute expansion table */
static const uint32 atex_table[] = {
  0x00000000, 0x10101010, 0x20202020, 0x30303030,
  0x40404040, 0x50505050, 0x60606060, 0x70707070
};

/* Sprite name look-up table */
static uint8 name_lut[0x400];

struct
{
  uint16 ypos;
  uint16 xpos;
  uint16 attr;
  uint8 size;
  uint8 index;
} object_info[20];

/* Pixel look-up tables and table base address */
static uint8 *lut[5];
static uint8 *lut_base = NULL;

#ifndef NGC
/* 8-bit pixel remapping data */
static uint8 pixel_8[0x100];
static uint8 pixel_8_lut[3][0x200];

/* 15-bit pixel remapping data */
static uint16 pixel_15[0x100];
static uint16 pixel_15_lut[3][0x200];

/* 32-bit pixel remapping data */
static uint32 pixel_32[0x100];
static uint32 pixel_32_lut[3][0x200];
#endif

/* 16-bit pixel remapping data */
static uint16 pixel_16[0x100];
static uint16 pixel_16_lut[3][0x200];

/* Line buffers */
static uint8 tmp_buf[0x400];	/* Temporary buffer */
static uint8 bg_buf[0x400];		/* Merged background buffer */
static uint8 nta_buf[0x400];	/* Plane A / Window line buffer */
static uint8 ntb_buf[0x400];	/* Plane B line buffer */
static uint8 obj_buf[0x400];	/* Object layer line buffer */

/* Sprite line buffer data */
static uint8 object_index_count;

/* 
   3:3:3 to 5:6:5 RGB pixel extrapolation tables 
   this is used to convert 3bits RGB values to 5bits (R,B) or 6bits (G) values
   there is three color modes:
   normal: RGB range is [0;MAX]
	 half:   RGB range is [0;MAX/2]   (shadow mode)
	 high:   RGB range is [MAX/2;MAX] (highlight mode)
   
   MAX is 31 (R,B) or 63 (G) for 5:6:5 pixels and 7 (R,G,B) for 3:3:3 pixels
   MAX/2 is rounded to inferior value (15, 31 or 3) 

   the extrapolation is linear and calculated like this:

    for (i=0; i<8; i++)
	  {
		 rgb565_norm[0][i] = round(((double)i * 31.0) / 7.0);
		 rgb565_norm[1][i] = round(((double)i * 63.0) / 7.0);
		 
		 rgb565_half[0][i] = round(((double)i * 31.0) / 7.0 / 2.0);
		 rgb565_half[1][i] = round(((double)i * 63.0) / 7.0 / 2.0);
		 
		 rgb565_high[0][i] = round(((double)i * 31.0) / 7.0 / 2.0 + 15.5);
		 rgb565_high[1][i] = round(((double)i * 63.0) / 7.0 / 2.0 + 31.5);
	  }

*/

uint8 rgb565_norm[2][8] = {{0 ,  4,  9, 13, 18, 22, 27, 31},
					     {0 ,  9, 18, 27, 36, 45, 54, 63}};
uint8 rgb565_half[2][8] = {{0 ,  2,  4,  6,  9, 11, 13, 15},
						   {0 ,  4,  9, 13, 18, 22, 27, 31}};
uint8 rgb565_high[2][8] = {{15, 17, 19, 21, 24, 26, 28, 31},
						   {31, 35, 40, 44, 49, 53, 58, 63}};


void palette_init(void)
{
	int i;

	for (i = 0; i < 0x200; i += 1)
	{
		int r, g, b;

		r = (i >> 6) & 7;
		g = (i >> 3) & 7;
		b = (i >> 0) & 7;

#ifndef NGC
		pixel_8_lut[0][i] = MAKE_PIXEL_8(r>>1,g>>1,b>>1);
		pixel_8_lut[1][i] = MAKE_PIXEL_8(r,g,b);
		pixel_8_lut[2][i] = MAKE_PIXEL_8((r>>1)|4,(g>>1)|4,(b>>1)|4);

		pixel_15_lut[0][i] = MAKE_PIXEL_15(r,g,b);
		pixel_15_lut[1][i] = MAKE_PIXEL_15(r<<1,g<<1,b<<1);
		pixel_15_lut[2][i] = MAKE_PIXEL_15(r|8,g|8,b|8);

		pixel_32_lut[0][i] = MAKE_PIXEL_32(r,g,b);
		pixel_32_lut[1][i] = MAKE_PIXEL_32(r<<1,g<<1,b<<1);
		pixel_32_lut[2][i] = MAKE_PIXEL_32(r|8,g|8,b|8);
#endif

		/* RGB 565 format: we extrapolate each 3-bit value into a 5-bit (R,B) or 6-bit (G) value 
		   this is needed to correctly cover full color range: [0-31] for R,B or [0-63] for G */
		pixel_16_lut[0][i] = MAKE_PIXEL_16(rgb565_half[0][r],rgb565_half[1][g],rgb565_half[0][b]);
		pixel_16_lut[1][i] = MAKE_PIXEL_16(rgb565_norm[0][r],rgb565_norm[1][g],rgb565_norm[0][b]);
		pixel_16_lut[2][i] = MAKE_PIXEL_16(rgb565_high[0][r],rgb565_high[1][g],rgb565_high[0][b]);
	}
}

/*--------------------------------------------------------------------------*/
/* Init, reset, shutdown routines                       */
/*--------------------------------------------------------------------------*/

int render_init (void)
{
	int bx, ax, i;

	/* Allocate and align pixel look-up tables */
	if (lut_base == NULL) lut_base = malloc ((LUT_MAX * LUT_SIZE) + LUT_SIZE);
	lut[0] = (uint8 *) (((uint32) lut_base + LUT_SIZE) & ~(LUT_SIZE - 1));
	for (i = 1; i < LUT_MAX; i += 1) lut[i] = lut[0] + (i * LUT_SIZE);

	/* Make pixel look-up table data */
	for (bx = 0; bx < 0x100; bx += 1)
		for (ax = 0; ax < 0x100; ax += 1)
		{
			uint16 index = (bx << 8) | (ax);
			lut[0][index] = make_lut_bg (bx, ax);
			lut[1][index] = make_lut_obj (bx, ax);
			lut[2][index] = make_lut_bg_ste (bx, ax);
			lut[3][index] = make_lut_obj_ste (bx, ax);
			lut[4][index] = make_lut_bgobj_ste (bx, ax);
		}

	/* Make pixel data tables */
	palette_init();

	/* Set up color update function */
#ifndef NGC
	switch(bitmap.depth)
	{
		case 8: color_update = color_update_8; break;
		case 15: color_update = color_update_15; break;
		case 16: color_update = color_update_16; break;
		case 32: color_update = color_update_32; break;
	}
#else
	color_update = color_update_16;
#endif

	/* Make sprite name look-up table */
	make_name_lut();

	return (1);
}

void make_name_lut(void)
{
	int col, row;
	int vcol, vrow;
	int width, height;
	int flipx, flipy;
	int i, name;

	memset (name_lut, 0, sizeof (name_lut));

	for (i = 0; i < 0x400; i += 1)
	{
		vcol = col = i & 3;
		vrow = row = (i >> 2) & 3;
		height = (i >> 4) & 3;
		width = (i >> 6) & 3;
		flipx = (i >> 8) & 1;
		flipy = (i >> 9) & 1;

		if(flipx) vcol = (width - col);
		if(flipy) vrow = (height - row);

		name = vrow + (vcol * (height + 1));

		if ((row > height) || col > width) name = -1;

		name_lut[i] = name;
	}
}

void render_reset(void)
{
  /* Clear display bitmap */
  memset(bitmap.data, 0, bitmap.pitch * bitmap.height);

	memset(&clip, 0, sizeof(clip));
  memset(bg_buf, 0, sizeof(bg_buf));
  memset(tmp_buf, 0, sizeof(tmp_buf));
  memset(nta_buf, 0, sizeof(nta_buf));
  memset(ntb_buf, 0, sizeof(ntb_buf));
  memset(obj_buf, 0, sizeof(obj_buf));

#ifndef NGC
	memset(&pixel_8, 0, sizeof(pixel_8));
  memset(&pixel_15, 0, sizeof(pixel_15));
  memset(&pixel_32, 0, sizeof(pixel_32));
#endif
  memset(&pixel_16, 0, sizeof(pixel_16));
}


void render_shutdown(void)
{
  if(lut_base) free(lut_base);
}

/*--------------------------------------------------------------------------*/
/* Line render function                           */
/*--------------------------------------------------------------------------*/
void remap_buffer(int line, int width)
{
  /* get line offset from framebuffer */
  int vline = (line + bitmap.viewport.y) % lines_per_frame;
    
  /* NTSC Filter */
  if (config.ntsc)
  {
    if (reg[12]&1)
    {
      if (config.render) md_ntsc_blit_double(&md_ntsc, ( MD_NTSC_IN_T const * )pixel_16, tmp_buf+0x20-bitmap.viewport.x, width, (vline * 2) + (interlaced ? odd_frame:0));
      else md_ntsc_blit(&md_ntsc, ( MD_NTSC_IN_T const * )pixel_16, tmp_buf+0x20-bitmap.viewport.x, width, vline);
    }
    else
    {
      if (config.render) sms_ntsc_blit_double(&sms_ntsc, ( SMS_NTSC_IN_T const * )pixel_16, tmp_buf+0x20-bitmap.viewport.x, width, (vline * 2) + (interlaced ? odd_frame:0));
      else sms_ntsc_blit(&sms_ntsc, ( SMS_NTSC_IN_T const * )pixel_16, tmp_buf+0x20-bitmap.viewport.x, width, vline);
    }
    return;
  }

  /* double resolution mode */
  if (config.render && interlaced) vline = (vline * 2) + odd_frame;
	
  void *out =((void *)&bitmap.data[(vline * bitmap.pitch)]);

#ifndef NGC
  switch(bitmap.depth)
  {
		case 8:
			remap_8(tmp_buf+0x20-bitmap.viewport.x, (uint8 *)out, pixel_8, width);
			break;
    case 15:
			remap_16(tmp_buf+0x20-bitmap.viewport.x, (uint16 *)out, pixel_15, width);
			break;
    case 16:
			remap_16(tmp_buf+0x20-bitmap.viewport.x, (uint16 *)out, pixel_16, width);
			break;
    case 32:
			remap_32(tmp_buf+0x20-bitmap.viewport.x, (uint32 *)out, pixel_32, width);
      break;
  }
#else
	remap_16(tmp_buf+0x20-bitmap.viewport.x, (uint16 *)out, pixel_16, width);
#endif
}


void render_line(int line, uint8 odd_frame)
{
  /* check if we are inside display area (including vertical borders) */
  int min = bitmap.viewport.h + bitmap.viewport.y;
  int max = lines_per_frame - bitmap.viewport.y;
  if ((line >= min) && (line < max)) return;

  uint8 *lb  = tmp_buf;
	int width  = bitmap.viewport.w;

	/* vertical borders or display OFF */
  if ((line >= bitmap.viewport.h) || (!(reg[1] & 0x40)))
  {
    memset(&lb[0x20], 0x40 | border, width);
	}
	else
  {
    update_bg_pattern_cache();
    window_clip(line);

    if(im2_flag)
    {
      if (clip[0].enable) render_ntx_im2(0, line, nta_buf, odd_frame);
      render_ntx_im2(1, line, ntb_buf, odd_frame);
			if (clip[1].enable) render_ntw_im2(line, nta_buf, odd_frame);
    }
    else
    {
      if(reg[11] & 4)
      {
        if (clip[0].enable) render_ntx_vs(0, line, nta_buf);
        render_ntx_vs(1, line, ntb_buf);
      }
      else
      {
        if (clip[0].enable) render_ntx(0, line, nta_buf);
        render_ntx(1, line, ntb_buf);
      }
			if (clip[1].enable) render_ntw(line, nta_buf);
    }

		if(reg[12] & 8)
    {
      merge(&nta_buf[0x20], &ntb_buf[0x20], &bg_buf[0x20], lut[2], width);
      memset(&obj_buf[0x20], 0, width);

      if(im2_flag) render_obj_im2(line, obj_buf, lut[3], odd_frame);
      else render_obj(line, obj_buf, lut[3]);

      merge(&obj_buf[0x20], &bg_buf[0x20], &lb[0x20], lut[4], width);
    }
    else
    {
      merge(&nta_buf[0x20], &ntb_buf[0x20], &lb[0x20], lut[0], width);
      if(im2_flag) render_obj_im2(line, lb, lut[1], odd_frame);
      else render_obj(line, lb, lut[1]);
    }

		  /* Mode 4 feature only (unemulated, no games rely on this) */
		  /*if(!(reg[1] & 0x04) && (reg[0] & 0x20)) memset(&lb[0x20], 0x40 | border, 0x08);*/
  }

	/* horizontal borders */
	if (config.overscan)
	{
		memset(&lb[0x20 - bitmap.viewport.x], 0x40 | border, bitmap.viewport.x);
		memset(&lb[0x20 + bitmap.viewport.w], 0x40 | border, bitmap.viewport.x);
		width += 2 * bitmap.viewport.x;
	}

  /* LightGun mark */
  if ((input.dev[4] == DEVICE_LIGHTGUN) && (config.gun_cursor))
  {
    int dy = v_counter - input.analog[0][1];

    if (abs(dy) < 6)
    {
      int i;
      int start = input.analog[0][0] - 4;
      int end = start + 8;
      if (start < 0) start = 0;
      if (end > bitmap.viewport.w) end = bitmap.viewport.w;
      for (i=start; i<end; i++)
      {
        lb[0x20+i] = 0xff;
      }
    }
  }

  remap_buffer(line,width);
}

/*--------------------------------------------------------------------------*/
/* Window rendering                             */
/*--------------------------------------------------------------------------*/

void render_ntw(int line, uint8 *buf)
{
  int column, v_line, width;
  uint32 *nt, *src, *dst, atex, atbuf;

  v_line = (line & 7) << 3;
  width = (reg[12] & 1) ? 7 : 6;
	nt = (uint32 *)&vram[ntwb | ((line >> 3) << width)];
  dst = (uint32 *)&buf[0x20 + (clip[1].left << 4)];

  for(column = clip[1].left; column < clip[1].right; column ++)
  {
    atbuf = nt[column];
    DRAW_COLUMN(atbuf, v_line)
  }
}

void render_ntw_im2(int line, uint8 *buf, uint8 odd)
{
  int column, v_line, width;
  uint32 *nt, *src, *dst, atex, atbuf, offs;

  v_line = ((line & 7) << 1 | odd) << 3;
  width = (reg[12] & 1) ? 7 : 6;
  nt = (uint32 *)&vram[ntwb | ((line >> 3) << width)];
  dst = (uint32 *)&buf[0x20 + (clip[1].left << 4)];

  for(column = clip[1].left; column < clip[1].right; column ++)
  {
    atbuf = nt[column];
    DRAW_COLUMN_IM2(atbuf, v_line)
  }
}

/*--------------------------------------------------------------------------*/
/* Background plane rendering                         */
/*--------------------------------------------------------------------------*/

void render_ntx(int which, int line, uint8 *buf)
{
  int column;
  int start, end;
  int index;
  int shift;
  int v_line;
  uint32 atex, atbuf, *src, *dst;
  uint16 xscroll = 0;
  int y_scroll;
  uint32 *nt;
  uint32 *vs;
  uint16 table[2] = {ntab,ntbb};
	uint8 xshift[2] = {0,2};
#ifdef LSB_FIRST
  uint8 vsr_shift[2] = {0,16};
#else
	uint8 vsr_shift[2] = {16,0};
#endif

  get_hscroll(line, xshift[which], &xscroll);
  shift = (xscroll & 0x0F);
  index = playfield_col_mask + 1 - ((xscroll >> 4) & playfield_col_mask);

  if(which)
  {
    start = 0;
    end = (reg[12] & 1) ? 20 : 16;
  }
  else
  {
    start = clip[0].left;
    end = clip[0].right;
    index = (index + clip[0].left) & playfield_col_mask;
  }

  vs = (uint32 *)&vsram[0];
  y_scroll = (vs[0] >> vsr_shift[which]) & 0x3FF;
	y_scroll = (line + y_scroll) & playfield_row_mask;
	v_line = (y_scroll & 7) << 3;
	nt = (uint32 *)&vram[table[which] + (((y_scroll >> 3) << playfield_shift) & y_mask)];

  if(shift)
  {
		dst = (uint32 *)&buf[0x10 + shift + (start<<4)];

		/* Window bug */
		if (start) atbuf = nt[(index) & playfield_col_mask];
		else atbuf = nt[(index-1) & playfield_col_mask];

		DRAW_COLUMN(atbuf, v_line);
  }

	dst = (uint32 *)&buf[0x20 + shift + (start<<4)];

  for(column = start; column < end; column ++, index ++)
  {
    atbuf = nt[index & playfield_col_mask];
    DRAW_COLUMN(atbuf, v_line)
  }
}

void render_ntx_im2(int which, int line, uint8 *buf, uint8 odd)
{
  int column;
  int start, end;
  int index;
  int shift;
  int v_line;
  uint32 atex, atbuf, *src, *dst;
  uint16 xscroll = 0;
  int y_scroll;
  uint32 *nt;
  uint32 *vs;
  uint32 offs;
  uint16 table[2] = {ntab,ntbb};
	uint8 xshift[2] = {0,2};
#ifdef LSB_FIRST
  uint8 vsr_shift[2] = {1,17};
#else
	uint8 vsr_shift[2] = {17,1};
#endif
  
  get_hscroll(line, xshift[which], &xscroll);
  shift = (xscroll & 0x0F);
  index = playfield_col_mask + 1 - ((xscroll >> 4) & playfield_col_mask);

  if(which)
  {
    start = 0;
    end = (reg[12] & 1) ? 20 : 16;
  }
  else
  {
    start = clip[0].left;
    end = clip[0].right;
    index = (index + clip[0].left) & playfield_col_mask;
  }

  vs = (uint32 *)&vsram[0];
	y_scroll = (vs[0] >> vsr_shift[which]) & 0x3FF;
  y_scroll = (line + y_scroll) & playfield_row_mask;
  v_line = (((y_scroll & 7) << 1) | odd) << 3;
  nt = (uint32 *)&vram[table[which] + (((y_scroll >> 3) << playfield_shift) & y_mask)];

	if(shift)
  {
		dst = (uint32 *)&buf[0x10 + shift + (start<<4)];

		/* Window bug */
		if (start) atbuf = nt[(index) & playfield_col_mask];
		else atbuf = nt[(index-1) & playfield_col_mask];
		DRAW_COLUMN_IM2(atbuf, v_line);
  }

	dst = (uint32 *)&buf[0x20 + shift + (start<<4)];
  for(column = start; column < end; column ++, index ++)
  {
    atbuf = nt[index & playfield_col_mask];
    DRAW_COLUMN_IM2(atbuf, v_line)
  }
}

void render_ntx_vs(int which, int line, uint8 *buf)
{
  int column;
  int start, end;
  int index;
  int shift;
  int v_line;
  uint32 atex, atbuf, *src, *dst;
  uint16 xscroll = 0;
  int y_scroll;
  uint32 *nt;
  uint32 *vs;
  uint16 table[2] = {ntab,ntbb};
	uint8 xshift[2] = {0,2};
#ifdef LSB_FIRST
  uint8 vsr_shift[2] = {0,16};
#else
	uint8 vsr_shift[2] = {16,0};
#endif
  
  get_hscroll(line, xshift[which], &xscroll);
  shift = (xscroll & 0x0F);
  index = playfield_col_mask + 1 - ((xscroll >> 4) & playfield_col_mask);

  if(which)
  {
    start = 0;
    end = (reg[12] & 1) ? 20 : 16;
  }
  else
  {
    start = clip[0].left;
    end = clip[0].right;
    index = (index + clip[0].left) & playfield_col_mask;
  }

  vs = (uint32 *)&vsram[0];

	if(shift)
  {
		dst = (uint32 *)&buf[0x10 + shift + (start<<4)];
		y_scroll = (line & playfield_row_mask);
		v_line = (y_scroll & 7) << 3;
		nt = (uint32 *)&vram[table[which] + (((y_scroll >> 3) << playfield_shift) & y_mask)];

		/* Window bug */
		if (start) atbuf = nt[(index) & playfield_col_mask];
		else atbuf = nt[(index-1) & playfield_col_mask];

		DRAW_COLUMN(atbuf, v_line);
  }

	dst = (uint32 *)&buf[0x20 + shift + (start<<4)];
  
  for(column = start; column < end; column ++, index ++)
  {
		y_scroll = (vs[column] >> vsr_shift[which]) & 0x3FF;
    y_scroll = (line + y_scroll) & playfield_row_mask;
    v_line = (y_scroll & 7) << 3;
    nt = (uint32 *)&vram[table[which] + (((y_scroll >> 3) << playfield_shift) & y_mask)];
    atbuf = nt[index & playfield_col_mask];
    DRAW_COLUMN(atbuf, v_line)
  }
}

/*--------------------------------------------------------------------------*/
/* Helper functions (cache update, hscroll, window clip)          */
/*--------------------------------------------------------------------------*/

void update_bg_pattern_cache(void)
{
  int i;
  uint8 x, y, c;
  uint16 name;
#ifdef LSB_FIRST
	uint8 shift_table[8] = {12, 8, 4, 0, 28, 24, 20, 16};
 #else
	uint8 shift_table[8] = {28, 24, 20, 16, 12, 8, 4, 0};
 #endif				

  if(!bg_list_index) return;

  for(i = 0; i < bg_list_index; i ++)
  {
    name = bg_name_list[i];
    bg_name_list[i] = 0;
    
    for(y = 0; y < 8; y ++)
    {
      if(bg_name_dirty[name] & (1 << y))
      {
        uint8 *dst = &bg_pattern_cache[name << 6];
        uint32 bp = *(uint32 *)&vram[(name << 5) | (y << 2)];

        for(x = 0; x < 8; x ++)
        {
					c = (bp >> shift_table[x]) & 0x0F;
					dst[0x00000 | (y << 3) | (x)] = (c);			/* hf=0, vf=0: normal */
          dst[0x20000 | (y << 3) | (x ^ 7)] = (c);		/* hf=1, vf=0: horizontal flipped */
          dst[0x40000 | ((y ^ 7) << 3) | (x)] = (c);		/* hf=0, vf=1: vertical flipped */
          dst[0x60000 | ((y ^ 7) << 3) | (x ^ 7)] = (c);	/* hf=1, vf=1: horizontal & vertical flipped */
        }
      }
    }
    bg_name_dirty[name] = 0;
  }
  bg_list_index = 0;
}

void get_hscroll(int line, int shift, uint16 *scroll)
{
  switch(reg[11] & 3)
  {
    case 0: /* Full-screen */
      *scroll = *(uint16 *)&vram[hscb + shift];
      break;

    case 1: /* First 8 lines */
      *scroll = *(uint16 *)&vram[hscb + ((line & 7) << 2) + shift];
      break;

    case 2: /* Every 8 lines */
      *scroll = *(uint16 *)&vram[hscb + ((line & ~7) << 2) + shift];
      break;

    case 3: /* Every line */
      *scroll = *(uint16 *)&vram[hscb + (line << 2) + shift];
      break;
  }

  *scroll &= 0x03FF;
}

void window_clip(int line)
{
  /* Window size and invert flags */
  int hp = (reg[17] & 0x1F);
  int hf = (reg[17] >> 7) & 1;
  int vp = (reg[18] & 0x1F) << 3;
  int vf = (reg[18] >> 7) & 1;

  /* Display size  */
  int sw = (reg[12] & 1) ? 20 : 16;

  /* Clear clipping data */
  memset(&clip, 0, sizeof(clip));

  /* Check if line falls within window range */
  if(vf == (line >= vp))
  {
    /* Window takes up entire line */
    clip[1].right = sw;
    clip[1].enable = 1;
  }
  else
  {
    /* Perform horizontal clipping; the results are applied in reverse
       if the horizontal inversion flag is set */
    int a = hf;
    int w = hf ^ 1;

    if(hp)
    {
      if(hp > sw)
      {
        /* Plane W takes up entire line */
        clip[w].right = sw;
        clip[w].enable = 1;
      }
      else
      {
        /* Window takes left side, Plane A takes right side */
        clip[w].right = hp;
        clip[a].left = hp;
        clip[a].right = sw;
        clip[0].enable = clip[1].enable = 1;
      }
    }
    else
    {
      /* Plane A takes up entire line */
      clip[a].right = sw;
      clip[a].enable = 1;
    }
  }
}


/*--------------------------------------------------------------------------*/
/* Look-up table functions (handles priority between layers pixels)     */
/*--------------------------------------------------------------------------*/

/* Input (bx):  d5-d0=color, d6=priority, d7=unused */
/* Input (ax):  d5-d0=color, d6=priority, d7=unused */
/* Output:    d5-d0=color, d6=priority, d7=unused */
int make_lut_bg(int bx, int ax)
{
  int bf, bp, b;
  int af, ap, a;
  int x = 0;
  int c;

  bf = (bx & 0x7F);
  bp = (bx >> 6) & 1;
  b  = (bx & 0x0F);
  
  af = (ax & 0x7F);   
  ap = (ax >> 6) & 1;
  a  = (ax & 0x0F);

  c = (ap ? (a ? af : (b ? bf : x)) : \
    (bp ? (b ? bf : (a ? af : x)) : \
    (   (a ? af : (b ? bf : x)) )));

  /* Strip palette bits from transparent pixels */
  if((c & 0x0F) == 0x00) c &= 0xC0;

  return (c);
}


/* Input (bx):  d5-d0=color, d6=priority, d7=sprite pixel marker */
/* Input (sx):  d5-d0=color, d6=priority, d7=unused */
/* Output:    d5-d0=color, d6=zero, d7=sprite pixel marker */
int make_lut_obj(int bx, int sx)
{
  int bf, bp, bs, b;
  int sf, sp, s;
  int c;

  bf = (bx & 0x3F);
  bs = (bx >> 7) & 1;
  bp = (bx >> 6) & 1;
  b  = (bx & 0x0F);
  
  sf = (sx & 0x3F);
  sp = (sx >> 6) & 1;
  s  = (sx & 0x0F);

  if(s == 0) return bx;

  if(bs)
  {
    c = bf; /* previous sprite has higher priority */
  }
  else
  {
    c = (sp ? (s ? sf : bf)  : \
      (bp ? (b ? bf : (s ? sf : bf)) : \
          (s ? sf : bf) ));
  }

  /* Strip palette bits from transparent pixels */
  if((c & 0x0F) == 0x00) c &= 0xC0;

  return (c | 0x80);
}


/* Input (bx):  d5-d0=color, d6=priority, d7=unused */
/* Input (sx):  d5-d0=color, d6=priority, d7=unused */
/* Output:    d5-d0=color, d6=priority, d7=intensity select (half/normal) */
int make_lut_bg_ste(int bx, int ax)
{
  int bf, bp, b;
  int af, ap, a;
  int gi;
  int x = 0;
  int c;

  bf = (bx & 0x7F);
  bp = (bx >> 6) & 1;
  b  = (bx & 0x0F);
  
  af = (ax & 0x7F);   
  ap = (ax >> 6) & 1;
  a  = (ax & 0x0F);

  gi = (ap | bp) ? 0x80 : 0x00;

  c = (ap ? (a ? af : (b ? bf : x)) :
     (bp ? (b ? bf : (a ? af : x)) : ((a ? af : (b ? bf : x)))));

  c |= gi;

  /* Strip palette bits from transparent pixels */
  if((c & 0x0F) == 0x00) c &= 0xC0;

  return (c);
}


/* Input (bx):  d5-d0=color, d6=priority, d7=sprite pixel marker */
/* Input (sx):  d5-d0=color, d6=priority, d7=unused */
/* Output:    d5-d0=color, d6=priority, d7=sprite pixel marker */
int make_lut_obj_ste(int bx, int sx)
{
  int bf, bs;
  int sf;
  int c;

  bf = (bx & 0x7F);   
  bs = (bx >> 7) & 1; 
  sf = (sx & 0x7F);

  if((sx & 0x0F) == 0) return bx;

  c = (bs) ? bf : sf;

  /* Strip palette bits from transparent pixels */
  if((c & 0x0F) == 0x00) c &= 0xC0;

  return (c | 0x80);
}


/* Input (bx):  d5-d0=color, d6=priority, d7=intensity (half/normal) */
/* Input (sx):  d5-d0=color, d6=priority, d7=sprite marker */
/* Output:    d5-d0=color, d6=intensity (half/normal), d7=(double/invalid) */
int make_lut_bgobj_ste(int bx, int sx)
{
  int c;

  int bf = (bx & 0x3F);
  int bp = (bx >> 6) & 1;
  int bi = (bx & 0x80) ? 0x40 : 0x00;
  int b  = (bx & 0x0F);

  int sf = (sx & 0x3F);
  int sp = (sx >> 6) & 1;
  int si = (sx & 0x40);
  int s  = (sx & 0x0F);

  if(bi & 0x40) si |= 0x40;

  if(sp)
  {
    if(s)
    {      
      if((sf & 0x3E) == 0x3E)
      {
        if(sf & 1)
        {
          c = (bf | 0x00);
        }
        else
        {
          c = (bx & 0x80) ? (bf | 0x80) : (bf | 0x40);
        }
      }
      else
      {
        if(sf == 0x0E || sf == 0x1E || sf == 0x2E)
        {
          c = (sf | 0x40);
        }
        else
        {
          c = (sf | si);
        }
      }
    }
    else
    {
      c = (bf | bi);
    }
  }
  else
  {
    if(bp)
    {
      if(b)
      {
        c = (bf | bi);
      }
      else
      {
        if(s)
        {
          if((sf & 0x3E) == 0x3E)
          {
            if(sf & 1)
            {
              c = (bf | 0x00);
            }
            else
            {
              c = (bx & 0x80) ? (bf | 0x80) : (bf | 0x40);
            }
          }
          else
          {
            if(sf == 0x0E || sf == 0x1E || sf == 0x2E)
            {
              c = (sf | 0x40);
            }
            else
            {
              c = (sf | si);
            }
          }
        }
        else
        {
          c = (bf | bi);
        }
      }
    }
    else
    {
      if(s)
      {
        if((sf & 0x3E) == 0x3E)
        {
          if(sf & 1)
          {
            c = (bf | 0x00);
          }
          else
          {
            c = (bx & 0x80) ? (bf | 0x80) : (bf | 0x40);
          }
        }
        else
        {
          if(sf == 0x0E || sf == 0x1E || sf == 0x2E)
          {
            c = (sf | 0x40);
          }
          else
          {
            c = (sf | si);
          }
        }
      }
      else
      {          
        c = (bf | bi);
      }
    }
  }

  if((c & 0x0f) == 0x00) c &= 0xC0;

  return (c);
}

/*--------------------------------------------------------------------------*/
/* Remap functions                              */
/*--------------------------------------------------------------------------*/
#ifndef NGC
void remap_8(uint8 *src, uint8 *dst, uint8 *table, int length)
{
  int count;
  for(count = 0; count < length; count += 1)
  {
    *dst++ = table[*src++];
  }
}

void remap_32(uint8 *src, uint32 *dst, uint32 *table, int length)
{
  int count;
  for(count = 0; count < length; count += 1)
  {
    *dst++ = table[*src++];
  }
}
#endif

void remap_16(uint8 *src, uint16 *dst, uint16 *table, int length)
{
  int count;
  for(count = 0; count < length; count += 1)
  {
    *dst++ = table[*src++];
  }
}


/*--------------------------------------------------------------------------*/
/* Merge functions                              */
/*--------------------------------------------------------------------------*/

void merge(uint8 *srca, uint8 *srcb, uint8 *dst, uint8 *table, int width)
{
  int i;
  for(i = 0; i < width; i += 1)
  {
    uint8 a = srca[i];
    uint8 b = srcb[i];
    uint8 c = table[(b << 8) | (a)];
    dst[i] = c;
  }
}

/*--------------------------------------------------------------------------*/
/* Color update functions                           */
/*--------------------------------------------------------------------------*/
#ifndef NGC

void color_update_8(int index, uint16 data)
{
  if(reg[12] & 8)
  {
    pixel_8[0x00 | index] = pixel_8_lut[0][data];
    pixel_8[0x40 | index] = pixel_8_lut[1][data];
    pixel_8[0x80 | index] = pixel_8_lut[2][data];
  }
  else
  {
    uint8 temp = pixel_8_lut[1][data];
    pixel_8[0x00 | index] = temp;
    pixel_8[0x40 | index] = temp;
    pixel_8[0x80 | index] = temp;
  }
}

void color_update_15(int index, uint16 data)
{
  if(reg[12] & 8)
  {
    pixel_15[0x00 | index] = pixel_15_lut[0][data];
    pixel_15[0x40 | index] = pixel_15_lut[1][data];
    pixel_15[0x80 | index] = pixel_15_lut[2][data];
  }
  else
  {
    uint16 temp = pixel_15_lut[1][data];
    pixel_15[0x00 | index] = temp;
    pixel_15[0x40 | index] = temp;
    pixel_15[0x80 | index] = temp;
  }
}

void color_update_32(int index, uint16 data)
{
  if(reg[12] & 8)
  {
    pixel_32[0x00 | index] = pixel_32_lut[0][data];
    pixel_32[0x40 | index] = pixel_32_lut[1][data];
    pixel_32[0x80 | index] = pixel_32_lut[2][data];
  }
  else
  {
    uint32 temp = pixel_32_lut[1][data];
    pixel_32[0x00 | index] = temp;
    pixel_32[0x40 | index] = temp;
    pixel_32[0x80 | index] = temp;
  }
}

#endif

void color_update_16(int index, uint16 data)
{
  if(reg[12] & 8)
  {
    pixel_16[0x00 | index] = pixel_16_lut[0][data];
    pixel_16[0x40 | index] = pixel_16_lut[1][data];
    pixel_16[0x80 | index] = pixel_16_lut[2][data];
  }
  else
  {
    uint16 temp = pixel_16_lut[1][data];
    pixel_16[0x00 | index] = temp;
    pixel_16[0x40 | index] = temp;
    pixel_16[0x80 | index] = temp;
  }
}

/*--------------------------------------------------------------------------*/
/* Object render functions                          */
/*--------------------------------------------------------------------------*/

void parse_satb(int line)
{
  static uint8 sizetab[] = {8, 16, 24, 32};
  uint8 link = 0;
	uint16 *p, *q;
  uint16 ypos;
	uint8 size;
  int count;
  int height;

  int limit = (reg[12] & 1) ? 20 : 16;
  int total = (reg[12] & 1) ? 80 : 64;

  object_index_count = 0;

  for(count = 0; count < total; count += 1)
  {
    q = (uint16 *) &sat[link << 3];

    ypos = q[0];
    if(im2_flag) ypos = (ypos >> 1) & 0x1FF;
    else ypos &= 0x1FF;

    size = q[1] >> 8;
		height = sizetab[size & 3];

    if((line >= ypos) && (line < (ypos + height)))
    {
      /* sprite limit (max. 16 or 20 sprites displayed per line) */
			if(object_index_count == limit)
      {
        if(vint_pending == 0) status |= 0x40;
        return;
      }

			// using xpos from internal satb stops sprite x
      // scrolling in bloodlin.bin,
      // but this seems to go against the test prog
			p = (uint16 *) &vram[satb + (link << 3)];
			object_info[object_index_count].ypos = q[0];
      object_info[object_index_count].xpos = p[3];
			object_info[object_index_count].attr = p[2];
      object_info[object_index_count].size = size;
      object_info[object_index_count].index = count;
      object_index_count += 1;
    }

    link = q[1] & 0x7F;
    if(link == 0) break;
  }
}

void render_obj(int line, uint8 *buf, uint8 *table)
{
  uint16 ypos;
  uint16 attr;
  uint16 xpos;
  uint8 sizetab[] = {8, 16, 24, 32};
  uint8 size;
  uint8 *src;

  int count;
  int pixelcount = 0;
  int width;
  int height;
  int v_line;
  int column;
  int sol_flag = 0;
  int left = 0x80;
  int right = 0x80 + bitmap.viewport.w;
	int i;

  uint8 *s, *lb;
  uint16 name, index;
  uint8 palette;

  int attr_mask, nt_row;

  for(count = 0; count < object_index_count; count += 1)
  {
    xpos = object_info[count].xpos;
    xpos &= 0x1ff;

    /* sprite masking */
		if(xpos != 0) sol_flag = 1;
		else if(xpos == 0 && sol_flag) return;

    size = object_info[count].size & 0x0f;
		width = sizetab[(size >> 2) & 3];

		/* update pixel count (off-screen sprites included) */
		pixelcount += width;
    
    if(((xpos + width) >= left) && (xpos < right))
    {
      ypos = object_info[count].ypos;
      ypos &= 0x1ff;
      attr = object_info[count].attr;
      attr_mask = (attr & 0x1800);

      height = sizetab[size & 3];
      palette = (attr >> 9) & 0x70;

      v_line = (line - ypos);
      nt_row = (v_line >> 3) & 3;
      v_line = (v_line & 7) << 3;

      name = (attr & 0x07FF);
      s = &name_lut[((attr >> 3) & 0x300) | (size << 4) | (nt_row << 2)];

      lb = (uint8 *)&buf[0x20 + (xpos - 0x80)];

			/* number of tiles to draw */
			width >>= 3;

      for(column = 0; column < width; column += 1, lb+=8)
      {
        index = attr_mask | ((name + s[column]) & 0x07FF);
        src = &bg_pattern_cache[(index << 6) | (v_line)];
        DRAW_SPRITE_TILE;
      }

    }

		/* sprite limit (256 or 320 pixels) */
		if (pixelcount >= bitmap.viewport.w) return;
     
  }
}

void render_obj_im2(int line, uint8 *buf, uint8 *table, uint8 odd)
{
  uint16 ypos;
  uint16 attr;
  uint16 xpos;
  uint8 sizetab[] = {8, 16, 24, 32};
  uint8 size;
  uint8 *src;

  int count;
  int pixelcount = 0;
  int width;
  int height;
  int v_line;
  int column;
  int sol_flag = 0;
  int left = 0x80;
  int right = 0x80 + bitmap.viewport.w;
	int i; 

  uint8 *s, *lb;
  uint16 name, index;
  uint8 palette;
  uint32 offs;

  int attr_mask, nt_row;

	for(count = 0; count < object_index_count; count += 1)
  {
    xpos = object_info[count].xpos;
		xpos &= 0x1ff;

		/* sprite masking */
		if(xpos != 0) sol_flag = 1;
		else if(xpos == 0 && sol_flag) return;

    size = object_info[count].size & 0x0f;
    width = sizetab[(size >> 2) & 3];
     
		/* update pixel count (off-screen sprites included) */
		pixelcount += width;
		
    if(((xpos + width) >= left) && (xpos < right))
    {
      ypos = object_info[count].ypos;
      ypos = (ypos >> 1) & 0x1ff;
      attr = object_info[count].attr;
      attr_mask = (attr & 0x1800);

      height = sizetab[size & 3];
      palette = (attr >> 9) & 0x70;

      v_line = (line - ypos);
      nt_row = (v_line >> 3) & 3;
      v_line = (((v_line & 7) << 1) | odd) << 3;      

      name = (attr & 0x03FF);
      s = &name_lut[((attr >> 3) & 0x300) | (size << 4) | (nt_row << 2)];

      lb = (uint8 *)&buf[0x20 + (xpos - 0x80)];

			/* number of tiles to draw */
			/* adjusted for sprite limit */
      if (pixelcount > bitmap.viewport.w) width -= (pixelcount - bitmap.viewport.w);
			width >>= 3;

      for(column = 0; column < width; column += 1, lb+=8)
      {
        index = (name + s[column]) & 0x3ff;
        offs = index << 7 | attr_mask << 6 | v_line;
        if(attr & 0x1000) offs ^= 0x40;
        src = &bg_pattern_cache[offs];
        DRAW_SPRITE_TILE;
      }
    }

		/* sprite limit (256 or 320 pixels) */
		if (pixelcount >= bitmap.viewport.w) return;
  }
}

