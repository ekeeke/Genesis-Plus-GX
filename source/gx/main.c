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
 * Force AHBPROT flags when IOS is reloaded 
 * Credits to DaveBaol for the original patch
 ***************************************************************************/
int Patch_IOS(void)
{
  /* full hardware access is initially required */
  if (read32(0xd800064) == 0xffffffff)
  {
    /* disable MEM2 protection */
    write16(0xd8b420a, 0);

    /* IOS area (top of MEM2, above IOS Heap area) */
    u8 *ptr_start = (u8*)*((u32*)0x80003134);
    u8 *ptr_end = (u8*)0x94000000;

    /* Make sure start pointer is valid */
    if (((u32)ptr_start < 0x90000000) || (ptr_start >= ptr_end))
    {
      /* use libogc default value (longer but safer) */
      ptr_start = (u8*) SYS_GetArena2Hi();
    }
   
    /* Search specific code pattern */
    const u8 es_set_ahbprot_pattern[] = { 0x68, 0x5B, 0x22, 0xEC, 0x00, 0x52, 0x18, 0x9B, 0x68, 0x1B, 0x46, 0x98, 0x07, 0xDB };
    while (ptr_start < (ptr_end - sizeof(es_set_ahbprot_pattern)))
    {
      if (!memcmp(ptr_start, es_set_ahbprot_pattern, sizeof(es_set_ahbprot_pattern)))
      {
        /* patch IOS (force AHBPROT bit to be set when launching titles) */
        ptr_start[25] = 0x01;
        DCFlushRange(ptr_start + 25, 1);
      }
      ptr_start++; /* could be optimized ? not sure if pattern coincides with instruction start */
    }
  }
  return 0;
}

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
    /* Soft Reset */
    gen_reset(0);
  }
  else if (system_hw == SYSTEM_SMS)
  {
    /* assert RESET input (Master System model 1 only) */
    io_reg[0x0D] &= ~IO_RESET_HI;
  }
}

/***************************************************************************
 * Genesis Plus Virtual Machine
 *
 ***************************************************************************/
static void init_machine(void)
{
  /* allocate cartridge ROM here (10 MB) */
  cart.rom = memalign(32, MAXROMSIZE);

  /* mark all BIOS as unloaded */
  config.bios &= 0x03;

  /* Genesis BOOT ROM support (2KB max) */
  memset(boot_rom, 0xFF, 0x800);
  FILE *fp = fopen(MD_BIOS, "rb");
  if (fp != NULL)
  {
    /* read BOOT ROM */
    fread(boot_rom, 1, 0x800, fp);
    fclose(fp);

    /* check BOOT ROM */
    if (!strncmp((char *)(boot_rom + 0x120),"GENESIS OS", 10))
    {
      /* mark Genesis BIOS as loaded */
      config.bios |= SYSTEM_MD;
    }
  }

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

static void run_emulation(void)
{
  /* main emulation loop */
  while (1)
  {
    /* emulated system */
    if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
    {
      /* Mega Drive type hardware */
      while (!ConfigRequested)
      {
        /* automatic frame skipping (only needed when running Virtua Racing on Gamecube) */
        if (frameticker > 1)
        {
          /* skip frame */
          system_frame_gen(1);
          frameticker = 1;
        }
        else
        {
          /* render frame */
          frameticker = 0;
          system_frame_gen(0);

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
    else
    {
      /* Master System type hardware */
      while (!ConfigRequested)
      {
        /* render frame (no frameskipping needed) */
        frameticker = 0;
        system_frame_sms(0);

        /* update video */
        gx_video_Update();

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

    /* stop video & audio */
    gx_audio_Stop();
    gx_video_Stop();

    /* show menu */
    ConfigRequested = 0;
    mainmenu();

    /* restart video & audio */
    gx_video_Start();
    gx_audio_Start();
    frameticker = 1;
  }
}

/*******************************************
  Restart emulation when loading a new game 
********************************************/
void reloadrom(void)
{
  /* Cartridge Hot Swap (make sure system has already been inited once) */
  if (config.hot_swap == 3)
  {
    /* Initialize cartridge hardware only */
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

    /* Allow hot swap */
    config.hot_swap |= 2;
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
  /* Temporary fix for HBC bug when using no_ios_reload with no connected network */
  /* Try to patch current IOS to force AHBPROT flags being set on reload */
  if (Patch_IOS())
  {
    /* reload IOS (full hardware access should now be preserved after reload) */
    IOS_ReloadIOS(IOS_GetVersion());

    /* enable DVD video commands */
    write32(0xd800180, read32(0xd800180) & ~0x00200000);
    usleep(200000);
  }
 
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
    if (OpenDirectory(TYPE_RECENT, -1))
    {
      if (LoadFile(0))
      {
        reloadrom();
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
