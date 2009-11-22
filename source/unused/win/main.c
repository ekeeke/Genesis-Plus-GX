#include <windows.h>

#include "SDL.h"
#include "SDL_thread.h"

#include "shared.h"
#include "sms_ntsc.h"
#include "md_ntsc.h"

#define SOUND_FREQUENCY 48000
#define SOUND_SAMPLES_SIZE  2048

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480

int joynum = 0;

int log_error   = 0;
int debug_on    = 0;
int turbo_mode  = 0;
int use_sound   = 1;
int fullscreen  = 0; /* SDL_FULLSCREEN */

/* sound */

struct {
  char* current_pos;
  char* buffer;
  int current_emulated_samples;
} sdl_sound;

static void sdl_sound_callback(void *userdata, Uint8 *stream, int len)
{
  if(sdl_sound.current_emulated_samples < len) {
    memset(stream, 0, len);
  }
  else {
    memcpy(stream, sdl_sound.buffer, len);
    /* loop to compensate desync */
    do {
      sdl_sound.current_emulated_samples -= len;
    } while(sdl_sound.current_emulated_samples > 2 * len);
    memcpy(sdl_sound.buffer,
           sdl_sound.current_pos - sdl_sound.current_emulated_samples,
           sdl_sound.current_emulated_samples);
    sdl_sound.current_pos = sdl_sound.buffer + sdl_sound.current_emulated_samples;
  }
}

static int sdl_sound_init()
{
  int n;
  SDL_AudioSpec as_desired, as_obtained;
  
  if(SDL_Init(SDL_INIT_AUDIO) < 0) {
    MessageBox(NULL, "SDL Audio initialization failed", "Error", 0);
    return 0;
  }

  as_desired.freq     = SOUND_FREQUENCY;
  as_desired.format   = AUDIO_S16LSB;
  as_desired.channels = 2;
  as_desired.samples  = SOUND_SAMPLES_SIZE;
  as_desired.callback = sdl_sound_callback;

  if(SDL_OpenAudio(&as_desired, &as_obtained) == -1) {
    MessageBox(NULL, "SDL Audio open failed", "Error", 0);
    return 0;
  }

  if(as_desired.samples != as_obtained.samples) {
    MessageBox(NULL, "SDL Audio wrong setup", "Error", 0);
    return 0;
  }

  sdl_sound.current_emulated_samples = 0;
  n = SOUND_SAMPLES_SIZE * 2 * sizeof(short) * 11;
  sdl_sound.buffer = (char*)malloc(n);
  if(!sdl_sound.buffer) {
    MessageBox(NULL, "Can't allocate audio buffer", "Error", 0);
    return 0;
  }
  memset(sdl_sound.buffer, 0, n);
  sdl_sound.current_pos = sdl_sound.buffer;
  return 1;
}

static void sdl_sound_update()
{
  int i;
  short* p;

  SDL_LockAudio();
  p = (short*)sdl_sound.current_pos;
  for(i = 0; i < snd.buffer_size; ++i) {
      *p = snd.buffer[0][i];
      ++p;
      *p = snd.buffer[1][i];
      ++p;
  }
  sdl_sound.current_pos = (char*)p;
  sdl_sound.current_emulated_samples += snd.buffer_size * 2 * sizeof(short);
  SDL_UnlockAudio();
}

static void sdl_sound_close()
{
  SDL_PauseAudio(1);
  SDL_CloseAudio();
  if (sdl_sound.buffer) 
    free(sdl_sound.buffer);
}

/* video */
md_ntsc_t md_ntsc;
sms_ntsc_t sms_ntsc;

struct {
  SDL_Surface* surf_screen;
  SDL_Surface* surf_bitmap;
  Uint32 frames_rendered;
} sdl_video;

static int sdl_video_init()
{
  if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
    MessageBox(NULL, "SDL Video initialization failed", "Error", 0);
    return 0;
  }
  sdl_video.surf_screen  = SDL_SetVideoMode(VIDEO_WIDTH, VIDEO_HEIGHT, 16, SDL_SWSURFACE | fullscreen);
  sdl_video.surf_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, 720, 576, 16, 0, 0, 0, 0);
  sdl_video.frames_rendered = 0;
  SDL_WM_SetCaption("Genesis Plus/SDL", NULL);
  SDL_ShowCursor(0);

  return 1;
}

