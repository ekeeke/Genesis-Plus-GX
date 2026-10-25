/****************************************************************************
 *  gamesettings.c
 *
 *  Settings of one game, see gamesettings.h
 *
 *  Copyright (c) 2026 VrepliroidV, under the same terms as Genesis Plus GX
 *  (see LICENSE.txt).
 ****************************************************************************/

#include <stddef.h>

#include "shared.h"
#include "file_load.h"
#include "autocrop.h"
#include "gamesettings.h"

#define GAME_FILE_HEADER "genplus-game 1"

enum { T_U8, T_S16, T_U32, T_FLOAT, T_STR };

typedef struct
{
  const char *name;
  int type;
  size_t offset;
  size_t size; /* T_STR */
} t_game_field;

#define FIELD(f, t) { #f, t, offsetof(t_config, f), 0 }
#define STR_FIELD(f) { #f, T_STR, offsetof(t_config, f), sizeof(((t_config *)0)->f) }

/* The options a game can have of its own: video, audio and system.
   Controllers, buttons, menu options, paths and the options that only count
   when a cartridge is inserted (system type, BIOS, lock-on) stay global. */
static const t_game_field fields[] =
{
  /* video */
  FIELD(render, T_U8),
  FIELD(tv_mode, T_U8),
  FIELD(vsync, T_U8),
  FIELD(bilinear, T_U8),
  FIELD(vfilter, T_U8),
#ifdef HW_RVL
  FIELD(trap, T_U8),
  FIELD(gamma, T_FLOAT),
#endif
  FIELD(lcd, T_U8),
  FIELD(ntsc, T_U8),
  FIELD(ntsc_sharpness, T_FLOAT),
  FIELD(ntsc_resolution, T_FLOAT),
  FIELD(ntsc_artifacts, T_FLOAT),
  FIELD(ntsc_fringing, T_FLOAT),
  FIELD(ntsc_bleed, T_FLOAT),
  FIELD(overscan, T_U8),
  FIELD(gg_extra, T_U8),
  FIELD(aspect, T_U8),
  FIELD(xshift, T_S16),
  FIELD(yshift, T_S16),
  FIELD(xscale, T_S16),
  FIELD(yscale, T_S16),
  FIELD(autocrop, T_U8),
  FIELD(autocrop_scale, T_U8),
  STR_FIELD(bezel),
  /* audio */
  FIELD(hq_fm, T_U8),
  FIELD(filter, T_U8),
  FIELD(hq_psg, T_U8),
  FIELD(ym2612, T_U8),
  FIELD(ym2413, T_U8),
  FIELD(mono, T_U8),
  FIELD(psg_preamp, T_S16),
  FIELD(fm_preamp, T_S16),
  FIELD(cdda_volume, T_S16),
  FIELD(pcm_volume, T_S16),
  FIELD(low_freq, T_S16),
  FIELD(high_freq, T_S16),
  FIELD(lg, T_S16),
  FIELD(mg, T_S16),
  FIELD(hg, T_S16),
  FIELD(lp_range, T_U32),
  /* system */
  FIELD(region_detect, T_U8),
  FIELD(master_clock, T_U8),
  FIELD(vdp_mode, T_U8),
  FIELD(force_dtack, T_U8),
  FIELD(addr_error, T_U8),
  FIELD(cd_latency, T_U8),
  FIELD(enhanced_vscroll, T_U8),
  FIELD(enhanced_vscroll_limit, T_U8),
  FIELD(m68k_overclock, T_FLOAT),
  FIELD(m68k_auto_overclock, T_U8),
  FIELD(s68k_overclock, T_FLOAT),
  FIELD(s68k_auto_overclock, T_U8),
  FIELD(z80_overclock, T_FLOAT)
};

#define FIELD_COUNT (sizeof(fields) / sizeof(fields[0]))

/* global values of the fields while a game uses its own, one after the
   other (a whole t_config would take about 80 KB of the nearly full MEM1) */
static u8 globals[512];
static int active = 0;

static size_t field_size(const t_game_field *f)
{
  switch (f->type)
  {
    case T_U8:  return sizeof(uint8);
    case T_S16: return sizeof(int16);
    case T_U32: return sizeof(uint32);
    case T_STR: return f->size;
    default:    return sizeof(float);
  }
}

/* config -> globals (store), globals -> config (restore) or both (swap) */
enum { STORE, RESTORE, SWAP };
static void move_globals(int how)
{
  unsigned int i;
  size_t pos = 0;
  for (i = 0; i < FIELD_COUNT; i++)
  {
    char *field = (char *)&config + fields[i].offset;
    size_t size = field_size(&fields[i]);
    if (pos + size > sizeof(globals))
    {
      break;
    }
    if (how == STORE)
    {
      memcpy(&globals[pos], field, size);
    }
    else if (how == RESTORE)
    {
      memcpy(field, &globals[pos], size);
    }
    else
    {
      size_t j;
      for (j = 0; j < size; j++)
      {
        u8 tmp = field[j];
        field[j] = globals[pos + j];
        globals[pos + j] = tmp;
      }
    }
    pos += size;
  }
}

