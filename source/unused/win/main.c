
#include <windows.h>
#include <SDL.h>
#include "shared.h"

#define SOUND_FREQUENCY    48000

int timer_count = 0;
int old_timer_count = 0;
int paused = 0;
int frame_count = 0;
int frameticker = 0;
int joynum = 0;

int update_input(void);
unsigned char *keystate;
unsigned char buf[0x24000];

static uint8 soundbuffer[2][3840];
static uint8 mixbuffer[16000];
static int mixhead = 0;
static int mixtail = 0;
static int whichab = 0;

uint8 log_error   = 0;
uint8 debug_on    = 0;
uint8 turbo_mode  = 0;
uint8 use_sound   = 1;    /* NOT WORKING */

static int mixercollect( uint8 *outbuffer, int len )
{
  uint32 *dst = (uint32 *)outbuffer;
  uint32 *src = (uint32 *)mixbuffer;
  int done = 0;

  memset(outbuffer, 0, len);

  while ( ( mixtail != mixhead ) && ( done < len ) )
  {
    *dst++ = src[mixtail++];
    if (mixtail == 4000) mixtail = 0;
    done += 4;
  }

  return done;
}

Uint32 fps_callback(Uint32 interval)
{
	if(paused) return 1000/vdp_rate;
	timer_count++;
  frameticker ++;
	if(timer_count % vdp_rate == 0)
	{
		int fps = frame_count + 1;
		char caption[100];
    char region[10];
    if (region_code == REGION_USA) sprintf(region,"USA");
    else if (region_code == REGION_EUROPE) sprintf(region,"EUR");
    else sprintf(region,"JAP");
    sprintf(caption, "Genesis Plus/SDL - %s (%s) - %d fps", rominfo.international, region, fps);
		SDL_WM_SetCaption(caption, NULL);
		frame_count = 0;
    
	}
	return 1000/vdp_rate;
}

static void sdl_sound_callback(void *userdata, Uint8 *stream, int len)
{
  int actuallen = mixercollect( soundbuffer[whichab], len );

  memcpy(stream, soundbuffer[whichab], len);
  whichab ^= 1;
}

int sdl_sound_init()
{
  SDL_InitSubSystem(SDL_INIT_AUDIO);

  SDL_AudioSpec as;
  
  as.freq = SOUND_FREQUENCY;
  as.format = AUDIO_S16;
  as.channels = 2;
  as.samples = snd.buffer_size;
  as.callback = sdl_sound_callback;

  if(SDL_OpenAudio(&as, 0) == -1)
  {
		char caption[256];
		sprintf(caption, "SDL open audio failed");
		MessageBox(NULL, caption, "Error", 0);
    return 0;
  }
  return 1;
}

static void sdl_sound_update()
{
  int i;
  uint32 *dst = (uint32 *)mixbuffer;

  for(i = 0; i < snd.buffer_size; ++i)
  {
    dst[mixhead++] =  (snd.buffer[0][i] << 16) | snd.buffer[1][i];
    if (mixhead == 4000) mixhead = 0;
  }
}