static void sdl_video_update()
{
  SDL_Rect rect;

  system_frame(0);

  /* viewport size changed */
  if(bitmap.viewport.changed)
  {
    bitmap.viewport.changed = 0;
    rect.w = bitmap.viewport.w+2*bitmap.viewport.x;
    rect.h = bitmap.viewport.h+2*bitmap.viewport.y;
    if (config.render && (interlaced || config.ntsc))  rect.h *= 2;
    if (config.ntsc) rect.w = (reg[12]&1) ? MD_NTSC_OUT_WIDTH(rect.w) : SMS_NTSC_OUT_WIDTH(rect.w);
    rect.x=(VIDEO_WIDTH-rect.w)/2;
    rect.y=(VIDEO_HEIGHT-rect.h)/2;

    /* init NTSC filter */
    md_ntsc_setup_t md_setup;
    sms_ntsc_setup_t sms_setup;
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
  }

  SDL_BlitSurface(sdl_video.surf_bitmap, NULL, sdl_video.surf_screen, &rect);
  SDL_UpdateRect(sdl_video.surf_screen, 0, 0, 0, 0);

  ++sdl_video.frames_rendered;

}

static void sdl_video_close()
{
  if (sdl_video.surf_bitmap) SDL_FreeSurface(sdl_video.surf_bitmap);
  if (sdl_video.surf_screen) SDL_FreeSurface(sdl_video.surf_screen);
}

/* Timer Sync */

struct {
  SDL_sem* sem_sync;
} sdl_sync;

/* sync */

static Uint32 sdl_sync_timer_callback(Uint32 interval)
{
  /*char caption[100];
  char region[10];
  if (region_code == REGION_USA) sprintf(region,"USA");
  else if (region_code == REGION_EUROPE) sprintf(region,"EUR");
  else sprintf(region,"JAP");
  sprintf(caption, "Genesis Plus/SDL - %s (%s) - %d fps - 0x%04X", rominfo.international, region, vdp_rate, realchecksum);
  SDL_WM_SetCaption(caption, NULL);*/
  SDL_SemPost(sdl_sync.sem_sync);
  return interval;
}

static int sdl_sync_init()
{
  if(SDL_InitSubSystem(SDL_INIT_TIMER|SDL_INIT_EVENTTHREAD) < 0) {
    MessageBox(NULL, "SDL Timer initialization failed", "Error", 0);
    return 0;
  }
  sdl_sync.sem_sync = SDL_CreateSemaphore(0);
  return 1;
}

static void sdl_sync_close()
{
  if(sdl_sync.sem_sync)
    SDL_DestroySemaphore(sdl_sync.sem_sync);
}

static int sdl_control_update(SDLKey keystate)
{
    switch (keystate)
    {
      case SDLK_TAB:
      {
        system_reset();
        break;
      }

      case SDLK_F2:
      {
        if (fullscreen) fullscreen = 0;
        else fullscreen = SDL_FULLSCREEN;
        sdl_video.surf_screen = SDL_SetVideoMode(VIDEO_WIDTH, VIDEO_HEIGHT, 16,  SDL_SWSURFACE | fullscreen);
        break;
      }

      case SDLK_F3:
      {
        config.render ^=1;
        break;
      }

      case SDLK_F4:
      {
        SDL_FillRect(sdl_video.surf_screen, 0, 0);
        config.ntsc ++;
        if (config.ntsc > 3) config.ntsc = 0;
        bitmap.viewport.changed = 1;
        break;
      }

      case SDLK_F5:
      {
        log_error ^= 1;
        break;
      }

      case SDLK_F6:
      {
        turbo_mode ^=1;
        break;
      }

      case SDLK_F7:
      {
        FILE *f = fopen("game.gpz","r+b");
        if (f)
        {
          uint8 buf[STATE_SIZE];
          fread(&buf, STATE_SIZE, 1, f);
          state_load(buf);
          fclose(f);
        }
        break;
      }

      case SDLK_F8:
      {
        FILE *f = fopen("game.gpz","w+b");
        if (f)
        {
          uint8 buf[STATE_SIZE];
          state_save(buf);
          fwrite(&buf, STATE_SIZE, 1, f);
          fclose(f);
        }
        break;
      }

      case SDLK_F9:
      {
        vdp_pal ^= 1;

        /* reinitialize timings */
        system_init ();
        unsigned char *temp = malloc(YM2612GetContextSize());
        if (temp) memcpy(temp, YM2612GetContextPtr(), YM2612GetContextSize());
        audio_init(SOUND_FREQUENCY);
        if (temp)
        {
          YM2612Restore(temp);
          free(temp);
        }

        /* reinitialize HVC tables */
        vctab = (vdp_pal) ? ((reg[1] & 8) ? vc_pal_240 : vc_pal_224) : vc_ntsc_224;
        hctab = (reg[12] & 1) ? cycle2hc40 : cycle2hc32;

        /* reinitialize overscan area */
        bitmap.viewport.x = config.overscan ? 14 : 0;
        bitmap.viewport.y = config.overscan ? (((reg[1] & 8) ? 0 : 8) + (vdp_pal ? 24 : 0)) : 0;

        break;
      }

      case SDLK_F10:
      {
        set_softreset();
        break;
      }

      case SDLK_F11:
      {
        joynum ++;
        if (joynum > MAX_DEVICES - 1)
          joynum = 0;
        break;
      }

      case SDLK_F12:
      {
        config.overscan ^= 1;
        bitmap.viewport.x = config.overscan ? ((reg[12] & 1) ? 16 : 12) : 0;
        bitmap.viewport.y = config.overscan ? (((reg[1] & 8) ? 0 : 8) + (vdp_pal ? 24 : 0)) : 0;
        bitmap.viewport.changed = 1;
        break;
      }

      case SDLK_ESCAPE:
      {
        return 0;
      }

      default:
        break;
    }

   return 1;
}

