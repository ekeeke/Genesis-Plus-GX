#ifndef _MSC_VER
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#ifdef _XBOX1
#include <xtl.h>
#endif

#define RETRO_DEVICE_MDPAD_3B             RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_MDPAD_6B             RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_MSPAD_2B             RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)
#define RETRO_DEVICE_MDPAD_3B_WAYPLAY     RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 3)
#define RETRO_DEVICE_MDPAD_6B_WAYPLAY     RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 4)
#define RETRO_DEVICE_MDPAD_3B_TEAMPLAYER  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 5)
#define RETRO_DEVICE_MDPAD_6B_TEAMPLAYER  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 6)
#define RETRO_DEVICE_MSPAD_2B_MASTERTAP   RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 7)
#define RETRO_DEVICE_PADDLE               RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 0)
#define RETRO_DEVICE_SPORTSPAD            RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1)
#define RETRO_DEVICE_XE_1AP               RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 2)
#define RETRO_DEVICE_PHASER               RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 0)
#define RETRO_DEVICE_MENACER              RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 1)
#define RETRO_DEVICE_JUSTIFIERS           RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 2)
#define RETRO_DEVICE_MDPAD_6B_ALT1        RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 8) // Laid out like a SNES controller for SSFII
#define RETRO_DEVICE_MDPAD_6B_ALT2        RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 9) // Laid out like a Madcatz fightpad

#include "shared.h"
#include "libretro.h"
#include "state.h"
#include "md_ntsc.h"
#include "sms_ntsc.h"

sms_ntsc_t *sms_ntsc;
md_ntsc_t  *md_ntsc;

char GG_ROM[256];
char AR_ROM[256];
char SK_ROM[256];
char SK_UPMEM[256];
char GG_BIOS[256];
char MS_BIOS_EU[256];
char MS_BIOS_JP[256];
char MS_BIOS_US[256];
char CD_BIOS_EU[256];
char CD_BIOS_US[256];
char CD_BIOS_JP[256];
char CD_BRAM_JP[256];
char CD_BRAM_US[256];
char CD_BRAM_EU[256];
char CART_BRAM[256];

static int vwidth;
static int vheight;
static bool mdalt1;
static bool mdalt2;

static uint32_t brm_crc[2];
static uint8_t brm_format[0x40] =
{
  0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x00,0x00,0x00,0x00,0x40,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x53,0x45,0x47,0x41,0x5f,0x43,0x44,0x5f,0x52,0x4f,0x4d,0x00,0x01,0x00,0x00,0x00,
  0x52,0x41,0x4d,0x5f,0x43,0x41,0x52,0x54,0x52,0x49,0x44,0x47,0x45,0x5f,0x5f,0x5f
};

static uint8_t temp[0x10000];
static int16 soundbuffer[3068];
static uint16_t bitmap_data_[720 * 576];
static const double pal_fps = 53203424.0 / (3420.0 * 313.0);
static const double ntsc_fps = 53693175.0 / (3420.0 * 262.0);

static char g_rom_dir[1024];

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;
static retro_audio_sample_batch_t audio_cb;

/************************************
 * Genesis Plus GX implementation
 ************************************/
#undef  CHUNKSIZE
#define CHUNKSIZE   (0x10000)

void error(char * fmt, ...)
{
   char buffer[256];
   va_list ap;
   va_start(ap, fmt);
   vsprintf(buffer, fmt, ap);
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, "%s\n", buffer);
   va_end(ap);
}

int load_archive(char *filename, unsigned char *buffer, int maxsize, char *extension)
{
  int size, left;

  /* Open file */
  FILE *fd = fopen(filename, "rb");

  if (!fd)
  {
    /* Master System & Game Gear BIOS are optional files */
    if (!strcmp(filename,MS_BIOS_US) || !strcmp(filename,MS_BIOS_EU) || !strcmp(filename,MS_BIOS_JP) || !strcmp(filename,GG_BIOS))
    {
      return 0;
    }
  
    /* Mega CD BIOS are required files */
    if (!strcmp(filename,CD_BIOS_US) || !strcmp(filename,CD_BIOS_EU) || !strcmp(filename,CD_BIOS_JP)) 
    {
       if (log_cb)
          log_cb(RETRO_LOG_ERROR, "Unable to open CD BIOS: %s.\n", filename);
       return 0;
    }

    if (log_cb)
       log_cb(RETRO_LOG_ERROR, "Unable to open file.\n");
    return 0;
  }

  /* Get file size */
  fseek(fd, 0, SEEK_END);
  size = ftell(fd);

  /* size limit */
  if(size > maxsize)
  {
    fclose(fd);
    if (log_cb)
       log_cb(RETRO_LOG_ERROR, "File is too large.\n");
    return 0;
  }

  if (log_cb)
     log_cb(RETRO_LOG_INFO, "INFORMATION - Loading %d bytes ...\n", size);

  /* filename extension */
  if (extension)
  {
    memcpy(extension, &filename[strlen(filename) - 3], 3);
    extension[3] = 0;
  }

  /* Read into buffer */
  left = size;
  fseek(fd, 0, SEEK_SET);
  while (left > CHUNKSIZE)
  {
    fread(buffer, CHUNKSIZE, 1, fd);
    buffer += CHUNKSIZE;
    left -= CHUNKSIZE;
  }

  /* Read remaining bytes */
  fread(buffer, left, 1, fd);

  /* Close file */
  fclose(fd);

  /* Return loaded ROM size */
  return size;
}

