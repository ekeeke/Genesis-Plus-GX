/******************************************************************************
 *
 *  Genesis Plus - Sega Megadrive / Genesis Emulator
 *
 *  NGC/Wii Video support
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
 ***************************************************************************/

#include "shared.h"
#include "font.h"
#include "gcaram.h"

/*** PAL 50hz flag ***/
uint8 gc_pal = 0;

/*** VI ***/
unsigned int *xfb[2];	/*** Double buffered            ***/
int whichfb = 0;		  /*** External framebuffer index ***/
GXRModeObj *vmode;    /*** Menu video mode            ***/

/*** GX ***/
#define TEX_WIDTH         356
#define TEX_HEIGHT        576
#define DEFAULT_FIFO_SIZE 256 * 1024
#define HASPECT           320
#define VASPECT           240

static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static u8 texturemem[TEX_WIDTH * (TEX_HEIGHT + 8) * 2] ATTRIBUTE_ALIGN (32);
static GXTexObj texobj;
static Mtx view;
static int vwidth, vheight;
static long long int stride;

/*** custom Video modes (used to emulate original console video modes) ***/
/* 288 lines progressive (PAL 50Hz) */
GXRModeObj TV50hz_288p = 
{
  VI_TVMODE_PAL_DS,       // viDisplayMode
  640,             // fbWidth
  286,             // efbHeight
  286,             // xfbHeight
  (VI_MAX_WIDTH_PAL - 720)/2,         // viXOrigin
  (VI_MAX_HEIGHT_PAL - 572)/2,        // viYOrigin
  720,             // viWidth
  572,             // viHeight
  VI_XFBMODE_SF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

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
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

/* 288 lines interlaced (PAL 50Hz) */
GXRModeObj TV50hz_288i = 
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    286,             // efbHeight
    286,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 720)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 572)/2,        // viYOrigin
    720,             // viWidth
    572,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_FALSE,        // aa

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
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

/* 576 lines interlaced (PAL 50Hz, scaled) */
GXRModeObj TV50hz_576i = 
{
  VI_TVMODE_PAL_INT,      // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  574,             // xfbHeight
  (VI_MAX_WIDTH_PAL - 720)/2,         // viXOrigin
  (VI_MAX_HEIGHT_PAL - 574)/2,        // viYOrigin
  720,             // viWidth
  574,             // viHeight
  VI_XFBMODE_DF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

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
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

/* 240 lines progressive (NTSC or PAL 60Hz) */
GXRModeObj TV60hz_240p = 
{
  VI_TVMODE_EURGB60_DS,      // viDisplayMode
  640,             // fbWidth
  240,             // efbHeight
  240,             // xfbHeight
  (VI_MAX_WIDTH_NTSC - 720)/2,        // viXOrigin
  (VI_MAX_HEIGHT_NTSC/2 - 480/2)/2,       // viYOrigin
  720,             // viWidth
  480,             // viHeight
  VI_XFBMODE_SF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

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
		 21,         // line n
		 22,         // line n
		 21,         // line n
		  0,         // line n+1
		  0          // line n+1
	}
};