int sdl_input_update(void)
{
  uint8 *keystate = SDL_GetKeyState(NULL);
  while (input.dev[joynum] == NO_DEVICE)
  {
    joynum ++;
    if (joynum > MAX_DEVICES - 1) joynum = 0;
  }

  /* reset input */
  input.pad[joynum] = 0;

  /* keyboard */
  if(keystate[SDLK_a])  input.pad[joynum] |= INPUT_A;
  if(keystate[SDLK_s])  input.pad[joynum] |= INPUT_B;
  if(keystate[SDLK_d])  input.pad[joynum] |= INPUT_C;
  if(keystate[SDLK_f])  input.pad[joynum] |= INPUT_START;
  if(keystate[SDLK_z])  input.pad[joynum] |= INPUT_X;
  if(keystate[SDLK_x])  input.pad[joynum] |= INPUT_Y;
  if(keystate[SDLK_c])  input.pad[joynum] |= INPUT_Z;
  if(keystate[SDLK_v])  input.pad[joynum] |= INPUT_MODE;
 
  switch (input.dev[joynum])
  {
    case DEVICE_LIGHTGUN:
    {
      /* get mouse (absolute values) */
      int x,y;
      int state = SDL_GetMouseState(&x,&y);

      /* Calculate X Y axis values */
      input.analog[joynum - 4][0] = (x * bitmap.viewport.w) / 640;
      input.analog[joynum - 4][1] = (y * bitmap.viewport.h) / 480;

      /* Map mouse buttons to player #1 inputs */
      if(state & SDL_BUTTON_MMASK) input.pad[joynum] |= INPUT_C;
      if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_B;
      if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_A;

      break;
    }

    case DEVICE_MOUSE:
    {
      /* get mouse (relative values) */
      int x,y;
      int state = SDL_GetRelativeMouseState(&x,&y);

      /* Sega Mouse range is -256;+256 */
      input.analog[2][0] = x;
      input.analog[2][1] = y;

      /* Vertical movement is upsidedown */
      if (!config.invert_mouse) input.analog[2][1] = 0 - input.analog[2][1];

      /* Map mouse buttons to player #1 inputs */
      if(state & SDL_BUTTON_MMASK) input.pad[joynum] |= INPUT_C;
      if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_B;
      if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_A;
    
      break;
    }

    default:
    {
      if(keystate[SDLK_UP])     input.pad[joynum] |= INPUT_UP;
      else
      if(keystate[SDLK_DOWN])   input.pad[joynum] |= INPUT_DOWN;
      if(keystate[SDLK_LEFT])   input.pad[joynum] |= INPUT_LEFT;
      else
      if(keystate[SDLK_RIGHT])  input.pad[joynum] |= INPUT_RIGHT;

      break;
    }
  }

  if (system_hw == SYSTEM_PICO)
  {
    /* get mouse (absolute values) */
    int x,y;
    int state = SDL_GetMouseState(&x,&y);

    /* Calculate X Y axis values */
    input.analog[0][0] = 0x3c  + (x * (0x17c-0x03c+1)) / 640;
    input.analog[0][1] = 0x1fc + (y * (0x2f7-0x1fc+1)) / 480;
 
    /* Map mouse buttons to player #1 inputs */
    if(state & SDL_BUTTON_MMASK) pico_current++;
    if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_B;
    if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_A;
  }

  free (keystate);
  return 1;
}


