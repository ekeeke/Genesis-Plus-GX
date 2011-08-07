/****************************************************************************
 *  main.c
 *
 *  Genesis Plus GX
 *
 *  Copyright Eke-Eke (2007-2011), based on original work from Softdev (2006)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

#include "shared.h"
#include "font.h"
#include "gui.h"
#include "menu.h"
#include "history.h"
#include "file_slot.h"
#include "file_load.h"
#include "filesel.h"
#include "cheats.h"

#include <fat.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <ogc/machine/processor.h>
#endif

/* audio "exact" samplerate, measured on real hardware */
#ifdef HW_RVL
#define SAMPLERATE_48KHZ 48000
#else
#define SAMPLERATE_48KHZ 48044
#endif

u32 Shutdown = 0;
u32 ConfigRequested = 1;
u32 frameticker;

#ifdef HW_RVL
/****************************************************************************
 * Power Button callback 
 ***************************************************************************/
static void PowerOff_cb(void)
{
  Shutdown = 1;
  ConfigRequested = 1;
}
#endif

/****************************************************************************
 * Reset Button callback 
 ***************************************************************************/
static void Reset_cb(void)
{
  if (system_hw & SYSTEM_MD)
  {
    /* SOFT-RESET */
    gen_reset(0);
  }
  else if (system_hw == SYSTEM_SMS)
  {
    /* assert RESET input */
    io_reg[0x0D] &= ~IO_RESET_HI;
  }
}

/***************************************************************************
 * Genesis Plus Virtual Machine
 *
 ***************************************************************************/
static void load_bios(void)
{
  /* clear BIOS detection flag */
  config.tmss &= ~2;

  /* open BIOS file */
  FILE *fp = fopen(OS_ROM, "rb");
  if (fp == NULL) return;

  /* read file */
  fread(bios_rom, 1, 0x800, fp);
  fclose(fp);

  /* check ROM file */
  if (!strncmp((char *)(bios_rom + 0x120),"GENESIS OS", 10))
  {
    /* valid BIOS detected */
    config.tmss |= 2;
  }
}

static void init_machine(void)
{
  /* allocate cart.rom here (10 MBytes) */
  cart.rom = memalign(32, MAXROMSIZE);

  /* BIOS support */
  load_bios();

  /* allocate global work bitmap */
  memset(&bitmap, 0, sizeof (bitmap));
  bitmap.width  = 720;
  bitmap.height = 576;
  bitmap.pitch = bitmap.width * 2;
  bitmap.viewport.w = 256;
  bitmap.viewport.h = 224;
  bitmap.viewport.x = 0;
  bitmap.viewport.y = 0;
  bitmap.data = texturemem;
}

/*******************************************
  Restart emulation when loading a new game 
********************************************/
static void reload(void)
{
  /* Cartridge Hot Swap (make sure system has already been inited once) */
  if (config.hot_swap && snd.enabled)
  {
    if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
    {
      md_cart_init();
      md_cart_reset(1);
    }
    else
    {
      sms_cart_init();
      sms_cart_reset();
    }
  }
  else
  {
    /* Initialize audio emulation */
    /* To prevent any sound skipping, sound chips must run at the exact same speed as the rest of emulation (see sound.c) */
    /* When TV output mode matches emulated video mode, we use video hardware interrupt (VSYNC) and exact framerates for perfect synchronization */
    /* In 60Hz TV modes, Wii & GC framerates have been measured to be 59.94 (interlaced or progressive) and ~59.825 fps (non-interlaced) */
    /* In 50Hz TV modes, Wii & GC framerates have been measured to be 50.00 (interlaced) and ~50.845 fps (non-interlaced) */
    /* When modes does not match, emulation is synchronized with audio hardware interrupt (DMA) and we use default framerates (50Hz for PAL, 60Hz for NTSC). */
    if (vdp_pal)
    {
      audio_init(SAMPLERATE_48KHZ, (config.tv_mode == 0) ? 50.0 : (config.render ? 50.00 : (1000000.0/19968.0)));
    }
    else
    {
      audio_init(SAMPLERATE_48KHZ, (config.tv_mode == 1) ? 60.0 : (config.render ? 59.94 : (1000000.0/16715.0)));
    }
     
    /* Switch virtual system on */
    system_init();
    system_reset();
  }

  /* Auto-Load SRAM file */
  if (config.s_auto & 1)
  {
    slot_autoload(0,config.s_device);
  }
            
  /* Auto-Load State file */
  if (config.s_auto & 2)
  {
    slot_autoload(config.s_default,config.s_device);
  }

  /* Load Cheat file */
  CheatLoad();
}