void osd_input_update(void)
{
  int i, player = 0;
  unsigned int temp;

  input_poll_cb();

  for (i = 0; i < MAX_INPUTS; i++)
  {
    temp = 0;
    switch (input.dev[i])
    {

      case DEVICE_PAD6B:
      {
        if(mdalt1)
        {
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
             temp |= INPUT_X;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
             temp |= INPUT_Y;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))
             temp |= INPUT_Z;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
             temp |= INPUT_MODE;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
             temp |= INPUT_A;  
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
             temp |= INPUT_B;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
             temp |= INPUT_C;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
             temp |= INPUT_START;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
             temp |= INPUT_UP;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
             temp |= INPUT_DOWN;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
             temp |= INPUT_LEFT;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
             temp |= INPUT_RIGHT;
           player++;
           break;
        }

        if(mdalt2)
        {
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
             temp |= INPUT_X;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
             temp |= INPUT_Y;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
             temp |= INPUT_Z;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
             temp |= INPUT_MODE;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
             temp |= INPUT_A;  
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
             temp |= INPUT_B;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
             temp |= INPUT_C;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
             temp |= INPUT_START;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
             temp |= INPUT_UP;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
             temp |= INPUT_DOWN;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
             temp |= INPUT_LEFT;
           if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
             temp |= INPUT_RIGHT;
           player++;
           break;
        }

        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))
          temp |= INPUT_X;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
          temp |= INPUT_Y;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
          temp |= INPUT_Z;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
          temp |= INPUT_MODE;
      }

      case DEVICE_PAD3B:
      {
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
          temp |= INPUT_A;
      }

      case DEVICE_PAD2B:
      {
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
          temp |= INPUT_B;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
          temp |= INPUT_C;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
          temp |= INPUT_START;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
          temp |= INPUT_UP;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
          temp |= INPUT_DOWN;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
          temp |= INPUT_LEFT;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
          temp |= INPUT_RIGHT;

        player++;
        break;
      }

      case DEVICE_MOUSE:
      {
        input.analog[i][0] = input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
        if (config.invert_mouse)
          input.analog[i][1] = input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
        else
          input.analog[i][1] = -input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

        if (input.analog[i][0] < -255)
          input.analog[i][0] = -255;
        else if (input.analog[i][0] > 255)
          input.analog[i][0] = 255;
        if (input.analog[i][1] < -255)
          input.analog[i][1] = -255;
        else if (input.analog[i][1] > 255)
          input.analog[i][1] = 255;

        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT))
          temp |= INPUT_MOUSE_LEFT;
        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT))
          temp |= INPUT_MOUSE_RIGHT;
        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN))
          temp |= INPUT_MOUSE_CENTER;
        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE))
          temp |= INPUT_START;

        player++;
        break;
      }

      case DEVICE_LIGHTGUN:
      {
        input.analog[i][0] = ((input_state_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) + 0x7fff) * bitmap.viewport.w) / 0xfffe;
        input.analog[i][1] = ((input_state_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) + 0x7fff) * bitmap.viewport.h) / 0xfffe;

        if (config.gun_cursor)
        {
          uint16_t *ptr = (uint16_t *)bitmap.data + ((bitmap.viewport.y + input.analog[i][1]) * bitmap.width) + input.analog[i][0] + bitmap.viewport.x;
          ptr[-3*bitmap.width] = ptr[-bitmap.width] = ptr[bitmap.width] = ptr[3*bitmap.width] = ptr[-3] = ptr[-1] = ptr[1] = ptr[3] = (i & 1) ? 0xf800 : 0x001f;
          ptr[-2*bitmap.width] = ptr[0] = ptr[2*bitmap.width] = ptr[-2] = ptr[2] = 0xffff;
        }

        if (input_state_cb(player, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER))
          temp |= INPUT_A;
        if (input_state_cb(player, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TURBO))
          temp |= INPUT_B;
        if (input_state_cb(player, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_PAUSE))
          temp |= INPUT_C;
        if (input_state_cb(player, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START))
          temp |= INPUT_START;

        player++;
        break;
      }

      case DEVICE_PADDLE:
      {
        input.analog[i][0] = (input_state_cb(player, RETRO_DEVICE_ANALOG, 0, RETRO_DEVICE_ID_ANALOG_X) + 0x8000) >> 8;

        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
          temp |= INPUT_BUTTON1;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
          temp |= INPUT_START;

        player++;
        break;
      }

      case DEVICE_SPORTSPAD:
      {
        input.analog[i][0] = (input_state_cb(player, RETRO_DEVICE_ANALOG, 0, RETRO_DEVICE_ID_ANALOG_X) + 0x8000) >> 8;
        input.analog[i][1] = (input_state_cb(player, RETRO_DEVICE_ANALOG, 0, RETRO_DEVICE_ID_ANALOG_Y) + 0x8000) >> 8;

        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
          temp |= INPUT_BUTTON1;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
          temp |= INPUT_BUTTON2;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
          temp |= INPUT_START;

        player++;
        break;
      }

      case DEVICE_PICO:
      {
        input.analog[i][0] = 0x03c + ((input_state_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) + 0x7fff) * (0x17c-0x03c)) / 0xfffe;
        input.analog[i][1] = 0x1fc + ((input_state_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) + 0x7fff) * (0x2f7-0x1fc)) / 0xfffe;

        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT))
          temp |= INPUT_PICO_PEN;
        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT))
          temp |= INPUT_PICO_RED;
        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP))
          pico_current = (pico_current - 1) & 7;
        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN))
          pico_current = (pico_current + 1) & 7;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
          temp |= INPUT_UP;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
          temp |= INPUT_DOWN;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
          temp |= INPUT_LEFT;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
          temp |= INPUT_RIGHT;

        player++;
        break;
      }

      case DEVICE_TEREBI:
      {
        input.analog[i][0] = ((input_state_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) + 0x7fff) * 250) / 0xfffe;
        input.analog[i][1] = ((input_state_cb(player, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) + 0x7fff) * 250) / 0xfffe;

        if (input.analog[0][0] < 0)
          input.analog[0][0] = 0;
        else if (input.analog[0][0] > 250)
          input.analog[0][0] = 250;
        if (input.analog[0][1] < 0)
          input.analog[0][1] = 0;
        else if (input.analog[0][1] > 250)
          input.analog[0][1] = 250;

        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT))
          temp |= INPUT_BUTTON1;
        if (input_state_cb(player, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE))
          temp |= INPUT_START;

        player++;
        break;
      }

      case DEVICE_XE_1AP:
      {
        int rx = input.analog[i][0] = input_state_cb(player, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
        int ry = input.analog[i][1] = input_state_cb(player, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
        if (abs(rx) > abs(ry))
        {
         input.analog[i+1][0] = (rx + 0x8000) >> 8;
        }
        else 
        {
         input.analog[i+1][0] = (0x7fff - ry) >> 8;
        }
        input.analog[i][0] = (input_state_cb(player, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) + 0x8000) >> 8;
        input.analog[i][1] = (input_state_cb(player, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) + 0x8000) >> 8;

        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
          temp |= INPUT_XE_A;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
          temp |= INPUT_XE_B;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))
          temp |= INPUT_XE_C;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
          temp |= INPUT_XE_D;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
          temp |= INPUT_XE_E1;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
          temp |= INPUT_XE_E2;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
          temp |= INPUT_XE_SELECT;
        if (input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
          temp |= INPUT_XE_START;

        player++;
        break;
      }

      default:
        break;
    }

    input.pad[i] = temp;
  }
}

