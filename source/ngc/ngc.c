/****************************************************************************
 *  Genesis Plus 1.2a
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald
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
 ***************************************************************************/
#include "shared.h"
#include "gcaram.h"
#include "dvd.h"
#include "font.h"

#define ROMOFFSET 0x80600000

unsigned char *gen_bmp;	/*** Work bitmap ***/
int frameticker = 0;
int ConfigRequested = 0;
int padcal = 70;
int RenderedFrameCount = 0;
int FrameCount = 0;
int FramesPerSecond = 0;
u8 isWII   = 0;

/***************************************************************************
 * Nintendo Gamecube Hardware Specific Functions
 *
 * T I M E R
 ***************************************************************************/
#define TB_CLOCK  40500000
#define mftb(rval) ({unsigned long u; do { \
         asm volatile ("mftbu %0" : "=r" (u)); \
         asm volatile ("mftb %0" : "=r" ((rval)->l)); \
         asm volatile ("mftbu %0" : "=r" ((rval)->u)); \
         } while(u != ((rval)->u)); })

typedef struct
{
	unsigned long l, u;
} tb_t;

unsigned long tb_diff_msec(tb_t *end, tb_t *start)
{
        unsigned long upper, lower;
        upper = end->u - start->u;
        if (start->l > end->l) upper--;
        lower = end->l - start->l;
        return ((upper*((unsigned long)0x80000000/(TB_CLOCK/2000))) + (lower/(TB_CLOCK/1000)));
}

int msBetweenFrames = 20;
tb_t now, prev;


/***************************************************************************
 * Nintendo Gamecube Hardware Specific Functions
 *
 * V I D E O
 ***************************************************************************/
/*** 2D Video ***/
unsigned int *xfb[2];	/*** Double buffered ***/
int whichfb = 0;		/*** Switch ***/
GXRModeObj *vmode;		/*** General video mode ***/

/*** GX ***/
#define TEX_WIDTH 320
#define TEX_HEIGHT 256
#define DEFAULT_FIFO_SIZE 256 * 1024

static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN (32);
static u8 texturemem[TEX_WIDTH * (TEX_HEIGHT + 8) * 2] ATTRIBUTE_ALIGN (32);
GXTexObj texobj;
static Mtx view;
int vwidth, vheight, oldvwidth, oldvheight;

/* New texture based scaler */
#define HASPECT 76
#define VASPECT 54

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
s16 square[] ATTRIBUTE_ALIGN (32) =
{
	/*
	 * X,   Y,  Z
	 * Values set are for roughly 4:3 aspect
	 */
	-HASPECT, VASPECT, 0,	// 0
	HASPECT, VASPECT, 0,	// 1
	HASPECT, -VASPECT, 0,	// 2
	-HASPECT, -VASPECT, 0,	// 3
};

static camera cam = { {0.0F, 0.0F, 0.0F},
{0.0F, 0.5F, 0.0F},
{0.0F, 0.0F, -0.5F}
};

/*** Framestart function
	 Simply increment the tick counter
 ***/
static void framestart()
{
	frameticker++;
}

/*** WIP3 - Scaler Support Functions
 ***/
static void draw_init (void)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));
  GX_SetNumTexGens (1);
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_InvalidateTexAll ();
  GX_InitTexObj (&texobj, texturemem, vwidth, vheight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
}

static void draw_vert (u8 pos, u8 c, f32 s, f32 t)
{
  GX_Position1x8 (pos);
  GX_Color1x8 (c);
  GX_TexCoord2f32 (s, t);
}

static void draw_square (Mtx v)
{
  Mtx m;			// model matrix.
  Mtx mv;			// modelview matrix.

  guMtxIdentity (m);
  guMtxTransApply (m, m, 0, 0, -100);
  guMtxConcat (v, m, mv);
  GX_LoadPosMtxImm (mv, GX_PNMTX0);
  GX_Begin (GX_QUADS, GX_VTXFMT0, 4);
  draw_vert (0, 0, 0.0, 0.0);
  draw_vert (1, 0, 1.0, 0.0);
  draw_vert (2, 0, 1.0, 1.0);
  draw_vert (3, 0, 0.0, 1.0);
  GX_End ();
}

/*** StartGX
	 This function initialises the GX.
     WIP3 - Based on texturetest from libOGC examples.
 ***/