/* 240 lines interlaced (NTSC or PAL 60Hz) */
GXRModeObj TV60hz_240i = 
{
    VI_TVMODE_EURGB60_INT,     // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 720)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    720,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_FALSE,        // aa

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
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

/* 480 lines interlaced (NTSC or PAL 60Hz) */
GXRModeObj TV60hz_480i = 
{
  VI_TVMODE_EURGB60_INT,     // viDisplayMode
  640,             // fbWidth
  480,             // efbHeight
  480,             // xfbHeight
  (VI_MAX_WIDTH_NTSC - 720)/2,        // viXOrigin
  (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
  720,             // viWidth
  480,             // viHeight
  VI_XFBMODE_DF,   // xFBmode
  GX_FALSE,        // field_rendering
  GX_FALSE,        // aa

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
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

/* TV Modes table */
GXRModeObj *tvmodes[6] = {
	 &TV60hz_240p, &TV60hz_240i, &TV60hz_480i,  /* 60hz modes */
	 &TV50hz_288p, &TV50hz_288i, &TV50hz_576i   /* 50Hz modes */
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
	-HASPECT,  VASPECT, 0,	// 0
   HASPECT,  VASPECT, 0,	// 1
	 HASPECT, -VASPECT, 0,	// 2
	-HASPECT, -VASPECT, 0,	// 3
};

static camera cam = {
  {0.0F, 0.0F, -100.0F},
  {0.0F, -1.0F, 0.0F},
  {0.0F, 0.0F, 0.0F}
};

/* rendering initialization */
/* should be called each time you change quad aspect ratio */
static void draw_init(void)
{
  /* Clear all Vertex params */
  GX_ClearVtxDesc ();

  /* Set Position Params (set quad aspect ratio) */
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));

  /* Set Tex Coord Params */
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetTevOp (GX_TEVSTAGE0, GX_REPLACE);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
  GX_SetNumTexGens (1);
  GX_SetNumChans(0);

  /** Set Modelview **/
  memset (&view, 0, sizeof (Mtx));
  guLookAt(view, &cam.pos, &cam.up, &cam.view);
  GX_LoadPosMtxImm (view, GX_PNMTX0);
}

/* vertex rendering */
static void draw_vert(u8 pos, f32 s, f32 t)
{
  GX_Position1x8 (pos);
  GX_TexCoord2f32 (s, t);
}

/* textured quad rendering */
static void draw_square (void)
{
  GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
  draw_vert (3, 0.0, 0.0);
  draw_vert (2, 1.0, 0.0);
  draw_vert (1, 1.0, 1.0);
  draw_vert (0, 0.0, 1.0);
  GX_End ();
}

/* retrace handler */
static void framestart(u32 retraceCnt)
{
  /* simply increment the tick counter */
  frameticker++;
}

static void gxStart(void)
{
  Mtx p;
  GXColor gxbackground = { 0, 0, 0, 0xff };

  /*** Clear out FIFO area ***/
  memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

  /*** GX default ***/
  GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear (gxbackground, 0x00ffffff);

  GX_SetViewport (0.0F, 0.0F, vmode->fbWidth, vmode->efbHeight, 0.0F, 1.0F);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  f32 yScale = GX_GetYScaleFactor(vmode->efbHeight, vmode->xfbHeight);
  u16 xfbHeight = GX_SetDispCopyYScale (yScale);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
  GX_SetDispCopyGamma (GX_GM_1_0);
  GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_TRUE);
  GX_SetColorUpdate (GX_TRUE);
  guOrtho(p, vmode->efbHeight/2, -(vmode->efbHeight/2), -(vmode->fbWidth/2), vmode->fbWidth/2, 100, 1000);
  GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);

  /*** reset XFB ***/
  GX_CopyDisp (xfb[whichfb ^ 1], GX_TRUE);

  /*** Initialize texture data ***/
  memset (texturemem, 0, TEX_WIDTH * TEX_HEIGHT * 2);
}

/* set GX scaler */
static void gxScale(void)
{
	int xscale, yscale, xshift, yshift, i;
  
  /* borders are emulated */
  if (config.overscan)
	{
    if (config.aspect)
    {
		  xscale = (reg[12] & 1) ? 360 : 358;
      if (gc_pal) xscale -= 1;
      yscale = (gc_pal && !config.render) ? (vdp_pal ? 144:143) : (vdp_pal ? 121:120);
    }
    else
    {
      /* fullscreen stretch */
      xscale = bitmap.viewport.w + 2*bitmap.viewport.x;
      yscale = (gc_pal && !config.render) ? (vdp_pal ? (268*144 / bitmap.viewport.h):143) : (vdp_pal ? (224*144 / bitmap.viewport.h):120);
    }
    
		xshift = (config.aspect || !gc_pal) ? 8 : 4;
		yshift = vdp_pal ? 1 : 3;
	}
  else
	{
    if (config.aspect)
    {
      /* borders are simulated (black) */
	    xscale = 327;
      yscale = bitmap.viewport.h / 2;
		  if (vdp_pal && (!gc_pal || config.render)) yscale = yscale * 243 / 288;
		  else if (!vdp_pal && gc_pal && !config.render) yscale = yscale * 288 / 243;
		}
    else
    {
      /* fit screen */
      xscale = 320;
      yscale = (gc_pal && !config.render) ? 134 : 112;
    }
    
		xshift = config.aspect ? 8 : 0;
		yshift = vdp_pal ? 0 : 2;
	}
	
  if (!config.aspect)
	{
    xscale += config.xscale;
    yscale += config.yscale;
  }
  
  xshift += config.xshift;
  yshift += config.yshift;
  
  /* check horizontal upscaling */
  if (xscale > 320)
  {
    /* let VI do horizontal scaling */
    uint32 scale = (xscale <= 360) ? xscale : 360;
    for (i=0; i<6; i++)
    {
      tvmodes[i]->viXOrigin = (720 - (scale * 2)) / 2;
      tvmodes[i]->viWidth   = scale * 2;
    }
    xscale -= (scale - 320);
  }
  else
  {
    /* let GX do horizontal downscaling */
    for (i=0; i<6; i++)
    {
      tvmodes[i]->viXOrigin = 40;
      tvmodes[i]->viWidth   = 640;
    }
  }

	/* double resolution */
	if (config.render)
	{
		 yscale *= 2;
		 yshift *= 2;
	}

  /* update matrix */
	square[6] = square[3]  =  xscale + xshift;
	square[0] = square[9]  = -xscale + xshift;
	square[4] = square[1]  =  yscale + yshift;
	square[7] = square[10] = -yscale + yshift;
	draw_init();

  /* vertex array have been modified */
  GX_InvVtxCache ();

}	

