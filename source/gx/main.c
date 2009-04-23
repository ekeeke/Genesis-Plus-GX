/****************************************************************************
 *  main.c
 *
 *  Genesis Plus GX main
 *
 *  code by Softdev (2006), Eke-Eke (2007,2008)
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
#include "menu.h"
#include "history.h"
#include "aram.h"
#include "dvd.h"

#include <fat.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <di/di.h>
#endif

#ifdef HW_RVL

/* Power Button callback */
u8 Shutdown = 0;
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
  char pathname[MAXPATHLEN];

  /* reset BIOS found flag */
  config.bios_enabled &= ~2;

  /* open BIOS file */
  sprintf (pathname, "%s/BIOS.bin",DEFAULT_PATH);
  FILE *fp = fopen(pathname, "rb");
  if (fp == NULL) return;

  /* read file */
  fread(bios_rom, 1, 0x800, fp);
  fclose(fp);

  /* update BIOS flags */
  config.bios_enabled |= 2;

  if (config.bios_enabled == 3)
  {
    /* initialize system */
    system_init ();
    audio_init(48000);
  }
}

static void init_machine(void)
{
  /* Allocate cart_rom here ( 10 MBytes ) */
  cart_rom = memalign(32, MAXROMSIZE);
  if (!cart_rom)
  {
    WaitPrompt("Failed to allocate ROM buffer... Rebooting");
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

  /* default system */
  input.system[0] = SYSTEM_GAMEPAD;
  input.system[1] = SYSTEM_GAMEPAD;
}

/**************************************************
  Load a new rom and performs some initialization
***************************************************/
void reloadrom (int size, char *name)
{
  genromsize = size;
  load_rom(name);       /* Load ROM */
  system_init ();     /* Initialize System */
  audio_init(48000);  /* Audio System initialization */
  ClearGGCodes ();    /* Clear Game Genie patches */
  system_reset ();    /* System Power ON */
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
  free(cart_rom);
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
  /* initialize DVD Mode */
  DI_Close();
  DI_Init();
#endif

  uint32 RenderedFrames   = 0;
  uint32 TotalFrames      = 0;
  uint32 FramesPerSecond  = 0;

  /* initialize hardware */
  gx_video_Init();
  gx_input_Init();
  gx_audio_Init();
#ifdef HW_DOL
  DVD_Init ();
  dvd_drive_detect();
#endif

  /* initialize FAT devices */
  if (fatInitDefault())
  {
    fat_enabled = 1;
#ifdef HW_RVL
    fatEnableReadAhead ("sd", 6, 64);
    fatEnableReadAhead ("usb", 6, 64);
#else
    fatEnableReadAhead ("carda", 6, 64);
    fatEnableReadAhead ("cardb", 6, 64);
#endif
  }

  /* Initialize sound engine */
  gx_audio_Init();

  /* default config */
  legal();
  config_setDefault();
  config_load();

  /* recent ROM files list */
  history_setDefault();
  history_load();

  /* initialize Virtual Machine */
  init_machine ();

  /* run any injected rom */
  if (genromsize)
  {
    ARAMFetch((char *)cart_rom, (void *)0x8000, genromsize);
    reloadrom (genromsize,"INJECT.bin");
    gx_video_Start();
    gx_audio_Start();
    frameticker = 1;
  }
  else
  {
    /* show menu first */
    ConfigRequested = 1;
  }

#ifdef HW_RVL
  /* Power button callback */
  SYS_SetPowerCallback(Power_Off);
#endif

  /* main emulation loop */
  while (1)
  {
    /* check for menu request */
    if (ConfigRequested)
    {
      /* stop audio & video */
      gx_video_Stop();
      gx_audio_Stop();

      /* go to menu */
      MainMenu ();
      ConfigRequested = 0;

      /* reset framecounts */
      RenderedFrames  = 0;
      TotalFrames     = 0;
      FramesPerSecond = vdp_rate;

      /* start audio & video */
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

      /* update audio only */
      gx_audio_Update();
    }
    else
    {
      /* framesync */
      while (frameticker < 1) usleep(1);
      frameticker--;

      /* render frame */
      system_frame (0);

      /* update video & audio */
      gx_video_Update();
      gx_audio_Update();
      RenderedFrames++;
    }

    /* update framecounts */
    TotalFrames++;
    if (TotalFrames == vdp_rate)
    {
      FramesPerSecond = RenderedFrames;
      RenderedFrames  = 0;
      TotalFrames     = 0;
    }
  }

  return 0;
}