static void StartGX (void)
{
  Mtx p;
  GXColor gxbackground = { 0, 0, 0, 0xff };

	/*** Clear out FIFO area ***/
  memset (&gp_fifo, 0, DEFAULT_FIFO_SIZE);

	/*** Initialise GX ***/
  GX_Init (&gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear (gxbackground, 0x00ffffff);
  GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
  GX_SetScissor (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopySrc (0, 0, vmode->fbWidth, vmode->efbHeight);
  GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
  GX_SetCopyFilter (vmode->aa, vmode->sample_pattern, GX_TRUE, vmode->vfilter);
  GX_SetFieldMode (vmode->field_rendering, ((vmode->viHeight == 2 * vmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetCullMode (GX_CULL_NONE);
  GX_CopyDisp (xfb[whichfb ^ 1], GX_TRUE);
  GX_SetDispCopyGamma (GX_GM_1_0);
  guPerspective (p, 60, 1.33F, 10.0F, 1000.0F);
  GX_LoadProjectionMtx (p, GX_PERSPECTIVE);
  memset (texturemem, 0, TEX_WIDTH * TEX_HEIGHT * 2);
  vwidth = 100;
  vheight = 100;
}

/*** InitGCVideo
	 This function MUST be called at startup.
 ***/
static void InitGCVideo ()
{
  int *romptr = (int *)ROMOFFSET;

  /*
   * Before doing anything else under libogc,
   * Call VIDEO_Init
   */
  VIDEO_Init ();

  /*
   * Before any memory is allocated etc.
   * Rescue any tagged ROM in data 2
   */
  StartARAM();
  if ( memcmp((char *)romptr,"GENPLUSR",8) == 0 )
  {
	  genromsize = romptr[2];
	  ARAMPut ((char *) 0x80600020, (char *) 0x8000, genromsize);
  }
  else genromsize = 0;

  /* Init Gamepads */
  PAD_Init ();

  /*
   * Reset the video mode
   * This is always set to 60hz
   * Whether your running PAL or NTSC
   */
  vmode = &TVNtsc480IntDf;
  VIDEO_Configure (vmode);

  /*** Now configure the framebuffer. 
	   Really a framebuffer is just a chunk of memory
	   to hold the display line by line.
   **/
  xfb[0] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));

  /*** I prefer also to have a second buffer for double-buffering.
	   This is not needed for the console demo.
   ***/
  xfb[1] = (u32 *) MEM_K0_TO_K1((u32 *) SYS_AllocateFramebuffer(vmode));

  /*** Define a console ***/
  console_init(xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

  /*** Clear framebuffer to black ***/
  VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

  /*** Set the framebuffer to be displayed at next VBlank ***/
  VIDEO_SetNextFramebuffer(xfb[0]);

  /*** Increment frameticker and timer ***/
  VIDEO_SetPreRetraceCallback(framestart);

  /*** Get the PAD status updated by libogc ***/
  VIDEO_SetPostRetraceCallback(PAD_ScanPads);
  VIDEO_SetBlack (FALSE);
  
  /*** Update the video for next vblank ***/
  VIDEO_Flush();

  /*** Wait for VBL ***/
  VIDEO_WaitVSync();
  if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

  DVD_Init ();
  SDCARD_Init ();
  unpackBackdrop ();
  init_font();
  StartGX ();

  /* Wii drive detection for 4.7Gb support */
  int driveid = dvd_inquiry();
  if ((driveid == 4) || (driveid == 6) || (driveid == 8)) isWII = 0;
  else isWII = 1;
}

/*** Video Update
     called after each emulation frame
 ***/
static void update_video ()
{
  int h, w;
  vwidth = (reg[12] & 1) ? 320 : 256;
  vheight = (reg[1] & 8) ? 240 : 224;

  long long int *dst = (long long int *)texturemem;
  long long int *src1 = (long long int *)(bitmap.data + 64);
  long long int *src2 = src1 + 256;
  long long int *src3 = src2 + 256;
  long long int *src4 = src3 + 256;
  long long int stride = 1024 - ( vwidth >> 2 );
 
  whichfb ^= 1;

  if ((oldvheight != vheight) || (oldvwidth != vwidth))
  {
	  /** Update scaling **/
      oldvwidth = vwidth;
      oldvheight = vheight;
      draw_init ();
      memset (&view, 0, sizeof (Mtx));
	  guLookAt(view, &cam.pos, &cam.up, &cam.view);
      GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
  }

  GX_InvVtxCache ();
  GX_InvalidateTexAll ();
  GX_SetTevOp (GX_TEVSTAGE0, GX_DECAL);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

  for (h = 0; h < vheight; h += 4)
  {
    for (w = 0; w < (vwidth >> 2); w++ )
	{
		*dst++ = *src1++;
		*dst++ = *src2++;
		*dst++ = *src3++;
		*dst++ = *src4++;
	}

    src1 += stride;
	src2 += stride;
	src3 += stride;
	src4 += stride;
  }

  DCFlushRange (texturemem, TEX_WIDTH * TEX_HEIGHT * 2);
  GX_SetNumChans (1);
  GX_LoadTexObj (&texobj, GX_TEXMAP0);
  draw_square (view);
  GX_DrawDone ();
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetColorUpdate (GX_TRUE);
  GX_CopyDisp (xfb[whichfb], GX_TRUE);
  GX_Flush ();
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
}

/***************************************************************************
 * Nintendo Gamecube Hardware Specific Functions
 *
 * A U D I O
 ***************************************************************************/
unsigned char soundbuffer[16][3840] ATTRIBUTE_ALIGN(32);
int mixbuffer = 0;
int playbuffer = 0;
int IsPlaying = 0;

/*** AudioSwitchBuffers
     Genesis Plus only provides sound data on completion of each frame.
     To try to make the audio less choppy, this function is called from both the
     DMA completion and update_audio.
     Testing for data in the buffer ensures that there are no clashes.
 ***/
static void AudioSwitchBuffers()
{
	u32 dma_len = (vdp_pal) ? 3840 : 3200;

	if ( !ConfigRequested )
	{
		AUDIO_InitDMA((u32) soundbuffer[playbuffer], dma_len);
		DCFlushRange(soundbuffer[playbuffer], dma_len);
		AUDIO_StartDMA();
		playbuffer++;
		playbuffer &= 0xf;
		if ( playbuffer == mixbuffer ) playbuffer--;
		if ( playbuffer < 0 ) playbuffer = 15;
		IsPlaying = 1;
	}
	else IsPlaying = 0;
}

/*** InitGCAudio
     Stock code to set the DSP at 48Khz
 ***/
static void InitGCAudio ()
{
  AUDIO_Init (NULL);
  AUDIO_SetDSPSampleRate (AI_SAMPLERATE_48KHZ);
  AUDIO_RegisterDMACallback (AudioSwitchBuffers);
  memset(soundbuffer, 0, 16 * 3840);
}

/*** Audio Update
     called after each emulation frame
 ***/
static void update_audio ()
{
  if (IsPlaying == 0) AudioSwitchBuffers ();
}

/***************************************************************************
 * Nintendo Gamecube Hardware Specific Functions
 *
 * I N P U T
 ***************************************************************************/
/**
 * IMPORTANT
 * If you change the padmap here, be sure to update confjoy to
 * reflect the changes - or confusion will ensue!
 *
 * DEFAULT MAPPING IS:
 *		Genesis			Gamecube
 *		  A			   B
 *		  B		           A
 *		  C			   X
 *		  X			   LT
 *		  Y			   Y
 *		  Z			   RT
 *
 * Mode is unused, as it's our menu hotkey for now :)
 * Also note that libOGC has LT/RT reversed - it's not a typo!
 */
unsigned int gppadmap[] = { INPUT_A, INPUT_B, INPUT_C,
  INPUT_X, INPUT_Y, INPUT_Z,
  INPUT_UP, INPUT_DOWN,
  INPUT_LEFT, INPUT_RIGHT,
  INPUT_START, INPUT_MODE
};

unsigned short gcpadmap[] = { PAD_BUTTON_B, PAD_BUTTON_A, PAD_BUTTON_X,
  PAD_TRIGGER_L, PAD_BUTTON_Y, PAD_TRIGGER_R,
  PAD_BUTTON_UP, PAD_BUTTON_DOWN,
  PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT,
  PAD_BUTTON_START, PAD_TRIGGER_Z
};

static unsigned int DecodeJoy (unsigned short p)
{
  unsigned int J = 0;
  int i;

  for (i = 0; i < 12; i++) if (p & gcpadmap[i]) J |= gppadmap[i];
  return J;
}

static unsigned int GetAnalog (int Joy)
{
  signed char x, y;
  unsigned int i = 0;

  x = PAD_StickX (Joy);
  y = PAD_StickY (Joy);
  if (x > padcal)  i |= INPUT_RIGHT;
  if (x < -padcal) i |= INPUT_LEFT;
  if (y > padcal)  i |= INPUT_UP;
  if (y < -padcal) i |= INPUT_DOWN;
  return i;
}

/*** Inputs Update
     called before each emulation frame
 ***/
static void update_inputs()
{
	int i = 0;
	int joynum = 0;

	/*** Check for SOFT-RESET combo ***/
	if ((PAD_ButtonsHeld (0) & PAD_TRIGGER_Z) &&
		(PAD_ButtonsHeld (0) & PAD_TRIGGER_L))
    {
	  m68k_pulse_reset ();
      return;
    }

	/*** Check for menu combo ***/
	if (PAD_ButtonsHeld (0) & PAD_TRIGGER_Z)
    {
      ConfigRequested = 1;
      return;
    }

	for (i=0; i<MAX_DEVICES; i++)
	{
		input.pad[i] = 0;
		if (input.dev[i] != NO_DEVICE)
		{
			input.pad[i] = DecodeJoy(PAD_ButtonsHeld (joynum));
			input.pad[i] |= GetAnalog (joynum);
			joynum ++;
			if (input.dev[i] == DEVICE_LIGHTGUN) lightgun_set();
		}
	}
}

/***************************************************************************
 * General Genesis Plus Support
 *
 ***************************************************************************/
void error (char *format, ...)
{
  /* Function does nothing, but must exist! */
  return;
}

/*** init_machine
	 Initialise the Genesis VM
 ***/
static void init_machine ()
{
  /*** Allocate cart_rom here ***/
  cart_rom = malloc(0x500000 + 32);
  if ((u32)cart_rom & 0x1f) cart_rom += 32 - ((u32)cart_rom & 0x1f);
  memset(cart_rom, 0, 0x500000);

  /*** Fetch from ARAM any linked rom ***/
  if (genromsize) ARAMFetch(cart_rom, (void *)0x8000, genromsize);
  
  /*** Allocate global work bitmap ***/
  memset (&bitmap, 0, sizeof (bitmap));
  bitmap.data = malloc (1024 * 512 * 2);
  bitmap.width = 1024;
  bitmap.height = 512;
  bitmap.depth = 16;
  bitmap.granularity = 2;
  bitmap.pitch = (bitmap.width * bitmap.granularity);
  bitmap.viewport.w = 256;
  bitmap.viewport.h = 224;
  bitmap.viewport.x = 0x20;
  bitmap.viewport.y = 0;
  bitmap.remap = 1;

  /* default inputs */
  input.system[0] = SYSTEM_GAMEPAD;
  input.system[1] = SYSTEM_GAMEPAD;
}

/***************************************************************************
 *  M A I N
 *
 ***************************************************************************/
extern void legal ();
extern void MainMenu ();
extern void reloadrom ();

int main (int argc, char *argv[])
{
  genromsize = 0;

  /* Initialize GC System */
  InitGCVideo ();
  InitGCAudio ();

  init_machine ();
  legal();

  if (genromsize)
  {
	  reloadrom ();
	  MainMenu();
  }
  else while (!genromsize) MainMenu();

  /* Main emulation loop */
  frameticker = 0;
  mftb(&prev);

  while (1)
  {
      /* Update all inputs */
      update_inputs();

      FrameCount++;

      if (vdp_pal) /* PAL 50Hz (use timer) */
      {
		mftb(&now);
		if (tb_diff_msec(&now, &prev) > msBetweenFrames)
		{
		  memcpy(&prev, &now, sizeof(tb_t));
          system_frame(1);
		}
		else
		{
		  /*** Delay ***/
		  while (tb_diff_msec(&now, &prev) < msBetweenFrames) mftb(&now);
		  memcpy(&prev, &now, sizeof(tb_t) );
		  system_frame(0);
		  RenderedFrameCount++;
		}
	  }
      else /* NTSC 60Hz (use vsync) */
      {
		  while ( frameticker < 1 ) usleep(10);	
		  
		  /** Simulate a frame **/
		  if (frameticker > 1)
	      {
			  frameticker--;
			  if (frameticker > 5)
			  {
				  system_frame (0);
				  RenderedFrameCount++;
				  frameticker = 1;
			  }
			  else system_frame (1);
		  }
          else
	      {
			  system_frame (0);
			  RenderedFrameCount++;
	      }
      }

	  frameticker--;

      /** Draw the frame **/
      update_video ();

      /** add the audio **/
      update_audio ();

      /** Check render frames **/
      if ((FrameCount == vdp_rate))
	  {
	    FramesPerSecond = RenderedFrameCount;
	    RenderedFrameCount = 0;
	    FrameCount = 0;
	  }

      if (ConfigRequested)
	  {
	    AUDIO_StopDMA ();
		IsPlaying = mixbuffer = playbuffer = 0;
	    MainMenu ();
	    ConfigRequested = 0;
	  }
  }
  return 0;
}