static void init_bitmap(void)
{
   memset(&bitmap, 0, sizeof(bitmap));
   bitmap.width      = 720;
   bitmap.height     = 576;
   bitmap.pitch      = 720 * 2;
   bitmap.data       = (uint8_t *)bitmap_data_;
}

static void config_default(void)
{
   int i;
   
   /* sound options */
   config.psg_preamp     = 150;
   config.fm_preamp      = 100;
   config.hq_fm          = 1; /* high-quality resampling */
   config.psgBoostNoise  = 1;
   config.filter         = 0; /* no filter */
   config.lp_range       = 0x9999; /* 0.6 in 16.16 fixed point */
   config.low_freq       = 880;
   config.high_freq      = 5000;
   config.lg             = 1.0;
   config.mg             = 1.0;
   config.hg             = 1.0;
   config.dac_bits 	     = 14; /* MAX DEPTH */ 
   config.ym2413         = 2; /* AUTO */
   config.mono           = 0; /* STEREO output */

   /* system options */
   config.system         = 0; /* AUTO */
   config.region_detect  = 0; /* AUTO */
   config.vdp_mode       = 0; /* AUTO */
   config.master_clock   = 0; /* AUTO */
   config.force_dtack    = 0;
   config.addr_error     = 1;
   config.bios           = 0;
   config.lock_on        = 0;

   /* video options */
   config.overscan = 0;
   config.gg_extra = 0;
   config.ntsc     = 0;
   config.render   = 0;

   /* input options */
   input.system[0] = SYSTEM_GAMEPAD;
   input.system[1] = SYSTEM_GAMEPAD;
   for (i=0; i<MAX_INPUTS; i++)
   {
     config.input[i].padtype = DEVICE_PAD2B | DEVICE_PAD3B | DEVICE_PAD6B;
   }
}

