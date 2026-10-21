/* Tests of the Genesis Plus GX libretro core through its API (dlopen): the
   automatic overclock of the main and Sega CD CPU, its indicator and Auto
   Crop, with the programs of make_test_programs.py.

   libretro_test <core> <folder>/test.md [<folder>/test-cd.iso]
   (run in that folder: it holds bios_CD_U.bin)

   Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libretro.h"

static const char *opt_overclock = "100%", *opt_sub_overclock = "100%", *opt_indicator = "disabled", *opt_crop = "disabled", *opt_overscan = "disabled";
static unsigned fw, fh, geometry_w, geometry_h;
static float geometry_aspect;
static uint16_t frame[720 * 576];

static bool env(unsigned cmd, void *data)
{
  switch (cmd)
  {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char **)data = ".";
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
      struct retro_variable *v = data;
      v->value = NULL;
      if (!strcmp(v->key, "genesis_plus_gx_overclock")) v->value = opt_overclock;
      else if (!strcmp(v->key, "genesis_plus_gx_sub_overclock")) v->value = opt_sub_overclock;
      else if (!strcmp(v->key, "genesis_plus_gx_auto_overclock_indicator")) v->value = opt_indicator;
      else if (!strcmp(v->key, "genesis_plus_gx_auto_crop")) v->value = opt_crop;
      else if (!strcmp(v->key, "genesis_plus_gx_overscan")) v->value = opt_overscan;
      return v->value != NULL;
    }
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    {
      const struct retro_game_geometry *g = data;
      geometry_w = g->base_width; geometry_h = g->base_height; geometry_aspect = g->aspect_ratio;
      return true;
    }
    default:
      return false;
  }
}
static void video(const void *d, unsigned w, unsigned h, size_t pitch)
{
  unsigned y;
  fw = w; fh = h;
  if (!d) return;
  for (y = 0; y < h; y++) memcpy(frame + y * w, (const uint8_t *)d + y * pitch, w * 2);
}
static void sample(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t batch(const int16_t *d, size_t f) { (void)d; return f; }
static void poll(void) {}
static int16_t state(unsigned p, unsigned d, unsigned i, unsigned id) { (void)p; (void)d; (void)i; (void)id; return 0; }

#define F(name) static __typeof__(&name) p_##name
F(retro_set_environment); F(retro_set_video_refresh); F(retro_set_audio_sample); F(retro_set_audio_sample_batch);
F(retro_set_input_poll); F(retro_set_input_state); F(retro_init); F(retro_load_game); F(retro_unload_game);
F(retro_run); F(retro_get_memory_data); F(retro_get_system_av_info);

static uint8_t rom[0x20000];
static size_t rom_size;
static const char *rom_path;
static int failures;

static void check(int ok, const char *what)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) failures++;
}

/* frames finished in the last 120 of `frames`: 500 light frames (overclock
   delay of the core and the time the automatic overclock waits), then `load`
   frames of work each frame with VDP register 1 = reg1 */
static unsigned run(double load, unsigned reg1, unsigned frames)
{
  struct retro_game_info game = { rom_path, rom, rom_size, NULL };
  uint8_t *ram;
  unsigned i, start = 0, end;
  const unsigned turns = (unsigned)(load * 262 * 3420 / 98), title = (unsigned)(0.3 * 262 * 3420 / 98);
  if (!p_retro_load_game(&game)) { printf("load failed\n"); exit(1); }
  ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
  for (i = 0; i < frames; i++)
  {
    if (i == frames - 120) start = *(uint16_t *)&ram[0x000C];
    *(uint16_t *)&ram[0x0002] = i < 500 ? title : turns;
    *(uint16_t *)&ram[0x0006] = i < 500 ? 0x74 : reg1;
    p_retro_run();
  }
  end = *(uint16_t *)&ram[0x000C];
  p_retro_unload_game();
  return (end - start) & 0xffff;
}

/* the same with the fake Mega CD: frames the sub 68k finished */
static unsigned run_cd(const char *iso, double load, unsigned frames)
{
  struct retro_game_info game = { iso, NULL, 0, NULL };
  uint8_t *ram;
  unsigned i, start = 0, end;
  const unsigned turns = (unsigned)(load * 262 * 3184 / 56), title = (unsigned)(0.3 * 262 * 3184 / 56);
  if (!p_retro_load_game(&game)) { printf("CD load failed\n"); exit(1); }
  ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
  for (i = 0; i < frames; i++)
  {
    if (i == frames - 120) start = *(uint16_t *)&ram[0x000E];
    *(uint16_t *)&ram[0x0002] = i < 500 ? title : turns;
    p_retro_run();
  }
  end = *(uint16_t *)&ram[0x000E];
  p_retro_unload_game();
  return (end - start) & 0xffff;
}

