/****************************************************************************
 *  gx_video.c
 *
 *  Genesis Plus GX video & rendering support
 *
 *  code by Softdev (2006), Eke-Eke (2007,2009)
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
#include "sms_ntsc.h"
#include "aram.h"
#include "md_ntsc.h"
#include "sms_ntsc.h"

#include <png.h>

#define TEX_WIDTH         720
#define TEX_HEIGHT        576
#define TEX_SIZE          (TEX_WIDTH * TEX_HEIGHT * 2)
#define DEFAULT_FIFO_SIZE 256 * 1024
#define HASPECT           320
#define VASPECT           240

/* libpng wrapper */
typedef struct
{
  u8 *buffer;
  u32 offset;
} png_file;

extern const u8 Crosshair_p1_png[];
extern const u8 Crosshair_p2_png[];

/*** VI ***/
unsigned int *xfb[2];  /* External Framebuffers */
int whichfb = 0;       /* Current Framebuffer   */
GXRModeObj *vmode;     /* Default Video Mode    */
u8 *texturemem;        /* Texture Data          */

/* 50/60hz flag */
u8 gc_pal = 0;

/*** NTSC Filters ***/
sms_ntsc_t sms_ntsc;
md_ntsc_t md_ntsc;
static sms_ntsc_setup_t sms_setup;
static md_ntsc_setup_t md_setup;

/*** GX FIFO ***/
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);

/*** custom Video modes ***/
static GXRModeObj *rmode;

/*** GX Textures ***/
static u32 vwidth,vheight;
static gx_texture *crosshair[2];

/* 288 lines progressive (PAL 50Hz) */
static GXRModeObj TV50hz_288p = 
{
  VI_TVMODE_PAL_DS,             // viDisplayMode
  640,                          // fbWidth
  286,                          // efbHeight
  286,                          // xfbHeight
  0,                            // viXOrigin
  (VI_MAX_HEIGHT_PAL - 572)/2,  // viYOrigin
  VI_MAX_WIDTH_PAL,             // viWidth
  572,                          // viHeight
  VI_XFBMODE_SF,                // xFBmode
  GX_FALSE,                     // field_rendering
  GX_FALSE,                     // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
    0,         // line n-1
    0,         // line n-1
    21,        // line n
    22,        // line n
    21,        // line n
    0,         // line n+1
    0          // line n+1
  }
};

/* 288 lines interlaced (PAL 50Hz) */
static GXRModeObj TV50hz_288i = 
{
  VI_TVMODE_PAL_INT,            // viDisplayMode
  640,                          // fbWidth
  286,                          // efbHeight
  286,                          // xfbHeight
  0,                            // viXOrigin
  (VI_MAX_HEIGHT_PAL - 572)/2,  // viYOrigin
  VI_MAX_WIDTH_PAL,             // viWidth
  572,                          // viHeight
  VI_XFBMODE_SF,                // xFBmode
  GX_TRUE,                      // field_rendering
  GX_FALSE,                     // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
    0,         // line n-1
    0,         // line n-1
    21,        // line n
    22,        // line n
    21,        // line n
    0,         // line n+1
    0          // line n+1
  }
};

/* 576 lines interlaced (PAL 50Hz, scaled) */
static GXRModeObj TV50hz_576i = 
{
  VI_TVMODE_PAL_INT,  // viDisplayMode
  640,                // fbWidth
  480,                // efbHeight
  VI_MAX_HEIGHT_PAL,  // xfbHeight
  0,                  // viXOrigin
  0,                  // viYOrigin
  VI_MAX_WIDTH_PAL,   // viWidth
  VI_MAX_HEIGHT_PAL,  // viHeight
  VI_XFBMODE_DF,      // xFBmode
  GX_FALSE,           // field_rendering
  GX_FALSE,           // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
    8,         // line n-1
    8,         // line n-1
    10,        // line n
    12,        // line n
    10,        // line n
    8,         // line n+1
    8          // line n+1
  }
};