static void bram_load(void)
{
    FILE *fp;

    /* automatically load internal backup RAM */
    switch (region_code)
    {
       case REGION_JAPAN_NTSC:
          fp = fopen(CD_BRAM_JP, "rb");
          break;
       case REGION_EUROPE:
          fp = fopen(CD_BRAM_EU, "rb");
          break;
       case REGION_USA:
          fp = fopen(CD_BRAM_US, "rb");
          break;
       default:
          return;
    }

    if (fp != NULL)
    {
      fread(scd.bram, 0x2000, 1, fp);
      fclose(fp);

      /* update CRC */
      brm_crc[0] = crc32(0, scd.bram, 0x2000);
    }
    else
    {
      /* force internal backup RAM format (does not use previous region backup RAM) */
      scd.bram[0x1fff] = 0;
    }

    /* check if internal backup RAM is correctly formatted */
    if (memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
    {
      /* clear internal backup RAM */
      memset(scd.bram, 0x00, 0x2000 - 0x40);

      /* internal Backup RAM size fields */
      brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] = 0x00;
      brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] = (sizeof(scd.bram) / 64) - 3;

      /* format internal backup RAM */
      memcpy(scd.bram + 0x2000 - 0x40, brm_format, 0x40);

      /* clear CRC to force file saving (in case previous region backup RAM was also formatted) */
      brm_crc[0] = 0;
    }

    /* automatically load cartridge backup RAM (if enabled) */
    if (scd.cartridge.id)
    {
      fp = fopen(CART_BRAM, "rb");
      if (fp != NULL)
      {
        int filesize = scd.cartridge.mask + 1;
        int done = 0;
        
        /* Read into buffer (2k blocks) */
        while (filesize > CHUNKSIZE)
        {
          fread(scd.cartridge.area + done, CHUNKSIZE, 1, fp);
          done += CHUNKSIZE;
          filesize -= CHUNKSIZE;
        }

        /* Read remaining bytes */
        if (filesize)
        {
          fread(scd.cartridge.area + done, filesize, 1, fp);
        }

        /* close file */
        fclose(fp);

        /* update CRC */
        brm_crc[1] = crc32(0, scd.cartridge.area, scd.cartridge.mask + 1);
      }

      /* check if cartridge backup RAM is correctly formatted */
      if (memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
      {
        /* clear cartridge backup RAM */
        memset(scd.cartridge.area, 0x00, scd.cartridge.mask + 1);

        /* Cartridge Backup RAM size fields */
        brm_format[0x10] = brm_format[0x12] = brm_format[0x14] = brm_format[0x16] = (((scd.cartridge.mask + 1) / 64) - 3) >> 8;
        brm_format[0x11] = brm_format[0x13] = brm_format[0x15] = brm_format[0x17] = (((scd.cartridge.mask + 1) / 64) - 3) & 0xff;

        /* format cartridge backup RAM */
        memcpy(scd.cartridge.area + scd.cartridge.mask + 1 - 0x40, brm_format, 0x40);
      }
    }
}