int main (int argc, char **argv)
{
	int running = 1;
  int sym;

  SDL_Rect viewport, display;
  SDL_Surface *bmp, *screen;
  SDL_Event event;

  error_init();

  /* Print help if no game specified */
	if(argc < 2)
	{
		char caption[256];
    sprintf(caption, "Genesis Plus\SDL by Charles MacDonald\nWWW: http://cgfm2.emuviews.com\nusage: %s gamename\n", argv[0]);
    MessageBox(NULL, caption, "Information", 0);
		exit(1);
	}

	/* Load game */
  cart_rom = malloc(0xA00000);
	memset(cart_rom, 0, 0xA00000);
  if(!load_rom(argv[1]))
	{
		char caption[256];
		sprintf(caption, "Error loading file `%s'.", argv[1]);
		MessageBox(NULL, caption, "Error", 0);
		exit(1);
	}
        
	/* load BIOS */
  memset(bios_rom, 0, sizeof(bios_rom));
	FILE *f = fopen("./BIOS.bin", "rb");
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
	else config.bios_enabled = 0;

  /* initialize SDL */
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
  {
		char caption[256];
		sprintf(caption, "SDL initialization failed");
		MessageBox(NULL, caption, "Error", 0);
    exit(1);
  }

  SDL_WM_SetCaption("Genesis Plus/SDL", NULL);
  SDL_ShowCursor(0);

  /* initialize SDL video */
	viewport.x = 0;
	viewport.y = 0;
  viewport.w = 640;
  viewport.h = 480;

  display.x = 0;
  display.y = 0;
	display.w = 256;
	display.h = 224;

  bmp = SDL_CreateRGBSurface(SDL_SWSURFACE, 360, 576, 16, 0xF800, 0x07E0, 0x001F, 0x0000);
  screen = SDL_SetVideoMode(viewport.w, viewport.h, 16,  SDL_SWSURFACE);
  if (!bmp || !screen)
  {
		char caption[256];
		sprintf(caption, "Video initialization failed");
		MessageBox(NULL, caption, "Error", 0);
		exit(1);
  }

  /* initialize Genesis display */
  memset(&bitmap, 0, sizeof(t_bitmap));
  bitmap.width  = 360;
  bitmap.height = 576;
  bitmap.depth  = 16;
  bitmap.granularity = 2;
  bitmap.pitch = (bitmap.width * bitmap.granularity);
  bitmap.data   = (unsigned char *)bmp->pixels;
  bitmap.viewport.w = 256;
  bitmap.viewport.h = 224;
  bitmap.viewport.x = 0;
  bitmap.viewport.y = 0;
  bitmap.remap = 1;

  /* set default config */
  set_config_defaults();
  input.system[0] = SYSTEM_GAMEPAD;
  input.system[1] = SYSTEM_GAMEPAD;

	/* initialize emulation */
  system_init();
  audio_init(48000);

  /* initialize SDL audio */
  if (use_sound) sdl_sound_init();

  /* load SRAM */
  f = fopen("./game.srm", "rb");
  if (f!=NULL)
  {
    fread(&sram.sram,0x10000,1, f);
	  fclose(f);
  }

  /* reset emulation */
  system_reset();

  /* start emulation loop */
  SDL_SetTimer(1000/vdp_rate, fps_callback);
  if (use_sound) SDL_PauseAudio(1);

	while(running)
	{
		while (SDL_PollEvent(&event)) 
		{
			switch(event.type) 
			{
				case SDL_QUIT: /* Windows was closed */
					running = 0;
					break;

				case SDL_ACTIVEEVENT: /* Window focus changed or was minimized */
					if(event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE))
					{
						paused = !event.active.gain;
            if (use_sound) SDL_PauseAudio(paused);
					}
					break;

        case SDL_KEYDOWN:   /* user options */
          sym = event.key.keysym.sym;

          if(sym == SDLK_TAB)    system_reset();
          else if(sym == SDLK_F5)     log_error ^=1;
          else if(sym == SDLK_F6)
          {
            turbo_mode ^=1;
            frameticker = 0;
          }
          else if(sym == SDLK_F7)
          {
            f = fopen("game.gpz","r+b");
            if (f)
            {
              fread(&buf, 0x23000, 1, f);
              state_load(buf);
              fclose(f);
            }
          }
          else if(sym == SDLK_F8)
          {
            f = fopen("game.gpz","w+b");
            if (f)
            {
              state_save(buf);
              fwrite(&buf, 0x23000, 1, f);
              fclose(f);
            }
          }
          else if(sym == SDLK_F9)     
          {
            vdp_pal ^= 1;

            /* reinitialize timings */
            system_init ();
            audio_init(48000);
            fm_restore();
                            
            /* reinitialize HVC tables */
            vctab = (vdp_pal) ? ((reg[1] & 8) ? vc_pal_240 : vc_pal_224) : vc_ntsc_224;
            hctab = (reg[12] & 1) ? cycle2hc40 : cycle2hc32;

            /* reinitialize overscan area */
            bitmap.viewport.x = config.overscan ? ((reg[12] & 1) ? 16 : 12) : 0;
            bitmap.viewport.y = config.overscan ? (((reg[1] & 8) ? 0 : 8) + (vdp_pal ? 24 : 0)) : 0;
            bitmap.viewport.changed = 1;
          }
          else if(sym == SDLK_F10) set_softreset();
          else if(sym == SDLK_F11)
          {
            joynum ++;
            if (joynum > MAX_DEVICES - 1) joynum = 0;
            while (input.dev[joynum] == NO_DEVICE)
            {
              joynum ++;
              if (joynum > MAX_DEVICES - 1) joynum = 0;
            }
          }
          else if(sym == SDLK_ESCAPE) running = 0;
          else if(sym == SDLK_F12)
          {
            config.overscan ^= 1;
            bitmap.viewport.x = config.overscan ? ((reg[12] & 1) ? 16 : 12) : 0;
            bitmap.viewport.y = config.overscan ? (((reg[1] & 8) ? 0 : 8) + (vdp_pal ? 24 : 0)) : 0;
            bitmap.viewport.changed = 1;
          }
          break;

				default:
					break;
		  }
	  }

    if(!paused)
    {
      if (frameticker > 1)
      {
        /* Frame skipping */
        frameticker--;
        system_frame (1);
      }
      else
      {
        /* Delay */
        while (!frameticker && !turbo_mode) SDL_Delay(1);
         
        system_frame (0);
        frame_count++;
      }

      frameticker--;

      if(bitmap.viewport.changed)
      {
         bitmap.viewport.changed = 0;
        display.w = (bitmap.viewport.w + 2*bitmap.viewport.x);
        display.h = (bitmap.viewport.h + 2 * bitmap.viewport.y) << ((config.render && interlaced) ? 1:0);
        viewport.w = bitmap.viewport.w;
        viewport.h = bitmap.viewport.h;
         viewport.x = (640 - viewport.w)/2;
        viewport.y = (480 - viewport.h)/2;
      }

      if (use_sound) sdl_sound_update();
      SDL_BlitSurface(bmp, &display, screen, &viewport);
      SDL_UpdateRect(screen, viewport.x, viewport.y, viewport.w, viewport.h);
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
  SDL_PauseAudio(1);
  SDL_CloseAudio();
  SDL_FreeSurface(bmp);
  SDL_FreeSurface(screen);
  SDL_Quit();
  error_shutdown();
  free(cart_rom);

  return 0;
}

int update_input(void)
{
  keystate = SDL_GetKeyState(NULL);

  while (input.dev[joynum] == NO_DEVICE)
{
    joynum ++;
    if (joynum > MAX_DEVICES - 1) joynum = 0;
  }

  /* reset input */
  input.pad[joynum] = 0;

  /* keyboard */
  if(keystate[SDLK_UP])     input.pad[joynum] |= INPUT_UP;
  else
  if(keystate[SDLK_DOWN])   input.pad[joynum] |= INPUT_DOWN;
  if(keystate[SDLK_LEFT])   input.pad[joynum] |= INPUT_LEFT;
  else
  if(keystate[SDLK_RIGHT])  input.pad[joynum] |= INPUT_RIGHT;

  if(keystate[SDLK_a])      input.pad[joynum] |= INPUT_A;
  if(keystate[SDLK_s])      input.pad[joynum] |= INPUT_B;
  if(keystate[SDLK_d])      input.pad[joynum] |= INPUT_C;
  if(keystate[SDLK_f])      input.pad[joynum] |= INPUT_START;
  if(keystate[SDLK_z])      input.pad[joynum] |= INPUT_X;
  if(keystate[SDLK_x])      input.pad[joynum] |= INPUT_Y;
  if(keystate[SDLK_c])      input.pad[joynum] |= INPUT_Z;
  if(keystate[SDLK_v])      input.pad[joynum] |= INPUT_MODE;

  extern uint8 pico_current;
  if (input.dev[joynum] == DEVICE_LIGHTGUN)
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
  }
  else if (input.dev[joynum] == DEVICE_MOUSE)
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
    if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_A;
    if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_B;
  }
  else if (system_hw == SYSTEM_PICO)
  {
    /* get mouse (absolute values) */
    int x,y;
    int state = SDL_GetMouseState(&x,&y);

    /* Calculate X Y axis values */
    input.analog[0][0] = 0x3c  + (x * (0x17c-0x03c+1)) / 640;
    input.analog[0][1] = 0x1fc + (y * (0x2f7-0x1fc+1)) / 480;
 
    /* Map mouse buttons to player #1 inputs */
    if(state & SDL_BUTTON_MMASK) pico_current++;
    if(state & SDL_BUTTON_RMASK) input.pad[joynum] |= INPUT_A;
    if(state & SDL_BUTTON_LMASK) input.pad[joynum] |= INPUT_B;
  }


  /* options */
  return (1);
}