/* saves/<system>/<game>.cfg */
static int game_path(char *path, int size)
{
  if (!rom_filename[0])
  {
    return 0;
  }
  snprintf(path, size, "%s/saves/%s.cfg", DEFAULT_PATH, rom_filename);
  return 1;
}

static int read_game_file(const char *path)
{
  char line[256];
  FILE *fp = fopen(path, "r");
  if (!fp)
  {
    return 0;
  }

  if (!fgets(line, sizeof(line), fp) || strncmp(line, GAME_FILE_HEADER, strlen(GAME_FILE_HEADER)))
  {
    fclose(fp);
    return 0;
  }

  while (fgets(line, sizeof(line), fp))
  {
    unsigned int i;
    char *eq = strchr(line, '=');
    if (!eq)
    {
      continue;
    }
    *eq = 0;
    for (i = 0; i < FIELD_COUNT; i++)
    {
      char *p = (char *)&config + fields[i].offset;
      if (strcmp(line, fields[i].name))
      {
        continue;
      }
      if (fields[i].type == T_STR)
      {
        char *value = eq + 1;
        value[strcspn(value, "\r\n")] = 0;
        snprintf(p, fields[i].size, "%s", value);
      }
      else if (fields[i].type == T_FLOAT)
      {
        float v;
        if (sscanf(eq + 1, "%f", &v) == 1) *(float *)p = v;
      }
      else
      {
        long v;
        if (sscanf(eq + 1, "%ld", &v) == 1)
        {
          if (fields[i].type == T_U8) *(uint8 *)p = (uint8)v;
          else if (fields[i].type == T_S16) *(int16 *)p = (int16)v;
          else *(uint32 *)p = (uint32)v;
        }
      }
      break;
    }
  }

  fclose(fp);
  return 1;
}

int gamesettings_active(void)
{
  return active;
}

void gamesettings_save(void)
{
  char path[MAXPATHLEN];
  unsigned int i;
  FILE *fp;

  if (!active || !game_path(path, sizeof(path)))
  {
    return;
  }

  fp = fopen(path, "w");
  if (!fp)
  {
    return;
  }
  fprintf(fp, "%s\n", GAME_FILE_HEADER);
  for (i = 0; i < FIELD_COUNT; i++)
  {
    const char *p = (const char *)&config + fields[i].offset;
    switch (fields[i].type)
    {
      case T_U8:  fprintf(fp, "%s=%d\n", fields[i].name, *(const uint8 *)p); break;
      case T_S16: fprintf(fp, "%s=%d\n", fields[i].name, *(const int16 *)p); break;
      case T_U32: fprintf(fp, "%s=%lu\n", fields[i].name, (unsigned long)*(const uint32 *)p); break;
      case T_STR: fprintf(fp, "%s=%s\n", fields[i].name, p); break;
      default:    fprintf(fp, "%s=%f\n", fields[i].name, *(const float *)p); break;
    }
  }
  fclose(fp);
}

void gamesettings_load(void)
{
  char path[MAXPATHLEN];

  /* back to the global options */
  if (active)
  {
    move_globals(RESTORE);
    active = 0;
  }

  if (game_path(path, sizeof(path)))
  {
    move_globals(STORE);
    if (read_game_file(path))
    {
      active = 1;

      /* values the menus could not set */
      if (config.autocrop >= AUTOCROP_MODES) config.autocrop = AUTOCROP_OFF;
      if (config.autocrop_scale >= AUTOCROP_SCALINGS) config.autocrop_scale = AUTOCROP_SCALE_SMOOTH;
      if (config.region_detect > 4) config.region_detect = 0;
      if (config.m68k_overclock < 1.0) config.m68k_overclock = 1.0;
      if (config.m68k_auto_overclock > 1) config.m68k_auto_overclock = 0;
      /* a game saved without overclock follows the automatic one turned on
         afterwards (its file had no say on it), up to the usual ratio */
      if (config.m68k_auto_overclock && (config.m68k_overclock <= 1.0)) config.m68k_overclock = 3.0;
      if (config.s68k_overclock < 1.0) config.s68k_overclock = 1.0;
      if (config.s68k_auto_overclock > 1) config.s68k_auto_overclock = 0;
      if (config.s68k_auto_overclock && (config.s68k_overclock <= 1.0)) config.s68k_overclock = 3.0;
      if (config.z80_overclock < 1.0) config.z80_overclock = 1.0;
      config.bezel[sizeof(config.bezel) - 1] = 0;
      if (strchr(config.bezel, '/') || strchr(config.bezel, '\\')) config.bezel[0] = 0;
    }
  }
}

int gamesettings_enable(void)
{
  if (active)
  {
    return 1;
  }
  if (!system_hw || !rom_filename[0])
  {
    return 0;
  }

  /* the current values are the global ones */
  move_globals(STORE);
  active = 1;
  gamesettings_save();
  return 1;
}

void gamesettings_disable(void)
{
  char path[MAXPATHLEN];

  if (!active)
  {
    return;
  }

  if (game_path(path, sizeof(path)))
  {
    remove(path);
  }
  move_globals(RESTORE);
  active = 0;
}

void gamesettings_begin_global_save(void)
{
  if (active)
  {
    move_globals(SWAP);
  }
}

void gamesettings_end_global_save(void)
{
  if (active)
  {
    move_globals(SWAP);
  }
}