/* Reinitialize GX */
void ogc_video__reset()
{
	GXRModeObj *rmode;
	Mtx p;

  /* reinitialize GC/Wii PAL mode */
  if ((config.tv_mode == 1) || ((config.tv_mode == 2) && vdp_pal)) gc_pal = 1;
  else gc_pal = 0;

  /* reset scaler */
  gxScale();

  /* reinitialize current TV mode */
  if (config.render == 2)
  {
    tvmodes[2]->viTVMode = VI_TVMODE_NTSC_PROG;
    tvmodes[2]->xfbMode = VI_XFBMODE_SF;
  }
  else
  {
    tvmodes[2]->viTVMode = tvmodes[0]->viTVMode & ~3;
    tvmodes[2]->xfbMode = VI_XFBMODE_DF;
  }

	if (config.render) rmode = tvmodes[gc_pal*3 + 2];
	else rmode = tvmodes[gc_pal*3 + interlaced];


	VIDEO_Configure (rmode);
	VIDEO_ClearFrameBuffer(rmode, xfb[whichfb], COLOR_BLACK);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	else while (VIDEO_GetNextField())  VIDEO_WaitVSync();
	odd_frame = 1;

  /* reset rendering mode */
  GX_SetViewport (0.0F, 0.0F, rmode->fbWidth, rmode->efbHeight, 0.0F, 1.0F);
  GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
  f32 yScale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
  u16 xfbHeight = GX_SetDispCopyYScale (yScale);
  GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopyDst (rmode->fbWidth, xfbHeight);
  GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, config.render ? GX_TRUE : GX_FALSE, rmode->vfilter);
  GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  guOrtho(p, rmode->efbHeight/2, -(rmode->efbHeight/2), -(rmode->fbWidth/2), rmode->fbWidth/2, 100, 1000);
  GX_LoadProjectionMtx (p, GX_ORTHOGRAPHIC);
}