static void bram_save(void)
{
    FILE *fp;

    /* verify that internal backup RAM has been modified */
    if (crc32(0, scd.bram, 0x2000) != brm_crc[0])
    {
      /* check if it is correctly formatted before saving */
      if (!memcmp(scd.bram + 0x2000 - 0x20, brm_format + 0x20, 0x20))
      {
        switch (region_code)
        {
          case REGION_JAPAN_NTSC:
            fp = fopen(CD_BRAM_JP, "wb");
            break;
          case REGION_EUROPE:
            fp = fopen(CD_BRAM_EU, "wb");
            break;
          case REGION_USA:
            fp = fopen(CD_BRAM_US, "wb");
            break;
          default:
            return;
        }

        if (fp != NULL)
        {
          fwrite(scd.bram, 0x2000, 1, fp);
          fclose(fp);

          /* update CRC */
          brm_crc[0] = crc32(0, scd.bram, 0x2000);
        }
      }
    }

    /* verify that cartridge backup RAM has been modified */
    if (scd.cartridge.id && (crc32(0, scd.cartridge.area, scd.cartridge.mask + 1) != brm_crc[1]))
    {
      /* check if it is correctly formatted before saving */
      if (!memcmp(scd.cartridge.area + scd.cartridge.mask + 1 - 0x20, brm_format + 0x20, 0x20))
      {
        fp = fopen(CART_BRAM, "wb");
        if (fp != NULL)
        {
          int filesize = scd.cartridge.mask + 1;
          int done = 0;
        
          /* Write to file (2k blocks) */
          while (filesize > CHUNKSIZE)
          {
            fwrite(scd.cartridge.area + done, CHUNKSIZE, 1, fp);
            done += CHUNKSIZE;
            filesize -= CHUNKSIZE;
          }

          /* Write remaining bytes */
          if (filesize)
          {
            fwrite(scd.cartridge.area + done, filesize, 1, fp);
          }

          /* Close file */
          fclose(fp);

          /* update CRC */
          brm_crc[1] = crc32(0, scd.cartridge.area, scd.cartridge.mask + 1);
        }
      }
    }
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   char *base;
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

static bool update_viewport(void)
{
  int ow = vwidth;
  int oh = vheight;

  vwidth  = bitmap.viewport.w + (bitmap.viewport.x * 2);
  vheight = bitmap.viewport.h + (bitmap.viewport.y * 2);

   if (config.ntsc)
   {
      if (reg[12] & 1)
         vwidth = MD_NTSC_OUT_WIDTH(vwidth);
      else
         vwidth = SMS_NTSC_OUT_WIDTH(vwidth);
   }

   if (config.render && interlaced)
   {
      vheight = vheight * 2;
   }

   return ((ow != vwidth) || (oh != vheight));
}

static void check_variables(void)
{
  unsigned orig_value;
  bool update_viewports = false;
  bool reinit = false;
  struct retro_variable var = {0};

  var.key = "system_hw";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.system;
    if (!strcmp(var.value, "sg-1000"))
      config.system = SYSTEM_SG;
    else if (!strcmp(var.value, "sg-1000 II"))
      config.system = SYSTEM_SGII;
    else if (!strcmp(var.value, "mark-III"))
      config.system = SYSTEM_MARKIII;
    else if (!strcmp(var.value, "master system"))
      config.system = SYSTEM_SMS;
    else if (!strcmp(var.value, "master system II"))
      config.system = SYSTEM_SMS2;
    else if (!strcmp(var.value, "game gear"))
      config.system = SYSTEM_GG;
    else if (!strcmp(var.value, "mega drive / genesis"))
      config.system = SYSTEM_MD;
    else
      config.system = 0;

    if (orig_value != config.system)
    {
      if (system_hw)
      {
        switch (config.system)
        {
          case 0:
            system_hw = romtype; /* AUTO */
            break;

          case SYSTEM_MD:
            system_hw = (romtype & SYSTEM_MD) ? romtype : SYSTEM_PBC;
            break;

          case SYSTEM_GG:
            system_hw = (romtype == SYSTEM_GG) ? SYSTEM_GG : SYSTEM_GGMS;
            break;

          default:
            system_hw = config.system;
            break;
        }

        reinit = true;
      }
    }
  }

  var.key = "region_detect";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.region_detect;
    if (!strcmp(var.value, "ntsc-u"))
      config.region_detect = 1;
    else if (!strcmp(var.value, "pal"))
      config.region_detect = 2;
    else if (!strcmp(var.value, "ntsc-j"))
      config.region_detect = 3;
    else
      config.region_detect = 0;

    if (orig_value != config.region_detect)
    {
      if (system_hw)
      {
        get_region(NULL);
        reinit = true;
      }
    }
  }

  var.key = "force_dtack";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    if (!strcmp(var.value, "enabled"))
      config.force_dtack = 1;
    else
      config.force_dtack = 0;
  }

  var.key = "addr_error";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    if (!strcmp(var.value, "enabled"))
      m68k.aerr_enabled = config.addr_error = 1;
    else
      m68k.aerr_enabled = config.addr_error = 0;
  }

  var.key = "lock_on";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.lock_on;
    if (!strcmp(var.value, "game genie"))
      config.lock_on = TYPE_GG;
    else if (!strcmp(var.value, "action replay (pro)"))
      config.lock_on = TYPE_AR;
    else if (!strcmp(var.value, "sonic & knuckles"))
      config.lock_on = TYPE_SK;
    else
      config.lock_on = 0;

    if (orig_value != config.lock_on)
    {
      if (system_hw == SYSTEM_MD)
        reinit = true;
    }
  }

  var.key = "ym2413";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.ym2413;
    if (!strcmp(var.value, "enabled"))
      config.ym2413 = 1;
    else if (!strcmp(var.value, "disabled"))
      config.ym2413 = 0;
    else
      config.ym2413 = 2;

    if (orig_value != config.ym2413)
    {
      if (system_hw && (config.ym2413 & 2) && ((system_hw & SYSTEM_PBC) != SYSTEM_MD))
      {
        memcpy(temp, sram.sram, sizeof(temp));
        sms_cart_init();
        memcpy(sram.sram, temp, sizeof(temp));
      }
    }
  }

  var.key = "dac_bits";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    if (!strcmp(var.value, "enabled"))
      config.dac_bits = 9;
    else
      config.dac_bits = 14;

    YM2612Config(config.dac_bits);
  }

  var.key = "blargg_ntsc_filter";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.ntsc;

    if (strcmp(var.value, "disabled") == 0)
      config.ntsc = 0;
    else if (strcmp(var.value, "monochrome") == 0)
    {
      config.ntsc = 1;
      sms_ntsc_init(sms_ntsc, &sms_ntsc_monochrome);
      md_ntsc_init(md_ntsc,   &md_ntsc_monochrome);
    }
    else if (strcmp(var.value, "composite") == 0)
    {
      config.ntsc = 1;
      sms_ntsc_init(sms_ntsc, &sms_ntsc_composite);
      md_ntsc_init(md_ntsc,   &md_ntsc_composite);
    }
    else if (strcmp(var.value, "svideo") == 0)
    {
      config.ntsc = 1;
      sms_ntsc_init(sms_ntsc, &sms_ntsc_svideo);
      md_ntsc_init(md_ntsc,   &md_ntsc_svideo);
    }
    else if (strcmp(var.value, "rgb") == 0)
    {
      config.ntsc = 1;
      sms_ntsc_init(sms_ntsc, &sms_ntsc_rgb);
      md_ntsc_init(md_ntsc,   &md_ntsc_rgb);
    }

    if (orig_value != config.ntsc)
      update_viewports = true;
  }

  var.key = "overscan";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.overscan;
    if (strcmp(var.value, "disabled") == 0)
      config.overscan = 0;
    else if (strcmp(var.value, "top/bottom") == 0)
      config.overscan = 1;
    else if (strcmp(var.value, "left/right") == 0)
      config.overscan = 2;
    else if (strcmp(var.value, "full") == 0)
      config.overscan = 3;
    if (orig_value != config.overscan)
      update_viewports = true;
  }

  var.key = "gg_extra";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.gg_extra;
    if (strcmp(var.value, "disabled") == 0)
      config.gg_extra = 0;
    else if (strcmp(var.value, "enabled") == 0)
      config.gg_extra = 1;
    if (orig_value != config.gg_extra)
      update_viewports = true;
  }

  var.key = "render";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    orig_value = config.render;
    if (strcmp(var.value, "normal") == 0)
      config.render = 0;
    else
      config.render = 1;
    if (orig_value != config.render)
      update_viewports = true;
  }

  var.key = "gun_cursor";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    if (strcmp(var.value, "off") == 0)
      config.gun_cursor = 0;
    else
      config.gun_cursor = 1;
  }

  var.key = "invert_mouse";
  environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
  {
    if (strcmp(var.value, "off") == 0)
      config.invert_mouse = 0;
    else
      config.invert_mouse = 1;
  }

  if (reinit)
  {
    audio_init(44100, snd.frame_rate);
    memcpy(temp, sram.sram, sizeof(temp));
    system_init();
    system_reset();
    memcpy(sram.sram, temp, sizeof(temp));
  }

  if (update_viewports)
  {
    bitmap.viewport.changed = 3;
    if ((system_hw == SYSTEM_GG) && !config.gg_extra)
      bitmap.viewport.x = (config.overscan & 2) ? 14 : -48;
    else
      bitmap.viewport.x = (config.overscan & 2) * 7;
  }
}

