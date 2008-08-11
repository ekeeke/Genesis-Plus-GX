#include "shared.h"
#include "font.h"

#include <fat.h>
#include <sys/dir.h>

t_config config;

void config_save()
{
  /* first check if directory exist */
  DIR_ITER *dir = diropen("/genplus");
  if (dir == NULL) mkdir("/genplus",S_IRWXU);
  else dirclose(dir);

  /* open file for writing */
  FILE *fp = fopen("/genplus/genplus.ini", "wb");
  if (fp == NULL) return;

  /* save options */
  fwrite(&config, sizeof(config), 1, fp);

  fclose(fp);
}

void config_load()
{
  /* open file for writing */
  FILE *fp = fopen("/genplus/genplus.ini", "rb");
  if (fp == NULL) return;

  /* read file */
  fread(&config, sizeof(config), 1, fp);

  fclose(fp);
}

void set_config_defaults(void)
{
  /* sound options */
  config.psg_preamp   = 1.5;
  config.fm_preamp    = 1.0;
  config.boost        = 1;
  config.hq_fm        = 1;
  config.fm_core      = 0;
  config.ssg_enabled  = 0;

  /* system options */
  config.freeze_auto    = -1;
  config.sram_auto      = -1;
  config.region_detect  = 0;
  config.force_dtack    = 0;
  config.bios_enabled   = 0;

  /* display options */
  config.xshift   = 0;
  config.yshift   = 0;
  config.xscale   = 0;
  config.yscale   = 0;
  config.aspect   = 1;
  config.overscan = 1;
  config.render   = (vmode->viTVMode == VI_TVMODE_NTSC_PROG) ? 2 : 0;

  /* controllers options */
  ogc_input__set_defaults();
  config.gun_cursor   = 1;
  config.invert_mouse = 0;
}