/* GX render update */
void ogc_video__update()
{
  int h, w;
  
  /* texture and bitmap buffers (buffers width is fixed to 360 pixels) */
  long long int *dst = (long long int *)texturemem;
  long long int *src1 = (long long int *)(bitmap.data); /* line n */
  long long int *src2 = src1 + 90;  /* line n+1 */
  long long int *src3 = src2 + 90;  /* line n+2 */
  long long int *src4 = src3 + 90;  /* line n+3 */

  /* check if viewport has changed */
  if (bitmap.viewport.changed)
  {
    /* Check interlaced mode changes */
    if ((bitmap.viewport.changed & 2) && (!config.render))
    {
      GXRModeObj *rmode;
	    rmode = tvmodes[gc_pal*3 + interlaced];
      VIDEO_Configure (rmode);
      GX_SetFieldMode (rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
      VIDEO_WaitVSync();
      if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
      else while (VIDEO_GetNextField()) VIDEO_WaitVSync();
    }
    
    bitmap.viewport.changed = 0;
    
    /* update texture size */
    vwidth  = bitmap.viewport.w + 2 * bitmap.viewport.x;
    vheight = bitmap.viewport.h + 2 * bitmap.viewport.y;
    if (interlaced && config.render) vheight *= 2;
    stride = bitmap.width - (vwidth >> 2);
    
    /* reset GX scaler */
    gxScale();
    
    /* reinitialize texture */
    GX_InvalidateTexAll ();
	  GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    
    /* original H40 mode: force filtering OFF */
    if (!config.render && (reg[12]&1))
    {
      GX_InitTexObjLOD(&texobj,GX_NEAR,GX_NEAR_MIP_NEAR,2.5,9.0,0.0,GX_FALSE,GX_FALSE,GX_ANISO_1);
    }
  }

  GX_InvalidateTexAll ();
 
  /* update texture data */
  for (h = 0; h < vheight; h += 4)
  {
    for (w = 0; w < (vwidth >> 2); w++)
    {
      *dst++ = *src1++;
      *dst++ = *src2++;
      *dst++ = *src3++;
      *dst++ = *src4++;
    }
    
    /* jump to next four lines */
    src1 += stride;
    src2 += stride;
    src3 += stride;
    src4 += stride;
  }

  /* load texture into GX */
  DCFlushRange (texturemem, vwidth * vheight * 2);
  GX_LoadTexObj (&texobj, GX_TEXMAP0);
  
  /* render textured quad */
  draw_square ();
  GX_DrawDone ();

  /* switch external framebuffers then copy EFB to XFB */
  whichfb ^= 1;
  GX_CopyDisp (xfb[whichfb], GX_TRUE);
  GX_Flush ();

  /* set next XFB */
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
}

/* Initialize VIDEO subsystem */
void ogc_video__init(void)
{
  /*
   * Before doing anything else under libogc,
   * Call VIDEO_Init
   */
  VIDEO_Init ();

  /*
   * Before any memory is allocated etc.
   * Rescue any tagged ROM in data 2
   */
  int *romptr = (int *)0x80700000;
  StartARAM();
  genromsize = 0;
  if ( memcmp((char *)romptr,"GENPLUSR",8) == 0 )
  {
    genromsize = romptr[2];
    ARAMPut ((char *) 0x80700000 + 0x20, (char *) 0x8000, genromsize);
  }

  /* Get the current video mode then :
      - set menu video mode (fullscreen, 480i or 576i)
      - set emulator rendering TV modes (PAL/MPAL/NTSC/EURGB60)
   */
  vmode = VIDEO_GetPreferredMode(NULL);

  /* adjust display settings */
  switch (vmode->viTVMode >> 2)
  {
    case VI_PAL:  /* 576 lines (PAL 50Hz) */

      TV60hz_240p.viTVMode = VI_TVMODE_EURGB60_DS;
      TV60hz_240i.viTVMode = VI_TVMODE_EURGB60_INT;
      TV60hz_480i.viTVMode = VI_TVMODE_EURGB60_INT;
      config.tv_mode = 1;
      gc_pal = 1;

      /* display should be centered vertically (borders) */
      vmode = &TVPal574IntDfScale;
      vmode->xfbHeight = 480;
      vmode->viYOrigin = (VI_MAX_HEIGHT_PAL - 480)/2;
      vmode->viHeight = 480;

      break;
    
    case VI_NTSC: /* 480 lines (NTSC 60hz) */
      TV60hz_240p.viTVMode = VI_TVMODE_NTSC_DS;
      TV60hz_240i.viTVMode = VI_TVMODE_NTSC_INT;
      TV60hz_480i.viTVMode = VI_TVMODE_NTSC_INT;
      config.tv_mode = 0;
	    gc_pal = 0;
      break;

    default:  /* 480 lines (PAL 60Hz) */
      TV60hz_240p.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_NON_INTERLACE);
      TV60hz_240i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
      TV60hz_480i.viTVMode = VI_TVMODE(vmode->viTVMode >> 2, VI_INTERLACE);
      config.tv_mode = 2;
  	  gc_pal = 0;
      break;
  }
   
  /* configure video mode */
  VIDEO_Configure (vmode);

  /* Configure the framebuffers (double-buffering) */
  xfb[0] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(&TV50hz_576i));
  xfb[1] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(&TV50hz_576i));

  /* Define a console */
  console_init(xfb[0], 20, 64, 640, 574, 574 * 2);

  /* Clear framebuffers to black */
  VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);

  /* Set the framebuffer to be displayed at next VBlank */
  VIDEO_SetNextFramebuffer (xfb[0]);

  /* Register Video Retrace handlers */
  VIDEO_SetPreRetraceCallback(framestart);

  /* Enable Video Interface */
  VIDEO_SetBlack (FALSE);
  
  /* Update video settings for next VBlank */
  VIDEO_Flush ();

  /* Wait for VBlank */
  VIDEO_WaitVSync();
  VIDEO_WaitVSync();

  /* initialize GUI */
  unpackBackdrop ();
  init_font();

  /* Initialize GX */
  gxStart();
}