/************************************
 * libretro implementation
 ************************************/
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { "system_hw", "System hardware; auto|sg-1000|sg-1000 II|mark-III|master system|master system II|game gear|mega drive / genesis" },
      { "region_detect", "System region; auto|ntsc-u|pal|ntsc-j" },
      { "force_dtack", "System lockups; enabled|disabled" },
      { "addr_error", "68k address error; enabled|disabled" },
      { "lock_on", "Cartridge lock-on; disabled|game genie|action replay (pro)|sonic & knuckles" },
      { "ym2413", "Master System FM; auto|disabled|enabled" },
      { "dac_bits", "YM2612 DAC quantization; disabled|enabled" },
      { "blargg_ntsc_filter", "Blargg NTSC filter; disabled|monochrome|composite|svideo|rgb" },
      { "overscan", "Borders; disabled|top/bottom|left/right|full" },
      { "gg_extra", "Game Gear extended screen; disabled|enabled" },
      { "render", "Interlaced mode 2 output; single field|double field" },
      { "gun_cursor", "Show Lightgun crosshair; no|yes" },
      { "invert_mouse", "Invert Mouse Y-axis; no|yes" },
      { NULL, NULL },
   };

   static const struct retro_controller_description port_1[] = {
      { "Joypad Auto", RETRO_DEVICE_JOYPAD },
      { "Joypad Port Empty", RETRO_DEVICE_NONE },
      { "MD Joypad 3 Button", RETRO_DEVICE_MDPAD_3B },
      { "MD Joypad 6 Button", RETRO_DEVICE_MDPAD_6B },
      { "MS Joypad 2 Button", RETRO_DEVICE_MSPAD_2B },
      { "MD Joypad 3 Button + 4-WayPlay", RETRO_DEVICE_MDPAD_3B_WAYPLAY },
      { "MD Joypad 6 Button + 4-WayPlay", RETRO_DEVICE_MDPAD_6B_WAYPLAY },
      { "MD Joypad 3 Button + Teamplayer", RETRO_DEVICE_MDPAD_3B_TEAMPLAYER },
      { "MD Joypad 6 Button + Teamplayer", RETRO_DEVICE_MDPAD_6B_TEAMPLAYER },
      { "MS Joypad 2 Button + Master Tap", RETRO_DEVICE_MSPAD_2B_MASTERTAP },
      { "MS Light Phaser", RETRO_DEVICE_PHASER },
      { "MS Paddle Control", RETRO_DEVICE_PADDLE },
      { "MS Sports Pad", RETRO_DEVICE_SPORTSPAD },
      { "MD XE-1AP", RETRO_DEVICE_XE_1AP },
      { "MD Mouse", RETRO_DEVICE_MOUSE },
      { "Alt Joypad 6 Button YXL1/BAR1", RETRO_DEVICE_MDPAD_6B_ALT1 },
      { "Alt Joypad 6 Button YXR1/BAR2", RETRO_DEVICE_MDPAD_6B_ALT2 }, 
   };

   static const struct retro_controller_description port_2[] = {
      { "Joypad Auto", RETRO_DEVICE_JOYPAD },
      { "Joypad Port Empty", RETRO_DEVICE_NONE },
      { "MD Joypad 3 Button", RETRO_DEVICE_MDPAD_3B },
      { "MD Joypad 6 Button", RETRO_DEVICE_MDPAD_6B },
      { "MS Joypad 2 Button", RETRO_DEVICE_MSPAD_2B },
      { "MD Joypad 3 Button + 4-WayPlay", RETRO_DEVICE_MDPAD_3B_WAYPLAY },
      { "MD Joypad 6 Button + 4-WayPlay", RETRO_DEVICE_MDPAD_6B_WAYPLAY },
      { "MD Joypad 3 Button + Teamplayer", RETRO_DEVICE_MDPAD_3B_TEAMPLAYER },
      { "MD Joypad 6 Button + Teamplayer", RETRO_DEVICE_MDPAD_6B_TEAMPLAYER },
      { "MS Joypad 2 Button + Master Tap", RETRO_DEVICE_MSPAD_2B_MASTERTAP },
      { "MD Menacer", RETRO_DEVICE_MENACER },
      { "MD Justifiers", RETRO_DEVICE_JUSTIFIERS },
      { "MS Light Phaser", RETRO_DEVICE_PHASER },
      { "MS Paddle Control", RETRO_DEVICE_PADDLE },
      { "MS Sports Pad", RETRO_DEVICE_SPORTSPAD },
      { "MD XE-1AP", RETRO_DEVICE_XE_1AP },
      { "MD Mouse", RETRO_DEVICE_MOUSE },
      { "Alt Joypad 6 Button YXL1/BAR1", RETRO_DEVICE_MDPAD_6B_ALT1 },
      { "Alt Joypad 6 Button YXR1/BAR2", RETRO_DEVICE_MDPAD_6B_ALT2 },
  };

   static const struct retro_controller_info ports[] = {
      { port_1, 17 },
      { port_2, 19 },
      { 0 },
   };

   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "Genesis Plus GX";
   info->library_version = "v1.7.4";
   info->valid_extensions = "mdx|md|smd|gen|bin|cue|iso|sms|gg|sg";
   info->block_extract = false;
   info->need_fullpath = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width    = vwidth;
   info->geometry.base_height   = vheight;
   info->geometry.max_width     = 720;
   info->geometry.max_height    = 576;
   info->geometry.aspect_ratio  = 4.0 / 3.0;
   info->timing.fps             = snd.frame_rate;
   info->timing.sample_rate     = 44100;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   switch(device)
   {
      case RETRO_DEVICE_NONE:
         input.system[port] = NO_SYSTEM;
         break;
      case RETRO_DEVICE_MDPAD_3B:
         config.input[port*4].padtype = DEVICE_PAD3B;
         input.system[port] = SYSTEM_GAMEPAD;
         break;
      case RETRO_DEVICE_MDPAD_6B:
         config.input[port*4].padtype = DEVICE_PAD6B;
         input.system[port] = SYSTEM_GAMEPAD;
         break;
      case RETRO_DEVICE_MSPAD_2B:
         config.input[port*4].padtype = DEVICE_PAD2B;
         input.system[port] = SYSTEM_GAMEPAD;
         break;
      case RETRO_DEVICE_MDPAD_3B_WAYPLAY:
      {
         int i;
         for (i=0; i<4; i++)
         {
            config.input[i].padtype = DEVICE_PAD3B;
         }
         input.system[0] = input.system[1] = SYSTEM_WAYPLAY;
         break;
      }
      case RETRO_DEVICE_MDPAD_6B_WAYPLAY:
      {
         int i;
         for (i=0; i<4; i++)
         {
            config.input[i].padtype = DEVICE_PAD6B;
         }
         input.system[0] = input.system[1] = SYSTEM_WAYPLAY;
         break;
      }
      case RETRO_DEVICE_MDPAD_3B_TEAMPLAYER:
      {
         int i;
         for (i=0; i<4; i++)
         {
            config.input[port*4 + i].padtype = DEVICE_PAD3B;
         }
         input.system[port] = SYSTEM_TEAMPLAYER;
         break;
      }
      case RETRO_DEVICE_MDPAD_6B_TEAMPLAYER:
      {
         int i;
         for (i=0; i<4; i++)
         {
            config.input[port*4 + i].padtype = DEVICE_PAD6B;
         }
         input.system[port] = SYSTEM_TEAMPLAYER;
         break;
      }
      case RETRO_DEVICE_MSPAD_2B_MASTERTAP:
      {
         int i;
         for (i=0; i<4; i++)
         {
            config.input[port*4 + i].padtype = DEVICE_PAD2B;
         }
         input.system[port] = SYSTEM_MASTERTAP;
         break;
      }
      case RETRO_DEVICE_MENACER:
         input.system[1] = SYSTEM_MENACER;
         break;
      case RETRO_DEVICE_JUSTIFIERS:
         input.system[1] = SYSTEM_JUSTIFIER;
         break;
      case RETRO_DEVICE_PHASER:
         input.system[port] = SYSTEM_LIGHTPHASER;
         break;
      case RETRO_DEVICE_PADDLE:
         input.system[port] = SYSTEM_PADDLE;
         break;
      case RETRO_DEVICE_SPORTSPAD:
         input.system[port] = SYSTEM_SPORTSPAD;
         break;
      case RETRO_DEVICE_XE_1AP:
         input.system[port] = SYSTEM_XE_1AP;
         break;
      case RETRO_DEVICE_MOUSE:
         input.system[port] = SYSTEM_MOUSE;
         break;
      case RETRO_DEVICE_MDPAD_6B_ALT1:
         config.input[port*4].padtype = DEVICE_PAD6B;
         input.system[port] = SYSTEM_GAMEPAD;
         mdalt1 = true;
         break;
      case RETRO_DEVICE_MDPAD_6B_ALT2:
         config.input[port*4].padtype = DEVICE_PAD6B;
         input.system[port] = SYSTEM_GAMEPAD;
         mdalt2 = true;
         break;
      case RETRO_DEVICE_JOYPAD:
      default:
         config.input[port*4].padtype = DEVICE_PAD2B | DEVICE_PAD6B | DEVICE_PAD3B;
         input.system[port] = SYSTEM_GAMEPAD;
         break;
   }

   old_system[0] = input.system[0];
   old_system[1] = input.system[1];
   
   io_init();
   input_reset();
}

