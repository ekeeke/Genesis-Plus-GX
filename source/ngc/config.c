/****************************************************************************
 *  config.c
 *
 *  Genesis Plus GX configuration file support
 *
 *  code by Eke-Eke (2008)
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

#ifdef HW_RVL
#define CONFIG_VERSION "GENPLUS 1.3.2W"
#else
#define CONFIG_VERSION "GENPLUS 1.3.2G"
#endif

void config_save()
{
  char pathname[MAXPATHLEN];

  if (!fat_enabled) return;

  /* first check if directory exist */
  sprintf (pathname, DEFAULT_PATH);
  DIR_ITER *dir = diropen(pathname);
  if (dir == NULL) mkdir(pathname,S_IRWXU);
  else dirclose(dir);

  /* open configuration file */
  sprintf (pathname, "%s/config.ini", pathname);
  FILE *fp = fopen(pathname, "wb");
  if (fp == NULL) return;

  /* save options */
  fwrite(&config, sizeof(config), 1, fp);

  fclose(fp);
}

void config_load()
{
  char pathname[MAXPATHLEN];

  /* open configuration file */
  sprintf (pathname, "%s/config.ini", DEFAULT_PATH);
  FILE *fp = fopen(pathname, "rb");
  if (fp == NULL) return;

  /* read version */
  char version[15];
  fread(version, 15, 1, fp); 
  fclose(fp);
  if (strcmp(version,CONFIG_VERSION)) return;

  /* read file */
  fp = fopen(pathname, "rb");
  fread(&config, sizeof(config), 1, fp);
  fclose(fp);
}

void set_config_defaults(void)
{
  /* version TAG */
  strncpy(config.version,CONFIG_VERSION,15);
  
  /* sound options */
  config.psg_preamp   = 150;
  config.fm_preamp    = 100;
  config.boost        = 1;
  config.hq_fm        = 1;
  config.filter       = 1;

  /* system options */
  config.freeze_auto    = -1;
#ifdef HW_RVL
  config.sram_auto      = 0; /* assume we always got SDCARD */
#else
  config.sram_auto      = -1;
#endif
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
  config.render   = VIDEO_HaveComponentCable() ? 2 : 0;
  config.ntsc     = 0;
  config.bilinear = 1;

  /* controllers options */
  ogc_input_default();
  config.gun_cursor   = 1;
  config.invert_mouse = 0;
}

