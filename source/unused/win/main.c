
#include <windows.h>
#include <SDL.h>
#include "shared.h"

int timer_count = 0;
int old_timer_count = 0;
int paused = 0;
int frame_count = 0;

int update_input(void);
unsigned char *keystate;

uint8 log_error = 1;
uint8 debug_on = 0;


Uint32 fps_callback(Uint32 interval)
{
	if(paused) return 1000/60;
	timer_count++;
	if(timer_count % 60 == 0)
	{
		int fps = frame_count;
		char caption[32];
    sprintf(caption, "Genesis Plus/SDL    FPS=%d", fps);
		SDL_WM_SetCaption(caption, NULL);
		frame_count = 0;
	}
	return 1000/60;
}


int main (int argc, char **argv)
{
	int running = 1;

  SDL_Rect viewport, src;
  SDL_Surface *bmp, *screen;
  SDL_Event event;

  error_init();

  /* Print help if no game specified */
	if(argc < 2)
	{
		char caption[256];
    sprintf(caption, "Genesis Plus\nby Charles MacDonald\nWWW: http://cgfm2.emuviews.com\nusage: %s gamename\n", argv[0]);
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
        
	/* load SRAM */
  FILE *f = fopen("./game.srm", "rb");
	if (f!=NULL)
	{
    fread(&sram.sram,0x10000,1, f);
		fclose(f);
	}

	/* load BIOS */
  memset(bios_rom, 0, sizeof(bios_rom));
	f = fopen("./BIOS.bin", "rb");
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
		config.bios_enabled = 3;
  }
	else config.bios_enabled = 0;

  /* initialize SDL */
  viewport.w = 640;
  viewport.h = 480;
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
  {
    exit(1);
  }
  SDL_WM_SetCaption("Genesis Plus/SDL", NULL);
  screen = SDL_SetVideoMode(viewport.w, viewport.h, 16, SDL_HWSURFACE);
  bmp = SDL_CreateRGBSurface(SDL_SWSURFACE, 1024, 512, 16, 0xF800, 0x07E0, 0x001F, 0x0000);

  /* initialize Genesis screen */
  memset(&bitmap, 0, sizeof(t_bitmap));
  bitmap.width  = 1024;
  bitmap.height = 512;
  bitmap.depth  = 16;
  bitmap.granularity = 2;
  bitmap.pitch = (bitmap.width * bitmap.granularity);
  bitmap.data   = (unsigned char *)bmp->pixels;
  bitmap.viewport.w = 256;
  bitmap.viewport.h = 224;
  bitmap.viewport.x = 0;
  bitmap.viewport.y = 0;
  bitmap.remap = 1;

  /* default config */
  set_config_defaults();
  input.system[0] = SYSTEM_GAMEPAD;
  input.system[1] = SYSTEM_GAMEPAD;

	/* initialize emulation */
	int buf_len = (44100 * 4) / vdp_rate;
	snd.buffer[0] = malloc (buf_len/2);
	snd.buffer[1] = malloc (buf_len/2);
	memset (snd.buffer[0], 0, buf_len/2);
  memset (snd.buffer[1], 0, buf_len/2);
  system_init();
  audio_init(44100);
  system_reset();


  /* emulation loop */
  SDL_SetTimer(1000/60, fps_callback);
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
					}
					break;

				default:
					break;
			}
		}

		if(!paused)
		{
			frame_count++;

      if(!system_frame(0))
        system_reset();

      if(bitmap.viewport.changed)
      {
        bitmap.viewport.changed = 0;
        src.w = (bitmap.viewport.w + 2 * bitmap.viewport.x);
        src.h = (bitmap.viewport.h + 2 * bitmap.viewport.y) << (interlaced ? 1:0);
        viewport.w = src.w;
        viewport.h = src.h;
        viewport.x = (640 - viewport.w) / 2;
        viewport.y = (480 - viewport.h) / 2;
        //screen = SDL_SetVideoMode(bitmap.viewport.w, bitmap.viewport.h, 16, SDL_SWSURFACE);
        SDL_BlitSurface(bmp, &src, screen, &viewport);
        SDL_UpdateRect(screen, viewport.x, viewport.y, viewport.w, viewport.h);
      }
    }
  }
  
  system_shutdown();
  SDL_Quit();
  error_shutdown();

  return 0;
}


/* Check if a key is pressed */
int check_key(int code)
{
  static char lastbuf[0x100] = {0};

  if((!keystate[code]) && (lastbuf[code] == 1))
    lastbuf[code] = 0;

  if((keystate[code]) && (lastbuf[code] == 0))
  {
    lastbuf[code] = 1;
    return (1);
  }                                                                    

  return (0);
}

int update_input(void)
{
  int running = 1;

  keystate = SDL_GetKeyState(NULL);

  memset(&input, 0, sizeof(t_input));
  if(keystate[SDLK_UP])     input.pad[0] |= INPUT_UP;
  else
  if(keystate[SDLK_DOWN])   input.pad[0] |= INPUT_DOWN;
  if(keystate[SDLK_LEFT])   input.pad[0] |= INPUT_LEFT;
  else
  if(keystate[SDLK_RIGHT])  input.pad[0] |= INPUT_RIGHT;

  if(keystate[SDLK_a])      input.pad[0] |= INPUT_A;
  if(keystate[SDLK_s])      input.pad[0] |= INPUT_B;
  if(keystate[SDLK_d])      input.pad[0] |= INPUT_C;
  if(keystate[SDLK_f])      input.pad[0] |= INPUT_START;
  if(keystate[SDLK_z])      input.pad[0] |= INPUT_X;
  if(keystate[SDLK_x])      input.pad[0] |= INPUT_Y;
  if(keystate[SDLK_c])      input.pad[0] |= INPUT_Z;
  if(keystate[SDLK_v])      input.pad[0] |= INPUT_MODE;
 
  if(keystate[SDLK_TAB])    system_reset();

  if(keystate[SDLK_ESCAPE]) running = 0;
  return (running);
}