size_t retro_serialize_size(void) { return STATE_SIZE; }

bool retro_serialize(void *data, size_t size)
{ 
   if (size != STATE_SIZE)
      return FALSE;

   state_save(data);

   return TRUE;
}

bool retro_unserialize(const void *data, size_t size)
{
   if (size != STATE_SIZE)
      return FALSE;

   if (!state_load((uint8_t*)data))
      return FALSE;

   return TRUE;
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

bool retro_load_game(const struct retro_game_info *info)
{
   int i;
   const char *dir;
#if defined(_WIN32)
   char slash = '\\';
#else
   char slash = '/';
#endif

   extract_directory(g_rom_dir, info->path, sizeof(g_rom_dir));

   if (!environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) || !dir)
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "[genplus]: Defaulting system directory to %s.\n", g_rom_dir);
      dir = g_rom_dir;
   }

   snprintf(GG_ROM, sizeof(GG_ROM), "%s%cggenie.bin", dir, slash);
   snprintf(AR_ROM, sizeof(AR_ROM), "%s%careplay.bin", dir, slash);
   snprintf(SK_ROM, sizeof(SK_ROM), "%s%csk.bin", dir, slash);
   snprintf(SK_UPMEM, sizeof(SK_UPMEM), "%s%cs2k.bin", dir, slash);
   snprintf(CD_BIOS_EU, sizeof(CD_BIOS_EU), "%s%cbios_CD_E.bin", dir, slash);
   snprintf(CD_BIOS_US, sizeof(CD_BIOS_US), "%s%cbios_CD_U.bin", dir, slash);
   snprintf(CD_BIOS_JP, sizeof(CD_BIOS_JP), "%s%cbios_CD_J.bin", dir, slash);
   snprintf(CD_BRAM_EU, sizeof(CD_BRAM_EU), "%s%cscd_E.brm", dir, slash);
   snprintf(CD_BRAM_US, sizeof(CD_BRAM_US), "%s%cscd_U.brm", dir, slash);
   snprintf(CD_BRAM_JP, sizeof(CD_BRAM_JP), "%s%cscd_J.brm", dir, slash);
   snprintf(CART_BRAM, sizeof(CART_BRAM), "%s%ccart.brm", dir, slash);
   if (log_cb)
   {
      log_cb(RETRO_LOG_INFO, "Game Genie ROM should be located at: %s\n", GG_ROM);
      log_cb(RETRO_LOG_INFO, "Action Replay (Pro) ROM should be located at: %s\n", AR_ROM);
      log_cb(RETRO_LOG_INFO, "Sonic & Knuckles (2 MB) ROM should be located at: %s\n", SK_ROM);
      log_cb(RETRO_LOG_INFO, "Sonic & Knuckles UPMEM (256 KB) ROM should be located at: %s\n", SK_UPMEM);
      log_cb(RETRO_LOG_INFO, "Mega CD PAL BIOS should be located at: %s\n", CD_BIOS_EU);
      log_cb(RETRO_LOG_INFO, "Sega CD NTSC-U BIOS should be located at: %s\n", CD_BIOS_US);
      log_cb(RETRO_LOG_INFO, "Mega CD NTSC-J BIOS should be located at: %s\n", CD_BIOS_JP);
      log_cb(RETRO_LOG_INFO, "Mega CD PAL BRAM is located at: %s\n", CD_BRAM_EU);
      log_cb(RETRO_LOG_INFO, "Sega CD NTSC-U BRAM is located at: %s\n", CD_BRAM_US);
      log_cb(RETRO_LOG_INFO, "Mega CD NTSC-J BRAM is located at: %s\n", CD_BRAM_JP);
      log_cb(RETRO_LOG_INFO, "Mega CD RAM CART is located at: %s\n", CART_BRAM);
   }

   check_variables();
   if (!load_rom((char *)info->path))
      return false;

   audio_init(44100, vdp_pal ? pal_fps : ntsc_fps);
   system_init();
   system_reset();

   if (system_hw == SYSTEM_MCD)
     bram_load();

   update_viewport();

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return FALSE;
}