static void run_emulation(void)
{
  /* main emulation loop */
  while (1)
  {
    /* Main Menu request */
    if (ConfigRequested)
    {
      /* stop video & audio */
      gx_audio_Stop();
      gx_video_Stop();

      /* show menu */
      ConfigRequested = 0;
      if (menu_execute())
      {
        /* new ROM has been loaded */
        reload();
      }

      /* start video & audio */
      gx_video_Start();
      gx_audio_Start();
      frameticker = 1;
    }

    /* automatic frame skipping (only necessary for Virtua Racing in Gamecube mode) */
    if (frameticker > 1)
    {
      /* skip frame */
      system_frame(1);
      frameticker = 1;
    }
    else
    {
      /* render frame */
      frameticker = 0;
      system_frame(0);

      /* update video */
      gx_video_Update();
    }

    /* update audio */
    gx_audio_Update();

    /* check interlaced mode change */
    if (bitmap.viewport.changed & 4)
    {
      /* VSYNC "original" mode */
      if (!config.render && (gc_pal == vdp_pal))
      {
        /* framerate has changed, reinitialize audio timings */
        if (vdp_pal)
        {
          audio_init(SAMPLERATE_48KHZ, interlaced ? 50.00 : (1000000.0/19968.0));
        }
        else
        {
          audio_init(SAMPLERATE_48KHZ, interlaced ? 59.94 : (1000000.0/16715.0));
        }

        /* reinitialize sound chips */
        sound_restore();
      }

      /* clear flag */
      bitmap.viewport.changed &= ~4;
    }

    /* wait for next frame */
    while (frameticker < 1) usleep(1);
  }
}

/**************************************************
  Shutdown everything properly
***************************************************/
void shutdown(void)
{
  /* save current config */
  config_save();

  /* save current game state */
  if (config.s_auto & 2)
  {
    slot_autosave(config.s_default,config.s_device);
  }

  /* shutdown emulation */
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
int main (int argc, char *argv[])
{
#ifdef HW_RVL
  /* enable 64-byte fetch mode for L2 cache */
  L2Enhance();
  
  /* initialize DI interface */
  DI_UseCache(0);
  DI_Init();
#endif

  /* initialize video engine */
  gx_video_Init();

#ifdef HW_DOL
  /* initialize DVD interface */
  DVD_Init();
#endif

  /* initialize input engine */
  gx_input_Init();

  /* initialize FAT devices */
  int retry = 0;
  int fatMounted = 0;

  /* try to mount FAT devices during 3 seconds */
  while (!fatMounted && (retry < 12))
  {
    fatMounted = fatInitDefault();
    usleep(250000);
    retry++;
  }

  if (fatMounted)
  {
    /* base directory */
    char pathname[MAXPATHLEN];
    sprintf (pathname, DEFAULT_PATH);
    DIR *dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);

    /* default SRAM & Savestate files directories */ 
    sprintf (pathname, "%s/saves",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/saves/md",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/saves/ms",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/saves/gg",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/saves/sg",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);

    /* default Snapshot files directories */ 
    sprintf (pathname, "%s/snaps",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/snaps/md",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/snaps/ms",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/snaps/gg",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/snaps/sg",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);

    /* default Cheat files directories */ 
    sprintf (pathname, "%s/cheats",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/cheats/md",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/cheats/ms",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/cheats/gg",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
    sprintf (pathname, "%s/cheats/sg",DEFAULT_PATH);
    dir = opendir(pathname);
    if (dir) closedir(dir);
    else mkdir(pathname,S_IRWXU);
  }

  /* initialize sound engine */
  gx_audio_Init();

  /* initialize genesis plus core */
  legal();
  config_default();
  history_default();
  init_machine();

  /* auto-load last ROM file */
  if (config.autoload)
  {
    SILENT = 1;
    if (OpenDirectory(TYPE_RECENT))
    {
      if (LoadFile(0))
      {
        reload();
        gx_video_Start();
        gx_audio_Start();
        frameticker = 1;
        ConfigRequested = 0;
      }
    }
    SILENT = 0;
  }

#ifdef HW_RVL
  /* power button callback */
  SYS_SetPowerCallback(PowerOff_cb);
#endif

  /* reset button callback */
  SYS_SetResetCallback(Reset_cb);

  /* main emulation loop */
  run_emulation();

  /* we should never return anyway */
  return 0;
}