/* 240 lines progressive (NTSC or PAL 60Hz) */
static GXRModeObj TV60hz_240p = 
{
  VI_TVMODE_EURGB60_DS, // viDisplayMode
  640,                  // fbWidth
  VI_MAX_HEIGHT_NTSC/2, // efbHeight
  VI_MAX_HEIGHT_NTSC/2, // xfbHeight
  0,                    // viXOrigin
  0,                    // viYOrigin
  VI_MAX_WIDTH_NTSC,    // viWidth
  VI_MAX_HEIGHT_NTSC,   // viHeight
  VI_XFBMODE_SF,        // xFBmode
  GX_FALSE,             // field_rendering
  GX_FALSE,             // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
    0,         // line n-1
    0,         // line n-1
    21,        // line n
    22,        // line n
    21,        // line n
    0,         // line n+1
    0          // line n+1
  }
};

/* 240 lines interlaced (NTSC or PAL 60Hz) */
static GXRModeObj TV60hz_240i = 
{
    VI_TVMODE_EURGB60_INT,  // viDisplayMode
    640,                    // fbWidth
    VI_MAX_HEIGHT_NTSC/2,   // efbHeight
    VI_MAX_HEIGHT_NTSC/2,   // xfbHeight
    0,                      // viXOrigin
    0,                      // viYOrigin
    VI_MAX_WIDTH_NTSC,      // viWidth
    VI_MAX_HEIGHT_NTSC,     // viHeight
    VI_XFBMODE_SF,          // xFBmode
    GX_TRUE,                // field_rendering
    GX_FALSE,               // aa

  // sample points arranged in increasing Y order
  {
    {3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {3,2},{9,6},{3,10},  // pix 1
    {9,2},{3,6},{9,10},  // pix 2
    {9,2},{3,6},{9,10}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
    0,         // line n-1
    0,         // line n-1
    21,        // line n
    22,        // line n
    21,        // line n
    0,         // line n+1
    0          // line n+1
  }
};

/* 480 lines interlaced (NTSC or PAL 60Hz) */
static GXRModeObj TV60hz_480i = 
{
  VI_TVMODE_EURGB60_INT,// viDisplayMode
  640,                  // fbWidth
  VI_MAX_HEIGHT_NTSC,   // efbHeight
  VI_MAX_HEIGHT_NTSC,   // xfbHeight
  0,                    // viXOrigin
  0,                    // viYOrigin
  VI_MAX_WIDTH_NTSC,    // viWidth
  VI_MAX_HEIGHT_NTSC,   // viHeight
  VI_XFBMODE_DF,        // xFBmode
  GX_FALSE,             // field_rendering
  GX_FALSE,             // aa

  // sample points arranged in increasing Y order
  {
    {6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
    {6,6},{6,6},{6,6},  // pix 1
    {6,6},{6,6},{6,6},  // pix 2
    {6,6},{6,6},{6,6}   // pix 3
  },

  // vertical filter[7], 1/64 units, 6 bits each
  {
    8,         // line n-1
    8,         // line n-1
    10,        // line n
    12,        // line n
    10,        // line n
    8,         // line n+1
    8          // line n+1
  }
};

/* TV modes pointer table */
static GXRModeObj *tvmodes[6] =
{
   /* 60hz modes */
   &TV60hz_240p,
   &TV60hz_240i,
   &TV60hz_480i,

   /* 50Hz modes */
   &TV50hz_288p,
   &TV50hz_288i,
   &TV50hz_576i   
};

typedef struct tagcamera
{
  Vector pos;
  Vector up;
  Vector view;
} camera;

/*** Square Matrix
     This structure controls the size of the image on the screen.
   Think of the output as a -80 x 80 by -60 x 60 graph.
***/
static s16 square[] ATTRIBUTE_ALIGN (32) =
{
  /*
   * X,   Y,  Z
   * Values set are for roughly 4:3 aspect
   */
  -HASPECT,  VASPECT, 0,  // 0
   HASPECT,  VASPECT, 0,  // 1
   HASPECT, -VASPECT, 0,  // 2
  -HASPECT, -VASPECT, 0,  // 3
};

static camera cam = {
  {0.0F, 0.0F, -100.0F},
  {0.0F, -1.0F, 0.0F},
  {0.0F, 0.0F, 0.0F}
};

static void updateFrameCount(u32 cnt)
{
  frameticker++;
}

/* Vertex Rendering */
static inline void draw_vert(u8 pos, f32 s, f32 t)
{
  GX_Position1x8(pos);
  GX_TexCoord2f32(s, t);
}

/* textured quad rendering */
static inline void draw_square(void)
{
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
  draw_vert(3, 0.0, 0.0);
  draw_vert(2, 1.0, 0.0);
  draw_vert(1, 1.0, 1.0);
  draw_vert(0, 0.0, 1.0);
  GX_End ();
}

/* Initialize GX renderer */
static void gxStart(void)
{
  /*** Clear out FIFO area ***/
  memset(&gp_fifo, 0, DEFAULT_FIFO_SIZE);

  /*** GX default ***/
  GX_Init(&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode(GX_CULL_NONE);
  GX_SetDispCopyGamma(GX_GM_1_0);
  GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
  GX_SetColorUpdate(GX_TRUE);
  GX_SetAlphaUpdate(GX_FALSE);

  /* Modelview */
  Mtx view;
  memset (&view, 0, sizeof (Mtx));
  guLookAt(view, &cam.pos, &cam.up, &cam.view);
  GX_LoadPosMtxImm(view, GX_PNMTX0);
  GX_Flush();
}

/* Reset GX rendering */
static void gxResetRendering(u8 type)
{
  GX_ClearVtxDesc();

  if (type)
  {
    /* uses direct positionning, alpha blending & color channel (menu rendering) */
    GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxDesc (GX_VA_CLR0, GX_DIRECT);
    /* 
       Color.out = Color.rasterized*Color.texture
       Alpha.out = Alpha.rasterized*Alpha.texture 
    */
    GX_SetTevOp (GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetNumTexGens(1);
    GX_SetNumChans(1);
  }
  else
  {
    /* uses array positionning, no alpha blending, no color channel (video emulation) */
    GX_SetBlendMode(GX_BM_NONE,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetArray(GX_VA_POS, square, 3 * sizeof (s16));
    /* 
       Color.out = Color.texture
       Alpha.out = Alpha.texture 
    */
    GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
    GX_SetNumTexGens(1);
    GX_SetNumChans(0);
  }

  GX_Flush();
}

/* Reset GX 2D rendering */
static void gxResetView(GXRModeObj *tvmode)
{
  Mtx44 p;
  f32 yScale = GX_GetYScaleFactor(tvmode->efbHeight, tvmode->xfbHeight);
  u16 xfbHeight = GX_SetDispCopyYScale(yScale);

  GX_SetCopyClear((GXColor)BLACK,0x00ffffff);
  GX_SetViewport(0.0F, 0.0F, tvmode->fbWidth, tvmode->efbHeight, 0.0F, 1.0F);
  GX_SetScissor(0, 0, tvmode->fbWidth, tvmode->efbHeight);
  GX_SetDispCopySrc(0, 0, tvmode->fbWidth, tvmode->efbHeight);
  GX_SetDispCopyDst(tvmode->fbWidth, xfbHeight);
  GX_SetCopyFilter(tvmode->aa, tvmode->sample_pattern, (tvmode->xfbMode == VI_XFBMODE_SF) ? GX_FALSE : GX_TRUE, tvmode->vfilter);
  GX_SetFieldMode(tvmode->field_rendering, ((tvmode->viHeight == 2 * tvmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  guOrtho(p, tvmode->efbHeight/2, -(tvmode->efbHeight/2), -(tvmode->fbWidth/2), tvmode->fbWidth/2, 100, 1000);
  GX_LoadProjectionMtx(p, GX_ORTHOGRAPHIC);
  GX_Flush();
}

/* Reset GX/VI scaler */
static void gxResetScale(u32 width, u32 height)
{
  int temp = 0;
  int xscale, yscale, xshift, yshift;

  /* aspect Ratio (depends on current configuration) */
  if (config.aspect)
  {
    /* original aspect ratio */
    /* the following values have been detected from comparison with a real 50/60hz Mega Drive */
    if (config.overscan)
    {
      /* borders are emulated */
      xscale = 358 + ((reg[12] & 1)*2) - gc_pal;
      yscale = vdp_pal + ((gc_pal && !config.render) ? 143 : 120);
    }
    else
    {
      /* borders are simulated (black) */
      xscale = 325 + ((reg[12] & 1)*2) - gc_pal;
      yscale = bitmap.viewport.h / 2;
      if (vdp_pal && (!gc_pal || config.render)) yscale = yscale * 240 / 288;
      else if (!vdp_pal && gc_pal && !config.render) yscale = yscale * 288 / 240;
    }

    xshift = config.xshift;
    yshift = 2 - vdp_pal + 2*(gc_pal & !config.render) + config.yshift;
  }
  else
  {
    /* manual aspect ratio (default is fullscreen) */
    if (config.overscan)
    {
      /* borders are emulated */
      xscale = 352;
      yscale = (gc_pal && !config.render) ? (vdp_pal ? (268*144 / bitmap.viewport.h):143) : (vdp_pal ? (224*144 / bitmap.viewport.h):120);
    }
    else
    {
      /* borders are simulated (black) */
      xscale = 320;
      yscale = (gc_pal && !config.render) ? 134 : 112;
    }

    /* user scaling */
    xscale += config.xscale;
    yscale += config.yscale;

    xshift = config.xshift;
    yshift = config.yshift;
  }

  /* double resolution modes */
  if (config.render)
  {
    yscale *= 2;
    yshift *= 2;
  }

  /* GX scaler (by default, use EFB maximal width) */
  rmode->fbWidth = 640;
  if (!config.bilinear && !config.ntsc)
  {
    /* filtering (soft or hard) is disabled, let VI handles horizontal scaling */
    /* if possible, let GX simply doubles the width, otherwise disable GX stretching completely */
    if ((width * 2) <= 640) rmode->fbWidth = width * 2; 
    else if (width <= 640) rmode->fbWidth = width;
  }

  /* horizontal scaling (GX/VI) */
  if (xscale > (rmode->fbWidth/2))
  {
    /* max width = 720 pixels */
    if (xscale > 360)
    {
      /* save offset for later */
      temp = xscale - 360;
      xscale = 360;
    }

    /* enable VI scaler */
    rmode->viWidth = xscale * 2;
    rmode->viXOrigin = (720 - (xscale * 2)) / 2;

    /* set GX scaling to max EFB width */
    xscale = temp + (rmode->fbWidth/2);
  }
  else
  {
    /* disable VI scaler */
    rmode->viWidth = rmode->fbWidth;
    rmode->viXOrigin = (720 - rmode->fbWidth) / 2;
  }

  /* update GX scaler (Vertex Position Matrix) */
  square[6] = square[3]  =  xscale + xshift;
  square[0] = square[9]  = -xscale + xshift;
  square[4] = square[1]  =  yscale + yshift;
  square[7] = square[10] =  -yscale + yshift;
  DCFlushRange(square, 32);
  GX_InvVtxCache();
}

static void gxDrawCrosshair(gx_texture *texture, int x, int y)
{
  if (texture->data)
  {
    /* load texture object */
    GXTexObj texObj;
    GX_InitTexObj(&texObj, texture->data, texture->width, texture->height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texObj,GX_LINEAR,GX_LIN_MIP_LIN,0.0,10.0,0.0,GX_FALSE,GX_TRUE,GX_ANISO_4);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    /* reset GX rendering */
    gxResetRendering(1);

    /* adjust coordinate system */
    x = ((x * rmode->fbWidth) / bitmap.viewport.w) - (texture->width/2) - (rmode->fbWidth/2) + (rmode->viWidth-rmode->fbWidth)/2;
    y = ((y * rmode->efbHeight) / bitmap.viewport.h) - (config.render ? (texture->height/2) : (texture->height/4)) - (rmode->efbHeight/2) + (rmode->xfbHeight-rmode->efbHeight)/2;;
    int w = texture->width - (rmode->viWidth-rmode->fbWidth);
    int h = (config.render ? texture->height : (texture->height/2)) - (rmode->xfbHeight-rmode->efbHeight);

    /* Draw textured quad */
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(x,y+h);
    GX_Color4u8(0xff,0xff,0xff,0xff);
    GX_TexCoord2f32(0.0, 1.0);
    GX_Position2s16(x+w,y+h);
    GX_Color4u8(0xff,0xff,0xff,0xff);
    GX_TexCoord2f32(1.0, 1.0);
    GX_Position2s16(x+w,y);
    GX_Color4u8(0xff,0xff,0xff,0xff);
    GX_TexCoord2f32(1.0, 0.0);
    GX_Position2s16(x,y);
    GX_Color4u8(0xff,0xff,0xff,0xff);
    GX_TexCoord2f32(0.0, 0.0);
    GX_End ();
    GX_DrawDone();

    /* restore GX rendering */
    gxResetRendering(0);

    /* restore texture object */
    GXTexObj texobj;
    GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    if (!config.bilinear) GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,0.0,10.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    GX_LoadTexObj(&texobj, GX_TEXMAP0);
    GX_InvalidateTexAll();
  }
}

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
gx_texture *gxTextureOpenPNG(const u8 *buffer)
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

#if 0
  /* ensure PNG images are in the supported format */
  u32 bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  u32 color_type = png_get_color_type(png_ptr, info_ptr);

  /* support for RGBA8 textures ONLY !*/
  if ((color_type != PNG_COLOR_TYPE_RGB_ALPHA) || (bit_depth != 8))
  {
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return;
  }

  /* 4x4 tiles are required */
  if ((width%4) || (height%4))
  {
    png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
    return;
  }
#endif

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
  gx_texture *texture = (gx_texture *)memalign(32, sizeof(gx_texture));
  if (!texture)
  {
    free (img_data);
    return NULL;
  }

  /* initialize texture data */
  texture->data = memalign(32, stride * height);
  if (!texture->data)
  {
    free (img_data);
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

void gxTextureClose(gx_texture **p_texture)
{
  gx_texture *texture = *p_texture;

  if (texture)
  {
    if (texture->data) free(texture->data);
    free(texture);
    *p_texture = NULL;
  }
}

void gxDrawScreenshot(u8 alpha)
{
  if (rmode)
  {
    GXTexObj texobj;
    GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_LoadTexObj(&texobj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    /* retrieve current xscale/xshift values */
    s32 xscale = (rmode->viWidth + square[6] - square[0] - rmode->fbWidth) / 2 - (vmode->viWidth - 640)/2;
    s32 xshift = (square[6] + square[0]) / 2;

    /* apply current position/size */
    s32 x = xshift - xscale;
    s32 y = square[7];
    s32 w = xscale * 2;
    s32 h = square[4] - square[7];
    if (rmode->efbHeight < 480)
    {
      y = y * 2;
      h = h * 2;
    }

    /* Draw textured quad */
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(x,y+h);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(0.0, 1.0);
    GX_Position2s16(x+w,y+h);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(1.0, 1.0);
    GX_Position2s16(x+w,y);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(1.0, 0.0);
    GX_Position2s16(x,y);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(0.0, 0.0);
    GX_End ();
    GX_DrawDone();
  }
}

void gxDrawTexture(gx_texture *texture, s32 x, s32 y, s32 w, s32 h, u8 alpha)
{
  if (!texture) return;
  if (texture->data)
  {
    /* load texture object */
    GXTexObj texObj;
    GX_InitTexObj(&texObj, texture->data, texture->width, texture->height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&texObj,GX_LINEAR,GX_LIN_MIP_LIN,0.0,10.0,0.0,GX_FALSE,GX_TRUE,GX_ANISO_4); /* does this really change anything ? */
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    /* vertex coordinate */
    x -= (vmode->fbWidth/2);
    y -= (vmode->efbHeight/2);

    /* draw textured quad */
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(x,y+h);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(0.0, 1.0);
    GX_Position2s16(x+w,y+h);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(1.0, 1.0);
    GX_Position2s16(x+w,y);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(1.0, 0.0);
    GX_Position2s16(x,y);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(0.0, 0.0);
    GX_End ();
    GX_DrawDone();
  }
}

void gxDrawTextureRepeat(gx_texture *texture, s32 x, s32 y, s32 w, s32 h, u8 alpha)
{
  if (!texture) return;
  if (texture->data)
  {
    /* load texture object */
    GXTexObj texObj;
    GX_InitTexObj(&texObj, texture->data, texture->width, texture->height, GX_TF_RGBA8, GX_MIRROR, GX_MIRROR, GX_FALSE);
    GX_LoadTexObj(&texObj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    /* vertex coordinate */
    x -= (vmode->fbWidth/2);
    y -= (vmode->efbHeight/2);

    /* texture coordinates */
    f32 s = (f32)w / (f32)texture->width;
    f32 t = (f32)h / (f32)texture->height;

    /* draw textured quad */
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(x,y+h);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(0.0, t);
    GX_Position2s16(x+w,y+h);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(s, t);
    GX_Position2s16(x+w,y);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(s, 0.0);
    GX_Position2s16(x,y);
    GX_Color4u8(0xff,0xff,0xff,alpha);
    GX_TexCoord2f32(0.0, 0.0);
    GX_End ();
    GX_DrawDone();
  }
}

void gxResetAngle(f32 angle)
{
  Mtx view;

  if (angle)
  {
    Mtx m,m1;
    Vector axis = (Vector) {0,0,1};
    guLookAt(m, &cam.pos, &cam.up, &cam.view);
    guMtxRotAxisDeg (m1, &axis, angle);
    guMtxConcat(m,m1,view);
  }
  else
  {
    guLookAt(view, &cam.pos, &cam.up, &cam.view);
  }
  
  GX_LoadPosMtxImm(view, GX_PNMTX0);
  GX_Flush();
}

GXColor BACKGROUND = {0xd4,0xd0,0xc8,0xff};

void gxSetScreen ()
{
  GX_CopyDisp(xfb[whichfb], GX_FALSE);
  GX_Flush();
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();
}

void gxClearScreen (GXColor color)
{
  whichfb ^= 1;
  GX_SetCopyClear(color,0x00ffffff);
  GX_CopyDisp(xfb[whichfb], GX_TRUE);
  GX_Flush();
}

/* Restore Menu Video mode */
void gx_video_stop(void)
{
  /* lightgun textures */
  if (crosshair[0])
  {
    if (crosshair[0]->data) free(crosshair[0]->data);
    free(crosshair[0]);
    crosshair[0] = NULL;
  }
  if (crosshair[1])
  {
    if (crosshair[1]->data) free(crosshair[1]->data);
    free(crosshair[1]);
    crosshair[1] = NULL;
  }

  /* reset GX */
  gxResetRendering(1);
  gxResetView(vmode);

  /* reset VI */
  gxDrawScreenshot(0xff);
  VIDEO_Configure(vmode);
  VIDEO_SetPreRetraceCallback(NULL);
  VIDEO_SetPostRetraceCallback(gx_input_updateMenu);
  gxSetScreen ();
}

/* Update Video settings */
void gx_video_start(void)
{
  /* 50Hz/60Hz mode */
  if ((config.tv_mode == 1) || ((config.tv_mode == 2) && vdp_pal)) gc_pal = 1;
  else gc_pal = 0;

  /* Video Interrupt synchronization */
  VIDEO_SetPostRetraceCallback(NULL);
  if (!gc_pal && !vdp_pal) VIDEO_SetPreRetraceCallback(updateFrameCount);
  VIDEO_Flush();

  /* interlaced/progressive mode */
  if (config.render == 2)
  {
    tvmodes[2]->viTVMode = VI_TVMODE_NTSC_PROG;
    tvmodes[2]->xfbMode = VI_XFBMODE_SF;
  }
  else if (config.render == 1)
  {
    tvmodes[2]->viTVMode = tvmodes[0]->viTVMode & ~3;
    tvmodes[2]->xfbMode = VI_XFBMODE_DF;
  }

  /* software NTSC filters */
  if (config.ntsc == 1)
  {
    sms_setup = sms_ntsc_composite;
    md_setup  = md_ntsc_composite;
    sms_ntsc_init( &sms_ntsc, &sms_setup );
    md_ntsc_init( &md_ntsc, &md_setup );
  }
  else if (config.ntsc == 2)
  {
    sms_setup = sms_ntsc_svideo;
    md_setup  = md_ntsc_svideo;
    sms_ntsc_init( &sms_ntsc, &sms_setup );
    md_ntsc_init( &md_ntsc, &md_setup );
  }
  else if (config.ntsc == 3)
  {
    sms_setup = sms_ntsc_rgb;
    md_setup  = md_ntsc_rgb;
    sms_ntsc_init( &sms_ntsc, &sms_setup );
    md_ntsc_init( &md_ntsc, &md_setup );
  }

  /* lightgun textures */
  if ((input.system[1] == SYSTEM_MENACER) || (input.system[1] == SYSTEM_JUSTIFIER))
  {
    if (config.gun_cursor)
    {
      if (input.dev[4] == DEVICE_LIGHTGUN) crosshair[0] = gxTextureOpenPNG(Crosshair_p1_png);
      if (input.dev[5] == DEVICE_LIGHTGUN) crosshair[1] = gxTextureOpenPNG(Crosshair_p2_png);
    }
  }

  /* apply changes on next video update */
  bitmap.viewport.changed = 1;

  /* reset GX rendering */
  gxResetRendering(0);

}


/* GX render update */
void gx_video_update(void)
{
  /* check if display has changed */
  if (bitmap.viewport.changed)
  {
    /* update texture size */
    vwidth  = bitmap.viewport.w + 2 * bitmap.viewport.x;
    vheight = bitmap.viewport.h + 2 * bitmap.viewport.y;

    /* special cases */
    if (config.render && interlaced) vheight = vheight << 1;
    if (config.ntsc) vwidth = (reg[12]&1) ? MD_NTSC_OUT_WIDTH(vwidth) : SMS_NTSC_OUT_WIDTH(vwidth);

    /* texels size must be multiple of 4 */
    vwidth  = (vwidth  >> 2) << 2;
    vheight = (vheight >> 2) << 2;

    /* initialize texture object */
    GXTexObj texobj;
    GX_InitTexObj(&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

    /* configure texture filtering */
    if (!config.bilinear)
    {
      GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,0.0,10.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    }

    /* load texture object */
    GX_LoadTexObj(&texobj, GX_TEXMAP0);

    /* reset TV mode */
    if (config.render) rmode = tvmodes[gc_pal*3 + 2];
    else rmode = tvmodes[gc_pal*3 + interlaced];

    /* reset aspect ratio */
    gxResetScale(vwidth,vheight);

    /* reset GX */
    gxResetView(rmode);
  }

  /* texture is now directly mapped by the line renderer */

  /* force texture cache update */
  DCFlushRange(texturemem, TEX_SIZE);
  GX_InvalidateTexAll();

  /* render textured quad */
  draw_square();
  GX_DrawDone();

  /* LightGun marks */
  if (crosshair[0]) gxDrawCrosshair(crosshair[0], input.analog[0][0],input.analog[0][1]);
  if (crosshair[1]) gxDrawCrosshair(crosshair[1], input.analog[1][0],input.analog[1][1]);

  /* swap XFB */
  whichfb ^= 1;

  /* reconfigure VI */
  if (bitmap.viewport.changed)
  {
    bitmap.viewport.changed = 0;

    /* change VI mode */
    VIDEO_Configure(rmode);
    VIDEO_Flush();

    /* copy EFB to XFB */
    GX_CopyDisp(xfb[whichfb], GX_TRUE);
    GX_Flush();
    VIDEO_SetNextFramebuffer(xfb[whichfb]);
    VIDEO_Flush();

    /* field synchronizations */
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
    else while (VIDEO_GetNextField() != odd_frame) VIDEO_WaitVSync();
    if (frameticker > 1) frameticker = 1;
  }
  else
  {
    /* copy EFB to XFB */
    GX_CopyDisp(xfb[whichfb], GX_TRUE);
    GX_Flush();
    VIDEO_SetNextFramebuffer(xfb[whichfb]);
    VIDEO_Flush();
  }
}

/* Initialize VIDEO subsystem */
void gx_video_init(void)
{
  /*
   * Before doing anything else under libogc,
   * Call VIDEO_Init
   */
  VIDEO_Init();

  /*
   * Before any memory is allocated etc.
   * Rescue any tagged ROM in data 2
   */
  int *romptr = (int *)0x80700000;
  StartARAM();
  genromsize = 0;
  if (memcmp((char *)romptr,"GENPLUSR",8) == 0)
  {
    genromsize = romptr[2];
    ARAMPut((char *) 0x80700000 + 0x20, (char *) 0x8000, genromsize);
  }

  /* Get the current VIDEO mode then :
      - set menu VIDEO mode (480p, 480i or 576i)
      - set emulator rendering TV modes (PAL/MPAL/NTSC/EURGB60)
   */
  vmode = VIDEO_GetPreferredMode(NULL);

  /* Adjust display settings */
  switch (vmode->viTVMode >> 2)
  {
    case VI_PAL:  /* 576 lines (PAL 50Hz) */

      TV60hz_240p.viTVMode = VI_TVMODE_EURGB60_DS;
      TV60hz_240i.viTVMode = VI_TVMODE_EURGB60_INT;
      TV60hz_480i.viTVMode = VI_TVMODE_EURGB60_INT;
      config.tv_mode = 1;

      /* use harwdare vertical scaling to fill screen */
      vmode = &TVPal574IntDfScale;
      break;
    
    case VI_NTSC: /* 480 lines (NTSC 60hz) */
      TV60hz_240p.viTVMode = VI_TVMODE_NTSC_DS;
      TV60hz_240i.viTVMode = VI_TVMODE_NTSC_INT;
      TV60hz_480i.viTVMode = VI_TVMODE_NTSC_INT;
      config.tv_mode = 0;

#ifndef HW_RVL
      /* force 480p on NTSC GameCube if the Component Cable is present */
      if (VIDEO_HaveComponentCable()) vmode = &TVNtsc480Prog;
#endif
      break;

    default:  /* 480 lines (PAL 60Hz) */
      TV60hz_240p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
      TV60hz_240i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
      TV60hz_480i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
      config.tv_mode = 2;
      break;
  }

  /* adjust overscan */
  vmode->viWidth    = 658;
  vmode->viXOrigin  = (VI_MAX_WIDTH_NTSC - 658)/2;
#ifdef HW_RVL
  if (CONF_GetAspectRatio()) 
  {
    vmode->viWidth    = 672;
    vmode->viXOrigin  = (VI_MAX_WIDTH_NTSC - 672)/2;
  }
#endif

  /* Configure VI */
  VIDEO_Configure (vmode);

  /* Configure the framebuffers (double-buffering) */
  xfb[0] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(&TV50hz_576i));
  xfb[1] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(&TV50hz_576i));

  /* Define a console */
  console_init(xfb[0], 20, 64, 640, 574, 574 * 2);

  /* Clear framebuffers to black */
  VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

  /* Set the framebuffer to be displayed at next VBlank */
  VIDEO_SetNextFramebuffer(xfb[0]);

  /* Enable Video Interface */
  VIDEO_SetBlack(FALSE);

  /* Update VIDEO settings for next VBlank */
  VIDEO_Flush();

  /* Wait for VBlank */
  VIDEO_WaitVSync();
  VIDEO_WaitVSync();

  /* Initialize GX */
  gxStart();
  gxResetRendering(1);
  gxResetView(vmode);

  /* Initialize texture data */
  texturemem = memalign(32, TEX_SIZE);
  if (!texturemem)
  {
    WaitPrompt("Failed to allocate texture buffer... Rebooting");
#ifdef HW_RVL
    DI_Close();
    SYS_ResetSystem(SYS_RESTART,0,0);
#else
    SYS_ResetSystem(SYS_HOTRESET,0,0);
#endif
  }
  memset (texturemem, 0, TEX_SIZE);
}