static int dark_box_top_left(void)
{
  unsigned x, y;
  for (y = 0; y < 16 && y < fh; y++) for (x = 0; x < 64 && x < fw; x++) if (frame[y * fw + x] == 0) return 1;
  return 0;
}

int main(int argc, char **argv)
{
  void *core = dlopen(argv[1], RTLD_NOW);
  FILE *f = fopen(argv[2], "rb");
  rom_path = argv[2];
  char text[160];
  unsigned normal, automatic, light, dark;
  if (!core || !f) { printf("open failed %s\n", dlerror()); return 1; }
  rom_size = fread(rom, 1, sizeof(rom), f); fclose(f);
#define L(name) p_##name = dlsym(core, #name)
  L(retro_set_environment); L(retro_set_video_refresh); L(retro_set_audio_sample); L(retro_set_audio_sample_batch);
  L(retro_set_input_poll); L(retro_set_input_state); L(retro_init); L(retro_load_game); L(retro_unload_game);
  L(retro_run); L(retro_get_memory_data); L(retro_get_system_av_info);
  p_retro_set_environment(env); p_retro_set_video_refresh(video); p_retro_set_audio_sample(sample);
  p_retro_set_audio_sample_batch(batch); p_retro_set_input_poll(poll); p_retro_set_input_state(state);
  p_retro_init();

  /* 1.3 frames of work: about 3 of 4 frames finished at 100%, all of them
     once the automatic overclock is up */
  normal = run(1.3, 0x74, 1100);
  opt_overclock = "auto_300";
  automatic = run(1.3, 0x74, 1100);
  light = run(0.3, 0x74, 1100);
  dark = run(1.3, 0x24, 1100); /* display off, VINT on */
  snprintf(text, sizeof(text), "Auto overclock, frames finished of 120: 100%% %u, auto %u, auto light load %u, auto with the display off %u", normal, automatic, light, dark);
  check(normal <= 95 && automatic >= 118 && light >= 119 && dark <= 95, text);

  opt_overclock = "125%";
  snprintf(text, sizeof(text), "Fixed 125%% still overclocks: %u finished", run(1.3, 0x74, 1100));
  check(1, text);

  opt_overclock = "auto_300";
  run(0.3, 0x74, 400);
  check(!dark_box_top_left(), "No indicator while Show Auto Overclock is disabled");
  opt_indicator = "enabled";
  run(0.3, 0x74, 400);
  check(dark_box_top_left(), "Show Auto Overclock draws at the top-left");
  opt_indicator = "disabled";
  opt_overclock = "100%";

  /* Auto Crop: the borders around a white picture are cropped */
  opt_overscan = "full";
  run(0.3, 0x74, 60);
  snprintf(text, sizeof(text), "Borders shown without Auto Crop: %ux%u", fw, fh);
  check(fw > 320 && fh > 224, text);
  opt_crop = "stretch";
  run(0.3, 0x74, 1100);
  snprintf(text, sizeof(text), "Auto Crop (stretch) removes the borders: %ux%u, geometry %ux%u aspect %.3f", fw, fh, geometry_w, geometry_h, geometry_aspect);
  check(fw == 320 && fh == 224 && geometry_w == 320 && geometry_h == 224, text);
  opt_crop = "fit";
  run(0.3, 0x74, 1100);
  snprintf(text, sizeof(text), "Auto Crop (fit): %ux%u aspect %.3f", fw, fh, geometry_aspect);
  check(fw == 320 && fh == 224, text);
  opt_crop = "disabled";
  opt_overscan = "disabled";

  /* Sega CD: the sub CPU */
  if (argc > 3)
  {
    unsigned cd_normal, cd_auto, cd_light;
    cd_normal = run_cd(argv[3], 1.3, 1100);
    opt_sub_overclock = "auto_300";
    cd_auto = run_cd(argv[3], 1.3, 1100);
    cd_light = run_cd(argv[3], 0.3, 1100);
    snprintf(text, sizeof(text), "Sega CD auto overclock, sub CPU frames finished of 120: 100%% %u, auto %u, auto light load %u", cd_normal, cd_auto, cd_light);
    check(cd_normal <= 95 && cd_auto >= 118 && cd_light >= 119, text);
    opt_sub_overclock = "300%";
    snprintf(text, sizeof(text), "Sega CD CPU Speed 300%%: %u finished", run_cd(argv[3], 1.3, 1100));
    check(1, text);
  }
  remove("test.autocrop");
  return failures ? 1 : 0;
}