void retro_unload_game(void) 
{
   if (system_hw == SYSTEM_MCD)
      bram_save();
}

unsigned retro_get_region(void) { return vdp_pal ? RETRO_REGION_PAL : RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned id)
{
   if (!sram.on)
      return NULL;

   switch (id)
   {
      case RETRO_MEMORY_SAVE_RAM:
         return sram.sram;

      default:
         return NULL;
   }
}

size_t retro_get_memory_size(unsigned id)
{
   if (!sram.on)
      return 0;

   switch (id)
   {
      case RETRO_MEMORY_SAVE_RAM:
         return 0x10000;

      default:
         return 0;
   }
}

static void check_system_specs(void)
{
   unsigned level = 7;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

void retro_init(void)
{
   struct retro_log_callback log;
   unsigned level, rgb565;
   sms_ntsc = calloc(1, sizeof(sms_ntsc_t));
   md_ntsc  = calloc(1, sizeof(md_ntsc_t));

   init_bitmap();
   config_default();

   level = 1;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

#ifdef FRONTEND_SUPPORTS_RGB565
   rgb565 = RETRO_PIXEL_FORMAT_RGB565;
   if(environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565))
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
#endif
   check_system_specs();
}

void retro_deinit(void)
{
   audio_shutdown();
   if (md_ntsc)
      free(md_ntsc);
   if (sms_ntsc)
      free(sms_ntsc);

}

void retro_reset(void) { system_reset(); }

void retro_run(void) 
{
   bool updated = false;

   if (system_hw == SYSTEM_MCD)
      system_frame_scd(0);
   else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
      system_frame_gen(0);
   else
      system_frame_sms(0);

   if (bitmap.viewport.changed & 1)
   {
      bitmap.viewport.changed &= ~1;
      if (update_viewport())
      {
         struct retro_system_av_info info;
         retro_get_system_av_info(&info);
         environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info.geometry);
      }
   }

   video_cb(bitmap.data, vwidth, vheight, 720 * 2);
   audio_cb(soundbuffer, audio_update(soundbuffer));

   environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated);
   if (updated)
      check_variables();
}

#undef  CHUNKSIZE