int main (int argc, char **argv)
{
  int running = 1;

  /* Print help if no game specified */
  if(argc < 2)
  {
    char caption[256];
    sprintf(caption, "Genesis Plus\\SDL by Charles MacDonald\nWWW: http://cgfm2.emuviews.com\nusage: %s gamename\n", argv[0]);
    MessageBox(NULL, caption, "Information", 0);
    exit(1);
  }

  /* set default config */
  error_init();
  set_config_defaults();

  /* Load ROM file */
  cart.rom = malloc(10*1024*1024);
  memset(cart.rom, 0, 10*1024*1024);
  if(!load_rom(argv[1]))
  {
    char caption[256];
    sprintf(caption, "Error loading file `%s'.", argv[1]);
    MessageBox(NULL, caption, "Error", 0);
    exit(1);
  }

  /* load BIOS */
  memset(bios_rom, 0, sizeof(bios_rom));
  FILE *f = fopen(OS_ROM, "rb");
  if (f!=NULL)
  {
    fread(&bios_rom, 0x800,1,f);
    fclose(f);
    int i;
    for(i = 0; i < 0x800; i += 2)
    {
      uint8 temp = bios_rom[i];
      bios_rom[i] = bios_rom[i+1];
      bios_rom[i+1] = temp;
    }
    config.bios_enabled |= 2;
  }

  /* initialize SDL */
  if(SDL_Init(0) < 0)
  {
    char caption[256];
    sprintf(caption, "SDL initialization failed");
    MessageBox(NULL, caption, "Error", 0);
    exit(1);
  }
  sdl_video_init();
  if (use_sound)
    sdl_sound_init();
  sdl_sync_init();

  /* initialize Genesis virtual system */
  SDL_LockSurface(sdl_video.surf_bitmap);
  memset(&bitmap, 0, sizeof(t_bitmap));
  bitmap.width        = 720;
  bitmap.height       = 576;
  bitmap.depth        = 16;
  bitmap.granularity  = 2;
  bitmap.pitch        = (bitmap.width * bitmap.granularity);
  bitmap.data         = sdl_video.surf_bitmap->pixels;
  SDL_UnlockSurface(sdl_video.surf_bitmap);

  /* initialize emulation */
  system_init();
  audio_init(SOUND_FREQUENCY);

  /* load SRAM */
  f = fopen("./game.srm", "rb");
  if (f!=NULL)
  {
    fread(sram.sram,0x10000,1, f);
    fclose(f);
  }

  /* reset emulation */
  system_reset();

  if(use_sound)
    SDL_PauseAudio(0);

  if(sdl_sync.sem_sync)
    SDL_SetTimer(vdp_pal ? 60 : 50, sdl_sync_timer_callback); /* 3 frames = 50 ms (60hz) or 60 ms (50hz) */

  /* emulation loop */
  while(running)
  {
    SDL_Event event;
    if (SDL_PollEvent(&event)) 
    {
      switch(event.type) 
      {
        case SDL_QUIT:
          running = 0;
          break;

        case SDL_KEYDOWN:
          running = sdl_control_update(event.key.keysym.sym);
          break;
      }
    }

    sdl_video_update();
    audio_update(snd.buffer_size);
    if(use_sound)
      sdl_sound_update();

    if(!turbo_mode && sdl_sync.sem_sync && sdl_video.frames_rendered % 3 == 0)
    {
      SDL_SemWait(sdl_sync.sem_sync);
    }

  }

  /* save SRAM */
  f = fopen("./game.srm", "wb");
  if (f!=NULL)
  {
    fwrite(&sram.sram,0x10000,1, f);
    fclose(f);
  }

  system_shutdown();
  audio_shutdown();
  error_shutdown();
  free(cart.rom);

  sdl_video_close();
  if (use_sound)
   sdl_sound_close();
  sdl_sync_close();
  SDL_Quit();

  return 0;
}
