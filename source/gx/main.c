/****************************************************************************
 *  main.c
 *
 *  Genesis Plus GX
 *
 *  Softdev (2006)
 *  Eke-Eke (2007,2008,2009)
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
#include "gui.h"
#include "history.h"
#include "aram.h"
#include "dvd.h"

#include <fat.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

u8 Shutdown = 0;

#ifdef HW_RVL

/* Power Button callback */
static void Power_Off(void)
{
  Shutdown = 1;
  ConfigRequested = 1;
}

#endif

/***************************************************************************
 * Genesis Plus Virtual Machine
 *
 ***************************************************************************/
static void load_bios(void)
{
  /* reset BIOS found flag */
  config.bios_enabled &= ~2;

  /* open BIOS file */
  FILE *fp = fopen(OS_ROM, "rb");
  if (fp == NULL) return;

  /* read file */
  fread(bios_rom, 1, 0x800, fp);
  fclose(fp);

  /* update BIOS flags */
  config.bios_enabled |= 2;
}

static void init_machine(void)
{
  /* Allocate cart_rom here ( 10 MBytes ) */
  cart.rom = memalign(32, MAXROMSIZE);
  if (!cart.rom)
  {
    FONT_writeCenter("Failed to allocate ROM buffer... Rebooting",18,0,640,200,(GXColor)WHITE);
    gxSetScreen();
    sleep(2);
    gx_audio_Shutdown();
    gx_video_Shutdown();
#ifdef HW_RVL
    DI_Close();
    SYS_ResetSystem(SYS_RESTART,0,0);
#else
    SYS_ResetSystem(SYS_HOTRESET,0,0);
#endif
  }

  /* BIOS support */
  load_bios();

  /* allocate global work bitmap */
  memset (&bitmap, 0, sizeof (bitmap));
  bitmap.width  = 720;
  bitmap.height = 576;
  bitmap.depth  = 16;
  bitmap.granularity = 2;
  bitmap.pitch = bitmap.width * bitmap.granularity;
  bitmap.viewport.w = 256;
  bitmap.viewport.h = 224;
  bitmap.viewport.x = 0;
  bitmap.viewport.y = 0;
  bitmap.data = texturemem;
}

/**************************************************
  Load a new rom and performs some initialization
***************************************************/
void reloadrom (int size, char *name)
{
  /* cartridge hot-swap support */
  uint8 hotswap = 0;
  if (cart.romsize) hotswap = config.hot_swap;

  /* Load ROM */
  cart.romsize = size;
  load_rom(name);

  if (hotswap)
  {
    cart_hw_init();
    cart_hw_reset();
  }
  else
  {
    /* initialize audio back-end */
    /* 60hz video mode requires synchronization with Video Interrupt.    */
    /* VSYNC period is 16715 us on Wii/Gamecube (approx. 802.32 samples per frame) */
    float framerate;
    if (vdp_pal)
      framerate = 50.0;
    else
      framerate = ((config.tv_mode == 0) || (config.tv_mode == 2)) ? (1000000.0/16715.0) : 60.0;
    audio_init(48000, framerate);

    /* System Power ON */
    system_init ();
    ClearGGCodes ();
    system_reset ();
  }
}

/**************************************************
  Shutdown everything properly
***************************************************/
void shutdown(void)
{
  /* system shutdown */
  memfile_autosave(-1,config.state_auto);
  system_shutdown();
  audio_shutdown();
  free(cart.rom);
  gx_audio_Shutdown();
  gx_video_Shutdown();
#ifdef HW_RVL
  DI_Close();
#endif
}

/***************************************************************************
 *  M A I N
 *
 ***************************************************************************/
u8 fat_enabled = 0;
u32 frameticker = 0;

int main (int argc, char *argv[])
{
#ifdef HW_RVL
  /* initialize DVDX */
  DI_Close();
  DI_Init();
#endif

  int size;

  /* initialize hardware */
  gx_video_Init();
  gx_input_Init();
#ifdef HW_DOL
  DVD_Init ();
  dvd_drive_detect();
#endif

  /* initialize FAT devices */
  if (fatInitDefault())
  {
    /* check for default directories */
    DIR_ITER *dir = NULL;

    /* base directory */
    char pathname[MAXPATHLEN];
    sprintf (pathname, DEFAULT_PATH);
    dir = diropen(pathname);
    if (dir == NULL) mkdir(pathname,S_IRWXU);
    else dirclose(dir);

    /* SRAM & Savestate files directory */ 
    sprintf (pathname, "%s/saves",DEFAULT_PATH);
    dir = diropen(pathname);
    if (dir == NULL) mkdir(pathname,S_IRWXU);
    else dirclose(dir);

    /* Snapshot files directory */ 
    sprintf (pathname, "%s/snaps",DEFAULT_PATH);
    dir = diropen(pathname);
    if (dir == NULL) mkdir(pathname,S_IRWXU);
    else dirclose(dir);

    /* Cheat files directory */ 
    sprintf (pathname, "%s/cheats",DEFAULT_PATH);
    dir = diropen(pathname);
    if (dir == NULL) mkdir(pathname,S_IRWXU);
    else dirclose(dir);
  }

  /* initialize sound engine */
  gx_audio_Init();

  /* initialize core engine */
  legal();
  config_default();
  history_default();
  init_machine();

  /* run any injected rom */
  if (cart.romsize)
  {
    ARAMFetch((char *)cart.rom, (void *)0x8000, cart.romsize);
    reloadrom (cart.romsize,"INJECT.bin");
    gx_video_Start();
    gx_audio_Start();
    frameticker = 1;
  }
  else
  {
    /* Main Menu */
    ConfigRequested = 1;
  }

  /* initialize GUI engine */
  GUI_Initialize();

#ifdef HW_RVL
  /* Power button callback */
  SYS_SetPowerCallback(Power_Off);
#endif

  /* main emulation loop */
  while (1)
  {
    /* Main Menu request */
    if (ConfigRequested)
    {
      /* stop video & audio */
      gx_video_Stop();
      gx_audio_Stop();

      /* show menu */
      MainMenu ();
      ConfigRequested = 0;

      /* start video & audio */
      /* always restart video first because it setup gc_pal */
      gx_video_Start();
      gx_audio_Start();

      /* reset framesync */
      frameticker = 1;
    }

    if (frameticker > 1)
    {
      /* skip frame */
      frameticker-=2;
      system_frame (1);
    }
    else
    {
      /* framesync */
      while (frameticker < 1) usleep(1);
      frameticker--;

      /* render frame */
      system_frame (0);
    }

    /* retrieve audio samples */
    size = audio_update();

    /* update video & audio */
    gx_video_Update();
    gx_audio_Update(size * 4);
  }

  return 0;
}
